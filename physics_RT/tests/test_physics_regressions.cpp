#include <cmath>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "CKContext.h"
#include "CKGlobals.h"
#include "CKIpionManager.h"
#include "PhysicsRTApi.h"
#include "RCK3dObject.h"
#include "RCKMesh.h"
#include "VxQuaternion.h"
#include "ivp_material.hxx"

extern void InitializeCK2_3D();
extern "C" int PhysicsRT_CAbiCompileProbe(void);

namespace {

void InitializeRenderClassesForTests();

void Check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

bool NearlyEqual(float lhs, float rhs, float epsilon = 0.0001f)
{
    return std::fabs(lhs - rhs) <= epsilon;
}

bool NearlyEqual(const VxVector &lhs, const VxVector &rhs, float epsilon = 0.0001f)
{
    return NearlyEqual(lhs.x, rhs.x, epsilon) &&
           NearlyEqual(lhs.y, rhs.y, epsilon) &&
           NearlyEqual(lhs.z, rhs.z, epsilon);
}

std::string ReadSource(const char *relativePath)
{
    std::ifstream file(std::string(CKBB_SOURCE_DIR) + "/" + relativePath, std::ios::binary);
    Check(static_cast<bool>(file), "Unable to read physics source file");
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

void FillTriangle(RCKMesh &mesh)
{
    Check(mesh.SetVertexCount(3) == TRUE, "Unable to allocate collision mesh vertices");

    VxVector positions[3] = {
        VxVector(-1.0f, 0.0f, -1.0f),
        VxVector(1.0f, 0.0f, -1.0f),
        VxVector(0.0f, 0.0f, 1.0f),
    };
    for (int i = 0; i < 3; ++i)
        mesh.SetVertexPosition(i, &positions[i]);
}

CK3dEntity *AsEntity(RCK3dObject &object)
{
    return reinterpret_cast<CK3dEntity *>(&object);
}

struct PhysicsFixture
{
    PhysicsFixture()
        : context(NULL), manager(NULL)
    {
        InitializeRenderClassesForTests();
        context = new CKContext(NULL, 0, 0);
        manager = new CKIpionManager(context);
        manager->CreateEnvironment();
    }

    CKContext *context;
    CKIpionManager *manager;
};

void InitializeRenderClassesForTests()
{
    static bool initialized = false;
    if (!initialized)
    {
        InitializeCK2_3D();
        initialized = true;
    }
}

PhysicsFixture &GetPhysicsFixture()
{
    // CKContext owns registered managers. Keep this integration fixture alive
    // for the process so its manager and IVP environment share one lifetime.
    static PhysicsFixture *fixture = new PhysicsFixture();
    return *fixture;
}

void DeletePhysicsObject(CKIpionManager &manager, CK3dEntity &entity)
{
    PhysicsObject *object = manager.GetPhysicsObject(&entity);
    if (object && object->m_RealObject)
        object->m_RealObject->delete_silently();
}

int CreateNamedPolygon(CKIpionManager &manager, CK3dEntity &entity,
                       CKMesh **convexes, int convexCount,
                       const char *surfaceName, IVP_Material *material)
{
    return manager.CreatePhysicsObjectOnParameters(
        &entity,
        convexCount,
        convexes,
        0,
        NULL,
        NULL,
        0,
        NULL,
        1.0f,
        const_cast<char *>(surfaceName),
        NULL,
        TRUE,
        material,
        1.0f,
        const_cast<char *>("RegTest"),
        TRUE,
        TRUE,
        TRUE,
        0.1f,
        0.1f);
}

void NamedSurfaceReuseDoesNotNeedGeometry()
{
    PhysicsFixture &fixture = GetPhysicsFixture();
    CKIpionManager *manager = fixture.manager;

    RCK3dObject entity(fixture.context, "PaperBall");
    RCKMesh mesh(fixture.context, "Ball_Paper_Mesh");
    FillTriangle(mesh);
    entity.SetCurrentMesh(&mesh);

    CKMesh *convexes[] = {&mesh};
    IVP_Material_Simple firstMaterial(0.4f, 0.5f);
    Check(CreateNamedPolygon(*manager, *AsEntity(entity), convexes, 1,
                             "Ball_Paper_Mesh", &firstMaterial) == CK_OK,
          "Initial named collision surface creation failed");
    Check(manager->GetCollisionSurface("Ball_Paper_Mesh") != NULL,
          "Named collision surface was not cached");
    DeletePhysicsObject(*manager, *AsEntity(entity));

    VxVector changedScale(1.5f, 1.5f, 1.5f);
    entity.SetScale(&changedScale, FALSE, FALSE);

    IVP_Material_Simple respawnMaterial(0.4f, 0.5f);
    Check(CreateNamedPolygon(*manager, *AsEntity(entity), NULL, 0,
                             "Ball_Paper_Mesh", &respawnMaterial) == CK_OK,
          "A cached named collision surface must be reusable without geometry inputs");
    DeletePhysicsObject(*manager, *AsEntity(entity));
}

void UnknownNamedSurfaceDoesNotGuessCurrentMesh()
{
    PhysicsFixture &fixture = GetPhysicsFixture();
    CKIpionManager *manager = fixture.manager;

    RCK3dObject entity(fixture.context, "PaperBall");
    RCKMesh mesh(fixture.context, "MissingSurface");
    FillTriangle(mesh);
    entity.SetCurrentMesh(&mesh);

    IVP_Material_Simple material(0.4f, 0.5f);
    Check(CreateNamedPolygon(*manager, *AsEntity(entity), NULL, 0,
                             "MissingSurface", &material) == CKERR_INVALIDPARAMETER,
          "A cache miss without explicit geometry must fail instead of guessing the current mesh");
}

void PhysicsWorldMatrixUpdatesKeepWorldScaleStable()
{
    PhysicsFixture &fixture = GetPhysicsFixture();
    CKIpionManager *manager = fixture.manager;

    RCK3dObject parent(fixture.context, "Parent");
    RCK3dObject entity(fixture.context, "Child");

    VxVector origin(0.0f, 0.0f, 0.0f);
    VxVector parentScale(1.75f, 0.6f, 1.2f);
    VxQuaternion parentRotation;
    parentRotation.FromEulerAngles(0.2f, 0.4f, -0.3f);
    Check(parent.ConstructWorldMatrix(&origin, &parentScale, &parentRotation) == TRUE,
          "Unable to construct parent transform");

    Check(entity.SetParent(AsEntity(parent), FALSE) == TRUE, "Unable to parent physics entity");
    VxVector localScale(0.8f, 1.1f, 1.35f);
    VxQuaternion localRotation;
    localRotation.FromEulerAngles(-0.15f, 0.3f, 0.25f);
    Check(entity.ConstructLocalMatrix(&origin, &localScale, &localRotation) == TRUE,
          "Unable to construct child transform");

    VxVector initialWorldScale;
    entity.GetScale(&initialWorldScale, FALSE);

    IVP_Material_Simple material(0.4f, 0.5f);
    Check(manager->CreatePhysicsObjectOnParameters(
              AsEntity(entity), 0, NULL, 0, NULL, NULL, 0, NULL, 1.0f, NULL, NULL,
              TRUE, &material, 1.0f, const_cast<char *>("RegTest"),
              TRUE, FALSE, TRUE, 0.1f, 0.1f) == CK_OK,
          "Unable to create physics object for transform test");

    PhysicsObject *physicsObject = manager->GetPhysicsObject(AsEntity(entity));
    Check(physicsObject && physicsObject->m_RealObject,
          "Transform test physics object is missing");
    for (int i = 0; i < 1000; ++i)
        CKIpionManager::UpdateObjectWorldMatrix(physicsObject->m_RealObject);

    VxVector finalWorldScale;
    entity.GetScale(&finalWorldScale, FALSE);
    Check(NearlyEqual(initialWorldScale, finalWorldScale, 0.001f),
          "Physics matrix updates must not feed local-scale error back into world scale");
    DeletePhysicsObject(*manager, *AsEntity(entity));
}

void CollisionCachePolicyIsExplicitAndDeterministic()
{
    const std::string managerSource = ReadSource("physics_RT/CKIpionManager.cpp");
    const std::string behaviorSource = ReadSource("physics_RT/Behaviors/Physicalize.cpp");

    Check(managerSource.find("BuildCollisionSurfaceKey") == std::string::npos,
          "Collision cache keys must not be derived from mesh or transform state");
    Check(managerSource.find("GetCollisionSurface(collisionSurface)") != std::string::npos,
          "Collision Surface must be used directly as the cache key");
    Check(behaviorSource.find("recoveredShapeMesh") == std::string::npos &&
              behaviorSource.find("currentMeshName") == std::string::npos,
          "Physicalize's runtime path must not infer collision geometry from the current render mesh");
}

void GameplayWritePolicyCoversBehaviorGraphMutationPaths()
{
    const std::string callbackSource = ReadSource("physics_RT/PhysicsCallback.cpp");
    const std::string forceSource = ReadSource("physics_RT/Behaviors/PhysicsForce.cpp");
    const std::string impulseSource = ReadSource("physics_RT/Behaviors/PhysicsImpulse.cpp");
    const std::string resetSource = ReadSource("physics_RT/Behaviors/PhysicsReset.cpp");
    const std::string globalsSource = ReadSource("physics_RT/Behaviors/SetPhysicsGlobals.cpp");
    const std::string physicalizeSource = ReadSource("physics_RT/Behaviors/Physicalize.cpp");

    Check(callbackSource.find("pc->IsGameplayWriteAllowed()") != std::string::npos &&
              callbackSource.find("cbs.remove_at(j)") != std::string::npos,
          "Denied deferred gameplay callbacks must be consumed, never replayed after teardown");
    Check(forceSource.find("CanGameplayWrite(m_TargetID)") != std::string::npos,
          "A continuous force controller must consult its target CK_ID policy");
    Check(impulseSource.find("!man->CanGameplayWrite(ent)") != std::string::npos &&
              resetSource.find("man->AreGameplayWritesEnabled()") != std::string::npos &&
              globalsSource.find("!man->AreGameplayWritesEnabled()") != std::string::npos,
          "Targeted impulse and world-global reset writes must use their respective policies");
    Check(physicalizeSource.find("if (!man->CanGameplayWrite(ent))") != std::string::npos,
          "Both Physicalize directions must honor the pre-body CK_ID policy");
}

void CheckPhysics(PhysicsRT_Result actual, PhysicsRT_Result expected, const char *message)
{
    if (actual != expected)
    {
        std::ostringstream detail;
        detail << message << " (expected " << (int)expected << ", got " << (int)actual << ")";
        throw std::runtime_error(detail.str());
    }
}

const PhysicsRT_ApiV1 *GetAuthorityApi()
{
    const PhysicsRT_ApiV1 *api = PhysicsRT_GetApi(PHYSICSRT_ABI_VERSION_1);
    Check(api != NULL, "PhysicsRT_GetApi(1) returned null");
    return api;
}

const PhysicsRT_ApiV2 *GetAuthorityApiV2()
{
    const PhysicsRT_ApiV1 *prefix =
        PhysicsRT_GetApi(PHYSICSRT_ABI_VERSION_2);
    Check(prefix != NULL, "PhysicsRT_GetApi(2) returned null");
    return reinterpret_cast<const PhysicsRT_ApiV2 *>(prefix);
}

PhysicsRT_WorldHandle GetAuthorityWorld()
{
    PhysicsFixture &fixture = GetPhysicsFixture();
    PhysicsRT_WorldHandle world = PHYSICSRT_INVALID_WORLD;
    Check(GetAuthorityApi()->acquire_world(fixture.context, &world) == PHYSICSRT_OK,
          "Unable to acquire the fixture's authority world");
    Check(world != PHYSICSRT_INVALID_WORLD, "Authority world handle is invalid");
    return world;
}

PhysicsRT_BallDesc MakeBallDesc(int32_t ckId, float x)
{
    PhysicsRT_BallDesc desc;
    std::memset(&desc, 0, sizeof(desc));
    desc.struct_size = sizeof(desc);
    desc.flags = PHYSICSRT_BALL_START_ACTIVE | PHYSICSRT_BALL_COLLISION_ENABLED;
    desc.ck_id = ckId;
    desc.radius = 1.0f;
    desc.mass = 1.0f;
    desc.friction = 0.4f;
    desc.restitution = 0.5f;
    desc.linear_damping = 0.0f;
    desc.angular_damping = 0.0f;
    desc.position[0] = x;
    desc.position[1] = 20.0f;
    desc.position[2] = 0.0f;
    desc.orientation_xyzw[3] = 1.0f;
    return desc;
}

void AuthorityAbiLayoutAndVersioningAreStable()
{
    Check(sizeof(PhysicsRT_BuildInfo) == 160, "PhysicsRT_BuildInfo size changed");
    Check(sizeof(PhysicsRT_BodyRef) == 16, "PhysicsRT_BodyRef size changed");
    Check(sizeof(PhysicsRT_BodyState) == 80, "PhysicsRT_BodyState size changed");
    Check(sizeof(PhysicsRT_BallDesc) == 112, "PhysicsRT_BallDesc size changed");
    Check(sizeof(PhysicsRT_ForceCommand) == 40, "PhysicsRT_ForceCommand size changed");
    Check(sizeof(PhysicsRT_GameplayWritePolicyEntry) == 16,
          "PhysicsRT_GameplayWritePolicyEntry size changed");
    Check(offsetof(PhysicsRT_BodyState, body) == 8, "Body handle ABI offset changed");
    Check(offsetof(PhysicsRT_BodyState, position) == 24, "Body pose ABI offset changed");

    const PhysicsRT_ApiV1 *api = GetAuthorityApi();
    Check(PhysicsRT_CAbiCompileProbe() != 0, "The C ABI compile/link probe failed");
    Check(api->struct_size == sizeof(PhysicsRT_ApiV1), "API function table size is wrong");
    Check(api->abi_version == PHYSICSRT_ABI_VERSION_1, "API function table version is wrong");
    const PhysicsRT_ApiV2 *api2 = GetAuthorityApiV2();
    Check(api2->v1.struct_size == sizeof(PhysicsRT_ApiV2) &&
              api2->v1.abi_version == PHYSICSRT_ABI_VERSION_2 &&
              api2->set_gameplay_write_policies != NULL &&
              api2->get_gameplay_write_policies != NULL &&
              api2->clear_gameplay_write_policies != NULL,
          "API v2 table or per-CK policy functions are invalid");
    Check(PhysicsRT_GetApi(0) == NULL && PhysicsRT_GetApi(3) == NULL,
          "Unsupported API versions must return null");
    Check(api->set_gameplay_writes_enabled != NULL &&
              api->get_gameplay_writes_enabled != NULL,
          "Gameplay-write policy functions are missing from the v1 table");

    PhysicsRT_BuildInfo info;
    std::memset(&info, 0, sizeof(info));
    info.struct_size = sizeof(info);
    Check(api->get_build_info(&info) == PHYSICSRT_OK, "Unable to read physics build info");
    Check(info.abi_version == PHYSICSRT_ABI_VERSION_1, "Build info ABI version is wrong");
    Check(info.source_build_id[0] != '\0', "Source build ID is empty");
    Check(info.solver_compatibility_id[0] != '\0', "Solver compatibility ID is empty");
    Check(NearlyEqual(info.fixed_step_seconds, 1.0f / 66.0f, 0.000001f),
          "Build info fixed step is not 1/66 second");
    Check(info.max_fixed_steps_per_call == PHYSICSRT_MAX_FIXED_STEPS_PER_CALL,
          "Build info catch-up limit is wrong");

    PhysicsRT_BuildInfo undersized;
    std::memset(&undersized, 0, sizeof(undersized));
    undersized.struct_size = sizeof(undersized) - 1;
    Check(api->get_build_info(&undersized) == PHYSICSRT_ERROR_INVALID_ARGUMENT,
          "Undersized build info must be rejected");

    PhysicsRT_BuildInfo info2;
    std::memset(&info2, 0, sizeof(info2));
    info2.struct_size = sizeof(info2);
    Check(api2->v1.get_build_info(&info2) == PHYSICSRT_OK &&
              info2.abi_version == PHYSICSRT_ABI_VERSION_2,
          "V2 build info did not report ABI version 2");
}

void AuthorityWorldRejectsCrossThreadAccess()
{
    const PhysicsRT_ApiV1 *api = GetAuthorityApi();
    const PhysicsRT_WorldHandle world = GetAuthorityWorld();
    Check(api->validate_world(world) == PHYSICSRT_OK, "Fixture world is not valid on its owner thread");

    PhysicsRT_Result crossThreadResult = PHYSICSRT_OK;
    std::thread otherThread([&]() { crossThreadResult = api->validate_world(world); });
    otherThread.join();
    Check(crossThreadResult == PHYSICSRT_ERROR_WRONG_THREAD,
          "Physics world access from a non-owner thread must be rejected");
}

void AuthorityHandlesStateAndFixedSteppingAreDeterministic()
{
    PhysicsFixture &fixture = GetPhysicsFixture();
    CKIpionManager *manager = fixture.manager;
    const PhysicsRT_ApiV1 *api = GetAuthorityApi();
    const PhysicsRT_ApiV2 *api2 = GetAuthorityApiV2();
    const PhysicsRT_WorldHandle world = GetAuthorityWorld();

    RCK3dObject entityA(fixture.context, "AuthorityBallA");
    RCK3dObject entityB(fixture.context, "AuthorityBallB");
    RCK3dObject entityClone(fixture.context, "AuthorityBallClone");
    PhysicsRT_BallDesc descA = MakeBallDesc(entityA.GetID(), -10.0f);
    descA.radius = 1.25f;
    descA.mass = 2.5f;
    descA.friction = 0.65f;
    descA.restitution = 0.35f;
    descA.linear_damping = 0.12f;
    descA.angular_damping = 0.23f;
    std::strcpy(descA.collision_group, "Player");
    const PhysicsRT_BallDesc descB = MakeBallDesc(entityB.GetID(), 10.0f);
    PhysicsRT_BodyHandle bodyA = PHYSICSRT_INVALID_BODY;
    PhysicsRT_BodyHandle bodyB = PHYSICSRT_INVALID_BODY;
    CheckPhysics(api->create_ball(world, &descA, &bodyA), PHYSICSRT_OK,
                 "Unable to create the first authority ball");
    CheckPhysics(api->create_ball(world, &descB, &bodyB), PHYSICSRT_OK,
                 "Unable to create the second authority ball");
    Check(bodyA != bodyB && bodyA != PHYSICSRT_INVALID_BODY && bodyB != PHYSICSRT_INVALID_BODY,
          "Authority body handles are not unique");

    PhysicsRT_BodyHandle found = PHYSICSRT_INVALID_BODY;
    Check(api->find_body_by_ck_id(world, entityA.GetID(), &found) == PHYSICSRT_OK && found == bodyA,
          "CK_ID lookup did not return the first body handle");
    Check(api->validate_body(world, bodyA) == PHYSICSRT_OK,
          "A newly created authority body is invalid");

    PhysicsRT_BallDesc captured;
    std::memset(&captured, 0, sizeof(captured));
    captured.struct_size = sizeof(captured);
    Check(api->capture_ball_desc(world, bodyA, &captured) == PHYSICSRT_OK,
          "Unable to capture an existing ball archetype");
    Check(captured.struct_size == sizeof(captured) && captured.ck_id == entityA.GetID(),
          "Captured ball descriptor identity/layout is wrong");
    Check(NearlyEqual(captured.radius, descA.radius) &&
              NearlyEqual(captured.mass, descA.mass) &&
              NearlyEqual(captured.friction, descA.friction) &&
              NearlyEqual(captured.restitution, descA.restitution) &&
              NearlyEqual(captured.linear_damping, descA.linear_damping) &&
              NearlyEqual(captured.angular_damping, descA.angular_damping) &&
              std::strcmp(captured.collision_group, descA.collision_group) == 0,
          "Captured ball descriptor changed physical archetype parameters");
    Check((captured.flags & PHYSICSRT_BALL_START_ACTIVE) != 0 &&
              (captured.flags & PHYSICSRT_BALL_COLLISION_ENABLED) != 0 &&
              NearlyEqual(captured.position[0], descA.position[0]),
          "Captured ball descriptor lost current state");
    PhysicsRT_BallDesc cloneDesc = captured;
    cloneDesc.ck_id = entityClone.GetID();
    cloneDesc.position[0] = 30.0f;
    cloneDesc.position[1] = 20.0f;
    std::memset(cloneDesc.linear_velocity_world, 0,
                sizeof(cloneDesc.linear_velocity_world));
    std::memset(cloneDesc.angular_velocity_world, 0,
                sizeof(cloneDesc.angular_velocity_world));
    PhysicsRT_BodyHandle cloneBody = PHYSICSRT_INVALID_BODY;
    Check(api->create_ball(world, &cloneDesc, &cloneBody) == PHYSICSRT_OK &&
              cloneBody != PHYSICSRT_INVALID_BODY,
          "Unable to create a ball from the captured archetype");
    PhysicsRT_BallDesc cloneRoundTrip;
    std::memset(&cloneRoundTrip, 0, sizeof(cloneRoundTrip));
    cloneRoundTrip.struct_size = sizeof(cloneRoundTrip);
    Check(api->capture_ball_desc(world, cloneBody, &cloneRoundTrip) == PHYSICSRT_OK &&
              NearlyEqual(cloneRoundTrip.radius, descA.radius) &&
              NearlyEqual(cloneRoundTrip.mass, descA.mass) &&
              NearlyEqual(cloneRoundTrip.position[0], cloneDesc.position[0]),
          "Captured ball archetype did not survive clone/create round trip");
    captured.struct_size = sizeof(captured) - 1;
    Check(api->capture_ball_desc(world, bodyA, &captured) ==
              PHYSICSRT_ERROR_INVALID_ARGUMENT,
          "Undersized ball archetype output must be rejected");

    uint32_t bodyCount = 0;
    Check(api->enumerate_bodies(world, NULL, 0, &bodyCount) == PHYSICSRT_ERROR_BUFFER_TOO_SMALL,
          "Enumeration size query did not report a required buffer");
    Check(bodyCount >= 3, "Authority enumeration omitted newly created bodies");
    std::vector<PhysicsRT_BodyRef> refs(bodyCount);
    uint32_t capacity = bodyCount;
    Check(api->enumerate_bodies(world, &refs[0], capacity, &bodyCount) == PHYSICSRT_OK,
          "Unable to enumerate authority bodies");
    for (uint32_t i = 0; i < bodyCount; ++i)
    {
        Check(refs[i].struct_size == sizeof(PhysicsRT_BodyRef), "Body reference size was not initialized");
        if (i != 0)
            Check(refs[i - 1].ck_id < refs[i].ck_id,
                  "Authority body enumeration must be stable and sorted by CK_ID");
    }

    const PhysicsRT_BodyHandle handles[2] = {bodyA, bodyB};
    PhysicsRT_BodyState states[2];
    Check(api->get_body_states(world, handles, 2, states) == PHYSICSRT_OK,
          "Unable to read an atomic authority snapshot");
    Check(states[0].body == bodyA && states[1].body == bodyB,
          "Authority snapshot reordered requested handles");
    Check((states[0].flags & PHYSICSRT_BODY_ACTIVE) != 0 &&
              (states[0].flags & PHYSICSRT_BODY_COLLISION_ENABLED) != 0,
          "Authority ball active/collision state was lost");

    const float originalX = states[0].position[0];
    PhysicsRT_BodyState invalidBatch[2] = {states[0], states[1]};
    invalidBatch[0].position[0] = 99.0f;
    invalidBatch[1].orientation_xyzw[0] = std::numeric_limits<float>::quiet_NaN();
    Check(api->set_body_states(world, invalidBatch, 2) == PHYSICSRT_ERROR_INVALID_STATE,
          "A non-finite quaternion must reject the whole state batch");
    PhysicsRT_BodyState unchanged;
    Check(api->get_body_states(world, &bodyA, 1, &unchanged) == PHYSICSRT_OK,
          "Unable to read state after rejected batch");
    Check(NearlyEqual(unchanged.position[0], originalX),
          "Rejected state batch partially changed another body");

    PhysicsRT_BodyState desired = unchanged;
    desired.position[0] = -3.0f;
    desired.position[1] = 7.0f;
    desired.linear_velocity_world[0] = 2.5f;
    desired.linear_velocity_world[1] = -1.0f;
    desired.linear_velocity_world[2] = 0.25f;
    desired.angular_velocity_world[0] = 0.1f;
    desired.angular_velocity_world[1] = 0.2f;
    desired.angular_velocity_world[2] = 0.3f;
    Check(api->set_body_states(world, &desired, 1) == PHYSICSRT_OK,
          "Unable to apply a valid body state");
    PhysicsRT_BodyState roundTrip;
    Check(api->get_body_states(world, &bodyA, 1, &roundTrip) == PHYSICSRT_OK,
          "Unable to read a body state round trip");
    Check(NearlyEqual(roundTrip.position[0], desired.position[0]) &&
              NearlyEqual(roundTrip.position[1], desired.position[1]) &&
              NearlyEqual(roundTrip.linear_velocity_world[0], desired.linear_velocity_world[0]) &&
              NearlyEqual(roundTrip.angular_velocity_world[2], desired.angular_velocity_world[2], 0.001f),
          "Pose or velocity did not survive an authority state round trip");

    PhysicsRT_BodyState sleeping = roundTrip;
    sleeping.flags &= ~PHYSICSRT_BODY_ACTIVE;
    sleeping.flags |= PHYSICSRT_BODY_SLEEPING;
    sleeping.linear_velocity_world[0] = 0.0f;
    sleeping.linear_velocity_world[1] = 0.0f;
    sleeping.linear_velocity_world[2] = 0.0f;
    sleeping.angular_velocity_world[0] = 0.0f;
    sleeping.angular_velocity_world[1] = 0.0f;
    sleeping.angular_velocity_world[2] = 0.0f;
    Check(api->set_body_states(world, &sleeping, 1) == PHYSICSRT_OK,
          "Unable to apply an authority sleep state");
    Check(api->get_body_states(world, &bodyA, 1, &roundTrip) == PHYSICSRT_OK &&
              (roundTrip.flags & PHYSICSRT_BODY_SLEEPING) != 0 &&
              (roundTrip.flags & PHYSICSRT_BODY_ACTIVE) == 0,
          "Authority sleep state did not round trip");

    desired.flags &= ~PHYSICSRT_BODY_SLEEPING;
    desired.flags |= PHYSICSRT_BODY_ACTIVE;

    Check(api->reconcile_body_states(world, &desired, 1) == PHYSICSRT_ERROR_NOT_READY,
          "Reconcile must require explicit authority mode");
    Check(api->set_authority_mode(world, 1) == PHYSICSRT_OK,
          "Unable to enable authority mode");
    uint32_t authorityEnabled = 0;
    Check(api->get_authority_mode(world, &authorityEnabled) == PHYSICSRT_OK && authorityEnabled == 1,
          "Authority mode did not remain enabled");
    manager->SetDeltaTime(2000.0f);
    Check(NearlyEqual(manager->m_PhysicsDeltaTime, 1.0f / 66.0f, 0.000001f),
          "Authority delta still uses legacy smoothing");

    desired.linear_velocity_world[0] = 0.0f;
    desired.linear_velocity_world[1] = 0.0f;
    desired.linear_velocity_world[2] = 0.0f;
    desired.angular_velocity_world[0] = 0.0f;
    desired.angular_velocity_world[1] = 0.0f;
    desired.angular_velocity_world[2] = 0.0f;
    Check(api->reconcile_body_states(world, &desired, 1) == PHYSICSRT_OK,
          "Authority reconcile rejected a valid body state");

    uint32_t gameplayWritesEnabled = 0;
    Check(api->get_gameplay_writes_enabled(world, &gameplayWritesEnabled) == PHYSICSRT_OK &&
              gameplayWritesEnabled == 1,
          "Gameplay writes must be enabled by default");
    Check(api->set_gameplay_writes_enabled(world, 2) ==
              PHYSICSRT_ERROR_INVALID_ARGUMENT,
          "Non-boolean gameplay-write policy values must be rejected");
    Check(api->set_gameplay_writes_enabled(world, 0) == PHYSICSRT_OK &&
              api->get_gameplay_writes_enabled(world, &gameplayWritesEnabled) == PHYSICSRT_OK &&
              gameplayWritesEnabled == 0,
          "Unable to enter client-mirror gameplay-write mode");

    RCK3dObject prePhysicalizePolicyEntity(
        fixture.context, "PrePhysicalizePolicyEntity");
    PhysicsRT_GameplayWritePolicyEntry policyEntries[2] = {};
    policyEntries[0].struct_size = sizeof(policyEntries[0]);
    policyEntries[0].ck_id = entityA.GetID();
    policyEntries[0].policy = PHYSICSRT_GAMEPLAY_WRITE_ALLOW;
    policyEntries[1].struct_size = sizeof(policyEntries[1]);
    policyEntries[1].ck_id = prePhysicalizePolicyEntity.GetID();
    policyEntries[1].policy = PHYSICSRT_GAMEPLAY_WRITE_ALLOW;
    Check(api2->set_gameplay_write_policies(world, policyEntries, 2) ==
              PHYSICSRT_OK,
          "Unable to set an atomic per-CK gameplay-write policy batch");
    Check(manager->CanGameplayWrite(entityA.GetID()) &&
              manager->CanGameplayWrite(prePhysicalizePolicyEntity.GetID()) &&
              !manager->CanGameplayWrite(entityB.GetID()),
          "Per-CK ALLOW did not override the denied world default before body creation");

    policyEntries[0].policy = PHYSICSRT_GAMEPLAY_WRITE_INHERIT;
    policyEntries[1].policy = PHYSICSRT_GAMEPLAY_WRITE_INHERIT;
    Check(api2->get_gameplay_write_policies(world, policyEntries, 2) ==
              PHYSICSRT_OK &&
              policyEntries[0].policy == PHYSICSRT_GAMEPLAY_WRITE_ALLOW &&
              policyEntries[1].policy == PHYSICSRT_GAMEPLAY_WRITE_ALLOW,
          "Per-CK gameplay-write policy did not round trip");

    PhysicsRT_GameplayWritePolicyEntry invalidPolicies[2] = {
        policyEntries[0], policyEntries[0]};
    invalidPolicies[0].policy = PHYSICSRT_GAMEPLAY_WRITE_DENY;
    invalidPolicies[1].policy = PHYSICSRT_GAMEPLAY_WRITE_DENY;
    Check(api2->set_gameplay_write_policies(world, invalidPolicies, 2) ==
              PHYSICSRT_ERROR_INVALID_ARGUMENT &&
              manager->CanGameplayWrite(entityA.GetID()),
          "An invalid policy batch partially mutated an earlier entry");

    PhysicsRT_ForceCommand force;
    std::memset(&force, 0, sizeof(force));
    force.struct_size = sizeof(force);
    force.flags = PHYSICSRT_FORCE_AT_CENTER;
    force.body = bodyA;
    force.vector_world[0] = 165.0f;
    Check(api->apply_forces(world, &force, 1) == PHYSICSRT_OK,
          "Unable to apply an authority force");
    PhysicsRT_ForceCommand impulse = force;
    impulse.vector_world[0] = 0.25f;
    Check(api->apply_impulses(world, &impulse, 1) == PHYSICSRT_OK,
          "Client-mirror policy incorrectly blocked a C ABI authority impulse");

    const double timeBefore = manager->GetSimulationTime().get_seconds();
    Check(api->step_fixed(world, 2) == PHYSICSRT_OK, "Unable to execute two fixed physics ticks");
    const double elapsed = manager->GetSimulationTime().get_seconds() - timeBefore;
    Check(std::fabs(elapsed - 2.0 / 66.0) <= 0.000001,
          "Authority stepping did not advance by exactly 2/66 seconds");
    Check(api->step_fixed(world, PHYSICSRT_MAX_FIXED_STEPS_PER_CALL + 1) ==
              PHYSICSRT_ERROR_LIMIT_EXCEEDED,
          "Fixed-step catch-up limit was not enforced");

    Check(api->get_body_states(world, &bodyA, 1, &roundTrip) == PHYSICSRT_OK,
          "Unable to read state after authority force");
    Check(roundTrip.linear_velocity_world[0] > 0.5f,
          "Client-mirror policy incorrectly blocked C ABI prediction commands");

    Check(api->reconcile_body_states(world, &desired, 1) == PHYSICSRT_OK,
          "Client-mirror policy incorrectly blocked C ABI reconcile");
    Check(api->set_gameplay_writes_enabled(world, 1) == PHYSICSRT_OK &&
              api->get_gameplay_writes_enabled(world, &gameplayWritesEnabled) == PHYSICSRT_OK &&
              gameplayWritesEnabled == 1,
          "Unable to restore legacy gameplay writes after client-mirror mode");
    Check(api2->clear_gameplay_write_policies(world, NULL, 0) ==
              PHYSICSRT_OK &&
              manager->GetGameplayWritePolicy(entityA.GetID()) ==
                  PHYSICSRT_GAMEPLAY_WRITE_INHERIT,
          "Unable to clear per-CK gameplay-write policies at epoch teardown");

    Check(api->set_authority_mode(world, 0) == PHYSICSRT_OK,
          "Unable to restore legacy physics mode");
    manager->SetDeltaTime(100.0f);
    Check(NearlyEqual(manager->m_PhysicsDeltaTime, 0.025f, 0.000001f),
          "Legacy delta smoothing changed after leaving authority mode");

    Check(api->destroy_body(world, bodyA) == PHYSICSRT_OK,
          "Unable to destroy the first authority body");
    Check(api->validate_body(world, bodyA) == PHYSICSRT_ERROR_INVALID_BODY,
          "Destroyed authority handle remained valid");
    Check(api->destroy_body(world, cloneBody) == PHYSICSRT_OK,
          "Unable to destroy the cloned authority body");
    PhysicsObject *directlyDeleted = manager->GetPhysicsObjectById(entityB.GetID());
    Check(directlyDeleted && directlyDeleted->m_RealObject,
          "Second authority body disappeared before lifecycle test");
    directlyDeleted->m_RealObject->delete_silently();
    Check(api->validate_body(world, bodyB) == PHYSICSRT_ERROR_INVALID_BODY,
          "Engine-side unphysicalize did not invalidate the body handle");
}

void AuthorityBallsCollideWithEachOtherAndStaticTerrain()
{
    PhysicsFixture &fixture = GetPhysicsFixture();
    CKIpionManager *manager = fixture.manager;
    const PhysicsRT_ApiV1 *api = GetAuthorityApi();
    const PhysicsRT_WorldHandle world = GetAuthorityWorld();

    RCK3dObject ballEntityA(fixture.context, "AuthorityCollisionBallA");
    RCK3dObject ballEntityB(fixture.context, "AuthorityCollisionBallB");
    PhysicsRT_BallDesc ballA = MakeBallDesc(ballEntityA.GetID(), -3.0f);
    PhysicsRT_BallDesc ballB = MakeBallDesc(ballEntityB.GetID(), 3.0f);
    ballA.position[1] = 20.0f;
    ballB.position[1] = 20.0f;
    ballA.restitution = 1.0f;
    ballB.restitution = 1.0f;
    ballA.linear_velocity_world[0] = 12.0f;
    ballB.linear_velocity_world[0] = -12.0f;
    // An empty no-collision identifier is intentional. Retail player-ball
    // archetypes often share a non-empty identifier, which makes IVP filter
    // them out against each other. Authority clones must clear that identifier.
    ballA.collision_group[0] = '\0';
    ballB.collision_group[0] = '\0';

    PhysicsRT_BodyHandle bodyA = PHYSICSRT_INVALID_BODY;
    PhysicsRT_BodyHandle bodyB = PHYSICSRT_INVALID_BODY;
    CheckPhysics(api->create_ball(world, &ballA, &bodyA), PHYSICSRT_OK,
                 "Unable to create the first colliding authority ball");
    CheckPhysics(api->create_ball(world, &ballB, &bodyB), PHYSICSRT_OK,
                 "Unable to create the second colliding authority ball");
    CheckPhysics(api->set_authority_mode(world, 1), PHYSICSRT_OK,
                 "Unable to enable fixed stepping for the collision test");

    const PhysicsRT_BodyHandle collisionHandles[2] = {bodyA, bodyB};
    PhysicsRT_BodyState collisionStates[2];
    for (int tick = 0; tick < 30; ++tick)
        CheckPhysics(api->step_fixed(world, 1), PHYSICSRT_OK,
                     "A fixed collision-test tick failed");

    CheckPhysics(api->get_body_states(world, collisionHandles, 2,
                                      collisionStates),
                 PHYSICSRT_OK,
                 "Unable to capture the two-ball collision result");
    const bool ballsStayedOrdered =
        collisionStates[0].position[0] < collisionStates[1].position[0];
    const bool ballsRebounded =
        collisionStates[0].linear_velocity_world[0] < -1.0f &&
        collisionStates[1].linear_velocity_world[0] > 1.0f;

    CheckPhysics(api->step_fixed(world, 1), PHYSICSRT_OK,
                 "Unable to advance after the two-ball collision");

    CheckPhysics(api->destroy_body(world, bodyA), PHYSICSRT_OK,
                 "Unable to destroy the first collision-test ball");
    CheckPhysics(api->destroy_body(world, bodyB), PHYSICSRT_OK,
                 "Unable to destroy the second collision-test ball");

    RCK3dObject terrainEntity(fixture.context, "AuthorityStaticTerrain");
    IVP_Material_Simple terrainMaterial(0.8f, 0.0f);
    Check(CreateNamedPolygon(*manager, *AsEntity(terrainEntity), NULL, 0,
                             "Ball_Paper_Mesh", &terrainMaterial) == CK_OK,
          "Unable to physicalize static authority terrain");

    RCK3dObject fallingEntity(fixture.context, "AuthorityTerrainBall");
    PhysicsRT_BallDesc falling = MakeBallDesc(fallingEntity.GetID(), 0.0f);
    falling.position[1] = 3.0f;
    falling.restitution = 0.0f;
    falling.collision_group[0] = '\0';
    PhysicsRT_BodyHandle fallingBody = PHYSICSRT_INVALID_BODY;
    CheckPhysics(api->create_ball(world, &falling, &fallingBody), PHYSICSRT_OK,
                 "Unable to create the terrain collision-test ball");
    for (int tick = 0; tick < 180; ++tick)
        CheckPhysics(api->step_fixed(world, 1), PHYSICSRT_OK,
                     "A fixed terrain-test tick failed");

    PhysicsRT_BodyState restingState;
    CheckPhysics(api->get_body_states(world, &fallingBody, 1, &restingState),
                 PHYSICSRT_OK,
                 "Unable to capture the terrain collision result");
    const bool terrainStoppedFall = restingState.position[1] > 0.5f &&
        restingState.position[1] < 1.5f &&
        std::fabs(restingState.linear_velocity_world[1]) < 1.0f;

    CheckPhysics(api->destroy_body(world, fallingBody), PHYSICSRT_OK,
                 "Unable to destroy the terrain collision-test ball");
    DeletePhysicsObject(*manager, *AsEntity(terrainEntity));
    CheckPhysics(api->set_authority_mode(world, 0), PHYSICSRT_OK,
                 "Unable to restore legacy stepping after collision tests");

    Check(ballsStayedOrdered,
          "Authority balls crossed through each other instead of colliding");
    Check(ballsRebounded,
          "Authority balls did not exchange direction after collision");
    Check(terrainStoppedFall,
          "Authority ball did not converge on the static terrain surface");
}

void CKShutdownDoesNotRepeatEnvironmentBehaviorReset()
{
    InitializeRenderClassesForTests();

    Check(CKStartUp() == CK_OK, "CKStartUp failed during shutdown test");
    CKContext *context = NULL;
    Check(CKCreateContext(&context, NULL, 0, 0) == CK_OK && context != NULL,
          "CKCreateContext failed during shutdown test");
    CKIpionManager *manager = new CKIpionManager(context);
    manager->CreateEnvironment();

    const PhysicsRT_ApiV1 *api = PhysicsRT_GetApi(PHYSICSRT_ABI_VERSION_1);
    Check(api != NULL, "PhysicsRT_GetApi(1) failed during shutdown test");

    PhysicsRT_WorldHandle world = PHYSICSRT_INVALID_WORLD;
    Check(api->acquire_world(context, &world) == PHYSICSRT_OK,
          "Unable to acquire the shutdown-test physics world");
    Check(api->set_authority_mode(world, 1) == PHYSICSRT_OK,
          "Unable to enable authority mode for shutdown test");
    Check(api->step_fixed(world, 1) == PHYSICSRT_OK,
          "Unable to step the shutdown-test physics world");
    // This is the production CK shutdown sequence: object clearing may destroy
    // the IVP environment before OnCKEnd, then CKContext deletes managers in
    // hash-table order.  Neither OnCKEnd nor the physics manager destructor may
    // walk behaviors after CKObjectManager has potentially already been deleted.
    Check(CKCloseContext(context) == CK_OK,
          "CKCloseContext failed during shutdown test");
    Check(api->validate_world(world) == PHYSICSRT_ERROR_INVALID_WORLD,
          "CK shutdown left the authority world handle valid");

    Check(CKShutdown() == CK_OK, "CKShutdown failed during shutdown test");
}

struct TestCase
{
    const char *name;
    void (*run)();
};

} // namespace

int main()
{
    const TestCase tests[] = {
        {"Collision cache policy is explicit and deterministic", &CollisionCachePolicyIsExplicitAndDeterministic},
        {"Gameplay-write policy covers behavior graph mutation paths",
         &GameplayWritePolicyCoversBehaviorGraphMutationPaths},
        {"Named surface reuse does not need geometry", &NamedSurfaceReuseDoesNotNeedGeometry},
        {"Unknown named surface does not guess current mesh", &UnknownNamedSurfaceDoesNotGuessCurrentMesh},
        {"Physics world matrix updates keep scale stable", &PhysicsWorldMatrixUpdatesKeepWorldScaleStable},
        {"Authority ABI layout and versioning are stable", &AuthorityAbiLayoutAndVersioningAreStable},
        {"Authority world rejects cross-thread access", &AuthorityWorldRejectsCrossThreadAccess},
        {"Authority handles, state, and fixed stepping are deterministic",
         &AuthorityHandlesStateAndFixedSteppingAreDeterministic},
        {"Authority balls collide with each other and static terrain",
         &AuthorityBallsCollideWithEachOtherAndStaticTerrain},
        {"CK shutdown is idempotent after an authority step",
         &CKShutdownDoesNotRepeatEnvironmentBehaviorReset},
    };

    int failed = 0;
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i)
    {
        std::printf("Running test: %s... ", tests[i].name);
        std::fflush(stdout);
        try
        {
            tests[i].run();
            std::printf("PASSED\n");
        }
        catch (const std::exception &error)
        {
            ++failed;
            std::printf("FAILED: %s\n", error.what());
        }
    }

    std::printf("\n=== Test Summary ===\n");
    std::printf("Total tests: %zu\n", sizeof(tests) / sizeof(tests[0]));
    std::printf("Passed: %zu\n", sizeof(tests) / sizeof(tests[0]) - failed);
    std::printf("Failed: %d\n", failed);
    return failed == 0 ? 0 : 1;
}
