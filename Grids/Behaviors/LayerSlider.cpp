/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
//
//		            LayerSlider
//
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
#include "CKAll.h"

CKObjectDeclaration *FillBehaviorLayerSliderDecl();
CKERROR CreateLayerSliderProto(CKBehaviorPrototype **);
int LayerSlider(const CKBehaviorContext &behcontext);
CKERROR LayerSliderCallBack(const CKBehaviorContext &behcontext); // CallBack Function

inline CKBOOL A_Calculate_Ellipse_Vector(float x, float y, float R1, float R2, float *X, float *Y);

// Inner struct
typedef struct
{
    VxVector Old_Pos;
} A_LS;
//---

CKObjectDeclaration *FillBehaviorLayerSliderDecl()
{
    CKObjectDeclaration *od = CreateCKObjectDeclaration("Layer Slider");
    od->SetDescription("Impede the 3d object to enter into no-null squares of a specific layers.");
    /* rem:
    <SPAN CLASS=in>On : </SPAN>Activates the building block (it will be called every frames).<BR>
    <SPAN CLASS=in>Off : </SPAN>Deactivate the building block.<BR>
    <BR>
    <SPAN CLASS=out>Contact : </SPAN>activate each time there's a contact.<BR>
    <SPAN CLASS=out>No Contact : </SPAN>activate each time there's no contact.<BR>
    <BR>
    <SPAN CLASS=pin>Influence Radius : </SPAN>amount of the object bounding sphere radius that will be taken into concideration to impede obstacles penetration.<BR>
    ( in most case 30% or 40% for a character would be fine )<BR>
    A new functionnality concerning the camera has been added : the influence radius is taken into concideration for the cameras.<BR>
    <SPAN CLASS=pin>Layer To Slide On : </SPAN>layer type to slide on.<BR>
    <BR>
    <SPAN CLASS=pout>Reaction Vector : </SPAN>(Parameter added by settings) 3d vector response (beware, this vector isn't normalized).<BR>
    <BR>
    <SPAN CLASS=setting>Reaction Vector : </SPAN>boolean value to be check if you want the reaction vector to be output as a parameter.<BR>
    <SPAN CLASS=setting>Output Contact Count : </SPAN>defines the number of output parameters to be added, so you can get all the contact points between the object (modelized as a cylinder) and the non-nul squares of the specified layers.<BR>
    <SPAN CLASS=setting>Accuracy : </SPAN>maximum accuracy for object collision (higher values for bad FPS, or if the velocity is too high).<BR>
    <BR>
    You can specify has many layer has you want.<BR>
    */
    /* warning:
    - 'Layer Slider' doesn't take the Grid's Priority into consideration. All the grids are taken into consideration.<BR>
    - 'Layer Slider' needs accurate environnement;<BR>
    1 - the object proportional diameter must be inferior to the width and length of a grid square.<BR>
    2 - the object should not move with a too great velocity (ie : greater than it's radius).<BR>
    3 - if you have a bad frame rate and do not want your object to pass trough walls when frame rate is too low, you should increment the <SPAN CLASS=setting>Accurate</SPAN> setting.<BR>
    <BR>
    - for cameras, try to put a high influence radius : as the near clip plane is quite a small value, the resulting velocity with regard to the camera radius is often very high.<BR>
    */
    od->SetType(CKDLL_BEHAVIORPROTOTYPE);
    od->SetCategory("Grids/Basic");
    od->SetGuid(CKGUID(0x7bac6da2, 0x1cbe76ed));
    od->SetAuthorGuid(VIRTOOLS_GUID);
    od->SetAuthorName("Virtools");
    od->SetVersion(0x00020000);
    od->SetCreationFunction(CreateLayerSliderProto);
    od->SetCompatibleClassId(CKCID_3DENTITY);
    return od;
}

CKERROR CreateLayerSliderProto(CKBehaviorPrototype **pproto)
{
    CKBehaviorPrototype *proto = CreateCKBehaviorPrototype("Layer Slider");
    if (!proto)
        return CKERR_OUTOFMEMORY;

    proto->DeclareInput("On");
    proto->DeclareInput("Off");

    proto->DeclareOutput("Contact");
    proto->DeclareOutput("No Contact");

    proto->DeclareInParameter("Influence Radius", CKPGUID_PERCENTAGE, "50");
    proto->DeclareInParameter("Layer To Slide On", CKPGUID_LAYERTYPE);

    proto->DeclareSetting("Reaction Vecto", CKPGUID_BOOL, "FALSE");
    proto->DeclareSetting("Output Contact Count", CKPGUID_INT, "0");
    proto->DeclareLocalParameter(NULL, CKPGUID_VOIDBUF); // inner struct LS
    proto->DeclareSetting("Accuracy", CKPGUID_INT, "0");

    proto->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);
    proto->SetBehaviorFlags((CK_BEHAVIOR_FLAGS)(CKBEHAVIOR_VARIABLEPARAMETERINPUTS | CKBEHAVIOR_TARGETABLE | CKBEHAVIOR_INTERNALLYCREATEDINPUTPARAMS));
    proto->SetFunction(LayerSlider);
    proto->SetBehaviorCallbackFct(LayerSliderCallBack);

    *pproto = proto;
    return CK_OK;
}

int LayerSlider(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;
    CKContext *ctx = behcontext.Context;

    CK3dEntity *obj = (CK3dEntity *)beh->GetTarget();
    if (!obj)
        return CKBR_OWNERERROR;

    A_LS *ls = NULL; // get local param
    beh->GetLocalParameterValue(2, &ls);

    VxVector init_pos;
    obj->GetPosition(&init_pos);

    //__________ OFF

    if (beh->IsInputActive(1))
    {
        beh->ActivateInput(1, FALSE);
        return CKBR_OK;
    }

    //__________ IN

    if (beh->IsInputActive(0))
    {
        beh->ActivateInput(0, FALSE);

        ls->Old_Pos = init_pos;
    }

    beh->ActivateInput(0, FALSE);

    CKGridManager *gm = (CKGridManager *)ctx->GetManagerByGuid(GRID_MANAGER_GUID);
    const XObjectPointerArray &grids = gm->GetGridArray();

    if (!grids.Size())
    {
        beh->ActivateOutput(1, TRUE);
        return CKBR_OK;
    }

    int contact_count = 0; // get setting : Contact Count
    beh->GetLocalParameterValue(1, &contact_count);
    int index_contact = 0;

    CKBOOL reaction = TRUE; // get setting : Reaction Vector ?
    if (!contact_count)
    {
        reaction = FALSE;
        beh->GetLocalParameterValue(0, &reaction);
    }

    float influence = 0.5f; // get the influence radius
    beh->GetInputParameterValue(0, &influence);

    CK_CLASSID classid = obj->GetClassID();
    if ((classid == CKCID_CAMERA) || (classid == CKCID_TARGETCAMERA))
    { // if obj is camera then radius = f( NearClipPlane, Fov )
        float fov = ((CKCamera *)obj)->GetFov();
        float d = ((CKCamera *)obj)->GetFrontPlane();
        influence = d * acosf(fov * 0.5f) * 1.2f * influence;
        if (influence < 0.5f)
            influence = 0.5f;
    }
    else if ((classid != CKCID_SPRITE3D) && (classid != CKCID_3DENTITY))
    {
        influence *= obj->GetRadius();
    }

    int w, l, x, y;
    int v1, v3, v5, v7, vC;
    float p1, p3, p5, p7;
    float tmp_minx, tmp_maxx, tmp_minz, tmp_maxz;
    CKGrid *grid;
    CKLayer *layer;
    VxVector pos, pos_current, is; // pos, influence_radius_scaled
    VxVector t(0, 0, 0), total;
    int layer_type, input_index, input_total = beh->GetInputParameterCount();
    CKBOOL contact = FALSE, posallreadycalc, scaleYallreadycalc, scaleXallreadycalc, scaleZallreadycalc;

    int precis = 0;
    beh->GetLocalParameterValue(3, &precis);
    if (precis < 0)
        precis = 0;

    int precis_tmp = precis;

    VxVector obj_step;
    if (precis)
    {
        precis_tmp = (int)(Magnitude(init_pos - ls->Old_Pos) / (influence - 0.1f));
        if (precis_tmp > precis)
            precis_tmp = precis;
        obj_step = (init_pos - ls->Old_Pos) / (precis_tmp + 1.0f);
        pos_current = ls->Old_Pos;
    }
    else
    {
        pos_current = init_pos;
    }

    do
    {
        if (precis)
        {
            pos_current += obj_step;
            obj->SetPosition(&pos_current);
        }

        for (CKObject **o = grids.Begin(); o != grids.End(); ++o)
        { // For all grids in Scene
            grid = (CKGrid *)*o;
            if (grid->IsActive())
            {

                posallreadycalc = FALSE;
                scaleYallreadycalc = FALSE;
                scaleXallreadycalc = FALSE;
                scaleZallreadycalc = FALSE;

                for (input_index = 1; input_index < input_total; ++input_index)
                { // For all specified layer to slide on
                    // get the layer to slide on
                    beh->GetInputParameterValue(input_index, &layer_type);
                    layer = grid->GetLayer(layer_type);
                    if (layer)
                    {

                        if (!posallreadycalc)
                        { // do not calc pos if it has been yet for prev layer
                            grid->InverseTransform(&pos, &pos_current);
                            posallreadycalc = TRUE;
                        }

                        // get grid's world scale
                        CKBOOL hasparent;
                        const VxMatrix &WMat = grid->GetWorldMatrix();

                        if (!scaleYallreadycalc)
                        {
                            hasparent = (grid->GetParent() != NULL);
                            if (hasparent)
                            {
                                is.y = Magnitude(WMat[1]);
                            }
                            else
                            {
                                grid->GetScale(&is);
                            }
                            is.y = influence / is.y;
                            scaleYallreadycalc = TRUE;
                        }

                        if ((pos.y >= -is.y) && (pos.y < 1.0f + is.y))
                        {
                            w = grid->GetWidth();
                            if (!scaleXallreadycalc)
                            {
                                if (hasparent)
                                    is.x = Magnitude(WMat[0]);
                                is.x = influence / is.x;
                                scaleXallreadycalc = TRUE;
                            }
                            tmp_minx = pos.x - is.x;
                            tmp_maxx = pos.x + is.x;
                            if ((tmp_minx < w) && (tmp_maxx >= 0.0f))
                            {
                                l = grid->GetLength();
                                if (!scaleZallreadycalc)
                                {
                                    if (hasparent)
                                        is.z = Magnitude(WMat[2]);
                                    is.z = influence / is.z;
                                    scaleZallreadycalc = TRUE;
                                }
                                tmp_minz = pos.z - is.z;
                                tmp_maxz = pos.z + is.z;
                                if ((tmp_minz < l) && (tmp_maxz >= 0.0f))
                                {

                                    x = (pos.x >= 0.0f) ? (int)pos.x : (int)(pos.x - 1.0f);
                                    y = (pos.z >= 0.0f) ? (int)pos.z : (int)(pos.z - 1.0f);
                                    /*
                                    :---:---:---:   o = the square containing the sphere
                                    | 0 | 1 | 2 |
                                    :---:---:---:
                                    | 7 | o | 3 |
                                    :---:---:---:
                                    | 6 | 5 | 4 |
                                    :---:---:---:
                                    */
                                    p1 = p3 = p5 = p7 = 1.0f; // penetration in column and raw (negative = penetration)
                                    if (tmp_minx >= 0.0f)
                                        p7 = tmp_minx - x;
                                    if (tmp_maxx < w)
                                        p3 = (x + 1) - tmp_maxx;
                                    if (tmp_minz >= 0.0f)
                                        p5 = tmp_minz - y;
                                    if (tmp_maxz < l)
                                        p1 = (y + 1) - tmp_maxz;

                                    int pflag = 0; // penetration flag

                                    t.x = 0.0f;
                                    t.z = 0.0f; // init translation vector

                                    if (p7 <= 0.0f)
                                    { // penetration on column
                                        if ((y >= 0) && (y < l))
                                        {
                                            layer->GetValue(x - 1, y, &v7);
                                            if (v7)
                                                t.x = -p7;
                                            else
                                                pflag |= 0x0001;
                                        }
                                        else
                                            pflag |= 0x0001;
                                    }
                                    else if (p3 <= 0.0f)
                                    {
                                        if ((y >= 0) && (y < l))
                                        {
                                            layer->GetValue(x + 1, y, &v3);
                                            if (v3)
                                                t.x = p3;
                                            else
                                                pflag |= 0x0010;
                                        }
                                        else
                                            pflag |= 0x0010;
                                    }

                                    if (p1 <= 0.0f)
                                    { // penetration on raw
                                        if ((x >= 0) && (x < w))
                                        {
                                            layer->GetValue(x, y + 1, &v1);
                                            if (v1)
                                                t.z = p1;
                                            else
                                                pflag |= 0x0100;
                                        }
                                        else
                                            pflag |= 0x0100;
                                    }
                                    else if (p5 <= 0.0f)
                                    {
                                        if ((x >= 0) && (x < w))
                                        {
                                            layer->GetValue(x, y - 1, &v5);
                                            if (v5)
                                                t.z = -p5;
                                            else
                                                pflag |= 0x1000;
                                        }
                                        else
                                            pflag |= 0x1000;
                                    }

                                    // Check penetration FLAG for corner contact
                                    switch (pflag)
                                    {

                                    case 0x0101: // Left-Top
                                    {
                                        layer->GetValue(x - 1, y + 1, &vC);
                                        if (vC)
                                        {
                                            float X, Y;
                                            if (A_Calculate_Ellipse_Vector(pos.x - x, y + 1 - pos.z, is.x, is.z, &X, &Y))
                                            {
                                                t.x = X;
                                                t.z = -Y;
                                            }
                                        }
                                    }
                                    break;

                                    case 0x1001: // Left-Bottom
                                    {
                                        layer->GetValue(x - 1, y - 1, &vC);
                                        if (vC)
                                        {
                                            float X, Y;
                                            if (A_Calculate_Ellipse_Vector(pos.x - x, pos.z - y, is.x, is.z, &X, &Y))
                                            {
                                                t.x = X;
                                                t.z = Y;
                                            }
                                        }
                                    }
                                    break;

                                    case 0x0110: // Rigth-Top
                                    {
                                        layer->GetValue(x + 1, y + 1, &vC);
                                        if (vC)
                                        {
                                            float X, Y;
                                            if (A_Calculate_Ellipse_Vector(x + 1 - pos.x, y + 1 - pos.z, is.x, is.z, &X, &Y))
                                            {
                                                t.x = -X;
                                                t.z = -Y;
                                            }
                                        }
                                    }
                                    break;

                                    case 0x1010: // Rigth-Bottom
                                    {
                                        layer->GetValue(x + 1, y - 1, &vC);
                                        if (vC)
                                        {
                                            float X, Y;
                                            if (A_Calculate_Ellipse_Vector(x + 1 - pos.x, pos.z - y, is.x, is.z, &X, &Y))
                                            {
                                                t.x = -X;
                                                t.z = Y;
                                            }
                                        }
                                    }
                                    break;
                                    }

                                    if (t.x || t.z)
                                    {
                                        obj->Translate(&t, grid);
                                        contact = TRUE;
                                        if (reaction)
                                        {
                                            grid->TransformVector(&t, &t);
                                            total += t;
                                        }
                                        if (index_contact < contact_count)
                                        {
                                            index_contact++;
                                            t.Normalize();
                                            t *= influence;
                                            VxVector contact_pos;
                                            obj->GetPosition(&contact_pos);
                                            contact_pos -= t;

                                            beh->SetOutputParameterValue(index_contact, &contact_pos);
                                        }
                                        precis_tmp = 0;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

    } while (precis_tmp--);

    if (precis)
    {
        obj->GetPosition(&ls->Old_Pos);
    }

    if (contact)
    { // Contact
        beh->ActivateOutput(0);
        if (reaction)
        {
            beh->SetOutputParameterValue(0, &total);
        }
    }
    else
    { // No Contact
        beh->ActivateOutput(1);
    }
    return CKBR_ACTIVATENEXTFRAME;
}

/***********************************************/
/*      Calculate Ellipse Vector               */
/***********************************************/
inline CKBOOL A_Calculate_Ellipse_Vector(float x, float y, float R1, float R2, float *X, float *Y)
{
    float xdy = x / y;
    float R1dR2 = R1 / R2;
    *Y = R1 / sqrtf(xdy * xdy + R1dR2 * R1dR2);

    if (*Y <= y)
        return FALSE; // (x,y) is outside ellipse

    *X = *Y * xdy - x;
    *Y -= y;
    return TRUE;
}

/*******************************************************/
/*                     CALLBACK                        */
/*******************************************************/
CKERROR LayerSliderCallBack(const CKBehaviorContext &behcontext)
{
    CKBehavior *beh = behcontext.Behavior;

    switch (behcontext.CallbackMessage)
    {

    case CKM_BEHAVIORCREATE:
    case CKM_BEHAVIORLOAD:
    {
        A_LS *ls = new A_LS;

        //--- if version < 2 create local parameter and settings
        if (beh->GetLocalParameterCount() < 4)
        {

            CKParameterLocal *p;

            p = beh->CreateLocalParameter("Reaction Vector", CKPGUID_BOOL);
            CKBOOL k = FALSE;
            p->SetValue(&k);

            p = beh->CreateLocalParameter("Output Contact Count", CKPGUID_INT);
            int c = 0;
            p->SetValue(&c);

            p = beh->CreateLocalParameter(NULL, CKPGUID_VOIDBUF); // inner struct LS

            p = beh->CreateLocalParameter("Accuracy", CKPGUID_INT);
            p->SetValue(&c);
        }
        beh->SetLocalParameterValue(2, &ls, sizeof(ls));
    }
    break;

    case CKM_BEHAVIORDELETE:
    {

        A_LS *ls = NULL;
        beh->GetLocalParameterValue(2, &ls);
        if (ls)
        {

            delete ls;
            ls = NULL;
            beh->SetLocalParameterValue(2, &ls);
        }
    }
    break;

    case CKM_BEHAVIOREDITED:
    {
        CKParameterIn *pin;
        CKParameter *pout;
        for (int i = 1; i < beh->GetInputParameterCount(); i++)
        {
            pin = beh->GetInputParameter(i);
            if (pin->GetGUID() != CKPGUID_LAYERTYPE)
            {
                pin->SetGUID(CKPGUID_LAYERTYPE);
                if ((pout = pin->GetRealSource()))
                {
                    pout->SetGUID(CKPGUID_LAYERTYPE);
                }
            }
        }
    }
    break;

    case CKM_BEHAVIORSETTINGSEDITED:
    {
        CKBOOL reaction = FALSE;

        //____ Output Contact Count
        int contact_count = 0;
        beh->GetLocalParameterValue(1, &contact_count);

        if (contact_count < 0)
            contact_count = 0;

        if (contact_count)
        {
            reaction = TRUE;
            beh->SetLocalParameterValue(0, &reaction);
        }
        else
        { //____ Recation ?
            beh->GetLocalParameterValue(0, &reaction);
        }

        if (beh->GetOutputParameterCount())
        {
            if (!reaction)
                CKDestroyObject(beh->RemoveOutputParameter(0));
        }
        else
        {
            if (reaction)
                beh->CreateOutputParameter("Reaction", CKPGUID_VECTOR);
        }

        //----
        char str[64];

        int dif = contact_count - beh->GetOutputParameterCount();
        if (dif >= 0 && contact_count > 0)
        { // add output
            for (int a = 0; a < (dif + 1); a++)
            {
                snprintf(str, sizeof(str), "contact%d", beh->GetOutputParameterCount());
                beh->CreateOutputParameter(str, CKPGUID_VECTOR);
            }
        }

        while (dif < -1)
        { // remove output
            beh->RemoveOutputParameter(beh->GetOutputParameterCount() - 1);
            dif++;
        }
    }
    break;
    }

    return CKBR_OK;
}
