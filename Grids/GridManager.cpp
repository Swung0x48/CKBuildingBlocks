/*************************************************************************/
//	File : GridManager.cpp
//
/*************************************************************************/
#include "CKAll.h"
#include "GridPathManager.h"
#include "GridManager.h"

// {secret}
const char *GridManagerName = "Grid Manager";
// {secret}
const char *GridClassificationName = "Grid Classification";
// {secret}
const char *GridName = "Grid";

// {secret}
GridManager::GridManager(CKContext *Context) : CKGridManager(Context, GRID_MANAGER_GUID, "Grid Manager")
{
    InitData();
    m_Context->RegisterNewManager(this);
    m_GridPathManager = 0;
}

// {secret}
GridManager::~GridManager()
{
    InternalClearData(FALSE);
    delete m_GridPathManager;
}

// {secret}
void GridManager::InitData()
{
    m_Remap = NULL;
    m_RemapCount = 0;

    m_LayerTypeCount = 2;
    m_LayerTypeName = new char*[2];
    m_LayerTypeName[0] = new char[20];
    strcpy(m_LayerTypeName[0], "invalid layer type");
    m_LayerTypeName[1] = new char[4];
    strcpy(m_LayerTypeName[1], "xxx");

    m_LayerTypeColor = new VxColor[2];
    m_LayerTypeColor[0].Set(0, 0, 0);
    m_LayerTypeColor[1].Set(232, 232, 49);

    m_AssociatedParam = new CKGUID[2];
    m_AssociatedParam[0] = CKPGUID_NONE;
    m_AssociatedParam[1] = CKPGUID_INT;

    m_LayerAttributeIndex = new int[2];
    m_LayerAttributeIndex[0] = m_LayerAttributeIndex[1] = -1;
}

// {secret}
CKERROR GridManager::PreProcess()
{

    if (m_GridPathManager)
        m_GridPathManager->Manage();
    return (CK_OK);
}

//-----------------------------------------------------
// Grid Objects

/*************************************************
Summary: Returns the number of grids
{Group:Grids}
Input Arguments:
    flag: GRID_FLAG_SCENE to return the number of grid in the current scene or
        GRID_FLAG_LEVEL to return the total number of grids.
Return Value:
    Number of grids
See also: GetGridObject
*************************************************/
int GridManager::GetGridObjectCount(int flag)
{
    switch (flag)
    {
    case GRID_FLAG_SCENE:
    {
        const XObjectPointerArray &grids = m_Context->GetAttributeManager()->GetAttributeListPtr(m_GridAttribute);
        return grids.Size();
    }
    break;
    case GRID_FLAG_LEVEL:
    {
        const XObjectPointerArray &grids = m_Context->GetAttributeManager()->GetGlobalAttributeListPtr(m_GridAttribute);
        return grids.Size();
    }
    break;
    }

    return 0;
}

/*************************************************
Summary: returns the Nth grid in the scene or level.
{Group:Grids}
Input Arguments:
    pos: index of the grid to return.
    flag: GRID_FLAG_SCENE to return the number of grid in the current scene or
        GRID_FLAG_LEVEL to return the total number of grids.
Return Value:
    A pointer to a CKGrid.
See also: GetGridObjectCount
*************************************************/
CKGrid *GridManager::GetGridObject(int pos, int flag)
{
    switch (flag)
    {
    case GRID_FLAG_SCENE:
    {
        const XObjectPointerArray &grids = m_Context->GetAttributeManager()->GetAttributeListPtr(m_GridAttribute);
        return (CKGrid *)grids[pos];
    }
    break;
    case GRID_FLAG_LEVEL:
    {
        const XObjectPointerArray &grids = m_Context->GetAttributeManager()->GetGlobalAttributeListPtr(m_GridAttribute);
        return (CKGrid *)grids[pos];
    }
    break;
    }
    return NULL;
}

//____________________________________________________
//      LAYER TYPE

/*************************************************
Name: GetTypeFromName

Summary: Gets the Layer Type from its name

Arguments:
    TypeName: Name of the Layer Type (ex: "collision", "rugosity", ...)

Return Value:
    The Layer Type if successful, 0 otherwise

Remarks:

See also: GetTypeName

*************************************************/
int GridManager::GetTypeFromName(CKSTRING TypeName)
{
    if (!TypeName)
        return 0;

    char *tmp;
    int type = m_LayerTypeCount;
    while (type)
    {
        if (tmp = m_LayerTypeName[--type])
        {
            if (!strcmp(tmp, TypeName))
                break;
        }
    }
    return type;
}

/*************************************************
Name: GetTypeName

Summary: Gets the name of a Layer Type

Arguments:
    type: the Layer Type

Return Value:
    The name of the Layer Type if successful, NULL otherwise

Remarks:

See also: GetTypeFromName

*************************************************/
CKSTRING GridManager::GetTypeName(int type)
{
    if ((type < m_LayerTypeCount) && (type >= 0))
    {
        return m_LayerTypeName[type];
    }
    else
    {
        return NULL;
    }
}

/*************************************************
Name: SetTypeName

Summary: Sets the name of a Layer Type

Arguments:
    type: the Layer Type

Return Value:
  - CK_OK if successful, Error Code otherwise

Remarks:

See also: GetTypeName, GetTypeFromName

*************************************************/
CKERROR GridManager::SetTypeName(int type, CKSTRING Name)
{
    if ((type < m_LayerTypeCount) && (type >= 0) && Name)
    {
        CKAttributeManager *atm = m_Context->GetAttributeManager();

        if (GetTypeFromName(Name))
            return CKERR_ALREADYPRESENT;

        // Change Attribute Name
        if (atm->IsAttributeIndexValid(m_LayerAttributeIndex[type]) == TRUE)
            atm->SetAttributeNameByType(m_LayerAttributeIndex[type], Name);

        delete[] m_LayerTypeName[type];
        m_LayerTypeName[type] = new char[strlen(Name) + 1];
        strcpy(m_LayerTypeName[type], Name);

        XString enumstring;
        XString tmp;
        enumstring = GetTypeName(1);
        enumstring << "=1";
        for (int a = 2; a < GetLayerTypeCount(); a++)
        {
            enumstring << "," << GetTypeName(a) << "=" << a;
        }
        m_Context->GetParameterManager()->ChangeEnumDeclaration(CKPGUID_LAYERTYPE, enumstring.Str());

        return CK_OK;
    }
    else
    {
        return CKERR_NOTFOUND;
    }
}

/*************************************************
Name: RegisterType

Summary: Registers a new Layer Type

Arguments:
    The Layer Type's name

Return Value:
  - The Layer Type index if successful, 0 otherwise

Remarks:
  - If a Layer Type with the same exists, the function return its index

See also: UnRegisterType, SetAssociatedParam

*************************************************/
int GridManager::RegisterType(CKSTRING TypeName)
{
    if (!TypeName)
        return 0;

    int type = GetTypeFromName(TypeName);

    if (type)
    { // if a Layer Type has the same Name
        return type;
    }
    else
    {

        //---
        char **tmp1 = new char*[m_LayerTypeCount + 1];
        memcpy(tmp1, m_LayerTypeName, sizeof(char*) * m_LayerTypeCount);
        delete[] m_LayerTypeName;
        m_LayerTypeName = tmp1;
        m_LayerTypeName[m_LayerTypeCount] = NULL;
        // m_LayerTypeName[type] = new char[strlen(Name)+1];
        // strcpy( m_LayerTypeName[type], Name );

        //---
        VxColor *tmp3 = new VxColor[m_LayerTypeCount + 1];
        memcpy(tmp3, m_LayerTypeColor, sizeof(VxColor) * m_LayerTypeCount);
        delete[] m_LayerTypeColor;
        m_LayerTypeColor = tmp3;

        int r3 = (rand() & 1) + (rand() & 1); // random color
        int R, G, B;
        if (r3 == 1)
            R = 128 + rand() & 127;
        else
            R = rand() & 255;
        if (r3 == 0)
            G = 128 + rand() & 127;
        else
            G = rand() & 255;
        if (r3 == 2)
            B = 128 + rand() & 127;
        else
            B = rand() & 255;
        m_LayerTypeColor[m_LayerTypeCount].Set(R, G, B);

        //---
        CKGUID *tmp2 = new CKGUID[m_LayerTypeCount + 1];
        memcpy(tmp2, m_AssociatedParam, sizeof(CKGUID) * m_LayerTypeCount);
        delete[] m_AssociatedParam;
        m_AssociatedParam = tmp2;
        m_AssociatedParam[m_LayerTypeCount] = CKPGUID_INT;

        //----
        int *tmp4 = new int[m_LayerTypeCount + 1];
        memcpy(tmp4, m_LayerAttributeIndex, sizeof(int) * m_LayerTypeCount);
        delete[] m_LayerAttributeIndex;
        m_LayerAttributeIndex = tmp4;
        m_LayerAttributeIndex[m_LayerTypeCount] = -1;

        //----
        m_LayerTypeCount++;
        if (SetTypeName(m_LayerTypeCount - 1, TypeName) == CK_OK)
        { //////////// ADDED
            // Create New Attribute
            CKAttributeManager *atm = m_Context->GetAttributeManager();
            int newIndex;
            newIndex = atm->RegisterNewAttributeType(TypeName, CKPGUID_FLOAT); // Linker => Cost for use, Integer => Wall minimal value
            atm->SetAttributeDefaultValue(newIndex, "1");
            atm->AddCategory("Grid Path Finding");
            atm->SetAttributeCategory(newIndex, "Grid Path Finding");
            // atm->SetAttributeCallbackFunction(new
            // If everything is right ...
            m_LayerAttributeIndex[m_LayerTypeCount - 1] = newIndex;
        }

        return (m_LayerTypeCount - 1);
    }
}

/*************************************************
Name: UnRegisterType

Summary: UnRegisters an existing Layer Type

Arguments:
    TypeName: The Layer Type's name

Return Value:
  - The Layer Type index if successful, 0 otherwise

Remarks:
  - If a Layer Type with the same exists, the function return its index

See also: UnRegisterType, SetAssociatedParam

*************************************************/
int GridManager::UnRegisterType(CKSTRING TypeName)
{
    if (!TypeName)
        return 0;

    int type = GetTypeFromName(TypeName);

    if (!type)
        return 0;

    /// ????

    {
        // m_LayerTypeCount--;
    }
    return 0;
}

/************************************************
Name: GetAssociatedColor

Summary: Gets the Color associated with the layer type

Arguments:
    type: Layer Type
    col: a pointer to the color

See also: SetAssociatedColor

************************************************/
CKERROR GridManager::GetAssociatedColor(int type, VxColor *col)
{
    if ((type < m_LayerTypeCount) && (type >= 0))
    {
        *col = m_LayerTypeColor[type];
        return CK_OK;
    }
    return CKERR_INVALIDPARAMETER;
}

/*************************************************
Name: SetAssociatedColor

Summary: Sets the Color associated with the layer type

Arguments:
    type: Layer Type
    col: a pointer to the color

Return Value: CK_OK if successful , Error Code otherwise

See also: GetAssociatedColor

*************************************************/
CKERROR GridManager::SetAssociatedColor(int type, VxColor *col)
{
    if ((type < m_LayerTypeCount) && (type >= 0))
    {
        if (col)
        {
            m_LayerTypeColor[type] = *col;
            return CK_OK;
        }
    }
    return CKERR_INVALIDPARAMETER;
}

/************************************************
Name: GetAssociatedParam

Summary: Gets the CKGUID of the parameter associated with the layer type

Return Value: parameter type (eg: CKPGUID_FLOAT, CKPGUID_SOUND) if successful, CKPGUID_NONE otherwise

Remarks:

See also: SetAssociatedParam

************************************************/
CKGUID GridManager::GetAssociatedParam(int type)
{
    if ((type < m_LayerTypeCount) && (type >= 0))
    {
        return m_AssociatedParam[type];
    }
    return CKPGUID_NONE;
}

/*************************************************
Name: SetAssociatedParam

Summary: Specify which parameter is associated with the given Layer Type

Arguments:
    type: Layer Type
    param: CKGUID to be associated with the Layer Type (eg: CKPGUID_FLOAT, CKPGUID_SOUND, ...)

Return Value: CKERROR: CK_OK if successful , Error Code otherwise

Remarks:

See also: GetAssociatedParam

*************************************************/
CKERROR GridManager::SetAssociatedParam(int type, CKGUID param)
{
    if ((type < m_LayerTypeCount) && (type >= 0))
    {
        m_AssociatedParam[type] = param;
        return CK_OK;
    }
    else
    {
        return CKERR_INVALIDPARAMETER;
    }
}

/************************************************
Name: GetLayerTypeCount

Summary: Gets the Layer Type Count

Return Value: the layer type count

************************************************/
int GridManager::GetLayerTypeCount()
{
    return m_LayerTypeCount;
}

//____________________________________________________
//      CLASSIFICATION
/************************************************
Name: GetGridClassificationCategory

Summary: Gets the index of the GridClassification Category

Return Value:

************************************************/
int GridManager::GetGridClassificationCategory()
{
    return m_GridClassificationCatego;
}

/*************************************************
Name: GetClassificationFromName

Summary: Get Classification Index in the Attribut Manager.

Arguments:
    ClassificationName: the name.

Return Value: The Classification index in the Attribut Manager if successful, -1 otherwise.

Remarks:

See also:

*************************************************/
int GridManager::GetClassificationFromName(CKSTRING ClassificationName)
{
    CKAttributeManager *attman = m_Context->GetAttributeManager();
    return attman->GetAttributeTypeByName(ClassificationName);
}

/*************************************************
Name: GetClassificationName

Summary: Get the Classification Name.

Arguments:
    Classification: Index in the Attribut Manager.

Return Value: The Classification Name if successful, NULL otherwise.

Remarks:

See also:

*************************************************/
CKSTRING GridManager::GetClassificationName(int Classification)
{

    CKAttributeManager *attman = m_Context->GetAttributeManager();
    return attman->GetAttributeNameByType(Classification);
}

/*************************************************
Name: RegisterClassification

Summary: Register a Classification in the Grid Manager (in fact in the Attribut Manager).

Arguments:
    ClassificationName: The Name of the Classification.

Return Value: The Classification Index in the Attribut Manager if successful, -1 otherwise.

Remarks:

See also:

*************************************************/
int GridManager::RegisterClassification(CKSTRING ClassificationName)
{
    CKAttributeManager *attman = m_Context->GetAttributeManager();

    int attrib = attman->RegisterNewAttributeType(ClassificationName, CKGUID(0, 0), CKCID_3DENTITY);
    attman->SetAttributeCategory(attrib, GridClassificationName);
    return attrib;
}

//____________________________________________________
//      GRID FINDING

/*************************************************
Name: GetGridArray

Summary: Gets a pointer to the array containing all the grid inside current scene

Return Value:
  - the grid array pointer

Remarks: Unlike 'GetNearestGrid', 'GetPreferredGrid' or 'IsInGrid' functions, this
one does not take the Activation flag into consideration (all the grids are listed,
even those deactivated).

See also:

*************************************************/
const XObjectPointerArray &GridManager::GetGridArray(int flag)
{
    CKAttributeManager *attman = m_Context->GetAttributeManager();
    switch (flag)
    {
    case GRID_FLAG_SCENE:
    {
        return attman->GetAttributeListPtr(m_GridAttribute);
    }
    break;
    case GRID_FLAG_LEVEL:
    {
        return attman->GetGlobalAttributeListPtr(m_GridAttribute);
    }
    break;
    }
    return attman->GetAttributeListPtr(-1);
}

/*************************************************
Summary: Gets the nearest grid from a 3d position expressed in a referential
Arguments:
    pos: the 3d position expressed in the referential ref
    ref: the referential of the position (NULL by default = World)
Return Value:
  the nearest grid if successful, NULL if no grid was found.
See also: GetPreferredGrid,IsIngrid
*************************************************/
CKGrid *GridManager::GetNearestGrid(VxVector *pos, CK3dEntity *ref)
{
    CKAttributeManager *attman = m_Context->GetAttributeManager();
    const XObjectPointerArray &grids = attman->GetAttributeListPtr(m_GridAttribute);

    CKGrid *grid, *nearest = NULL;
    float mag_tmp, min_magnitude = 100000000.0f;
    for (CKObject **o = grids.Begin(); o != grids.End(); ++o)
    {
        if (grid = (CKGrid *)*o)
        {
            if (grid->IsActive())
            {
                VxVector tmp2;
                grid->InverseTransform(&tmp2, pos, ref);

                tmp2.x -= 0.5f * grid->GetWidth();
                tmp2.z -= 0.5f * grid->GetLength();
                tmp2.y -= 0.5f;
                mag_tmp = Magnitude(tmp2);
                if (mag_tmp < min_magnitude)
                {
                    min_magnitude = mag_tmp;
                    nearest = grid;
                }
            }
        }
    }

    return nearest;
}

/*************************************************
Summary: Gets the grid containing a point given its priority.
{group:Grids}
Arguments:
    pos: position to consider, expressed in the referential ref
    ref: the referential in which the pos are expressed (NULL by default = World)
Return Value:
    The grid with the highest priority that contains the point pos.
See also:GetNearestGrid,IsIngrid
*************************************************/
CKGrid *GridManager::GetPreferredGrid(VxVector *pos, CK3dEntity *ref)
{
    CKAttributeManager *attman = m_Context->GetAttributeManager();
    const XObjectPointerArray &grids = attman->GetAttributeListPtr(m_GridAttribute);

    CKGrid *grid, *preferenced = NULL;
    int tmp_priority, max_priority = -100000000;

    for (CKObject **o = grids.Begin(); o != grids.End(); ++o)
    {
        if (grid = (CKGrid *)*o)
        {
            if (grid->IsActive())
            {
                if (IsInGrid(grid, pos, ref))
                {
                    tmp_priority = grid->GetGridPriority();
                    if (tmp_priority > max_priority)
                    {
                        max_priority = tmp_priority;
                        preferenced = grid;
                    }
                }
            }
        }
    }

    return preferenced;
}

/*************************************************
Summary: TRUE if 3d point is in grid range, FALSE otherwise.
Arguments:
    grid: the grid to be checked.
    pos: the 3d position expressed in the referential 'ref'.
    ref: the referential in which 'pos' is expressed, NULL for world coordinates.
Return Value:
    TRUE if the point is inside the grid range.
Remarks: Takes the Activation Flags into consideration
See also:
*************************************************/
CKBOOL GridManager::IsInGrid(CKGrid *grid, VxVector *pos, CK3dEntity *ref)
{
    if (!grid)
        return FALSE;
    if (!grid->IsActive())
        return FALSE;

    VxVector tmp2;
    grid->InverseTransform(&tmp2, pos, ref);

    if ((tmp2.x >= 0.0f) && (tmp2.x < (float)grid->GetWidth()))
        if ((tmp2.z >= 0.0f) && (tmp2.z < (float)grid->GetLength()))
            if ((tmp2.y >= 0.0f) && (tmp2.y < 1.0f))
                return TRUE;

    return FALSE;
}

//*********************************************************************************************************
//*********************************************************************************************************
//*********************************************************************************************************
//***************************** UNDOCUMENTED FUNCTIONS ****************************************************
//*********************************************************************************************************
//*********************************************************************************************************

// {secret}
CKERROR GridManager::LoadData(CKStateChunk *chunk, CKFile *LoadedFile)
{
    m_Remap = NULL;
    m_RemapCount = 0;
    XArray<int> LayerRemap;
    LayerRemap.Resize(2);
    LayerRemap[0] = 0;
    LayerRemap[1] = 1;

    //____________________________
    // Remapping of LayerTypes values
    int ObjectCount = LoadedFile->m_FileObjects.Size();
    XHashTable<int, const char *> AlreadyRegistered;
    for (int a = 0; a < ObjectCount; a++)
    {
        if (LoadedFile->m_FileObjects[a].ObjectCid == CKCID_LAYER)
        {
            CKSTRING TypeName = LoadedFile->m_FileObjects[a].Name;
            //--- Only add to the table of remapping if the type name has
            // not been parsed yet
            if (TypeName && strcmp(TypeName, DEFAULT_LAYER_NAME))
            { // do not concider - default layer -
                if (!AlreadyRegistered.FindPtr(TypeName))
                {
                    AlreadyRegistered.Insert(TypeName, 1);

                    int layer_type = GetTypeFromName(TypeName);
                    if (layer_type)
                    {
                        LayerRemap.PushBack(layer_type);
                    }
                    else
                    {
                        LayerRemap.PushBack(RegisterType(TypeName));
                    }
                }
            }
        }
    }

    if (LayerRemap.Size() > 2)
    {
        int count = LoadedFile->m_FileObjects.Size();
        CKParameter *param;
        int paramtype = m_Context->GetParameterManager()->ParameterGuidToType(CKPGUID_LAYERTYPE);

        CKStateChunk *dataTmp;

        for (int a = 0; a < count; a++)
        {
            // Remap every parameter value if needed
            dataTmp = LoadedFile->m_FileObjects[a].Data;
            if (CKIsChildClassOf(LoadedFile->m_FileObjects[a].ObjectCid, CKCID_PARAMETER))
            {
                if (param = (CKParameter *)LoadedFile->m_FileObjects[a].ObjPtr)
                {

                    if (dataTmp->SeekIdentifier(CK_STATESAVE_PARAMETEROUT_VAL))
                    {
                        CKGUID guidTmp = dataTmp->ReadGuid();
                        if (guidTmp == CKPGUID_LAYERTYPE)
                        {
                            LoadedFile->m_FileObjects[a].Data->RemapParameterInt(CKPGUID_LAYERTYPE, LayerRemap.Begin(), LayerRemap.Size());
                        }
                    }
                }
            }
            else
                // Remap every parameter value that may be stored
                // by initial conditions
                if (CKIsChildClassOf(LoadedFile->m_FileObjects[a].ObjectCid, CKCID_SCENE))
                {
                    if (LoadedFile->m_FileObjects[a].Data)
                        LoadedFile->m_FileObjects[a].Data->RemapParameterInt(CKPGUID_LAYERTYPE, LayerRemap.Begin(), LayerRemap.Size());
                }
        }
    }

    //--- put all the guid of all loaded 'layer type' parameters

    return CK_OK;
}

//----------------------
//----------------------
//----------------------
//----------------------

// {secret}
CKERROR GridManager::PreSave()
{
    m_Remap = new int[m_LayerTypeCount];
    memset(m_Remap, 0, sizeof(int) * m_LayerTypeCount);
    m_RemapCount = 2;

    m_Remap[0] = 0;
    m_Remap[1] = 1;

    return CK_OK;
}

// {secret}
CKStateChunk *GridManager::SaveData(CKFile *SavedFile)
{

    int paramtype = m_Context->GetParameterManager()->ParameterGuidToType(CKPGUID_LAYERTYPE);
    CKParameter *param;
    CKStateChunk *ManagerChunk = NULL;

    if (m_Remap)
    {
        int count = SavedFile->m_FileObjects.Size();
        for (int a = 0; a < count; a++)
        {
            // Remap every parameter value if needed
            if (CKIsChildClassOf(SavedFile->m_FileObjects[a].ObjectCid, CKCID_PARAMETER))
            {
                if (param = (CKParameter *)SavedFile->m_FileObjects[a].ObjPtr)
                {
                    if (param->GetType() == paramtype)
                    {
                        SavedFile->m_FileObjects[a].Data->RemapParameterInt(CKPGUID_LAYERTYPE, m_Remap, m_LayerTypeCount);
                    }
                }
            }
            else
                // Remap every parameter value that may be stored
                // by initial conditions
                if (CKIsChildClassOf(SavedFile->m_FileObjects[a].ObjectCid, CKCID_SCENE) ||
                    CKIsChildClassOf(SavedFile->m_FileObjects[a].ObjectCid, CKCID_LEVEL))
                {
                    if (SavedFile->m_FileObjects[a].Data)
                        SavedFile->m_FileObjects[a].Data->RemapParameterInt(CKPGUID_LAYERTYPE, m_Remap, m_LayerTypeCount);
                }
        }
        if (m_RemapCount > 2)
        {
            // Just an empty chunk so that the manager is called
            // when loading the file
            ManagerChunk = CreateCKStateChunk((CK_CLASSID)0, SavedFile);
            ManagerChunk->StartWrite();
            ManagerChunk->CloseChunk();
        }
        delete[] m_Remap;
    }
    m_Remap = NULL;
    m_RemapCount = 0;

    return ManagerChunk;
}

//----------------------
//----------------------
//----------------------
//----------------------

// {secret}
void GridManager::ClearData()
{
    InternalClearData(TRUE);
}

// {secret}
void GridManager::InternalClearData(CKBOOL unregister)
{

    // --- Unregister Layer Attributes ... ---
    if (unregister)
    {
        CKAttributeManager *am = m_Context->GetAttributeManager();
        for (int i = 1; i < m_LayerTypeCount; i++)
        {
            am->UnRegisterAttribute(m_LayerTypeName[i]);
        }
    }

    while (m_LayerTypeCount--)
    {
        delete[] m_LayerTypeName[m_LayerTypeCount];
        m_LayerTypeName[m_LayerTypeCount] = NULL;
    }

    delete[] m_LayerTypeColor;
    delete[] m_AssociatedParam;

    // --- ... End of unregistering ---
    delete[] m_LayerTypeName;
    delete[] m_LayerAttributeIndex;

    m_LayerTypeColor = NULL;
    m_LayerTypeName = NULL;
    m_AssociatedParam = NULL;
    m_LayerAttributeIndex = NULL;
}

// {secret}
void GridAttributeCallback(int AttribType, CKBOOL Set, CKBeObject *obj, void *arg)
{
    if (Set)
    {	// Attribut is set
        //    ((CKGridManager*)obj)->AddGridObject((CK3dEntity*) obj);
    }
    else
    { // Attribut is unset
    }
}

CKERROR GridManager::OnCKInit()
{
    ClearData();
    InitData();
    m_Context->GetParameterManager()->RegisterNewEnum(CKPGUID_LAYERTYPE, "Layer Type", "*=1");
    XArray<CKGUID> collisionGuids;
    collisionGuids.PushBack(CKPGUID_FILTER);
    collisionGuids.PushBack(CKPGUID_FLOAT);
    collisionGuids.PushBack(CKPGUID_FLOAT);
    m_Context->GetParameterManager()->RegisterNewStructure(CKPGUID_PATHFINDINGCOLLISION, "Collision", "Collision Mask,Collision Radius,Avoid Radius", collisionGuids);

    SetTypeName(1, DEFAULT_LAYER_NAME);

    // We create the attributes
    // we register all the attributes types related to the Grid Manager
    CKAttributeManager *attman = m_Context->GetAttributeManager();

    // first we create the attribute category "Grid" & "Grid Classification"
    attman->AddCategory(GridManagerName, CK_ATTRIBUT_HIDDEN);
    m_GridClassificationCatego = attman->AddCategory(GridClassificationName);
    attman->AddCategory("Grid Path Finding");

    // Grid Attribute
    m_GridAttribute = attman->RegisterNewAttributeType(GridName, CKGUID(0, 0), CKCID_3DENTITY, (CK_ATTRIBUT_FLAGS)(CK_ATTRIBUT_HIDDEN | CK_ATTRIBUT_SYSTEM));
    attman->SetAttributeCategory(m_GridAttribute, GridManagerName);
    // attman->SetAttributeCallbackFunction(m_GridAttribute,GridAttributeCallback,this);

    // Grid path finding attribut
    int newAttr = attman->RegisterNewAttributeType("Path Finding Obstacle", CKPGUID_PATHFINDINGCOLLISION);
    attman->SetAttributeCategory(newAttr, "Grid Path Finding");

    // Grid Classification
    RegisterClassification("- Default Grid Class -");

    // -- END
    return CK_OK;
}

CKERROR GridManager::PostClearAll()
{
    ClearData();
    InitData();

    SetTypeName(1, DEFAULT_LAYER_NAME);

    return CK_OK;
}

CKERROR GridManager::OnCKReset()
{
    if (m_GridPathManager)
        delete m_GridPathManager;
    m_GridPathManager = new GridPathManager(this, m_Context->GetMessageManager(), m_Context->GetAttributeManager());
    return CK_OK;
}

CKERROR GridManager::SequenceToBeDeleted(CK_ID *objids, int count)
{
    if (m_GridPathManager)
        return m_GridPathManager->SequenceToBeDeleted(objids, count);
    return CK_OK;
}

//____________________________________________________
//      OTHER ...

/*************************************************
Name: FillGridWithObjectShape

Summary: Uses entity bounding box to fill grid's layer with a specific value

Arguments:	ent -- Entity used to fill
            type -- Layer's type
            fillVal -- Value to fill with

Return Value:
    none

Remarks:

See also:

*************************************************/
void GridManager::FillGridWithObjectShape(CK3dEntity *ent, int type, void *fillVal)
{
    CKGrid *grid;
    CKLayer *layer;
    VxVector pos, dirX, dirZ, dirY, deb, onRay;
    VxIntersectionDesc interDesc;

    int ax, ay, dx, dy, tmp, i, j;
    float ix, iz;

    // Retrieves necessary informations
    const VxBbox &box = ent->GetBoundingBox();
    if (!box.IsValid())
        return;
    ent->GetPosition(&pos);
    if ((grid = GetPreferredGrid(&pos)) == NULL)
        return;
    if ((layer = grid->GetLayer(type)) == NULL)
        return;

    // Fill
    grid->Get2dCoordsFrom3dPos(&box.Max, &ax, &ay);
    grid->Get2dCoordsFrom3dPos(&box.Min, &dx, &dy);

    if (ax > dx)
    {
        tmp = ax;
        ax = dx;
        dx = tmp;
    }
    if (ay > dy)
    {
        tmp = ay;
        ay = dy;
        dy = tmp;
    }
    if (ax < 0)
        ax = 0;
    else if (ax >= grid->GetWidth())
        ax = grid->GetWidth() - 1;
    if (dx < 0)
        dx = 0;
    else if (dx >= grid->GetWidth())
        dx = grid->GetWidth() - 1;

    for (i = ay, iz = ay + 0.5f; i <= dy; i++, iz += 1)
    {
        for (j = ax, ix = ax + 0.5f; j <= dx; j++, ix += 1)
        {
            pos.Set(ix, 0, iz);
            onRay.Set(ix, 1, iz);

            if (ent->RayIntersection(&pos, &onRay, &interDesc, grid) == TRUE)
                layer->SetValue(j, i, fillVal);
        }
    }
}

/*************************************************
Name: FillPartWithShape

Summary: Uses entity shape to fill grid's layer with a specific value

Arguments:

Return Value:
    none

Remarks:

See also:
FillGridWithObjectShape

*************************************************/

void GridManager::FillPartWithShape(CK3dEntity *ent, CKGrid *grid, Vx2DVector min, Vx2DVector max, int solidLayerType, int shapeLayerType, void *fillVal)
{
    CKLayer *layer;
    VxVector pos, onRay;
    VxIntersectionDesc interDesc;
    static unsigned char table[90000];
    VxBbox virtuaBox;

    int tmp, i, j, width;
    float ix, iz;

    // Scan the part to compute object shape.
    for (i = (int)min.y, iz = min.y + 0.5f, tmp = 0; i <= max.y; i++, iz += 1.0f, tmp++)
    {
        for (j = (int)min.x, ix = min.x + 0.5f; j <= max.x; j++, ix += 1.0f, tmp++)
        {
            table[tmp] = 0;

            pos.Set(ix, 0, iz);
            onRay.Set(ix, 1, iz);
            if ((ent->RayIntersection(&pos, &onRay, &interDesc, grid) == TRUE) && (interDesc.Distance < 1))
                table[tmp] = 1;
        }
    }

    // Modify the 'Solid Layer'
    if (solidLayerType != -1)
    {
        layer = grid->GetLayer(solidLayerType);
        if (layer != NULL)
        {
            for (i = (int)min.y, tmp = 0; i <= max.y; i++, tmp++)
            {
                for (j = (int)min.x; j <= max.x; j++, tmp++)
                    if (table[tmp] == 1)
                        layer->SetValue(j, i, fillVal);
            }
        }
    }

    // Modify the 'Shape Layer'
    if (shapeLayerType != -1)
    {
        layer = grid->GetLayer(shapeLayerType);
        if (layer != NULL)
        {
            width = (int)(max.x - min.x + 1);
            for (i = (int)min.y, tmp = 0; i <= max.y; tmp++, i++)
            {
                for (j = (int)min.x; j <= max.x; tmp++, j++)
                {
                    if (table[tmp] == 0)
                    {
                        if (((j < max.x) && (table[tmp + 1] == 1)) ||
                            ((j > min.x) && (table[tmp - 1] == 1)))
                            layer->SetValue(j, i, fillVal);
                        if (((i < max.y) && (table[tmp + width + 1] == 1)) ||
                            ((i > min.y) && (table[tmp - width - 1] == 1)))
                            layer->SetValue(j, i, fillVal);
                    }
                }
            }
        }
    }
}

/*************************************************
Name: FillGridWithObjectShape

Summary: Uses entity shape to fill grid's layer with a specific value

Arguments:	ent -- Entity used to fill
            solidLayerType -- Solid Layer's type
            shapeLayerType -- Shape  "        "
            fillVal -- Value to fill with

Return Value:
    none

Remarks:

See also:

*************************************************/

void GridManager::FillGridWithObjectShape(CK3dEntity *ent, int solidLayerType, int shapeLayerType, void *fillVal)
{
    VxBbox gbox;
    CKGrid *grid;
    VxVector boundVector;

    Vx2DVector min, max;

    float vx[2], vy[2], vz[2];

    int i, tx, ty, x, y, z, width, length, gridcount;

    // Retrieves necessary informations
    const VxBbox &box = ent->GetBoundingBox();
    if (!box.IsValid())
        return;

    vx[0] = box.Min.x;
    vy[0] = box.Min.y;
    vz[0] = box.Min.z;
    vx[1] = box.Max.x;
    vy[1] = box.Max.y;
    vz[1] = box.Max.z;

    // Select Implied Grids
    gridcount = GetGridObjectCount();
    for (i = 0; i < gridcount; i++)
    {
        grid = GetGridObject(i);
        const VxBbox &gbox = grid->GetBoundingBox();
        if ((grid->IsActive() == TRUE) && (box.IsValid()) && (VxIntersect::AABBAABB(box, gbox) == TRUE))
        {
            max.x = max.y = 0.0f;
            min.y = (float)grid->GetLength();
            min.x = (float)grid->GetWidth();

            // Bounding Box Scan
            for (x = 0; x < 2; x++)
            {
                for (y = 0; y < 2; y++)
                {
                    for (z = 0; z < 2; z++)
                    {
                        boundVector.Set(vx[x], vy[y], vz[z]);
                        grid->Get2dCoordsFrom3dPos(&boundVector, &tx, &ty);

                        if (tx < (int)min.x)
                            min.x = (float)tx;
                        if (tx > (int)max.x)
                            max.x = (float)tx;

                        if (ty < (int)min.y)
                            min.y = (float)ty;
                        if (ty > (int)max.y)
                            max.y = (float)ty;
                    }
                }
            }
            width = grid->GetWidth();
            length = grid->GetLength();

            min.x--;
            min.y--;
            max.x += 2;
            max.y += 2;

            if (min.x < 0)
                min.x = 0;
            else if (min.x >= width)
                min.x = (float)(width - 1);

            if (max.x < 0)
                max.x = 0;
            else if (max.x >= width)
                max.x = (float)(width - 1);

            if (min.y < 0)
                min.y = 0;
            else if (min.y >= length)
                min.y = (float)(length - 1);

            if (max.y < 0)
                max.y = 0;
            else if (max.y >= length)
                max.y = (float)(length - 1);

            FillPartWithShape(ent, grid, min, max, solidLayerType, shapeLayerType, fillVal);
        }
    }
}

void GridManager::FillBoundingBox(const VxBbox &box, int solidLayerType, void *fillVal)
{
    CKGrid *grid;
    VxVector boundVector;
    CKRECT r;
    float vx[2], vy[2], vz[2];
    int i, tx, ty, x, y, z, width, length, gridcount;

    // Retrieves necessary informations
    if (!box.IsValid())
        return;

    vx[0] = box.Min.x;
    vy[0] = box.Min.y;
    vz[0] = box.Min.z;
    vx[1] = box.Max.x;
    vy[1] = box.Max.y;
    vz[1] = box.Max.z;

    // Select Implied Grids
    gridcount = GetGridObjectCount();
    for (i = 0; i < gridcount; i++)
    {
        grid = GetGridObject(i);
        const VxBbox &gbox = grid->GetBoundingBox();
        if ((grid->IsActive() == TRUE) && (box.IsValid()) && (VxIntersect::AABBAABB(box, gbox) == TRUE))
        {
            r.right = 0;
            r.top = grid->GetLength() - 1;
            r.left = grid->GetWidth() - 1;
            r.bottom = 0;

            // Bounding Box Scan
            for (x = 0; x < 2; x++)
            {
                for (y = 0; y < 2; y++)
                {
                    for (z = 0; z < 2; z++)
                    {
                        boundVector.Set(vx[x], vy[y], vz[z]);
                        grid->Get2dCoordsFrom3dPos(&boundVector, &tx, &ty);

                        if (tx < r.left)
                            r.left = tx;
                        if (tx > r.right)
                            r.right = tx;

                        if (ty < r.top)
                            r.top = ty;
                        if (ty > r.bottom)
                            r.bottom = ty;
                    }
                }
            }
            width = grid->GetWidth();
            length = grid->GetLength();

            if (r.left < 0)
                r.left = 0;
            else if (r.left >= width)
                r.left = (width - 1);

            if (r.right < 0)
                r.right = 0;
            else if (r.right >= width)
                r.right = (width - 1);

            if (r.top < 0)
                r.top = 0;
            else if (r.top >= length)
                r.top = (length - 1);

            if (r.bottom < 0)
                r.bottom = 0;
            else if (r.bottom >= length)
                r.bottom = (length - 1);

            FillPartWithRect(grid, r, solidLayerType, fillVal);
        }
    }
}

void GridManager::FillPartWithRect(CKGrid *grid, const CKRECT &r, int LayerType, void *fillVal)
{
    CKLayer *layer;
    // Modify the 'Layer'
    if (LayerType != -1)
    {
        layer = grid->GetLayer(LayerType);
        if (layer != NULL)
        {
            for (int i = r.left; i <= r.right; ++i)
            {
                for (int j = r.top; j <= r.bottom; ++j)
                {
                    layer->SetValue(i, j, fillVal);
                }
            }
        }
    }
}

//____________________________________________________
//	  GRID PATH FINDING
GridPathManager *GridManager::GetGridPathManager()
{
    return m_GridPathManager;
}
