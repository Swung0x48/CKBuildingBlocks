//
// MeshModifiers.cpp : Defines the initialization routines for the DLL.
//
#include "CKAll.h"
#include "Behaviors/LODManager.h"

#ifdef CK_LIB
#define RegisterBehaviorDeclarations    Register_MeshModifiers_BehaviorDeclarations
#define InitInstance                    _MeshModifiers_InitInstance
#define ExitInstance                    _MeshModifiers_ExitInstance
#define CKGetPluginInfoCount            CKGet_MeshModifiers_PluginInfoCount
#define CKGetPluginInfo                 CKGet_MeshModifiers_PluginInfo
#define g_PluginInfo                    g_MeshModifiers_PluginInfo
#else
#define RegisterBehaviorDeclarations    RegisterBehaviorDeclarations
#define InitInstance                    InitInstance
#define ExitInstance                    ExitInstance
#define CKGetPluginInfoCount            CKGetPluginInfoCount
#define CKGetPluginInfo                 CKGetPluginInfo
#define g_PluginInfo                    g_PluginInfo
#endif

#define CKPGUID_COLORCHANNELTYPE CKDEFINEGUID(0x39d40df7, 0x48433312)
#define MESHMODIF_BEHAVIOR CKGUID(0x68ca037d, 0x6bef5e1a)

CKERROR CreateLODManager(CKContext *context);
CKERROR InitInstance(CKContext *context);
CKERROR ExitInstance(CKContext *context);
PLUGIN_EXPORT void RegisterBehaviorDeclarations(XObjectDeclarationArray *reg);

/***********************************************/
/*       PLUGINS INFO
 ***********************************************/

CKPluginInfo g_PluginInfo;

PLUGIN_EXPORT int CKGetPluginInfoCount() { return 2; }

PLUGIN_EXPORT CKPluginInfo *CKGetPluginInfo(int Index)
{
    switch (Index)
    {
    case 0:
        g_PluginInfo.m_Author = "Virtools";
        g_PluginInfo.m_Description = "Mesh Modification building blocks";
        g_PluginInfo.m_Extension = "";
        g_PluginInfo.m_Type = CKPLUGIN_BEHAVIOR_DLL;
        g_PluginInfo.m_Version = 0x000001;
        g_PluginInfo.m_InitInstanceFct = InitInstance;
        g_PluginInfo.m_ExitInstanceFct = ExitInstance;
        g_PluginInfo.m_GUID = MESHMODIF_BEHAVIOR;
        g_PluginInfo.m_Summary = "Mesh Modifications";
        break;
    case 1:
        g_PluginInfo.m_Author = "Virtools";
        g_PluginInfo.m_Description = "Level Of Detail Manager";
        g_PluginInfo.m_Extension = "";
        g_PluginInfo.m_Type = CKPLUGIN_MANAGER_DLL;
        g_PluginInfo.m_Version = 0x000001;
        g_PluginInfo.m_InitInstanceFct = CreateLODManager;
        g_PluginInfo.m_ExitInstanceFct = NULL;
        g_PluginInfo.m_GUID = LOD_MANAGER_GUID;
        g_PluginInfo.m_Summary = LODManagerName;
        break;
    }
    return &g_PluginInfo;
}

CKERROR CreateLODManager(CKContext *context)
{
    new LODManager(context); // create the LOD Manager
    // rem: the manager will be deleted automatically

    return CK_OK;
}

CKERROR InitInstance(CKContext *context)
{
    CKParameterManager *pm = context->GetParameterManager();
    pm->RegisterNewEnum(CKPGUID_COLORCHANNELTYPE, "RGB Component", "Red=0,Green=1,Blue=2");

    CKParameterTypeDesc *ptype;

#define CKPGUID_LODMANAGEROPTIONS_SETTING CKDEFINEGUID(0x3c5302cc, 0x2a611067)
    pm->RegisterNewFlags(CKPGUID_LODMANAGEROPTIONS_SETTING, "", "Activate=1,Global LOD Factor=2,Build Normals For Progressive Meshes=4");
    ptype = pm->GetParameterTypeDescription(CKPGUID_LODMANAGEROPTIONS_SETTING);
    ptype->dwFlags |= CKPARAMETERTYPE_HIDDEN;

#define CKPGUID_SETLODATTRIBUTE_SETTING CKDEFINEGUID(0x7e4417e1, 0x5d0d45b3)
    pm->RegisterNewFlags(CKPGUID_SETLODATTRIBUTE_SETTING, "", "Multiple Meshes=1,Character Animation=2,Patch Mesh=4,Alpha=8,Screen Mag=16,Screen Min=32,Face Mag=64,Face Min=128,Screen Alpha Mag=256,Screen Alpha Min=512");
    ptype = pm->GetParameterTypeDescription(CKPGUID_SETLODATTRIBUTE_SETTING);
    ptype->dwFlags |= CKPARAMETERTYPE_HIDDEN;

    return CK_OK;
}

CKERROR ExitInstance(CKContext *context)
{
    CKParameterManager *pm = context->GetParameterManager();
    pm->UnRegisterParameterType(CKPGUID_COLORCHANNELTYPE);

    return CK_OK;
}

/**********************************************************************************/
/**********************************************************************************/

void RegisterBehaviorDeclarations(XObjectDeclarationArray *reg)
{
    // Basic
    RegisterBehavior(reg, FillBehaviorSetVertexDecl);
    RegisterBehavior(reg, FillBehaviorSetFaceDecl);
    RegisterBehavior(reg, FillBehaviorSetVertexCountDecl);
    RegisterBehavior(reg, FillBehaviorSetFaceCountDecl);
    RegisterBehavior(reg, FillBehaviorPrecomputeLightingDecl);

    // Mesh Modifications/Deformation
    RegisterBehavior(reg, FillBehaviorBendDecl);
    RegisterBehavior(reg, FillBehaviorExplodeDecl);
    RegisterBehavior(reg, FillBehaviorMeshMorpherDecl);
    RegisterBehavior(reg, FillBehaviorNoiseDecl);
    RegisterBehavior(reg, FillBehaviorMeshSinusDecl);
    RegisterBehavior(reg, FillBehaviorMultiMeshMorpherDecl);
    RegisterBehavior(reg, FillBehaviorSetPatchMeshStepsDecl);
    RegisterBehavior(reg, FillBehaviorSkinJoinDecl);
    RegisterBehavior(reg, FillBehaviorStretchDecl);
    RegisterBehavior(reg, FillBehaviorTaperDecl);
    RegisterBehavior(reg, FillBehaviorTwistDecl);

    // Mesh Modifications/Local Deformation
    RegisterBehavior(reg, FillBehaviorTextureDisplaceDecl);
    RegisterBehavior(reg, FillBehaviorChangeReferentialDecl);
    RegisterBehavior(reg, FillBehaviorVerticesTranslateDecl);
    RegisterBehavior(reg, FillBehaviorInverseWindingDecl);

    // Mesh Modifications/Multi Mesh
    RegisterBehavior(reg, FillBehaviorAddMeshDecl);
    RegisterBehavior(reg, FillBehaviorRemoveMeshDecl);
    RegisterBehavior(reg, FillBehaviorSelectMeshDecl);

    // Optimizations/Level Of Detail
    RegisterBehavior(reg, FillBehaviorLevelOfDetailDecl); // LOD MultiMesh OBSOLETE

    RegisterBehavior(reg, FillBehaviorLODManagerOptionsDecl); // LOD Manager Options
    RegisterBehavior(reg, FillBehaviorSetLODAttributeDecl);   // Set LOD Attribute
    RegisterBehavior(reg, FillBehaviorGetLODAttributeDecl);   // Get LOD Attribute
    RegisterBehavior(reg, FillBehaviorSetProgressiveMeshOptionsDecl);
}
