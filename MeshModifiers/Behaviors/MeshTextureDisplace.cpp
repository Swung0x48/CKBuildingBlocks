/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
//
//		            MeshTextureDisplace
//
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
#include "CKAll.h"

#define CKPGUID_COLORCHANNELTYPE CKDEFINEGUID(0x39d40df7, 0x48433312)

CKERROR CreateTextureDisplaceBehaviorProto(CKBehaviorPrototype **);
int TextureDisplace(const CKBehaviorContext &behcontext);

CKObjectDeclaration *FillBehaviorTextureDisplaceDecl()
{
    CKObjectDeclaration *od = CreateCKObjectDeclaration("Texture Displace");
    od->SetDescription("Displaces the vertices of the mesh, according to the luminosity of the texture mapped to this vertex.");
    /* rem:
    <SPAN CLASS=in>In: </SPAN>triggers the process.<BR>
    <SPAN CLASS=out>Out: </SPAN>is activated when the process is completed.<BR>
    <BR>
    <SPAN CLASS=pin>Texture: </SPAN>texture to use for displacement displace.<BR>
    <SPAN CLASS=pin>Red Scale: </SPAN>Amount of displacement for red channel.<BR>
    <SPAN CLASS=pin>Green Scale: </SPAN>Amount of displacement for green channel.<BR>
    <SPAN CLASS=pin>Blue Scale: </SPAN>Amount of displacement for blue channel.<BR>
    <SPAN CLASS=pin>Alpha Scale: </SPAN>Amount of the displacement for alpha channel.<BR>
    <SPAN CLASS=pin>Min Texture Source Rectangle: </SPAN>specifies the region of the texture to be used (top-left point of a 2D rectangle).<BR>
    <SPAN CLASS=pin>Max Texture Source Rectangle: </SPAN>specifies the region of the texture to be used (bottom-right point of a 2D rectangle).<BR>
    <BR>
    Useful to create landscapes on the fly: apply this behavior to a plane object. The plane will be locally deformed according to the selected channel (R, G, B or A) values on the texture.
    */
    od->SetCategory("Mesh Modifications/Local Deformation");
    od->SetType(CKDLL_BEHAVIORPROTOTYPE);
    od->SetGuid(CKGUID(0x32145678, 0x32145678));
    od->SetAuthorGuid(CKGUID());
    od->SetAuthorName("Virtools");
    od->SetVersion(0x00030000);
    od->SetCreationFunction(CreateTextureDisplaceBehaviorProto);
    od->SetCompatibleClassId(CKCID_3DOBJECT);
    return od;
}

CKERROR CreateTextureDisplaceBehaviorProto(CKBehaviorPrototype **pproto)
{
    CKBehaviorPrototype *proto = CreateCKBehaviorPrototype("Texture Displace");
    if (!proto)
        return CKERR_OUTOFMEMORY;

    proto->DeclareInput("In");
    proto->DeclareOutput("Out");

    proto->DeclareInParameter("Texture", CKPGUID_TEXTURE);
    proto->DeclareInParameter("Red Scale", CKPGUID_PERCENTAGE, "20");
    proto->DeclareInParameter("Green Scale", CKPGUID_PERCENTAGE, "20");
    proto->DeclareInParameter("Blue Scale", CKPGUID_PERCENTAGE, "20");
    proto->DeclareInParameter("Alpha Scale", CKPGUID_PERCENTAGE, "20");
    proto->DeclareInParameter("Min Texture Source Rectangle", CKPGUID_2DVECTOR, "0,0");
    proto->DeclareInParameter("Max Texture Source Rectangle", CKPGUID_2DVECTOR, "-1,-1");
    proto->DeclareInParameter("Reset Mesh", CKPGUID_BOOL, "FALSE");
    proto->DeclareInParameter("Normal Recalculation", CKPGUID_BOOL, "TRUE");

    proto->DeclareSetting("3D Displacement", CKPGUID_BOOL, "TRUE");

    proto->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);
    proto->SetBehaviorFlags(CKBEHAVIOR_TARGETABLE);

    proto->SetFunction(TextureDisplace);

    *pproto = proto;
    return CK_OK;
}

int TextureDisplace(const CKBehaviorContext &behcontext)
{
    float RedEffect = 0, BlueEffect = 0, GreenEffect = 0, AlphaEffect = 0;
    CKBOOL ResetMesh = FALSE, NormalRecalculation = TRUE;
    CKBOOL Displacement3D = FALSE;

    CKBehavior *beh = behcontext.Behavior;

    CK3dEntity *ent = (CK3dEntity *)beh->GetTarget();
    if (!ent)
        return CKBR_OWNERERROR;

    CKMesh *mesh = ent->GetCurrentMesh();
    if (!mesh)
        return CKBR_OWNERERROR;
    /*	if (CKIsChildClassOf(mesh,CKCID_PATCHMESH)){
            CKPatchMesh* patchmesh =(CKPatchMesh*) mesh;
            CKPatch* PatchDesc;
            patchmesh->GetPatch(0, PatchDesc);
        }
    */

    // we get a CKTexture (input parameter)
    CKTexture *tex = (CKTexture *)beh->GetInputParameterObject(0);
    if (!tex)
        return CKBR_PARAMETERERROR;

    int width = tex->GetWidth();
    int height = tex->GetHeight();

    VxImageDescEx desc;
    tex->GetImageDesc(desc);

    int bpp = desc.BitsPerPixel;
    if (bpp != 32)
        return CKBR_OK; // Texture system data need to be 32 bit per pixel
    CKDWORD *tex_data = (CKDWORD *)tex->LockSurfacePtr();
    int nbv = mesh->GetVertexCount();
    CKDWORD TStride, VStride, NStride;
    CKBYTE *TCoords = (CKBYTE *)mesh->GetTextureCoordinatesPtr(&TStride);
    CKBYTE *VCoords = (CKBYTE *)mesh->GetPositionsPtr(&VStride);
    CKBYTE *NCoords = (CKBYTE *)mesh->GetNormalsPtr(&NStride);

    if (!(tex_data && TCoords && VCoords))
    {
        if (tex_data)
            tex->ReleaseSurfacePtr();
        return CKBR_OK;
    }

    Vx2DVector tmin(0.0f, 0.0f);
    Vx2DVector tmax(-1.0f, -1.0f);

    switch (beh->GetVersion())
    {
    case (0x00010000):
    {
        // we get a float (input parameter)
        float effect;
        beh->GetInputParameterValue(1, &effect);

        int channel = 0;
        beh->GetInputParameterValue(2, &channel);

        switch (channel)
        {
        case 0:
            RedEffect = effect;
            break;
        case 1:
            GreenEffect = effect;
            break;
        case 2:
            BlueEffect = effect;
            break;
        }
        beh->GetInputParameterValue(3, &tmin);
        beh->GetInputParameterValue(4, &tmax);
        break;
    }
    case (0x00020000):
    {
        beh->GetInputParameterValue(1, &RedEffect);
        beh->GetInputParameterValue(2, &GreenEffect);
        beh->GetInputParameterValue(3, &BlueEffect);
        beh->GetInputParameterValue(4, &AlphaEffect);
        beh->GetInputParameterValue(5, &tmin);
        beh->GetInputParameterValue(6, &tmax);
        break;
    }
    case (0x00030000):
    {
        beh->GetInputParameterValue(1, &RedEffect);
        beh->GetInputParameterValue(2, &GreenEffect);
        beh->GetInputParameterValue(3, &BlueEffect);
        beh->GetInputParameterValue(4, &AlphaEffect);
        beh->GetInputParameterValue(5, &tmin);
        beh->GetInputParameterValue(6, &tmax);
        beh->GetInputParameterValue(7, &ResetMesh);
        beh->GetInputParameterValue(8, &NormalRecalculation);

        beh->GetLocalParameterValue(0, &Displacement3D);
        break;
    }
    }

    if (ResetMesh)
    { // The Mesh must be resetted
        if (!CKIsChildClassOf(mesh, CKCID_PATCHMESH))
        {
            CKScene *scn = behcontext.CurrentScene;
            CKStateChunk *chunk = scn->GetObjectInitialValue(mesh);

            if (chunk)
            {
                mesh->LoadVertices(chunk);
                if (Displacement3D)
                {
                    mesh->BuildNormals();
                }
            }
        }
    }

    // Texture Region
    while (tmin.x < 0.0f)
        tmin.x += width;
    while (tmin.x > width)
        tmin.x -= width;
    while (tmin.y < 0.0f)
        tmin.y += height;
    while (tmin.y > height)
        tmin.y -= height;
    while (tmax.x < 0.0f)
        tmax.x += width;
    while (tmax.x > width)
        tmax.x -= width;
    while (tmax.y < 0.0f)
        tmax.y += height;
    while (tmax.y > height)
        tmax.y -= height;

    float startx = tmin.x;
    float lenx = tmax.x - tmin.x;
    if (tmin.x > tmax.x)
    {
        startx = 0.0f;
        lenx = tmin.x - tmax.x;
    }

    float starty = tmin.y;
    float leny = tmax.y - tmin.y;
    if (tmin.y > tmax.y)
    {
        starty = 0.0f;
        leny = tmin.y - tmax.y;
    }

    if (Displacement3D)
    {
        if (!NCoords)
        {
            tex->ReleaseSurfacePtr();
            return CKBR_OK;
        }

        for (int i = 0; i < nbv; ++i, VCoords += VStride, TCoords += TStride, NCoords += NStride)
        {
            float u = ((VxUV *)TCoords)->u;
            float v = ((VxUV *)TCoords)->v;
            u -= (int)u;
            if (u < 0.0f)
                u += 1.0f;
            v -= (int)v;
            if (v < 0.0f)
                v += 1.0f;

            CKDWORD col = (int)tex_data[(int)(startx + u * lenx) + width * (int)(starty + v * leny)];
            float R = (float)((col & 0x00FF0000) >> 16);
            float G = (float)((col & 0x0000FF00) >> 8);
            float B = (float)((col & 0x00FF));
            float A = (float)((col & 0xFF000000) >> 24);

            *(VxVector *)VCoords += (*(VxVector *)NCoords) * (R * RedEffect + G * GreenEffect + B * BlueEffect + A * AlphaEffect);
        }
    }
    else
    {
        for (int i = 0; i < nbv; ++i, VCoords += VStride, TCoords += TStride)
        {
            float u = ((VxUV *)TCoords)->u;
            float v = ((VxUV *)TCoords)->v;
            u -= (int)u;
            if (u < 0.0f)
                u += 1.0f;
            v -= (int)v;
            if (v < 0.0f)
                v += 1.0f;

            CKDWORD col = (int)tex_data[(int)(startx + u * lenx) + width * (int)(starty + v * leny)];
            float R = (float)((col & 0x00FF0000) >> 16);
            float G = (float)((col & 0x0000FF00) >> 8);
            float B = (float)((col & 0x00FF));
            float A = (float)((col & 0xFF000000) >> 24);

            ((VxVector *)VCoords)->y = R * RedEffect + G * GreenEffect + B * BlueEffect + A * AlphaEffect;
        }
    }

    // if not in prelit mode, we calculate the vertice normals
    if (NormalRecalculation)
    {
        if (mesh->GetLitMode() != VX_PRELITMESH)
            mesh->BuildNormals();
        else
            // calculate the face normals
            mesh->BuildFaceNormals();
    }

    // Notify vertex movement
    mesh->VertexMove();
    tex->ReleaseSurfacePtr();

    beh->ActivateInput(0, FALSE);
    beh->ActivateOutput(0);

    return CKBR_OK;
}
