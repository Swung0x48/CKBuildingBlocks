/*************************************************************************/
/*	File : LODManager.cpp
 *
 *************************************************************************/
#include "CKAll.h"
#include "LODManager.h"

#ifdef TIMEBOMB
#include "windows.h"
#endif

const char *LODAttributeCategoryName = "LOD (Level Of Detail)";
const char *LODAttributeName = "LOD Object";
const char *LODManagerName = "LOD (Level Of Detail)";

int LODPreRender(CKRenderContext *dev, CKRenderObject *rent, void *arg);
int LODPostRender(CKRenderContext *dev, CKRenderObject *rent, void *arg);

/*****************************************************/
/*****************************************************/
/*
 *  Misc Functions
 *
 *****************************************************/
/*****************************************************/
float CalcInvScreenWidth(CKContext *ctx, CKRenderContext *rc)
{
    CKCamera *camera = rc->GetAttachedCamera();
    if (!camera)
        return 1.0f;

    float screenWidth = 2.0f * tanf(0.5f * camera->GetFov());
    return 1.0f / (screenWidth * screenWidth);
}

/*****************************************************/
/*****************************************************/
/*
 *  LOD Options Parameter
 *
 *****************************************************/
/*****************************************************/

/*-- LOD Options string conversion function --*/
int LODOptionsStringFunc(CKParameter *p, char *value, CKBOOL ReadFrom)
{
    if (!p)
        return 0;

    LODOptions lo;

    if (ReadFrom) // READ FROM STRING
    {
        if (!value)
            return 0;
        lo.flags = 0;
        char *ptr = value;
        for (int a = 0, powerOfTwo = 1; a < 4; ++a, powerOfTwo *= 2)
        {
            if (!strncmp(ptr, "TRUE:", 5))
            {
                lo.flags |= powerOfTwo;
                ptr += 5;
            }
            else if (!strncmp(ptr, "FALSE:", 6))
                ptr += 6;
            else
                return 0;
        }
        sscanf(ptr, "%g:%g:%g:%g:%g:%g", &lo.screenMag, &lo.screenMin, &lo.facesMag, &lo.facesMin,
               &lo.alphaScreenMag, &lo.alphaScreenMin);
        lo.screenMag *= 0.01f;
        lo.screenMin *= 0.01f;
        lo.facesMag *= 0.01f;
        lo.facesMin *= 0.01f;
        lo.alphaScreenMag *= 0.01f;
        lo.alphaScreenMin *= 0.01f;
        p->SetValue(&lo);
    }
    else // WRITE TO STRING
    {
        p->GetValue(&lo, FALSE);
        char temp[128] = {0};
        snprintf(temp, sizeof(temp), "%s:%s:%s:%s:%g:%g:%g:%g:%g:%g",
                (lo.flags & LODMultiMesh) ? "TRUE" : "FALSE",
                (lo.flags & LODAnimation) ? "TRUE" : "FALSE",
                (lo.flags & LODPatchMesh) ? "TRUE" : "FALSE",
                (lo.flags & LODAlpha) ? "TRUE" : "FALSE",
                lo.screenMag * 100.0f, lo.screenMin * 100.0f, lo.facesMag * 100.0f, lo.facesMin * 100.0f,
                lo.alphaScreenMag * 100.0f, lo.alphaScreenMin * 100.0f);
        if (value)
            strcpy(value, temp);
        return (int)strlen(temp) + 1;
    }
    return 0;
}

/*-- LOD Options create function --*/
CKERROR LODOptionsParameterCreator(CKParameter *p)
{
    // just to put initial values
    LODOptions lo;
    p->SetValue(&lo);

    return CK_OK;
}

/*****************************************************/
/*****************************************************/
/*
 *  LOD Manager Part
 *
 *****************************************************/
/*****************************************************/

// Constructor
LODManager::LODManager(CKContext *ctx) : CKBaseManager(ctx, LOD_MANAGER_GUID, LODManagerName)
{
    ctx->RegisterNewManager(this);

    //-------------------------------------
    //--- register new parameter types
    CKParameterManager *pm = m_Context->GetParameterManager();
    CKParameterTypeDesc param_type;

    // "LOD Options" parameter
    param_type.TypeName = "LOD Options";
    param_type.DefaultSize = sizeof(LODOptions);
    param_type.Guid = CKPGUID_LODOPTIONS;
    param_type.StringFunction = LODOptionsStringFunc;
    param_type.CreateDefaultFunction = LODOptionsParameterCreator;

    pm->RegisterParameterType(&param_type);

    //--- Some initialization
    m_GlobalLODFactor = 1.0f;
    m_LODActive = FALSE;
    m_BuildNormalsForPM = FALSE;
}

// Destructor
LODManager::~LODManager() {}

/***********************************************/
//	LOD Attribute
/***********************************************/

/********************************************************************************
 *	Name:	    HasLODAttribute
 *	Description:	convenience function to check if object has LOD Attribute.
 *	Note:
 ********************************************************************************/
inline CKParameterOut *LODManager::HasLODAttribute(CK3dEntity *ent)
{
    return ent->GetAttributeParameter(m_LODAttribute);
}

/********************************************************************************
 *	Name:	    GetLOD
 *	Description:	convenience function to check if object has LOD Attribute,
 * and get the ptr to the LODOptions
 *	Note:
 ********************************************************************************/
LODOptions *LODManager::GetLOD(CK3dEntity *ent)
{
    if (!ent)
        return NULL;

    CKParameterOut *pout = HasLODAttribute(ent);
    if (!pout)
        return NULL;

    return (LODOptions *)pout->GetReadDataPtr(FALSE);
}

/********************************************************************************
 *	Name:	    LODAttributeCallback
 *	Description:	This function is called each time a LOD Attribute is created,
 * or destroyed
 *	Note:
 ********************************************************************************/
void LODAttributeCallback(int AttribType, CKBOOL Set, CKBeObject *obj, void *arg)
{
    CK3dEntity *ent = (CK3dEntity *)obj;
    if (!ent)
        return;
    LODManager *lm = (LODManager *)arg;
    if (!lm)
        return;

    if (Set) //-- Attribute is SET
    {
        CKParameterOut *pout = lm->HasLODAttribute(ent);
        if (!pout)
            return;

        if (lm->m_Context->IsPlaying()) //-- attribute is added at run-time
        {

            if (lm->IsActiveLODManager())
                ent->AddPreRenderCallBack(LODPreRender, lm);
        }
        else //-- attribute is added in the interface
        {

            LODOptions *lo = (LODOptions *)pout->GetReadDataPtr(FALSE);
            if (!lo)
                return;

            // We automatically preset some flags
            // If objects is a Character...
            if (ent->GetClassID() == CKCID_CHARACTER)
                lo->flags |= LODAnimation;

            // If objects has several meshes...
            if (ent->GetMeshCount() > 1)
                lo->flags |= LODMultiMesh;

            // If objects is a patchmesh...
            CKMesh *currentMesh = ent->GetCurrentMesh();
            if (currentMesh)
            {
                if (currentMesh->GetClassID() == CKCID_PATCHMESH)
                    lo->flags |= LODPatchMesh;
            }
        }
    }
    else
    { //-- Attribute is UNSET
        lm->RestoreNormalLOD(*ent, LODPreRender);
    }
}

/********************************************************************************
 *	Name:	    RegisterLODAttribute
 *	Description: Register (or re-register) the LOD Attribute
 *	Note:
 ********************************************************************************/
void LODManager::RegisterLODAttribute()
{
    // category LOD Attribute
    CKAttributeManager *attman = m_Context->GetAttributeManager();
    int catego_LOD = attman->AddCategory(LODAttributeCategoryName);

    // LOD Attribute
    m_LODAttribute = attman->RegisterNewAttributeType(LODAttributeName, CKPGUID_LODOPTIONS, CKCID_3DENTITY);
    attman->SetAttributeCategory(m_LODAttribute, LODAttributeCategoryName);
    attman->SetAttributeCallbackFunction(m_LODAttribute, LODAttributeCallback, this);
}

/***********************************************/
//	Restore Normal LOD
//	This function removes the LOD Render
// Callback, but also restores the "Normal"
// Object's Level Of Detail (=Max as read in
// the attribute).
/***********************************************/
void LODManager::RestoreNormalLOD(CK3dEntity &ent, CK_RENDEROBJECT_CALLBACK fct)
{
    // remove the render callback
    ent.RemovePreRenderCallBack(LODPreRender, this);

    // restore normal LOD
    LODOptions *lo = GetLOD(&ent);
    if (!lo)
        return;

    SetObjectLOD(ent, lo->facesMag, 1.0f, *lo);
}

/***********************************************/
//	Set Object LOD
// This function sets the LOD of an Object,
// given:
// - ent
// - faceProportion
// - LODOptions put on this object (mainly used for flags)
//
// This function is called in the PreRenderCallback,
// and in RestoreNormalLOD
/***********************************************/
void LODManager::SetObjectLOD(CK3dEntity &ent, const float faceProportion, const float alphaProportion, const LODOptions &lo)
{
    //________________________________________________________
    //                                          MULTI MESH
    if (lo.flags & LODMultiMesh)
    {
        const int meshCount = ent.GetMeshCount();
        if (meshCount >= 2)
        { // there's at least 2 meshes
            int a, faceCount, faceMax = -1;
            CKMesh *mesh;
            //-- find MAX Mesh and MIN Mesh
            for (a = 0; a < meshCount; ++a)
            {
                mesh = ent.GetMesh(a);
                faceCount = mesh->GetFaceCount();
                if (faceCount > faceMax)
                    faceMax = faceCount;
            }

            //-- find appropriate Mesh
            CKMesh *choosenMesh = NULL;

            // wanted face count
            faceCount = (int)(faceMax * faceProportion);
            if (faceCount > faceMax)
                faceCount = faceMax;

            int delta, deltaMin = faceMax;
            for (a = 0; a < meshCount; ++a)
            {
                mesh = ent.GetMesh(a);
                delta = mesh->GetFaceCount() - faceCount;
                if (delta >= 0 && delta < deltaMin)
                {
                    deltaMin = delta;
                    choosenMesh = mesh;
                }
            }

            if (choosenMesh)
                ent.SetCurrentMesh(choosenMesh, FALSE);
        }
    }

    //________________________________________________________
    //                                        ANIMATION
    if ((lo.flags & LODAnimation) && (ent.GetClassID() == CKCID_CHARACTER))
    {
        // LODAnimation is used only on characters
        ((CKCharacter &)ent).SetAnimationLevelOfDetail(faceProportion);
    }

    // memorize current mesh
    m_CurrentMesh = ent.GetCurrentMesh();
    if (!m_CurrentMesh)
        return;

    //________________________________________________________
    //                                          ALPHA_INSTRUCTION
    if (lo.flags & LODAlpha)
    {
        // Do not change anything to materials
        // if alphaProportion is 100%
        if (alphaProportion < 1.0f)
        {
            // To restore material changes
            ent.AddPostRenderCallBack(LODPostRender, this, TRUE);

            if (alphaProportion <= 0.001f)
            {
                // if it's too transparent
                // do not render at all
                ent.SetCurrentMesh(NULL, FALSE);
                return;
            }
            else
            {
                // stock materials info
                // & fade materials
                const int matCount = m_CurrentMesh->GetMaterialCount();
                m_StockedAlphaInfo.Resize(matCount);
                CKMaterial *currentMaterial;
                VxColor newDiffuse;
                for (int a = 0; a < matCount; ++a)
                {
                    if ((currentMaterial = m_CurrentMesh->GetMaterial(a)))
                    {
                        // stock material
                        m_StockedAlphaInfo[a].srcmode = currentMaterial->GetSourceBlend();
                        m_StockedAlphaInfo[a].destmode = currentMaterial->GetDestBlend();
                        m_StockedAlphaInfo[a].alphaBlendWasEnabled = currentMaterial->AlphaBlendEnabled();
                        newDiffuse = currentMaterial->GetDiffuse();
                        m_StockedAlphaInfo[a].alpha = newDiffuse.a;

                        // modify material properties just after stocking it
                        currentMaterial->SetSourceBlend(VXBLEND_SRCALPHA);
                        currentMaterial->SetDestBlend(VXBLEND_INVSRCALPHA);
                        currentMaterial->EnableAlphaBlend();
                        newDiffuse.a *= alphaProportion;
                        currentMaterial->SetDiffuse(newDiffuse);
                    }
                }

                //--- if PRELIT
                if (m_CurrentMesh->GetLitMode() == VX_PRELITMESH)
                {
                    int vCount = m_CurrentMesh->GetVertexCount();

                    m_VertexAlpha.Resize(vCount);

                    CKDWORD colStride;
                    CKDWORD *col = (CKDWORD *)m_CurrentMesh->GetColorsPtr(&colStride);

                    CKBYTE alpha;
                    for (int a = 0; a < vCount; ++a)
                    {
                        // stock alpha
                        alpha = (CKBYTE)ColorGetAlpha(*col);
                        m_VertexAlpha[a] = alpha;

                        // change alpha
                        alpha = (CKBYTE)((float)alpha * alphaProportion);
                        *col = ColorSetAlpha(*col, alpha);

                        col = (CKDWORD *)((CKBYTE *)col + colStride);
                    }
                    m_CurrentMesh->ColorChanged();
                }
            }
        }
    }

    //________________________________________________________
    //                                       PROGRESSIVE MESH
    if (m_CurrentMesh->IsPM())
    {
        m_CurrentMesh->SetVerticesRendered((int)(faceProportion * m_CurrentMesh->GetVertexCount()));

        // tells whether to allow PM normals building or not
        if (m_BuildNormalsForPM)
        {
            m_CurrentMesh->SetFlags(m_CurrentMesh->GetFlags() | VXMESH_PM_BUILDNORM);
            // Note: the flag is removed in Mesh::BuildRenderMesh() function,
            // each time the mesh change from resolution
        }
    }
    else
    {
        //________________________________________________________
        //                                          PATCH MESH
        if ((lo.flags & LODPatchMesh) && (m_CurrentMesh->GetClassID() == CKCID_PATCHMESH))
        {

            // WARNING: changing the iteration count of a patch mesh is very time consuming
            // thus, you should avoid using LOD on 3D Objects sharing the same PatchMesh.

            // iteration Max
            const float itMax = 5.0f;

            // calc wanted iteration
            int iteration = (int)((itMax + 1.0f) * sqrtf(faceProportion) - 0.5f);

            ((CKPatchMesh *)m_CurrentMesh)->SetIterationCount(iteration);
        }
    }
}

/***********************************************/
//	PreRender Callback
/***********************************************/
int LODPreRender(CKRenderContext *rc, CKRenderObject *rent, void *arg)
{
    LODManager *lm = (LODManager *)arg;
    CK3dEntity *ent = (CK3dEntity *)rent;

    LODOptions *lo = lm->GetLOD(ent);
    if (!lo)
        return 0;

    // calc Squared Diameter (approximated to the World Bbox diagonal)
    const VxBbox &box = ent->GetBoundingBox();
    const float squareDiam = SquareMagnitude(box.Max - box.Min);

    // calc Z distance between object and the camera
    // from which the current rendering is done
    CKCamera *camera = rc->GetAttachedCamera();
    if (!camera)
        return 0;
    const VxMatrix &wCamMat = camera->GetWorldMatrix();
    const VxMatrix &wEntMat = ent->GetWorldMatrix();

    const float zDist = DotProduct(wCamMat[2], wEntMat[3] - wCamMat[3]);

    // calc screenObject (proportionnal size of object on screen)
    const float initScreenObject = (squareDiam / (zDist * zDist)) * lm->m_invScreenWidth2;
    float screenObject = initScreenObject;

    // calc faceProportion (proportionnal number of faces)
    const float SMin = lo->screenMin * lo->screenMin;
    const float SMag = lo->screenMag * lo->screenMag;

    float faceProportion, alphaProportion = 1.0f;

    if (SMin < SMag)
    {
        if (screenObject < SMin)
            screenObject = SMin;
        else if (screenObject > SMag)
            screenObject = SMag;
        faceProportion = lo->facesMin + (screenObject - SMin) / (SMag - SMin) * (lo->facesMag - lo->facesMin);
    }
    else
    {
        faceProportion = lo->facesMag;
        if (screenObject < (SMin + SMag) * 0.5f)
            faceProportion = lo->facesMin;
    }

    // Alpha special proportion
    if (lo->flags & LODAlpha)
    {
        float screenObject = initScreenObject;
        const float ASMin = lo->alphaScreenMin * lo->alphaScreenMin;
        const float ASMag = lo->alphaScreenMag * lo->alphaScreenMag;
        if (ASMin < ASMag)
        {

            if (initScreenObject < ASMin)
                screenObject = ASMin;
            else if (initScreenObject > ASMag)
                screenObject = ASMag;
            alphaProportion = (screenObject - ASMin) / (ASMag - ASMin);
        }
        else
        {

            if (screenObject < (ASMin + ASMag) * 0.5f)
                alphaProportion = 0.0f;
        }
    }

    // Multiply Proportions by the Global LOD Factor
    faceProportion *= lm->GetGlobalLODFactor();
    alphaProportion *= lm->GetGlobalLODFactor();

    lm->SetObjectLOD(*ent, faceProportion, alphaProportion, *lo);

    return 0; // no need to update extents
}

/***********************************************/
//	PostRender Callback
/***********************************************/
int LODPostRender(CKRenderContext *rc, CKRenderObject *rent, void *arg)
{
    LODManager *lm = (LODManager *)arg;
    CK3dEntity *ent = (CK3dEntity *)rent;

    //--- Restore mesh (removed if alpha was too small)
    CKMesh *currentMesh = ent->GetCurrentMesh();
    CKBOOL mesh_rendered = TRUE;
    if (!currentMesh)
    {
        mesh_rendered = FALSE;
        if (!lm->m_CurrentMesh)
            return 0;
        currentMesh = lm->m_CurrentMesh;
        ent->SetCurrentMesh(currentMesh, FALSE);
    }

    if (mesh_rendered)
    {
        //--- Restore material properties changed by LODAlpha
        VxColor newDiffuse;

        const int matCount = currentMesh->GetMaterialCount();
        CKMaterial *currentMaterial;
        for (int a = 0; a < matCount; ++a)
        {
            if ((currentMaterial = currentMesh->GetMaterial(a)))
            {

                const LODstockedAlphaInfo &smi = lm->m_StockedAlphaInfo[a];
                currentMaterial->SetSourceBlend(smi.srcmode);
                currentMaterial->SetDestBlend(smi.destmode);
                currentMaterial->EnableAlphaBlend(smi.alphaBlendWasEnabled);
                newDiffuse = currentMaterial->GetDiffuse();
                newDiffuse.a = smi.alpha;
                currentMaterial->SetDiffuse(newDiffuse);
            }
        }

        //--- if PRELIT
        if (currentMesh->GetLitMode() == VX_PRELITMESH)
        {
            int vCount = currentMesh->GetVertexCount();

            lm->m_VertexAlpha.Resize(vCount);

            CKDWORD colStride;
            CKDWORD *col = (CKDWORD *)currentMesh->GetColorsPtr(&colStride);

            for (int a = 0; a < vCount; ++a)
            {
                // restore alpha
                *col = ColorSetAlpha(*col, lm->m_VertexAlpha[a]);

                col = (CKDWORD *)((CKBYTE *)col + colStride);
            }
        }
    }

    return 0; // no need to update extents
}

/***********************************************/
//	Activates the LOD Manager
/***********************************************/
void LODManager::ActivateLODManager(CKBOOL active)
{
    if (active == m_LODActive)
        return;
    PreRenderCallbackOfAllLODObjects(active);
    m_LODActive = active;
}

/***********************************************/
//	Add/Remove PreRender Callbacks to all
// LOD objects
/***********************************************/
void LODManager::PreRenderCallbackOfAllLODObjects(CKBOOL add)
{
    CKAttributeManager *attman = m_Context->GetAttributeManager();
    const XObjectPointerArray &LODObjects = attman->GetGlobalAttributeListPtr(m_LODAttribute);

    CK3dEntity *ent;

    int count = LODObjects.Size();
    for (int a = 0; a < count; ++a)
    {
        ent = (CK3dEntity *)LODObjects[a];
        if (add)
        {
            ent->AddPreRenderCallBack(LODPreRender, this);
        }
        else
        {
            RestoreNormalLOD(*ent, LODPreRender);
        }
    }
}

//_______________________________________________________
//                                              CK EVENTS
/*******************************************************/
// On Play
/*******************************************************/
CKERROR LODManager::OnCKPlay()
{
    // set the invScreenWidth2 for the first time
    CKRenderContext *rc = m_Context->GetPlayerRenderContext();
    if (rc)
        m_invScreenWidth2 = CalcInvScreenWidth(m_Context, rc);

    if (m_Context->IsReseted())
    { // to be sure it's not a un-resume PLAY
        m_GlobalLODFactor = 1.0f;
        m_LODActive = FALSE;
        m_BuildNormalsForPM = FALSE;
        ActivateLODManager();
        return CKBR_OK;
    }

    PreRenderCallbackOfAllLODObjects(m_LODActive);

    return CK_OK;
}

/*******************************************************/
// On Pause
/*******************************************************/
CKERROR LODManager::OnCKPause()
{
    PreRenderCallbackOfAllLODObjects(FALSE);
    return CK_OK;
}

/*******************************************************/
// On the Manager Creation
/*******************************************************/
CKERROR LODManager::OnCKInit()
{
    RegisterLODAttribute();

    return CK_OK;
}

/*******************************************************/
// On the Manager Destruction
/*******************************************************/
CKERROR LODManager::OnCKEnd()
{
    return CK_OK;
}

/*******************************************************/
// Just after Reset
/*******************************************************/
CKERROR LODManager::OnCKPostReset()
{
    ActivateLODManager(FALSE);

    return CK_OK;
}

/*******************************************************/
// Just after ClearAll
/*******************************************************/
CKERROR LODManager::PostClearAll()
{
    RegisterLODAttribute();

    return CK_OK;
}

/*******************************************************/
// Just before Behavioral Process
/*******************************************************/
CKERROR LODManager::OnPreRender(CKRenderContext *rc)
{
    CKAttributeManager *attman = m_Context->GetAttributeManager();
    const XObjectPointerArray &LODObjects = attman->GetGlobalAttributeListPtr(m_LODAttribute);
    if (LODObjects.Size() && IsActiveLODManager()) // do something only if needed
    {
        // update the invScreenWidth2
        m_invScreenWidth2 = CalcInvScreenWidth(m_Context, rc);
    }

    return CK_OK;
}
