//////////////////////////////////////////
//////////////////////////////////////////
//
//        TT_ListDir
//
//////////////////////////////////////////
//////////////////////////////////////////
#include "CKAll.h"
#include "ToolboxGuids.h"
#include "VxWindowFunctions.h"

CKObjectDeclaration *FillBehaviorListDirDecl();
CKERROR CreateListDirProto(CKBehaviorPrototype **pproto);
int ListDir(const CKBehaviorContext &behcontext);

static XBOOL AddListDirEntry(const VxDirectoryEntry *entry, void *userData)
{
    CKDataArray *array = (CKDataArray *)userData;
    if (!array || !entry)
        return FALSE;
    array->InsertRow(-1);
    int row = array->GetRowCount() - 1;
    if (!array->SetElementStringValue(row, 0, entry->Name.CStr()))
        array->RemoveRow(row);
    return TRUE;
}

CKObjectDeclaration *FillBehaviorListDirDecl()
{
    CKObjectDeclaration *od = CreateCKObjectDeclaration("TT_ListDir");
    od->SetDescription("Checks for existing of a File");
    od->SetCategory("TT Toolbox/File");
    od->SetType(CKDLL_BEHAVIORPROTOTYPE);
    od->SetGuid(CKGUID(0x496f429e, 0x7d602e9d));
    od->SetAuthorGuid(TERRATOOLS_GUID);
    od->SetAuthorName("Terratools");
    od->SetVersion(0x00010000);
    od->SetCreationFunction(CreateListDirProto);
    od->SetCompatibleClassId(CKCID_BEOBJECT);
    return od;
}

CKERROR CreateListDirProto(CKBehaviorPrototype **pproto)
{
    CKBehaviorPrototype *proto = CreateCKBehaviorPrototype("TT_ListDir");
    if (!proto) return CKERR_OUTOFMEMORY;

    proto->DeclareInput("IN");

    proto->DeclareOutput("OUT");

    proto->DeclareInParameter("SearchPath", CKPGUID_STRING);
    proto->DeclareInParameter("Array", CKPGUID_DATAARRAY);

    proto->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);
    proto->SetFunction(ListDir);

    *pproto = proto;
    return CK_OK;
}

int ListDir(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;
    CKContext *ctx = behcontext.Context;

    CKSTRING searchPath = (CKSTRING)beh->GetInputParameterReadDataPtr(0);
    CKDataArray *array = (CKDataArray *)beh->GetInputParameterObject(1);

    if (!searchPath || !array)
    {
        ctx->OutputToConsole("Invalid file handle", TRUE);
        return CKBR_GENERICERROR;
    }

    const char *slash = strrchr(searchPath, '/');
    const char *backslash = strrchr(searchPath, '\\');
    const char *separator = slash;
    if (!separator || (backslash && backslash > separator))
        separator = backslash;

    XString dir;
    XString mask;
    if (separator) {
        dir = XString(searchPath, (int)(separator - searchPath));
        mask = separator + 1;
    } else {
        dir = ".";
        mask = searchPath;
    }

    if (!VxListDirectory(dir.CStr(), mask.CStr(), TRUE, AddListDirEntry, array))
        return CKBR_GENERICERROR;

    return CKBR_OK;
}
