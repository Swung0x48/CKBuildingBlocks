/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
//
//		           TT Read Array
//
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
#include "CKAll.h"
#include "InterfaceManager.h"

#include <stdio.h>

CKObjectDeclaration *FillBehaviorReadArrayDecl();
CKERROR CreateReadArrayProto(CKBehaviorPrototype **pproto);
int ReadArray(const CKBehaviorContext &behcontext);

CKObjectDeclaration *FillBehaviorReadArrayDecl()
{
    CKObjectDeclaration *od = CreateCKObjectDeclaration("TT Read Array");
    od->SetDescription("Reads arrays from manager");
    od->SetCategory("TT InterfaceManager/General");
    od->SetType(CKDLL_BEHAVIORPROTOTYPE);
    od->SetGuid(CKGUID(0x5c825017, 0x6d367973));
    od->SetAuthorGuid(TERRATOOLS_GUID);
    od->SetAuthorName("Terratools");
    od->SetVersion(0x00010000);
    od->SetCreationFunction(CreateReadArrayProto);
    od->SetCompatibleClassId(CKCID_BEOBJECT);
    return od;
}

CKERROR CreateReadArrayProto(CKBehaviorPrototype **pproto)
{
    CKBehaviorPrototype *proto = CreateCKBehaviorPrototype("TT Read Array");
    if (!proto) return CKERR_OUTOFMEMORY;

    proto->DeclareInput("In");

    proto->DeclareOutput("Out");

    proto->DeclareInParameter("Name of Array", CKPGUID_DATAARRAY);
    proto->DeclareInParameter("Filename of CMO", CKPGUID_STRING);
    proto->DeclareInParameter("Show message: 'red array'", CKPGUID_BOOL);

    proto->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);
    proto->SetFunction(ReadArray);

    *pproto = proto;
    return CK_OK;
}

int ReadArray(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;
    CKContext *context = behcontext.Context;

    beh->ActivateInput(0, FALSE);

    InterfaceManager *man = InterfaceManager::GetManager(context);
    if (!man)
    {
        InterfaceManager::PostPlayerCommand(context, TT_PLAYER_COMMAND_NO_GAMEINFO, 0x19, 1);
        context->OutputToConsoleExBeep("ReadArray: gameInfo not exists");
        beh->ActivateOutput(0);
        return CKBR_OK;
    }

    CKDataArray *array = (CKDataArray *)beh->GetInputParameterObject(0);
    CKSTRING cmo = (CKSTRING)beh->GetInputParameterReadDataPtr(1);
    if (!array || !cmo)
    {
        context->OutputToConsoleExBeep("ReadArray: invalid array or cmo");
        beh->ActivateOutput(0);
        return CKBR_OK;
    }

    CNemoArrayList *nemoArraylist = man->GetNemoArrayList();
    CNemoArray *nemoArray = nemoArraylist->Search(cmo, array);
    if (nemoArray)
    {
        nemoArray->Read(array);

        CKBOOL bShowMessage = FALSE;
        beh->GetInputParameterValue(2, &bShowMessage);
        if (bShowMessage)
        {
            char *msg = new char[128];
            snprintf(msg, 128, " '%s'  from file '%s' read from manager", array->GetName(), cmo);
            context->OutputToConsole(msg, FALSE);
            delete[] msg;
        }
    }

    beh->ActivateOutput(0);
    return CKBR_OK;
}
