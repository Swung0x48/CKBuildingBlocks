////////////////////////////////////////
////////////////////////////////////////
//
//        TT_LoadMotorSettings
//
////////////////////////////////////////
////////////////////////////////////////
#include "CKAll.h"
#include "ToolboxGuids.h"
#include "MotorSettings.h"

#include <stdio.h>

CKObjectDeclaration *FillBehaviorLoadMotorSettingsDecl();
CKERROR CreateLoadMotorSettingsProto(CKBehaviorPrototype **pproto);
int LoadMotorSettings(const CKBehaviorContext &behcontext);

CKObjectDeclaration *FillBehaviorLoadMotorSettingsDecl()
{
    CKObjectDeclaration *od = CreateCKObjectDeclaration("TT_LoadMotorSettings");
    od->SetDescription("Loads sound settings for TT_MotorSound behavior");
    od->SetCategory("TT Toolbox/Sounds");
    od->SetType(CKDLL_BEHAVIORPROTOTYPE);
    od->SetGuid(CKGUID(0xf822838, 0x74116583));
    od->SetAuthorGuid(TERRATOOLS_GUID);
    od->SetAuthorName("Terratools");
    od->SetVersion(0x00010000);
    od->SetCreationFunction(CreateLoadMotorSettingsProto);
    od->SetCompatibleClassId(CKCID_BEOBJECT);
    od->NeedManager(SOUND_MANAGER_GUID);
    return od;
}

CKERROR CreateLoadMotorSettingsProto(CKBehaviorPrototype **pproto)
{
    CKBehaviorPrototype *proto = CreateCKBehaviorPrototype("TT_LoadMotorSettings");
    if (!proto) return CKERR_OUTOFMEMORY;

    proto->DeclareInput("Start");

    proto->DeclareOutput("Loading finished");
    proto->DeclareOutput("Error");

    proto->DeclareInParameter("SettingsFile", CKPGUID_STRING);

    proto->DeclareLocalParameter("FirstTime", CKPGUID_BOOL, "TRUE");

    proto->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);
    proto->SetFunction(LoadMotorSettings);

    *pproto = proto;
    return CK_OK;
}

int LoadMotorSettings(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;
    CKContext *ctx = behcontext.Context;

    beh->ActivateInput(0, FALSE);

    // Get the settings file name
    char fileName[240];
    beh->GetInputParameterValue(0, fileName);

    FILE *file = fopen(fileName, "rt");
    if (file)
    {
        float motorVolume = 0.0f;
        MotorChannelSettings channel1 = {0};
        MotorChannelSettings channel2 = {0};
        MotorChannelSettings channel3 = {0};

        // Read volume
        const int volumeFields = fscanf(file, "Volume:%f\n", &motorVolume);

        // Read channel 1 settings (low speed)
        const int channel1Fields = fscanf(file, "PitchMax:%f PitchMin:%f vMax:%f vMin:%f vFadeIn:%f vFadeOut:%f\n",
                                          &channel1.pitchMax,
                                          &channel1.pitchMin,
                                          &channel1.vMax,
                                          &channel1.vMin,
                                          &channel1.vFadeIn,
                                          &channel1.vFadeOut);

        // Read channel 2 settings (mid speed)
        const int channel2Fields = fscanf(file, "PitchMax:%f PitchMin:%f vMax:%f vMin:%f vFadeIn:%f vFadeOut:%f\n",
                                          &channel2.pitchMax,
                                          &channel2.pitchMin,
                                          &channel2.vMax,
                                          &channel2.vMin,
                                          &channel2.vFadeIn,
                                          &channel2.vFadeOut);

        // Read channel 3 settings (high speed)
        const int channel3Fields = fscanf(file, "PitchMax:%f PitchMin:%f vMax:%f vMin:%f vFadeIn:%f vFadeOut:%f\n",
                                          &channel3.pitchMax,
                                          &channel3.pitchMin,
                                          &channel3.vMax,
                                          &channel3.vMin,
                                          &channel3.vFadeIn,
                                          &channel3.vFadeOut);

        fclose(file);
        if (volumeFields == 1 && channel1Fields == 6 && channel2Fields == 6 && channel3Fields == 6)
        {
            g_MotorVolume = motorVolume;
            g_MotorChannel1 = channel1;
            g_MotorChannel2 = channel2;
            g_MotorChannel3 = channel3;
            beh->ActivateOutput(0, TRUE);
        }
        else
        {
            ctx->OutputToConsoleExBeep("Motor settings file is malformed!");
            beh->ActivateOutput(1, TRUE);
        }
    }
    else
    {
        // File not found - show error message
        ctx->OutputToConsoleExBeep("File Motor.txt nicht gefunden!");
        beh->ActivateOutput(1, TRUE);
    }

    return CKBR_OK;
}
