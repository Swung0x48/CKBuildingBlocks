/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
//
//		            GetPosFromValue
//
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
#include "CKAll.h"

CKObjectDeclaration *FillBehaviorGetPosFromValueDecl();
CKERROR CreateGetPosFromValueProto(CKBehaviorPrototype **);
int GetPosFromValue(const CKBehaviorContext &behcontext);

typedef CKBOOL (*OpFctVal)(CKSquare *square, int value, int w, int l);

struct
{
    int a;
    int b;
    int grid;
    OpFctVal fcttabtest;
} localposfromval;

CKObjectDeclaration *FillBehaviorGetPosFromValueDecl()
{
    CKObjectDeclaration *od = CreateCKObjectDeclaration("Get Pos From Value");
    od->SetDescription("Gets the position of all the layer squares confirm test.");
    /* rem:
    <SPAN CLASS=in>In : </SPAN>triggers the process.<BR>
    <SPAN CLASS=in>Loop In : </SPAN>triggers the next step in a behavior's process loop.<BR>

      <SPAN CLASS=out>Out : </SPAN>is activated when the process is completed.<BR>
      <SPAN CLASS=out>Loop Out : </SPAN>is activated when the behavior's process needs to loop.<BR>
      <BR>
      <SPAN CLASS=pin>Layer : </SPAN>Layer to be concidered.<BR>
      <SPAN CLASS=pin>Value : </SPAN>Value to be put.<BR>
      <SPAN CLASS=pin>Test : </SPAN>Test to be done with the given value.<BR>
      <BR>
      <SPAN CLASS=pout>Pos3d : </SPAN>3d position of the pointed square (nota bene: this position is expressed in a specific grid coordinate [see next output parameter]).
      The Y component of the given 3d vector stands for the 'height validity' [0-1].
      <BR>
      <SPAN CLASS=pout>Grid Ref : </SPAN>Grid basis in which the 3d position is expressed (it is also the grid from which the square were retrieved).<BR>
      <BR><BR>
      Remember, if you want to express an object position just set the referential as your object, and let 3dpos to (0,0,0)
    */
    /* warning:
    - by now the only possible values are bound to a 0-255 range.<BR>
    - the Test is taken into concideration only when entering by <FONT COLOR=#a03030>'In'</FONT>. (You can't change it will looping)<BR>
    */
    od->SetType(CKDLL_BEHAVIORPROTOTYPE);
    od->SetCategory("Grids/Basic");
    od->SetGuid(CKGUID(0x670616ad, 0x2a5c54ac));
    od->SetAuthorGuid(VIRTOOLS_GUID);
    od->SetAuthorName("Virtools");
    od->SetVersion(0x00010000);
    od->SetCreationFunction(CreateGetPosFromValueProto);
    od->SetCompatibleClassId(CKCID_BEOBJECT);
    return od;
}

/************************************************/
/*            Functions of Test                 */
/************************************************/
//______________________________________________/
// Equal=1, Not Equal=2, Lesser than=3, Lesser or equal than=4, Greater than=5, Greater or equal than=6;
OpFctVal fcttab[6];

//____________________________________/ EQUAL
CKBOOL fcttab_Equal(CKSquare *square, int value, int w, int l)
{
    int a = localposfromval.a;
    for (int b = localposfromval.b; b < l; ++b, a = 0)
    {
        for (; a < w; ++a, ++square)
        {
            if (square->ival == value)
            {
                localposfromval.a = a;
                localposfromval.b = b;
                return TRUE;
            }
        }
    }
    return FALSE;
}

//____________________________________/ NOT EQUAL
CKBOOL fcttab_NotEqual(CKSquare *square, int value, int w, int l)
{
    int a = localposfromval.a;
    for (int b = localposfromval.b; b < l; ++b, a = 0)
    {
        for (; a < w; ++a, ++square)
        {
            if (square->ival != value)
            {
                localposfromval.a = a;
                localposfromval.b = b;
                return TRUE;
            }
        }
    }
    return FALSE;
}

//____________________________________/ LESS THAN
CKBOOL fcttab_Less(CKSquare *square, int value, int w, int l)
{
    int a = localposfromval.a;
    for (int b = localposfromval.b; b < l; ++b, a = 0)
    {
        for (; a < w; ++a, ++square)
        {
            if (square->ival < value)
            {
                localposfromval.a = a;
                localposfromval.b = b;
                return TRUE;
            }
        }
    }
    return FALSE;
}

//____________________________________/ LESS OR EQUAL
CKBOOL fcttab_LessEqual(CKSquare *square, int value, int w, int l)
{
    int a = localposfromval.a;
    for (int b = localposfromval.b; b < l; ++b, a = 0)
    {
        for (; a < w; ++a, ++square)
        {
            if (square->ival <= value)
            {
                localposfromval.a = a;
                localposfromval.b = b;
                return TRUE;
            }
        }
    }
    return FALSE;
}

//____________________________________/ GREATER THAN
CKBOOL fcttab_Greater(CKSquare *square, int value, int w, int l)
{
    int a = localposfromval.a;
    for (int b = localposfromval.b; b < l; ++b, a = 0)
    {
        for (; a < w; ++a, ++square)
        {
            if (square->ival > value)
            {
                localposfromval.a = a;
                localposfromval.b = b;
                return TRUE;
            }
        }
    }
    return FALSE;
}

//____________________________________/ GREATER OR EQUAL
CKBOOL fcttab_GreaterEqual(CKSquare *square, int value, int w, int l)
{
    int a = localposfromval.a;
    for (int b = localposfromval.b; b < l; ++b, a = 0)
    {
        for (; a < w; ++a, ++square)
        {
            if (square->ival >= value)
            {
                localposfromval.a = a;
                localposfromval.b = b;
                return TRUE;
            }
        }
    }
    return FALSE;
}

/************************************************/
/*                Proto                         */
/************************************************/
CKERROR CreateGetPosFromValueProto(CKBehaviorPrototype **pproto)
{
    fcttab[0] = fcttab_Equal;
    fcttab[1] = fcttab_NotEqual;
    fcttab[2] = fcttab_Less;
    fcttab[3] = fcttab_LessEqual;
    fcttab[4] = fcttab_Greater;
    fcttab[5] = fcttab_GreaterEqual;

    CKBehaviorPrototype *proto = CreateCKBehaviorPrototype("Get Pos From Value");
    if (!proto)
        return CKERR_OUTOFMEMORY;

    proto->DeclareInput("In");
    proto->DeclareInput("Loop In");

    proto->DeclareOutput("Out");
    proto->DeclareOutput("Loop Out");

    proto->DeclareInParameter("Layer", CKPGUID_LAYERTYPE, DEFAULT_LAYER_NAME);
    proto->DeclareInParameter("Value", CKPGUID_INT, "255");
    proto->DeclareInParameter("Test", CKPGUID_COMPOPERATOR, "Equal");

    proto->DeclareOutParameter("Pos3d", CKPGUID_VECTOR);
    proto->DeclareOutParameter("Grid Ref", CKPGUID_3DENTITY);

    proto->DeclareLocalParameter("local", CKPGUID_VOIDBUF);

    proto->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);
    proto->SetFunction(GetPosFromValue);

    *pproto = proto;
    return CK_OK;
}

/************************************************/
/*            Behavior Function                 */
/************************************************/
int GetPosFromValue(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;
    CKContext *ctx = behcontext.Context;

    //_________________________________/ Retrieve Inputs
    int layer_type = 1;
    beh->GetInputParameterValue(0, &layer_type);

    int value = 255;
    beh->GetInputParameterValue(1, &value);

    if (beh->IsInputActive(0))
    { //_________________________________/ Enter by IN
        beh->ActivateInput(0, FALSE);
        localposfromval.a = 0;
        localposfromval.b = 0;
        localposfromval.grid = 0;

        //---
        CK_COMPOPERATOR op = CKEQUAL;
        beh->GetInputParameterValue(2, &op);

        localposfromval.fcttabtest = fcttab[op - 1];
        //---

        beh->SetLocalParameterValue(0, &localposfromval, sizeof(localposfromval));
    }
    else
    { //_________________________________/ Enter by LOOPIN
        beh->ActivateInput(1, FALSE);
    }

    //_________________________________/ Retireve Local Params
    beh->GetLocalParameterValue(0, &localposfromval);

    //_________________________________/ Get Grids
    CKGridManager *gm = (CKGridManager *)ctx->GetManagerByGuid(GRID_MANAGER_GUID);
    int count = gm->GetGridObjectCount();
    CKGrid *grid;
    for (int c = localposfromval.grid; c < count; c++)
    {
        if ((grid = gm->GetGridObject(c)))
        {
            if (grid->IsActive())
            {
                CKLayer *layer = grid->GetLayer(layer_type);
                if (layer)
                {

                    int w = grid->GetWidth();
                    int l = grid->GetLength();

                    CKSquare *square = layer->GetSquareArray() + localposfromval.a + localposfromval.b * w;

                    if (localposfromval.fcttabtest(square, value, w, l))
                    {
                        localposfromval.grid = c;
                        VxVector pos;
                        pos.x = (float)localposfromval.a + 0.5f;
                        pos.y = 0.5f;
                        pos.z = (float)localposfromval.b + 0.5f;
                        beh->SetOutputParameterValue(0, &pos);
                        beh->SetOutputParameterObject(1, grid);
                        localposfromval.a++;
                        beh->SetLocalParameterValue(0, &localposfromval);
                        beh->ActivateOutput(1); // activate LOOP OUT
                        return CKBR_OK;
                    }
                }
            }
        }
        localposfromval.a = localposfromval.b = 0;
    }

    beh->ActivateOutput(0); // activate OUT

    return CKBR_OK;
}
