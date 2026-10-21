/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
//
//		           Physics Force
//
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
#include "CKAll.h"

#include "CKIpionManager.h"

CKObjectDeclaration *FillBehaviorPhysicsForceDecl();
CKERROR CreatePhysicsForceProto(CKBehaviorPrototype **pproto);
int PhysicsForce(const CKBehaviorContext &behcontext);
CKERROR PhysicsForceCallBack(const CKBehaviorContext &behcontext);

CKObjectDeclaration *FillBehaviorPhysicsForceDecl()
{
    CKObjectDeclaration *od = CreateCKObjectDeclaration("SetPhysicsForce");
    od->SetDescription("Physics Force");
    od->SetCategory("Physics");
    od->SetType(CKDLL_BEHAVIORPROTOTYPE);
    od->SetGuid(CKGUID(0x56e20c57, 0xb926068));
    od->SetAuthorGuid(TERRATOOLS_GUID);
    od->SetAuthorName("Terratools");
    od->SetVersion(0x00010000);
    od->SetCreationFunction(CreatePhysicsForceProto);
    od->SetCompatibleClassId(CKCID_3DENTITY);
    return od;
}

CKERROR CreatePhysicsForceProto(CKBehaviorPrototype **pproto)
{
    CKBehaviorPrototype *proto = CreateCKBehaviorPrototype("SetPhysicsForce");
    if (!proto)
        return CKERR_OUTOFMEMORY;

    proto->DeclareInput("Create");
    proto->DeclareInput("Shutdown");

    proto->DeclareOutput("Out1");
    proto->DeclareOutput("Out2");

    proto->DeclareInParameter("Position", CKPGUID_VECTOR, "0,0,0");
    proto->DeclareInParameter("Pos Referential", CKPGUID_3DENTITY);
    proto->DeclareInParameter("Direction", CKPGUID_VECTOR, "0,0,1");
    proto->DeclareInParameter("Direction Ref", CKPGUID_3DENTITY);
    proto->DeclareInParameter("Force Value", CKPGUID_FLOAT, "10");

    proto->DeclareLocalParameter("IVP_Handle", CKPGUID_POINTER);

    proto->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);
    proto->SetFunction(PhysicsForce);

    proto->SetBehaviorFlags(CKBEHAVIOR_TARGETABLE);
    proto->SetBehaviorCallbackFct(PhysicsForceCallBack);

    *pproto = proto;
    return CK_OK;
}

#define POSITION 0
#define REFERENTIAL 1
#define DIRECTION 2
#define DIRECTION_REF 3
#define FORCE_VALUE 4

class PhysicsControllerForce : public IVP_Controller_Independent
{
public:
    PhysicsControllerForce(IVP_Real_Object *obj, const IVP_U_Point &pos, const IVP_U_Point &force)
    {
        m_Core = obj->get_core();
        m_Position = pos;
        m_Force = force;

        if (m_Core)
            IVP_Controller_Manager::add_controller_to_core(this, m_Core);
    }

    ~PhysicsControllerForce()
    {
        if (m_Core)
            IVP_Controller_Manager::remove_controller_from_core(this, m_Core);
    }

    void core_is_going_to_be_deleted_event(IVP_Core *my_core)
    {
        if (my_core == m_Core)
        {
            my_core->rem_core_controller(this);
            m_Core = NULL;
        }
    }

    void do_simulation_controller(IVP_Event_Sim *es, IVP_U_Vector<IVP_Core> *core_list)
    {
        if (core_list && core_list->len() != 0)
        {
            IVP_U_Matrix mat;
            m_Core->calc_at_matrix(m_Core->get_environment()->get_current_time(), &mat);

            IVP_U_Point out;
            mat.vimult3(&m_Force, &out);

            IVP_U_Float_Point pos(m_Position.k[0], m_Position.k[1], m_Position.k[2]);
            IVP_U_Float_Point fcs(out.k[0], out.k[1], out.k[2]);
            IVP_U_Float_Point fws(m_Force.k[0], m_Force.k[1], m_Force.k[2]);
            m_Core->async_push_core(&pos, &fcs, &fws);
        }
    }

    IVP_CONTROLLER_PRIORITY get_controller_priority() { return IVP_CP_ACTUATOR; };

    // BallanceMMO engine change #18: the controller is named after the block
    // that made it ("rt:force/<behavior id>"), so that two force blocks on one
    // body are two controllers the savestate can tell apart when it puts the
    // core's controller order back.  IVP reads the name for diagnostics only.
    const char *get_controller_name() { return m_Name[0] ? m_Name : IVP_Controller_Independent::get_controller_name(); }
    void SetBehavior(CK_ID id) { snprintf(m_Name, sizeof(m_Name), "rt:force/%u", (unsigned)id); }

    IVP_Core *m_Core;
    IVP_U_Point m_Position;
    IVP_U_Point m_Force;
    char m_Name[32] = "";
};

class PhysicsForceCallback : public PhysicsCallback
{
public:
    PhysicsForceCallback(CKIpionManager *man, CKBehavior *beh) : PhysicsCallback(man, beh, 2) {}

    virtual int Execute()
    {
        CKBehavior *beh = GetBehavior();
        if (!beh)
            return 1;

        CK3dEntity *ent = (CK3dEntity *)beh->GetTarget();
        if (!ent)
            return 1;

        VxVector position;
        beh->GetInputParameterValue(POSITION, &position);

        CK3dEntity *referential = (CK3dEntity *)beh->GetInputParameterObject(REFERENTIAL);

        VxVector direction;
        beh->GetInputParameterValue(DIRECTION, &direction);

        CK3dEntity *directionRef = (CK3dEntity *)beh->GetInputParameterObject(DIRECTION_REF);

        float forceValue = 10.0f;
        beh->GetInputParameterValue(FORCE_VALUE, &forceValue);

        PhysicsObject *po = m_IpionManager->GetPhysicsObject(ent);
        if (!po)
            return 0;

        IVP_Real_Object *obj = po->m_RealObject;

        VxVector vec;
        if (directionRef)
            directionRef->TransformVector(&vec, &direction);
        else
            vec = direction;

        IVP_U_Point force(vec.x, vec.y, vec.z);
        if (force.quad_length() <= 0.0001)
            force.set(1.0, 0.0, 0.0);
        else
            force.normize();
        force.mult(forceValue);

        IVP_U_Point pos(position.x, position.y, position.z);
        if (referential != ent)
        {
            VxVector p;
            if (referential)
                referential->Transform(&p, &position);
            else
                p = position;

            IVP_U_Point p1(p.x, p.y, p.z);
            IVP_U_Matrix mat;
            obj->get_core()->calc_at_matrix(obj->get_environment()->get_current_time(), &mat);
            mat.vimult4(&p1, &pos);
        }

        PhysicsControllerForce *controller = new PhysicsControllerForce(obj, pos, force);
        controller->SetBehavior(beh->GetID());
        beh->SetLocalParameterValue(0, &controller);
        if (m_IpionManager->m_ForceObserver)
            m_IpionManager->m_ForceObserver(obj, &force, &pos, m_IpionManager->m_ProcessingQueuedCallbacks > 0 ? 1 : 0);

        return 1;
    }
};

int PhysicsForce(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;
    CKContext *context = behcontext.Context;

    PhysicsControllerForce *controller = NULL;
    beh->GetLocalParameterValue(0, &controller);

    if (beh->IsInputActive(0))
    {
        if (!controller)
        {
            CK3dEntity *ent = (CK3dEntity *)beh->GetTarget();
            if (!ent)
                return CKBR_OWNERERROR;

            CKIpionManager *man = CKIpionManager::GetManager(context);
            if (!man || !man->GetEnvironment() || !man->m_PreSimulateCallbacks)
                return CKBR_GENERICERROR;

            PhysicsForceCallback *cb = new PhysicsForceCallback(man, beh);
            man->m_PreSimulateCallbacks->Process(cb);
        }

        beh->ActivateInput(0, FALSE);
        beh->ActivateOutput(0, TRUE);
    }
    else
    {
        if (controller)
        {
            delete controller;
            controller = NULL;
            beh->SetLocalParameterValue(0, &controller);
        }

        beh->ActivateInput(1, FALSE);
        beh->ActivateOutput(1, TRUE);
    }

    return CKBR_OK;
}

// BallanceMMO: the controller a SetPhysicsForce block holds, for the client's
// rollback.  A replay that re-executes the mechanism scripts has to start them
// from the anchor's state, and the one piece of that state CK does not carry is
// the IVP controller the block created (a raw pointer in local parameter 0):
// the rollback records whether the block had one and where it pushed, and puts
// that back - deleting a controller the anchor did not have, re-creating one it
// had - so the replayed Shutdown / Create edges meet the object they expect.
// The point and the force are the ones the controller was built with, in the
// body's object space (see PhysicsForceCallback::Execute).  Called from the
// physics bridge (BallanceMMOCommon/src/physics/physics_state.cpp).
extern "C" int BmmoPhysicsForceBlockState(CKBehavior *beh, int *exists, double pos[3], double force[3])
{
    if (!beh || !exists || !pos || !force)
        return 0;
    PhysicsControllerForce *controller = NULL;
    beh->GetLocalParameterValue(0, &controller);
    *exists = controller ? 1 : 0;
    for (int k = 0; k < 3; ++k)
    {
        pos[k] = controller ? controller->m_Position.k[k] : 0.0;
        force[k] = controller ? controller->m_Force.k[k] : 0.0;
    }
    return 1;
}

extern "C" int BmmoPhysicsForceBlockRestore(CKBehavior *beh, int exists, const double pos[3], const double force[3])
{
    if (!beh || !pos || !force)
        return 0;
    PhysicsControllerForce *controller = NULL;
    beh->GetLocalParameterValue(0, &controller);
    if (!exists)
    {
        if (controller)
        {
            delete controller;
            controller = NULL;
            beh->SetLocalParameterValue(0, &controller);
        }
        return 1;
    }
    if (controller)
    {
        controller->m_Position.set(pos[0], pos[1], pos[2]);
        controller->m_Force.set(force[0], force[1], force[2]);
        return 1;
    }
    CK3dEntity *ent = (CK3dEntity *)beh->GetTarget();
    CKIpionManager *man = CKIpionManager::GetManager(beh->GetCKContext());
    PhysicsObject *po = (ent && man) ? man->GetPhysicsObject(ent) : NULL;
    if (!po || !po->m_RealObject)
        return 0;
    IVP_U_Point p(pos[0], pos[1], pos[2]);
    IVP_U_Point f(force[0], force[1], force[2]);
    controller = new PhysicsControllerForce(po->m_RealObject, p, f);
    controller->SetBehavior(beh->GetID());
    beh->SetLocalParameterValue(0, &controller);
    return 1;
}

CKERROR PhysicsForceCallBack(const CKBehaviorContext &behcontext)
{
    if (behcontext.CallbackMessage == CKM_BEHAVIORRESET ||
        behcontext.CallbackMessage == CKM_BEHAVIORDELETE ||
        behcontext.CallbackMessage == CKM_BEHAVIORDETACH)
    {
        CKBehavior *beh = behcontext.Behavior;
        PhysicsControllerForce *controller = NULL;
        beh->GetLocalParameterValue(0, &controller);

        CKIpionManager *man = CKIpionManager::GetManager(behcontext.Context);
        if (controller && man && man->GetEnvironment())
            delete controller;

        controller = NULL;
        beh->SetLocalParameterValue(0, &controller);
    }

    return CKBR_OK;
}
