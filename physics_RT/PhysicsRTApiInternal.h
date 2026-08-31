#ifndef PHYSICS_RT_API_INTERNAL_H
#define PHYSICS_RT_API_INTERNAL_H

#include <stdint.h>

class CKContext;
class CKIpionManager;

void PhysicsRT_InternalRegisterWorld(CKIpionManager *manager, CKContext *context);
void PhysicsRT_InternalUnregisterWorld(CKIpionManager *manager);
void PhysicsRT_InternalRegisterBody(CKIpionManager *manager, int32_t ckId);
void PhysicsRT_InternalInvalidateBody(CKIpionManager *manager, int32_t ckId);
void PhysicsRT_InternalInvalidateAllBodies(CKIpionManager *manager);

#endif /* PHYSICS_RT_API_INTERNAL_H */
