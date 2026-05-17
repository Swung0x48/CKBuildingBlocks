/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
//
//		            SetFace
//
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
#include "CKAll.h"

CKERROR CreateSetFaceBehaviorProto(CKBehaviorPrototype **);
int SetFace(const CKBehaviorContext &behcontext);

CKObjectDeclaration *FillBehaviorSetFaceDecl()
{
    CKObjectDeclaration *od = CreateCKObjectDeclaration("Set Face Properties");
    od->SetDescription("Set the properties of a Face.");
    /* rem:
    <SPAN CLASS=in>In: </SPAN>triggers the process.<BR>
    <SPAN CLASS=out>Out: </SPAN>is activated when the process is completed.<BR>
    <BR>
    <SPAN CLASS=pin>Face Index: </SPAN>index of the Face to modify.<BR>
    <SPAN CLASS=pin>Face Material: </SPAN>material to set to the face.<BR>
    <SPAN CLASS=pin>Face Vertex Indices: </SPAN>index of the vertex compositing the face.<BR>
    <BR>
    */
    /* warning:
    - If you do not link one input parameter, it will not be affected.<BR>
    */
    od->SetCategory("Mesh Modifications/Basic");
    od->SetType(CKDLL_BEHAVIORPROTOTYPE);
    od->SetGuid(CKGUID(0x1d8e2716, 0x64f47239));
    od->SetAuthorGuid(CKGUID());
    od->SetAuthorName("Virtools");
    od->SetVersion(0x00010000);
    od->SetCreationFunction(CreateSetFaceBehaviorProto);
    od->SetCompatibleClassId(CKCID_MESH);
    return od;
}

CKERROR CreateSetFaceBehaviorProto(CKBehaviorPrototype **pproto)
{
    CKBehaviorPrototype *proto = CreateCKBehaviorPrototype("Set Face Properties");
    if (!proto)
        return CKERR_OUTOFMEMORY;

    proto->DeclareInput("In");
    proto->DeclareOutput("Out");

    proto->DeclareInParameter("Face Index", CKPGUID_INT);
    proto->DeclareInParameter("Face Material", CKPGUID_MATERIAL);
    proto->DeclareInParameter("Face Vertex Indices", CKPGUID_VECTOR, "0,1,2");

    proto->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);

    proto->SetFunction(SetFace);
    proto->SetBehaviorFlags(CKBEHAVIOR_TARGETABLE);

    *pproto = proto;
    return CK_OK;
}

int SetFace(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;

    beh->ActivateInput(0, FALSE);
    beh->ActivateOutput(0);

    CKMesh *mesh = (CKMesh *)beh->GetTarget();
    if (!mesh)
        return CKBR_OWNERERROR;

    int face = 0;
    beh->GetInputParameterValue(0, &face);

    // Material
    int matid = 0;
    if (!beh->GetInputParameterValue(1, &matid))
    {
        CKMaterial *mat = (CKMaterial *)behcontext.Context->GetObject(matid);
        mesh->SetFaceMaterial(face, mat);
    }

    VxVector v;
    // Vertices Index
    if (!beh->GetInputParameterValue(2, &v))
    {
        int a, b, c;
        a = (int)v.x;
        b = (int)v.y;
        c = (int)v.z;
        mesh->SetFaceVertexIndex(face, a, b, c);
        mesh->ModifierVertexMove(FALSE, TRUE);
    }

    return CKBR_OK;
}
