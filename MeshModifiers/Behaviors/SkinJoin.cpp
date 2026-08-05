/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
//
//		            SkinJoin
//
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
#include "CKAll.h"

//__________________________________
//       CORRESPONDANCE
//
typedef struct
{
    int ie; // index of 3dentity's (referenced in the obj. array) to which the join vertice will match
    int iv; // index of vertice to which the join vertice will match
} CORRESPONDANCE;

//__________________________________
//       TEMPORARY INFO
//
typedef struct
{
    VxMatrix mat;
    CKMesh *mesh;
} TMP_INFO;

CKObjectDeclaration *FillBehaviorSkinJoinDecl();
CKERROR CreateSkinJoinProto(CKBehaviorPrototype **);
int SkinJoin(const CKBehaviorContext &behcontext);
CKERROR SkinJoinCallBackObject(const CKBehaviorContext &behcontext);

int prerenderjoin(CKRenderContext *dev, CKRenderObject *mov, void *arg);

CKObjectDeclaration *FillBehaviorSkinJoinDecl()
{
    CKObjectDeclaration *od = CreateCKObjectDeclaration("Skin Join");
    od->SetDescription("Deforms a join so it sticks on the objects it's joined.");
    /* rem:
    <SPAN CLASS=in>On : </SPAN>activates the behavior process.<BR>
    <SPAN CLASS=in>Off : </SPAN>deactivates the behavior process.<BR>
    <BR>
    <SPAN CLASS=pin>Threshold : </SPAN>.<BR>
    <SPAN CLASS=pin>Object 1 : </SPAN>first object to which the join will be glued.<BR>
    <SPAN CLASS=pin>Object 2 : </SPAN>second object to which the join will be glued.<BR>
    ...<BR>
    <BR>
    This building block is an easy way to simulate skins.<BR>
    You first need to modelize the join that will take place between your object (eg: the neck between a head an the body).<BR>
    Then load all the objects in Virtools, and Drag'n Drop the 'Skin Join' building block on the join object (eg: ... the neck).<BR>
    Now, you need to specify a 'Threshold' ... this parameter represents the minimal 3d distance from which 2 vertices will be considered as being the same.<BR>
    If you give a too small 'Threshold' you will not have all the vertice-joints you would expect.<BR>
    And if you give a too great 'Threshold', the inside vertice of your 'join object' will be considered has vertice-joints.<BR>
    You can specify several object to be linked to the join ( just add Input Parameters ).<BR>
    */
    /*
    warning:
    - Do not use several SkinJoins on the same object, but add input parameters.<BR>
    - You'd better put an Initial Condition to the mesh of the join, so if the 'Skin Join' building block deforms your object, you'll always be able to comeback to the initial topology of your object.<BR>
    - If the 'Threshold' you gave doesn't correspond to what you want, you'll have to change it by editing the 'Skin Join' building block.<BR>
    - The vertices correspondence array is evaluated each time you edit the building block.<BR>
    */
    od->SetType(CKDLL_BEHAVIORPROTOTYPE);
    od->SetGuid(CKGUID(0x38677c72, 0x4f4715cb));
    od->SetAuthorGuid(VIRTOOLS_GUID);
    od->SetAuthorName("Virtools");
    od->SetVersion(0x00010000);
    od->SetCreationFunction(CreateSkinJoinProto);
    od->SetCompatibleClassId(CKCID_3DOBJECT);
    od->SetCategory("Mesh Modifications/Deformation");
    return od;
}

CKERROR CreateSkinJoinProto(CKBehaviorPrototype **pproto)
{
    CKBehaviorPrototype *proto = NULL;
    proto = CreateCKBehaviorPrototype("Skin Join");
    if (!proto)
        return CKERR_OUTOFMEMORY;

    proto->DeclareInput("On");
    proto->DeclareInput("Off");

    proto->DeclareInParameter("Threshold", CKPGUID_FLOAT, "0.2");
    proto->DeclareInParameter("Object 1", CKPGUID_OBJECT3D);
    proto->DeclareInParameter("Object 2", CKPGUID_OBJECT3D);

    proto->DeclareLocalParameter("Joined Objects", CKPGUID_OBJECTARRAY);
    proto->DeclareLocalParameter("Correspondance", CKPGUID_VOIDBUF);
    proto->DeclareLocalParameter("Is Joining", CKPGUID_BOOL);
    proto->DeclareLocalParameter("Temporary Joined Object Info", CKPGUID_VOIDBUF);

    proto->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);
    proto->SetFunction(SkinJoin);

    proto->SetBehaviorFlags((CK_BEHAVIOR_FLAGS)(CKBEHAVIOR_VARIABLEPARAMETERINPUTS));
    proto->SetBehaviorCallbackFct(SkinJoinCallBackObject);

    *pproto = proto;
    return CK_OK;
}

int SkinJoin(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;
    CK3dObject *ent = (CK3dObject *)beh->GetOwner();

    if (beh->IsInputActive(0)) //------- ON
    {
        beh->ActivateInput(0, FALSE);
        CKBOOL is_joining = TRUE;
        beh->SetLocalParameterValue(2, &is_joining);

        ent->AddPreRenderCallBack(prerenderjoin, beh);
    }

    if (beh->IsInputActive(1)) //------- OFF
    {
        beh->ActivateInput(1, FALSE);

        ent->RemovePreRenderCallBack(prerenderjoin, beh);
        CKBOOL is_joining = FALSE;
        beh->SetLocalParameterValue(2, &is_joining);
        return CKBR_OK;
    }

    //--- ensusre join rendering if one of the joined_obj is inside the viewing frustum
    if (ent->IsAllOutsideFrustrum())
    {
        XObjectArray *obj_array = *((XObjectArray **)beh->GetLocalParameterReadDataPtr(0));
        if (!obj_array)
            return CKBR_ACTIVATENEXTFRAME;

        int count = obj_array->Size();

        CK3dEntity *joined_obj;

        for (int a = 0; a < count; ++a)
        {
            joined_obj = (CK3dEntity *)behcontext.Context->GetObject((*obj_array)[a]);
            if (joined_obj && !joined_obj->IsAllOutsideFrustrum())
            {
                const VxBbox &bbox = joined_obj->GetBoundingBox();
                ent->SetBoundingBox(&bbox);
                break;
            }
        }
    }

    return CKBR_ACTIVATENEXTFRAME;
}

/*******************************************************/
/*                     CALLBACK                        */
/*******************************************************/
CKERROR SkinJoinCallBackObject(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;
    switch (behcontext.CallbackMessage)
    {

    case CKM_BEHAVIORATTACH:
    {
        CKBOOL is_joining = FALSE;
        beh->SetLocalParameterValue(2, &is_joining);
    }
    break;

    case CKM_BEHAVIOREDITED:
    {
        int a, b;

        //------------- check type of ParamIn
        CKParameterIn *pin;
        int c_pin = beh->GetInputParameterCount();
        char str[64];
        for (a = 1; a < c_pin; a++)
        {
            pin = beh->GetInputParameter(a);
            if ((pin->GetGUID() != CKPGUID_OBJECT3D) && (pin->GetGUID() != CKPGUID_3DENTITY))
            {
                snprintf(str, sizeof(str), "Object %d", a);
                pin->SetGUID(CKPGUID_OBJECT3D, TRUE, str);
            }
        }

        //------------- fill joined_obj
        XObjectArray *obj_array = *((XObjectArray **)beh->GetLocalParameterReadDataPtr(0));
        if (!obj_array)
            return 0;
        obj_array->Clear();

        CK3dObject *obj;
        CKMesh *mesh_obj;
        int total_vertex_count = 0;

        int obj_count = beh->GetInputParameterCount();

        for (a = 1; a < obj_count; a++)
        {
            if ((obj = (CK3dObject *)beh->GetInputParameterObject(a)))
            {
                obj_array->PushBack(obj->GetID());
                if ((mesh_obj = obj->GetCurrentMesh()))
                {
                    total_vertex_count += mesh_obj->GetVertexCount();
                }
            }
        }
        obj_count = obj_array->Size();

        //-------------  fill temporary fill array
        VxVector *tmp_pos = new VxVector[total_vertex_count];
        int *tmp_vertex_index = new int[total_vertex_count];
        int *tmp_obj_index = new int[total_vertex_count];

        CKDWORD Stride;
        VxVector *pos;
        int vertex_index, obj_index;

        b = 0;
        CK_ID *ids;
        for (obj_index = 0, ids = obj_array->Begin(); ids != obj_array->End(); ++ids, obj_index++)
        {
            obj = (CK3dObject *)behcontext.Context->GetObject(*ids);
            if ((mesh_obj = obj->GetCurrentMesh()))
            {

                const VxMatrix &tmp_matrix = obj->GetWorldMatrix();
                pos = (VxVector *)mesh_obj->GetPositionsPtr(&Stride);

                for (vertex_index = 0; vertex_index < mesh_obj->GetVertexCount(); vertex_index++)
                {
                    Vx3DMultiplyMatrixVector(&tmp_pos[b], tmp_matrix, pos);
                    tmp_vertex_index[b] = vertex_index;
                    tmp_obj_index[b] = obj_index;
                    pos = (VxVector *)((CKBYTE *)pos + Stride);
                    b++;
                }
            }
        }

        //------------- fill correspondance array
        CK3dObject *ent;
        if ((ent = (CK3dObject *)beh->GetOwner()))
        {
            CKMesh *mesh;
            if ((mesh = ent->GetCurrentMesh()))
            {

                int count = mesh->GetVertexCount();

                CORRESPONDANCE *corresp = new CORRESPONDANCE[count];

                float threshold; // get threshold
                beh->GetInputParameterValue(0, &threshold);
                threshold = threshold * threshold; // not using 'sqrt'

                VxVector vpos, vdif;
                float dist_mini, dist;
                int index_mini, ie;

                pos = (VxVector *)mesh->GetPositionsPtr(&Stride);

                const VxMatrix &tmp_matrix = ent->GetWorldMatrix();

                for (a = 0; a < count; a++)
                {
                    ie = -1;
                    index_mini = -1;
                    dist_mini = 66666666.0f;

                    Vx3DMultiplyMatrixVector(&vpos, tmp_matrix, pos);

                    for (b = 0; b < total_vertex_count; b++)
                    {
                        vdif = vpos - tmp_pos[b];
                        vdif.x *= vdif.x;
                        vdif.y *= vdif.y;
                        vdif.z *= vdif.z;

                        dist = vdif.x + vdif.y + vdif.z;
                        if ((dist < threshold) && (dist < dist_mini))
                        {
                            dist_mini = dist;
                            index_mini = tmp_vertex_index[b];
                            ie = tmp_obj_index[b];
                        }
                    }

                    corresp[a].ie = ie;
                    corresp[a].iv = index_mini;

                    pos = (VxVector *)((CKBYTE *)pos + Stride);
                }

                //-- copy corresp in Local Correspondance array
                beh->SetLocalParameterValue(1, corresp, count * sizeof(CORRESPONDANCE));
                delete[] corresp;
            }
        }

        //------------- allocate memory space for tmp_info
        TMP_INFO *tmp_info_vide = new TMP_INFO[obj_count];
        beh->SetLocalParameterValue(3, tmp_info_vide, obj_count * sizeof(TMP_INFO));

        //------------- free allocated memory
        delete[] tmp_info_vide;
        delete[] tmp_pos;
        delete[] tmp_vertex_index;
        delete[] tmp_obj_index;
    }
    break;

    case CKM_BEHAVIORRESET:
    {
        CK3dObject *ent = (CK3dObject *)beh->GetOwner();
        if (!ent)
            return 0;

        CKBOOL is_joining;
        beh->GetLocalParameterValue(2, &is_joining);

        if (is_joining) // remove join render callback if it was joining
        {
            is_joining = FALSE;
            beh->SetLocalParameterValue(2, &is_joining);
            ent->RemovePreRenderCallBack(prerenderjoin, beh);
        }
    }
    break;

    case CKM_BEHAVIORDEACTIVATESCRIPT:
    case CKM_BEHAVIORPAUSE:
    {
        CK3dObject *ent = (CK3dObject *)beh->GetOwner();
        if (!ent)
            return 0;

        CKBOOL is_joining;
        beh->GetLocalParameterValue(2, &is_joining);

        if (is_joining) // remove join render callback if it was joining
        {
            ent->RemovePreRenderCallBack(prerenderjoin, beh);
        }
    }
    break;

    case CKM_BEHAVIORACTIVATESCRIPT:
    case CKM_BEHAVIORRESUME:
    {
        CK3dObject *ent = (CK3dObject *)beh->GetOwner();
        if (!ent)
            return 0;

        CKBOOL is_joining;
        beh->GetLocalParameterValue(2, &is_joining);

        if (is_joining) // add join render callback if it was joining
        {
            ent->AddPreRenderCallBack(prerenderjoin, beh);
        }
    }
    break;

    case CKM_BEHAVIORDETACH:
    {
        CK3dObject *ent = (CK3dObject *)beh->GetOwner();
        if (!ent)
            return 0;

        CKBOOL is_joining;
        beh->GetLocalParameterValue(2, &is_joining);

        if (is_joining) // remove join render callback if it was joining
        {
            is_joining = FALSE;
            beh->SetLocalParameterValue(2, &is_joining);
            ent->RemovePreRenderCallBack(prerenderjoin, beh);
        }
    }
    break;

    case CKM_BEHAVIORNEWSCENE:
    {
        CK3dObject *ent = (CK3dObject *)beh->GetOwner();
        if (!ent)
            return 0;

        CKBOOL is_joining;
        beh->GetLocalParameterValue(2, &is_joining);

        if (!(beh->IsParentScriptActiveInScene(behcontext.CurrentScene)))
        {

            if (is_joining)
            {
                CK3dObject *ent = (CK3dObject *)beh->GetOwner();
                if (!ent)
                    return 0;
                ent->RemovePreRenderCallBack(prerenderjoin, beh);
            }
        }
        else
        {
            if (is_joining) // add join render callback if it was joining
            {
                ent->AddPreRenderCallBack(prerenderjoin, beh);
            }
        }
    }
    break;
    }

    return CKBR_OK;
}

/*******************************************************/
/****************** RENDER FUNCTION ********************/
int prerenderjoin(CKRenderContext *dev, CKRenderObject *rent, void *arg)
{
    CKBehavior *beh = (CKBehavior *)arg;
    CK3dEntity *ent = (CK3dEntity *)rent;

    CKMesh *mesh = ent->GetCurrentMesh();

    //------------- scan joined_obj and get tmp_info (there transfo.matrix and mesh)
    const VxMatrix &mat_p = ent->GetInverseWorldMatrix();

    XObjectArray *obj_array = *((XObjectArray **)beh->GetLocalParameterReadDataPtr(0));
    if (!obj_array)
        return 0;

    TMP_INFO *tmp_info = (TMP_INFO *)beh->GetLocalParameterWriteDataPtr(3);
    CK3dObject *obj;

    int c;
    CK_ID *ids;
    for (c = 0, ids = obj_array->Begin(); ids != obj_array->End(); ids++, c++)
    {
        obj = (CK3dObject *)beh->GetCKContext()->GetObject(*ids);
        if (obj)
        {
            tmp_info[c].mat = obj->GetWorldMatrix();
            tmp_info[c].mesh = obj->GetCurrentMesh();
        }
        else
        {
            tmp_info[c].mesh = (CKMesh *)NULL;
        }

        Vx3DMultiplyMatrix(tmp_info[c].mat, mat_p, tmp_info[c].mat);
    }

    //------------- get the correspondance array
    CORRESPONDANCE *corresp = (CORRESPONDANCE *)beh->GetLocalParameterReadDataPtr(1);

    //------------- position join's vertice
    int count = mesh->GetVertexCount();

    CKDWORD posStride = 0;
    VxVector *pos = (VxVector *)mesh->GetPositionsPtr(&posStride);
    CKDWORD normalStride = 0;
    VxVector *normal = (VxVector *)mesh->GetNormalsPtr(&normalStride);
    if (!corresp || !pos || !normal)
        return 0;

    CKMesh *obj_mesh;
    VxVector V, V_tmp;
    int ie, iv;

    for (int i = 0; i < count; i++)
    {
        ie = corresp[i].ie;
        iv = corresp[i].iv;

        ENDIANSWAP32(ie);
        ENDIANSWAP32(iv);

        if (ie > -1)
        {
            obj_mesh = tmp_info[ie].mesh;
            if (obj_mesh)
            {
                obj_mesh->GetVertexPosition(iv, &V);
                obj_mesh->GetVertexNormal(iv, &V_tmp);
                V_tmp += V;
                Vx3DMultiplyMatrixVector(pos, tmp_info[ie].mat, &V);
                Vx3DMultiplyMatrixVector(normal, tmp_info[ie].mat, &V_tmp);
                *normal -= *pos;
                normal->Normalize();
            }
        }
        pos = (VxVector *)((CKBYTE *)pos + posStride);
        normal = (VxVector *)((CKBYTE *)normal + normalStride);
    }

    //------------- building normals
    mesh->BuildFaceNormals();

    //------------- update bounding box
    mesh->VertexMove();
    ent->SetBoundingBox(NULL);

    return 1;
}
