#include "CKAll.h"
#include "NodeGrid.h"
#include "FollowProblem.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// FollowProblem
/////////////////////////////////////////////////////////////////////////////////////////////////////////
FollowProblem::FollowProblem(CKMessageManager *messageManager, CKAttributeManager *attributManager, NodeGrid *arrayNodeGrid)
{
    m_MessageManager = messageManager;
    m_AttributManager = attributManager;
    m_ArrayNodeGrid = arrayNodeGrid;
    m_Message[0] = m_MessageManager->AddMessageType("Joy_Up");
    m_Message[1] = m_MessageManager->AddMessageType("Joy_Down");
    m_Message[2] = m_MessageManager->AddMessageType("Joy_Left");
    m_Message[3] = m_MessageManager->AddMessageType("Joy_Right");
    m_Invisible = FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// ~FollowProblem
/////////////////////////////////////////////////////////////////////////////////////////////////////////
FollowProblem::~FollowProblem()
{
    // Clear the path
    int size = m_Path.m_ArraySubPath.Size();
    for (int i = 0; i < size; i++)
        delete m_Path.m_ArraySubPath[i];
    m_Path.m_ArraySubPath.Clear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void FollowProblem::Set(FollowStruct *followStruct, Path *path, CKAttributeType attrType)
{
    if (followStruct->beh->GetOutputCount() == 7)
        m_ManageTeleporter = TRUE;
    else
        m_ManageTeleporter = FALSE;
    m_Beh = followStruct->beh;
    m_Target = followStruct->target;
    m_Character = (CKCharacter *)m_Target;
    m_Path.CopyAndErase(*path);
    m_PathID = followStruct->followPathID;
    if (followStruct->minDistanceFactor > 1)
        m_MinDistanceFactor = 1;
    else if (followStruct->minDistanceFactor < 0)
        m_MinDistanceFactor = 0.0001f;
    else
        m_MinDistanceFactor = followStruct->minDistanceFactor;
    m_LimitAngleCos = XAbs(cosf(followStruct->limitAngle));
    m_Fuzzyness = XAbs(followStruct->fuzzyness);
    if (m_Fuzzyness > 1)
        m_Fuzzyness = 1;
    m_Fuzzyness2 = 0.5f * m_Fuzzyness;
    m_Fuzzyness /= static_cast<float>(RAND_MAX);
    m_Pingpong = followStruct->pingpong;
    if (followStruct->maxBlockedTime >= 0)
        m_MaxBlockedTime = followStruct->maxBlockedTime;
    else
        m_MaxBlockedTime = 0;
    m_DistanceBlocked = XAbs(followStruct->distanceBlocked);

    m_AttrType = attrType;
    CKParameterOut *pout = m_Target->GetAttributeParameter(attrType);
    if (pout)
    {
        CK_ID *paramids = (CK_ID *)pout->GetReadDataPtr();
        ((CKParameterOut *)m_MessageManager->CKGetObject(paramids[0]))->GetValue(&m_ColisionFilter);
        ((CKParameterOut *)m_MessageManager->CKGetObject(paramids[1]))->GetValue(&m_RadiusColision);
        ((CKParameterOut *)m_MessageManager->CKGetObject(paramids[2]))->GetValue(&m_RadiusAvoid);
    }
    else
    {
        m_ColisionFilter = 0;
        m_RadiusColision = 1;
        m_RadiusAvoid = 1;
    }

    if (followStruct->enterTele >= 0)
        m_EnterTeleTime = followStruct->enterTele;
    else
        m_EnterTeleTime = 0;
    if (followStruct->travelTele)
        m_TravelTeleTime = followStruct->travelTele;
    else
        m_TravelTeleTime = 0;
    if (followStruct->exitTele)
        m_ExitTeleTime = followStruct->exitTele;
    else
        m_ExitTeleTime = 0;
    m_State = on;
    m_StateFunction = ::ComputeDir;
    m_TestTurnAngleCount = 0;
    m_LinkerObs = path->m_LinkerObs;

    // Set align function.
    switch (followStruct->direction)
    {
    case 1:
        m_AlignFunction = AlignOnPath1;
        break;
    case 2:
        m_AlignFunction = AlignOnPath2;
        break;
    case 3:
        m_AlignFunction = AlignOnPath3;
        break;
    case 4:
        m_AlignFunction = AlignOnPath4;
        break;
    case 5:
        m_AlignFunction = AlignOnPath5;
        break;
    case 6:
        m_AlignFunction = AlignOnPath6;
        break;
    }
    m_TimeBlocked.Reset();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Reset
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void FollowProblem::Reset()
{
    m_State = finish;
    m_StateFunction = ::ComputeDir;

    // Clear the path.
    m_Path.Clear();

    if (m_Invisible)
    {
        m_Invisible = FALSE;
        m_Target->Show(CKSHOW);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//	Orient
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void FollowProblem::FirstOrient(int firstSubPath)
{
    // Get path information.
    m_NumSubPath = m_Path.NbSubPath() - 1 - firstSubPath;
    m_SubPath = m_Path.GetSubPath(m_NumSubPath);
    if (m_NumSubPath)
        m_NextSubPath = m_Path.GetSubPath(m_NumSubPath - 1);
    m_TotalPoint = m_SubPath->NbCase();

    // Get usefull information.
    m_Grid = m_SubPath->m_Grid;
    m_Layer = m_Grid->GetLayer(m_Path.m_ObstacleLayer);
    m_Grid->GetScale(&m_GridScale);
    m_GridScale.y = 0;
    m_RadiusCgrid = m_RadiusColision / m_GridScale.x;
    m_MinDistance = (float)(Magnitude(m_GridScale) * m_MinDistanceFactor);
    m_W = m_Grid->GetWidth();
    m_L = m_Grid->GetLength();

    // Get current character position.
    m_Charac = m_Character->GetFloorReferenceObject();
    if (!m_Charac)
        m_Charac = m_Character->GetRootBodyPart();
    m_Charac->GetPosition(&m_RealPosition);
    m_Position = m_RealPosition;
    m_Position.y = 0;

    // Set Old Pos.
    m_OldPosition = m_Position;
    m_PosToMoveFrom = m_Position;

    m_NumPoint = 1;

    // Calcul real dir.
    PointAndDir2();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// CalculDirection
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void FollowProblem::CalculDirection()
{
    if (m_State == off)
        return;

    // Orient the target.
    m_StateFunction(this);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// CorrectDirection
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void FollowProblem::CorrectDirection()
{
    if (m_State != on && m_State != slow)
        return;

    // Reset to normal.
    m_MoveFunction = ::TurnAndUp;
    m_State = on;

    FollowProblem *fpColision, *fpAvoid;
    CK3dEntity *entCollision, *entAvoid;
    float entRadiusCollision;

    // Scan for 3dentities obstacles.
    if (!ScanRadius(fpColision, fpAvoid))
        ScanRadius(entCollision, entAvoid, entRadiusCollision);

    // Manage collision.
    if (fpColision)
    {
        RejectByTarget(fpColision);
        // Activate collision output
        m_Beh->ActivateOutput(2);
        // Write collions parameter output
        m_Beh->SetOutputParameterObject(0, fpColision->m_Target);
    }
    else if (entCollision)
    {
        RejectByTarget(entCollision, entRadiusCollision);
        // Activate collision output
        m_Beh->ActivateOutput(2);
        // Write collions parameter output
        m_Beh->SetOutputParameterObject(0, entCollision);
    }

    // Manage avoiding
    if (fpAvoid)
    {
        int pos, dir, speed;
        AnalyseTarget(fpAvoid, pos, dir, speed);
        AvoidTarget(fpAvoid, pos, dir, speed);
        // Activate avoid.
        m_Beh->ActivateOutput(3);
        // Write avoid parameter output.
        m_Beh->SetOutputParameterObject(1, fpAvoid->m_Target);
    }
    else if (entAvoid)
    {
        AvoidTarget(entAvoid, entRadiusCollision);
        // Activate avoid.
        m_Beh->ActivateOutput(3);
        // Write avoid parameter output.
        m_Beh->SetOutputParameterObject(1, entAvoid);
    }

    // Check for Layer obstacles.
    RejectByObstacle();

    // Detect blocked situation;
    DetectBlocked();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Move
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void FollowProblem::Move()
{
    m_MoveFunction(this);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Up
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void FollowProblem::ComputeDir()
{
    // Get current character position.
    m_Charac = m_Character->GetFloorReferenceObject();
    if (!m_Charac)
        m_Charac = m_Character->GetRootBodyPart();
    m_Charac->GetPosition(&m_RealPosition);
    m_Position = m_RealPosition;
    m_Position.y = 0;

    VxVector pt1;
    VxVector pt2;
    int a = m_SubPath->GetCase(0);
    int b = m_SubPath->GetCase(1);
    m_Dt = div(a, m_W);
    m_NextY = m_Dt.quot;
    m_NextX = m_Dt.rem;
    m_Grid->Get3dPosFrom2dCoords(&pt1, m_NextX, m_NextY);
    m_Dt = div(b, m_W);
    m_NextY = m_Dt.quot;
    m_NextX = m_Dt.rem;
    m_Grid->Get3dPosFrom2dCoords(&pt2, m_NextX, m_NextY);

    // Recalculate (if grid is moving !)
    if (m_Grid->GetMoveableFlags() & VX_MOVEABLE_HASMOVED)
        PointAndDir();

    m_Distance = Magnitude((m_NextPoint - m_Position));
    if (m_Distance < m_MinDistance)
    {
        // Save Type case.
        if (m_SubPath->m_LinkerStart)
            m_OldTypeCase = m_SubPath->m_LinkerStart->m_TypeCase;
        else
            m_OldTypeCase = tc_normal;

        // Next subpath point.
        ++m_NumPoint;
        if (m_NumPoint == m_TotalPoint)
        {
            // Next subpath.
            if (--m_NumSubPath < 0 || m_OldTypeCase == tc_tele2)
            {
                if (m_Pingpong)
                {
                    m_Path.Inverse();
                    if (m_OldTypeCase == tc_tele2)
                        FirstOrient(++m_NumSubPath);
                    else
                        FirstOrient(0);
                    return;
                }
                // The path is finish.
                m_State = finish;
                return;
            }

            // Change subpath.
            m_SubPath = m_Path.GetSubPath(m_NumSubPath);
            if (m_NumSubPath)
                m_NextSubPath = m_Path.GetSubPath(m_NumSubPath - 1);
            m_TotalPoint = m_SubPath->NbCase();

            // Get usefull information.
            m_Grid = m_SubPath->m_Grid;
            m_Layer = m_Grid->GetLayer(m_Path.m_ObstacleLayer);
            m_Grid->GetScale(&m_GridScale);
            m_GridScale.y = 0;
            m_RadiusCgrid = m_RadiusColision / m_GridScale.x;
            m_MinDistance = (float)(Magnitude(m_GridScale) * m_MinDistanceFactor);
            m_W = m_Grid->GetWidth();
            m_L = m_Grid->GetLength();

            if (m_OldTypeCase != tc_door)
            {
                m_NumPoint = 1;

                // Set new target position (teleportation).
                m_Dt = div(m_SubPath->GetCase(0), m_W);
                m_NextY = m_Dt.quot;
                m_NextX = m_Dt.rem;
                m_Grid->Get3dPosFrom2dCoords(&m_Position, m_NextX, m_NextY);

                // Manage teleporter wait.
                m_TimeTele.Reset();
                m_State = entertele;
                m_StateFunction = ::EnterTele;
                if (m_ManageTeleporter)
                    m_Beh->ActivateOutput(4);
                EnterTele();
                return;
            }
            else
                m_NumPoint = 0;
        }
        PointAndDir();
    }

    // Set the new direction.
    // In fact, if we haven't changing the subpath point, the new direction shouldn't
    // change but if character is moved by other thing we must recalculate the direction.

    VxVector vector = m_Position - m_NextPointFuzzy;
    VxVector point = m_NextPointFuzzy + (DotProduct(vector, m_RealDir) + m_GridScale.x) * m_RealDir;
    point.y = 0;

    if (Magnitude(m_NextPointFuzzy - point) < m_Distance2Point)
        m_Dir = point - m_Position;
    else
        m_Dir = m_NextPoint - m_Position;
    m_Dir.y = 0;
    m_Dir.Normalize();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// PointAndDir
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void FollowProblem::PointAndDir()
{
    m_NextPointFuzzy = m_NextPoint;

    // Set new next point.
    if (!m_NumSubPath && (m_NumPoint == m_TotalPoint - 1))
    {
        VxVector lastPoint;
        m_Grid->InverseTransform(&lastPoint, &m_Path.m_LastPoint);
        lastPoint.y = 0;
        m_Grid->Transform(&m_NextPoint, &lastPoint);
    }
    else
    {
        m_Dt = div(m_SubPath->GetCase(m_NumPoint), m_W);
        m_NextY = m_Dt.quot;
        m_NextX = m_Dt.rem;
        m_Grid->Get3dPosFrom2dCoords(&m_NextPoint, m_NextX, m_NextY);
    }
    if (m_Fuzzyness)
    {
        // Add random in nextpoint.
        m_Grid->InverseTransform(&m_NextPoint, &m_NextPoint);
        m_NextPoint.x += ((float)rand() * m_Fuzzyness) - m_Fuzzyness2;
        m_NextPoint.z += ((float)rand() * m_Fuzzyness) - m_Fuzzyness2;
        m_Grid->Transform(&m_NextPoint, &m_NextPoint);
    }
    m_NextPoint.y = 0;

    // Calcul the real direction.
    m_RealDir = m_NextPoint - m_NextPointFuzzy;
    m_Distance2Point = Magnitude(m_RealDir);
    m_RealDir.y = 0;
    m_RealDir.Normalize();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// PointAndDir2
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void FollowProblem::PointAndDir2()
{
    // Set new next point.
    m_Dt = div(m_SubPath->GetCase(1), m_W);
    m_NextY = m_Dt.quot;
    m_NextX = m_Dt.rem;
    m_Grid->Get3dPosFrom2dCoords(&m_NextPoint, m_NextX, m_NextY);
    if (m_Fuzzyness)
    {
        // Add random in nextpoint.
        m_Grid->InverseTransform(&m_NextPoint, &m_NextPoint);
        m_NextPoint.x += ((float)rand() * m_Fuzzyness) - m_Fuzzyness2;
        m_NextPoint.z += ((float)rand() * m_Fuzzyness) - m_Fuzzyness2;
        m_Grid->Transform(&m_NextPoint, &m_NextPoint);
    }
    m_NextPoint.y = 0;

    m_NextPointFuzzy = m_Position;

    // Calcul the real direction.
    m_RealDir = m_NextPoint - m_NextPointFuzzy;
    m_Distance2Point = Magnitude(m_RealDir);
    m_RealDir.y = 0;
    m_RealDir.Normalize();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// EnterTele
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void FollowProblem::EnterTele()
{
    if (m_NextSubPath->m_LinkerStart->m_Occupy)
        m_TimeTele.Reset();
    if (m_TimeTele.Current() < m_EnterTeleTime)
        return;
    m_State = traveltele;
    m_StateFunction = ::TravelTele;
    m_Target->Show(CKHIERARCHICALHIDE);
    m_Invisible = TRUE;
    m_TimeTele.Reset();
    if (m_ManageTeleporter)
        m_Beh->ActivateOutput(5);

    // Mark linker as occuped.
    m_NextSubPath->m_LinkerStart->m_Occupy = TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// TravelTele
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void FollowProblem::TravelTele()
{
    if (m_TimeTele.Current() < m_TravelTeleTime)
        return;
    m_State = exittele;
    m_StateFunction = ::ExitTele;
    m_Character->SetPosition(&m_Position);
    m_RealPosition = m_Position;
    m_Target->Show(CKSHOW);
    m_Invisible = FALSE;
    m_TimeTele.Reset();
    if (m_ManageTeleporter)
        m_Beh->ActivateOutput(6);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// ExitTele
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void FollowProblem::ExitTele()
{
    if (m_TimeTele.Current() < m_ExitTeleTime)
        return;

    // Mark linker as unoccuped.
    m_NextSubPath->m_LinkerStart->m_Occupy = FALSE;
    m_State = on;
    m_StateFunction = ::ComputeDir;
    PointAndDir2();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// CheckLayerObstacle
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void FollowProblem::RejectByObstacle()
{
    if (!m_NumPoint)
        // We are betwwenn two grid so we don't check for obstacle.
        return;
    VxVector posGrid;
    VxVector min, max;
    VxVector vector;
    float dist = 999999999.0f;
    float dist2;
    int sI = -1;
    int sJ;
    int val;

    m_Grid->InverseTransform(&posGrid, &m_RealPosition);
    min.x = posGrid.x - m_RadiusCgrid - 1;
    min.z = posGrid.z - m_RadiusCgrid - 1;
    min.y = 0;
    max.x = posGrid.x + m_RadiusCgrid + 1;
    max.z = posGrid.z + m_RadiusCgrid + 1;
    max.y = 0;
    if (min.x < 0)
        min.x = 0;
    if (max.x >= m_W)
        max.x = (float)m_W - 1;
    if (min.z < 0)
        min.z = 0;
    if (max.z >= m_L)
        max.z = (float)m_L - 1;
    for (int i = (int)min.x; i <= (int)max.x; ++i)
        for (int j = (int)min.z; j <= (int)max.z; ++j)
        {
            val = 0;
            if (m_Layer)
                m_Layer->GetValue(i, j, &val);
            // Check Layer Obstacle.
            if (val > m_Path.m_ObstacleThreshold)
            {
                if ((dist2 = Magnitude(posGrid - VxVector(i + 0.5f, 0, j + 0.5f))) < dist)
                {
                    sI = i;
                    sJ = j;
                    dist = dist2;
                }
            }
            // Check Linker Obstacle.
            else if (m_LinkerObs)
            {
                int index = j * m_W + i;
                NodeLinker *linker = m_ArrayNodeGrid[index].m_LinkerObs[m_SubPath->m_NumGrid];
                if (linker && linker->m_TypeCase != tc_door && !(index == m_SubPath->GetCase(0) || index == m_SubPath->GetCase(m_TotalPoint - 1)))
                    if ((dist2 = Magnitude(posGrid - VxVector(i + 0.5f, 0, j + 0.5f))) < dist)
                    {
                        sI = i;
                        sJ = j;
                        dist = dist2;
                    }
            }
        }
    if (sI != -1)
    {
        VxVector trans;
        float p, q, y;

        vector = posGrid - VxVector(sI + 0.5f, 0, sJ + 0.5f);
        vector.Normalize();
        if (vector.x)
        {
            p = vector.z / vector.x;
            q = sJ + 0.5f - p * (sI + 0.5f);
            if (vector.x > 0)
                y = p * (sI + 1.0f) + q;
            else
                y = p * sI + q;
        }
        else
            y = sJ + 2.0f;
        if (y > sJ + 1 || y < sJ)
        {
            // Cut x.
            if (vector.z > 0)
            {
                trans.z = posGrid.z - (sJ + 1);
                if (trans.z >= m_RadiusCgrid)
                    return;
                trans.z = m_RadiusCgrid - trans.z;
            }
            else
            {
                trans.z = sJ - posGrid.z;
                if (trans.z >= m_RadiusCgrid)
                    return;
                trans.z = -m_RadiusCgrid + trans.z;
            }
            trans.x = 0;
        }
        else
        {
            // Cut z.
            if (vector.x > 0)
            {
                trans.x = posGrid.x - (sI + 1);
                if (trans.x >= m_RadiusCgrid)
                    return;
                trans.x = m_RadiusCgrid - trans.x;
            }
            else
            {
                trans.x = sI - posGrid.x;
                if (trans.x >= m_RadiusCgrid)
                    return;
                trans.x = -m_RadiusCgrid + trans.x;
            }
            trans.z = 0;
        }
        trans.y = 0;
        m_Character->Translate(&trans, m_Grid);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// ScanRadius
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CKBOOL FollowProblem::ScanRadius(FollowProblem *&fpCollision, FollowProblem *&fpAvoid)
{
    fpCollision = 0;
    fpAvoid = 0;
    if (!m_ColisionFilter)
        return FALSE;

    FollowProblem *fp;
    int size = m_ArrayFollowProblem->Size();
    float distance;

    // Search the follower target.
    for (int i = 0; i < size; ++i)
    {
        fp = (*m_ArrayFollowProblem)[i];
        if (fp->m_Grid != m_Grid || fp->m_Target == m_Target)
            continue;

        // Read PF Colision attribut.
        if (!(fp->m_ColisionFilter & m_ColisionFilter))
            continue;

        // Check distance.
        distance = Magnitude(fp->m_Position - m_Position);
        if (distance <= m_RadiusColision + fp->m_RadiusColision)
            fpCollision = fp;
        if (distance <= m_RadiusAvoid + fp->m_RadiusColision)
            fpAvoid = fp;
        if (fpCollision && fpAvoid)
            return TRUE;
    }
    return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// ScanRadius
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CKBOOL FollowProblem::ScanRadius(CK3dEntity *&entCollision, CK3dEntity *&entAvoid, float &entRadiusCollision)
{
    entCollision = 0;
    entAvoid = 0;
    if (!m_ColisionFilter)
        return FALSE;

    // Search the "attributed" target.
    const XObjectPointerArray &array = m_AttributManager->GetAttributeListPtr(m_AttrType);
    VxVector pos;
    float distance;
    int colisionFilter;

    for (CKObject **it = array.Begin(); it != array.End(); ++it)
    {
        CK3dEntity *ent = (CK3dEntity *)*it;
        if (m_Target == ent)
            continue;
        CKParameterOut *pout = ent->GetAttributeParameter(m_AttrType);
        if (pout)
        {
            CK_ID *paramids = (CK_ID *)pout->GetReadDataPtr();
            ((CKParameterOut *)m_MessageManager->CKGetObject(paramids[0]))->GetValue(&colisionFilter);
            if (colisionFilter & m_ColisionFilter)
            {
                ((CKParameterOut *)m_MessageManager->CKGetObject(paramids[1]))->GetValue(&entRadiusCollision);

                // Check distance.
                ent->GetPosition(&pos);
                pos.y = 0;
                distance = Magnitude(pos - m_Position);
                if (distance <= m_RadiusColision + entRadiusCollision)
                    entCollision = ent;
                if (distance <= m_RadiusAvoid + entRadiusCollision)
                    entAvoid = ent;
                if (entCollision && entAvoid)
                    return TRUE;
            }
        }
    }
    return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// AnalyseTarget
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void FollowProblem::AnalyseTarget(FollowProblem *fp, int &pos, int &dir, int &speed)
{
    VxVector perpDir;
    VxVector perpDirFp;
    perpDir.x = m_Dir1.z;
    perpDir.z = -m_Dir1.x;
    perpDir.y = 0;
    m_DirFP = fp->m_Position - m_Position;
    m_DirFP.y = 0;
    m_DirFP.Normalize();
    perpDirFp.x = -m_DirFP.z;
    perpDirFp.z = m_DirFP.x;
    perpDirFp.y = 0;
    float angleP1 = DotProduct(m_Dir1, m_DirFP);
    float angleP2 = DotProduct(perpDirFp, fp->m_Dir1);
    float angleD1 = DotProduct(m_Dir1, fp->m_Dir1);
    float angleD2 = DotProduct(perpDir, fp->m_Dir1);
    float speed1, speed2;

    // Get position between this and fp.
    if (angleP2 < 0)
        pos = 0;
    else
        pos = 1;
    if (angleP1 < -0.707)
        pos += 6;
    else if (angleP1 < 0)
        pos += 4;
    else if (angleP1 < 0.707)
        pos += 2;

    // Get direction between this and fp.
    if (angleD2 > 0)
        dir = 0;
    else
        dir = 1;
    if (angleD1 < -0.707)
        dir += 6;
    else if (angleD1 < 0)
        dir += 4;
    else if (angleD1 < 0.707)
        dir += 2;

    // Calcul speed.
    speed1 = Magnitude(m_Position - m_OldPosition);
    speed2 = Magnitude(fp->m_Position - fp->m_OldPosition);
    if (XAbs(speed1 - speed2) < 0.001)
        speed = 0;
    else if (speed2 > speed1)
        speed = 1;
    else
        speed = -1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  RejectByTarget
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void FollowProblem::RejectByTarget(FollowProblem *fp)
{
    VxVector corect;
    float factor;

    // Calcul corect.
    corect = m_Position - fp->m_Position;
    corect.y = 0;
    factor = m_RadiusColision + fp->m_RadiusColision - Magnitude(corect);
    corect.y = 0;
    corect.Normalize();
    corect *= factor;

    // Corect character pos.
    m_Character->Translate(&corect);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  RejectByTarget
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void FollowProblem::RejectByTarget(CK3dEntity *ent, float entRadiusCollision)
{
    VxVector pos;
    VxVector corect;
    float factor;

    ent->GetPosition(&pos);
    pos.y = 0;

    // Calcul corect.
    corect = m_Position - pos;
    corect.y = 0;
    factor = m_RadiusColision + entRadiusCollision - Magnitude(corect);
    corect.Normalize();
    corect *= factor;

    // Corect character pos.
    m_Character->Translate(&corect);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  AvoidTarget
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void FollowProblem::AvoidTarget(FollowProblem *fp, int pos, int dir, int speed)
{

    if (pos <= 3)
    {
        // Obstacle is in the front.
        if (dir > 2)
        {
            // Change direction.
            if (pos & 1)
                // Rigth.
                m_Dir = m_Dir2;
            else
                // Left.
                m_Dir = -m_Dir2;

            m_TestTurnAngleCount = 10;
            m_MoveFunction = ::TurnAndUp2;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  AvoidTarget
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void FollowProblem::AvoidTarget(CK3dEntity *ent, float entRadiusCollision)
{
    VxVector posEnt;
    VxVector dirEnt;

    ent->GetPosition(&posEnt);
    posEnt.y = 0;
    dirEnt = posEnt - m_Position;

    if (DotProduct(dirEnt, m_Dir1) < 0.5)
        return;
    if (DotProduct(dirEnt, m_Dir2) > 0)
        // Left.
        m_Dir = -m_Dir2;
    else
        // Right.
        m_Dir = m_Dir2;
    m_TestTurnAngleCount = 100;
    m_MoveFunction = ::TurnAndUp2;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// DetectBlocked
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void FollowProblem::DetectBlocked()
{
    if (Magnitude(m_Position - m_PosToMoveFrom) > m_DistanceBlocked)
    {
        m_PosToMoveFrom = m_Position;
        m_TimeBlocked.Reset();
    }
    else if (m_TimeBlocked.Current() >= m_MaxBlockedTime)
    {
        m_State = timeout;
        m_PosToMoveFrom = m_Position;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// TurnAndUp
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void FollowProblem::TurnAndUp()
{
    // Set Old Pos.
    m_OldPosition = m_Position;
    if (m_State != on)
        return;

    // Calcul Character orientation.
    m_AlignFunction(this);

    // Calcul angle between character and path direction.
    float angle1 = DotProduct(m_Dir1, m_Dir);
    float angle2 = DotProduct(m_Dir2, m_Dir);

    if (angle1 < 0.9f)
    {
        // Send rotate message
        if (angle2 < 0.0f)
            m_MessageManager->SendMessageSingle(m_Message[2], m_Character, m_Character);
        else
            m_MessageManager->SendMessageSingle(m_Message[3], m_Character, m_Character);
    }
    if (m_TestTurnAngleCount || (angle1 > m_LimitAngleCos))
        // Send up message
        m_MessageManager->SendMessageSingle(m_Message[0], m_Character, m_Character);
    if (m_TestTurnAngleCount)
        --m_TestTurnAngleCount;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// TurnAndUp2
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void FollowProblem::TurnAndUp2()
{
    // Set Old Pos.
    m_OldPosition = m_Position;
    if (m_State != on)
        return;

    // Calcul Character orientation.
    m_AlignFunction(this);

    // Calcul angle between character and path direction.
    float angle1 = DotProduct(m_Dir1, m_Dir);
    float angle2 = DotProduct(m_Dir2, m_Dir);

    if (angle1 < 0.99f)
    {
        // Send rotate message
        if (angle2 < 0.0f)
            m_MessageManager->SendMessageSingle(m_Message[2], m_Character, m_Character);
        else
            m_MessageManager->SendMessageSingle(m_Message[3], m_Character, m_Character);
    }
    m_MessageManager->SendMessageSingle(m_Message[0], m_Character, m_Character);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Up
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void FollowProblem::Up()
{
    // Set Old Pos.
    m_OldPosition = m_Position;
    if (m_State != on)
        return;

    // Calcul Character orientation.
    m_AlignFunction(this);
    m_MessageManager->SendMessageSingle(m_Message[0], m_Character, m_Character);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// AlignOnPath1
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void AlignOnPath1(FollowProblem *fp)
{
    fp->m_Character->GetOrientation(&fp->m_Dir2, NULL, &fp->m_Dir1);
    fp->m_Dir2 = -fp->m_Dir2;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// AlignOnPath2
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void AlignOnPath2(FollowProblem *fp)
{
    fp->m_Character->GetOrientation(&fp->m_Dir2, NULL, &fp->m_Dir1);
    fp->m_Dir1 = -fp->m_Dir1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// AlignOnPath3
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void AlignOnPath3(FollowProblem *fp)
{
    fp->m_Character->GetOrientation(NULL, &fp->m_Dir1, &fp->m_Dir2);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// AlignOnPath4
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void AlignOnPath4(FollowProblem *fp)
{
    fp->m_Character->GetOrientation(NULL, &fp->m_Dir1, &fp->m_Dir2);
    fp->m_Dir1 = -fp->m_Dir1;
    fp->m_Dir2 = -fp->m_Dir2;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// AlignOnPath5
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void AlignOnPath5(FollowProblem *fp)
{
    fp->m_Character->GetOrientation(&fp->m_Dir1, NULL, &fp->m_Dir2);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// AlignOnPath6
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void AlignOnPath6(FollowProblem *fp)
{
    fp->m_Character->GetOrientation(&fp->m_Dir1, NULL, &fp->m_Dir2);
    fp->m_Dir1 = -fp->m_Dir1;
    fp->m_Dir2 = -fp->m_Dir2;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Up
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void ComputeDir(FollowProblem *fp)
{
    fp->ComputeDir();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// EnterTele
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void EnterTele(FollowProblem *fp)
{
    fp->EnterTele();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// TravelTele
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void TravelTele(FollowProblem *fp)
{
    fp->TravelTele();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// ExitTele
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void ExitTele(FollowProblem *fp)
{
    fp->ExitTele();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// TurnAndUp
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void TurnAndUp(FollowProblem *fp)
{
    fp->TurnAndUp();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// TurnAndUp2
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void TurnAndUp2(FollowProblem *fp)
{
    fp->TurnAndUp2();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Up
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void Up(FollowProblem *fp)
{
    fp->Up();
}
