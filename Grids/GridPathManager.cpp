#include "CKAll.h"
#include <math.h>
#include "GridManager.h"
#include "GridPathManager.h"
#include "OpenList.h"
#include "NodeLinker.h"
#include "NodeLinker.h"

const float GridPathManager::m_SuccesorIndexCost[12] =
    {
        DISTANCE_CASE_DIAGONAL, DISTANCE_CASE_JUXTAPOSE,
        DISTANCE_CASE_DIAGONAL, DISTANCE_CASE_JUXTAPOSE,
        DISTANCE_CASE_DIAGONAL, DISTANCE_CASE_JUXTAPOSE,
        DISTANCE_CASE_DIAGONAL, DISTANCE_CASE_JUXTAPOSE,
        DISTANCE_CASE_DIAGONAL, DISTANCE_CASE_JUXTAPOSE,
        DISTANCE_CASE_DIAGONAL, DISTANCE_CASE_JUXTAPOSE};

const float GridPathManager::m_SuccesorIndexCost2[6] =
    {
        DISTANCE_CASE_JUXTAPOSE,
        DISTANCE_CASE_JUXTAPOSE,
        DISTANCE_CASE_JUXTAPOSE,
        DISTANCE_CASE_JUXTAPOSE,
        DISTANCE_CASE_JUXTAPOSE,
        DISTANCE_CASE_JUXTAPOSE};

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// GridPathManager
/////////////////////////////////////////////////////////////////////////////////////////////////////////
GridPathManager::GridPathManager(CKGridManager *gridManager, CKMessageManager *messageManager, CKAttributeManager *attributManager)
    : m_ArrayNodeGrid(0),
      m_LayerID2Index(0),
      m_ArrayPathProblem(0),
      m_NumPathProblem(0),
      m_NumFollowProblem(0),
      m_MaxCase(0),
      m_FollowPathID(0),
      m_GraphDone(FALSE)
{
    m_GridManager = gridManager;
    m_MessageManager = messageManager;
    m_AttributManager = attributManager;
    m_MaxPathProblem = DEFAUL_MAX_PATH_CONTEXT;
    m_MaxFollowProblem = DEFAUL_MAX_FOLLOW_CONTEXT;
    m_PFCAttrType = m_AttributManager->GetAttributeTypeByName("Path Finding Obstacle");
    m_Reseted = FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// ~GridPathManager
/////////////////////////////////////////////////////////////////////////////////////////////////////////
GridPathManager::~GridPathManager()
{
    int i;
    /***********************/
    /* Path finding delete */
    /***********************/

    // Delete all the linker.
    for (NodeLinkerIt it = m_ListNodeLinker.Begin(); it != m_ListNodeLinker.End(); it++)
    {
        if (*it)
        {
            delete *it;
        }
    }
    if (m_LayerID2Index)
    {
        delete[] m_LayerID2Index;
    }

    // Delete the all path problem
    for (i = 0; i < m_ArrayPathProblem.Size(); ++i)
    {
        delete m_ArrayPathProblem[i];
    }

    if (m_ArrayNodeGrid)
    {
        delete[] m_ArrayNodeGrid;
    }

    // Delete all gridinfo
    for (Grid2GridInfoIt itGrid = m_ListGridInfo.Begin(); itGrid != m_ListGridInfo.End(); itGrid++)
    {
        if (*itGrid)
        {
            delete *itGrid;
        }
    }

    /**********************/
    /* Path follow delete */
    /**********************/

    // Delete stayed path
    for (PathID2PathIt itPath = m_PathID2Path.Begin(); itPath != m_PathID2Path.End(); itPath++)
    {
        delete *itPath;
    }

    // Delete follow problem.
    for (i = 0; i < m_ArrayFollowProblem.Size(); ++i)
    {
        delete m_ArrayFollowProblem[i];
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Reset
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void GridPathManager::Reset()
{
    if (m_Reseted)
        return;
    /****************/
    /* Path finding */
    /****************/

    NodeLinkerIt it;
    Grid2GridInfoIt itGrid;

    // Delete all the linker.
    for (it = m_ListNodeLinker.Begin(); it != m_ListNodeLinker.End(); it++)
        if (*it)
            delete *it;
    m_ListNodeLinker.Clear();

    if (m_LayerID2Index)
    {
        delete[] m_LayerID2Index;
        m_LayerID2Index = 0;
    }

    // Delete the all path problem
    int i;
    for (i = 0; i < m_ArrayPathProblem.Size(); ++i)
        delete m_ArrayPathProblem[i];

    if (m_ArrayNodeGrid)
    {
        delete[] m_ArrayNodeGrid;
        m_ArrayNodeGrid = 0;
    }

    // Delete all gridinfo
    for (itGrid = m_ListGridInfo.Begin(); itGrid != m_ListGridInfo.End(); itGrid++)
        if (*itGrid)
            delete *itGrid;

    m_ListGridInfo.Clear();
    m_StackFreePathIndex.Clear();

    /**********************/
    /* Path follow delete */
    /**********************/

    // Delete stayed path
    for (PathID2PathIt itPath = m_PathID2Path.Begin(); itPath != m_PathID2Path.End(); itPath++)
    {
        delete *itPath;
    }
    m_PathID2Path.Clear();

    // Delete follow problem.
    for (i = 0; i < m_ArrayFollowProblem.Size(); ++i)
    {
        delete m_ArrayFollowProblem[i];
    }
    m_ArrayPathProblem.Clear();
    m_ArrayFollowProblem.Clear();
    m_Target2PathContext.Clear();
    m_PathID2FollowContext.Clear();
    m_StackFreeFollowIndex.Clear();
    m_NumPathProblem = 0;
    m_NumFollowProblem = 0;
    m_MaxPathProblem = DEFAUL_MAX_PATH_CONTEXT;
    m_MaxFollowProblem = DEFAUL_MAX_FOLLOW_CONTEXT;
    m_GraphDone = FALSE;

    m_Reseted = TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// ConstructListNodeLinker
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CKBOOL GridPathManager::ConstructListNodeLinker(XList<int> *listLayer)
{
    int i, j, x, y;
    const XObjectPointerArray &ArrayGrid = m_GridManager->GetGridArray();
    int nbGrid = 0;
    CKGrid *grid;
    CKLayer *layer;
    int val;
    VxVector pos;
    NodeLinker *nodeLinker;
    GridInfo *gridInfo;
    int w, l;
    int numLayer = 0;
    int numGrid = 0;

    if (!ArrayGrid.Size())
        return FALSE;

    // In this part, we create and add all the 'concerned' linkers in a list.
    for (i = 0; i < ArrayGrid.Size(); ++i)
    {
        grid = (CKGrid *)ArrayGrid[i];

        // Does the grid is a valid one ?
        if (GridContainOneLayer(grid, listLayer))
        {
            // Create the grid's grid info
            gridInfo = new GridInfo(grid);
            gridInfo->m_Num = numGrid++;
            m_ListGridInfo.Insert((uintptr_t)grid, gridInfo, FALSE);

            w = grid->GetWidth();
            l = grid->GetLength();
            if (w * l > m_MaxCase)
                m_MaxCase = w * l;

            for (j = 0; j < m_GridManager->GetLayerTypeCount(); ++j)
                if (m_GridManager->GetAssociatedParam(j) == CKPGUID_LINKERGRAPH_ENUM)
                    if ((layer = grid->GetLayer(j)))
                    {
                        for (x = 0; x < w; ++x)
                            for (y = 0; y < l; ++y)
                            {
                                layer->GetValue(x, y, &val);
                                if (val)
                                {
                                    // Create a NodeLinker.
                                    nodeLinker = new NodeLinker(m_MaxPathProblem, listLayer->Size());

                                    // Fill the hash table m_Linker2Attribut
                                    nodeLinker->m_Attribut = m_GridManager->m_Context->GetAttributeManager()->GetAttributeTypeByName(layer->GetName());
                                    nodeLinker->m_Grid = grid;

                                    switch (val)
                                    {
                                    case 1:
                                        nodeLinker->m_TypeCase = tc_tele1;
                                        break;
                                    case 2:
                                        nodeLinker->m_TypeCase = tc_tele2;
                                        break;
                                    case 3:
                                        nodeLinker->m_TypeCase = tc_tele3;
                                        break;
                                    case 4:
                                        nodeLinker->m_TypeCase = tc_door;
                                        break;
                                    default:
                                        nodeLinker->m_TypeCase = tc_door;
                                        break;
                                    }
                                    nodeLinker->m_LinkerID = j;
                                    nodeLinker->m_X = x;
                                    nodeLinker->m_Y = y;

                                    // If the linker is a door we need its 3d position.
                                    if (val == 4)
                                    {
                                        grid->Get3dPosFrom2dCoords(&pos, x, y);
                                        nodeLinker->SetPos(pos);
                                    }
                                    gridInfo->m_ListNodeLinker.PushBack(nodeLinker);
                                    m_ListNodeLinker.PushBack(nodeLinker);
                                }
                            }
                    }
        }
    }

    // Create m_LayerID2Index
    int maxLayer = 0;
    for (IntIt itl = listLayer->Begin(); itl != listLayer->End(); itl++)
        if (maxLayer < *itl)
            maxLayer = *itl;
    m_LayerID2Index = new int[maxLayer + 1];

    // Create grid A* search data.
    m_ArrayNodeGrid = new NodeGrid[m_MaxCase];
    for (i = 0; i < m_MaxCase; ++i)
        m_ArrayNodeGrid[i].Set(m_MaxPathProblem, i, numGrid);

    // Fill "LinkerNode as obstacle" info.
    int index;
    NodeLinker *node;
    for (Grid2GridInfoIt it = m_ListGridInfo.Begin(); it != m_ListGridInfo.End(); it++)
        for (NodeLinkerIt it2 = (*it)->m_ListNodeLinker.Begin(); it2 != (*it)->m_ListNodeLinker.End(); it2++)
        {
            node = (*it2);
            index = node->m_Y * node->m_Grid->GetWidth() + node->m_X;
            m_ArrayNodeGrid[index].m_LinkerObs[(*it)->m_Num] = node;
        }

    // In this part, we connect linker each other.
    NodeLinkerIt it1;
    NodeLinkerIt it2;
    NodeLinker *linker1;
    NodeLinker *linker2;
    for (it1 = m_ListNodeLinker.Begin(); it1 != m_ListNodeLinker.End(); it1++)
    {
        linker1 = *it1;
        for (it2 = m_ListNodeLinker.Begin(); it2 != m_ListNodeLinker.End(); it2++)
        {
            linker2 = *it2;
            if (linker1 == linker2)
                continue;
            if (linker1->m_LinkerID == linker2->m_LinkerID)
            {
                // Linker are connected.
                if ((linker1->m_TypeCase == tc_door && linker2->m_TypeCase == tc_door) ||
                    ((linker1->m_TypeCase == tc_tele1 || linker1->m_TypeCase == tc_tele3) && (linker2->m_TypeCase == tc_tele2 || linker2->m_TypeCase == tc_tele3)))
                    linker1->m_ListConnected.PushBack(linker2);
            }
        }

        // For each obstacles layer, we will see if linkers which are in the same grid,
        // but which are not connected, can have a path each other.
        numLayer = 0;
        for (IntIt it = listLayer->Begin(); it != listLayer->End(); it++)
        {
            GridContext *contextInfo;
            for (it2 = it1; it2 != m_ListNodeLinker.End(); it2++)
            {
                linker2 = *it2;
                if (linker1->m_LinkerID != linker2->m_LinkerID && linker1->m_Grid == linker2->m_Grid)
                {
                    // Instant A star.
                    ResetArrayContextInfo(0, linker1->m_Grid->GetWidth() * linker1->m_Grid->GetLength());
                    int maxLayerValue;
                    contextInfo = CasesDistance(linker1->m_Grid, *it, linker1->m_X, linker1->m_Y, linker2->m_X, linker2->m_Y, maxLayerValue);
                    if (contextInfo)
                    {
                        IndoorLink *indooLink1 = &linker1->m_ArrayIndoor[numLayer];
                        indooLink1->m_LayerID = *it;
                        indooLink1->m_ListSameGrid.PushBack(linker2);
                        indooLink1->m_ListSameGridCoast.PushBack(contextInfo->m_Cost);
                        indooLink1->m_ListSameGridLayerCoast.PushBack((int)contextInfo->m_LayerCost);
                        indooLink1->m_ListSameGridMaxLayerCoast.PushBack(maxLayerValue);

                        IndoorLink *indooLink2 = &linker2->m_ArrayIndoor[numLayer];
                        indooLink2->m_LayerID = *it;
                        indooLink2->m_ListSameGrid.PushBack(linker1);
                        indooLink2->m_ListSameGridCoast.PushBack(contextInfo->m_Cost);
                        indooLink2->m_ListSameGridLayerCoast.PushBack((int)contextInfo->m_LayerCost);
                        indooLink2->m_ListSameGridMaxLayerCoast.PushBack(maxLayerValue);
                    }
                }
            }
            // Init the conversion table.
            m_LayerID2Index[*it] = numLayer;
            ++numLayer;
        }
    }
    if (!m_ListNodeLinker.Size())
        for (IntIt it = listLayer->Begin(); it != listLayer->End(); it++)
            // Init the conversion table.
            m_LayerID2Index[*it] = numLayer++;
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// ConstructGraph
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CKBOOL GridPathManager::ConstructGraph(XList<int> *listLayer)
{
    XList<int> listLayer2;
    CKBOOL listLinkerOk;
    int i, j;

    // If a graph was allready done erase it.
    Reset();

    if (!listLayer)
    {
        // If list layer is empty all the layer are considered.
        for (j = 0; j < m_GridManager->GetLayerTypeCount(); ++j)
            if (m_GridManager->GetAssociatedParam(j) != CKPGUID_LINKERGRAPH_ENUM)
                listLayer2.PushBack(j);
        listLinkerOk = ConstructListNodeLinker(&listLayer2);
    }
    else
        listLinkerOk = ConstructListNodeLinker(listLayer);

    if (listLinkerOk)
    {
        // Build m_ArrayPathProblem
        m_ArrayPathProblem.Resize(DEFAUL_MAX_PATH_CONTEXT);

        for (i = 0; i < DEFAUL_MAX_PATH_CONTEXT; ++i)
        {
            m_ArrayPathProblem[i] = new PathProblem();

            // Just to have acces to public attributs...
            m_ArrayPathProblem[i]->m_GridPathManager = this;

            // Set each open list.
            m_ArrayPathProblem[i]->SetOpenList(i, (int)(m_ListNodeLinker.Size() * REALTIME_OPENLIST_BUFFER_FACTOR), (int)(m_MaxCase * REALTIME_OPENLIST_BUFFER_FACTOR));

            // Fill StackFreeIndex.
            m_StackFreePathIndex.PushBack(i);

            // Build NodeGrid goal and end.
            if (listLayer)
                m_NumLayer = listLayer->Size();
            else
                m_NumLayer = listLayer2.Size();
            m_ArrayPathProblem[i]->m_NodeLinkerDynStart = new NodeLinker(m_MaxPathProblem, m_NumLayer);
            m_ArrayPathProblem[i]->m_NodeLinkerDynEnd = new NodeLinker(m_MaxPathProblem, m_NumLayer);
        }

        // Follow data construction.
        ConstructFollowData();

        m_GraphDone = TRUE;
        m_Reseted = FALSE;
        return TRUE;
    }
    return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// GetTargetStatus
/////////////////////////////////////////////////////////////////////////////////////////////////////////
int GridPathManager::GetTargetStatus(CK3dEntity *target)
{
    int *contextPtr;

    if (!(contextPtr = m_Target2PathContext.FindPtr((uintptr_t)target)))
        return 0;
    return m_ArrayPathProblem[*contextPtr]->m_State;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// RegisterPathProblem
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CKBOOL GridPathManager::RegisterPathProblem(CK3dEntity *target, VxVector *targetPos, CK3dEntity *goalRef, VxVector *goalPos, int obstacleLayer, int obstacleThreshold, float slowingFactor, CKBOOL linker, CKBOOL linkerObs, float heuristicCoef, float timeFrame, int heuristic, CKBOOL diagonal, CKBOOL optimize)
{
    int pathID;

    // The graph is build with the firt problem registration.

    if (!m_GraphDone && !ConstructGraph(0))
        return FALSE;

    // Get a free index.
    if (m_StackFreePathIndex.Size())
    {
        pathID = *(m_StackFreePathIndex.Begin());
        m_StackFreePathIndex.PopFront();
    }
    else
    {
        // Resize all parallel context.
        pathID = m_MaxPathProblem++;
        ResizePathContextArray(m_MaxPathProblem);
    }

    ++m_NumPathProblem;
    m_ArrayPathProblem[pathID]->Set(target, targetPos, goalRef, goalPos, obstacleLayer, obstacleThreshold, slowingFactor, linker, linkerObs, heuristicCoef, timeFrame, heuristic, diagonal, optimize, STATE_GETSTARTENDGRIDSID);
    m_Target2PathContext.Insert((uintptr_t)target, pathID, FALSE);
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// ResizePathContextArray
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void GridPathManager::ResizePathContextArray(int numberContext)
{
    int oldNumberContext = m_ArrayPathProblem.Size();
    int i;

    m_ArrayPathProblem.Resize(numberContext);
    for (i = 0; i < oldNumberContext; ++i)
    {
        m_ArrayPathProblem[i]->m_NodeLinkerDynStart->ResizeParallelContext(numberContext);
        m_ArrayPathProblem[i]->m_NodeLinkerDynEnd->ResizeParallelContext(numberContext);
    }
    if (numberContext > oldNumberContext)
        for (i = oldNumberContext; i < numberContext; ++i)
        {
            PathProblem *pathProblem = new PathProblem();

            // Just to have acces to public attributs...
            pathProblem->m_GridPathManager = this;

            // Set each open list.
            pathProblem->SetOpenList(i, (int)(m_ListNodeLinker.Size() * REALTIME_OPENLIST_BUFFER_FACTOR), (int)(m_MaxCase * REALTIME_OPENLIST_BUFFER_FACTOR));

            // Build NodeGrid goal and end.
            pathProblem->m_NodeLinkerDynStart = new NodeLinker(m_MaxPathProblem, m_NumLayer);
            pathProblem->m_NodeLinkerDynEnd = new NodeLinker(m_MaxPathProblem, m_NumLayer);

            m_ArrayPathProblem[i] = pathProblem;
        }
    for (NodeLinkerIt NodeIt = m_ListNodeLinker.Begin(); NodeIt != m_ListNodeLinker.End(); NodeIt++)
        (*NodeIt)->ResizeParallelContext(numberContext);
    for (i = 0; i < m_MaxCase; ++i)
        m_ArrayNodeGrid[i].ResizeParallelContext(numberContext);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// UnregisterPathProblem
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CKBOOL GridPathManager::UnregisterPathProblem(CK3dEntity *target, CKBOOL giveToFollower)
{
    PathProblem *pathProblem;
    int *pathContextPtr;
    int pathContext;

    if (!(pathContextPtr = m_Target2PathContext.FindPtr((uintptr_t)target)))
        return FALSE;
    pathContext = *pathContextPtr;
    m_Target2PathContext.Remove((uintptr_t)target);

    pathProblem = m_ArrayPathProblem[pathContext];
    if (giveToFollower)
    {
        Path *newPath = new Path();
        newPath->CopyAndErase(pathProblem->m_Path);
        newPath->m_ObstacleLayer = pathProblem->m_ObstacleLayer;
        newPath->m_ObstacleThreshold = pathProblem->m_ObstacleThreshold;

        // Debug
        for (int i = 0; i < newPath->NbSubPath(); ++i)
            newPath->m_ArraySubPath[i]->m_PathFID = pathProblem->m_FollowPathID;

        m_PathID2Path.Insert(pathProblem->m_FollowPathID, newPath, FALSE);
    }

    // Reset the path problem.
    pathProblem->Reset();

    // Clear the dyn nodelinker of all the linker for the context.
    for (NodeLinkerIt it = m_ListNodeLinker.Begin(); it != m_ListNodeLinker.End(); it++)
        (*it)->m_DynNodeLinker[pathContext] = 0;

    // Free the index context.
    m_StackFreePathIndex.PushBack(pathContext);

    --m_NumPathProblem;
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// GetStartEndGridID
/////////////////////////////////////////////////////////////////////////////////////////////////////////
int GetStartEndGridID(PathProblem &pathProblem, int context)
{
    CKGrid *gridStart;
    CKGrid *gridEnd;
    GridPathManager *gridManager = pathProblem.m_GridPathManager;

    if (!(gridStart = gridManager->m_GridManager->GetPreferredGrid(&pathProblem.m_TargetPos)))
    {
        pathProblem.m_StateFunction = STATE_PATHNOTFOUND;
        return 0;
    }
    if (gridManager->m_ListGridInfo.FindPtr((uintptr_t)gridStart))
    {
        if (!(gridEnd = gridManager->m_GridManager->GetPreferredGrid(&pathProblem.m_GoalPos)))
        {
            pathProblem.m_StateFunction = STATE_PATHNOTFOUND;
            return 0;
        }
        if (gridManager->m_ListGridInfo.FindPtr((uintptr_t)gridEnd))
        {
            NodeLinker *nodeStart = pathProblem.m_NodeLinkerDynStart;
            NodeLinker *nodeEnd = pathProblem.m_NodeLinkerDynEnd;
            pathProblem.m_GridStart = gridStart;
            pathProblem.m_GridEnd = gridEnd;

            // Init the dyn NodeLinkers
            nodeStart->ClearConnected();
            nodeStart->ClearDynConnected();
            nodeStart->m_Grid = gridStart;
            nodeStart->m_LinkerID = -1;
            gridStart->Get2dCoordsFrom3dPos(&pathProblem.m_TargetPos, &nodeStart->m_X, &nodeStart->m_Y);

            nodeEnd->ClearConnected();
            nodeEnd->ClearDynConnected();
            nodeEnd->m_Grid = gridEnd;
            nodeEnd->m_LinkerID = -2;
            gridEnd->Get2dCoordsFrom3dPos(&pathProblem.m_GoalPos, &nodeEnd->m_X, &nodeEnd->m_Y);

            // If start == end path is not found...
            if (nodeStart->m_X == nodeEnd->m_X && nodeStart->m_Y == nodeEnd->m_Y)
            {
                pathProblem.m_StateFunction = STATE_PATHNOTFOUND;
                return 0;
            }

            int val;
            CKLayer *layer;

            // If goal is on a obstacle path is not found.
            if (pathProblem.m_LinkerObs && gridManager->CaseIsLinker(pathProblem, gridEnd, nodeEnd->m_X + nodeEnd->m_Y * gridEnd->GetWidth()))
            {
                pathProblem.m_StateFunction = STATE_PATHNOTFOUND;
                return 0;
            }
            if ((layer = gridEnd->GetLayer(pathProblem.m_ObstacleLayer)))
            {
                layer->GetValue(nodeEnd->m_X, nodeEnd->m_Y, &val);
                if (val > pathProblem.m_ObstacleThreshold)
                {
                    pathProblem.m_StateFunction = STATE_PATHNOTFOUND;
                    return 0;
                }
            }

            // If target dont use linkers or if there is no linkers...
            // Or if target as no attribut (it is not allow to take linkers).
            if (!pathProblem.m_Linker || !gridManager->m_ListNodeLinker.Size() || !pathProblem.m_Target->GetAttributeCount())
            {
                if (gridStart == gridEnd)
                {
                    // ...we want find a direct path.
                    pathProblem.m_StateFunction = STATE_DIRECTPATH;
                    return 1;
                }
                pathProblem.m_StateFunction = STATE_PATHNOTFOUND;
                return 0;
            }
            pathProblem.m_StateFunction = STATE_BEFORESTARTLINKER;
            return 1;
        }
    }
    pathProblem.m_StateFunction = STATE_PATHNOTFOUND;
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// BeforeStartLinker
// Warning :This function is time/frame based.
/////////////////////////////////////////////////////////////////////////////////////////////////////////
int BeforeStartLinker(PathProblem &pathProblem, int context)
{
    GridPathManager *gridManager = pathProblem.m_GridPathManager;
    do
    {
        NodeLinker *nodeStart = pathProblem.m_NodeLinkerDynStart;
        NodeLinker *nodeEnd = pathProblem.m_NodeLinkerDynEnd;

        if (!pathProblem.m_StartPartSearch && pathProblem.m_GridStart == pathProblem.m_GridEnd)
        {
            pathProblem.m_StartPartSearch = TRUE;

            pathProblem.InitBeforeCase2Case(pathProblem.m_GridStart, &gridManager->m_ListGridInfo);

            // Refresh the context info.
            gridManager->ResetArrayContextInfo(context, pathProblem.m_GridW * pathProblem.m_GridL);

            // Set the linkers.
            pathProblem.SetLinker(NULL, NULL);

            // We inverse the path problem (to avoid to inverse it later...)
            // The dyn start linker is the start node grid.
            NodeGrid *nodeGrid = &gridManager->m_ArrayNodeGrid[nodeEnd->m_Y * nodeEnd->m_Grid->GetWidth() + nodeEnd->m_X];
            GridContext *contextInfo = nodeGrid->m_ArrayContextInfo[context];
            contextInfo->m_Cost = 0;
            contextInfo->m_F = sqrtf((float)((nodeEnd->m_X - nodeStart->m_X) * (nodeEnd->m_X - nodeStart->m_X) + (nodeEnd->m_Y - nodeStart->m_Y) * (nodeEnd->m_Y - nodeStart->m_Y)));
            contextInfo->m_Parent = 0;

            // Init the open list.
            pathProblem.m_OpenListGrid->Push(nodeGrid);

            // The dyn en linker is the goal node grid.
            pathProblem.m_XEndSearch = nodeStart->m_X;
            pathProblem.m_YEndSearch = nodeStart->m_Y;
            pathProblem.m_EndIndex = nodeStart->m_Y * nodeStart->m_Grid->GetWidth() + nodeStart->m_X;

            // Ready for next state.
            pathProblem.m_NodeGrideEnd = 0;
            // pathProblem.m_StateFunction = STATE_AFTERENDLINKER;
            return 1;
        }

        if (pathProblem.m_GridStart == pathProblem.m_GridEnd)
        {
            // Search.
            if (!GetCase2CasePath(pathProblem, context))
            {
                pathProblem.m_StartPartSearch = FALSE;

                // Ready for next state.
                pathProblem.m_StateFunction = STATE_GETLINKERFROMSTART;
                return 1;
            }
            else if (pathProblem.m_NodeGrideEnd)
            {
                IndoorLink *indoorLink = &nodeStart->m_ArrayIndoor[gridManager->m_LayerID2Index[pathProblem.m_ObstacleLayer]];
                indoorLink->m_ListSameGrid.PushBack(nodeEnd);
                indoorLink->m_ListSameGridCoast.PushBack(pathProblem.m_NodeGrideEnd->m_ArrayContextInfo[context]->m_Cost);
                indoorLink->m_ListSameGridLayerCoast.PushBack(0);
                indoorLink->m_ListSameGridMaxLayerCoast.PushBack(0);

                // Save the path found.
                pathProblem.m_HypSubPathStart2End = new SubPath(pathProblem.m_GridSearch, pathProblem.m_NodeGrideEnd->m_ArrayContextInfo[context]->m_Cost);
                pathProblem.BuildSubPath(context, pathProblem.m_HypSubPathStart2End);
                pathProblem.m_HypSubPathStart2EndCoast = pathProblem.m_HypSubPathStart2End->m_Coast;

                pathProblem.m_StartPartSearch = FALSE;

                // Ready for next state.
                pathProblem.m_StateFunction = STATE_GETLINKERFROMSTART;
                return 1;
            }
        }
        else
        {
            pathProblem.m_StartPartSearch = FALSE;

            // Ready for next state.
            pathProblem.m_StateFunction = STATE_GETLINKERFROMSTART;
            return 1;
        }
    } while (pathProblem.TimeNotOut());
    return 1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// GetLinkerFromStart
// Warning :This function is time/frame based.
/////////////////////////////////////////////////////////////////////////////////////////////////////////
int GetLinkerFromStart(PathProblem &pathProblem, int context)
{
    GridPathManager *gridManager = pathProblem.m_GridPathManager;
    do
    {
        // First time we enter in GetLinkerFromStart function.
        if (!pathProblem.m_StartPartSearch)
        {
            GridInfo *gridInfo = *gridManager->m_ListGridInfo.FindPtr((uintptr_t)pathProblem.m_GridStart);
            pathProblem.m_ItLinkerStart = gridInfo->m_ListNodeLinker.Begin();
            pathProblem.m_ItLinkerEnd = gridInfo->m_ListNodeLinker.End();

            // The search grid is the startGrid.
            pathProblem.m_GridSearch = pathProblem.m_GridStart;

            // The start case is the target position
            pathProblem.m_GridStart->Get2dCoordsFrom3dPos(&pathProblem.m_TargetPos, &pathProblem.m_XStartSearch, &pathProblem.m_YStartSearch);

            pathProblem.m_NodeGrideEnd = (NodeGrid *)1;
            pathProblem.m_StartPartSearch = TRUE;
        }

        // If solution is not found
        if (!pathProblem.m_NodeGrideEnd)
        {
            // We search...
            if (!GetCase2CasePath(pathProblem, context))
                // Path not found.
                pathProblem.m_NodeGrideEnd = (NodeGrid *)1;
            else if (pathProblem.m_NodeGrideEnd)
            {
                // Path is found.
                // Save the path.
                SubPath *subPath = new SubPath(pathProblem.m_GridSearch, pathProblem.m_NodeGrideEnd->m_ArrayContextInfo[context]->m_Cost);
                pathProblem.BuildSubPath(context, subPath, FALSE);
                pathProblem.m_HypSubPathStart.PushBack(subPath);

                // Update connectivity between dyn nodeLinker and current linker.
                NodeLinkerIt itLinker = pathProblem.m_ItLinkerStart;
                itLinker--;
                IndoorLink *indoorLink = &pathProblem.m_NodeLinkerDynStart->m_ArrayIndoor[gridManager->m_LayerID2Index[pathProblem.m_ObstacleLayer]];
                indoorLink->m_ListSameGrid.PushBack(*itLinker);
                indoorLink->m_ListSameGridCoast.PushBack(pathProblem.m_NodeGrideEnd->m_ArrayContextInfo[context]->m_Cost);
                indoorLink->m_ListSameGridLayerCoast.PushBack(0);
                indoorLink->m_ListSameGridMaxLayerCoast.PushBack(0);
            }
        }
        else
        {
            if (pathProblem.m_ItLinkerStart != pathProblem.m_ItLinkerEnd || !pathProblem.m_NodeGrideEnd)
            {
                // Test if the target have access to linker
                while (!pathProblem.Linker2StartIsGood())
                {
                    pathProblem.m_ItLinkerStart++;
                    if (pathProblem.m_ItLinkerStart == pathProblem.m_ItLinkerEnd)
                        break;
                }
            }

            // The search is over.
            if (pathProblem.m_ItLinkerStart == pathProblem.m_ItLinkerEnd && pathProblem.m_NodeGrideEnd)
            {
                // If we haven't hyp path start..
                if (!pathProblem.m_HypSubPathStart.Size())
                {
                    // ... and the goal is in the same grid...
                    if (pathProblem.m_GridStart == pathProblem.m_GridEnd)
                    {
                        // ...we want find a direct path.
                        pathProblem.m_StartPartSearch = FALSE;
                        pathProblem.m_StateFunction = STATE_DIRECTPATH;
                        return 1;
                    }
                    pathProblem.m_StateFunction = STATE_PATHNOTFOUND;
                    return 0;
                }

                pathProblem.m_StartPartSearch = FALSE;
                pathProblem.m_StateFunction = STATE_GETLINKERFROMEND;
                return 1;
            }

            pathProblem.InitBeforeCase2Case(pathProblem.m_GridStart, &gridManager->m_ListGridInfo);

            // Refresh the context info
            gridManager->ResetArrayContextInfo(context, pathProblem.m_GridW * pathProblem.m_GridL);

            // The new start case is the new linker case...
            NodeLinker *linkerStart = *pathProblem.m_ItLinkerStart;
            pathProblem.m_XEndSearch = linkerStart->m_X;
            pathProblem.m_YEndSearch = linkerStart->m_Y;
            pathProblem.m_EndIndex = pathProblem.m_YEndSearch * pathProblem.m_GridStart->GetWidth() + pathProblem.m_XEndSearch;

            // Set the linkers.
            pathProblem.SetLinker(linkerStart, NULL);

            // The initial node in the open list is the start case;
            NodeGrid *nodeGrid = &gridManager->m_ArrayNodeGrid[pathProblem.m_YStartSearch * pathProblem.m_GridSearch->GetWidth() + pathProblem.m_XStartSearch];
            GridContext *contextInfo = nodeGrid->m_ArrayContextInfo[context];
            contextInfo->m_Cost = 0;
            contextInfo->m_F = sqrtf((float)((pathProblem.m_XStartSearch - pathProblem.m_XEndSearch) * (pathProblem.m_XStartSearch - pathProblem.m_XEndSearch) + (pathProblem.m_YStartSearch - pathProblem.m_YEndSearch) * (pathProblem.m_YStartSearch - pathProblem.m_YEndSearch)));
            contextInfo->m_Parent = 0;
            pathProblem.m_OpenListGrid->Push(nodeGrid);

            // We search nothing for now.
            pathProblem.m_NodeGrideEnd = 0;

            // Next linker.
            pathProblem.m_ItLinkerStart++;
        }
    } while (pathProblem.TimeNotOut());
    return 1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// GetLinkerFromEnd
// Warning :This function is time/frame based.
/////////////////////////////////////////////////////////////////////////////////////////////////////////
int GetLinkerFromEnd(PathProblem &pathProblem, int context)
{
    GridPathManager *gridManager = pathProblem.m_GridPathManager;
    do
    {
        // First time we enter in GetLinkerFromEnd function.
        if (!pathProblem.m_StartPartSearch)
        {
            GridInfo *gridInfo = *gridManager->m_ListGridInfo.FindPtr((uintptr_t)pathProblem.m_GridEnd);
            pathProblem.m_ItLinkerStart = gridInfo->m_ListNodeLinker.Begin();
            pathProblem.m_ItLinkerEnd = gridInfo->m_ListNodeLinker.End();

            // The search grid is the end Grid.
            pathProblem.m_GridSearch = pathProblem.m_GridEnd;

            // The start case is the goal position
            pathProblem.m_GridEnd->Get2dCoordsFrom3dPos(&pathProblem.m_GoalPos, &pathProblem.m_XStartSearch, &pathProblem.m_YStartSearch);

            pathProblem.m_NodeGrideEnd = (NodeGrid *)1;
            pathProblem.m_StartPartSearch = TRUE;
        }

        // If solution is not found
        if (!pathProblem.m_NodeGrideEnd)
        {
            // We search...
            if (!GetCase2CasePath(pathProblem, context))
                // Path not found.
                pathProblem.m_NodeGrideEnd = (NodeGrid *)1;
            else if (pathProblem.m_NodeGrideEnd)
            {
                // Path is found.
                // Save the path.
                SubPath *subPath = new SubPath(pathProblem.m_GridSearch, pathProblem.m_NodeGrideEnd->m_ArrayContextInfo[context]->m_Cost);
                pathProblem.BuildSubPath(context, subPath);
                pathProblem.m_HypSubPathEnd.PushBack(subPath);

                // Update connectivity between dyn nodeLinker and current linker.
                NodeLinkerIt itLinker = pathProblem.m_ItLinkerStart;
                itLinker--;
                (*itLinker)->m_DynNodeLinker[context] = pathProblem.m_NodeLinkerDynEnd;
                (*itLinker)->m_DynNodeLinkerCoast[context] = pathProblem.m_NodeGrideEnd->m_ArrayContextInfo[context]->m_Cost;
            }
        }
        else
        {
            if (pathProblem.m_ItLinkerStart != pathProblem.m_ItLinkerEnd || !pathProblem.m_NodeGrideEnd)
            {
                // Test if the target have access to linker
                while (!pathProblem.Linker2EndIsGood())
                {
                    pathProblem.m_ItLinkerStart++;
                    if (pathProblem.m_ItLinkerStart == pathProblem.m_ItLinkerEnd)
                        break;
                }
            }

            // The search is over.
            if (pathProblem.m_ItLinkerStart == pathProblem.m_ItLinkerEnd && pathProblem.m_NodeGrideEnd)
            {
                // If we haven't hyp path end..
                if (!pathProblem.m_HypSubPathEnd.Size())
                {
                    // ... and the goal is in the same grid...
                    if (pathProblem.m_GridStart == pathProblem.m_GridEnd)
                    {
                        // ...we want find a direct path.
                        pathProblem.m_StartPartSearch = FALSE;
                        pathProblem.m_StateFunction = STATE_DIRECTPATH;
                        return 1;
                    }
                    pathProblem.m_StateFunction = STATE_PATHNOTFOUND;
                    return 0;
                }

                pathProblem.m_StartPartSearch = FALSE;

                // We search nothing for now.
                pathProblem.m_NodeLinkerEnd = 0;

                // Refresh Contex info data
                gridManager->ResetArrayLinkerContextInfo(context);

                // Add linker to list.
                LinkerContext *contextInfo = pathProblem.m_NodeLinkerDynStart->m_ArrayContextInfo[context];
                contextInfo->m_Cost = 0;
                contextInfo->m_F = 0;
                contextInfo->m_Parent = 0;
                pathProblem.m_OpenListLinker->Push(pathProblem.m_NodeLinkerDynStart);

                pathProblem.m_NodeGrideEnd = (NodeGrid *)1;
                pathProblem.m_StartPartSearch = TRUE;

                // Ready for next state.
                pathProblem.m_StateFunction = STATE_GETGRID2DRIDPATH;
                return 1;
            }

            pathProblem.InitBeforeCase2Case(pathProblem.m_GridEnd, &gridManager->m_ListGridInfo);

            // Refresh the context info.
            gridManager->ResetArrayContextInfo(context, pathProblem.m_GridW * pathProblem.m_GridL);

            // The new end case is the new linker case...
            NodeLinker *linkerEnd = *pathProblem.m_ItLinkerStart;
            pathProblem.m_XEndSearch = linkerEnd->m_X;
            pathProblem.m_YEndSearch = linkerEnd->m_Y;
            pathProblem.m_EndIndex = pathProblem.m_YEndSearch * pathProblem.m_GridEnd->GetWidth() + pathProblem.m_XEndSearch;

            // Set the linkers.
            pathProblem.SetLinker(linkerEnd, NULL);

            // The initial node in the open list is the end case.
            NodeGrid *nodeGrid = &gridManager->m_ArrayNodeGrid[pathProblem.m_YStartSearch * pathProblem.m_GridSearch->GetWidth() + pathProblem.m_XStartSearch];
            GridContext *contextInfo = nodeGrid->m_ArrayContextInfo[context];
            contextInfo->m_Cost = 0;
            contextInfo->m_F = sqrtf((float)((pathProblem.m_XStartSearch - pathProblem.m_XEndSearch) * (pathProblem.m_XStartSearch - pathProblem.m_XEndSearch) + (pathProblem.m_YStartSearch - pathProblem.m_YEndSearch) * (pathProblem.m_YStartSearch - pathProblem.m_YEndSearch)));
            contextInfo->m_Parent = 0;
            pathProblem.m_OpenListGrid->Push(nodeGrid);

            // We search nothing for now.
            pathProblem.m_NodeGrideEnd = 0;

            // Next linker.
            pathProblem.m_ItLinkerStart++;
        }
    } while (pathProblem.TimeNotOut());
    return 1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// DirectPath
// Warning :This function is time/frame based.
/////////////////////////////////////////////////////////////////////////////////////////////////////////
int DirectPath(PathProblem &pathProblem, int context)
{
    GridPathManager *gridManager = pathProblem.m_GridPathManager;
    do
    {
        if (!pathProblem.m_StartPartSearch)
        {
            pathProblem.InitBeforeCase2Case(pathProblem.m_GridStart, &gridManager->m_ListGridInfo);

            // Refresh the context info.
            gridManager->ResetArrayContextInfo(context, pathProblem.m_GridW * pathProblem.m_GridL);

            // Set the linkers.
            pathProblem.SetLinker(NULL, NULL);

            // We inverse the path problem (to avoid to inverse it later...)
            // The dyn start linker is the start node grid.
            NodeLinker *nodeStart = pathProblem.m_NodeLinkerDynStart;
            NodeLinker *nodeEnd = pathProblem.m_NodeLinkerDynEnd;

            NodeGrid *nodeGrid = &gridManager->m_ArrayNodeGrid[nodeEnd->m_Y * nodeEnd->m_Grid->GetWidth() + nodeEnd->m_X];
            GridContext *contextInfo = nodeGrid->m_ArrayContextInfo[context];
            contextInfo->m_Cost = 0;
            contextInfo->m_F = sqrtf((float)((nodeEnd->m_X - nodeStart->m_X) * (nodeEnd->m_X - nodeStart->m_X) + (nodeEnd->m_Y - nodeStart->m_Y) * (nodeEnd->m_Y - nodeStart->m_Y)));
            contextInfo->m_Parent = 0;

            // Init the open list.
            pathProblem.m_OpenListGrid->Push(nodeGrid);

            // The dyn en linker is the goal node grid.
            pathProblem.m_XEndSearch = nodeStart->m_X;
            pathProblem.m_YEndSearch = nodeStart->m_Y;
            pathProblem.m_EndIndex = nodeStart->m_Y * nodeStart->m_Grid->GetWidth() + nodeStart->m_X;

            pathProblem.m_StartPartSearch = TRUE;
            pathProblem.m_NodeGrideEnd = 0;
        }
        else
        {
            // Search.
            if (!GetCase2CasePath(pathProblem, context))
            {
                // Ready for next state.
                pathProblem.m_StateFunction = STATE_PATHNOTFOUND;
                return 0;
            }
            else if (pathProblem.m_NodeGrideEnd)
            {
                // Save the path found.
                SubPath *subPath = new SubPath(pathProblem.m_GridSearch, pathProblem.m_NodeGrideEnd->m_ArrayContextInfo[context]->m_Cost);
                pathProblem.BuildSubPath(context, subPath);
                pathProblem.m_Path.AddSubPathBack(subPath);

                // Ready for next state.
                pathProblem.m_StateFunction = STATE_PATHFOUND;
                return 1;
            }
        }
    } while (pathProblem.TimeNotOut());
    return 1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// GetGrid2GridPath
// Warning :This function is time/frame based.
/////////////////////////////////////////////////////////////////////////////////////////////////////////
int GetGrid2GridPath(PathProblem &pathProblem, int context)
{
    NodeLinker *nodeLinker;
    GridPathManager *gridManager = pathProblem.m_GridPathManager;

    do
    {
        // Main A* loop
        if (pathProblem.m_OpenListLinker->Size())
        {
            // Pop the node with le lowest F.
            nodeLinker = pathProblem.m_OpenListLinker->PopBack();
            nodeLinker->m_ArrayContextInfo[context]->m_InOpen = FALSE;

            // Does the linker is the goal ?
            if (nodeLinker == pathProblem.m_NodeLinkerDynEnd)
            {
                // We find a path between grids.
                pathProblem.m_NodeLinkerEnd = pathProblem.m_NodeLinkerDynEnd;

                // We set the new state.
                AnalyseGrid2GridPath(pathProblem, context);
                return 2;
            }

            NodeLinkerIt it;
            FloatIt it2;
            IntIt it3;
            IntIt it4;
            // For each succesor...
            // Same ID.
            float cost = 1;
            if (nodeLinker->m_LinkerID != -1)
            {
                CKParameterOut *param = pathProblem.m_Target->GetAttributeParameter(nodeLinker->m_Attribut);
                if (param)
                {
                    param->GetValue(&cost);
                    if (cost <= 0)
                        cost = 0.001f;
                }
            }
            for (it = nodeLinker->m_ListConnected.Begin(); it != nodeLinker->m_ListConnected.End(); it++)
                ManageNodeLinkerSuccesor(pathProblem, context, nodeLinker, *it, cost);

            // Different ID.
            if (nodeLinker->m_ArrayIndoor)
            {
                int layerIndex = gridManager->m_LayerID2Index[pathProblem.m_ObstacleLayer];
                if (nodeLinker->m_ArrayIndoor[layerIndex].m_ListSameGrid.Size())
                {
                    // These succesor are linkers wich are in the same grid an wich are not connected.
                    // But a path between them exist (it was precalculated).
                    IndoorLink *indoorLink = &nodeLinker->m_ArrayIndoor[layerIndex];
                    it = indoorLink->m_ListSameGrid.Begin();
                    it2 = indoorLink->m_ListSameGridCoast.Begin();
                    it3 = indoorLink->m_ListSameGridLayerCoast.Begin();
                    it4 = indoorLink->m_ListSameGridMaxLayerCoast.Begin();
                    float slowingDivTresh = pathProblem.m_SlowingDivTreshold;
                    for (; it != nodeLinker->m_ArrayIndoor[layerIndex].m_ListSameGrid.End(); it++, it2++, it3++, it4++)
                        if (*it4 <= pathProblem.m_ObstacleThreshold)
                            ManageNodeLinkerSuccesor(pathProblem, context, nodeLinker, *it, *it2 + *it3 * slowingDivTresh);
                }
            }

            // Dynamic NodeLinker.
            if (nodeLinker->m_DynNodeLinker[context])
                ManageNodeLinkerSuccesor(pathProblem, context, nodeLinker, nodeLinker->m_DynNodeLinker[context], nodeLinker->m_DynNodeLinkerCoast[context]);

            nodeLinker->m_ArrayContextInfo[context]->m_Closed = TRUE;
        }
        else
        {
            pathProblem.m_StateFunction = STATE_PATHNOTFOUND;
            return 0;
        }
    } while (pathProblem.TimeNotOut());
    return 1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// ManageNodeLinkerSuccesor
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void ManageNodeLinkerSuccesor(PathProblem &pathProblem, int context, NodeLinker *nodeLinker, NodeLinker *nodeLinkerSuccesor, float distance)
{
    // Reject the linker if target can't take it.
    if (nodeLinkerSuccesor->m_LinkerID >= 0 && !pathProblem.m_Target->HasAttribute(nodeLinkerSuccesor->m_Attribut))
        return;

    // Reject the linker if obstacle is on the linker.
    CKLayer *layer = nodeLinkerSuccesor->m_Grid->GetLayer(pathProblem.m_ObstacleLayer);
    if (layer)
    {
        int val;
        layer->GetValue(nodeLinkerSuccesor->m_X, nodeLinkerSuccesor->m_Y, &val);
        if (val > pathProblem.m_ObstacleThreshold)
            return;
    }

    // If the 2 linkers are doors distance is real distance between them.
    if ((nodeLinker->m_TypeCase == tc_door && nodeLinkerSuccesor->m_TypeCase == tc_door) &&
        (nodeLinker->m_LinkerID == nodeLinkerSuccesor->m_LinkerID))
        distance = Magnitude(VxVector(*nodeLinker->m_Pos - *nodeLinkerSuccesor->m_Pos));

    LinkerContext *contexInfoS = nodeLinkerSuccesor->m_ArrayContextInfo[context];

    float newCoast = nodeLinker->m_ArrayContextInfo[context]->m_Cost + distance;
    if ((contexInfoS->m_InOpen || contexInfoS->m_Closed) && contexInfoS->m_Cost <= newCoast)
        return;

    // Update succesor.
    contexInfoS->m_Cost = newCoast;
    contexInfoS->m_F = newCoast;
    contexInfoS->m_Parent = nodeLinker;

    // Manage list.
    contexInfoS->m_Closed = FALSE;
    if (!contexInfoS->m_InOpen)
    {
        contexInfoS->m_InOpen = TRUE;
        pathProblem.m_OpenListLinker->Push(nodeLinkerSuccesor);
    }
    else
        pathProblem.m_OpenListLinker->Update();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// AnalyseGrid2GridPath
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void AnalyseGrid2GridPath(PathProblem &pathProblem, int context)
{
    NodeLinker *nodeLinker;
    NodeLinker *nodeLinkerFirt = 0;
    NodeLinker *nodeLinkerSec = 0;
    NodeLinker *nodeLinkerThird = 0;
    NodeLinker *nodeLinkerFour = 0;
    NodeLinker *nodeLinkerLast;
    int numNode = 0;
    int i;

    // Count the number of Nodegrid. And eliminate the none twin linkers.
    nodeLinker = pathProblem.m_NodeLinkerDynEnd;
    nodeLinkerLast = nodeLinker;
    while (nodeLinker)
    {
        ++numNode;
        nodeLinkerFour = nodeLinkerThird;
        nodeLinkerThird = nodeLinkerSec;
        nodeLinkerSec = nodeLinkerFirt;
        nodeLinkerFirt = nodeLinker;
        if ((numNode & 0x00000001) && numNode >= 3 && nodeLinkerSec->m_LinkerID != nodeLinkerFirt->m_LinkerID)
        {
            nodeLinkerThird->m_ArrayContextInfo[context]->m_Parent = nodeLinkerFirt;
            nodeLinkerSec = nodeLinkerThird;
            nodeLinkerThird = nodeLinkerFour;
            --numNode;
        }
        nodeLinker = nodeLinker->m_ArrayContextInfo[context]->m_Parent;
    }

    if (numNode == 2)
    {
        // Delete unused hyp path.
        int size = pathProblem.m_HypSubPathStart.Size();
        for (i = 0; i < size; ++i)
            delete pathProblem.m_HypSubPathStart[i];
        pathProblem.m_HypSubPathStart.Clear();

        size = pathProblem.m_HypSubPathEnd.Size();
        for (i = 0; i < size; ++i)
            delete pathProblem.m_HypSubPathEnd[i];
        pathProblem.m_HypSubPathEnd.Clear();

        if (!pathProblem.m_HypSubPathStart2End)
        {
            // Ready for next state.
            pathProblem.m_StateFunction = STATE_PATHNOTFOUND;
            return;
        }

        // The path is start->end, we have already construct it.
        pathProblem.m_Path.AddSubPathBack(pathProblem.m_HypSubPathStart2End);

        // Because we don't want to delete it...
        pathProblem.m_HypSubPathStart2End = 0;

        // Ready for next state.
        pathProblem.m_StateFunction = STATE_PATHFOUND;
        return;
    }

    // Find index of start and end hyp path.
    FindHypPath(pathProblem, context, nodeLinkerSec, nodeLinkerLast->m_ArrayContextInfo[context]->m_Parent, &pathProblem.m_SubPathStart, &pathProblem.m_SubPathEnd);

    // Layer modification when path is calculating can produce no logical situation.
    if (!pathProblem.m_SubPathEnd || !pathProblem.m_SubPathStart || (pathProblem.m_SubPathStart->m_ArrayCase[0] != pathProblem.m_NodeLinkerDynStart->m_Y * pathProblem.m_NodeLinkerDynStart->m_Grid->GetWidth() + pathProblem.m_NodeLinkerDynStart->m_X))
    {
        // Ready for next state.
        pathProblem.m_StateFunction = STATE_PATHNOTFOUND;
        return;
    }

    if (numNode == 4)
    {
        // The path is start->linker->linker->end, we all allready construct it.
        pathProblem.m_Path.AddSubPathBack(pathProblem.m_SubPathEnd);
        pathProblem.m_Path.AddSubPathBack(pathProblem.m_SubPathStart);

        // Ready for next state.
        pathProblem.m_StateFunction = STATE_PATHFOUND;
        return;
    }

    // We remove the nodelinker extremities.
    nodeLinkerThird->m_ArrayContextInfo[context]->m_Parent = 0;
    pathProblem.m_NodeLinkerEnd = pathProblem.m_NodeLinkerEnd->m_ArrayContextInfo[context]->m_Parent->m_ArrayContextInfo[context]->m_Parent;

    // Ready for next state.
    pathProblem.m_StartPartSearch = FALSE;
    pathProblem.m_StateFunction = STATE_GETALLCASE2CASEPATH;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// FindHypPathIndex
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void FindHypPath(PathProblem &pathProblem, int context, NodeLinker *nodeLinkerFirst, NodeLinker *nodeLinkerLast, SubPath **startPath, SubPath **endPath)
{
    int i, size;
    int index;
    XArray<SubPath *> *m_HypSubPath;

    index = nodeLinkerFirst->m_Y * nodeLinkerFirst->m_Grid->GetWidth() + nodeLinkerFirst->m_X;
    m_HypSubPath = &pathProblem.m_HypSubPathStart;
    size = m_HypSubPath->Size();
    for (i = 0; i < size; ++i)
        if ((*m_HypSubPath)[i]->m_ArrayCase[(*m_HypSubPath)[i]->m_ArrayCase.Size() - 1] == index)
        {
            *startPath = (*m_HypSubPath)[i];
            m_HypSubPath->Remove(*startPath);
            break;
        }

    --size;
    for (i = 0; i < size; ++i)
        delete (*m_HypSubPath)[i];
    m_HypSubPath->Clear();

    index = nodeLinkerLast->m_Y * nodeLinkerLast->m_Grid->GetWidth() + nodeLinkerLast->m_X;
    m_HypSubPath = &pathProblem.m_HypSubPathEnd;
    size = m_HypSubPath->Size();
    for (i = 0; i < size; ++i)
        if ((*m_HypSubPath)[i]->m_ArrayCase[0] == index)
        {
            *endPath = (*m_HypSubPath)[i];
            m_HypSubPath->Remove(*endPath);
            break;
        }

    --size;
    for (i = 0; i < size; ++i)
        delete (*m_HypSubPath)[i];
    m_HypSubPath->Clear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// GetCase2CasePath
// Warning :This function is time/frame based.
/////////////////////////////////////////////////////////////////////////////////////////////////////////
int GetCase2CasePath(PathProblem &pathProblem, int context)
{
    GridPathManager *gridManager = pathProblem.m_GridPathManager;
    int w = pathProblem.m_GridW;
    NodeGrid *nodeGrid;
    NodeGrid *nodeGridSuccesor;
    int succIndex;
    float newCost;
    float newLayerCost;
    int *m_SuccesorID = gridManager->m_SuccesorID;
    float *m_SuccesorCost = gridManager->m_SuccesorCost;
    int *m_SuccesorLayerCost = gridManager->m_SuccesorLayerCost;

    do
    {
        // Main A* loop
        if (pathProblem.m_OpenListGrid->Size())
        {
            // Pop the node with le lowest F.
            nodeGrid = pathProblem.m_OpenListGrid->PopBack();
            nodeGrid->m_ArrayContextInfo[context]->m_InOpen = FALSE;
            GridContext *contextInfo = nodeGrid->m_ArrayContextInfo[context];

            // Does the node is the goal ?
            if (nodeGrid->m_CaseIndex == pathProblem.m_EndIndex)
            {
                pathProblem.m_NodeGrideEnd = nodeGrid;
                return 2;
            }

            // Get succesor index of nodeGrid.
            gridManager->FillSuccessor(pathProblem, nodeGrid->m_CaseIndex);

            // For each succesor...
            succIndex = 0;
            while (m_SuccesorID[succIndex] != -1)
            {
                nodeGridSuccesor = &gridManager->m_ArrayNodeGrid[m_SuccesorID[succIndex]];

                // The first cost is the real distance. The second is the (layer value cost)*(factor).
                GridContext *contextInfoS = nodeGridSuccesor->m_ArrayContextInfo[context];
                newCost = contextInfo->m_Cost + m_SuccesorCost[succIndex];
                newLayerCost = contextInfo->m_LayerCost + m_SuccesorLayerCost[succIndex] * pathProblem.m_SlowingDivTreshold;

                if ((contextInfoS->m_InOpen || contextInfoS->m_Closed) && contextInfoS->m_Cost + contextInfoS->m_LayerCost <= newCost + newLayerCost)
                {
                    ++succIndex;
                    continue;
                }

                // Update succesor.
                contextInfoS->m_Cost = newCost;
                contextInfoS->m_LayerCost = newLayerCost;

                // Heuristic.
                int dx, dy;
                {
                    div_t dt = div(nodeGridSuccesor->m_CaseIndex, w);
                    dy = dt.quot - pathProblem.m_YEndSearch;
                    dx = dt.rem - pathProblem.m_XEndSearch;
                }

                switch (pathProblem.m_Heuristic)
                {
                case HEURISTIC_EUCLIDIAN_DISTANCE:
                    contextInfoS->m_F = newCost + newLayerCost + sqrtf((float)dx * dx + dy * dy) * pathProblem.m_HeuristicCoef;
                    break;
                case HEURISTIC_MANHATTAN_DISTANCE:
                    contextInfoS->m_F = newCost + newLayerCost + (abs(dx) + abs(dy)) * pathProblem.m_HeuristicCoef;
                    break;
                case HEURISTIC_SQUARED_EUCLIDIAN_DISTANCE:
                    contextInfoS->m_F = newCost + newLayerCost + (dx * dx + dy * dy) * pathProblem.m_HeuristicCoef;
                    break;
                case HEURISTIC_OPTIMIZED_EUCLIDIAN_DISTANCE:
                    dx = abs(dx);
                    dy = abs(dy);
                    if (dy > dx)
                    {
                        dx = dx ^ dy;
                        dy = dx ^ dy;
                        dx = dx ^ dy;
                    }
                    contextInfoS->m_F = newCost + newLayerCost + (0.9604f * dx + 0.3978f * dy) * pathProblem.m_HeuristicCoef;
                    break;
                default:
                    contextInfoS->m_F = newCost + newLayerCost;
                }
                // Parent.
                contextInfoS->m_Parent = nodeGrid;

                // Manage list.
                contextInfoS->m_Closed = FALSE;
                if (!contextInfoS->m_InOpen)
                {
                    contextInfoS->m_InOpen = TRUE;
                    pathProblem.m_OpenListGrid->Push(nodeGridSuccesor);
                }
                else
                    pathProblem.m_OpenListGrid->Update();
                ++succIndex;
            }
            contextInfo->m_Closed = TRUE;
        }
        else
        {
            if (pathProblem.m_StateFunction == STATE_GETGRID2DRIDPATH)
                pathProblem.m_StateFunction = STATE_PATHNOTFOUND;
            return 0;
        }
    } while (pathProblem.TimeNotOut());
    return 1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// GetAllCase2CasePath
/////////////////////////////////////////////////////////////////////////////////////////////////////////
int GetAllCase2CasePath(PathProblem &pathProblem, int context)
{
    GridPathManager *gridManager = pathProblem.m_GridPathManager;
    do
    {
        if (!pathProblem.m_StartPartSearch)
        {
            // We add the last subpath.
            pathProblem.m_Path.AddSubPathFront(pathProblem.m_SubPathEnd);

            pathProblem.m_NodeGrideEnd = (NodeGrid *)1;
            pathProblem.m_StartPartSearch = TRUE;
        }

        if (!pathProblem.m_NodeLinkerEnd && pathProblem.m_NodeGrideEnd)
        {
            // We add the first subpath.
            pathProblem.m_Path.AddSubPathBack(pathProblem.m_SubPathStart);

            // Ready for next state.
            pathProblem.m_StartPartSearch = FALSE;
            pathProblem.m_StateFunction = STATE_PATHFOUND;
            return 1;
        }

        if (!pathProblem.m_NodeGrideEnd)
        {
            // We search...
            if (!GetCase2CasePath(pathProblem, context))
            {
                pathProblem.m_StateFunction = STATE_PATHNOTFOUND;
                return 0;
            }
            else if (pathProblem.m_NodeGrideEnd)
            {
                // Path found.
                SubPath *subPath = new SubPath(pathProblem.m_GridSearch, pathProblem.m_NodeGrideEnd->m_ArrayContextInfo[context]->m_Cost);
                pathProblem.BuildSubPath(context, subPath);
                pathProblem.m_Path.AddSubPathBack(subPath);
            }
        }
        else
        {
            pathProblem.InitBeforeCase2Case(pathProblem.m_NodeLinkerEnd->m_Grid, &gridManager->m_ListGridInfo);

            // Refresh the context info.
            gridManager->ResetArrayContextInfo(context, pathProblem.m_GridW * pathProblem.m_GridL);

            NodeLinker *nodeLinkerEnd = pathProblem.m_NodeLinkerEnd;
            NodeLinker *nodeLinkerParent = nodeLinkerEnd->m_ArrayContextInfo[context]->m_Parent;

            // Set the linkers.
            pathProblem.SetLinker(nodeLinkerEnd, nodeLinkerParent);

            // Init start case.
            pathProblem.m_XStartSearch = nodeLinkerEnd->m_X;
            pathProblem.m_YStartSearch = nodeLinkerEnd->m_Y;

            // Init end case.
            pathProblem.m_XEndSearch = nodeLinkerParent->m_X;
            pathProblem.m_YEndSearch = nodeLinkerParent->m_Y;
            pathProblem.m_EndIndex = pathProblem.m_YEndSearch * pathProblem.m_GridW + pathProblem.m_XEndSearch;

            // The initial node in the open list is the start case;
            NodeGrid *nodeGrid = &gridManager->m_ArrayNodeGrid[pathProblem.m_YStartSearch * pathProblem.m_GridW + pathProblem.m_XStartSearch];
            GridContext *contextInfo = nodeGrid->m_ArrayContextInfo[context];
            contextInfo->m_Parent = 0;
            pathProblem.m_OpenListGrid->Push(nodeGrid);

            // We search nothing for now.
            pathProblem.m_NodeGrideEnd = 0;

            // Next Linker.
            pathProblem.m_NodeLinkerEnd = nodeLinkerParent->m_ArrayContextInfo[context]->m_Parent;
        }
    } while (pathProblem.TimeNotOut());
    return 1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// PathFound
/////////////////////////////////////////////////////////////////////////////////////////////////////////
int PathFound(PathProblem &pathProblem, int context)
{
    GridPathManager *gridManager = pathProblem.m_GridPathManager;
    //--gridManager->m_NumPathProblem;

    // Information for Follow problem

    // Give num grid to subpath.
    int size = pathProblem.m_Path.NbSubPath();
    for (int i = 0; i < size; ++i)
    {
        SubPath *subPath = pathProblem.m_Path.GetSubPath(i);
        subPath->m_NumGrid = gridManager->GetGridNum(subPath->m_Grid);
    }
    pathProblem.m_Path.m_LastPoint = pathProblem.m_GoalPos;
    pathProblem.m_Path.m_LinkerObs = pathProblem.m_LinkerObs;

    if (pathProblem.m_Optimize)
        gridManager->OptimizePath(pathProblem);
    pathProblem.m_State = 2;
    pathProblem.m_StateFunction = STATE_FINISH;
    return 2;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// PathNotFound
/////////////////////////////////////////////////////////////////////////////////////////////////////////
int PathNotFound(PathProblem &pathProblem, int context)
{
    //--pathProblem.m_GridPathManager->m_NumPathProblem;
    pathProblem.m_State = 3;
    pathProblem.m_StateFunction = STATE_FINISH;
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Finish
/////////////////////////////////////////////////////////////////////////////////////////////////////////
int Finish(PathProblem &pathProblem, int context)
{
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// GetPath
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void GridPathManager::GetPath(CK3dEntity *target, float &coast, int &pathID)
{
    int *pathContextPtr;
    PathProblem *pathProblem;

    // Get the pathProblem attached to the target.
    if (!(pathContextPtr = m_Target2PathContext.FindPtr((uintptr_t)target)))
        return;
    pathProblem = m_ArrayPathProblem[*pathContextPtr];
    pathProblem->m_FollowPathID = m_FollowPathID;
    pathID = m_FollowPathID++;
    coast = pathProblem->m_Path.m_Coast;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// GetPath
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void GridPathManager::GetPath(CK3dEntity *target, float &coast, CKCurve *curve)
{
    int *contextPtr;
    PathProblem *pathProblem;
    CKCurvePoint *cpoint;
    VxVector pos;
    int x, y;
    SubPath *subPath;
    CKGrid *grid;
    int indexPoint = 0;
    int size;

    // Get the pathProblem attached to the target.
    if (!(contextPtr = m_Target2PathContext.FindPtr((uintptr_t)target)))
        return;
    pathProblem = m_ArrayPathProblem[*contextPtr];

    // Fill the curve.
    for (int i = pathProblem->m_Path.NbSubPath() - 1; i >= 0; --i)
    {
        subPath = pathProblem->m_Path.GetSubPath(i);
        grid = subPath->m_Grid;
        size = subPath->m_ArrayCase.Size();
        for (int j = 0; j < size; ++j)
        {
            if (!(cpoint = curve->GetControlPoint(indexPoint)))
            {
                cpoint = (CKCurvePoint *)(target->GetCKContext()->CreateObject(CKCID_CURVEPOINT, NULL, CK_OBJECTCREATION_DYNAMIC));
                curve->AddControlPoint(cpoint);
            }
            {
                div_t dt = div(subPath->GetCase(j), grid->GetWidth());
                y = dt.quot;
                x = dt.rem;
            }
            subPath->m_Grid->Get3dPosFrom2dCoords(&pos, x, y);
            cpoint->SetLinear(TRUE);
            cpoint->SetPosition(&pos);
            ++indexPoint;
        }
    }
    while ((cpoint = curve->GetControlPoint(indexPoint)))
    {
        curve->RemoveControlPoint(cpoint);
        target->GetCKContext()->DestroyObject(cpoint->GetID());
    }
    coast = pathProblem->m_Path.m_Coast;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// GetPath
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void GridPathManager::GetPath(CK3dEntity *target, float &coast, CKDataArray *dataArray)
{
    int *contextPtr;
    PathProblem *pathProblem;
    VxVector pos;
    int x, y;
    SubPath *subPath;
    CKGrid *grid;
    int indexPoint = 0;

    // Get the pathProblem attached to the target.
    if (!(contextPtr = m_Target2PathContext.FindPtr((uintptr_t)target)))
        return;
    pathProblem = m_ArrayPathProblem[*contextPtr];

    // Fill the array.
    for (int i = pathProblem->m_Path.NbSubPath() - 1; i >= 0; --i)
    {
        subPath = pathProblem->m_Path.GetSubPath(i);
        grid = subPath->m_Grid;
        for (int j = 0; j < subPath->m_ArrayCase.Size(); ++j)
        {
            {
                div_t dt = div(subPath->GetCase(j), grid->GetWidth());
                y = dt.quot;
                x = dt.rem;
            }
            subPath->m_Grid->Get3dPosFrom2dCoords(&pos, x, y);
            if (dataArray->GetRowCount() <= indexPoint)
                dataArray->InsertRow();
            dataArray->SetElementValue(indexPoint, 0, &pos);
            ++indexPoint;
        }
    }
    coast = pathProblem->m_Path.m_Coast;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// ManagePathFinding
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void GridPathManager::ManagePathFinding()
{
    int NumProblem = m_NumPathProblem;

    for (int i = 0; i < m_MaxPathProblem; ++i)
    {
        if (!NumProblem)
            return;

        PathProblem *pathProblem = m_ArrayPathProblem[i];
        if (pathProblem->m_StateFunction != STATE_FINISH)
        {
            --NumProblem;
            pathProblem->ResetTime();
            do
            {
                pathProblem->m_StateFunction(*pathProblem, i);
                if (pathProblem->m_State != 1)
                    break;
            } while (pathProblem->TimeNotOut());
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Manage
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void GridPathManager::Manage()
{
    if (!m_GraphDone)
        return;
    ManagePathFinding();
    ManagePathFollow();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// SequenceToBeDeleted
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CKERROR GridPathManager::SequenceToBeDeleted(CK_ID *objids, int count)
{
    if (!m_GraphDone)
        return CK_OK;

    int i = 0;

    // Check if a PathProblem uses an object to be deleted
    if (m_NumPathProblem)
    {
        for (i = 0; i < m_ArrayPathProblem.Size(); ++i)
        {
            PathProblem *problem = m_ArrayPathProblem[i];
            if (!problem || !problem->m_Target || problem->m_StateFunction == STATE_FINISH)
                continue;

            if (problem->m_Target->IsToBeDeleted() ||
                (problem->m_GridStart && problem->m_GridStart->IsToBeDeleted()) ||
                (problem->m_GridEnd && problem->m_GridEnd->IsToBeDeleted()) ||
                (problem->m_GridSearch && problem->m_GridSearch->IsToBeDeleted()))
            {
                UnregisterPathProblem(problem->m_Target, FALSE);
                problem->m_StateFunction = STATE_FINISH;
            }
        }
    }

    // Check if a FollowProblem uses an object to be deleted
    for (i = 0; i < m_ArrayFollowProblem.Size(); ++i)
    {
        FollowProblem *problem = m_ArrayFollowProblem[i];
        if (problem->m_State == finish)
            continue;

        if (problem->m_Target->IsToBeDeleted() ||
            problem->m_Beh->IsToBeDeleted() ||
            problem->m_Charac->IsToBeDeleted() ||
            problem->m_Grid->IsToBeDeleted())
        {
            UnregisterFollowProblem(problem->m_PathID);
        }
    }

    return CK_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// GridContainLayer
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CKBOOL GridPathManager::GridContainOneLayer(CKGrid *grid, XList<int> *listLayer) const
{
    IntIt it;

    for (it = listLayer->Begin(); it != listLayer->End(); it++)
        if (grid->GetLayer(*it))
            return TRUE;
    return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// FillSuccessor
// Used to contruct the graph.
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void GridPathManager::FillSuccessor(CKGrid *grid, int index, int layerIndex)
{
    m_NumSucc = 0;
    m_Count = 0;
    m_Layer = grid->GetLayer(layerIndex);
    m_GridW = grid->GetWidth();
    m_GridL = grid->GetLength();
    m_CaseY = index / m_GridW;
    m_CaseX = index - m_CaseY * m_GridW;
    for (int i = m_CaseX - 1; i <= m_CaseX + 1; ++i)
        for (int j = m_CaseY - 1; j <= m_CaseY + 1; ++j)
        {
            if (i >= 0 && i < m_GridW && j >= 0 && j < m_GridL && (m_CaseX != i || m_CaseY != j))
            {
                if (m_Layer)
                    m_Layer->GetValue(i, j, &m_LayerVal);
                else
                    m_LayerVal = 0;
                if (m_LayerVal < 255)
                {
                    m_SuccesorID[m_NumSucc] = j * m_GridW + i;
                    if (m_Count == 0 || m_Count == 2 || m_Count == 6 || m_Count == 8)
                        m_SuccesorCost[m_NumSucc] = DISTANCE_CASE_DIAGONAL;
                    else
                        m_SuccesorCost[m_NumSucc] = DISTANCE_CASE_JUXTAPOSE;
                    m_SuccesorLayerCost[m_NumSucc++] = m_LayerVal;
                }
            }
            ++m_Count;
        }
    m_SuccesorID[m_NumSucc] = -1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// FillSuccessor
// with diagonal
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void GridPathManager::FillSuccessor(PathProblem &pathProblem, int index)
{
    //	 -----
    //	|4 6 5|
    //	|     |
    //	|7 9 8|
    //	|     |
    //	|1 3 2|
    //	 -----
    m_GridW = pathProblem.m_GridW;
    m_GridL = pathProblem.m_GridL;
    // Find pos of index in the grid.
    {
        div_t dt = div(index, m_GridW);
        m_CaseY = dt.quot;
        m_CaseX = dt.rem;
    }

    m_NumSucc = 0;
    const float *succIndexCoast;
    if (pathProblem.m_Diagonal)
    {
        succIndexCoast = m_SuccesorIndexCost;
        if (!m_CaseY)
        {
            if (!m_CaseX)
            {
                m_IndexSuccStart = 1;
                m_IndexSuccEnd = 3;
            } // 1
            else if (m_CaseX == m_GridW - 1)
            {
                m_IndexSuccStart = 7;
                m_IndexSuccEnd = 9;
            } // 2
            else
            {
                m_IndexSuccStart = 7;
                m_IndexSuccEnd = 11;
            } // 3
        }
        else if (m_CaseY == m_GridL - 1)
        {
            if (!m_CaseX)
            {
                m_IndexSuccStart = 3;
                m_IndexSuccEnd = 5;
            } // 4
            else if (m_CaseX == m_GridW - 1)
            {
                m_IndexSuccStart = 5;
                m_IndexSuccEnd = 7;
            } // 5
            else
            {
                m_IndexSuccStart = 3;
                m_IndexSuccEnd = 7;
            } // 6
        }
        else if (!m_CaseX)
        {
            m_IndexSuccStart = 1;
            m_IndexSuccEnd = 5;
        } // 7
        else if (m_CaseX == m_GridW - 1)
        {
            m_IndexSuccStart = 5;
            m_IndexSuccEnd = 9;
        } // 8
        else
        {
            m_IndexSuccStart = 0;
            m_IndexSuccEnd = 7;
        } // 9
    }
    else
    {
        succIndexCoast = m_SuccesorIndexCost2;
        if (!m_CaseY)
        {
            if (!m_CaseX)
            {
                m_IndexSuccStart = 0;
                m_IndexSuccEnd = 1;
            } // 1
            else if (m_CaseX == m_GridW - 1)
            {
                m_IndexSuccStart = 3;
                m_IndexSuccEnd = 4;
            } // 2
            else
            {
                m_IndexSuccStart = 3;
                m_IndexSuccEnd = 5;
            } // 3
        }
        else if (m_CaseY == m_GridL - 1)
        {
            if (!m_CaseX)
            {
                m_IndexSuccStart = 1;
                m_IndexSuccEnd = 2;
            } // 4
            else if (m_CaseX == m_GridW - 1)
            {
                m_IndexSuccStart = 2;
                m_IndexSuccEnd = 3;
            } // 5
            else
            {
                m_IndexSuccStart = 1;
                m_IndexSuccEnd = 3;
            } // 6
        }
        else if (!m_CaseX)
        {
            m_IndexSuccStart = 0;
            m_IndexSuccEnd = 2;
        } // 7
        else if (m_CaseX == m_GridW - 1)
        {
            m_IndexSuccStart = 2;
            m_IndexSuccEnd = 4;
        } // 8
        else
        {
            m_IndexSuccStart = 0;
            m_IndexSuccEnd = 3;
        } // 9
    }

    int indexSucc;
    CKBOOL linkerObs = pathProblem.m_LinkerObs;
    int obsTresh = pathProblem.m_ObstacleThreshold;
    CKSquare *square = pathProblem.m_Square;
    GridPathManager *gridManager = pathProblem.m_GridPathManager;

    for (int i = m_IndexSuccStart; i <= m_IndexSuccEnd; ++i)
    {
        // Index of the successor.
        indexSucc = pathProblem.m_SuccesorIndex[i] + index;

        // Test is case is a linker obstacle.
        if (linkerObs && gridManager->CaseIsLinker(pathProblem, indexSucc))
            continue;

        // Check layer cost.
        int layerCost = 0;
        if (!square || (layerCost = square[indexSucc].dval) <= obsTresh)
        {
            // Add a successor.
            m_SuccesorID[m_NumSucc] = indexSucc;
            m_SuccesorCost[m_NumSucc] = succIndexCoast[i];
            m_SuccesorLayerCost[m_NumSucc++] = layerCost;
        }
    }
    m_SuccesorID[m_NumSucc] = -1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// CaseIsLinker
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CKBOOL GridPathManager::CaseIsLinker(PathProblem &pathProblem, CKGrid *grid, int index)
{
    NodeLinker *nodeLinker = m_ArrayNodeGrid[index].m_LinkerObs[(*m_ListGridInfo.FindPtr((uintptr_t)grid))->m_Num];

    if (nodeLinker)
    {
        if (nodeLinker->m_TypeCase == tc_door)
            return FALSE;
        if (pathProblem.m_Target->HasAttribute(nodeLinker->m_Attribut))
        {
            if (index != pathProblem.m_EndIndex)
                return TRUE;
            else
                return FALSE;
        }
        return TRUE;
    }
    return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// CaseIsLinker
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CKBOOL GridPathManager::CaseIsLinker(PathProblem &pathProblem, int index)
{
    NodeLinker *nodeLinker = m_ArrayNodeGrid[index].m_LinkerObs[pathProblem.m_NumGrid];

    if (nodeLinker)
    {
        if (nodeLinker->m_TypeCase == tc_door)
            return FALSE;
        if (pathProblem.m_Target->HasAttribute(nodeLinker->m_Attribut))
        {
            if (index != pathProblem.m_EndIndex)
                return TRUE;
            else
                return FALSE;
        }
        return TRUE;
    }
    return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// CasesDistance
// This function do a A* search, but it is not time/frame based
// because it is used to initialyse the graph.
/////////////////////////////////////////////////////////////////////////////////////////////////////////
GridContext *GridPathManager::CasesDistance(CKGrid *grid, int layer, int sX, int sY, int eX, int eY, int &maxLayerValue)
{
    OpenListGrid listNodeGrid(0, (int)(grid->GetWidth() * grid->GetLength() * PRECALC_OPENLIST_BUFFER_FACTOR));
    int w = grid->GetWidth();
    int goalIndex = eY * w + eX;
    int index;
    NodeGrid *nodeGrid;
    NodeGrid *nodeGridSuccesor;
    int succIndex;
    float newCoast;
    int succX, succY;

    // Init start node
    index = sY * grid->GetWidth() + sX;
    m_ArrayNodeGrid[index].m_ArrayContextInfo[0]->m_Cost = 0;
    m_ArrayNodeGrid[index].m_ArrayContextInfo[0]->m_F = sqrtf((float)(((sX - eX) * (sX - eX) + (sY - eY) * (sY - eY))));

    // Add start node in open list.
    listNodeGrid.Push(&m_ArrayNodeGrid[index]);

    // Main A* loop
    while (listNodeGrid.Size())
    {
        // Pop the node with le lowest F.
        nodeGrid = listNodeGrid.PopBack();
        nodeGrid->m_ArrayContextInfo[0]->m_InOpen = FALSE;

        // Does the node is the goal ?
        if (nodeGrid->m_CaseIndex == goalIndex)
        {
            // Search the max layer value.
            NodeGrid *nodeGrid2 = nodeGrid;
            maxLayerValue = 0;
            while (nodeGrid2)
            {
                if (maxLayerValue < nodeGrid2->m_LayerValue)
                    maxLayerValue = nodeGrid2->m_LayerValue;
                nodeGrid2 = nodeGrid2->m_ArrayContextInfo[0]->m_Parent;
            }
            return nodeGrid->m_ArrayContextInfo[0];
        }

        // Get succesor index of nodeGrid.
        FillSuccessor(grid, nodeGrid->m_CaseIndex, layer);

        // For each succesor...
        succIndex = 0;
        while (m_SuccesorID[succIndex] != -1)
        {
            nodeGridSuccesor = &m_ArrayNodeGrid[m_SuccesorID[succIndex]];
            GridContext *gridContextS = nodeGridSuccesor->m_ArrayContextInfo[0];
            newCoast = nodeGrid->m_ArrayContextInfo[0]->m_Cost + m_SuccesorCost[succIndex];

            if ((gridContextS->m_InOpen || gridContextS->m_Closed) && gridContextS->m_Cost <= newCoast)
            {
                succIndex++;
                continue;
            }

            // Update succesor.
            gridContextS->m_Cost = newCoast;
            {
                div_t dt = div(nodeGridSuccesor->m_CaseIndex, w);
                succY = dt.quot;
                succX = dt.rem;
            }
            gridContextS->m_F = newCoast + sqrtf((float)(((succX - eX) * (succX - eX) + (succY - eY) * (succY - eY))));
            gridContextS->m_Parent = nodeGrid;
            nodeGridSuccesor->m_LayerValue = m_SuccesorLayerCost[succIndex];

            // Manage list.
            gridContextS->m_Closed = FALSE;
            if (!gridContextS->m_InOpen)
            {
                gridContextS->m_InOpen = TRUE;
                listNodeGrid.Push(nodeGridSuccesor);
            }
            else
                listNodeGrid.Update();
            succIndex++;
        }

        nodeGrid->m_ArrayContextInfo[0]->m_Closed = TRUE;
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// ResetArrayContextInfo
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void GridPathManager::ResetArrayContextInfo(int context, int numCase) const
{
    for (int i = 0; i < numCase; i++)
    {
        GridContext *contextInfo = m_ArrayNodeGrid[i].m_ArrayContextInfo[context];
        contextInfo->m_Cost = 0.0f;
        contextInfo->m_LayerCost = 0.0f;
        contextInfo->m_Closed = FALSE;
        contextInfo->m_InOpen = FALSE;
        contextInfo->m_Parent = 0;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// ResetArrayLinkerContextInfo
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void GridPathManager::ResetArrayLinkerContextInfo(int context) const
{
    for (NodeLinkerIt it = m_ListNodeLinker.Begin(); it != m_ListNodeLinker.End(); it++)
    {
        LinkerContext *contextInfo = (*it)->m_ArrayContextInfo[context];
        contextInfo->m_Cost = 0.0f;
        contextInfo->m_LayerCost = 0.0f;
        contextInfo->m_Closed = FALSE;
        contextInfo->m_InOpen = FALSE;
        contextInfo->m_Parent = 0;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// GetGridNum
/////////////////////////////////////////////////////////////////////////////////////////////////////////
int GridPathManager::GetGridNum(CKGrid *grid)
{
    return (*m_ListGridInfo.FindPtr((uintptr_t)grid))->m_Num;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// OptimizePath
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void GridPathManager::OptimizePath(PathProblem &pp)
{
    CKLayer *layer;
    SubPath *path;
    int size = pp.m_Path.NbSubPath();
    int size2;
    XArray<int> *arrayCase;

    for (int i = 0; i < size; ++i)
    {
        // For each subpath.
        path = pp.m_Path.GetSubPath(i);
        layer = path->m_Grid->GetLayer(pp.m_ObstacleLayer);
        if (layer)
            pp.m_Square = layer->GetSquareArray();
        else
            pp.m_Square = 0;
        arrayCase = &path->m_ArrayCase;
        pp.m_GridW = path->m_Grid->GetWidth();
        pp.m_NumGrid = path->m_NumGrid;
        int indexMax = pp.m_GridW * path->m_Grid->GetLength();

        // We check obstacles between each point.
        size2 = arrayCase->Size();
        for (int j = 0; j < size2 - 2; ++j)
        {
            int next = j + 1;
            int k = j + 2;
            int checkLinker;
            if (!j || j == size2 - 1)
                checkLinker = 1;
            else
                checkLinker = 0;
            while (k < size2 && !ObsBetweenPoint(pp, (*arrayCase)[j], (*arrayCase)[k], indexMax))
            {
                next = k;
                ++k;
            }
            for (int l = j + 1; l < next; ++l)
            {
                (*arrayCase).RemoveAt(j + 1);
                --size2;
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// CheckObsBetweenPoint
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CKBOOL GridPathManager::ObsBetweenPoint(PathProblem &pp, int start, int end, int indexMax)
{
    int w = pp.m_GridW;
    div_t dt = div(start, w);
    int y1 = dt.quot;
    int x1 = dt.rem;
    dt = div(end, w);
    int y2 = dt.quot;
    int x2 = dt.rem;
    int i, j, tmp;
    int Y1, Y2;
    int index;
    unsigned int obs = pp.m_ObstacleThreshold;
    int numGrid = pp.m_NumGrid;
    CKSquare *square = pp.m_Square;
    float error = 0.0f;

    if (!pp.m_LinkerObs && !square)
        return FALSE;

    // Vertical case.
    if (x1 == x2)
    {
        if (y1 > y2)
        {
            tmp = y1;
            y1 = y2;
            y2 = tmp;
        }
        for (j = y1; j <= y2; j++)
        {
            index = j * w + x1;
            if (square && (square[index].dval > obs))
                return TRUE;
            if (pp.m_LinkerObs && m_ArrayNodeGrid[index].m_LinkerObs[numGrid])
                return TRUE;
        }
        return FALSE;
    }

    // Other case.
    if (x1 > x2)
    {
        tmp = x1;
        x1 = x2;
        x2 = tmp;
        tmp = y1;
        y1 = y2;
        y2 = tmp;
    }

    float x1bis = -x1 + 0.5f;
    float x1bis2 = x1bis + 1;
    float dy = ((float)(y2 - y1) / (x2 - x1));

    Y1 = y1;
    Y2 = y1 + (int)(ceilf((x1 + x1bis) * dy));
    if (Y1 > Y2)
    {
        tmp = Y1;
        Y1 = Y2;
        Y2 = tmp;
    }
    Y1 = (int)(Y1 - error);
    Y2 = (int)(ceilf(Y2 + error));
    for (j = Y1; j <= Y2; ++j)
    {
        index = j * w + x1;
        if (index == start || index == end || index >= indexMax)
            continue;
        if (square && (square[index].dval > obs))
            return TRUE;
        if (pp.m_LinkerObs && m_ArrayNodeGrid[index].m_LinkerObs[numGrid])
            return TRUE;
    }

    for (i = x1; i < x2 - 1; ++i)
    {
        Y1 = (int)(y1 + (i + x1bis) * dy);
        Y2 = (int)(ceilf(y1 + (i + x1bis2) * dy));
        if (Y1 > Y2)
        {
            tmp = Y1;
            Y1 = Y2;
            Y2 = tmp;
        }
        Y1 = (int)(Y1 - error);
        Y2 = (int)(ceilf(Y2 + error));
        for (j = Y1; j <= Y2; ++j)
        {
            index = j * w + i + 1;
            if (index >= indexMax)
                continue;
            if (square && (square[index].dval > obs))
                return TRUE;
            if (pp.m_LinkerObs && m_ArrayNodeGrid[index].m_LinkerObs[numGrid])
                return TRUE;
        }
    }

    Y1 = (int)(ceilf(y1 + (x2 - 2 + x1bis2) * dy));
    Y2 = y2;
    if (Y1 > Y2)
    {
        tmp = Y1;
        Y1 = Y2;
        Y2 = tmp;
    }
    Y1 = (int)(Y1 - error);
    Y2 = (int)(ceilf(Y2 + error));
    for (j = Y1; j <= Y2; ++j)
    {
        index = j * w + x2;
        if (index == start || index == end || index >= indexMax)
            continue;
        if (square && (square[index].dval > obs))
            return TRUE;
        if (pp.m_LinkerObs && m_ArrayNodeGrid[index].m_LinkerObs[numGrid])
            return TRUE;
    }
    return FALSE;
}
