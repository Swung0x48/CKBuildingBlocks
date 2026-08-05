/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
//
//		            FillLayer
//
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
#include "CKAll.h"

CKObjectDeclaration *FillBehaviorFillLayerDecl();
CKERROR CreateFillLayerProto(CKBehaviorPrototype **);
int FillLayer(const CKBehaviorContext &behcontext);

CKObjectDeclaration *FillBehaviorFillLayerDecl()
{
    CKObjectDeclaration *od = CreateCKObjectDeclaration("Fill Layer");
    od->SetDescription("Fills a specific layer type with some value.");
    /* rem:
    <SPAN CLASS=in>In : </SPAN>triggers the process.<BR>
    <SPAN CLASS=out>Out : </SPAN>is activated when the process is completed.<BR>
    <BR>
    <SPAN CLASS=pin>Layer : </SPAN>Layer to be concidered.<BR>
    <SPAN CLASS=pin>Value : </SPAN>Value to be put on each square.<BR>
    <BR><BR>
    */
    /* warning:
    - by now the only possible values are bound to a 0-255 range.<BR>
    */
    od->SetType(CKDLL_BEHAVIORPROTOTYPE);
    od->SetCategory("Grids/Basic");
    od->SetGuid(CKGUID(0x513344bb, 0x6b943d9f));
    od->SetAuthorGuid(VIRTOOLS_GUID);
    od->SetAuthorName("Virtools");
    od->SetVersion(0x00010000);
    od->SetCreationFunction(CreateFillLayerProto);
    od->SetCompatibleClassId(CKCID_BEOBJECT);
    return od;
}

CKERROR CreateFillLayerProto(CKBehaviorPrototype **pproto)
{
    CKBehaviorPrototype *proto = CreateCKBehaviorPrototype("Fill Layer");
    if (!proto)
        return CKERR_OUTOFMEMORY;

    proto->DeclareInput("In");
    proto->DeclareOutput("Out");

    proto->DeclareInParameter("Layer", CKPGUID_LAYERTYPE, DEFAULT_LAYER_NAME);
    proto->DeclareInParameter("Value", CKPGUID_INT, "255");

    proto->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);
    proto->SetFunction(FillLayer);

    *pproto = proto;
    return CK_OK;
}

int FillLayer(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;
    CKContext *ctx = behcontext.Context;

    beh->ActivateInput(0, FALSE);
    beh->ActivateOutput(0);

    //_________________________________/ Retrieve Inputs
    int layer_type = 1;
    beh->GetInputParameterValue(0, &layer_type);

    int value = 255;
    beh->GetInputParameterValue(1, &value);

    //_________________________________/ Get Grids
    CKGridManager *gm = (CKGridManager *)ctx->GetManagerByGuid(GRID_MANAGER_GUID);
    CKGrid *grid;
    const XObjectPointerArray &array = gm->GetGridArray();

    for (CKObject **o = array.Begin(); o != array.End(); ++o)
    {
        if ((grid = (CKGrid *)*o))
        {
            if (grid->IsActive())
            {
                CKLayer *layer = grid->GetLayer(layer_type);
                if (layer)
                {

                    CKSquare *square = layer->GetSquareArray();

                    if (value > 255)
                        value = 255;
                    else if (value < 0)
                        value = 0;

                    //____________________________/ Fill Layer
                    CKSquare *squarefinal = square + grid->GetWidth() * grid->GetLength();
                    while (square < squarefinal)
                    {
                        square->ival = value;
                        ++square;
                    }
                }
            }
        }
    }

    return CKBR_OK;
}
