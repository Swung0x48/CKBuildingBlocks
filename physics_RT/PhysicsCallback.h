#ifndef PHYSICS_RT_PHYSICSCALLBACK_H
#define PHYSICS_RT_PHYSICSCALLBACK_H

#include "CKBehavior.h"

#include "ivu_types.hxx"
#include "ivu_vector.hxx"

class CKIpionManager;

class PhysicsCallback
{
public:
    PhysicsCallback()
        : m_IpionManager(NULL), m_Type(0), m_BehaviorID(0), m_IsGameplayWrite(FALSE) {}
    PhysicsCallback(CKIpionManager *pm, CKBehavior *beh, int type,
                    CKBOOL isGameplayWrite = FALSE)
        : m_IpionManager(pm), m_Type(type), m_BehaviorID(beh ? beh->GetID() : 0),
          m_IsGameplayWrite(isGameplayWrite ? TRUE : FALSE) {}
    virtual int Execute() = 0;
    virtual ~PhysicsCallback(){};

    CKBehavior *GetBehavior() const;

    CKIpionManager *m_IpionManager;
    int m_Type;
    CK_ID m_BehaviorID;
    CKBOOL m_IsGameplayWrite;
};

class PhysicsCallbackContainer
{
public:
    explicit PhysicsCallbackContainer(CKIpionManager *manager) : m_IpionManager(manager), m_HasCallbacks(FALSE) {}
    ~PhysicsCallbackContainer();

    void Clear();
    void ClearBehavior(CK_ID behaviorID);
    void Process();
    void Process(PhysicsCallback *pc);

    CKIpionManager *m_IpionManager;
    CKBOOL m_HasCallbacks;
    IVP_U_Vector<PhysicsCallback> m_Callbacks[3];
};

#endif // PHYSICS_RT_PHYSICSCALLBACK_H
