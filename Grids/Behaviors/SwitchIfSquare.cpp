/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
//
//		            SwitchIfSquare
//
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
#include "CKAll.h"

CKObjectDeclaration *FillBehaviorSwitchIfSquareDecl();
CKERROR CreateSwitchIfSquareProto(CKBehaviorPrototype **);
int SwitchIfSquare(const CKBehaviorContext &behcontext);

CKERROR SwitchIfSquareCallBack(const CKBehaviorContext &behcontext); // CallBack Function

CKObjectDeclaration *FillBehaviorSwitchIfSquareDecl()
{
    CKObjectDeclaration *od = CreateCKObjectDeclaration("Switch If Square");
    od->SetDescription("Switch streaming if the value on the specified layer on the specified position isn't NULL.");
    /* rem:
    <SPAN CLASS=in>In : </SPAN>triggers the process.<BR>
    <BR>
    <SPAN CLASS=out>None : </SPAN>is activated when the process is completed.<BR>
    <SPAN CLASS=out>Layer1 : </SPAN>is activated when a none null value is detected on first layer, at the specific position.<BR>
    <SPAN CLASS=out>Layer2 : </SPAN>is activated when a none null value is detected on second layer, at the specific position.<BR>
    <SPAN CLASS=out>...</SPAN><BR>
    <BR>
    <SPAN CLASS=pin>Pos3d : </SPAN>3d position to be converted into a 2d position in the affected grid.<BR>
    <SPAN CLASS=pin>Ref : </SPAN>basis in which the 3d position is expressed.<BR>
    <SPAN CLASS=pin>Layer1 : </SPAN>First layer to be considered.<BR>
    <SPAN CLASS=pin>Layer2 : </SPAN>Second layer to be considered.<BR>
    <SPAN CLASS=pin>...</SPAN><BR>
    <BR>
    <SPAN CLASS=pout>Value1 : </SPAN>Value of square from first Layer.<BR>
    <SPAN CLASS=pout>Value2 : </SPAN>Value of square from second Layer.<BR>
    <SPAN CLASS=pout>...</SPAN><BR>
    <BR><BR>
    Remember, if you want to express an object position just set the referential as your object, and let 3dpos to (0,0,0).<BR>
    */
    /* warning:
    - by now the only possible values are bound to a 0-255 range.<BR>
    */
    od->SetType(CKDLL_BEHAVIORPROTOTYPE);
    od->SetCategory("Grids/Basic");
    od->SetGuid(CKGUID(0x254074c1, 0x22c0397e));
    od->SetAuthorGuid(VIRTOOLS_GUID);
    od->SetAuthorName("Virtools");
    od->SetVersion(0x00010000);
    od->SetCreationFunction(CreateSwitchIfSquareProto);
    od->SetCompatibleClassId(CKCID_BEOBJECT);
    return od;
}

CKERROR CreateSwitchIfSquareProto(CKBehaviorPrototype **pproto)
{
    CKBehaviorPrototype *proto = CreateCKBehaviorPrototype("Switch If Square");
    if (!proto)
        return CKERR_OUTOFMEMORY;

    proto->DeclareInput("In");
    proto->DeclareOutput("None");
    proto->DeclareOutput("Layer1");

    proto->DeclareInParameter("Pos 3D", CKPGUID_VECTOR, "0,0,0");
    proto->DeclareInParameter("Ref", CKPGUID_3DENTITY);
    proto->DeclareInParameter("Layer1", CKPGUID_LAYERTYPE, DEFAULT_LAYER_NAME);

    proto->DeclareOutParameter("Value1", CKPGUID_INT);

    proto->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);
    proto->SetFunction(SwitchIfSquare);
    proto->SetBehaviorFlags((CK_BEHAVIOR_FLAGS)(CKBEHAVIOR_VARIABLEPARAMETERINPUTS | CKBEHAVIOR_INTERNALLYCREATEDOUTPUTS | CKBEHAVIOR_INTERNALLYCREATEDOUTPUTPARAMS | CKBEHAVIOR_INTERNALLYCREATEDINPUTPARAMS));
    proto->SetBehaviorCallbackFct(SwitchIfSquareCallBack, CKCB_BEHAVIORALL);

    *pproto = proto;
    return CK_OK;
}

int SwitchIfSquare(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;
    CKContext *ctx = behcontext.Context;

    beh->ActivateInput(0, FALSE);

    //_________________________________/ Retrieve Inputs
    VxVector pos(0, 0, 0);
    beh->GetInputParameterValue(0, &pos);

    CK3dEntity *ref = (CK3dEntity *)beh->GetInputParameterObject(1);

    //_________________________________/ Get Preferred Grid
    CKGridManager *gm = (CKGridManager *)ctx->GetManagerByGuid(GRID_MANAGER_GUID);
    CKGrid *grid = gm->GetPreferredGrid(&pos, ref);

    CKBOOL none = TRUE;

    if (grid)
    {

        VxVector tmp;
        grid->InverseTransform(&tmp, &pos, ref);

        int x = (int)tmp.x;
        int y = (int)tmp.z;

        int value, layer_type;
        CKLayer *layer;
        int count = beh->GetInputParameterCount() - 2;
        while (count)
        {
            --count;
            layer_type = 1;
            beh->GetInputParameterValue(count + 2, &layer_type);
            if ((layer = grid->GetLayer(layer_type)))
            {
                value = 0;
                layer->GetValue(x, y, &value);
                beh->SetOutputParameterValue(count, &value);
                if (value)
                {
                    beh->ActivateOutput(count + 1);
                    none = FALSE;
                }
            }
        }
    }

    if (none)
        beh->ActivateOutput(0);

    return CKBR_OK;
}

/*******************************************************/
/*                     CALLBACK                        */
/*******************************************************/
CKERROR SwitchIfSquareCallBack(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;

    switch (behcontext.CallbackMessage)
    {

    case CKM_BEHAVIOREDITED:
    {
        int c_out = beh->GetOutputCount() - 1;
        int c_pin = beh->GetInputParameterCount() - 2;
        int c_pout = beh->GetOutputParameterCount();

        char out_str[15];

        while (c_out < c_pin) // we must add 'Outputs'
        {
            snprintf(out_str, sizeof(out_str), "Layer%d", c_out + 1);
            beh->AddOutput(out_str);
            c_out++;
        }

        while (c_out > c_pin) // we must remove 'Outputs'
        {
            beh->DeleteOutput(c_out);
            c_out--;
        }

        CKParameter *pout;
        while (c_pout < c_pin) // we must add 'Output Params'
        {
            snprintf(out_str, sizeof(out_str), "Value%d", c_pout + 1);
            beh->CreateOutputParameter(out_str, CKPGUID_INT);
            c_pout++;
        }

        while (c_pout > c_pin) // we must remove 'Output Params'
        {
            pout = beh->GetOutputParameter(c_pout - 1);
            CKDestroyObject(pout);
            c_pout--;
        }

        CKParameterIn *pin1;
        if ((pin1 = beh->GetInputParameter(0)))
        {

            CKParameterIn *pin;
            int type_1 = pin1->GetType();
            while (c_pin) // we check the type of each 'Input Parameter'
            {
                c_pin--;
                pin = beh->GetInputParameter(c_pin + 2);
                if (pin->GetGUID() != CKPGUID_LAYERTYPE)
                {
                    pin->SetGUID(CKPGUID_LAYERTYPE);
                    if ((pout = pin->GetRealSource()))
                    {
                        pout->SetGUID(CKPGUID_LAYERTYPE);
                    }
                }
            }
        }
    }
    break;
    }

    return CKBR_OK;
}
