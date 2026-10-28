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

    if (m_IpionManager)
        ++m_IpionManager->m_ProcessingQueuedCallbacks;
    m_HasCallbacks = FALSE;
    for (int i = 0; i < 3; ++i)
    {
        IVP_U_Vector<PhysicsCallback> &cbs = m_Callbacks[i];
        for (int j = cbs.len() - 1; j >= 0; --j)
        {
            PhysicsCallback *pc = cbs.element_at(j);
            if (!pc || !pc->GetBehavior() || pc->Execute() != 0)
            {
                cbs.remove_at(j);
                delete pc;
            }
        }

        if (cbs.len() != 0)
            m_HasCallbacks = TRUE;
    }
    if (m_IpionManager && m_IpionManager->m_ProcessingQueuedCallbacks > 0)
        --m_IpionManager->m_ProcessingQueuedCallbacks;
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

// ---------------------------------------------------------------------------
// BallanceMMO, engine change #20 (docs/engine-changes.md; the rollback engine
// gaps design, gap Q): access to the PreSimulate queue for the client's
// rollback.  A queued retail entry carries no state of its own - every
// callback class holds the manager, the type and its block, and re-reads its
// inputs from the block when it runs - so an entry is (block, slot), and the
// rollback puts the anchor's entries back by making them again.  Free
// functions only: no class layout changes and no existing path runs them; the
// physics bridge (BallanceMMOCommon/src/physics) calls them at a restore and
// after a replay.  Allocation and deletion stay in this module.

PhysicsCallback *BmmoMakeCallback_Force(CKIpionManager *man, CKBehavior *beh);
PhysicsCallback *BmmoMakeCallback_WakeUp(CKIpionManager *man, CKBehavior *beh);
PhysicsCallback *BmmoMakeCallback_CollisionDetection(CKIpionManager *man, CKBehavior *beh);
PhysicsCallback *BmmoMakeCallback_BallJoint(CKIpionManager *man, CKBehavior *beh);
PhysicsCallback *BmmoMakeCallback_Hinge(CKIpionManager *man, CKBehavior *beh);
PhysicsCallback *BmmoMakeCallback_Slider(CKIpionManager *man, CKBehavior *beh);
PhysicsCallback *BmmoMakeCallback_Spring(CKIpionManager *man, CKBehavior *beh);
PhysicsCallback *BmmoMakeCallback_Buoyancy(CKIpionManager *man, CKBehavior *beh);
PhysicsCallback *BmmoMakeCallback_ContinuousContact(CKIpionManager *man, CKBehavior *beh);
PhysicsCallback *BmmoMakeCallback_DeleteCollisionSurfaces(CKIpionManager *man, CKBehavior *beh);

// The callback the block's own function would queue, constructed - not
// executed and not queued - keyed by the block's prototype (the files'
// SetGuid lines).  NULL for any other prototype.
extern "C" void *BmmoPhysicsCallbackMake(CKIpionManager *man, CKBehavior *beh)
{
    if (!man || !beh)
        return NULL;
    static const struct
    {
        CKDWORD d1, d2;
        PhysicsCallback *(*make)(CKIpionManager *, CKBehavior *);
    } kMakers[] = {
        {0x56e20c57, 0x0b926068, BmmoMakeCallback_Force},
        {0x38b851b5, 0x72ca74ac, BmmoMakeCallback_WakeUp},
        {0x7435488d, 0x201d1188, BmmoMakeCallback_CollisionDetection},
        {0x5e624f0a, 0x35160450, BmmoMakeCallback_BallJoint},
        {0x41cd3653, 0x0de60c1d, BmmoMakeCallback_Hinge},
        {0x2973360e, 0x23d31aa7, BmmoMakeCallback_Slider},
        {0x24a06a3a, 0x07100fce, BmmoMakeCallback_Spring},
        {0x2c015f2b, 0x5b147512, BmmoMakeCallback_Buoyancy},
        {0x199e4cf1, 0x545a78fe, BmmoMakeCallback_ContinuousContact},
        {0x53bf75aa, 0x770c7021, BmmoMakeCallback_DeleteCollisionSurfaces},
    };
    const CKGUID guid = beh->GetPrototypeGuid();
    for (size_t i = 0; i < sizeof(kMakers) / sizeof(kMakers[0]); ++i)
    {
        if (guid.d1 == kMakers[i].d1 && guid.d2 == kMakers[i].d2)
            return kMakers[i].make(man, beh);
    }
    return NULL;
}

static void BmmoPhysicsCallbackRecount(PhysicsCallbackContainer *queue)
{
    queue->m_HasCallbacks = FALSE;
    for (int k = 0; k < 3; ++k)
    {
        if (queue->m_Callbacks[k].len() != 0)
        {
            queue->m_HasCallbacks = TRUE;
            break;
        }
    }
}

// One entry out of the PreSimulate queue (remove_at keeps the order of the
// rest), not deleted.  NULL when there is no such entry.
extern "C" void *BmmoPhysicsCallbackDetach(CKIpionManager *man, int slot, int index)
{
    PhysicsCallbackContainer *queue = man ? man->m_PreSimulateCallbacks : NULL;
    if (!queue || slot < 0 || slot >= 3 || index < 0 || index >= queue->m_Callbacks[slot].len())
        return NULL;
    PhysicsCallback *pc = queue->m_Callbacks[slot].element_at(index);
    queue->m_Callbacks[slot].remove_at(index);
    BmmoPhysicsCallbackRecount(queue);
    return pc;
}

// One entry into the PreSimulate queue at `index` (len appends): appended,
// then moved down with swap_elems (insert_after cannot insert at 0).  Returns
// 1, or 0 for a bad slot or index.
extern "C" int BmmoPhysicsCallbackInsert(CKIpionManager *man, int slot, int index, void *cb)
{
    PhysicsCallbackContainer *queue = man ? man->m_PreSimulateCallbacks : NULL;
    if (!queue || !cb || slot < 0 || slot >= 3 || index < 0 || index > queue->m_Callbacks[slot].len())
        return 0;
    IVP_U_Vector<PhysicsCallback> &cbs = queue->m_Callbacks[slot];
    cbs.add((PhysicsCallback *)cb);
    for (int j = cbs.len() - 1; j > index; --j)
        cbs.swap_elems(j, j - 1);
    queue->m_HasCallbacks = TRUE;
    return 1;
}

// An entry made by BmmoPhysicsCallbackMake or detached, deleted (virtual).
extern "C" void BmmoPhysicsCallbackFree(void *cb)
{
    delete (PhysicsCallback *)cb;
}
