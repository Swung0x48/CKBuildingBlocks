#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "CKContext.h"
#include "CKIpionManager.h"
#include "RCK3dObject.h"
#include "RCKMesh.h"
#include "VxQuaternion.h"
#include "ivp_material.hxx"

namespace {

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
        : context(new CKContext(NULL, 0, 0)), manager(new CKIpionManager(context))
    {
        manager->CreateEnvironment();
    }

    CKContext *context;
    CKIpionManager *manager;
};

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

int scriptWakeups = 0;
IVP_Real_Object *lastScriptWakeup = NULL;

void CountScriptWakeup(IVP_Real_Object *object)
{
    ++scriptWakeups;
    lastScriptWakeup = object;
}

class WakeupListener : public IVP_Listener_Object
{
public:
    int revived = 0;
    void event_object_deleted(IVP_Event_Object *) override {}
    void event_object_created(IVP_Event_Object *) override {}
    void event_object_revived(IVP_Event_Object *) override { ++revived; }
    void event_object_frozen(IVP_Event_Object *) override {}
};

void ScriptWakeupsAreDistinctFromSimulationRevival()
{
    PhysicsFixture &fixture = GetPhysicsFixture();
    CKIpionManager &manager = *fixture.manager;
    // This fixture creates its environment directly, without the CK init
    // callback which normally starts the manager's clock.
    manager.SetTimeFactor(1.0f);
    RCK3dObject entity(fixture.context, "ScriptWakeupProbe");
    IVP_Material_Simple material(0.4f, 0.5f);
    WakeupListener listener;
    manager.GetEnvironment()->add_listener_object_global(&listener);
    void (*previousObserver)(IVP_Real_Object *) = manager.m_ScriptWakeupObserver;
    struct Cleanup
    {
        CKIpionManager &manager;
        CK3dEntity *entity;
        WakeupListener *listener;
        void (*previousObserver)(IVP_Real_Object *);
        ~Cleanup()
        {
            manager.m_ScriptWakeupObserver = previousObserver;
            manager.GetEnvironment()->remove_listener_object_global(listener);
            DeletePhysicsObject(manager, *entity);
        }
    } cleanup{manager, AsEntity(entity), &listener, previousObserver};
    scriptWakeups = 0;
    lastScriptWakeup = NULL;
    manager.m_ScriptWakeupObserver = CountScriptWakeup;

    Check(manager.CreatePhysicsObjectOnParameters(
              AsEntity(entity), 0, NULL, 0, NULL, NULL, 0, NULL, 1.0f, NULL, NULL,
              FALSE, &material, 1.0f, const_cast<char *>("RegTest"),
              TRUE, FALSE, TRUE, 0.1f, 0.1f) == CK_OK,
          "Unable to create sleeping wakeup probe");
    IVP_Real_Object *object = manager.GetPhysicsObject(AsEntity(entity))->m_RealObject;
    Check(object->get_movement_state() == IVP_MT_NOT_SIM, "Probe should start asleep");
    Check(scriptWakeups == 0, "Manager creation must not impersonate a script wakeup");

    // This is the same delayed wake used by state restoration and IVP
    // collision paths. It produces a real revived event, but no script intent.
    object->ensure_in_simulation();
    for (int i = 0; i < 4; ++i) manager.Simulate(1000.0f / 66.0f);
    Check(listener.revived == 1, "A direct IVP wake must still reach diagnostics");
    Check(scriptWakeups == 0, "An IVP revival must not become a script wakeup");
    Check(object->disable_simulation() == IVP_TRUE, "Unable to freeze wakeup probe");

    // A callback that only succeeds on a later PreSimulate pass invokes
    // this same path there; its source survives independently of timing.
    manager.WakeUpFromScript(object);
    Check(scriptWakeups == 1 && lastScriptWakeup == object,
          "An explicit script wake must identify its target");
    for (int i = 0; i < 4; ++i) manager.Simulate(1000.0f / 66.0f);
    Check(listener.revived == 2, "Script wakes must keep the ordinary IVP lifecycle");
    manager.WakeUpFromScript(object);
    Check(scriptWakeups == 2, "Explicit intent must survive an already-awake predicted target");

    Check(object->disable_simulation() == IVP_TRUE, "Unable to freeze before restoring the probe");
    IVP_U_Quat rotation;
    IVP_U_Point position;
    object->calc_at_quaternion(manager.GetSimulationTime(), &rotation, &position);
    position.k[0] += 5.0;
    object->beam_object_to_new_position(&rotation, &position, IVP_TRUE);
    object->ensure_in_simulation();
    for (int i = 0; i < 4; ++i) manager.Simulate(1000.0f / 66.0f);
    Check(listener.revived == 3, "Restoring a sleeping body must retain its diagnostic revival");
    Check(scriptWakeups == 2, "A restored and re-simulated body must not produce script intent");

    DeletePhysicsObject(manager, *AsEntity(entity));
    Check(manager.CreatePhysicsObjectOnParameters(
              AsEntity(entity), 0, NULL, 0, NULL, NULL, 0, NULL, 1.0f, NULL, NULL,
              TRUE, &material, 1.0f, const_cast<char *>("RegTest"),
              TRUE, FALSE, TRUE, 0.1f, 0.1f) == CK_OK,
          "Unable to create fixed wakeup probe");
    object = manager.GetPhysicsObject(AsEntity(entity))->m_RealObject;
    manager.WakeUpFromScript(object);
    Check(scriptWakeups == 2, "A fixed target must not generate a wakeup report");
}

IVP_Real_Object *CreateFallingProbe(CKIpionManager &manager, CK3dEntity &entity, IVP_Material &material,
                                    float x)
{
    entity.SetPosition3f(x, 0.0f, 0.0f);
    Check(manager.CreatePhysicsObjectOnParameters(
              &entity, 0, NULL, 0, NULL, NULL, 0, NULL, 1.0f, NULL, NULL,
              FALSE, &material, 1.0f, const_cast<char *>("RegTest"),
              FALSE, FALSE, TRUE, 0.1f, 0.1f) == CK_OK,
          "Unable to create falling probe");
    PhysicsObject *object = manager.GetPhysicsObject(&entity);
    Check(object && object->m_RealObject, "Falling probe has no physics object");
    return object->m_RealObject;
}

double FallenDistance(IVP_Real_Object *object, CKIpionManager &manager, const IVP_U_Point &start)
{
    IVP_U_Quat rotation;
    IVP_U_Point position;
    object->calc_at_quaternion(manager.GetSimulationTime(), &rotation, &position);
    return position.k[1] - start.k[1];
}

// A body beamed to the pose it is already in must keep moving as if nothing
// had happened.  IVP integrates with a leapfrog - the position of a PSI is
// advanced by the delta written at the PREVIOUS one, and the rotation of the
// next PSI is predicted at this one - and set_transformation throws both away,
// so the writer has to put them back.  This is the engine-side half of the
// BMMO bridge's restore (physics_state.cpp, design 9.25); the control probe
// shows what the same write costs without it.
void BeamingABodyToItsOwnPoseKeepsItsLeapfrogState()
{
    PhysicsFixture &fixture = GetPhysicsFixture();
    CKIpionManager &manager = *fixture.manager;
    manager.SetTimeFactor(1.0f);

    RCK3dObject reference(fixture.context, "LeapfrogReference");
    RCK3dObject restored(fixture.context, "LeapfrogRestored");
    RCK3dObject naive(fixture.context, "LeapfrogNaive");
    IVP_Material_Simple material(0.4f, 0.5f);
    // Collision is off and the probes stand 50 m apart: they fall side by side
    // without ever meeting, so any difference between them is the write.
    IVP_Real_Object *probes[3] = {
        CreateFallingProbe(manager, *AsEntity(reference), material, 0.0f),
        CreateFallingProbe(manager, *AsEntity(restored), material, 50.0f),
        CreateFallingProbe(manager, *AsEntity(naive), material, 100.0f),
    };
    IVP_U_Point starts[3];
    for (int i = 0; i < 3; ++i)
    {
        IVP_U_Quat rotation;
        probes[i]->get_core()->rot_speed.set(0.0f, 2.0f, 0.0f);
        probes[i]->calc_at_quaternion(manager.GetSimulationTime(), &rotation, &starts[i]);
    }
    for (int i = 0; i < 20; ++i) manager.Simulate(1000.0f / 66.0f);

    for (int i = 1; i < 3; ++i)
    {
        IVP_Real_Object *object = probes[i];
        IVP_Core *core = object->get_core();
        IVP_U_Quat rotation;
        IVP_U_Point position;
        object->calc_at_quaternion(manager.GetSimulationTime(), &rotation, &position);
        const IVP_U_Float_Point speed = core->speed;
        const IVP_U_Float_Point rot_speed = core->rot_speed;
        IVP_U_Quat psi_rotation_step;
        psi_rotation_step.set_invert_mult(&core->q_world_f_core_last_psi, &core->q_world_f_core_next_psi);

        object->beam_object_to_new_position(&rotation, &position, IVP_TRUE);
        core->speed.set(&speed);
        core->rot_speed.set(&rot_speed);
        if (i == 1)
        {
            core->delta_world_f_core_psis.set(&core->speed);
            core->q_world_f_core_next_psi.inline_set_mult_quat(&core->q_world_f_core_last_psi, &psi_rotation_step);
            core->q_world_f_core_next_psi.fast_normize_quat();
        }
    }
    Check(probes[1]->get_core()->delta_world_f_core_psis.quad_length() > 0.0f,
          "The restored probe must carry a per-PSI delta again");
    Check(probes[2]->get_core()->delta_world_f_core_psis.quad_length() == 0.0f,
          "set_transformation is expected to zero the per-PSI delta");

    for (int i = 0; i < 20; ++i) manager.Simulate(1000.0f / 66.0f);
    const double fallen[3] = {
        FallenDistance(probes[0], manager, starts[0]),
        FallenDistance(probes[1], manager, starts[1]),
        FallenDistance(probes[2], manager, starts[2]),
    };
    Check(std::fabs(fallen[0]) > 0.5, "The probes must actually be moving at the restore");
    Check(std::fabs(fallen[0] - fallen[1]) < 1e-4,
          "A body restored with its leapfrog state must follow the untouched one");
    Check(std::fabs(fallen[0] - fallen[2]) > 1e-3,
          "A body restored without it must visibly lag behind (one PSI of motion)");

    DeletePhysicsObject(manager, *AsEntity(reference));
    DeletePhysicsObject(manager, *AsEntity(restored));
    DeletePhysicsObject(manager, *AsEntity(naive));
}

// Why the v10 body write only revives a body that is actually frozen: IVP's
// ensure_in_simulation() on a simulated object is not a no-op, it restarts the
// freeze check (ivp_object.cxx), so a per-tick correction through it would keep
// a mechanism awake for as long as the corrections last.
void WakingAnAlreadySimulatedBodyRestartsItsFreezeCheck()
{
    PhysicsFixture &fixture = GetPhysicsFixture();
    CKIpionManager &manager = *fixture.manager;
    manager.SetTimeFactor(1.0f);

    RCK3dObject entity(fixture.context, "FreezeCheckProbe");
    IVP_Material_Simple material(0.4f, 0.5f);
    IVP_Real_Object *object = CreateFallingProbe(manager, *AsEntity(entity), material, 150.0f);
    IVP_Core *core = object->get_core();
    for (int i = 0; i < 10; ++i) manager.Simulate(1000.0f / 66.0f);

    const IVP_Time before = core->time_of_calm_reference[0];
    for (int i = 0; i < 4; ++i) manager.Simulate(1000.0f / 66.0f);
    Check(core->time_of_calm_reference[0].get_time() == before.get_time(),
          "A simulated body's freeze reference must not move on its own here");
    object->ensure_in_simulation();
    Check(core->time_of_calm_reference[0].get_time() > before.get_time(),
          "ensure_in_simulation() on an awake body restarts the freeze check");
    DeletePhysicsObject(manager, *AsEntity(entity));
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
        {"Named surface reuse does not need geometry", &NamedSurfaceReuseDoesNotNeedGeometry},
        {"Unknown named surface does not guess current mesh", &UnknownNamedSurfaceDoesNotGuessCurrentMesh},
        {"Physics world matrix updates keep scale stable", &PhysicsWorldMatrixUpdatesKeepWorldScaleStable},
        {"Script wakeups are distinct from simulation revival", &ScriptWakeupsAreDistinctFromSimulationRevival},
        {"Beaming a body to its own pose keeps its leapfrog state", &BeamingABodyToItsOwnPoseKeepsItsLeapfrogState},
        {"Waking an already simulated body restarts its freeze check", &WakingAnAlreadySimulatedBodyRestartsItsFreezeCheck},
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
