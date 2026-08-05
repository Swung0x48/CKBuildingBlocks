/*************************************************************************/
/*	File : LODManager.h
 *
 *	Author :  Francisco Cabrita
 *	Last Modification :
 *
 *	Virtools VI SDK
 *	Copyright (c) 2001, All Rights Reserved.
 *************************************************************************/
#ifndef LODMANAGER_H
#define LODMANAGER_H "$Id:$"

#include "CKDefines.h"
#include "CKBaseManager.h"

extern const char *LODManagerName;

class LODManager;

#define LOD_MANAGER_GUID CKGUID(0x314f3f83, 0x6f0e30)

/*****************************************************/
/*****************************************************/
/*
 *  LOD Parameter
 *
 *****************************************************/
/*****************************************************/

#define CKPGUID_LODOPTIONS CKGUID(0x2b2d2f43, 0x2ab7bc5)

enum
{
    LODAlpha = 1,
    LODMultiMesh = 2,
    LODAnimation = 4,
    LODPatchMesh = 8
    // NOTE: we don't need any LODProgressiveMesh flags because
    // PM is decided in the Mesh Setup.
    // We suppose that if the guy use a PM it's to have a LOD on it.
    // Not like a Patch Mesh that is not necessary used for LOD,
    // but can be used only for Web applications
};

// This is the structure memorized
// in the attribute parameter
class LODOptions
{
public:
    //--- Basic Infos
    int flags;                  // LODAlpha | LODMultiMesh | LODAnimation | LODPatchMesh
    float screenMag, screenMin; // MAG screen %, and MIN screen %
    float facesMag, facesMin;   // MAG screen %, and MIN screen %

    float alphaScreenMag, alphaScreenMin; // MAG screen % for alpha, and MIN screen % for alpha

    //--- Initialisation
    LODOptions() : flags(0),
                   screenMag(0.4f), screenMin(0.01f),
                   facesMag(1.0f), facesMin(0.02f), alphaScreenMag(0.1f), alphaScreenMin(0.05f){};
};

// structure used to store all information
// about the materials of the object (only used for LODAlpha)
class LODstockedAlphaInfo
{
public:
    float alpha;
    VXBLEND_MODE srcmode, destmode;
    CKBOOL alphaBlendWasEnabled;
};

/*****************************************************/
/*****************************************************/
/*
 *  LOD Manager Part
 *
 *****************************************************/
/*****************************************************/
class LODManager : public CKBaseManager
{
    ////////////////////////////////////////////////////////
    //                Public Part                       ////
    ////////////////////////////////////////////////////////
public:
    // CK Callbacks
    DLL_EXPORT virtual CKERROR OnCKInit();
    DLL_EXPORT virtual CKERROR OnCKEnd();
    DLL_EXPORT virtual CKERROR OnCKPostReset();
    DLL_EXPORT virtual CKERROR PostClearAll();
    DLL_EXPORT virtual CKERROR OnCKPlay();
    DLL_EXPORT virtual CKERROR OnCKPause();
    DLL_EXPORT virtual CKERROR OnPreRender(CKRenderContext *rc);

    DLL_EXPORT virtual CKDWORD GetValidFunctionsMask() { return CKMANAGER_FUNC_OnCKInit |
                                                                CKMANAGER_FUNC_OnCKEnd |
                                                                CKMANAGER_FUNC_OnCKPostReset |
                                                                CKMANAGER_FUNC_PostProcess |
                                                                CKMANAGER_FUNC_PostClearAll |
                                                                CKMANAGER_FUNC_OnCKPlay |
                                                                CKMANAGER_FUNC_OnCKPause |
                                                                CKMANAGER_FUNC_OnSequenceToBeDeleted |
                                                                CKMANAGER_FUNC_OnPreRender; }

    //{secret}
    LODManager(CKContext *ctx);
    //{secret}
    ~LODManager();

    ////////////////////////////////////////////////////////
    //                   Methodes                       ////
    ////////////////////////////////////////////////////////
    inline CKParameterOut *HasLODAttribute(CK3dEntity *ent);
    LODOptions *GetLOD(CK3dEntity *ent);
    void RegisterLODAttribute();
    void PreRenderCallbackOfAllLODObjects(CKBOOL add);
    void RestoreNormalLOD(CK3dEntity &ent, CK_RENDEROBJECT_CALLBACK fct);
    void SetObjectLOD(CK3dEntity &ent, const float faceProportion, const float alphaProportion, const LODOptions &lo);

    inline int GetLODAttribute() { return m_LODAttribute; }

    //--- Manager States
    void ActivateLODManager(const CKBOOL active = TRUE);
    inline CKBOOL IsActiveLODManager() { return m_LODActive; }

    inline void SetGlobalLODFactor(const float factor) { m_GlobalLODFactor = factor; }
    inline float GetGlobalLODFactor() { return m_GlobalLODFactor; }

    inline void AllowNormalBuildingForPM(const CKBOOL allow) { m_BuildNormalsForPM = allow; }
    inline CKBOOL IsNormalBuildingForPMAllowed() { return m_BuildNormalsForPM; }

    ////////////////////////////////////////////////////////
    //                   Members                        ////
    ////////////////////////////////////////////////////////
    int m_LODAttribute; // Attribute ID

    //--- convenient variables to avoid redundant
    //--- calculation at run-time
    float m_invScreenWidth2; // 2*tan(0.5*FOV)

    //--- used for LODAlpha	memorization
    XArray<LODstockedAlphaInfo> m_StockedAlphaInfo;
    XArray<CKBYTE> m_VertexAlpha;
    CKMesh *m_CurrentMesh;

protected:
    CKBOOL m_LODActive;
    float m_GlobalLODFactor;
    CKBOOL m_BuildNormalsForPM;
};

#endif
