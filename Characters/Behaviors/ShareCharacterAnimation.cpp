/////////////////////////////////////////////////////
//		         Share Character Animations
/////////////////////////////////////////////////////
#include "CKAll.h"

CKObjectDeclaration *FillBehaviorShareCharacterAnimationsDecl();
CKERROR CreateShareCharacterAnimationsProto(CKBehaviorPrototype **pproto);
CKERROR ShareCharacterAnimationsCB(const CKBehaviorContext &behcontext);

CKObjectDeclaration *FillBehaviorShareCharacterAnimationsDecl()
{
    CKObjectDeclaration *od = CreateCKObjectDeclaration("Share Character Animation Keys");

    od->SetDescription("Share animations keys of with all characters in a group.");
    /* rem:
    <SPAN CLASS=pin>Characters Group: </SPAN>a group of characters that will share their animations from the character animations.<BR>
    <BR>
    <SPAN CLASS=pout>Number Of Shared Animations: </SPAN>retrieve the number of animation that could be shared with this method.<BR>
    <BR>
    For each animations of each characters in the group of characters, if an animation is the same
    than an animation of the character targeted by this building block then its data is replaced by a reference to the data of the source character animation.<BR>
    Use this building block to save some memory and file size. As animations take a lot of memory and file size, you may want to share the data key informations... that's what this building block does.<BR>
    To know if two animations are the same, this building block compare each animation keys... the comparison is not based on the name.<BR>
    */
    /* Warning:
    - This behavior shares the data keys when its edited ... beware.<BR>
    - Processing of this building block can be very long depending on the number of characters and animations to process.<BR>
    - With this building block, two characters can share the same animation keys (data keys), but the animation is proper to each character...
    this means each character can be animated independently (ie: the animation of one character does not influence the animation of the other character).<BR>
    - Once triggered one time the building block can be safely removed.<BR>
    */

    od->SetCategory("Characters/Animation");
    od->SetType(CKDLL_BEHAVIORPROTOTYPE);
    od->SetGuid(CKGUID(0x1C0D5E24, 0xDE47B706));
    od->SetAuthorGuid(VIRTOOLS_GUID);
    od->SetAuthorName("Virtools");
    od->SetVersion(0x00010000);
    od->SetCreationFunction(CreateShareCharacterAnimationsProto);
    od->SetCompatibleClassId(CKCID_CHARACTER);

    return od;
}

CKERROR CreateShareCharacterAnimationsProto(CKBehaviorPrototype **pproto)
{
    CKBehaviorPrototype *proto = CreateCKBehaviorPrototype("Share Character Animation Keys");
    if (!proto)
        return CKERR_OUTOFMEMORY;

    proto->DeclareInParameter("Characters Group", CKPGUID_GROUP);

    proto->DeclareOutParameter("Number Of Shared Animations", CKPGUID_INT);

    proto->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);
    proto->SetBehaviorFlags((CK_BEHAVIOR_FLAGS)(CKBEHAVIOR_TARGETABLE));
    proto->SetBehaviorCallbackFct(ShareCharacterAnimationsCB);

    *pproto = proto;
    return CK_OK;
}

CKERROR ShareCharacterAnimationsCB(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;
    CKContext *ctx = behcontext.Context;

    switch (behcontext.CallbackMessage)
    {
    case CKM_BEHAVIOREDITED:
    {
        CKCharacter *SourceCharacter = (CKCharacter *)beh->GetTarget();
        if (!SourceCharacter)
            return CKBR_OK;
        CKGroup *group = (CKGroup *)beh->GetInputParameterObject(0);
        if (!group)
            return CKBR_OK;

        int AnimsShared = 0;
        int ObjectAnimationsCount = ctx->GetObjectsCountByClassID(CKCID_OBJECTANIMATION);
        CK_ID *ObjectAnimations = ctx->GetObjectsListByClassID(CKCID_OBJECTANIMATION);

        int CharacterCount = group->GetObjectCount();
        CKCharacter *Carac;
        for (int i = 0; i < CharacterCount; ++i)
            if ((Carac = (CKCharacter *)group->GetObject(i)))
                if (CKIsChildClassOf(Carac, CKCID_CHARACTER))
                    if (Carac != SourceCharacter)
                        if (Carac->CheckIfSameKindOfHierarchy(SourceCharacter))
                        {
                            int AnimCount = SourceCharacter->GetAnimationCount();
                            CKKeyedAnimation *src_anim;
                            for (int j = 0; j < AnimCount; ++j)
                                if ((src_anim = (CKKeyedAnimation *)SourceCharacter->GetAnimation(j)))
                                    if (CKIsChildClassOf(src_anim, CKCID_KEYEDANIMATION))
                                    {
                                        int SubanimCount = src_anim->GetAnimationCount();
                                        CKObjectAnimation *Oanim;
                                        for (int k = 0; k < SubanimCount; ++k)
                                            if ((Oanim = src_anim->GetAnimation(k)))
                                            {
                                                for (int l = 0; l < ObjectAnimationsCount; l++)
                                                {
                                                    CKObjectAnimation *ani = (CKObjectAnimation *)ctx->GetObject(ObjectAnimations[l]);
                                                    CKBodyPart *bd;
                                                    if (ani != Oanim)
                                                        if ((bd = (CKBodyPart *)ani->Get3dEntity()))
                                                            if (CKIsChildClassOf(bd, CKCID_BODYPART))
                                                                if (bd->GetCharacter() == Carac)
                                                                    if (ani->Compare(Oanim))
                                                                    {
                                                                        ani->ShareDataFrom(Oanim);
                                                                        AnimsShared++;
                                                                    }
                                                }
                                            }
                                    }
                        }

        beh->SetOutputParameterValue(0, &AnimsShared);
    }
    break;
    }
    return CKBR_OK;
}
