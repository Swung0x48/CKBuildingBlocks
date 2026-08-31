#include "PhysicsRTApi.h"

#include "PhysicsRTApiInternal.h"
#include "CKIpionManager.h"

#include "CK3dEntity.h"
#include "CKContext.h"
#include "CKObject.h"
#include "ivp_ball.hxx"
#include "ivp_material.hxx"

#include <cmath>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#ifndef PHYSICSRT_SOURCE_BUILD_ID
#define PHYSICSRT_SOURCE_BUILD_ID "unknown"
#endif

#ifndef PHYSICSRT_SOLVER_COMPATIBILITY_ID
#define PHYSICSRT_SOLVER_COMPATIBILITY_ID "ivp-2.1-authority-66hz-v1"
#endif

static_assert(sizeof(PhysicsRT_BuildInfo) == 160, "PhysicsRT_BuildInfo ABI layout changed");
static_assert(sizeof(PhysicsRT_BodyRef) == 16, "PhysicsRT_BodyRef ABI layout changed");
static_assert(sizeof(PhysicsRT_BodyState) == 80, "PhysicsRT_BodyState ABI layout changed");
static_assert(sizeof(PhysicsRT_BallDesc) == 112, "PhysicsRT_BallDesc ABI layout changed");
static_assert(sizeof(PhysicsRT_ForceCommand) == 40, "PhysicsRT_ForceCommand ABI layout changed");
static_assert(sizeof(PhysicsRT_GameplayWritePolicyEntry) == 16,
              "PhysicsRT_GameplayWritePolicyEntry ABI layout changed");
static_assert(offsetof(PhysicsRT_ApiV2, v1) == 0,
              "PhysicsRT_ApiV2 must preserve the V1 binary prefix");
static_assert(PHYSICSRT_COLLISION_GROUP_CAPACITY == IVP_NO_COLL_GROUP_STRING_LEN,
              "Public and IVP collision group capacities differ");
static_assert(offsetof(PhysicsRT_BodyState, body) == 8, "PhysicsRT_BodyState handle offset changed");
static_assert(offsetof(PhysicsRT_BodyState, position) == 24, "PhysicsRT_BodyState pose offset changed");
static_assert(offsetof(PhysicsRT_BallDesc, position) == 40, "PhysicsRT_BallDesc pose offset changed");

namespace {

const uint32_t kKnownBodyFlags = PHYSICSRT_BODY_ACTIVE |
                                 PHYSICSRT_BODY_SLEEPING |
                                 PHYSICSRT_BODY_COLLISION_ENABLED |
                                 PHYSICSRT_BODY_STATIC;
const uint32_t kKnownBallFlags = PHYSICSRT_BALL_START_ACTIVE |
                                 PHYSICSRT_BALL_COLLISION_ENABLED;
const uint32_t kKnownForceFlags = PHYSICSRT_FORCE_AT_CENTER;
const uint32_t kMaxBatchCount = 65536;

struct WorldRecord
{
    CKIpionManager *manager;
    CKContext *context;
    std::thread::id ownerThread;
    std::map<int32_t, PhysicsRT_BodyHandle> bodiesByCkId;
    std::map<PhysicsRT_BodyHandle, int32_t> ckIdByBody;
};

struct Registry
{
    std::mutex mutex;
    PhysicsRT_WorldHandle nextWorld;
    PhysicsRT_BodyHandle nextBody;
    std::map<PhysicsRT_WorldHandle, WorldRecord> worlds;
    std::map<CKIpionManager *, PhysicsRT_WorldHandle> worldsByManager;

    Registry() : nextWorld(1), nextBody(1) {}
};

struct ResolvedWorld
{
    PhysicsRT_WorldHandle handle;
    CKIpionManager *manager;
    CKContext *context;
};

struct ResolvedBody
{
    PhysicsRT_BodyHandle handle;
    int32_t ckId;
    CKIpionManager *manager;
    PhysicsObject *physicsObject;
};

Registry &GetRegistry()
{
    // Intentionally process-lifetime storage. This avoids static destruction
    // order hazards when a CKContext is released during plugin shutdown.
    static Registry *registry = new Registry();
    return *registry;
}

uint64_t AllocateHandle(uint64_t &next)
{
    uint64_t result = next++;
    if (result == 0)
        result = next++;
    return result;
}

PhysicsRT_Result ResolveWorld(PhysicsRT_WorldHandle handle, ResolvedWorld *outWorld)
{
    if (!outWorld || handle == PHYSICSRT_INVALID_WORLD)
        return PHYSICSRT_ERROR_INVALID_WORLD;

    Registry &registry = GetRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    std::map<PhysicsRT_WorldHandle, WorldRecord>::iterator it = registry.worlds.find(handle);
    if (it == registry.worlds.end() || !it->second.manager)
        return PHYSICSRT_ERROR_INVALID_WORLD;
    if (it->second.ownerThread != std::this_thread::get_id())
        return PHYSICSRT_ERROR_WRONG_THREAD;

    outWorld->handle = handle;
    outWorld->manager = it->second.manager;
    outWorld->context = it->second.context;
    return PHYSICSRT_OK;
}

PhysicsRT_Result ResolveBody(PhysicsRT_WorldHandle worldHandle,
                             PhysicsRT_BodyHandle bodyHandle,
                             ResolvedBody *outBody)
{
    if (!outBody || bodyHandle == PHYSICSRT_INVALID_BODY)
        return PHYSICSRT_ERROR_INVALID_BODY;

    Registry &registry = GetRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    std::map<PhysicsRT_WorldHandle, WorldRecord>::iterator worldIt = registry.worlds.find(worldHandle);
    if (worldIt == registry.worlds.end() || !worldIt->second.manager)
        return PHYSICSRT_ERROR_INVALID_WORLD;
    if (worldIt->second.ownerThread != std::this_thread::get_id())
        return PHYSICSRT_ERROR_WRONG_THREAD;

    std::map<PhysicsRT_BodyHandle, int32_t>::const_iterator bodyIt =
        worldIt->second.ckIdByBody.find(bodyHandle);
    if (bodyIt == worldIt->second.ckIdByBody.end())
        return PHYSICSRT_ERROR_INVALID_BODY;

    PhysicsObject *physicsObject = worldIt->second.manager->GetPhysicsObjectById((CK_ID)bodyIt->second);
    if (!physicsObject || !physicsObject->m_RealObject)
        return PHYSICSRT_ERROR_INVALID_BODY;

    outBody->handle = bodyHandle;
    outBody->ckId = bodyIt->second;
    outBody->manager = worldIt->second.manager;
    outBody->physicsObject = physicsObject;
    return PHYSICSRT_OK;
}

bool IsFinite(float value)
{
    return std::isfinite((double)value) != 0;
}

bool IsFinite3(const float value[3])
{
    return value && IsFinite(value[0]) && IsFinite(value[1]) && IsFinite(value[2]);
}

bool IsValidQuaternion(const float value[4])
{
    if (!value || !IsFinite(value[0]) || !IsFinite(value[1]) ||
        !IsFinite(value[2]) || !IsFinite(value[3]))
        return false;

    const double lengthSquared = (double)value[0] * value[0] +
                                 (double)value[1] * value[1] +
                                 (double)value[2] * value[2] +
                                 (double)value[3] * value[3];
    return lengthSquared >= 0.5 && lengthSquared <= 1.5;
}

void CopyBuildId(char destination[PHYSICSRT_BUILD_ID_CAPACITY], const char *source)
{
    std::memset(destination, 0, PHYSICSRT_BUILD_ID_CAPACITY);
    if (source)
        std::strncpy(destination, source, PHYSICSRT_BUILD_ID_CAPACITY - 1);
}

PhysicsRT_Result ReadBodyState(const ResolvedBody &resolved, PhysicsRT_BodyState *outState)
{
    if (!outState || !resolved.physicsObject || !resolved.physicsObject->m_RealObject)
        return PHYSICSRT_ERROR_INVALID_BODY;

    IVP_Real_Object *object = resolved.physicsObject->m_RealObject;
    IVP_Core *core = object->get_core();
    if (!core)
        return PHYSICSRT_ERROR_INVALID_BODY;

    PhysicsRT_BodyState state;
    std::memset(&state, 0, sizeof(state));
    state.struct_size = sizeof(state);
    state.body = resolved.handle;
    state.ck_id = resolved.ckId;

    IVP_U_Quat orientation;
    IVP_U_Point position;
    object->get_quat_world_f_object_AT(&orientation, &position);
    state.position[0] = (float)position.k[0];
    state.position[1] = (float)position.k[1];
    state.position[2] = (float)position.k[2];
    state.orientation_xyzw[0] = (float)orientation.x;
    state.orientation_xyzw[1] = (float)orientation.y;
    state.orientation_xyzw[2] = (float)orientation.z;
    state.orientation_xyzw[3] = (float)orientation.w;

    state.linear_velocity_world[0] = (float)core->speed.k[0];
    state.linear_velocity_world[1] = (float)core->speed.k[1];
    state.linear_velocity_world[2] = (float)core->speed.k[2];

    IVP_U_Matrix worldFromCore;
    core->calc_at_matrix(core->get_environment()->get_current_time(), &worldFromCore);
    IVP_U_Float_Point angularVelocityWorld;
    worldFromCore.vmult3(&core->rot_speed, &angularVelocityWorld);
    state.angular_velocity_world[0] = angularVelocityWorld.k[0];
    state.angular_velocity_world[1] = angularVelocityWorld.k[1];
    state.angular_velocity_world[2] = angularVelocityWorld.k[2];

    const IVP_Movement_Type movement = object->get_movement_state();
    if (core->physical_unmoveable || movement == IVP_MT_STATIC || movement == IVP_MT_STATIC_PHANTOM)
    {
        state.flags |= PHYSICSRT_BODY_STATIC;
    }
    else if (IVP_MTIS_SIMULATED(movement))
    {
        state.flags |= PHYSICSRT_BODY_ACTIVE;
    }
    else
    {
        state.flags |= PHYSICSRT_BODY_SLEEPING;
    }

    if (object->is_collision_detection_enabled())
        state.flags |= PHYSICSRT_BODY_COLLISION_ENABLED;

    *outState = state;
    return PHYSICSRT_OK;
}

PhysicsRT_Result ValidateState(const ResolvedBody &resolved, const PhysicsRT_BodyState &state)
{
    if (state.struct_size != sizeof(PhysicsRT_BodyState) || state.body != resolved.handle ||
        (state.ck_id != 0 && state.ck_id != resolved.ckId) || (state.flags & ~kKnownBodyFlags) != 0 ||
        !IsFinite3(state.position) || !IsValidQuaternion(state.orientation_xyzw) ||
        !IsFinite3(state.linear_velocity_world) || !IsFinite3(state.angular_velocity_world))
        return PHYSICSRT_ERROR_INVALID_STATE;

    IVP_Core *core = resolved.physicsObject->m_RealObject->get_core();
    if (!core)
        return PHYSICSRT_ERROR_INVALID_BODY;

    const bool isStatic = core->physical_unmoveable != IVP_FALSE;
    if (((state.flags & PHYSICSRT_BODY_STATIC) != 0) != isStatic)
        return PHYSICSRT_ERROR_INVALID_STATE;

    const bool wantsActive = (state.flags & PHYSICSRT_BODY_ACTIVE) != 0;
    const bool wantsSleeping = (state.flags & PHYSICSRT_BODY_SLEEPING) != 0;
    if (isStatic)
    {
        if (wantsActive || wantsSleeping)
            return PHYSICSRT_ERROR_INVALID_STATE;
    }
    else if (wantsActive == wantsSleeping)
    {
        return PHYSICSRT_ERROR_INVALID_STATE;
    }

    return PHYSICSRT_OK;
}

void ApplyState(const ResolvedBody &resolved, const PhysicsRT_BodyState &state)
{
    IVP_Real_Object *object = resolved.physicsObject->m_RealObject;
    IVP_U_Quat orientation;
    orientation.x = state.orientation_xyzw[0];
    orientation.y = state.orientation_xyzw[1];
    orientation.z = state.orientation_xyzw[2];
    orientation.w = state.orientation_xyzw[3];
    orientation.normize_quat();
    IVP_U_Point position(state.position[0], state.position[1], state.position[2]);
    object->beam_object_to_new_position(&orientation, &position, IVP_TRUE);

    IVP_Core *core = object->get_core();
    if (!core->physical_unmoveable)
    {
        // IVP rebuilds the core's next-PSI state while transitioning between
        // IVP_MT_NOT_SIM and an active simulation unit.  In particular,
        // revive_simulation_core() calls calc_next_PSI_matrix_zero_speed(), so
        // velocities written before ensure_in_simulation_now() are silently
        // discarded.  Apply the requested movement state first, then install
        // the authoritative velocities so create/reconcile is atomic from the
        // caller's point of view.
        if ((state.flags & PHYSICSRT_BODY_ACTIVE) != 0)
            object->ensure_in_simulation_now();
        else
            object->disable_simulation();

        core->speed.set(state.linear_velocity_world[0],
                        state.linear_velocity_world[1],
                        state.linear_velocity_world[2]);
        core->speed_change.set(0.0f, 0.0f, 0.0f);

        IVP_U_Matrix worldFromCore;
        core->calc_at_matrix(core->get_environment()->get_current_time(), &worldFromCore);
        IVP_U_Float_Point angularWorld(state.angular_velocity_world[0],
                                       state.angular_velocity_world[1],
                                       state.angular_velocity_world[2]);
        IVP_U_Float_Point angularCore;
        worldFromCore.vimult3(&angularWorld, &angularCore);
        core->rot_speed.set(&angularCore);
        core->rot_speed_change.set(0.0f, 0.0f, 0.0f);
        core->reset_freeze_check_values();
    }

    object->enable_collision_detection(
        (state.flags & PHYSICSRT_BODY_COLLISION_ENABLED) != 0 ? IVP_TRUE : IVP_FALSE);
    CKIpionManager::UpdateObjectWorldMatrix(object);
}

PhysicsRT_Result SetBodyStatesCommon(PhysicsRT_WorldHandle world,
                                     const PhysicsRT_BodyState *states,
                                     uint32_t bodyCount,
                                     bool requireAuthority)
{
    if (bodyCount > kMaxBatchCount)
        return PHYSICSRT_ERROR_LIMIT_EXCEEDED;
    if (bodyCount != 0 && !states)
        return PHYSICSRT_ERROR_INVALID_ARGUMENT;

    ResolvedWorld resolvedWorld;
    PhysicsRT_Result result = ResolveWorld(world, &resolvedWorld);
    if (result != PHYSICSRT_OK)
        return result;
    if (requireAuthority && !resolvedWorld.manager->IsAuthorityMode())
        return PHYSICSRT_ERROR_NOT_READY;

    std::vector<ResolvedBody> resolvedBodies;
    resolvedBodies.reserve(bodyCount);
    std::set<PhysicsRT_BodyHandle> uniqueBodies;
    for (uint32_t i = 0; i < bodyCount; ++i)
    {
        if (!uniqueBodies.insert(states[i].body).second)
            return PHYSICSRT_ERROR_INVALID_STATE;

        ResolvedBody body;
        result = ResolveBody(world, states[i].body, &body);
        if (result != PHYSICSRT_OK)
            return result;
        result = ValidateState(body, states[i]);
        if (result != PHYSICSRT_OK)
            return result;
        resolvedBodies.push_back(body);
    }

    // No operation above mutates IVP. Once every entry is known-valid, the
    // batch is applied without an intervening simulation step on the owner
    // thread, which is the atomic physics boundary promised by this ABI.
    for (uint32_t i = 0; i < bodyCount; ++i)
        ApplyState(resolvedBodies[i], states[i]);
    return PHYSICSRT_OK;
}

PhysicsRT_Result GetBuildInfoForVersion(PhysicsRT_BuildInfo *outInfo,
                                        uint32_t abiVersion)
{
    if (!outInfo || outInfo->struct_size < sizeof(PhysicsRT_BuildInfo))
        return PHYSICSRT_ERROR_INVALID_ARGUMENT;

    PhysicsRT_BuildInfo info;
    std::memset(&info, 0, sizeof(info));
    info.struct_size = sizeof(info);
    info.abi_version = abiVersion;
    CopyBuildId(info.source_build_id, PHYSICSRT_SOURCE_BUILD_ID);
    CopyBuildId(info.solver_compatibility_id, PHYSICSRT_SOLVER_COMPATIBILITY_ID);
    info.fixed_step_seconds = 1.0f / (float)PHYSICSRT_FIXED_TICK_HZ;
    info.max_fixed_steps_per_call = PHYSICSRT_MAX_FIXED_STEPS_PER_CALL;
    *outInfo = info;
    return PHYSICSRT_OK;
}

PhysicsRT_Result PHYSICSRT_CALL GetBuildInfoV1Impl(
    PhysicsRT_BuildInfo *outInfo)
{
    return GetBuildInfoForVersion(outInfo, PHYSICSRT_ABI_VERSION_1);
}

PhysicsRT_Result PHYSICSRT_CALL GetBuildInfoV2Impl(
    PhysicsRT_BuildInfo *outInfo)
{
    return GetBuildInfoForVersion(outInfo, PHYSICSRT_ABI_VERSION_2);
}

PhysicsRT_Result PHYSICSRT_CALL AcquireWorldImpl(void *ckContext, PhysicsRT_WorldHandle *outWorld)
{
    if (!ckContext || !outWorld)
        return PHYSICSRT_ERROR_INVALID_ARGUMENT;

    CKContext *context = static_cast<CKContext *>(ckContext);
    CKIpionManager *manager = CKIpionManager::GetManager(context);
    if (!manager)
        return PHYSICSRT_ERROR_NOT_READY;

    Registry &registry = GetRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    for (std::map<PhysicsRT_WorldHandle, WorldRecord>::const_iterator it = registry.worlds.begin();
         it != registry.worlds.end(); ++it)
    {
        if (it->second.context == ckContext)
        {
            if (it->second.ownerThread != std::this_thread::get_id())
                return PHYSICSRT_ERROR_WRONG_THREAD;
            *outWorld = it->first;
            return PHYSICSRT_OK;
        }
    }

    const PhysicsRT_WorldHandle handle = AllocateHandle(registry.nextWorld);
    WorldRecord record;
    record.manager = manager;
    record.context = context;
    record.ownerThread = std::this_thread::get_id();
    registry.worlds.insert(std::make_pair(handle, record));
    registry.worldsByManager[manager] = handle;
    *outWorld = handle;
    return PHYSICSRT_OK;
}

PhysicsRT_Result PHYSICSRT_CALL ValidateWorldImpl(PhysicsRT_WorldHandle world)
{
    ResolvedWorld resolved;
    return ResolveWorld(world, &resolved);
}

PhysicsRT_Result PHYSICSRT_CALL SetAuthorityModeImpl(PhysicsRT_WorldHandle world, uint32_t enabled)
{
    ResolvedWorld resolved;
    PhysicsRT_Result result = ResolveWorld(world, &resolved);
    if (result != PHYSICSRT_OK)
        return result;
    resolved.manager->SetAuthorityMode(enabled != 0 ? TRUE : FALSE);
    return PHYSICSRT_OK;
}

PhysicsRT_Result PHYSICSRT_CALL GetAuthorityModeImpl(PhysicsRT_WorldHandle world, uint32_t *outEnabled)
{
    if (!outEnabled)
        return PHYSICSRT_ERROR_INVALID_ARGUMENT;
    ResolvedWorld resolved;
    PhysicsRT_Result result = ResolveWorld(world, &resolved);
    if (result != PHYSICSRT_OK)
        return result;
    *outEnabled = resolved.manager->IsAuthorityMode() ? 1u : 0u;
    return PHYSICSRT_OK;
}

PhysicsRT_Result PHYSICSRT_CALL SetGameplayWritesEnabledImpl(
    PhysicsRT_WorldHandle world, uint32_t enabled)
{
    if (enabled > 1u)
        return PHYSICSRT_ERROR_INVALID_ARGUMENT;
    ResolvedWorld resolved;
    PhysicsRT_Result result = ResolveWorld(world, &resolved);
    if (result != PHYSICSRT_OK)
        return result;
    resolved.manager->SetGameplayWritesEnabled(enabled != 0 ? TRUE : FALSE);
    return PHYSICSRT_OK;
}

PhysicsRT_Result PHYSICSRT_CALL GetGameplayWritesEnabledImpl(
    PhysicsRT_WorldHandle world, uint32_t *outEnabled)
{
    if (!outEnabled)
        return PHYSICSRT_ERROR_INVALID_ARGUMENT;
    ResolvedWorld resolved;
    PhysicsRT_Result result = ResolveWorld(world, &resolved);
    if (result != PHYSICSRT_OK)
        return result;
    *outEnabled = resolved.manager->AreGameplayWritesEnabled() ? 1u : 0u;
    return PHYSICSRT_OK;
}

bool ValidGameplayWritePolicy(uint32_t policy)
{
    return policy <= (uint32_t)PHYSICSRT_GAMEPLAY_WRITE_DENY;
}

PhysicsRT_Result ValidateGameplayWritePolicyTarget(
    const ResolvedWorld &world, int32_t ckId)
{
    if (ckId <= 0 || !world.context)
        return PHYSICSRT_ERROR_INVALID_ARGUMENT;
    CKObject *object = world.context->GetObject((CK_ID)ckId);
    return CK3dEntity::Cast(object) ? PHYSICSRT_OK
                                    : PHYSICSRT_ERROR_INVALID_ARGUMENT;
}

PhysicsRT_Result PHYSICSRT_CALL SetGameplayWritePoliciesImpl(
    PhysicsRT_WorldHandle world,
    const PhysicsRT_GameplayWritePolicyEntry *entries,
    uint32_t entryCount)
{
    if (entryCount > PHYSICSRT_MAX_GAMEPLAY_WRITE_POLICIES)
        return PHYSICSRT_ERROR_LIMIT_EXCEEDED;
    if (entryCount != 0 && !entries)
        return PHYSICSRT_ERROR_INVALID_ARGUMENT;

    ResolvedWorld resolved;
    PhysicsRT_Result result = ResolveWorld(world, &resolved);
    if (result != PHYSICSRT_OK)
        return result;

    std::set<int32_t> uniqueIds;
    for (uint32_t i = 0; i < entryCount; ++i)
    {
        const PhysicsRT_GameplayWritePolicyEntry &entry = entries[i];
        if (entry.struct_size != sizeof(PhysicsRT_GameplayWritePolicyEntry) ||
            entry.reserved != 0 || !ValidGameplayWritePolicy(entry.policy) ||
            !uniqueIds.insert(entry.ck_id).second)
            return PHYSICSRT_ERROR_INVALID_ARGUMENT;
        result = ValidateGameplayWritePolicyTarget(resolved, entry.ck_id);
        if (result != PHYSICSRT_OK)
            return result;
    }

    for (uint32_t i = 0; i < entryCount; ++i)
        resolved.manager->SetGameplayWritePolicy(
            (CK_ID)entries[i].ck_id,
            (PhysicsRT_GameplayWritePolicy)entries[i].policy);
    return PHYSICSRT_OK;
}

PhysicsRT_Result PHYSICSRT_CALL GetGameplayWritePoliciesImpl(
    PhysicsRT_WorldHandle world,
    PhysicsRT_GameplayWritePolicyEntry *entries,
    uint32_t entryCount)
{
    if (entryCount > PHYSICSRT_MAX_GAMEPLAY_WRITE_POLICIES)
        return PHYSICSRT_ERROR_LIMIT_EXCEEDED;
    if (entryCount != 0 && !entries)
        return PHYSICSRT_ERROR_INVALID_ARGUMENT;

    ResolvedWorld resolved;
    PhysicsRT_Result result = ResolveWorld(world, &resolved);
    if (result != PHYSICSRT_OK)
        return result;

    std::set<int32_t> uniqueIds;
    for (uint32_t i = 0; i < entryCount; ++i)
    {
        if (entries[i].struct_size !=
                sizeof(PhysicsRT_GameplayWritePolicyEntry) ||
            entries[i].reserved != 0 ||
            !uniqueIds.insert(entries[i].ck_id).second)
            return PHYSICSRT_ERROR_INVALID_ARGUMENT;
        result = ValidateGameplayWritePolicyTarget(resolved, entries[i].ck_id);
        if (result != PHYSICSRT_OK)
            return result;
    }

    for (uint32_t i = 0; i < entryCount; ++i)
    {
        entries[i].policy = (uint32_t)resolved.manager->GetGameplayWritePolicy(
            (CK_ID)entries[i].ck_id);
        entries[i].reserved = 0;
    }
    return PHYSICSRT_OK;
}

PhysicsRT_Result PHYSICSRT_CALL ClearGameplayWritePoliciesImpl(
    PhysicsRT_WorldHandle world, const int32_t *ckIds, uint32_t ckIdCount)
{
    if (ckIdCount > PHYSICSRT_MAX_GAMEPLAY_WRITE_POLICIES)
        return PHYSICSRT_ERROR_LIMIT_EXCEEDED;
    if (ckIdCount != 0 && !ckIds)
        return PHYSICSRT_ERROR_INVALID_ARGUMENT;

    ResolvedWorld resolved;
    PhysicsRT_Result result = ResolveWorld(world, &resolved);
    if (result != PHYSICSRT_OK)
        return result;
    if (ckIdCount == 0)
    {
        resolved.manager->ClearGameplayWritePolicies();
        return PHYSICSRT_OK;
    }

    std::set<int32_t> uniqueIds;
    for (uint32_t i = 0; i < ckIdCount; ++i)
        if (ckIds[i] <= 0 || !uniqueIds.insert(ckIds[i]).second)
            return PHYSICSRT_ERROR_INVALID_ARGUMENT;
    for (uint32_t i = 0; i < ckIdCount; ++i)
        resolved.manager->ClearGameplayWritePolicy((CK_ID)ckIds[i]);
    return PHYSICSRT_OK;
}

PhysicsRT_Result PHYSICSRT_CALL StepFixedImpl(PhysicsRT_WorldHandle world, uint32_t tickCount)
{
    if (tickCount == 0)
        return PHYSICSRT_ERROR_INVALID_ARGUMENT;
    if (tickCount > PHYSICSRT_MAX_FIXED_STEPS_PER_CALL)
        return PHYSICSRT_ERROR_LIMIT_EXCEEDED;

    ResolvedWorld resolved;
    PhysicsRT_Result result = ResolveWorld(world, &resolved);
    if (result != PHYSICSRT_OK)
        return result;
    if (!resolved.manager->IsAuthorityMode() || !resolved.manager->GetEnvironment())
        return PHYSICSRT_ERROR_NOT_READY;

    for (uint32_t i = 0; i < tickCount; ++i)
    {
        if (!resolved.manager->StepAuthoritySimulation())
            return PHYSICSRT_ERROR_NOT_READY;
    }
    return PHYSICSRT_OK;
}

PhysicsRT_Result PHYSICSRT_CALL FindBodyByCkIdImpl(PhysicsRT_WorldHandle world,
                                                   int32_t ckId,
                                                   PhysicsRT_BodyHandle *outBody)
{
    if (!outBody || ckId <= 0)
        return PHYSICSRT_ERROR_INVALID_ARGUMENT;

    Registry &registry = GetRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    std::map<PhysicsRT_WorldHandle, WorldRecord>::iterator worldIt = registry.worlds.find(world);
    if (worldIt == registry.worlds.end())
        return PHYSICSRT_ERROR_INVALID_WORLD;
    if (worldIt->second.ownerThread != std::this_thread::get_id())
        return PHYSICSRT_ERROR_WRONG_THREAD;
    std::map<int32_t, PhysicsRT_BodyHandle>::const_iterator bodyIt = worldIt->second.bodiesByCkId.find(ckId);
    if (bodyIt == worldIt->second.bodiesByCkId.end())
    {
        PhysicsObject *physicsObject =
            worldIt->second.manager->GetPhysicsObjectById((CK_ID)ckId);
        if (!physicsObject || !physicsObject->m_RealObject)
            return PHYSICSRT_ERROR_INVALID_BODY;
        const PhysicsRT_BodyHandle handle = AllocateHandle(registry.nextBody);
        worldIt->second.bodiesByCkId[ckId] = handle;
        worldIt->second.ckIdByBody[handle] = ckId;
        *outBody = handle;
        return PHYSICSRT_OK;
    }
    *outBody = bodyIt->second;
    return PHYSICSRT_OK;
}

PhysicsRT_Result PHYSICSRT_CALL EnumerateBodiesImpl(PhysicsRT_WorldHandle world,
                                                    PhysicsRT_BodyRef *outBodies,
                                                    uint32_t capacity,
                                                    uint32_t *inoutCount)
{
    if (!inoutCount)
        return PHYSICSRT_ERROR_INVALID_ARGUMENT;

    Registry &registry = GetRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    std::map<PhysicsRT_WorldHandle, WorldRecord>::const_iterator worldIt = registry.worlds.find(world);
    if (worldIt == registry.worlds.end())
        return PHYSICSRT_ERROR_INVALID_WORLD;
    if (worldIt->second.ownerThread != std::this_thread::get_id())
        return PHYSICSRT_ERROR_WRONG_THREAD;

    const uint32_t required = (uint32_t)worldIt->second.bodiesByCkId.size();
    *inoutCount = required;
    if (required == 0)
        return PHYSICSRT_OK;
    if (!outBodies || capacity < required)
        return PHYSICSRT_ERROR_BUFFER_TOO_SMALL;

    uint32_t index = 0;
    for (std::map<int32_t, PhysicsRT_BodyHandle>::const_iterator it =
             worldIt->second.bodiesByCkId.begin();
         it != worldIt->second.bodiesByCkId.end(); ++it, ++index)
    {
        PhysicsRT_BodyRef &body = outBodies[index];
        std::memset(&body, 0, sizeof(body));
        body.struct_size = sizeof(body);
        body.ck_id = it->first;
        body.body = it->second;
    }
    return PHYSICSRT_OK;
}

PhysicsRT_Result PHYSICSRT_CALL ValidateBodyImpl(PhysicsRT_WorldHandle world,
                                                 PhysicsRT_BodyHandle body)
{
    ResolvedBody resolved;
    return ResolveBody(world, body, &resolved);
}

PhysicsRT_Result PHYSICSRT_CALL CreateBallImpl(PhysicsRT_WorldHandle world,
                                               const PhysicsRT_BallDesc *desc,
                                               PhysicsRT_BodyHandle *outBody)
{
    if (!desc || !outBody || desc->struct_size < sizeof(PhysicsRT_BallDesc) || desc->ck_id <= 0 ||
        (desc->flags & ~kKnownBallFlags) != 0 || !IsFinite(desc->radius) || desc->radius <= 0.0f ||
        !IsFinite(desc->mass) || desc->mass <= 0.0f || !IsFinite(desc->friction) || desc->friction < 0.0f ||
        !IsFinite(desc->restitution) || desc->restitution < 0.0f || desc->restitution > 1.0f ||
        !IsFinite(desc->linear_damping) || desc->linear_damping < 0.0f ||
        !IsFinite(desc->angular_damping) || desc->angular_damping < 0.0f ||
        !IsFinite3(desc->position) || !IsValidQuaternion(desc->orientation_xyzw) ||
        !IsFinite3(desc->linear_velocity_world) || !IsFinite3(desc->angular_velocity_world) ||
        !std::memchr(desc->collision_group, '\0', PHYSICSRT_COLLISION_GROUP_CAPACITY))
        return PHYSICSRT_ERROR_INVALID_ARGUMENT;

    ResolvedWorld resolvedWorld;
    PhysicsRT_Result result = ResolveWorld(world, &resolvedWorld);
    if (result != PHYSICSRT_OK)
        return result;
    if (!resolvedWorld.manager->GetEnvironment())
        return PHYSICSRT_ERROR_NOT_READY;
    if (resolvedWorld.manager->GetPhysicsObjectById((CK_ID)desc->ck_id))
        return PHYSICSRT_ERROR_ALREADY_EXISTS;

    CKObject *object = resolvedWorld.context->GetObject((CK_ID)desc->ck_id);
    if (!object || !CKIsChildClassOf(object, CKCID_3DENTITY))
        return PHYSICSRT_ERROR_INVALID_ARGUMENT;
    CK3dEntity *entity = (CK3dEntity *)object;

    IVP_Material_Simple *material = new IVP_Material_Simple(desc->friction, desc->restitution);
    VxVector localCenter(0.0f, 0.0f, 0.0f);
    float radius = desc->radius;
    char collisionGroup[PHYSICSRT_COLLISION_GROUP_CAPACITY];
    std::memcpy(collisionGroup, desc->collision_group, sizeof(collisionGroup));
    collisionGroup[sizeof(collisionGroup) - 1] = '\0';
    char collisionIdentity[] = "PhysicsRTAuthorityBall";
    const CKBOOL startFrozen = (desc->flags & PHYSICSRT_BALL_START_ACTIVE) != 0 ? FALSE : TRUE;
    const CKBOOL enableCollision =
        (desc->flags & PHYSICSRT_BALL_COLLISION_ENABLED) != 0 ? TRUE : FALSE;

    const int createResult = resolvedWorld.manager->CreatePhysicsObjectOnParameters(
        entity, 0, NULL, 1, &localCenter, &radius, 0, NULL, desc->radius,
        collisionIdentity, NULL, FALSE, material, desc->mass, collisionGroup,
        startFrozen, enableCollision, TRUE, desc->linear_damping, desc->angular_damping);
    if (createResult != CK_OK)
    {
        delete material;
        return PHYSICSRT_ERROR_INVALID_STATE;
    }
    resolvedWorld.manager->OwnMaterial(entity, material);

    PhysicsRT_BodyHandle handle = PHYSICSRT_INVALID_BODY;
    result = FindBodyByCkIdImpl(world, desc->ck_id, &handle);
    if (result != PHYSICSRT_OK)
    {
        PhysicsObject *created = resolvedWorld.manager->GetPhysicsObjectById((CK_ID)desc->ck_id);
        if (created && created->m_RealObject)
            created->m_RealObject->delete_silently();
        return result;
    }

    ResolvedBody resolvedBody;
    result = ResolveBody(world, handle, &resolvedBody);
    if (result != PHYSICSRT_OK)
        return result;

    PhysicsRT_BodyState state;
    std::memset(&state, 0, sizeof(state));
    state.struct_size = sizeof(state);
    state.body = handle;
    state.ck_id = desc->ck_id;
    state.flags = (desc->flags & PHYSICSRT_BALL_START_ACTIVE) != 0 ?
        PHYSICSRT_BODY_ACTIVE : PHYSICSRT_BODY_SLEEPING;
    if ((desc->flags & PHYSICSRT_BALL_COLLISION_ENABLED) != 0)
        state.flags |= PHYSICSRT_BODY_COLLISION_ENABLED;
    std::memcpy(state.position, desc->position, sizeof(state.position));
    std::memcpy(state.orientation_xyzw, desc->orientation_xyzw, sizeof(state.orientation_xyzw));
    std::memcpy(state.linear_velocity_world, desc->linear_velocity_world,
                sizeof(state.linear_velocity_world));
    std::memcpy(state.angular_velocity_world, desc->angular_velocity_world,
                sizeof(state.angular_velocity_world));
    ApplyState(resolvedBody, state);

    *outBody = handle;
    return PHYSICSRT_OK;
}

PhysicsRT_Result PHYSICSRT_CALL DestroyBodyImpl(PhysicsRT_WorldHandle world,
                                                PhysicsRT_BodyHandle body)
{
    ResolvedBody resolved;
    PhysicsRT_Result result = ResolveBody(world, body, &resolved);
    if (result != PHYSICSRT_OK)
        return result;

    IVP_Real_Object *object = resolved.physicsObject->m_RealObject;
    PhysicsRT_InternalInvalidateBody(resolved.manager, resolved.ckId);
    object->delete_silently();
    return PHYSICSRT_OK;
}

PhysicsRT_Result PHYSICSRT_CALL CaptureBallDescImpl(PhysicsRT_WorldHandle world,
                                                    PhysicsRT_BodyHandle body,
                                                    PhysicsRT_BallDesc *outDesc)
{
    if (!outDesc || outDesc->struct_size < sizeof(PhysicsRT_BallDesc))
        return PHYSICSRT_ERROR_INVALID_ARGUMENT;

    ResolvedBody resolved;
    PhysicsRT_Result result = ResolveBody(world, body, &resolved);
    if (result != PHYSICSRT_OK)
        return result;

    IVP_Real_Object *object = resolved.physicsObject->m_RealObject;
    IVP_Ball *ball = dynamic_cast<IVP_Ball *>(object);
    if (!ball)
        return PHYSICSRT_ERROR_INVALID_STATE;
    IVP_Core *core = object->get_core();
    if (!core)
        return PHYSICSRT_ERROR_INVALID_STATE;
    if (core->physical_unmoveable)
        return PHYSICSRT_ERROR_STATIC_BODY;
    IVP_Material *material = object->l_default_material;
    if (!material)
        return PHYSICSRT_ERROR_INVALID_STATE;

    const float radius = ball->get_radius();
    const float mass = core->get_mass();
    const float friction = (float)material->get_friction_factor();
    const float restitution = (float)material->get_elasticity();
    const float linearDamping = core->speed_damp_factor;
    const float angularDamping = core->rot_speed_damp_factor.k[0];
    if (!IsFinite(radius) || radius <= 0.0f || !IsFinite(mass) || mass <= 0.0f ||
        !IsFinite(friction) || friction < 0.0f || !IsFinite(restitution) ||
        restitution < 0.0f || restitution > 1.0f || !IsFinite(linearDamping) ||
        linearDamping < 0.0f || !IsFinite(angularDamping) || angularDamping < 0.0f ||
        !IsFinite(core->rot_speed_damp_factor.k[1]) ||
        !IsFinite(core->rot_speed_damp_factor.k[2]) ||
        std::fabs(core->rot_speed_damp_factor.k[1] - angularDamping) > 1.0e-6f ||
        std::fabs(core->rot_speed_damp_factor.k[2] - angularDamping) > 1.0e-6f)
        return PHYSICSRT_ERROR_INVALID_STATE;

    PhysicsRT_BodyState state;
    result = ReadBodyState(resolved, &state);
    if (result != PHYSICSRT_OK)
        return result;

    PhysicsRT_BallDesc desc;
    std::memset(&desc, 0, sizeof(desc));
    desc.struct_size = sizeof(desc);
    desc.flags = (state.flags & PHYSICSRT_BODY_ACTIVE) != 0 ?
        PHYSICSRT_BALL_START_ACTIVE : 0;
    if ((state.flags & PHYSICSRT_BODY_COLLISION_ENABLED) != 0)
        desc.flags |= PHYSICSRT_BALL_COLLISION_ENABLED;
    desc.ck_id = resolved.ckId;
    desc.radius = radius;
    desc.mass = mass;
    desc.friction = friction;
    desc.restitution = restitution;
    desc.linear_damping = linearDamping;
    desc.angular_damping = angularDamping;
    std::memcpy(desc.position, state.position, sizeof(desc.position));
    std::memcpy(desc.orientation_xyzw, state.orientation_xyzw,
                sizeof(desc.orientation_xyzw));
    std::memcpy(desc.linear_velocity_world, state.linear_velocity_world,
                sizeof(desc.linear_velocity_world));
    std::memcpy(desc.angular_velocity_world, state.angular_velocity_world,
                sizeof(desc.angular_velocity_world));
    std::memcpy(desc.collision_group, object->nocoll_group_ident,
                PHYSICSRT_COLLISION_GROUP_CAPACITY - 1);
    desc.collision_group[PHYSICSRT_COLLISION_GROUP_CAPACITY - 1] = '\0';
    *outDesc = desc;
    return PHYSICSRT_OK;
}

PhysicsRT_Result PHYSICSRT_CALL GetBodyStatesImpl(PhysicsRT_WorldHandle world,
                                                  const PhysicsRT_BodyHandle *bodies,
                                                  uint32_t bodyCount,
                                                  PhysicsRT_BodyState *outStates)
{
    if (bodyCount > kMaxBatchCount)
        return PHYSICSRT_ERROR_LIMIT_EXCEEDED;
    if (bodyCount != 0 && (!bodies || !outStates))
        return PHYSICSRT_ERROR_INVALID_ARGUMENT;

    ResolvedWorld resolvedWorld;
    PhysicsRT_Result result = ResolveWorld(world, &resolvedWorld);
    if (result != PHYSICSRT_OK)
        return result;

    std::vector<PhysicsRT_BodyState> snapshot(bodyCount);
    for (uint32_t i = 0; i < bodyCount; ++i)
    {
        ResolvedBody body;
        result = ResolveBody(world, bodies[i], &body);
        if (result != PHYSICSRT_OK)
            return result;
        result = ReadBodyState(body, &snapshot[i]);
        if (result != PHYSICSRT_OK)
            return result;
    }
    if (bodyCount != 0)
        std::memcpy(outStates, &snapshot[0], sizeof(PhysicsRT_BodyState) * bodyCount);
    return PHYSICSRT_OK;
}

PhysicsRT_Result PHYSICSRT_CALL SetBodyStatesImpl(PhysicsRT_WorldHandle world,
                                                  const PhysicsRT_BodyState *states,
                                                  uint32_t bodyCount)
{
    return SetBodyStatesCommon(world, states, bodyCount, false);
}

PhysicsRT_Result PHYSICSRT_CALL ReconcileBodyStatesImpl(PhysicsRT_WorldHandle world,
                                                        const PhysicsRT_BodyState *states,
                                                        uint32_t bodyCount)
{
    return SetBodyStatesCommon(world, states, bodyCount, true);
}

PhysicsRT_Result ApplyCommandsCommon(PhysicsRT_WorldHandle world,
                                     const PhysicsRT_ForceCommand *commands,
                                     uint32_t commandCount,
                                     bool isForce)
{
    if (commandCount > kMaxBatchCount)
        return PHYSICSRT_ERROR_LIMIT_EXCEEDED;
    if (commandCount != 0 && !commands)
        return PHYSICSRT_ERROR_INVALID_ARGUMENT;

    ResolvedWorld resolvedWorld;
    PhysicsRT_Result result = ResolveWorld(world, &resolvedWorld);
    if (result != PHYSICSRT_OK)
        return result;
    const float forceDelta = resolvedWorld.manager->GetForceDeltaSeconds();
    if (isForce && (!IsFinite(forceDelta) || forceDelta <= 0.0f))
        return PHYSICSRT_ERROR_NOT_READY;

    std::vector<ResolvedBody> resolvedBodies;
    resolvedBodies.reserve(commandCount);
    for (uint32_t i = 0; i < commandCount; ++i)
    {
        if (commands[i].struct_size != sizeof(PhysicsRT_ForceCommand) ||
            (commands[i].flags & ~kKnownForceFlags) != 0 || !IsFinite3(commands[i].vector_world) ||
            ((commands[i].flags & PHYSICSRT_FORCE_AT_CENTER) == 0 &&
             !IsFinite3(commands[i].point_world)))
            return PHYSICSRT_ERROR_INVALID_ARGUMENT;

        ResolvedBody body;
        result = ResolveBody(world, commands[i].body, &body);
        if (result != PHYSICSRT_OK)
            return result;
        if (body.physicsObject->m_RealObject->get_core()->physical_unmoveable)
            return PHYSICSRT_ERROR_STATIC_BODY;
        resolvedBodies.push_back(body);
    }

    for (uint32_t i = 0; i < commandCount; ++i)
    {
        IVP_Real_Object *object = resolvedBodies[i].physicsObject->m_RealObject;
        object->ensure_in_simulation_now();
        const float multiplier = isForce ? forceDelta : 1.0f;
        IVP_U_Float_Point impulse(commands[i].vector_world[0] * multiplier,
                                  commands[i].vector_world[1] * multiplier,
                                  commands[i].vector_world[2] * multiplier);
        if ((commands[i].flags & PHYSICSRT_FORCE_AT_CENTER) != 0)
        {
            object->get_core()->async_center_push_core_multiple_ws(&impulse);
        }
        else
        {
            IVP_U_Point point(commands[i].point_world[0],
                              commands[i].point_world[1],
                              commands[i].point_world[2]);
            object->async_push_object_ws(&point, &impulse);
        }
    }
    return PHYSICSRT_OK;
}

PhysicsRT_Result PHYSICSRT_CALL ApplyForcesImpl(PhysicsRT_WorldHandle world,
                                                const PhysicsRT_ForceCommand *commands,
                                                uint32_t commandCount)
{
    return ApplyCommandsCommon(world, commands, commandCount, true);
}

PhysicsRT_Result PHYSICSRT_CALL ApplyImpulsesImpl(PhysicsRT_WorldHandle world,
                                                  const PhysicsRT_ForceCommand *commands,
                                                  uint32_t commandCount)
{
    return ApplyCommandsCommon(world, commands, commandCount, false);
}

const PhysicsRT_ApiV1 kApiV1 = {
    sizeof(PhysicsRT_ApiV1),
    PHYSICSRT_ABI_VERSION_1,
    &GetBuildInfoV1Impl,
    &AcquireWorldImpl,
    &ValidateWorldImpl,
    &SetAuthorityModeImpl,
    &GetAuthorityModeImpl,
    &StepFixedImpl,
    &FindBodyByCkIdImpl,
    &EnumerateBodiesImpl,
    &ValidateBodyImpl,
    &CreateBallImpl,
    &DestroyBodyImpl,
    &GetBodyStatesImpl,
    &SetBodyStatesImpl,
    &ReconcileBodyStatesImpl,
    &ApplyForcesImpl,
    &ApplyImpulsesImpl,
    &CaptureBallDescImpl,
    &SetGameplayWritesEnabledImpl,
    &GetGameplayWritesEnabledImpl,
};

const PhysicsRT_ApiV2 kApiV2 = {
    {
        sizeof(PhysicsRT_ApiV2),
        PHYSICSRT_ABI_VERSION_2,
        &GetBuildInfoV2Impl,
        &AcquireWorldImpl,
        &ValidateWorldImpl,
        &SetAuthorityModeImpl,
        &GetAuthorityModeImpl,
        &StepFixedImpl,
        &FindBodyByCkIdImpl,
        &EnumerateBodiesImpl,
        &ValidateBodyImpl,
        &CreateBallImpl,
        &DestroyBodyImpl,
        &GetBodyStatesImpl,
        &SetBodyStatesImpl,
        &ReconcileBodyStatesImpl,
        &ApplyForcesImpl,
        &ApplyImpulsesImpl,
        &CaptureBallDescImpl,
        &SetGameplayWritesEnabledImpl,
        &GetGameplayWritesEnabledImpl,
    },
    &SetGameplayWritePoliciesImpl,
    &GetGameplayWritePoliciesImpl,
    &ClearGameplayWritePoliciesImpl,
};

} // namespace

void PhysicsRT_InternalRegisterWorld(CKIpionManager *manager, CKContext *context)
{
    if (!manager || !context)
        return;

    Registry &registry = GetRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    std::map<CKIpionManager *, PhysicsRT_WorldHandle>::iterator old =
        registry.worldsByManager.find(manager);
    if (old != registry.worldsByManager.end())
        registry.worlds.erase(old->second);

    const PhysicsRT_WorldHandle handle = AllocateHandle(registry.nextWorld);
    WorldRecord record;
    record.manager = manager;
    record.context = context;
    record.ownerThread = std::this_thread::get_id();
    registry.worlds.insert(std::make_pair(handle, record));
    registry.worldsByManager[manager] = handle;
}

void PhysicsRT_InternalUnregisterWorld(CKIpionManager *manager)
{
    if (!manager)
        return;

    Registry &registry = GetRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    std::map<CKIpionManager *, PhysicsRT_WorldHandle>::iterator it =
        registry.worldsByManager.find(manager);
    if (it == registry.worldsByManager.end())
        return;
    registry.worlds.erase(it->second);
    registry.worldsByManager.erase(it);
}

void PhysicsRT_InternalRegisterBody(CKIpionManager *manager, int32_t ckId)
{
    if (!manager || ckId <= 0)
        return;

    Registry &registry = GetRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    std::map<CKIpionManager *, PhysicsRT_WorldHandle>::const_iterator managerIt =
        registry.worldsByManager.find(manager);
    if (managerIt == registry.worldsByManager.end())
        return;
    WorldRecord &world = registry.worlds[managerIt->second];
    std::map<int32_t, PhysicsRT_BodyHandle>::iterator old = world.bodiesByCkId.find(ckId);
    if (old != world.bodiesByCkId.end())
    {
        world.ckIdByBody.erase(old->second);
        world.bodiesByCkId.erase(old);
    }

    const PhysicsRT_BodyHandle handle = AllocateHandle(registry.nextBody);
    world.bodiesByCkId[ckId] = handle;
    world.ckIdByBody[handle] = ckId;
}

void PhysicsRT_InternalInvalidateBody(CKIpionManager *manager, int32_t ckId)
{
    if (!manager || ckId <= 0)
        return;

    Registry &registry = GetRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    std::map<CKIpionManager *, PhysicsRT_WorldHandle>::const_iterator managerIt =
        registry.worldsByManager.find(manager);
    if (managerIt == registry.worldsByManager.end())
        return;
    WorldRecord &world = registry.worlds[managerIt->second];
    std::map<int32_t, PhysicsRT_BodyHandle>::iterator bodyIt = world.bodiesByCkId.find(ckId);
    if (bodyIt == world.bodiesByCkId.end())
        return;
    world.ckIdByBody.erase(bodyIt->second);
    world.bodiesByCkId.erase(bodyIt);
}

void PhysicsRT_InternalInvalidateAllBodies(CKIpionManager *manager)
{
    if (!manager)
        return;

    Registry &registry = GetRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    std::map<CKIpionManager *, PhysicsRT_WorldHandle>::const_iterator managerIt =
        registry.worldsByManager.find(manager);
    if (managerIt == registry.worldsByManager.end())
        return;
    WorldRecord &world = registry.worlds[managerIt->second];
    world.bodiesByCkId.clear();
    world.ckIdByBody.clear();
}

extern "C" PHYSICSRT_PUBLIC const PhysicsRT_ApiV1 *PHYSICSRT_CALL
PhysicsRT_GetApi(uint32_t requestedAbiVersion)
{
    if (requestedAbiVersion == PHYSICSRT_ABI_VERSION_1)
        return &kApiV1;
    if (requestedAbiVersion == PHYSICSRT_ABI_VERSION_2)
        return &kApiV2.v1;
    return NULL;
}
