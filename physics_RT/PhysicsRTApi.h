#ifndef PHYSICS_RT_PUBLIC_API_H
#define PHYSICS_RT_PUBLIC_API_H

/*
 * Versioned C ABI for authoritative users of physics_RT.
 *
 * This header deliberately contains no Virtools, IVP, STL, or C++ types.  A
 * world is owned by the CKContext that created the physics manager; all world
 * and body operations must be issued from that context's owner thread.
 */

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#define PHYSICSRT_CALL __cdecl
#if defined(PHYSICSRT_BUILDING_LIBRARY) && !defined(CK_LIB)
#define PHYSICSRT_PUBLIC __declspec(dllexport)
#else
#define PHYSICSRT_PUBLIC
#endif
#elif defined(__GNUC__) && defined(PHYSICSRT_BUILDING_LIBRARY)
#define PHYSICSRT_CALL
#define PHYSICSRT_PUBLIC __attribute__((visibility("default")))
#else
#define PHYSICSRT_CALL
#define PHYSICSRT_PUBLIC
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define PHYSICSRT_ABI_VERSION_1 UINT32_C(1)
#define PHYSICSRT_FIXED_TICK_HZ UINT32_C(66)
#define PHYSICSRT_MAX_FIXED_STEPS_PER_CALL UINT32_C(8)
#define PHYSICSRT_BUILD_ID_CAPACITY UINT32_C(65)
#define PHYSICSRT_COLLISION_GROUP_CAPACITY UINT32_C(8)

typedef uint64_t PhysicsRT_WorldHandle;
typedef uint64_t PhysicsRT_BodyHandle;

#define PHYSICSRT_INVALID_WORLD UINT64_C(0)
#define PHYSICSRT_INVALID_BODY UINT64_C(0)

typedef enum PhysicsRT_Result
{
    PHYSICSRT_OK = 0,
    PHYSICSRT_ERROR_INVALID_ARGUMENT = -1,
    PHYSICSRT_ERROR_UNSUPPORTED_VERSION = -2,
    PHYSICSRT_ERROR_INVALID_WORLD = -3,
    PHYSICSRT_ERROR_INVALID_BODY = -4,
    PHYSICSRT_ERROR_BUFFER_TOO_SMALL = -5,
    PHYSICSRT_ERROR_WRONG_THREAD = -6,
    PHYSICSRT_ERROR_NOT_READY = -7,
    PHYSICSRT_ERROR_INVALID_STATE = -8,
    PHYSICSRT_ERROR_STATIC_BODY = -9,
    PHYSICSRT_ERROR_LIMIT_EXCEEDED = -10,
    PHYSICSRT_ERROR_ALREADY_EXISTS = -11
} PhysicsRT_Result;

typedef enum PhysicsRT_BodyFlags
{
    PHYSICSRT_BODY_ACTIVE = UINT32_C(1) << 0,
    PHYSICSRT_BODY_SLEEPING = UINT32_C(1) << 1,
    PHYSICSRT_BODY_COLLISION_ENABLED = UINT32_C(1) << 2,
    PHYSICSRT_BODY_STATIC = UINT32_C(1) << 3
} PhysicsRT_BodyFlags;

typedef enum PhysicsRT_BallFlags
{
    PHYSICSRT_BALL_START_ACTIVE = UINT32_C(1) << 0,
    PHYSICSRT_BALL_COLLISION_ENABLED = UINT32_C(1) << 1
} PhysicsRT_BallFlags;

typedef enum PhysicsRT_ForceFlags
{
    /* Ignore point_world and apply the command at the center of mass. */
    PHYSICSRT_FORCE_AT_CENTER = UINT32_C(1) << 0
} PhysicsRT_ForceFlags;

typedef struct PhysicsRT_BuildInfo
{
    uint32_t struct_size;
    uint32_t abi_version;
    char source_build_id[PHYSICSRT_BUILD_ID_CAPACITY];
    char solver_compatibility_id[PHYSICSRT_BUILD_ID_CAPACITY];
    float fixed_step_seconds;
    uint32_t max_fixed_steps_per_call;
    uint8_t reserved[12];
} PhysicsRT_BuildInfo;

typedef struct PhysicsRT_BodyRef
{
    uint32_t struct_size;
    int32_t ck_id;
    PhysicsRT_BodyHandle body;
} PhysicsRT_BodyRef;

/*
 * Positions and linear velocities are in Virtools/IVP world units.
 * orientation_xyzw is a normalized world-from-object quaternion.
 * angular_velocity_world is in radians/second in world coordinates.
 */
typedef struct PhysicsRT_BodyState
{
    uint32_t struct_size;
    uint32_t flags;
    PhysicsRT_BodyHandle body;
    int32_t ck_id;
    uint32_t reserved0;
    float position[3];
    float orientation_xyzw[4];
    float linear_velocity_world[3];
    float angular_velocity_world[3];
    uint8_t reserved[4];
} PhysicsRT_BodyState;

typedef struct PhysicsRT_BallDesc
{
    uint32_t struct_size;
    uint32_t flags;
    int32_t ck_id;
    uint32_t reserved0;
    float radius;
    float mass;
    float friction;
    float restitution;
    float linear_damping;
    float angular_damping;
    float position[3];
    float orientation_xyzw[4];
    float linear_velocity_world[3];
    float angular_velocity_world[3];
    char collision_group[PHYSICSRT_COLLISION_GROUP_CAPACITY];
    uint8_t reserved[12];
} PhysicsRT_BallDesc;

typedef struct PhysicsRT_ForceCommand
{
    uint32_t struct_size;
    uint32_t flags;
    PhysicsRT_BodyHandle body;
    float vector_world[3];
    float point_world[3];
} PhysicsRT_ForceCommand;

typedef PhysicsRT_Result(PHYSICSRT_CALL *PhysicsRT_GetBuildInfoFn)(PhysicsRT_BuildInfo *out_info);
typedef PhysicsRT_Result(PHYSICSRT_CALL *PhysicsRT_AcquireWorldFn)(void *ck_context,
                                                                  PhysicsRT_WorldHandle *out_world);
typedef PhysicsRT_Result(PHYSICSRT_CALL *PhysicsRT_ValidateWorldFn)(PhysicsRT_WorldHandle world);
typedef PhysicsRT_Result(PHYSICSRT_CALL *PhysicsRT_SetAuthorityModeFn)(PhysicsRT_WorldHandle world,
                                                                      uint32_t enabled);
typedef PhysicsRT_Result(PHYSICSRT_CALL *PhysicsRT_GetAuthorityModeFn)(PhysicsRT_WorldHandle world,
                                                                      uint32_t *out_enabled);
typedef PhysicsRT_Result(PHYSICSRT_CALL *PhysicsRT_StepFixedFn)(PhysicsRT_WorldHandle world,
                                                               uint32_t tick_count);
typedef PhysicsRT_Result(PHYSICSRT_CALL *PhysicsRT_FindBodyByCkIdFn)(PhysicsRT_WorldHandle world,
                                                                    int32_t ck_id,
                                                                    PhysicsRT_BodyHandle *out_body);
typedef PhysicsRT_Result(PHYSICSRT_CALL *PhysicsRT_EnumerateBodiesFn)(PhysicsRT_WorldHandle world,
                                                                     PhysicsRT_BodyRef *out_bodies,
                                                                     uint32_t capacity,
                                                                     uint32_t *inout_count);
typedef PhysicsRT_Result(PHYSICSRT_CALL *PhysicsRT_ValidateBodyFn)(PhysicsRT_WorldHandle world,
                                                                  PhysicsRT_BodyHandle body);
typedef PhysicsRT_Result(PHYSICSRT_CALL *PhysicsRT_CreateBallFn)(PhysicsRT_WorldHandle world,
                                                                 const PhysicsRT_BallDesc *desc,
                                                                 PhysicsRT_BodyHandle *out_body);
typedef PhysicsRT_Result(PHYSICSRT_CALL *PhysicsRT_DestroyBodyFn)(PhysicsRT_WorldHandle world,
                                                                  PhysicsRT_BodyHandle body);
typedef PhysicsRT_Result(PHYSICSRT_CALL *PhysicsRT_GetBodyStatesFn)(PhysicsRT_WorldHandle world,
                                                                   const PhysicsRT_BodyHandle *bodies,
                                                                   uint32_t body_count,
                                                                   PhysicsRT_BodyState *out_states);
typedef PhysicsRT_Result(PHYSICSRT_CALL *PhysicsRT_SetBodyStatesFn)(PhysicsRT_WorldHandle world,
                                                                   const PhysicsRT_BodyState *states,
                                                                   uint32_t body_count);
typedef PhysicsRT_Result(PHYSICSRT_CALL *PhysicsRT_ApplyCommandsFn)(PhysicsRT_WorldHandle world,
                                                                   const PhysicsRT_ForceCommand *commands,
                                                                   uint32_t command_count);
typedef PhysicsRT_Result(PHYSICSRT_CALL *PhysicsRT_CaptureBallDescFn)(PhysicsRT_WorldHandle world,
                                                                     PhysicsRT_BodyHandle body,
                                                                     PhysicsRT_BallDesc *out_desc);

typedef struct PhysicsRT_ApiV1
{
    uint32_t struct_size;
    uint32_t abi_version;
    PhysicsRT_GetBuildInfoFn get_build_info;
    PhysicsRT_AcquireWorldFn acquire_world;
    PhysicsRT_ValidateWorldFn validate_world;
    PhysicsRT_SetAuthorityModeFn set_authority_mode;
    PhysicsRT_GetAuthorityModeFn get_authority_mode;
    PhysicsRT_StepFixedFn step_fixed;
    PhysicsRT_FindBodyByCkIdFn find_body_by_ck_id;
    PhysicsRT_EnumerateBodiesFn enumerate_bodies;
    PhysicsRT_ValidateBodyFn validate_body;
    PhysicsRT_CreateBallFn create_ball;
    PhysicsRT_DestroyBodyFn destroy_body;
    PhysicsRT_GetBodyStatesFn get_body_states;
    PhysicsRT_SetBodyStatesFn set_body_states;
    PhysicsRT_SetBodyStatesFn reconcile_body_states;
    PhysicsRT_ApplyCommandsFn apply_forces;
    PhysicsRT_ApplyCommandsFn apply_impulses;
    /*
     * Captures a reusable archetype and current state from an existing native
     * IVP ball. The caller replaces ck_id and, when desired, pose/velocities
     * before passing the descriptor to create_ball. Polygon/multi-ball and
     * static bodies are rejected rather than approximated.
     */
    PhysicsRT_CaptureBallDescFn capture_ball_desc;
} PhysicsRT_ApiV1;

/* Returns NULL for every unsupported ABI version. */
PHYSICSRT_PUBLIC const PhysicsRT_ApiV1 *PHYSICSRT_CALL PhysicsRT_GetApi(uint32_t requested_abi_version);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PHYSICS_RT_PUBLIC_API_H */
