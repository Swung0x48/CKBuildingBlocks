#include "PhysicsCallback.h"

#include "CKIpionManager.h"

class PhysicsCallbackProcessingScope
{
public:
    explicit PhysicsCallbackProcessingScope(CKIpionManager *manager) : m_Manager(manager)
    {
        if (m_Manager)
            ++m_Manager->m_CallbackProcessingDepth;
    }

    ~PhysicsCallbackProcessingScope()
    {
        if (m_Manager && m_Manager->m_CallbackProcessingDepth > 0)
            --m_Manager->m_CallbackProcessingDepth;
    }

private:
    CKIpionManager *m_Manager;
};

CKBehavior *PhysicsCallback::GetBehavior() const
{
    if (!m_IpionManager || !m_IpionManager->m_Context || m_BehaviorID == 0)
        return NULL;

    CKObject *obj = m_IpionManager->m_Context->GetObject(m_BehaviorID);
    if (!obj || !CKIsChildClassOf(obj, CKCID_BEHAVIOR))
        return NULL;

    return (CKBehavior *)obj;
}

PhysicsCallbackContainer::~PhysicsCallbackContainer()
{
    Clear();
}

void PhysicsCallbackContainer::Clear()
{
    for (int i = 0; i < 3; ++i)
    {
        IVP_U_Vector<PhysicsCallback> &cbs = m_Callbacks[i];
        for (int j = cbs.len() - 1; j >= 0; --j)
        {
            PhysicsCallback *pc = cbs.element_at(j);
            cbs.remove_at(j);
            delete pc;
        }
    }

    m_HasCallbacks = FALSE;
}

void PhysicsCallbackContainer::ClearBehavior(CK_ID behaviorID)
{
    if (behaviorID == 0)
        return;

    for (int i = 0; i < 3; ++i)
    {
        IVP_U_Vector<PhysicsCallback> &cbs = m_Callbacks[i];
        for (int j = cbs.len() - 1; j >= 0; --j)
        {
            PhysicsCallback *pc = cbs.element_at(j);
            if (pc && pc->m_BehaviorID == behaviorID)
            {
                cbs.remove_at(j);
                delete pc;
            }
        }
    }

    m_HasCallbacks = FALSE;
    for (int k = 0; k < 3; ++k)
    {
        if (m_Callbacks[k].len() != 0)
        {
            m_HasCallbacks = TRUE;
            break;
        }
    }
}

void PhysicsCallbackContainer::Process()
{
    PhysicsCallbackProcessingScope processing(m_IpionManager);

    m_HasCallbacks = FALSE;
    for (int i = 0; i < 3; ++i)
    {
        IVP_U_Vector<PhysicsCallback> &cbs = m_Callbacks[i];
        for (int j = cbs.len() - 1; j >= 0; --j)
        {
            PhysicsCallback *pc = cbs.element_at(j);
            if (!pc || !pc->GetBehavior())
            {
                cbs.remove_at(j);
                delete pc;
                continue;
            }

            if (pc->m_IsGameplayWrite && m_IpionManager &&
                !m_IpionManager->AreGameplayWritesEnabled())
                continue;

            if (pc->Execute() == 0)
                continue;

            cbs.remove_at(j);
            delete pc;
        }

        if (cbs.len() != 0)
            m_HasCallbacks = TRUE;
    }
}

void PhysicsCallbackContainer::Process(PhysicsCallback *pc)
{
    if (!pc)
        return;

    PhysicsCallbackProcessingScope processing(m_IpionManager);

    if (!pc->GetBehavior())
    {
        delete pc;
        return;
    }

    if (pc->m_IsGameplayWrite && m_IpionManager &&
        !m_IpionManager->AreGameplayWritesEnabled())
    {
        if (0 <= pc->m_Type && pc->m_Type < 3)
        {
            m_Callbacks[pc->m_Type].add(pc);
            m_HasCallbacks = TRUE;
        }
        else
        {
            delete pc;
        }
        return;
    }

    if (pc->GetBehavior())
    {
        if (pc->Execute() != 0)
        {
            delete pc;
        }
        else if (0 <= pc->m_Type && pc->m_Type < 3)
        {
            m_Callbacks[pc->m_Type].add(pc);
            m_HasCallbacks = TRUE;
        }
        else
        {
            delete pc;
        }
    }
}
