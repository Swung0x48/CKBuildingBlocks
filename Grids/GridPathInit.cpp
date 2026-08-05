/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
//
//		            Grid Path Init
//
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
#include "CKAll.h"
#include "GridManager.h"
#include "GridPathManager.h"

// input
enum { INPUT_IN };

// outpout
enum { OUTPUT_OUT, OUTPUT_ERROR };

CKObjectDeclaration *FillBehaviorGridPathInitDecl();
CKERROR CreateGridPathInitProto(CKBehaviorPrototype **);
CKERROR GridPathInitCallback(const CKBehaviorContext &behcontext);
int GridPathInit(const CKBehaviorContext &behcontext);

//-------------------------------------------------
// Declaration
//-------------------------------------------------

CKObjectDeclaration *FillBehaviorGridPathInitDecl()
{
    CKObjectDeclaration *od = CreateCKObjectDeclaration("Grid Path Init");

    od->SetDescription("Initialize the grid graph for grid path finding");
    /* rem:
    <SPAN CLASS=in>In: </SPAN>Activate the initialisation of the grid path graph.<BR>
    <BR>
    <SPAN CLASS=out>Out: </SPAN>Is activate when graph is initialized.<BR>
    <SPAN CLASS=out>Error: </SPAN>Is activate if graph can't be initialyzed..<BR>
    <BR>
    <SPAN CLASS=pin>Layer 1: </SPAN>Obstacle layer.<BR>
    ...etc.<BR>
    <BR>
    <BR>
    */
    od->SetCategory("Grids/Path Finding");
    od->SetType(CKDLL_BEHAVIORPROTOTYPE);
    od->SetGuid(CKGUID(0x74d12456, 0x6f481719));
    od->SetAuthorGuid(VIRTOOLS_GUID);
    od->SetAuthorName("Virtools");
    od->SetVersion(0x00010000);
    od->SetCreationFunction(CreateGridPathInitProto);
    od->SetCompatibleClassId(CKCID_BEOBJECT);
    return od;
}

//-------------------------------------------------
// Prototype
//-------------------------------------------------

CKERROR CreateGridPathInitProto(CKBehaviorPrototype **pproto)
{
    CKBehaviorPrototype *proto = CreateCKBehaviorPrototype("Grid Path Init");
    if (!proto)
        return CKERR_OUTOFMEMORY;

    proto->DeclareInput("In");

    proto->DeclareOutput("Out");
    proto->DeclareOutput("Error");

    proto->DeclareInParameter("Layer 1", CKPGUID_LAYERTYPE);

    proto->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);
    proto->SetFunction(GridPathInit);
    proto->SetBehaviorFlags((CK_BEHAVIOR_FLAGS)(CKBEHAVIOR_VARIABLEPARAMETERINPUTS));
    proto->SetBehaviorCallbackFct(GridPathInitCallback);

    *pproto = proto;
    return CK_OK;
}

//-------------------------------------------------
// Fonction
//-------------------------------------------------

int GridPathInit(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;
    CKGridManager *gm = (CKGridManager *)behcontext.Context->GetManagerByGuid(GRID_MANAGER_GUID);
    GridPathManager *gridPathManager = ((GridManager *)gm)->GetGridPathManager();
    XList<int> listLayer;
    int layer;

    beh->ActivateInput(INPUT_IN, FALSE);

    // Fill the layer list.
    for (int i = 0; i < beh->GetInputParameterCount(); i++)
    {
        beh->GetInputParameterValue(i, &layer);
        listLayer.PushBack(layer);
    }

    // Construct graph.
    if (gridPathManager->ConstructGraph(&listLayer))
        beh->ActivateOutput(OUTPUT_OUT);
    else
        beh->ActivateOutput(OUTPUT_ERROR);

    return CKBR_OK;
}

//-------------------------------------------------
// Callback
//-------------------------------------------------

CKERROR GridPathInitCallback(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;

    switch (behcontext.CallbackMessage)
    {
    case CKM_BEHAVIOREDITED:
    case CKM_BEHAVIORSETTINGSEDITED:
    {
        CKParameterIn *pin;
        char name[10];

        // Force parameter input type to "layer type".
        // And rename to "layer i".
        for (int i = 1; i < beh->GetInputParameterCount(); i++)
        {
            snprintf(name, sizeof(name), "Layer %d", i + 1);
            pin = beh->GetInputParameter(i);
            pin->SetGUID(CKPGUID_LAYERTYPE);
            pin->SetName(name);
        }
    }
    break;
    }
    return CKBR_OK;
}
