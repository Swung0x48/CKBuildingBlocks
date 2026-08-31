#include "PhysicsRTApi.h"

/* Keep the public boundary consumable by a C compiler, not just extern "C". */
typedef char PhysicsRT_CAssertBuildInfoSize[(sizeof(PhysicsRT_BuildInfo) == 160) ? 1 : -1];
typedef char PhysicsRT_CAssertBodyRefSize[(sizeof(PhysicsRT_BodyRef) == 16) ? 1 : -1];
typedef char PhysicsRT_CAssertBodyStateSize[(sizeof(PhysicsRT_BodyState) == 80) ? 1 : -1];
typedef char PhysicsRT_CAssertBallDescSize[(sizeof(PhysicsRT_BallDesc) == 112) ? 1 : -1];
typedef char PhysicsRT_CAssertForceSize[(sizeof(PhysicsRT_ForceCommand) == 40) ? 1 : -1];
typedef char PhysicsRT_CAssertBodyOffset[(offsetof(PhysicsRT_BodyState, body) == 8) ? 1 : -1];
typedef char PhysicsRT_CAssertPoseOffset[(offsetof(PhysicsRT_BodyState, position) == 24) ? 1 : -1];

int PhysicsRT_CAbiCompileProbe(void)
{
    const PhysicsRT_ApiV1 *api = PhysicsRT_GetApi(PHYSICSRT_ABI_VERSION_1);
    return api != NULL && api->abi_version == PHYSICSRT_ABI_VERSION_1 &&
           api->capture_ball_desc != NULL;
}
