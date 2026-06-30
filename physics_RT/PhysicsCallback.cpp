#include "PhysicsCallback.h"

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

void PhysicsCallbackContainer::Process()
{
    m_HasCallbacks = FALSE;
    for (int i = 0; i < 3; ++i)
    {
        IVP_U_Vector<PhysicsCallback> &cbs = m_Callbacks[i];
        for (int j = cbs.len() - 1; j >= 0; --j)
        {
            PhysicsCallback *pc = cbs.element_at(j);
            if (!pc || !pc->m_Behavior || pc->Execute() != 0)
            {
                cbs.remove_at(j);
                delete pc;
            }
        }

        if (cbs.len() != 0)
            m_HasCallbacks = TRUE;
    }
}

void PhysicsCallbackContainer::Process(PhysicsCallback *pc)
{
    if (!pc)
        return;

    if (!pc->m_Behavior)
    {
        delete pc;
        return;
    }

    if (pc->m_Behavior)
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
