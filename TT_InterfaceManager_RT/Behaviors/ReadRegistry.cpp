/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
//
//		          TT Read Registry
//
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
#include "CKAll.h"
#include "InterfaceManager.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "RegistryUtils.h"

#ifndef _MSC_VER
#define _snprintf snprintf
#endif

CKObjectDeclaration *FillBehaviorReadRegistryDecl();
CKERROR CreateReadRegistryProto(CKBehaviorPrototype **pproto);
int ReadRegistry(const CKBehaviorContext &behcontext);
CKERROR ReadRegistryCallBack(const CKBehaviorContext &behcontext);

static CKBOOL ReadIntegerFromRegistry(const char *subKey, CKContext *context, const char *valueName, int *value);
static CKBOOL ReadFloatFromRegistry(const char *subKey, CKContext *context, const char *valueName, float *value);
static CKBOOL ReadStringFromRegistry(const char *subKey, CKContext *context, const char *valueName, char *buffer, size_t bufferSize);

CKObjectDeclaration *FillBehaviorReadRegistryDecl()
{
    CKObjectDeclaration *od = CreateCKObjectDeclaration("TT_ReadRegistry");
    od->SetDescription("Reads a value from the registry");
    od->SetCategory("TT InterfaceManager/Registry");
    od->SetType(CKDLL_BEHAVIORPROTOTYPE);
    od->SetGuid(CKGUID(0x460044b5, 0x6e927b66));
    od->SetAuthorGuid(TERRATOOLS_GUID);
    od->SetAuthorName("Terratools");
    od->SetVersion(0x00010000);
    od->SetCreationFunction(CreateReadRegistryProto);
    od->SetCompatibleClassId(CKCID_BEOBJECT);
    return od;
}

CKERROR CreateReadRegistryProto(CKBehaviorPrototype **pproto)
{
    CKBehaviorPrototype *proto = CreateCKBehaviorPrototype("TT_ReadRegistry");
    if (!proto) return CKERR_OUTOFMEMORY;

    proto->DeclareInput("In");

    proto->DeclareOutput("OK");
    proto->DeclareOutput("FAILED");

    proto->DeclareInParameter("Reg-Section  ...\\..\\", CKPGUID_STRING);
    proto->DeclareInParameter("Reg-Entry", CKPGUID_STRING);
    proto->DeclareInParameter("Destination-Array", CKPGUID_DATAARRAY);
    proto->DeclareInParameter("Array to load", CKPGUID_STRING);

    proto->DeclareOutParameter("Data", CKPGUID_INT);

    proto->DeclareSetting("SaveArray-Mode", CKPGUID_BOOL);

    proto->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);
    proto->SetFunction(ReadRegistry);

    proto->SetBehaviorFlags(CKBEHAVIOR_VARIABLEPARAMETEROUTPUTS);
    proto->SetBehaviorCallbackFct(ReadRegistryCallBack);

    *pproto = proto;
    return CK_OK;
}

int ReadRegistry(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;
    CKContext *context = behcontext.Context;

    CKBOOL saveArrayMode = FALSE;
    beh->GetLocalParameterValue(0, &saveArrayMode);

    char regSection[200] = {0};
    char regEntry[64] = {0};
    char arrayToLoad[64] = {0};
    beh->GetInputParameterValue(0, regSection);

    if (saveArrayMode)
        beh->GetInputParameterValue(3, arrayToLoad);
    else
        beh->GetInputParameterValue(1, regEntry);

    if (saveArrayMode)
    {
        CKDataArray *array = (CKDataArray *)beh->GetInputParameterObject(2);
        if (!array)
        {
            context->OutputToConsoleExBeep("TT_ReadRegistry: Destination array is NULL");
            beh->ActivateOutput(1);
            return CKBR_OK;
        }
        int cols = array->GetColumnCount();
        int rows = array->GetRowCount();
        const char *arrayName = (arrayToLoad[0] != '\0') ? arrayToLoad : array->GetName();
        if (!arrayName || arrayName[0] == '\0')
        {
            context->OutputToConsoleExBeep("TT_ReadRegistry: Array name is empty");
            beh->ActivateOutput(1);
            return CKBR_OK;
        }
        char baseSection[200] = {0};
        const int baseLen = _snprintf(baseSection, sizeof(baseSection), "%s\\%s\\", regSection, arrayName);
        if (baseLen < 0 || baseLen >= (int)sizeof(baseSection))
        {
            context->OutputToConsoleExBeep("TT_ReadRegistry: Registry path is too long");
            beh->ActivateOutput(1);
            return CKBR_OK;
        }

        if (cols > 0)
        {
            for (int c = 0; c < cols; ++c)
            {
                const int sectionLen = _snprintf(regSection, sizeof(regSection), "%s%d\\", baseSection, c);
                if (sectionLen < 0 || sectionLen >= (int)sizeof(regSection))
                {
                    context->OutputToConsoleExBeep("TT_ReadRegistry: Registry path is too long");
                    beh->ActivateOutput(1);
                    return CKBR_OK;
                }

                CK_ARRAYTYPE type = array->GetColumnType(c);
                for (int i = 0; i < rows; ++i)
                {
                    switch (type)
                    {
                    case CKARRAYTYPE_INT:
                    {
                        const int entryLen = _snprintf(regEntry, sizeof(regEntry), "%d", i);
                        if (entryLen < 0 || entryLen >= (int)sizeof(regEntry))
                        {
                            context->OutputToConsoleExBeep("TT_ReadRegistry: Registry entry is too long");
                            beh->ActivateOutput(1);
                            return CKBR_OK;
                        }
                        int val = 0;
                        if (!ReadIntegerFromRegistry(regSection, context, regEntry, &val))
                        {
                            beh->ActivateOutput(1);
                            return CKBR_OK;
                        }
                        array->SetElementValue(i, c, &val, sizeof(val));
                    }
                    break;

                    case CKARRAYTYPE_FLOAT:
                    {
                        const int entryLen = _snprintf(regEntry, sizeof(regEntry), "%d", i);
                        if (entryLen < 0 || entryLen >= (int)sizeof(regEntry))
                        {
                            context->OutputToConsoleExBeep("TT_ReadRegistry: Registry entry is too long");
                            beh->ActivateOutput(1);
                            return CKBR_OK;
                        }
                        float val = 0.0f;
                        if (!ReadFloatFromRegistry(regSection, context, regEntry, &val))
                        {
                            beh->ActivateOutput(1);
                            return CKBR_OK;
                        }
                        array->SetElementValue(i, c, &val, sizeof(val));
                    }
                    break;

                    case CKARRAYTYPE_PARAMETER:
                    {
                        CKParameter *parameter = *(CKParameter **)array->GetElement(i, c);
                        if (parameter->GetGUID() != CKPGUID_BOOL)
                        {
                            context->OutputToConsoleExBeep("TT_ReadRegistry: ArrayColumnType invalid(use string/bool/int/float)");
                            beh->ActivateOutput(1);
                            return CKBR_OK;
                        }
                        const int entryLen = _snprintf(regEntry, sizeof(regEntry), "%d", i);
                        if (entryLen < 0 || entryLen >= (int)sizeof(regEntry))
                        {
                            context->OutputToConsoleExBeep("TT_ReadRegistry: Registry entry is too long");
                            beh->ActivateOutput(1);
                            return CKBR_OK;
                        }
                        int val = 0;
                        if (!ReadIntegerFromRegistry(regSection, context, regEntry, &val))
                        {
                            beh->ActivateOutput(1);
                            return CKBR_OK;
                        }
                        array->SetElementValue(i, c, &val, sizeof(val));
                    }
                    break;
                    case CKARRAYTYPE_STRING:
                    {
                        const int entryLen = _snprintf(regEntry, sizeof(regEntry), "%d", i);
                        if (entryLen < 0 || entryLen >= (int)sizeof(regEntry))
                        {
                            context->OutputToConsoleExBeep("TT_ReadRegistry: Registry entry is too long");
                            beh->ActivateOutput(1);
                            return CKBR_OK;
                        }
                        char str[256] = {0};
                        if (!ReadStringFromRegistry(regSection, context, regEntry, str, sizeof(str)))
                        {
                            beh->ActivateOutput(1);
                            return CKBR_OK;
                        }
                        array->SetElementStringValue(i, c, str);
                    }
                    break;
                    default:
                        break;
                    }
                }
            }
        }
    }
    else
    {
        CKGUID guid = beh->GetOutputParameter(0)->GetGUID();
        if (guid == CKPGUID_INT || guid == CKPGUID_BOOL)
        {
            int val = 0;
            if (!ReadIntegerFromRegistry(regSection, context, regEntry, &val))
            {
                beh->ActivateOutput(1);
                return CKBR_OK;
            }
            beh->SetOutputParameterValue(0, &val);
        }
        else if (guid == CKPGUID_FLOAT)
        {
            float val = 0.0f;
            if (!ReadFloatFromRegistry(regSection, context, regEntry, &val))
            {
                beh->ActivateOutput(1);
                return CKBR_OK;
            }
            beh->SetOutputParameterValue(0, &val);
        }
        else if (guid == CKPGUID_STRING)
        {
            char str[256] = {0};
            if (!ReadStringFromRegistry(regSection, context, regEntry, str, sizeof(str)))
            {
                beh->ActivateOutput(1);
                return CKBR_OK;
            }
            beh->SetOutputParameterValue(0, str, (int)strlen(str) + 1);
        }
    }

    beh->ActivateOutput(0);
    return CKBR_OK;
}

CKERROR ReadRegistryCallBack(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;
    CKContext *context = behcontext.Context;

    char buffer[16];

    switch (behcontext.CallbackMessage)
    {
    case CKM_BEHAVIORATTACH:
    case CKM_BEHAVIORLOAD:
        break;
    case CKM_BEHAVIOREDITED:
    case CKM_BEHAVIORSETTINGSEDITED:
    {
        CKParameterOut *data = beh->GetOutputParameter(0);
        if (!data)
        {
            snprintf(buffer, sizeof(buffer), "Data");
            data = beh->CreateOutputParameter(buffer, CKPGUID_INT);
        }

        if (data)
        {
            CKGUID guid = data->GetGUID();
            if (guid != CKPGUID_INT &&
                guid != CKPGUID_FLOAT &&
                guid != CKPGUID_BOOL &&
                guid != CKPGUID_STRING)
            {
                snprintf(buffer, sizeof(buffer), "Data");
                context->DestroyObject(data);
                data = beh->CreateOutputParameter(buffer, CKPGUID_INT);
                if (data)
                {
                    int zero = 0;
                    data->SetValue(&zero, sizeof(zero));
                }
                context->OutputToConsoleExBeep("TT_ReadRegistry: ArrayColumnType invalid(use string/bool/int/float)");
            }
        }
    }
    default:
        break;
    }

    CKBOOL saveArrayMode = FALSE;
    beh->GetLocalParameterValue(0, &saveArrayMode);
    if (saveArrayMode)
    {
        beh->EnableInputParameter(1, FALSE);
        beh->EnableInputParameter(2, TRUE);
        beh->EnableInputParameter(3, TRUE);
        beh->EnableOutputParameter(0, FALSE);
    }
    else
    {
        beh->EnableInputParameter(1, TRUE);
        beh->EnableInputParameter(2, FALSE);
        beh->EnableInputParameter(3, FALSE);
        beh->EnableOutputParameter(0, TRUE);
    }

    return CKBR_OK;
}

static CKBOOL ReadIntegerFromRegistry(const char *subKey, CKContext *context, const char *valueName, int *value)
{
    if (!value)
        return FALSE;

    if (!TTReadRegistryInteger(VXCONFIG_ROOT_CURRENT_USER, subKey, valueName, value))
    {
        context->OutputToConsoleExBeep("TT_ReadRegistry:  ReadError: %s.", valueName);
        return FALSE;
    }
    return TRUE;
}

static CKBOOL ReadFloatFromRegistry(const char *subKey, CKContext *context, const char *valueName, float *value)
{
    if (!value)
        return FALSE;

    if (!TTReadRegistryFloat(VXCONFIG_ROOT_CURRENT_USER, subKey, valueName, value))
    {
        context->OutputToConsoleExBeep("TT_ReadRegistry:  ReadError: %s.", valueName);
        return FALSE;
    }
    return TRUE;
}

static CKBOOL ReadStringFromRegistry(const char *subKey, CKContext *context, const char *valueName, char *buffer, size_t bufferSize)
{
    if (!buffer || bufferSize == 0)
        return FALSE;

    buffer[0] = '\0';

    if (!TTReadRegistryString(VXCONFIG_ROOT_CURRENT_USER, subKey, valueName, buffer, bufferSize))
    {
        context->OutputToConsoleExBeep("TT_ReadRegistry:  ReadError: %s.", valueName);
        return FALSE;
    }

    buffer[bufferSize - 1] = '\0';
    return TRUE;
}
