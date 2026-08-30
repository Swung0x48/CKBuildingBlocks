/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
//
//		            MultiMeshMorpher
//
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
#include "CKAll.h"

CKObjectDeclaration *FillBehaviorMultiMeshMorpherDecl();
CKERROR CreateMultiMeshMorpherProto(CKBehaviorPrototype **);
int MultiMeshMorpher(const CKBehaviorContext &behcontext);

CKERROR MultiMeshMorpherCallBack(const CKBehaviorContext &behcontext); // CallBack Functioon

/*****************************************************************/
/*                         DECLARATION                           */
/*****************************************************************/
CKObjectDeclaration *FillBehaviorMultiMeshMorpherDecl()
{
    CKObjectDeclaration *od = CreateCKObjectDeclaration("Multi Mesh Morpher");
    od->SetDescription("Transforms the current mesh so it matches the interpolation of other meshes (ie there vertice)");
    od->SetType(CKDLL_BEHAVIORPROTOTYPE);
    od->SetGuid(CKGUID(0x53da65c4, 0x6f0f194a));
    od->SetAuthorGuid(VIRTOOLS_GUID);
    od->SetAuthorName("Virtools");
    od->SetVersion(0x00010000);
    od->SetCreationFunction(CreateMultiMeshMorpherProto);
    od->SetCompatibleClassId(CKCID_3DOBJECT);
    od->SetCategory("Mesh Modifications/Deformation");
    return od;
}

/*****************************************************************/
/*                         PROTOTYPE                             */
/*****************************************************************/
CKERROR CreateMultiMeshMorpherProto(CKBehaviorPrototype **pproto)
{
    CKBehaviorPrototype *proto = NULL;
    proto = CreateCKBehaviorPrototype("Multi Mesh Morpher");
    if (!proto)
        return CKERR_OUTOFMEMORY;

    proto->DeclareInput("In");
    proto->DeclareOutput("Out");

    proto->DeclareInParameter("Mesh 1", CKPGUID_MESH);
    proto->DeclareInParameter("Coef 1", CKPGUID_FLOAT, "0");
    proto->DeclareInParameter("Mesh 2", CKPGUID_MESH);
    proto->DeclareInParameter("Coef 2", CKPGUID_FLOAT, "1");

    proto->DeclareSetting("Mesh Count", CKPGUID_INT, "2");
    proto->DeclareSetting("Calculate Normals", CKPGUID_BOOL, "TRUE");
    proto->DeclareSetting("Use Relative Morphing", CKPGUID_BOOL, "FALSE");

    proto->SetBehaviorFlags((CK_BEHAVIOR_FLAGS)(CKBEHAVIOR_TARGETABLE | CKBEHAVIOR_INTERNALLYCREATEDINPUTPARAMS));
    proto->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);
    proto->SetFunction(MultiMeshMorpher);

    proto->SetBehaviorCallbackFct(MultiMeshMorpherCallBack);

    *pproto = proto;
    return CK_OK;
}

/*****************************************************************/
/*                         FUNCTION                              */
/*****************************************************************/
int MultiMeshMorpher(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;

    beh->ActivateInput(0, FALSE);
    beh->ActivateOutput(0);

    CK3dObject *obj = (CK3dObject *)beh->GetTarget();
    if (!obj)
        return CKBR_OWNERERROR;

    CKMesh *meshA, *mesh = obj->GetCurrentMesh();
    if (!mesh)
        return CKBR_PARAMETERERROR;

    int a, b, c_pin = beh->GetInputParameterCount();

    int vertice_count = mesh->GetModifierVertexCount();

    CKDWORD dstStride = 0;
    VxVector *pos = NULL;
    VxVector *posA = NULL;
    VxVector *pos_init = (VxVector *)mesh->GetModifierVertices(&dstStride);
    if (!pos_init)
        return CKBR_PARAMETERERROR;

    //--- first mesh (initialisation)
    meshA = (CKMesh *)beh->GetInputParameterObject(0);
    if (!meshA)
        return CKBR_PARAMETERERROR;
    if (meshA->GetModifierVertexCount() != vertice_count)
        return CKBR_PARAMETERERROR;

    CKDWORD srcStride = 0;
    posA = (VxVector *)meshA->GetModifierVertices(&srcStride);
    if (!posA)
        return CKBR_PARAMETERERROR;

    pos = pos_init;

    // Use Relative Morphing ?
    CKBOOL useRelativeMorph = FALSE;
    beh->GetLocalParameterValue(2, &useRelativeMorph);

    // calc sum
    float coef = 1.0f;

    if (useRelativeMorph)
    {
        //_________________/ use RELATIVE morphing

        VxVector *posN, *posNeutral = posA;
        CKDWORD neutralStride = srcStride;

        //--- first mesh (initialisation)
        for (b = 0; b < vertice_count; b++)
        {
            *pos = *posA;
            pos = (VxVector *)((CKBYTE *)pos + dstStride);
            posA = (VxVector *)((CKBYTE *)posA + srcStride);
        }

        //--- other meshes
        for (a = 2; a < c_pin; a += 2)
        {

            meshA = (CKMesh *)beh->GetInputParameterObject(a);
            if (!meshA)
                continue;
            if (meshA->GetModifierVertexCount() != vertice_count)
                continue;

            beh->GetInputParameterValue(a + 1, &coef);

            if (coef)
            {
                CKDWORD currentSrcStride = 0;
                posA = (VxVector *)meshA->GetModifierVertices(&currentSrcStride);
                if (!posA)
                    continue;

                pos = pos_init;
                posN = posNeutral;

                for (b = 0; b < vertice_count; b++)
                {

                    *pos += coef * (*posA - *posN);

                    pos = (VxVector *)((CKBYTE *)pos + dstStride);
                    posA = (VxVector *)((CKBYTE *)posA + currentSrcStride);
                    posN = (VxVector *)((CKBYTE *)posN + neutralStride);
                }
            }
        }
    }
    else
    {
        //_________________/ use STANDARD morphing

        float inv_coef_sum = 0.0f;

        for (a = 1; a < c_pin; a += 2)
        {
            beh->GetInputParameterValue(a, &coef);
            inv_coef_sum += coef;
        }
        inv_coef_sum = (!inv_coef_sum) ? 1.0f : (1.0f / inv_coef_sum);

        coef = 1.0f;
        beh->GetInputParameterValue(1, &coef);
        coef *= inv_coef_sum;

        //--- first mesh (initialisation)
        for (b = 0; b < vertice_count; b++)
        {
            *pos = coef * *posA;
            pos = (VxVector *)((CKBYTE *)pos + dstStride);
            posA = (VxVector *)((CKBYTE *)posA + srcStride);
        }

        //--- other meshes
        for (a = 2; a < c_pin; a += 2)
        {
            meshA = (CKMesh *)beh->GetInputParameterObject(a);
            if (!meshA)
                continue;
            if (meshA->GetModifierVertexCount() != vertice_count)
                continue;

            beh->GetInputParameterValue(a + 1, &coef);
            if (coef)
            {
                coef *= inv_coef_sum;

                CKDWORD currentSrcStride = 0;
                posA = (VxVector *)meshA->GetModifierVertices(&currentSrcStride);
                if (!posA)
                    continue;

                pos = pos_init;

                for (b = 0; b < vertice_count; b++)
                {
                    *pos += coef * *posA;
                    pos = (VxVector *)((CKBYTE *)pos + dstStride);
                    posA = (VxVector *)((CKBYTE *)posA + currentSrcStride);
                }
            }
        }
    }

    // Update Normals ?
    CKBOOL updatenormals = TRUE;
    beh->GetLocalParameterValue(1, &updatenormals);

    mesh->ModifierVertexMove(updatenormals, TRUE);

    return CKBR_OK;
}

/*******************************************************/
/*                     CALLBACK                        */
/*******************************************************/
CKERROR MultiMeshMorpherCallBack(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;

    switch (behcontext.CallbackMessage)
    {
    case CKM_BEHAVIORSETTINGSEDITED:
    {
        int wanted_c_pin = 2;
        beh->GetLocalParameterValue(0, &wanted_c_pin);

        if (wanted_c_pin < 2)
        {
            wanted_c_pin = 2;
            beh->SetLocalParameterValue(0, &wanted_c_pin);
        }

        int c_pin = beh->GetInputParameterCount();

        char pin_str[32];

        while ((c_pin >> 1) < wanted_c_pin)
        { // we must add 'Input Param'
            snprintf(pin_str, sizeof(pin_str), "Mesh %d", (c_pin >> 1) + 1);
            beh->CreateInputParameter(pin_str, CKPGUID_MESH);
            snprintf(pin_str, sizeof(pin_str), "Coef %d", (c_pin >> 1) + 1);
            beh->CreateInputParameter(pin_str, CKPGUID_FLOAT);
            c_pin += 2;
        }

        while ((c_pin >> 1) > wanted_c_pin)
        { // we must remove 'Input Param'
            CKDestroyObject(beh->RemoveInputParameter(--c_pin));
            CKDestroyObject(beh->RemoveInputParameter(--c_pin));
        }

        // hide "Coef 1" as we use "Relative Morphing"
        // because "Mesh 1" is the neutral version
        CKBOOL useRelativeMorph = FALSE;
        beh->GetLocalParameterValue(2, &useRelativeMorph);

        CKParameterIn *pIn = beh->GetInputParameter(1);

        if (pIn)
        {
            CKParameterIn *pIn0 = beh->GetInputParameter(0);

            if (useRelativeMorph)
            {
                pIn->Enable(FALSE);
                pIn0->SetGUID(CKPGUID_MESH, TRUE, "Neutral");
            }
            else
            {
                pIn->Enable(TRUE);
                pIn0->SetGUID(CKPGUID_MESH, TRUE, "Mesh 1");
            }
        }
    }
    break;
    }

    return CKBR_OK;
}
