//
// Characters.cpp : Defines the initialization routines for the DLL.
//
#include "CKAll.h"

#ifdef CK_LIB
#define RegisterBehaviorDeclarations    Register_Characters_BehaviorDeclarations
#define InitInstance                    _Characters_InitInstance
#define ExitInstance                    _Characters_ExitInstance
#define CKGetPluginInfoCount            CKGet_Characters_PluginInfoCount
#define CKGetPluginInfo                 CKGet_Characters_PluginInfo
#define g_PluginInfo                    g_Characters_PluginInfo
#else
#define RegisterBehaviorDeclarations    RegisterBehaviorDeclarations
#define InitInstance                    InitInstance
#define ExitInstance                    ExitInstance
#define CKGetPluginInfoCount            CKGetPluginInfoCount
#define CKGetPluginInfo                 CKGetPluginInfo
#define g_PluginInfo                    g_PluginInfo
#endif

#define CKPGUID_ANIMTRANSITION CKGUID(0x70e63b54, 0x23c4347c)
#define CKPGUID_SANIMPLAYMODE CKGUID(0xc763303, 0x2b8f4f38)

CKERROR InitInstance(CKContext *context);
CKERROR ExitInstance(CKContext *context);
PLUGIN_EXPORT void RegisterBehaviorDeclarations(XObjectDeclarationArray *reg);

#define CHARACTER_BEHAVIOR CKGUID(0x32de5087, 0xca56355)

CKPluginInfo g_PluginInfo;

PLUGIN_EXPORT int CKGetPluginInfoCount() { return 1; }

PLUGIN_EXPORT CKPluginInfo *CKGetPluginInfo(int Index)
{
    g_PluginInfo.m_Author = "Virtools";
    g_PluginInfo.m_Description = "Character building blocks";
    g_PluginInfo.m_Extension = "";
    g_PluginInfo.m_Type = CKPLUGIN_BEHAVIOR_DLL;
    g_PluginInfo.m_Version = 0x000001;
    g_PluginInfo.m_InitInstanceFct = InitInstance;
    g_PluginInfo.m_ExitInstanceFct = ExitInstance;
    g_PluginInfo.m_GUID = CHARACTER_BEHAVIOR;
    g_PluginInfo.m_Summary = "Character";
    return &g_PluginInfo;
}

/**********************************************************************************/
/**********************************************************************************/
CKERROR InitInstance(CKContext *context)
{
    CKParameterManager *pm = context->GetParameterManager();

    pm->RegisterNewEnum(CKPGUID_ANIMTRANSITION, "Animation Transition", "Break=1,Warp to start=18,Warp best=34,Warp same pos=258");
    pm->RegisterNewEnum(CKPGUID_SANIMPLAYMODE, "Secondary Animation Play Mode", "Stop=0,Play Once=4,Play Loop=8,Play Loop N=64");

    // We hide the parameters (structures and flags)
    CKParameterTypeDesc *param_type;
    if ((param_type = pm->GetParameterTypeDescription(CKPGUID_ANIMTRANSITION)))
        param_type->dwFlags |= CKPARAMETERTYPE_HIDDEN;
    if ((param_type = pm->GetParameterTypeDescription(CKPGUID_SANIMPLAYMODE)))
        param_type->dwFlags |= CKPARAMETERTYPE_HIDDEN;

    return CK_OK;
}

CKERROR ExitInstance(CKContext *context)
{
    CKParameterManager *pm = context->GetParameterManager();

    pm->UnRegisterParameterType(CKPGUID_ANIMTRANSITION);
    pm->UnRegisterParameterType(CKPGUID_SANIMPLAYMODE);

    return CK_OK;
}

void RegisterBehaviorDeclarations(XObjectDeclarationArray *reg)
{
    // Characters/Animation
    RegisterBehavior(reg, FillBehaviorAnimationSynchroDecl);
    RegisterBehavior(reg, FillBehaviorCreateMergedAnimationDecl);
    RegisterBehavior(reg, FillBehaviorCreateBlendedAnimation2Decl);
    RegisterBehavior(reg, FillBehaviorExcludeFromAnimationDecl);
    RegisterBehavior(reg, FillBehaviorSetAnimationFrameDecl);
    RegisterBehavior(reg, FillBehaviorSetAnimationStepDecl);
    RegisterBehavior(reg, FillBehaviorSetBodypartAnimationFrameDecl);
    RegisterBehavior(reg, FillBehaviorSetMergedAnimationFactorDecl);
    RegisterBehavior(reg, FillBehaviorShareCharacterAnimationsDecl);
    RegisterBehavior(reg, FillBehaviorAddAnimationDecl);

    // Level Of Detail
    RegisterBehavior(reg, FillBehaviorAnimationLODDecl);

    // Characters/Basic
    RegisterBehavior(reg, FillBehaviorGetNearestObjectDecl);
    RegisterBehavior(reg, FillBehaviorSetFloorReferenceDecl);

    // Characters/IK
    RegisterBehavior(reg, FillIKControllerDecl);

    // Characters/Movement
    RegisterBehavior(reg, FillBehaviorCharacterControllerDecl);
    RegisterBehavior(reg, FillBehaviorGoToDecl);
    RegisterBehavior(reg, FillBehaviorCharacterPathFollowDecl);
    RegisterBehavior(reg, FillBehaviorCharacterPathFollow2Decl);
    RegisterBehavior(reg, FillBehaviorEnhancedCharacterControllerDecl);
    RegisterBehavior(reg, FillBehaviorUnlimitedControllerDecl);
}