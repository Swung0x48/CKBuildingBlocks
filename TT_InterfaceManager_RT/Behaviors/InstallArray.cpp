/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
//
//		          TT Install Array
//
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
#include "CKAll.h"
#include "InterfaceManager.h"

#include <stdio.h>

CKObjectDeclaration *FillBehaviorInstallArrayDecl();
CKERROR CreateInstallArrayProto(CKBehaviorPrototype **);
int InstallArray(const CKBehaviorContext &behcontext);

CKObjectDeclaration *FillBehaviorInstallArrayDecl()
{
    CKObjectDeclaration *od = CreateCKObjectDeclaration("TT Install Array");
    od->SetDescription("Install Array to Manager");
    od->SetCategory("TT InterfaceManager/General");
    od->SetType(CKDLL_BEHAVIORPROTOTYPE);
    od->SetGuid(CKGUID(0x41f47ec4, 0x69470bbd));
    od->SetAuthorGuid(TERRATOOLS_GUID);
    od->SetAuthorName("Terratools");
    od->SetVersion(0x00010000);
    od->SetCreationFunction(CreateInstallArrayProto);
    od->SetCompatibleClassId(CKCID_BEOBJECT);
    return od;
}

CKERROR CreateInstallArrayProto(CKBehaviorPrototype **pproto)
{
    CKBehaviorPrototype *proto = CreateCKBehaviorPrototype("TT Install Array");
    if (!proto) return CKERR_OUTOFMEMORY;

    proto->DeclareInput("In");

    proto->DeclareOutput("Out");

    proto->DeclareInParameter("Array to install", CKPGUID_DATAARRAY);
    proto->DeclareInParameter("Filename of current CMO", CKPGUID_STRING);
    proto->DeclareInParameter("Show message: INSTALLED", CKPGUID_BOOL);

    proto->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);
    proto->SetFunction(InstallArray);

    *pproto = proto;
    return CK_OK;
}

int InstallArray(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;
    CKContext *context = behcontext.Context;

    beh->ActivateInput(0, FALSE);

    InterfaceManager *man = InterfaceManager::GetManager(context);
    if (!man)
    {
        InterfaceManager::PostPlayerCommand(context, TT_PLAYER_COMMAND_NO_GAMEINFO, 0x1A, 1);
        context->OutputToConsoleExBeep("InstallArray: gameInfo not exists");
        beh->ActivateOutput(0);
        return CKBR_OK;
    }

    CKDataArray *array = (CKDataArray *)beh->GetInputParameterObject(0);
    CKSTRING cmo = (CKSTRING)beh->GetInputParameterReadDataPtr(1);
    if (!array || !cmo)
    {
        context->OutputToConsoleExBeep("InstallArray: invalid array or cmo");
        beh->ActivateOutput(0);
        return CKBR_OK;
    }

    CNemoArrayList *nemoArrayList = man->GetNemoArrayList();
    if (!nemoArrayList->Search(cmo, array))
    {
        nemoArrayList->Add(cmo, array);

        CKBOOL bShowMessage = FALSE;
        beh->GetInputParameterValue(2, &bShowMessage);
        if (bShowMessage)
        {
            char *msg = new char[256];
            snprintf(msg, 256, " '%s'  from file '%s' installed to manager", array->GetName(), cmo);
            context->OutputToConsole(msg, FALSE);
            delete[] msg;
        }
    }

    beh->ActivateOutput(0);
    return CKBR_OK;
}
