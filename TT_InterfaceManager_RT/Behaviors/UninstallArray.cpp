/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
//
//		          TT Uninstall Array
//
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
#include "CKAll.h"
#include "InterfaceManager.h"

#include <stdio.h>

CKObjectDeclaration *FillBehaviorUninstallArrayDecl();
CKERROR CreateUninstallArrayProto(CKBehaviorPrototype **pproto);
int UninstallArray(const CKBehaviorContext &behcontext);

CKObjectDeclaration *FillBehaviorUninstallArrayDecl()
{
    CKObjectDeclaration *od = CreateCKObjectDeclaration("TT Uninstall Array");
    od->SetDescription("Uninstalls Array from Manager");
    od->SetCategory("TT InterfaceManager/General");
    od->SetType(CKDLL_BEHAVIORPROTOTYPE);
    od->SetGuid(CKGUID(0x77dd56cf, 0x63dc30b4));
    od->SetAuthorGuid(TERRATOOLS_GUID);
    od->SetAuthorName("Terratools");
    od->SetVersion(0x00010000);
    od->SetCreationFunction(CreateUninstallArrayProto);
    od->SetCompatibleClassId(CKCID_BEOBJECT);
    return od;
}

CKERROR CreateUninstallArrayProto(CKBehaviorPrototype **pproto)
{
    CKBehaviorPrototype *proto = CreateCKBehaviorPrototype("TT Uninstall Array");
    if (!proto) return CKERR_OUTOFMEMORY;

    proto->DeclareInput("In");

    proto->DeclareOutput("Out");

    proto->DeclareInParameter("Array to uninstall", CKPGUID_DATAARRAY);
    proto->DeclareInParameter("Filename of current CMO", CKPGUID_STRING);
    proto->DeclareInParameter("Show message: UNINSTALLED", CKPGUID_BOOL);

    proto->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);
    proto->SetFunction(UninstallArray);

    *pproto = proto;
    return CK_OK;
}

int UninstallArray(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;
    CKContext *context = behcontext.Context;

    beh->ActivateInput(0, FALSE);

    InterfaceManager *man = InterfaceManager::GetManager(context);
    if (!man)
    {
        InterfaceManager::PostPlayerCommand(context, TT_PLAYER_COMMAND_NO_GAMEINFO, 0x1D, 1);
        context->OutputToConsoleExBeep("UninstallArray: gameInfo not exists");
        beh->ActivateOutput(0);
        return CKBR_OK;
    }

    CKDataArray *array = (CKDataArray *)beh->GetInputParameterObject(0);
    CKSTRING cmo = (CKSTRING)beh->GetInputParameterReadDataPtr(1);
    if (!array || !cmo)
    {
        context->OutputToConsoleExBeep("UninstallArray: invalid array or cmo");
        beh->ActivateOutput(0);
        return CKBR_OK;
    }

    CNemoArrayList *nemoArrayList = man->GetNemoArrayList();
    if (nemoArrayList->Search(cmo, array))
    {
        nemoArrayList->Remove(cmo, array);

        CKBOOL bShowMessage = FALSE;
        beh->GetInputParameterValue(2, &bShowMessage);
        if (bShowMessage)
        {
            char *msg = new char[256];
            snprintf(msg, 256, " '%s'  from file '%s' uninstalled from manager", array->GetName(), cmo);
            context->OutputToConsole(msg, FALSE);
            delete[] msg;
        }
    }

    beh->ActivateOutput(0);
    return CKBR_OK;
}
