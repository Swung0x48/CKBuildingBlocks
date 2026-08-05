/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
//
//		         UnlimitedController
//
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
#include "UnlimitedParams.h"

CKERROR CreateUnlimitedControllerBehaviorProto(CKBehaviorPrototype **pproto);
int DoUnlimitedController(const CKBehaviorContext &behcontext);
void CheckAnimations(CKContext *ctx, int count, UnlimitedParam *Params, const XObjectArray &array, CKCharacter *owner);
CKERROR DaUCharacterCB(const CKBehaviorContext &behcontext);

#define NOPARAM_VERSION 0x00010000
#define CURRENT_VERSION 0x00020000

CKObjectDeclaration *FillBehaviorUnlimitedControllerDecl()
{
    CKObjectDeclaration *od = CreateCKObjectDeclaration("Unlimited Controller");
    od->SetDescription("Controls a character by using animations played in response to messages.");
    /* rem:
    The only way to parametrize this building block, is by editing it once it has been applied to the character.<BR>
    You can also right click on any parameter in the dialog box to create an input parameter so it can parametrized at runtime.<BR>
    The 'Unlimited Controller' allows you to define animations for the whole character (Primary Animations) and specific body-part animations or exceptional animations (Secondary Animations)<BR>
    For each couple (message,animation) parameters can be set to specify how animations should be played (use of motion warping for transitions,length of the warp sequence, time base for animation [frame or real time] and a priority to specify the order in which messages will be tested)<BR>
    <BR>
    <FONT SIZE=2>Primary Animations ...</FONT><BR>
    <FONT COLOR=#ffffff>- Order: </FONT>the higher the order are, the lower its priority will be. You should therefore, put the animations like standing with a high order, while shot animations, or jump animations should have a lowest order (because when these animations are played, they must stops the others.<BR>
    <FONT COLOR=#ffffff>- Message: </FONT>the waited message which start playing the associated animation.<BR>
    <FONT COLOR=#ffffff>- Animation: </FONT>the character animation played when the character received the associated message.<BR>
    <FONT COLOR=#ffffff>- Warp [None/Start/Best]: </FONT>specifies whether you want to have a transition animation with the next one "None" breaks the current animation and start the new one instantly, "Start" creates a transition to the start of the next animation and "Best" find the best frame to which begin the next animation (i.e next animation will not automatically be  played from start).<BR>
    <FONT COLOR=#ffffff>- Warp length: </FONT>duration of the warping animation part (transition part) according to the 'TimeBase' and the 'Fps'.<BR>
    Say 'Warp length'=5 and 'Fps'=30.0<BR>
    If 'TimeBase'=Frame, then the warping animation will take 5 frames (5 rendering process), and the 'Fps' value will not change anything.<BR>
    If 'TimeBase'=Time, then the warping animation will take 5/30 seconds (independent of the frame rate).<BR>
    <FONT COLOR=#ffffff>- Stopable [Yes/No]: </FONT>specifies if the animation can be stoped instantly (on request), or if the character will have to complete this animation to start an other (eg: typically a jump animation shouldn't be stopable).<BR>
    <FONT COLOR=#ffffff>- Time Base [Time/Frame]: </FONT>specifies whether the base of time used to interpret duration will be proportionnal to 'Frames' or not (if not it use second [Time]). See 'Warp length'.<BR>
    <FONT COLOR=#ffffff>- Fps: </FONT>number of animation frames to be played per seconds. This value will not be taken into concideration if the 'TimeBase'=Frame, because therefore, one animation frame will played for each rendering process.<BR>
    <FONT COLOR=#ffffff>- Turn [Yes/No]: </FONT>specifies whether the character can be rotated (whith the 'Joy_Left' and 'Joy_Right' messages) while playing the raw-referenced animation, or not (eg: typically a jump animation shouldn't be orientable interactively).<BR>
    <FONT COLOR=#ffffff>- Orient [Yes/No]: </FONT>specifies whether the animation should define a new character's orientation, or not (typically, set this values to 'Yes' if you the character has a Rotate Left animation, and if you want him to keep this orientation later on).<BR>
    <FONT COLOR=#ffffff>- Description: </FONT>description field to define the animation.<BR>
    <BR>
    <FONT SIZE=2>Secondary Animations ...</FONT><BR>
    <FONT COLOR=#ffffff>- Message: </FONT>the waited message which starts or stops playing the associated body-part animation.<BR>
    <FONT COLOR=#ffffff>- Action [Play/Stop]: </FONT>specifies whether the message will start or stop the animation.<BR>
    <FONT COLOR=#ffffff>- Animation: </FONT>the body-part animation that has to be started or stoped when the character received the associated message.<BR>
    <FONT COLOR=#ffffff>- Time Base [Time/Frame]: </FONT>ref. just above.<BR>
    <FONT COLOR=#ffffff>- Fps: </FONT>ref. just above.<BR>
    <FONT COLOR=#ffffff>- Loop [Yes/No]: </FONT>specifies whether this animation has to be looped or not (to stop a looping animation, you'll need to send the appropriate stoping message).<BR>
    <FONT COLOR=#ffffff>- Start Frame: </FONT>starting animation frame.<BR>
    <FONT COLOR=#ffffff>- Description: </FONT>description field to define the animation.<BR>
    <BR>
    <FONT SIZE=2>Ending Parameters ...</FONT><BR>
    <FONT COLOR=#ffffff>- Keep On Floor: </FONT>if checked, the character will stand on floor, otherwise it will be in the air.<BR>
    <FONT COLOR=#ffffff>- Rotation Angle: proportionnal value to determine the speed of the character rotation (when receiving the 'Joy_Left' and 'Joy_Right' messages)</FONT>.<BR>
    */
    od->SetCategory("Characters/Movement");
    od->SetType(CKDLL_BEHAVIORPROTOTYPE);
    od->SetGuid(CKGUID(0x59537e89, 0x79d02f25));
    od->SetAuthorGuid(CKGUID());
    od->SetAuthorName("Virtools");
    od->SetVersion(0x00020000);
    od->SetCreationFunction(CreateUnlimitedControllerBehaviorProto);
    od->SetCompatibleClassId(CKCID_CHARACTER);
    od->NeedManager(FLOOR_MANAGER_GUID);
    return od;
}

CKERROR CreateUnlimitedControllerBehaviorProto(CKBehaviorPrototype **pproto)
{
    CKBehaviorPrototype *proto = CreateCKBehaviorPrototype("Unlimited Controller");
    if (!proto)
        return CKERR_OUTOFMEMORY;

    proto->DeclareInput("On");
    proto->DeclareInput("Off");

    proto->DeclareOutput("Exit On");
    proto->DeclareOutput("exit Off");

#define UC_ANIMATIONS 0
#define UC_PRIMARYDATA 1
#define UC_NBPRIMARYDATA 2
#define UC_CHECKFLOOR 3
#define UC_ROTATES 4
#define UC_MSGROT 5
#define UC_SECONDARYDATA 6
#define UC_NBSECONDARYDATA 7

    proto->DeclareLocalParameter("Anims", CKPGUID_OBJECTARRAY);
    proto->DeclareLocalParameter("Data", CKPGUID_VOIDBUF);
    proto->DeclareLocalParameter("NbData", CKPGUID_INT, "0");
    proto->DeclareLocalParameter("Floor", CKPGUID_BOOL, "TRUE");
    proto->DeclareLocalParameter("Rotates", CKPGUID_FLOAT, "0.06");
    proto->DeclareLocalParameter("MsgRot", CKPGUID_VOIDBUF);
    proto->DeclareLocalParameter("Data2", CKPGUID_VOIDBUF);
    proto->DeclareLocalParameter("NbData2", CKPGUID_INT, "0");

    proto->DeclareOutParameter("Current Anim", CKPGUID_ANIMATION);
    proto->DeclareOutParameter("Current Msg", CKPGUID_MESSAGE);
    proto->DeclareOutParameter("Current Step (0..1)", CKPGUID_FLOAT);
    proto->DeclareOutParameter("Current Frame", CKPGUID_FLOAT);

    proto->DeclareSetting("Detection Offset", CKPGUID_FLOAT, "0.05");

    proto->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);
    proto->SetBehaviorFlags((CK_BEHAVIOR_FLAGS)(CKBEHAVIOR_MESSAGERECEIVER | CKBEHAVIOR_CUSTOMEDITDIALOG | CKBEHAVIOR_INTERNALLYCREATEDINPUTPARAMS));

    proto->SetFunction(DoUnlimitedController);
    proto->SetBehaviorCallbackFct(DaUCharacterCB);

    *pproto = proto;
    return CK_OK;
}

CKERROR DaUCharacterCB(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;
    CKMessageManager *mm = behcontext.MessageManager;
    CKContext *ctx = behcontext.Context;

    switch (behcontext.CallbackMessage)
    {

    case CKM_BEHAVIORATTACH:
    {
        // Initialization on attach
        UnlimitedParam Params[3];
        memset(Params, 0, sizeof(UnlimitedParam) * 3);
        int count;
        beh->GetLocalParameterValue(UC_NBPRIMARYDATA, &count);

        if (count == 0)
        {
            count = 3;
            strcpy(Params[0].Message, "Joy_Up");
            strcpy(Params[1].Message, "Joy_Down");
            strcpy(Params[2].Message, "");
            strcpy(Params[0].Desc, "Walk Animation");
            strcpy(Params[1].Desc, "Walkback Animation");
            strcpy(Params[2].Desc, "Stand Animation");

            for (int i = 0; i < 3; i++)
            {
                Params[i].IndexAnimation = -1;
                Params[i].Priority = i;
                Params[i].AnimationPtr = NULL;
                Params[i].flags = PARAM_WARP | PARAM_USERIGHTLEFT | PARAM_TIMEBASE | PARAM_LOOP | PARAM_STOPABLE;
                Params[i].fps = 30.0f;
                Params[i].warplength = 5.0f;
                Params[i].Msg = mm->AddMessageType(Params[i].Message);
                Params[i].ParameterMask = 0;
                for (int j = 0; j < 16; ++j)
                    Params[i].ParameterIndexes[j] = -1;
            }
            Params[2].Priority = 128;

            beh->SetLocalParameterValue(UC_PRIMARYDATA, (void *)Params, 3 * sizeof(UnlimitedParam));
            beh->SetLocalParameterValue(UC_NBPRIMARYDATA, &count);
            count = 0;
            beh->SetLocalParameterValue(UC_NBSECONDARYDATA, &count);

            int tab[2];
            tab[0] = mm->AddMessageType("Joy_Left");
            tab[1] = mm->AddMessageType("Joy_Right");
            beh->SetLocalParameterValue(UC_MSGROT, (void *)tab, 2 * sizeof(int));
        }
        else
        {
            // It may be a copy of unlimitted controller => we need to update the animations ptr
            // from the Objectarray
            XObjectArray *array = NULL;
            beh->GetLocalParameterValue(UC_ANIMATIONS, &array);
            int count = 0;

            UnlimitedParam *params = (UnlimitedParam *)beh->GetLocalParameterReadDataPtr(UC_PRIMARYDATA);
            beh->GetLocalParameterValue(UC_NBPRIMARYDATA, &count);
            int i;
            for (i = 0; i < count; ++i)
            {
                if (!(params[i].ParameterMask & PARAM_ANIMATION_MASK) || (params[i].ParameterIndexes[PARAM_ANIMATION_INDEX] < 0))
                { // use Animation index
                    params[i].AnimationPtr = NULL;
                }
            }

            params = (UnlimitedParam *)beh->GetLocalParameterReadDataPtr(UC_SECONDARYDATA);
            beh->GetLocalParameterValue(UC_NBSECONDARYDATA, &count);
            for (i = 0; i < count; ++i)
            {
                if (!(params[i].ParameterMask & PARAM_ANIMATION_MASK) || (params[i].ParameterIndexes[PARAM_ANIMATION_INDEX] < 0))
                { // use Animation index
                    params[i].AnimationPtr = NULL;
                }
            }
        }

        CKCharacter *carac = (CKCharacter *)beh->GetOwner();
        if (!carac)
            return CKBR_OWNERERROR;
        carac->SetAsWaitingForMessages();
        return CKBR_OK;
    }
    break;

    case CKM_BEHAVIORREADSTATE:
    case CKM_BEHAVIORRESET:
    case CKM_BEHAVIORLOAD:
    {
        CKCharacter *carac = (CKCharacter *)beh->GetOwner();
        if (!carac)
            return CKBR_OWNERERROR;

        //---------- Restore Messages
        int tab[2];
        tab[0] = mm->AddMessageType("Joy_Left");
        tab[1] = mm->AddMessageType("Joy_Right");
        beh->SetLocalParameterValue(UC_MSGROT, (void *)tab, 2 * sizeof(int));

        int count;
        UnlimitedParam *params = NULL;

        XObjectArray *array = NULL;
        beh->GetLocalParameterValue(UC_ANIMATIONS, &array);

        //------------ Primary Animations
        params = (UnlimitedParam *)beh->GetLocalParameterReadDataPtr(UC_PRIMARYDATA);

        beh->GetLocalParameterValue(UC_NBPRIMARYDATA, &count);
        if (params)
        {
            for (int i = 0; i < count; i++)
            {

                if (CKM_BEHAVIORLOAD == behcontext.CallbackMessage)
                {
                    // Do Swap For Mac
#if !defined(WIN32) && !defined(_WIN32)
                    ENDIANSWAP32(params[i].IndexAnimation);
                    ENDIANSWAP32(params[i].StartFrame);
                    ENDIANSWAP32(params[i].Priority);

                    ENDIANSWAP16(params[i].Align);
                    ENDIANSWAP16(params[i].ParameterMask);

                    ENDIANSWAP32(params[i].Msg);
                    ENDIANSWAPFLOAT(params[i].fps);
                    ENDIANSWAPFLOAT(params[i].warplength);

                    ENDIANSWAP16(params[i].flags);
                    ENDIANSWAP16(params[i].LoopCount);
#endif
                }

                params[i].AnimationPtr = NULL;
                params[i].Msg = mm->AddMessageType(params[i].Message);
                if (beh->GetVersion() <= NOPARAM_VERSION)
                { // Old version invalidate parameter indexes
                    for (int j = 0; j < 16; ++j)
                        params[i].ParameterIndexes[j] = -1;
                    params[i].ParameterMask = 0;
                }
                if (!(params[i].ParameterMask & PARAM_ANIMATION_MASK)) {
                    if (params[i].IndexAnimation >= 0)
                    {
                        params[i].AnimationPtr = (CKAnimation *)array->GetObject(ctx, params[i].IndexAnimation);
                    }
                    else
                    {
                        params[i].AnimationPtr = NULL;
                    }
                }
            }
        }

        //----------- Secondary Animations
        params = (UnlimitedParam *)beh->GetLocalParameterReadDataPtr(UC_SECONDARYDATA);
        beh->GetLocalParameterValue(UC_NBSECONDARYDATA, &count);

        if (params)
        {
            for (int i = 0; i < count; i++)
            {

                if (CKM_BEHAVIORLOAD == behcontext.CallbackMessage)
                {
                    // Do Swap For Mac
#if !defined(WIN32) && !defined(_WIN32)
                    ENDIANSWAP32(params[i].IndexAnimation);
                    ENDIANSWAP32(params[i].StartFrame);
                    ENDIANSWAP32(params[i].Priority);

                    ENDIANSWAP16(params[i].Align);
                    ENDIANSWAP16(params[i].ParameterMask);

                    ENDIANSWAP32(params[i].Msg);
                    ENDIANSWAPFLOAT(params[i].fps);
                    ENDIANSWAPFLOAT(params[i].warplength);

                    ENDIANSWAP16(params[i].flags);
                    ENDIANSWAP16(params[i].LoopCount);
#endif
                }

                params[i].AnimationPtr = NULL;
                params[i].Msg = mm->AddMessageType(params[i].Message);
                if (beh->GetVersion() <= NOPARAM_VERSION)
                { // Old version invalidate parameter indexes
                    for (int j = 0; j < 16; ++j)
                        params[i].ParameterIndexes[j] = -1;
                    params[i].ParameterMask = 0;
                }
                if (!(params[i].ParameterMask & PARAM_ANIMATION_MASK)) {
                    if (params[i].IndexAnimation >= 0)
                    {
                        params[i].AnimationPtr = (CKAnimation *)array->GetObject(ctx, params[i].IndexAnimation);
                    }
                    else
                    {
                        params[i].AnimationPtr = NULL;
                    }
                }
            }
        }
        if (beh->GetVersion() <= NOPARAM_VERSION) // Old version invalidate parameter indexes
            beh->SetVersion(CURRENT_VERSION);	  // reset to current version

        carac->SetAsWaitingForMessages();
        return CKBR_OK;
    }
    break;

    case CKM_BEHAVIOREDITED:
    {

        XObjectArray *array = NULL;
        beh->GetLocalParameterValue(UC_ANIMATIONS, &array);
        int count = 0;

        UnlimitedParam *params = (UnlimitedParam *)beh->GetLocalParameterReadDataPtr(UC_PRIMARYDATA);
        beh->GetLocalParameterValue(UC_NBPRIMARYDATA, &count);
        CheckAnimations(ctx, count, params, *array, (CKCharacter *)beh->GetOwner());

        params = (UnlimitedParam *)beh->GetLocalParameterReadDataPtr(UC_SECONDARYDATA);
        beh->GetLocalParameterValue(UC_NBSECONDARYDATA, &count);
        CheckAnimations(ctx, count, params, *array, (CKCharacter *)beh->GetOwner());

        CKInterfaceManager *UIMan = (CKInterfaceManager *)behcontext.Context->GetManagerByGuid(INTERFACE_MANAGER_GUID);
        if (UIMan)
            UIMan->CallBehaviorEditionFunction(beh, 0);
    }
    break;

    case CKM_BEHAVIORDETACH:
    {
        CKCharacter *carac = (CKCharacter *)beh->GetOwner();
        if (!carac)
            return CKBR_OWNERERROR;

        carac->SetAsWaitingForMessages(FALSE);
        carac->SetAutomaticProcess(FALSE);
        return CKBR_OK;
    }
    break;
    }
    return CKBR_OK;
}

int DoUnlimitedController(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;

    if (beh->IsInputActive(1))
    {
        CKCharacter *carac = (CKCharacter *)beh->GetOwner();
        beh->ActivateInput(1, FALSE);
        beh->ActivateOutput(1, TRUE);
        carac->SetAutomaticProcess(FALSE);
        return CKBR_OK;
    }

    if (beh->IsInputActive(0))
    {
        beh->ActivateInput(0, FALSE);
        beh->ActivateOutput(0, TRUE);
    }

    CKContext *ctx = behcontext.Context;
    CKCharacter *carac = (CKCharacter *)beh->GetOwner();
    CKLevel *level = behcontext.CurrentLevel;
    CKMessageManager *mmanager = behcontext.MessageManager;
    CKFloorManager *fmanager = (CKFloorManager *)ctx->GetManagerByGuid(FLOOR_MANAGER_GUID);

    XObjectArray *array = NULL;
    int i, count, count2;
    UnlimitedParam *Params = NULL;
    UnlimitedParam *Params2 = NULL;
    CKBOOL OnFloor, Rotates = FALSE;
    float RotateAngle;
    int RotateMsg[2];
    CKAnimation *anim;

    //---------------------------------------------------------------------------------
    //------------- Checking Parameters -----------------------------------------------
    //---------------------------------------------------------------------------------
    beh->GetLocalParameterValue(UC_ANIMATIONS, &array);
    beh->GetLocalParameterValue(UC_NBPRIMARYDATA, &count);
    beh->GetLocalParameterValue(UC_CHECKFLOOR, &OnFloor);
    beh->GetLocalParameterValue(UC_ROTATES, &RotateAngle);
    beh->GetLocalParameterValue(UC_MSGROT, RotateMsg);
    beh->GetLocalParameterValue(UC_NBSECONDARYDATA, &count2);
    Params = (UnlimitedParam *)beh->GetLocalParameterReadDataPtr(UC_PRIMARYDATA);
    Params2 = (UnlimitedParam *)beh->GetLocalParameterReadDataPtr(UC_SECONDARYDATA);

    CheckAnimations(ctx, count, Params, *array, carac);
    CheckAnimations(ctx, count2, Params2, *array, carac);

    carac->SetAutomaticProcess(TRUE);

    //------------- Message Checking
    XBitArray Messages;
    CKMessage *msg = NULL;
    for (i = 0; i < carac->GetLastFrameMessageCount(); i++)
        if ((msg = carac->GetLastFrameMessage(i)))
            Messages.Set(msg->GetMsgType());

    //---------------------------------------------------------------------------------
    //------------- Checking Primary Animations ---------------------------------------
    //---------------------------------------------------------------------------------
    int finded = -1;
    for (i = 0; i < count; i++)
        if (Params[i].Msg >= 0)
            if (Messages[Params[i].Msg])
            {
                finded = i;
                break;
            }
    if (finded < 0)
        for (i = 0; i < count; i++)
            if (Params[i].Msg < 0)
            {
                finded = i;
                break;
            }

    if (finded >= 0)
    {
        int i = finded;

        CKDWORD TransitionFlags;
        switch (Params[i].flags & PARAM_WARPMASK)
        {
        case PARAM_WARP:
            TransitionFlags = CK_TRANSITION_WARPBEST;
            break;
        case PARAM_WARP | PARAM_WARPSTART:
            TransitionFlags = CK_TRANSITION_WARPSTART;
            break;
        case PARAM_WARP | PARAM_WARPSAME:
            TransitionFlags = CK_TRANSITION_WARPSAMEPOS;
            break;
        default:
            TransitionFlags = CK_TRANSITION_BREAK;
            break;
        }
        float WarpLength = Params[i].warplength;
        CKBOOL LinkedToFps = Params[i].flags & PARAM_TIMEBASE;
        float Fps = Params[i].fps;
        CKBOOL Stopeable = Params[i].flags & PARAM_STOPABLE;
        CKBOOL CanTurn = Params[i].flags & PARAM_USERIGHTLEFT;
        CKBOOL TakeOrient = Params[i].flags & PARAM_ORIENTATION;

        //---------- Check for parameters values
        //---------- if user has linked a value to input parameters (context menu)
        if (Params[i].ParameterMask)
        { // At least one input parameter is used

            if (Params[i].ParameterIndexes[PARAM_ANIMATION_INDEX] >= 0)
                anim = (CKAnimation *)beh->GetInputParameterObject(Params[i].ParameterIndexes[PARAM_ANIMATION_INDEX]);
            else
                anim = (CKAnimation *)array->GetObject(ctx, Params[i].IndexAnimation);

            if (anim && (Params[i].ParameterMask & (~PARAM_ANIMATION_MASK)))
            { // specific values are only meaningful if there is an anim to play
                // Warp Flags
                if (Params[i].ParameterMask & PARAM_WARPFLAGS_MASK)
                    beh->GetInputParameterValue(Params[i].ParameterIndexes[PARAM_WARPFLAGS_INDEX], &TransitionFlags);
                // Warp Length
                if (Params[i].ParameterMask & PARAM_WARPLENGTH_MASK)
                    beh->GetInputParameterValue(Params[i].ParameterIndexes[PARAM_WARPLENGTH_INDEX], &WarpLength);
                // TimeBased ?
                if (Params[i].ParameterMask & PARAM_TIME_MASK)
                    beh->GetInputParameterValue(Params[i].ParameterIndexes[PARAM_TIME_INDEX], &LinkedToFps);
                // fps ?
                if (Params[i].ParameterMask & PARAM_FPS_MASK)
                    beh->GetInputParameterValue(Params[i].ParameterIndexes[PARAM_FPS_INDEX], &Fps);
                // Stopeable .
                if (Params[i].ParameterMask & PARAM_STOP_MASK)
                    beh->GetInputParameterValue(Params[i].ParameterIndexes[PARAM_STOP_INDEX], &Stopeable);
                // CanTurn ?
                if (Params[i].ParameterMask & PARAM_TURN_MASK)
                    beh->GetInputParameterValue(Params[i].ParameterIndexes[PARAM_TURN_INDEX], &CanTurn);
                // TakeOrient ?
                if (Params[i].ParameterMask & PARAM_ORIENT_MASK)
                    beh->GetInputParameterValue(Params[i].ParameterIndexes[PARAM_ORIENT_INDEX], &TakeOrient);
            }
        }
        else
            anim = (CKAnimation *)array->GetObject(ctx, Params[i].IndexAnimation);

        if (anim)
        {
            anim->LinkToFrameRate(LinkedToFps, Fps);
            anim->SetCanBeInterrupt(Stopeable);
            CKDWORD anim_flg = anim->GetFlags();
            if (CanTurn)
                anim_flg |= CKANIMATION_ALLOWTURN;
            else
                anim_flg &= ~CKANIMATION_ALLOWTURN;
            if (TakeOrient)
                anim_flg |= CKANIMATION_ALIGNORIENTATION;
            else
                anim_flg &= ~CKANIMATION_ALIGNORIENTATION;
            anim->SetFlags(anim_flg);
            carac->SetNextActiveAnimation(anim, TransitionFlags | CK_TRANSITION_USEVELOCITY | CK_TRANSITION_LOOPIFEQUAL, WarpLength);
        }
        else
            carac->SetNextActiveAnimation(NULL, 0, WarpLength);
        beh->SetOutputParameterValue(1, &Params[i].Msg);
    }

    //---------------------------------------------------------------------------------
    //------------- Checking Secondary Animations -------------------------------------
    //---------------------------------------------------------------------------------
    for (i = 0; i < count2; i++)
        if (Params2[i].Msg >= 0)
            if (Messages[Params2[i].Msg])
            {
                anim = (CKAnimation *)array->GetObject(ctx, Params2[i].IndexAnimation);
                CKDWORD PlayMode = (Params2[i].flags & PARAM_PLAY) ? CKSECONDARYANIMATION_ONESHOT : 0;
                if (Params2[i].flags & PARAM_LOOP)
                    PlayMode = CKSECONDARYANIMATION_LOOP;
                if (Params2[i].flags & PARAM_LOOPN)
                    PlayMode = CKSECONDARYANIMATION_LOOPNTIMES;
                float WarpLength = Params2[i].warplength;
                int LoopCount = Params2[i].LoopCount;
                CKBOOL Warp = Params2[i].flags & PARAM_WARP;
                float StartFrame = Params2[i].StartFrame;
                float Fps = Params2[i].fps;
                CKBOOL LinkedToFps = Params2[i].flags & PARAM_TIMEBASE;
                CKBOOL StayOnLast = Params2[i].flags & PARAM_STAY;

                //---------- Check for parameters values
                if (Params2[i].ParameterMask)
                { // At least one parameter is used

                    if (Params2[i].ParameterIndexes[PARAM_ANIMATION_INDEX] >= 0)
                        anim = (CKAnimation *)beh->GetInputParameterObject(Params2[i].ParameterIndexes[PARAM_ANIMATION_INDEX]);

                    if (anim && (Params2[i].ParameterMask & (~PARAM_ANIMATION_MASK)))
                    { // specific values are only meaningful if there is an anim to play
                        // Play Mode
                        if (Params2[i].ParameterMask & PARAM_SPLAYMODE_MASK)
                            beh->GetInputParameterValue(Params2[i].ParameterIndexes[PARAM_SPLAYMODE_INDEX], &PlayMode);
                        // Warp Length
                        if (Params2[i].ParameterMask & PARAM_WARPLENGTH_MASK)
                            beh->GetInputParameterValue(Params2[i].ParameterIndexes[PARAM_WARPLENGTH_INDEX], &WarpLength);
                        // TimeBased ?
                        if (Params2[i].ParameterMask & PARAM_TIME_MASK)
                            beh->GetInputParameterValue(Params2[i].ParameterIndexes[PARAM_TIME_INDEX], &LinkedToFps);
                        // fps ?
                        if (Params2[i].ParameterMask & PARAM_FPS_MASK)
                            beh->GetInputParameterValue(Params2[i].ParameterIndexes[PARAM_FPS_INDEX], &Fps);
                        // StayOnLast .
                        if (Params2[i].ParameterMask & PARAM_STAYONLAST_MASK)
                            beh->GetInputParameterValue(Params2[i].ParameterIndexes[PARAM_STAYONLAST_INDEX], &StayOnLast);
                        // Start Frame
                        if (Params2[i].ParameterMask & PARAM_STARTFRAME_MASK)
                            beh->GetInputParameterValue(Params2[i].ParameterIndexes[PARAM_STARTFRAME_INDEX], &StartFrame);
                        // Warp ?
                        if (Params2[i].ParameterMask & PARAM_WARPFLAGS_MASK)
                            beh->GetInputParameterValue(Params2[i].ParameterIndexes[PARAM_WARPFLAGS_INDEX], &Warp);
                        // LoopCount ?
                        if (Params2[i].ParameterMask & PARAM_LOOPCOUNT_MASK)
                            beh->GetInputParameterValue(Params2[i].ParameterIndexes[PARAM_LOOPCOUNT_INDEX], &LoopCount);
                    }
                }

                if (anim)
                {
                    if (PlayMode)
                    {
                        anim->LinkToFrameRate(LinkedToFps, Fps);
                        if (StayOnLast)
                            PlayMode |= CKSECONDARYANIMATION_LASTFRAME;
                        if (Warp)
                            PlayMode |= CKSECONDARYANIMATION_DOWARP;
                        carac->PlaySecondaryAnimation(anim, StartFrame, (CK_SECONDARYANIMATION_FLAGS)PlayMode, WarpLength, LoopCount);
                    }
                    else
                        carac->StopSecondaryAnimation(anim, Warp, WarpLength);
                }
            }

    //------------- Rotate Management
    if ((anim = carac->GetActiveAnimation()))
        Rotates = (anim->GetFlags() & CKANIMATION_ALLOWTURN);
    if (Rotates)
    {
        VxVector up(0, 1.0f, 0);
        float m_FrameRate = anim ? anim->GetLinkedFrameRate() : 30.0f;

        if (Messages[RotateMsg[1]])
            carac->Rotate(&up, -RotateAngle * (behcontext.DeltaTime * m_FrameRate) * 0.001f);
        if (Messages[RotateMsg[0]])
            carac->Rotate(&up, RotateAngle * (behcontext.DeltaTime * m_FrameRate) * 0.001f);
    }

    //------------- Floor Management
    if (OnFloor)
    {

        float detOffset = 0.0f;
        beh->GetLocalParameterValue(8, &detOffset);

        CK3dEntity *pas = carac->GetFloorReferenceObject();
        VxVector pos;
        if (pas)
        { // Use floor reference object
            pas->GetPosition(&pos);

            pos.y += detOffset;

            float distup, distdown;
            CK3dEntity *eup, *edown;
            CKFloorPoint fp;
            if (fmanager && fmanager->GetNearestFloors(pos, &fp))
            {
                distup = fp.m_UpDistance;
                distdown = fp.m_DownDistance;
                eup = (CK3dEntity *)ctx->GetObject(fp.m_UpFloor);
                edown = (CK3dEntity *)ctx->GetObject(fp.m_DownFloor);
                carac->GetPosition(&pos);
                if (eup)
                    pos.y += distup + detOffset;
                else if (edown)
                    pos.y += distdown + detOffset;

                carac->SetPosition(&pos);
            }
        }
        else
        { // Use Bounding box
            const VxBbox &box = carac->GetBoundingBox();
            pos = (box.Max + box.Min) / 2.0f;
            pos.y = box.Min.y + detOffset;

            float distup, distdown;
            CK3dEntity *eup, *edown;
            CKFloorPoint fp;
            if (fmanager && fmanager->GetNearestFloors(pos, &fp))
            {
                distup = fp.m_UpDistance;
                distdown = fp.m_DownDistance;
                eup = (CK3dEntity *)ctx->GetObject(fp.m_UpFloor);
                edown = (CK3dEntity *)ctx->GetObject(fp.m_DownFloor);

                carac->GetPosition(&pos);
                if (eup)
                    pos.y += distup + detOffset;
                else if (edown)
                    pos.y += distdown + detOffset;
                carac->SetPosition(&pos);
            }
        }
    }

    //--------- Update Ouput parameters
    CKAnimation *CurrAnim = carac->GetActiveAnimation();
    CK_ID CurrAnimId = CKOBJID(CurrAnim);
    float currstep = CurrAnim ? CurrAnim->GetStep() : 0, currframe = CurrAnim ? CurrAnim->GetFrame() : 0;

    beh->SetOutputParameterValue(0, &CurrAnimId);
    beh->SetOutputParameterValue(2, &currstep);
    beh->SetOutputParameterValue(3, &currframe);

    //------------- Exit Management

    return CKBR_ACTIVATENEXTFRAME;
}

// Reconstruct indexes in UnlimitedParams so they match the stored animation pointer
// This function should only be called upon deletion of animations on this character
void ReconstructIndexes(CKContext *ctx, int count, UnlimitedParam *Params, const XObjectArray &array)
{
    for (int i = 0; i < count; ++i)
    {
        if (!(Params[i].ParameterMask & PARAM_ANIMATION_MASK) || (Params[i].ParameterIndexes[PARAM_ANIMATION_INDEX] < 0))
        { // use Animation index
            // The store ptr may be invalid => Iter on Array
            int Pos = -1;
            CKAnimation *Found = NULL;
            for (CK_ID *ids = array.Begin(); ids != array.End(); ++ids)
            {
                CKAnimation *anim = (CKAnimation *)ctx->GetObject(*ids);
                if (anim == Params[i].AnimationPtr)
                {
                    Found = anim;
                    Pos = (int)(ids - array.Begin());
                    break;
                }
            }
            Params[i].AnimationPtr = Found;
            Params[i].IndexAnimation = Pos;
        }
    }
}

// Check that each animation index points to the correct
// animation, if a deletion occurs on a animation
// the animation array will be resized and indexes will become invalid
// so we'll have to reconstruct the indexes
void CheckAnimations(CKContext *ctx, int count, UnlimitedParam *Params, const XObjectArray &array, CKCharacter *owner)
{
    for (int i = 0; i < count; ++i)
    {
        if (!(Params[i].ParameterMask & PARAM_ANIMATION_MASK) || (Params[i].ParameterIndexes[PARAM_ANIMATION_INDEX] < 0))
        { // use Animation index
            if (Params[i].AnimationPtr != (CKAnimation *)array.GetObject(ctx, Params[i].IndexAnimation))
            {

                if (!Params[i].AnimationPtr)
                {
                    Params[i].AnimationPtr = (CKAnimation *)array.GetObject(ctx, Params[i].IndexAnimation);
                }
                else
                {
                    ReconstructIndexes(ctx, count, Params, array);
                }
            }
            else if (owner && Params[i].AnimationPtr && Params[i].AnimationPtr->GetCharacter() != owner)
            {
                if (Params[i].AnimationPtr->GetCharacter())
                    ctx->OutputToConsoleEx("Character %s : invalid animation in Unlimited Controller (%s belongs to a different character %s)", owner->GetName(), Params[i].AnimationPtr->GetName(), Params[i].AnimationPtr->GetCharacter()->GetName());
                else
                    ctx->OutputToConsoleEx("Character %s : invalid animation in Unlimited Controller (%s does not belong to this character)", owner->GetName(), Params[i].AnimationPtr->GetName());
                Params[i].IndexAnimation = -1;
                Params[i].AnimationPtr = 0;
            }
        }
    }
}
