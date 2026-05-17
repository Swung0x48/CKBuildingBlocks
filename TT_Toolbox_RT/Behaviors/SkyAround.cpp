////////////////////////////////
////////////////////////////////
//
//        TT SkyAround
//
////////////////////////////////
////////////////////////////////
#include "CKAll.h"
#include "ToolboxGuids.h"

CKObjectDeclaration *FillBehaviorTTSkyAroundDecl();
CKERROR CreateTTSkyAroundProto(CKBehaviorPrototype **);
int TTSkyAround(const CKBehaviorContext &behcontext);

CKERROR SkyAroundCallBack(const CKBehaviorContext &behcontext); // CallBack Functioon

static int prerender(CKRenderContext *dev, CKRenderObject *mov, void *arg);  // UnZ-Buffer the rendering of the cube
static int postrender(CKRenderContext *dev, CKRenderObject *mov, void *arg); // Z-Buffer the rendering of the cube
static CK3dEntity *CreateSkyAroundCube(const CKBehaviorContext &behcontext);

static void A_SkyShow(CK3dEntity *ent, CK_OBJECT_SHOWOPTION b)
{
    if ((b == CKHIDE) == (ent->IsVisible()))
        ent->Show(b);
    CKContext *ctx = ent->GetCKContext();
    CKRenderContext *rc = ctx->GetPlayerRenderContext();
    if (rc)
        rc->SetClearBackground(!b);
}

typedef struct
{
    float effect;     // Distortion Effect
    VxMatrix projmat; // Projection Matrix
    CK3dEntity *cube; // Entity of the Sky-Cube
    CK3dEntity *orientation;
} Effect_ProjMat;

static void ResetSkyAroundData(Effect_ProjMat *efp)
{
    if (!efp)
        return;
    memset(efp, 0, sizeof(Effect_ProjMat));
}

static void SetSkyAroundPlanarUv(CKMesh *mesh, int index, float x, float z)
{
    float u = 0.5f;
    float v = 0.5f;
    float length = sqrtf(x * x + z * z);
    if (length > 0.0f)
    {
        float invLength = 1.0f / length;
        u += x * invLength * 0.5f;
        v += z * invLength * 0.5f;
    }
    mesh->SetVertexTextureCoordinates(index, u, v);
}

static void SetSkyAroundVertexPosition(CKMesh *mesh, int index, float x, float y, float z)
{
    VxVector position(x, y, z);
    mesh->SetVertexPosition(index, &position);
}

#define PIN_DISTORTION 0
#define PIN_FILTERING_COLOR 1
#define PIN_ADDITIONAL_COLOR 2
#define PIN_SOURCE_BLEND 3
#define PIN_DEST_BLEND 4
#define PIN_ORIENTATION_OBJECT 5
#define PIN_RADIUS 6
#define PIN_QUADRATIC_SIDEFACES 7
#define PIN_SIDEFACE_HEIGHT 8
#define PIN_SKY_Y 9
#define PIN_SIDE_MATERIALS 10

#define LOCAL_SKYAROUND 0
#define SETTING_SIDE_MATERIALS 1
#define SETTING_TOP_MATERIAL 2
#define SETTING_BOTTOM_MATERIAL 3

CKObjectDeclaration *FillBehaviorTTSkyAroundDecl()
{
    CKObjectDeclaration *od = CreateCKObjectDeclaration("TT SkyAround");
    od->SetDescription("Creates a skyaround object with any number of faces");
    od->SetCategory("TT Toolbox/Object");
    od->SetType(CKDLL_BEHAVIORPROTOTYPE);
    od->SetGuid(CKGUID(0x2C7D5826, 0x688778F4));
    od->SetAuthorGuid(TERRATOOLS_GUID);
    od->SetAuthorName("Terratools");
    od->SetVersion(0x00010000);
    od->SetCreationFunction(CreateTTSkyAroundProto);
    od->SetCompatibleClassId(CKCID_BEOBJECT);
    return od;
}

CKERROR CreateTTSkyAroundProto(CKBehaviorPrototype **pproto)
{
    CKBehaviorPrototype *proto = CreateCKBehaviorPrototype("TT SkyAround");
    if (!proto) return CKERR_OUTOFMEMORY;

    proto->DeclareInput("On");
    proto->DeclareInput("Off");

    proto->DeclareOutput("Exit On");
    proto->DeclareOutput("Exit Off");

    proto->DeclareInParameter("Distortion", CKPGUID_PERCENTAGE, "30");
    proto->DeclareInParameter("Filtering Color", CKPGUID_COLOR, "255, 255, 255, 128");
    proto->DeclareInParameter("Additional Color", CKPGUID_COLOR, "0, 0, 0, 0");
    proto->DeclareInParameter("Source Blend", CKPGUID_BLENDFACTOR, "One");
    proto->DeclareInParameter("Dest Blend", CKPGUID_BLENDFACTOR, "Zero");
    proto->DeclareInParameter("Orientation Object", CKPGUID_3DENTITY);
    proto->DeclareInParameter("Radius", CKPGUID_FLOAT, "70.0f");
    proto->DeclareInParameter("Quadratic SideFaces?", CKPGUID_BOOL, "TRUE");
    proto->DeclareInParameter("SideFace-Heigth", CKPGUID_FLOAT, "10");
    proto->DeclareInParameter("Y-Position of Sky", CKPGUID_FLOAT, "0");

    proto->DeclareLocalParameter("Skyaround", CKPGUID_VOIDBUF);
    proto->DeclareSetting("Side Materials", CKPGUID_INT, "4");
    proto->DeclareSetting("Top Material", CKPGUID_BOOL, "True");
    proto->DeclareSetting("Bottom Material", CKPGUID_BOOL, "True");

    proto->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);
    proto->SetFunction(TTSkyAround);

    proto->SetBehaviorCallbackFct(SkyAroundCallBack);

    *pproto = proto;
    return CK_OK;
}

int TTSkyAround(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;

    Effect_ProjMat *efp = (Effect_ProjMat *)beh->GetLocalParameterWriteDataPtr(0);
    if (!efp)
        return CKBR_PARAMETERERROR;

    if (beh->IsInputActive(0))
    {
        efp->effect = 0.3f;
        beh->GetInputParameterValue(PIN_DISTORTION, &efp->effect);
        if (efp->effect > 1.0f)
            efp->effect = 1.0f;
        efp->orientation = (CK3dEntity *)beh->GetInputParameterObject(PIN_ORIENTATION_OBJECT);
        efp->cube = CreateSkyAroundCube(behcontext);
        if (!efp->cube)
            return CKBR_PARAMETERERROR;
        efp->cube->AddPreRenderCallBack(prerender, efp);
        efp->cube->AddPostRenderCallBack(postrender, efp);

        beh->ActivateInput(0, FALSE);
        beh->ActivateOutput(0, TRUE);
    }

    // We get the cube
    CK3dEntity *ent = efp->cube;
    if (!ent)
        return CKBR_PARAMETERERROR;

    if (beh->IsInputActive(1))
    { // we enter by 'OFF'	 ( beware the order between the 'OFF' statement and the 'ON' statement is very important)
        beh->ActivateInput(1, FALSE);
        beh->ActivateOutput(1, TRUE);

        A_SkyShow(ent, CKHIDE);
        return CKBR_OK;
    }
    A_SkyShow(ent, CKSHOW);

    CK3dEntity *cam = behcontext.CurrentRenderContext->GetAttachedCamera();
    if (!cam)
        cam = behcontext.CurrentRenderContext->GetViewpoint(); // We get the Camera
    if (!cam)
        return CKBR_PARAMETERERROR;

    if (efp->orientation)
        ent->SetWorldMatrix(efp->orientation->GetWorldMatrix());
    else
        ent->SetWorldMatrix(VxMatrix::Identity());
    ent->SetPosition3f(0.0f, 0.0f, 0.0f, cam);

    return CKBR_ACTIVATENEXTFRAME;
}

static CK3dEntity *CreateSkyAroundCube(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;
    CKContext *ctx = behcontext.Context;

    int sideCount = 4;
    CKBOOL topMaterial = FALSE;
    CKBOOL bottomMaterial = FALSE;
    beh->GetLocalParameterValue(SETTING_SIDE_MATERIALS, &sideCount);
    beh->GetLocalParameterValue(SETTING_TOP_MATERIAL, &topMaterial);
    beh->GetLocalParameterValue(SETTING_BOTTOM_MATERIAL, &bottomMaterial);
    if (sideCount < 4)
        sideCount = 4;

    float radius = 70.0f;
    beh->GetInputParameterValue(PIN_RADIUS, &radius);

    CKBOOL quadraticSideFaces = TRUE;
    beh->GetInputParameterValue(PIN_QUADRATIC_SIDEFACES, &quadraticSideFaces);

    float sideFaceHeight = 10.0f;
    if (!quadraticSideFaces)
        beh->GetInputParameterValue(PIN_SIDEFACE_HEIGHT, &sideFaceHeight);

    float skyY = 0.0f;
    beh->GetInputParameterValue(PIN_SKY_Y, &skyY);

    if (quadraticSideFaces)
    {
        float angle = (2 * PI) / (float)sideCount;
        float dx = radius - cosf(angle) * radius;
        float dz = sinf(angle) * radius;
        sideFaceHeight = sqrtf(dx * dx + dz * dz);
    }

    float topY = skyY + sideFaceHeight * 0.5f;
    float bottomY = skyY - sideFaceHeight * 0.5f;
    int verticesPerSegment = 4 + (topMaterial ? 3 : 0) + (bottomMaterial ? 3 : 0);
    int facesPerSegment = 2 + (topMaterial ? 1 : 0) + (bottomMaterial ? 1 : 0);

    VxColor filteringColor(1.0f, 1.0f, 1.0f, 0.5f);
    VxColor additionalColor(0.0f, 0.0f, 0.0f, 0.0f);
    beh->GetInputParameterValue(PIN_FILTERING_COLOR, &filteringColor);
    beh->GetInputParameterValue(PIN_ADDITIONAL_COLOR, &additionalColor);
    CKDWORD vertexColor = RGBAFTOCOLOR(filteringColor.r, filteringColor.g, filteringColor.b, filteringColor.a);
    CKDWORD specularColor = RGBAFTOCOLOR(additionalColor.r, additionalColor.g, additionalColor.b, additionalColor.a);

    CKMesh *mesh = (CKMesh *)ctx->CreateObject(CKCID_MESH, "TT_SkyAround_Mesh", beh->IsDynamic() ? CK_OBJECTCREATION_DYNAMIC : CK_OBJECTCREATION_NONAMECHECK);
    if (!mesh)
        return NULL;

    mesh->ModifyObjectFlags(CK_OBJECT_NOTTOBELISTEDANDSAVED, 0);
    mesh->SetVertexCount(sideCount * verticesPerSegment);
    mesh->SetLitMode((VXMESH_LITMODE)VX_PRELITMESH);
    mesh->SetFaceCount(sideCount * facesPerSegment);

    VXBLEND_MODE sourceBlend = VXBLEND_ONE;
    VXBLEND_MODE destBlend = VXBLEND_ZERO;
    beh->GetInputParameterValue(PIN_SOURCE_BLEND, &sourceBlend);
    beh->GetInputParameterValue(PIN_DEST_BLEND, &destBlend);

    int materialIndex = PIN_SIDE_MATERIALS;
    int vertexIndex = 0;
    int faceIndex = 0;
    for (int side = 0; side < sideCount; ++side)
    {
        float angle0 = 6.2831855f * (float)side / (float)sideCount;
        float angle1 = 6.2831855f * (float)(side + 1) / (float)sideCount;
        float x0 = cosf(angle0) * radius;
        float z0 = sinf(angle0) * radius;
        float x1 = cosf(angle1) * radius;
        float z1 = sinf(angle1) * radius;

        SetSkyAroundVertexPosition(mesh, vertexIndex + 0, x0, bottomY, z0);
        SetSkyAroundVertexPosition(mesh, vertexIndex + 1, x1, bottomY, z1);
        SetSkyAroundVertexPosition(mesh, vertexIndex + 2, x1, topY, z1);
        SetSkyAroundVertexPosition(mesh, vertexIndex + 3, x0, topY, z0);

        mesh->SetVertexTextureCoordinates(vertexIndex + 0, 1.0f, 1.0f);
        mesh->SetVertexTextureCoordinates(vertexIndex + 1, 0.0f, 1.0f);
        mesh->SetVertexTextureCoordinates(vertexIndex + 2, 0.0f, 0.0f);
        mesh->SetVertexTextureCoordinates(vertexIndex + 3, 1.0f, 0.0f);

        mesh->SetFaceVertexIndex(faceIndex + 0, vertexIndex + 2, vertexIndex + 0, vertexIndex + 1);
        mesh->SetFaceVertexIndex(faceIndex + 1, vertexIndex + 2, vertexIndex + 3, vertexIndex + 0);

        CKMaterial *sideMat = (CKMaterial *)beh->GetInputParameterObject(materialIndex++);
        if (sideMat)
        {
            sideMat->SetSourceBlend(sourceBlend);
            sideMat->SetDestBlend(destBlend);
            sideMat->EnableAlphaBlend(TRUE);
            sideMat->SetTextureAddressMode(VXTEXTURE_ADDRESSCLAMP);
        }
        mesh->SetFaceMaterial(faceIndex + 0, sideMat);
        mesh->SetFaceMaterial(faceIndex + 1, sideMat);

        vertexIndex += 4;
        faceIndex += 2;

        if (topMaterial)
        {
            SetSkyAroundVertexPosition(mesh, vertexIndex + 0, x0, topY, z0);
            SetSkyAroundVertexPosition(mesh, vertexIndex + 1, x1, topY, z1);
            SetSkyAroundVertexPosition(mesh, vertexIndex + 2, 0.0f, topY, 0.0f);

            SetSkyAroundPlanarUv(mesh, vertexIndex + 0, x0, z0);
            SetSkyAroundPlanarUv(mesh, vertexIndex + 1, x1, z1);
            mesh->SetVertexTextureCoordinates(vertexIndex + 2, 0.5f, 0.5f);

            mesh->SetFaceVertexIndex(faceIndex, vertexIndex + 0, vertexIndex + 1, vertexIndex + 2);

            CKMaterial *topMat = (CKMaterial *)beh->GetInputParameterObject(PIN_SIDE_MATERIALS + sideCount);
            if (topMat)
            {
                topMat->SetSourceBlend(sourceBlend);
                topMat->SetDestBlend(destBlend);
                topMat->EnableAlphaBlend(TRUE);
                topMat->SetTextureAddressMode(VXTEXTURE_ADDRESSCLAMP);
            }
            mesh->SetFaceMaterial(faceIndex, topMat);

            vertexIndex += 3;
            faceIndex += 1;
        }

        if (bottomMaterial)
        {
            SetSkyAroundVertexPosition(mesh, vertexIndex + 0, x0, bottomY, z0);
            SetSkyAroundVertexPosition(mesh, vertexIndex + 1, 0.0f, bottomY, 0.0f);
            SetSkyAroundVertexPosition(mesh, vertexIndex + 2, x1, bottomY, z1);

            SetSkyAroundPlanarUv(mesh, vertexIndex + 0, x0, z0);
            mesh->SetVertexTextureCoordinates(vertexIndex + 1, 0.5f, 0.5f);
            SetSkyAroundPlanarUv(mesh, vertexIndex + 2, x1, z1);

            mesh->SetFaceVertexIndex(faceIndex, vertexIndex + 0, vertexIndex + 1, vertexIndex + 2);

            int bottomIndex = PIN_SIDE_MATERIALS + sideCount + (topMaterial ? 1 : 0);
            CKMaterial *bottomMat = (CKMaterial *)beh->GetInputParameterObject(bottomIndex);
            if (bottomMat)
            {
                bottomMat->SetSourceBlend(sourceBlend);
                bottomMat->SetDestBlend(destBlend);
                bottomMat->EnableAlphaBlend(TRUE);
                bottomMat->SetTextureAddressMode(VXTEXTURE_ADDRESSCLAMP);
            }
            mesh->SetFaceMaterial(faceIndex, bottomMat);

            vertexIndex += 3;
            faceIndex += 1;
        }
    }

    for (int i = 0; i < mesh->GetVertexCount(); ++i)
    {
        mesh->SetVertexColor(i, vertexColor);
        mesh->SetVertexSpecularColor(i, specularColor);
    }

    mesh->ColorChanged();
    mesh->BuildFaceNormals();

    CK3dEntity *ent = (CK3dEntity *)ctx->CreateObject(CKCID_3DENTITY, "TT_SkyAround_Entity", beh->IsDynamic() ? CK_OBJECTCREATION_DYNAMIC : CK_OBJECTCREATION_NONAMECHECK);
    if (!ent)
        return NULL;

    ent->ModifyObjectFlags(CK_OBJECT_NOTTOBELISTEDANDSAVED, 0);
    ent->SetMoveableFlags(ent->GetMoveableFlags() | VX_MOVEABLE_RENDERFIRST);
    behcontext.CurrentLevel->AddObject(ent);

    A_SkyShow(ent, CKHIDE);

    ent->AddMesh(mesh);
    ent->SetCurrentMesh(mesh);
    ent->SetPickable(FALSE);

    CKDWORD flags = ent->GetMoveableFlags();
    flags |= VX_MOVEABLE_NOZBUFFERTEST;
    flags |= VX_MOVEABLE_NOZBUFFERWRITE;
    ent->SetMoveableFlags(flags);

    return ent;
}

CKERROR SkyAroundCallBack(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;

    switch (behcontext.CallbackMessage)
    {
    case CKM_BEHAVIORATTACH:
    case CKM_BEHAVIORLOAD:
    {
        Effect_ProjMat efp;
        ResetSkyAroundData(&efp);
        beh->SetLocalParameterValue(LOCAL_SKYAROUND, &efp, sizeof(Effect_ProjMat));
    }
    break;

    case CKM_BEHAVIORDETACH:
    case CKM_BEHAVIORDELETE:
    case CKM_BEHAVIORRESET:
    {
        Effect_ProjMat *efp = (Effect_ProjMat *)beh->GetLocalParameterWriteDataPtr(0);
        if (efp && efp->cube)
        {
            CKDestroyObject(efp->cube);
            efp->cube = NULL;
            efp->orientation = NULL;
        }
    }
    break;

    case CKM_BEHAVIORSETTINGSEDITED:
    {
        CKBOOL top = FALSE;
        CKBOOL bottom = FALSE;
        int sideCount = 4;

        beh->GetLocalParameterValue(SETTING_TOP_MATERIAL, &top);
        beh->GetLocalParameterValue(SETTING_BOTTOM_MATERIAL, &bottom);
        beh->GetLocalParameterValue(SETTING_SIDE_MATERIALS, &sideCount);
        if (sideCount < 4)
            sideCount = 4;
        beh->SetLocalParameterValue(SETTING_SIDE_MATERIALS, &sideCount);

        for (int i = beh->GetInputParameterCount(); i > PIN_SIDE_MATERIALS; --i)
        {
            CKParameterIn *pin = beh->RemoveInputParameter(i - 1);
            CKDestroyObject(pin);
        }

        char name[64];
        for (int i = 1; i <= sideCount; ++i)
        {
            sprintf(name, "%d.Side-Mat", i);
            beh->CreateInputParameter(name, CKPGUID_MATERIAL);
        }

        if (top)
            beh->CreateInputParameter("Top Mat", CKPGUID_MATERIAL);
        if (bottom)
            beh->CreateInputParameter("Bottom Mat", CKPGUID_MATERIAL);
    }
    break;

    }

    return CKBR_OK;
}

/***********************************************************/
/****************** PRE-RENDER CALLBACK ********************/
static int prerender(CKRenderContext *dev, CKRenderObject *obj, void *arg)
{

    CK3dEntity *mov = (CK3dEntity *)obj;
    Effect_ProjMat *efp = (Effect_ProjMat *)arg;
    if (!efp)
        return CKBR_PARAMETERERROR;

    VxMatrix mat;

    // we change the fov to make it fun !!!
    float near_plane = 1, far_plane = 200;

    VxRect rect;
    dev->GetViewRect(rect);
    float ViewWidth = rect.GetWidth();
    float ViewHeight = rect.GetHeight();

    efp->projmat = dev->GetProjectionTransformationMatrix();

    float current_fov = (float)fabs(2.0f * (float)atanf(1.0f / efp->projmat[0][0]));
    float fov = (float)(current_fov + efp->effect * (3.141592654 - current_fov));
    memset(mat, 0, sizeof(VxMatrix));
    float denom = 1.0f / (far_plane - near_plane);
    mat[0][0] = cosf(fov * 0.5f) / sinf(fov * 0.5f);
    mat[1][1] = mat[0][0] * (float)ViewWidth / (float)ViewHeight;
    mat[2][2] = far_plane * denom;
    mat[3][2] = -mat[2][2] * near_plane;
    mat[2][3] = 1;

    dev->SetProjectionTransformationMatrix(mat);

    // we set the position of the cube to the camera
    CK3dEntity *cam = dev->GetAttachedCamera();
    if (!cam)
        cam = dev->GetViewpoint(); // We get the Camera
    if (cam)
        mov->SetPosition(&VxVector::axis0(), cam);

    return 0;
}

/***********************************************************/
/****************** POST-RENDER CALLBACK *******************/
static int postrender(CKRenderContext *dev, CKRenderObject *obj, void *arg)
{

    CK3dEntity *mov = (CK3dEntity *)obj;

    Effect_ProjMat *efp = (Effect_ProjMat *)arg;

    dev->SetProjectionTransformationMatrix(efp->projmat);

    return 0;
}
