/* Creature.cpp
 *
 * Copyright (C) 2004 Wow Daemon
 * Copyright (C) 2005 MaNGOS <https://opensvn.csie.org/traccgi/MaNGOS/trac.cgi/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Creature.h"
#include "Quest.h"
#include "Player.h"
#include "Opcodes.h"
#include "Stats.h"
#include "Log.h"
#include "ZoneMapper.h"
#include "LootMgr.h"
#include "Chat.h" // UQ1: for string formatting...

#ifdef ENABLE_GRID_SYSTEM
#include "MapManager.h"
#endif

Creature::Creature() : Unit()
{
    mQuestIds.clear();

    m_corpseDelay = 45000;
    m_respawnDelay = 25000;

    m_respawnTimer = 0;
    m_deathTimer = 0;
    m_moveTimer = 0;

    m_valuesCount = UNIT_END;

    // FIXME: use constant
    itemcount = 0;
    memset(item_list, 0, 8*128);

    m_nWaypoints = 0;
    m_currentWaypoint = 0;
    m_moveBackward = false;
    m_moveRandom = false;
    m_moveRun = false;
    m_creatureState = STOPPED;

    m_regenTimer=0;
    m_destinationX = m_destinationY = m_destinationZ = 0;
    m_moveSpeed = 0;
    m_timeToMove = 0;
    m_timeMoved = 0;
    m_useAI = true;
    m_moveSpeed = 7.0f;
    m_canMove = true;
}


Creature::~Creature()
{
    mQuestIds.clear( );
}


void Creature::UpdateMobMovement( uint32 p_time)
{
    uint32 timediff = 0;

    //Log::getSingleton( ).outDetail("Creature::UpdateMobMovement called!");

    if(m_moveTimer > 0)
    {
        if(p_time >= m_moveTimer)
        {
            timediff = p_time - m_moveTimer;
            m_moveTimer = 0;
        }
        else
            m_moveTimer -= p_time;
    }

    if(m_timeToMove > 0)
        m_timeMoved = m_timeToMove <= p_time + m_timeMoved ? m_timeToMove : p_time + m_timeMoved;

    if(m_creatureState == MOVING)
    {
        if(!m_moveTimer)
        {
            if(m_timeMoved == m_timeToMove)
            {
                m_creatureState = STOPPED;
                // wait before next move
                m_moveTimer = rand() % (m_moveRun ? 5000 : 10000);

#ifndef ENABLE_GRID_SYSTEM
                m_positionX = m_destinationX;
                m_positionY = m_destinationY;
                m_positionZ = m_destinationZ;
#else
                assert( m_destinationX != 0 && m_destinationZ != 0 && m_destinationY != 0);
                MapManager::Instance().GetMap(m_mapId)->ObjectRelocation<Creature>(this, m_destinationX, m_destinationY, m_destinationZ, m_orientation);        
#endif

                if(((uint32)m_positionX==respawn_cord[0])&&
                    ((uint32)m_positionY==respawn_cord[1])&&
                    ((uint32)m_positionZ==respawn_cord[2]))
                    SetUInt32Value(UNIT_FIELD_HEALTH,GetUInt32Value(UNIT_FIELD_MAXHEALTH));

                m_destinationX = m_destinationY = m_destinationZ = 0;
                m_timeMoved = 0;
                m_timeToMove = 0;
            }
            else
            {
#ifndef ENABLE_GRID_SYSTEM
                float q = (float)m_timeMoved / (float)m_timeToMove;
                m_positionX += (m_destinationX - m_positionX) * q;
                m_positionY += (m_destinationY - m_positionY) * q;
                m_positionZ += (m_destinationZ - m_positionZ) * q;

                m_timeToMove -= m_timeMoved;
                m_timeMoved = 0;

                AI_SendMoveToPacket(m_destinationX, m_destinationY, m_destinationZ, m_timeToMove, m_moveSpeed == 7.0*0.001);

                m_moveTimer = (UNIT_MOVEMENT_INTERPOLATE_INTERVAL < m_timeToMove) ? UNIT_MOVEMENT_INTERPOLATE_INTERVAL : m_timeToMove;
#else
                // Movement of creates should be updated first before sending the packet.. in case
                // creature travel out of the grid.. for instances, chasing a player around to different
                // location
                float q = (float)m_timeMoved / (float)m_timeToMove;
                float x = m_positionX + ((m_destinationX - m_positionX) * q);
                float y = m_positionY + ((m_destinationY - m_positionY) * q);
                float z = m_positionZ + ((m_destinationZ - m_positionZ) * q);

                m_timeToMove -= m_timeMoved;
                m_timeMoved = 0;

                AI_SendMoveToPacket(x, y, z, m_timeToMove, m_moveSpeed == 7.0*0.001);
                m_moveTimer = (UNIT_MOVEMENT_INTERPOLATE_INTERVAL < m_timeToMove) ? UNIT_MOVEMENT_INTERPOLATE_INTERVAL : m_timeToMove;
#endif
            }
        }                                           // still moving
    }
    // creature is stoped
    else if(m_creatureState == STOPPED && !m_moveTimer && !m_timeMoved)
    {
        if(sWorld.getAllowMovement() == false)      //is creature movement enabled?
            return;

    // if Spirit Healer don't move
        if(GetUInt32Value(UNIT_FIELD_DISPLAYID) == 5233)
            return;

        int destpoint;
        // If creature has no waypoints just wander aimlessly around spawnpoint
        if(m_nWaypoints==0)                         //no waypoints
        {
            // UQ1: Testing random movement...
/*            m_moveRandom = true;

            if(m_moveRandom)
            {
                //if((rand()%10)==0)
                if (m_moveTimer < timeGetTime())
                {
                    float wanderDistance=rand()%4+2;
                    float wanderX=((wanderDistance*rand())/RAND_MAX)-wanderDistance/2;
                    float wanderY=((wanderDistance*rand())/RAND_MAX)-wanderDistance/2;
                    float wanderZ=0;              // FIX ME ( i dont know how to get apropriate Z coord, maybe use client height map data)

                    if(getdistance(m_positionX,m_positionY,respawn_cord[0],respawn_cord[1])>10)
                    {
                        //return home
                        //Log::getSingleton( ).outDetail("Creature (return home) moving to %f %f %f (time %i)\n", respawn_cord[0],respawn_cord[1],respawn_cord[2], timeGetTime());
                        //AI_MoveTo(respawn_cord[0],respawn_cord[1],respawn_cord[2],false);
                        m_moveTimer = timeGetTime() + 1000;
                        AI_SendMoveToPacket(respawn_cord[0],respawn_cord[1],respawn_cord[2], 1000, 0);
                    }
                    else
                    {
                        //Log::getSingleton( ).outDetail("Creature moving to %f %f %f (time %i)\n", m_positionX+wanderX,m_positionY+wanderY,m_positionZ+wanderZ, timeGetTime());
                        //AI_MoveTo(m_positionX+wanderX,m_positionY+wanderY,m_positionZ+wanderZ,false);
                        m_moveTimer = timeGetTime() + 1000;
                        AI_SendMoveToPacket(m_positionX+wanderX,m_positionY+wanderY,m_positionZ+wanderZ, 1000, 0);
                    }
                }
            }*/

            // UQ1: Try auto-generating waypoints...
            uint32 loop;
            float x, y, z;

#define MAX_RAND_WAYPOINTS 8

            //Log::getSingleton( ).outDetail(fmtstring("Creature %s (at %f %f %f) generating random waypoints.\n", GetName(), respawn_cord[0], respawn_cord[1], respawn_cord[2]));

            if (!respawn_cord || !respawn_cord[0] || !respawn_cord[1] || !respawn_cord[2])
                return;

            for (loop = 0; loop < MAX_RAND_WAYPOINTS; loop++)
            {
                //float wanderDistance=rand()%4+2;
                float wanderDistance=16;
                float wanderX=((wanderDistance*rand())/RAND_MAX)-wanderDistance/2;
                float wanderY=((wanderDistance*rand())/RAND_MAX)-wanderDistance/2;
                float wanderZ=0;              // FIX ME ( i dont know how to get apropriate Z coord, maybe use client height map data)

                if (m_nWaypoints == 0)
                {// First waypoint, start from the respawn point...
                    x = respawn_cord[0]+wanderX;
                    y = respawn_cord[1]+wanderY;
                    z = respawn_cord[2]+wanderZ;
                }
                else
                {// In route waypoint, start from the last added point...
                    x = m_waypoints[m_nWaypoints-1][0]+wanderX;
                    y = m_waypoints[m_nWaypoints-1][1]+wanderY;
                    z = m_waypoints[m_nWaypoints-1][2]+wanderZ;
                }

                m_waypoints[m_nWaypoints][0] = x;
                m_waypoints[m_nWaypoints][1] = y;
                m_waypoints[m_nWaypoints][2] = z;
                m_nWaypoints++;
            }
            m_nWaypoints--;
            m_canMove = true;
            return;
        }
        else                                        //we do have waypoints
        {
            if(m_moveRandom)                        //is random move on if so move to a random waypoint
            {
                // if(m_nWaypoints > 1)
                destpoint = rand() % m_nWaypoints;
            }
            else                                    //random move is not on lets follow the path
            {
                //Log::getSingleton( ).outDetail("Creature (%s) following waypoints.\n", GetName());

                // Are we on the last waypoint? if so walk back
                if (m_currentWaypoint == (m_nWaypoints-1))
                    m_moveBackward = true;
                if (m_currentWaypoint == 0)         // Are we on the first waypoint? if so lets goto the second waypoint
                    m_moveBackward = false;
                if (!m_moveBackward)                // going 0..n
                    destpoint = ++m_currentWaypoint;
                else                                // going (n-1)..0
                    destpoint = --m_currentWaypoint;
            }
            if(m_canMove)
                AI_MoveTo(m_waypoints[destpoint][0], m_waypoints[destpoint][1], m_waypoints[destpoint][2], m_moveRun);
        }
    }
}

#ifndef __NO_PLAYERS_ARRAY__
#define PLAYERS_MAX 64550 // UQ1: What is the max GUID value???
extern uint32 NumActivePlayers;
extern long long ActivePlayers[PLAYERS_MAX];
extern float PlayerPositions[PLAYERS_MAX][2]; // UQ1: Defined in World.cpp...
extern long int PlayerZones[PLAYERS_MAX]; // UQ1: Defined in World.cpp...
extern long int PlayerMaps[PLAYERS_MAX]; // UQ1: Defined in World.cpp...

//#define VectorSubtract(a,b,c) ((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2])
#define VectorSubtract(a,b,c) c[0]=(a[0])-(b[0]); c[1]=(a[1])-(b[1]); c[2]=(a[2])-(b[2])

float VectorLength(float v[3])
{
    int        i;
    double    length;
    
    length = 0;
    for (i=0 ; i < 3 ; i++)
        length += v[i]*v[i];
    length = sqrt (length);        // FIXME
    return (float)length;
}

float Distance(float from1, float from2, float from3, float to1, float to2, float to3)
{
    float vec[3];
    float v1a[3];
    float v2a[3];

    v1a[0] = from1;
    v1a[1] = from2;
    v1a[2] = from3;
    v2a[0] = to1;
    v2a[1] = to2;
    v2a[2] = to3;

    VectorSubtract(v2a, v1a, vec);
    return (float)VectorLength(vec);
}

float DistanceNoHeight(float from1, float from2, float to1, float to2)
{
    float vec[3];
    float v1a[3];
    float v2a[3];

    v1a[0] = from1;
    v1a[1] = from2;
    v1a[2] = 0;
    v2a[0] = to1;
    v2a[1] = to2;
    v2a[2] = 0;

    VectorSubtract(v2a, v1a, vec); 
    return (float)VectorLength(vec);
}

float HeightDistance(float from, float to)
{
    float vec[3];
    float v1a[3];
    float v2a[3];

    v1a[0] = 0;
    v1a[1] = 0;
    v1a[2] = from;
    v2a[0] = 0;
    v2a[1] = 0;
    v2a[2] = to;

    VectorSubtract(v2a, v1a, vec);
    return VectorLength(vec);
}
#endif //__NO_PLAYERS_ARRAY__

extern float max_creature_distance;

void Creature::Update( uint32 p_time )
{
    //boolean isVendor = false;

#ifndef __NO_PLAYERS_ARRAY__
    uint32 loop;

    if (m_nextThinkTime > time(NULL))
        return; // Think once every 10 secs only for creatures that are not near a player...

    if (NumActivePlayers == 0)
        return; // UQ1: If there's no players online, why think???

    std::stringstream ss;
    ss.rdbuf()->str("");
    ss << GetMapId();//m_mapId;
    long int mapId = atoi(ss.str().c_str());
    //Log::getSingleton( ).outDetail("mapId: %s->%i.", ss.str().c_str(), mapId);

    std::stringstream ss2;
    ss2.rdbuf()->str("");
    ss2 << GetZoneId();//m_zoneId;
    long int zoneId = atoi(ss2.str().c_str());
    //Log::getSingleton( ).outDetail("zoneId: %s->%i.", ss2.str().c_str(), zoneId);

    std::stringstream ss3;
    ss3.rdbuf()->str("");
    ss3 << GetPositionX();//m_positionX;
    float x = (float)atof(ss3.str().c_str());
    //Log::getSingleton( ).outDetail("x: %s->%f.", ss3.str().c_str(), x);

    std::stringstream ss4;
    ss4.rdbuf()->str("");
    ss4 << GetPositionY();//m_positionY;
    float y = (float)atof(ss4.str().c_str());
    //Log::getSingleton( ).outDetail("y: %s->%f.", ss4.str().c_str(), y);

    std::stringstream ss5;
    ss5.rdbuf()->str("");
    ss5 << GetPositionZ();//m_positionZ;
    float z = (float)atof(ss5.str().c_str());
    //Log::getSingleton( ).outDetail("z: %s->%f.", ss5.str().c_str(), z);
#endif //__NO_PLAYERS_ARRAY__

    //if (this->getItemCount() > 0 && this->getItemCount() < MAX_CREATURE_ITEMS)
    //    isVendor = true;

    Unit::Update( p_time );

    if (HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_VENDOR) 
        || HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP)
        || HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER)
        || HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_TAXIVENDOR)
        || HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_TRAINER)
        || HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_SPIRITHEALER)
        || HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_BANKER)
        || HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_PETITIONER)
        || HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_TABARDVENDOR))
        return; // These guys shouldn't move...

    /*if (isVendor
        //|| HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP)
        //|| HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER)
        || HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_TAXIVENDOR)
        || HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_TRAINER)
        || HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_SPIRITHEALER)
        || HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_BANKER)
        || HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_PETITIONER))
        return; // These guys shouldn't move...*/

#ifndef ENABLE_GRID_SYSTEM

#ifdef __NO_PLAYERS_ARRAY__
    if(ZoneIDMap.GetZoneBit(this->GetZoneId()) == false)
    {
        // Still Moving well then lets stop
        if(m_creatureState == MOVING)
        {
            // Stop Moving
            m_moveSpeed = 7.0f*0.001f;
            AI_SendMoveToPacket(m_positionX, m_positionY, m_positionZ, 0, 1);
            m_moveTimer = 0;
            m_destinationX = m_destinationY = m_destinationZ = 0;
            m_timeMoved = 0;
            m_timeToMove = 0;
            m_creatureState = STOPPED;
        }
        return;
    } 
    
#else //!__NO_PLAYERS_ARRAY__
    //UQ1: This should be much faster (use less CPU time)...
    bool do_full_think = false;

    for (loop = 0; loop < NumActivePlayers; loop++)
    {// Exit procedure here if no players are close...
        bool sameFaction = false;

        if (PlayerMaps[ActivePlayers[loop]] != mapId)
            continue;

        // UQ1: Some NPCs have wrong zoneIds ??? WTF???
        //if (PlayerZones[ActivePlayers[loop]] != zoneId)
        //    continue;

        float distance = DistanceNoHeight(PlayerPositions[ActivePlayers[loop]][0], PlayerPositions[ActivePlayers[loop]][1], x, y);

        if(distance<=max_creature_distance)
        {
            do_full_think = true;
            m_nextThinkTime = time(NULL);

//            if (this->GetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE) == objmgr.GetObject<Player>(ActivePlayers[loop])->getFaction())
//                sameFaction = true; // Same faction as us...

            Unit *pVictim = (Unit*) objmgr.GetObject<Player>(ActivePlayers[loop]);
            WPAssert(pVictim);

            //if(distance<=50 && !sameFaction)
            if(distance<=GetAttackDistance(pVictim) && !sameFaction)
            {// If close enough, attack...
                if (HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_VENDOR) 
                    //|| HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP)
                    || HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER)
                    || HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_TAXIVENDOR)
                    || HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_TRAINER)
                    || HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_SPIRITHEALER)
                    || HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_BANKER)
                    || HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_PETITIONER)
                    || HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_TABARDVENDOR))
                    continue;

                if(m_attackers.empty())
                {
                    //Unit *pVictim = (Unit*) objmgr.GetObject<Player>(ActivePlayers[loop]);
                    //WPAssert(pVictim);

                    if (pVictim->isAlive())
                    {
                        m_attackers.insert(pVictim);
                    }
                }
            }
            break;
        }
    }

    if (!do_full_think)
    {
        // Still Moving well then lets stop
        if(m_creatureState == MOVING)
        {
            // Stop Moving
            m_moveSpeed = 7.0f*0.001f;
            AI_SendMoveToPacket(m_positionX, m_positionY, m_positionZ, 0, 1);
            m_moveTimer = 0;
            m_destinationX = m_destinationY = m_destinationZ = 0;
            m_timeMoved = 0;
            m_timeToMove = 0;
            m_creatureState = STOPPED;
        }

        m_nextThinkTime = time(NULL) + 10;
        return;
    }
#endif //__NO_PLAYERS_ARRAY__

#else //ENABLE_GRID_SYSTEM
    
    // if no player in the zone.. why bother updating me
    if( !MapManager::Instance().GetMap(m_mapId)->IsActiveGrid(this) )
    {
        if( m_creatureState = MOVING )
        {
            m_moveSpeed = 7.0f*0.001f; // ???
            m_moveTimer = 0;
            m_destinationX = m_destinationY = m_destinationZ = 0;
            m_timeMoved = 0;
            m_timeToMove = 0;
            m_creatureState = STOPPED;
            //return;
        }

        m_nextThinkTime = time(NULL) + 10;
        return;
    }

#ifndef __NO_PLAYERS_ARRAY__
    /* UQ1: Attack checking */
    for (loop = 0; loop < NumActivePlayers; loop++)
    {// Exit procedure here if no players are close...
        bool sameFaction = false;

        if (PlayerMaps[ActivePlayers[loop]] != mapId)
            continue;

        //if (PlayerZones[ActivePlayers[loop]] != zoneId)
        //    continue;

//        if (this->GetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE) == objmgr.GetObject<Player>(ActivePlayers[loop])->getFaction())
//            sameFaction = true; // Same faction as us...

        float distance = DistanceNoHeight(PlayerPositions[ActivePlayers[loop]][0], PlayerPositions[ActivePlayers[loop]][1], x, y);

        Unit *pVictim = (Unit*) ObjectAccessor::Instance().FindPlayer(ActivePlayers[loop]);
        WPAssert(pVictim);

        //if(distance<=50 && !sameFaction)
        if(distance<=GetAttackDistance(pVictim) && !sameFaction)
        {// If close enough, attack...
            if (HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_VENDOR) 
                //|| HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP)
                || HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER)
                || HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_TAXIVENDOR)
                || HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_TRAINER)
                || HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_SPIRITHEALER)
                || HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_BANKER)
                || HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_PETITIONER)
                || HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_TABARDVENDOR))
                continue;

            if(m_attackers.empty())
            {
                //Unit *pVictim = (Unit*) ObjectAccessor::Instance().FindPlayer(ActivePlayers[loop]);
                //WPAssert(pVictim);

                if (pVictim->isAlive())
                {
                    m_attackers.insert(pVictim);
                }
            }

            break; // We found an enemy to attack... Exit the loop...
        }
    }
#endif //__NO_PLAYERS_ARRAY__

    m_nextThinkTime = time(NULL);

#endif //ENABLE_GRID_SYSTEM

    if (m_deathState == JUST_DIED)
    {
        this->SetUInt32Value(UNIT_NPC_FLAGS , uint32(0));
        // UpdateObject();

        // remove me as an attacker from the AttackMap
        m_attackers.clear();
        m_deathState = CORPSE;
    }

    if(m_deathTimer > 0)
    {
        if(p_time >= m_deathTimer)
            m_deathTimer = 0;
        else
            m_deathTimer -= p_time;

        if (m_deathTimer <= 0)
        {
            // time to respawn!
            Log::getSingleton( ).outDetail("Removing corpse...");
#ifndef ENABLE_GRID_SYSTEM
            RemoveFromMap();
#else
        // TheDeadManager::Instance().Dead(me);
        // MapManager::Instance().GetMap(m_mapId)->Remove(this);
        this->RemoveFromWorld();
#endif
            m_respawnTimer = m_respawnDelay;
            setDeathState(DEAD);

            m_positionX = respawn_cord[0];
            m_positionY = respawn_cord[1];
            m_positionZ = respawn_cord[2];

            return;
        }
    }
    else if (m_respawnTimer > 0)
    {
        if(p_time >= m_respawnTimer)
            m_respawnTimer = 0;
        else
            m_respawnTimer -= p_time;

        if(m_respawnTimer <= 0)
        {
            // WorldPacket data;
            Log::getSingleton( ).outDetail("Respawning...");
            SetUInt32Value(UNIT_FIELD_HEALTH, GetUInt32Value(UNIT_FIELD_MAXHEALTH));
#ifndef ENABLE_GRID_SYSTEM
            PlaceOnMap();
#else
        this->AddToWorld(); // ?? not sure
        // TheDeadManager::Instance().Revive(this);
#endif
            setDeathState(ALIVE);
            m_creatureState = STOPPED;            // after respawn monster can move
        }
    }

    // FIXME
    if (isAlive())
    {
        if(m_attackers.empty())
        {
            RegenerateAll();
        }

        if (GetTypeId() != TYPEID_CORPSE)
            UpdateMobMovement( p_time );

        AI_Update();

        // Clear the NPC flags bit so it doesn't get auto- updated each frame. NPC flags are set per player and this would ruin is
        // unsetUpdateMaskBit(UNIT_NPC_FLAGS);
        // UpdateObject();
    }
}


void Creature::Create (uint32 guidlow, const char* name, uint32 mapid, float x, float y, float z, float ang)
{
    Object::_Create(guidlow, HIGHGUID_UNIT, mapid, x, y, z, ang);

    respawn_cord[0] = x;
    respawn_cord[1] = y;
    respawn_cord[2] = z;

    m_name = name;

    AI_SendCreaturePacket( guidlow );
}


/// Quests
uint32 Creature::getQuestStatus(Player *pPlayer)
{
    for( std::list<uint32>::iterator i = mQuestIds.begin( ); i != mQuestIds.end( ); ++ i )
    {
        uint32 quest_id = *i;
        uint32 status = pPlayer->getQuestStatus(quest_id);

        if (status == 0 || status == QUEST_STATUS_UNAVAILABLE)
        {
            Quest *pQuest = objmgr.GetQuest(quest_id);
            // if 0, then the player has never been offered this before
            // Add it to the player with a new quest value of 4
            if (pQuest->m_requiredLevel >= pPlayer->GetUInt32Value(UNIT_FIELD_LEVEL))
                status = pPlayer->addNewQuest(quest_id,2);
            else
                status = pPlayer->addNewQuest(quest_id);
        }

        if (status != QUEST_STATUS_COMPLETE)
            return status;
    }

    return 0;
}


uint32 Creature::getCurrentQuest(Player *pPlayer)
{
    for( std::list<uint32>::iterator i = mQuestIds.begin( ); i != mQuestIds.end( ); ++ i )
    {
        uint32 quest_id = *i;
        uint32 status = pPlayer->getQuestStatus(quest_id);

        if (status == 0)
        // if 0, then the player has never been offered this before
        // Add it to the player with a new quest value of 4
            status = pPlayer->addNewQuest(quest_id);

        if (status != QUEST_STATUS_COMPLETE)      // if quest is not completed yet, then this is the active quest to return
            return quest_id;
    }

    return 0;
}


char* Creature::getQuestTitle(uint32 quest_id)
{
    Quest *pQuest = objmgr.GetQuest(quest_id);
    return (char*)pQuest->m_title.c_str();
}


char* Creature::getQuestDetails(uint32 quest_id)
{
    Quest *pQuest = objmgr.GetQuest(quest_id);
    return (char*)pQuest->m_details.c_str();
}


char* Creature::getQuestObjectives(uint32 quest_id)
{
    Quest *pQuest = objmgr.GetQuest(quest_id);
    return (char*)pQuest->m_objectives.c_str();
}


char* Creature::getQuestCompleteText(uint32 quest_id)
{
    Quest *pQuest = objmgr.GetQuest(quest_id);
    return (char*)pQuest->m_completedText.c_str();
}


char* Creature::getQuestIncompleteText(uint32 quest_id)
{
    Quest *pQuest = objmgr.GetQuest(quest_id);
    return (char*)pQuest->m_incompleteText.c_str();
}


bool Creature::hasQuest(uint32 quest_id)
{
    for( std::list<uint32>::iterator i = mQuestIds.begin( ); i != mQuestIds.end( ); ++ i )
    {
        if (*i == quest_id)
            return true;
    }

    return false;
}


/*
int Creature::CheckQuestGiverFlag(Player *pPlayer, UpdateMask *unitMask, WorldPacket * data)
{
    for( std::list<uint32>::iterator i = mQuestIds.begin( ); i != mQuestIds.end( ); ++ i )
    {
        uint32 quest_id = *i;
        uint32 status = pPlayer->getQuestStatus(quest_id);
        Quest *pQuest = objmgr.getQuest(quest_id);
        // if (status != 0)
        // {
        if (pQuest->m_targetGuid != 0 && pQuest->m_targetGuid != m_guid[0] && status == QUEST_STATUS_INCOMPLETE)
        {
            // If this is a talk to quest, and the target NPC is not THIS npc, and the status is Incomplete,...
            // Set NPC_FLAGS to 0 so it doesn't offer a quest to this player
            SetUInt32Value(UNIT_NPC_FLAGS, 0);
            CreateObject(unitMask, data, 0);
            SetUInt32Value(UNIT_NPC_FLAGS, 2);
            return 1;
        }
        else if (pQuest->m_targetGuid == m_guid[0] && (status == QUEST_STATUS_COMPLETE || status == QUEST_STATUS_INCOMPLETE))
        {
            // If this creature has a Talk To quest, and it is the target of the quest, and the quest is either complete or currently
            // underway, then allow this creature to have quest flags
            SetUInt32Value(UNIT_NPC_FLAGS, 2);
            CreateObject(unitMask, data, 0);
            SetUInt32Value(UNIT_NPC_FLAGS, 0);
            return 1;
        }
        else if (pQuest->m_targetGuid == m_guid[0] && (status == QUEST_STATUS_AVAILABLE || status == 0))
        {
            // If this Creature has a Talk to quest, and is the target of the quest, and the quest is currently available,
            // Remove Questgiver flags
            SetUInt32Value(UNIT_NPC_FLAGS, 0);
            CreateObject(unitMask, data, 0);
            SetUInt32Value(UNIT_NPC_FLAGS, 2);
            return 1;
        }
        // }
    }
    return 0;
}
*/

///////////
/// Looting

void Creature::generateLoot()
{
    memset(item_list, 0, 8*128);
    itemcount = 0; //set Total Unit Item count to 0
    int LootValue = 0, MaxLootValue = 0;
    
    // max items to give for this creature
    int itemsToGet = 0;
    int creature_level = getLevel();
    if(creature_level < 10)
    {
        itemsToGet = rand()%2; // 0 or 1 items
    }
    else if(creature_level < 25)
    {
        itemsToGet = rand()%3; // 0 to 2 items
    }
    else if(creature_level < 40)
    {
        itemsToGet = rand()%4; // 0 to 3 items
    }
    else if(creature_level < 60)
    {
        itemsToGet = rand()%5; // 0 to 4 items
    }
    else if(creature_level < 80)
    {
        itemsToGet = rand()%6; // 0 to 5 items
    }
    else 
    {
        itemsToGet = rand()%7; // 0 to 6 items
    }
    
    m_lootMoney = (uint32)(creature_level * (rand()%5 + 1)*sWorld.getRate(RATE_DROP)); //generate copper    
    
    if( itemsToGet == 0 )
    return; // sorry dude.. nothing for you

    // Generate max value
    MaxLootValue = (int)(((creature_level * (rand()%40+50))/5)*sWorld.getRate(RATE_DROP)+rand()%5+5);

    /*
      We need an algorithm that mimic the randomless given the probabilities of each items.
      The algorithm must not affected by the order the items in the list and only
      by the probability they assigned to.  Note, this is important due to the fact
      that there's a max loot item.
      Algorithm:
         Given N items, randomly draw an item form the list (and remove it)
     Generate the probability and compare against the item's assigned probability
     If max items has not been achieved, repeat the whole process with N-1
     recursively repeat until either all items has been drawn or itemsToGet fulfilled.
     */

    const LootMgr::LootList &loot_list(LootMgr::getSingleton().getCreaturesLootList(GetUInt32Value(OBJECT_FIELD_ENTRY)));
    bool not_done = (loot_list.size()  && itemsToGet);
    std::vector<short> indexes(loot_list.size());
    std::generate(indexes.begin(), indexes.end(), SequenceGen());
    sLog.outDebug("Number of items to get %d", itemsToGet);
    
    while (not_done)
    {
    // generate the item you need to pick
    int idx = rand()%indexes.size();
    const LootItem &item(loot_list[indexes[idx]]);
    indexes.erase(indexes.begin()+idx);
    ItemPrototype *pCurItem = objmgr.GetItemPrototype(item.itemid);
    
    if( pCurItem != NULL && item.chance >= (rand()%100) )
    {
        if( !(LootValue > MaxLootValue) )
        {
        LootValue += pCurItem->BuyPrice;
        addItem(item.itemid, 1);        
        --itemsToGet;
        }
    }
    
    not_done = (itemsToGet && indexes.size() && !(LootValue > MaxLootValue));
    }
}




///////////////
/// Creature AI

// This Creature has just been attacked by someone
// Reaction: Add this Creature to the list of current attackers
void Creature::AI_AttackReaction(Unit *pAttacker, uint32 damage_dealt)
{
    if(!m_useAI)
        return;

    WPAssert(pAttacker);

    AttackerSet::iterator itr;
    for (itr = m_attackers.begin(); itr != m_attackers.end(); ++itr)
    {
        if (*itr == pAttacker)
            return;                               // Attacker already in list
    }

    if(m_canMove)
    {
        //move to attacker when attacked
        m_moveSpeed = 7.0f*0.001f;
        AI_SendMoveToPacket(m_positionX, m_positionY, m_positionZ, 100, 1);
        m_moveTimer = 0;
        m_destinationX = m_destinationY = m_destinationZ = 0;
        m_timeMoved = 0;
        m_timeToMove = 0;
        // m_creatureState = MOVING; //makes them fall through the ground

        // m_creatureState = STOPPED;
        // AI_MoveTo(m_positionX, m_positionY, m_positionZ, true);
    }

    m_attackers.insert(pAttacker);
}


void Creature::AI_Update()
{
    if(!m_useAI)
        return;

    Unit *closestTarget = NULL;
    float targetDistanceSq = 10000.0f;

    // Cycle through attackers
    // If one is out of range, remove from the map
    AttackerSet::iterator itr;
    if(m_attackers.empty() && m_creatureState == ATTACKING)
    {
        m_creatureState = STOPPED;                // after killing all attackers they can start moving again
    }

    for (itr = m_attackers.begin(); itr != m_attackers.end(); )
    {
        Unit *pVictim = *itr;
        WPAssert(pVictim);

        if (!pVictim->isAlive())
        {
            m_attackers.erase(itr++);
            continue;
        }

        float lengthSq = GetDistanceSq(pVictim);

        if (lengthSq > 30.0f*30.0f)               // must be greater than the max range of spells
        {
            // stop attacking because the target is too far
            m_attackers.erase(itr++);
            continue;
        }

        if (lengthSq < targetDistanceSq)
        {
            closestTarget = *itr;
            targetDistanceSq = lengthSq;
        }

        ++itr;
    }

    if(m_creatureState==MOVING)
        return;

    if(getdistance(m_positionX,m_positionY,respawn_cord[0],respawn_cord[1])>50)
    {
        m_attackers.clear();
        AI_MoveTo(respawn_cord[0],respawn_cord[1],respawn_cord[2],true);
    }
    else  if( closestTarget )
    {
        float targetDistance = sqrt(targetDistanceSq);
        float reach = GetFloatValue(UNIT_FIELD_COMBATREACH);
        float radius = GetFloatValue(UNIT_FIELD_BOUNDINGRADIUS);
        if (targetDistance > (reach + radius))
        {
            float q = (targetDistance - radius)/targetDistance;
            float x = (m_positionX + closestTarget->GetPositionX()*q)/(1+q);
            float y = (m_positionY + closestTarget->GetPositionY()*q)/(1+q);
            float z = (m_positionZ + closestTarget->GetPositionZ()*q)/(1+q);
            m_destinationX = x;
            m_destinationY = y;
            m_destinationZ = z;

            if(m_canMove)
            {
                m_creatureState = STOPPED;
                AI_MoveTo(x, y, z, true);
            }
        }
        else
        {
            AI_ChangeState(ATTACKING);
            if (isAttackReady())
            {
/*
                if(!isInFront(closestTarget,10.00000f))
                {
                    if(setInFront(closestTarget, 10.00000f))
                    {
                        setAttackTimer(0);
                        AttackerStateUpdate(closestTarget, 0);
                    }
                    else
                        setAttackTimer(uint32(500));
                }
                else
                {
*/
                //CreatureInfo* ci = objmgr.GetCreatureName(GetGUID());
                
                
                // UQ1: Add damage values...
#if !defined ( _VERSION_1_7_0_ ) && !defined ( _VERSION_1_8_0_ )
                uint32 minDmg = (this->GetUInt32Value(UNIT_FIELD_MINDAMAGE));
                uint32 maxDmg = (this->GetUInt32Value(UNIT_FIELD_MAXDAMAGE));
#else //_VERSION_1_7_0_
                // Damage values in the new DB are too high... All over 1000... So..
                uint32 minDmg = irand(0, getLevel());
                uint32 maxDmg = irand(minDmg, getLevel());

                if (getLevel() < 20)
                    maxDmg*=(getLevel()*0.3);
#endif //_VERSION_1_7_0_
                
                // Fix for bad creature info...
                if (maxDmg > 32768)
                {
                    if (minDmg > 32768)
                        maxDmg = irand(0, getLevel());
                    else
                        maxDmg = irand(minDmg, getLevel());
                    //maxDmg = 32767; // For integer random function... 32767 should be plenty anyway.. Think its just a db error.
                }

                if (minDmg > 32768)
                {
                    minDmg = irand(minDmg, getLevel());
                    //maxDmg = 32767; // For integer random function... 32767 should be plenty anyway.. Think its just a db error.
                }

                uint32 damge = irand(minDmg, maxDmg);

                setAttackTimer(0);
                AttackerStateUpdate(closestTarget, damge);
                // }

            }
        }
    }
}


void Creature::AI_SendMoveToPacket(float x, float y, float z, uint32 time, bool run)
{
    WorldPacket data;
    data.Initialize( SMSG_MONSTER_MOVE );
    data << GetGUID();
    data << GetPositionX() << GetPositionY() << GetPositionZ() << GetOrientation();
    data << uint8(0);
    data << uint32(run ? 0x00000100 : 0x00000000);
    data << time;
    data << uint32(1);
    data << x << y << z;
    WPAssert( data.size() == 49 );
    SendMessageToSet( &data, false );
#ifdef ENABLE_GRID_SYSTEM
    MapManager::Instance().GetMap(m_mapId)->ObjectRelocation<Creature>(this, x, y, z, m_orientation);
#endif
}

/*
if (mobile1 != null)
            {
                int num1 = 4;
                Converter.ToBytes(id, this.tempBuff, ref num1);
                Converter.ToBytes(mobile1.Name, this.tempBuff, ref num1);
                Converter.ToBytes((byte) 0, this.tempBuff, ref num1);
                if (mobile1.Name2 != null)
                {
                    Converter.ToBytes(mobile1.Name2, this.tempBuff, ref num1);
                    Converter.ToBytes((byte) 0, this.tempBuff, ref num1);
                }
                else
                {
                    Converter.ToBytes((byte) 0, this.tempBuff, ref num1);
                }
                Converter.ToBytes((byte) 0, this.tempBuff, ref num1);
                Converter.ToBytes((byte) 0, this.tempBuff, ref num1);
                if (mobile1.Guild != null)
                {
                    Converter.ToBytes(mobile1.Guild, this.tempBuff, ref num1);
                    Converter.ToBytes((byte) 0, this.tempBuff, ref num1);
                }
                else
                {
                    Converter.ToBytes((byte) 0, this.tempBuff, ref num1);
                }
                Converter.ToBytes(mobile1.Flags1, this.tempBuff, ref num1);
                if ((mobile1.NpcType & 2) > 0)
                {
                    Converter.ToBytes(7, this.tempBuff, ref num1);
                }
                else
                {
                    Converter.ToBytes(0, this.tempBuff, ref num1);
                }
                Converter.ToBytes(mobile1.NpcType, this.tempBuff, ref num1);
                Converter.ToBytes(mobile1.Unk4, this.tempBuff, ref num1);
                Converter.ToBytes(0, this.tempBuff, ref num1);
                Converter.ToBytes(0, this.tempBuff, ref num1);
                this.Send(OpCodes.SMSG_CREATURE_QUERY_RESPONSE, this.tempBuff, num1);
            }
*/

void Creature::AI_SendCreaturePacket( uint32 guidlow )
{
/*    WorldPacket data;
    uint32 entry = guidlow;
    uint64 guid = GetGUID();
    CreatureInfo *ci;

    guid = GetGUID();

    ci = objmgr.GetCreatureName(entry);
    Log::getSingleton( ).outDetail("WORLD: CMSG_CREATURE_QUERY '%s' - entry: %u guid: %u", ci->Name.c_str());

    data.Initialize( SMSG_CREATURE_QUERY_RESPONSE );
    data << (uint32)GetGUID();//entry;
    
    
    data << ci->Name.c_str();
    //data << uint8(0) << uint8(0) << uint8(0);
    data << uint32(0);
    if (stricmp(ci->SubName.c_str(), ""))
        data << ci->SubName.c_str();                  // Subname

    data << uint32(0);
    data << uint32(0);
    data << uint32(0);

    //if (mobile1.Guild != null)
    //    data << uint32(0);

    data << uint32(0);

    data << uint32(0); //flags

    if ((ci->Type & 2) > 0)
    {
        data << uint32(7);
    }
    else
    {
        data << uint32(0);
    }
    data << uint32(ci->Type);

    data << ci->unknown4;                         // unknown 5
    data << uint32(0);
    data << uint32(0);

//    data << ci->DisplayID;                        // DisplayID
*/    

    //UQ1: WowwoW Style...
/*    data << ci->SubName.c_str();
    data << ci->Name.c_str();
    
    data << uint8(0);
    data << uint8(0);
    // UQ1: This one is actually for guild id..
    //if (mobile1.Guild != null)
    //    data << uint32(0); // guild id, if it has one??...
    data << uint8(0); //
    data << uint32(0); // Flags

    if ((ci->Type & 2) > 0)
    {
        data << uint8(7);
    }
    else
    {
        data << uint32(0);
    }

    data << ci->Type;                             // Creature Type
    data << ci->unknown4;                         // unknown 4
    data << uint32(0);
    data << ci->DisplayID;                        // DisplayID*/

    //SendPacket( &data );
//    SendMessageToSet( &data, false );
}

void Creature::AI_MoveTo(float x, float y, float z, bool run)
{
#ifndef ENABLE_GRID_SYSTEM
    float dx = x - m_positionX;
    float dy = y - m_positionY;
    float dz = z - m_positionZ;

    float distance = sqrt((dx*dx) + (dy*dy) + (dz*dz));
    if(!distance)
        return;

    m_destinationX = x;
    m_destinationY = y;
    m_destinationZ = z;

    float speed=0;
    if(!run)
        m_moveSpeed = 2.5f*0.001f;
    else
        m_moveSpeed = 7.0f*0.001f;

    uint32 moveTime = (uint32) (distance / m_moveSpeed);
    AI_SendMoveToPacket(x, y, z, moveTime, run);

    m_timeToMove = moveTime;
    // update every 300 msecs
    m_moveTimer =  UNIT_MOVEMENT_INTERPOLATE_INTERVAL;

    if(m_creatureState != MOVING)
        m_creatureState = MOVING;
#else
    float dx = x - m_positionX;
    float dy = y - m_positionY;
    float dz = z - m_positionZ;

    m_destinationX = x;
    m_destinationY = y;
    m_destinationZ = z;

    float distance = sqrt((dx*dx) + (dy*dy) + (dz*dz));
    if(!distance)
        return;

    float speed=0;
    if(!run)
        m_moveSpeed = 2.5f*0.001f;
    else
        m_moveSpeed = 7.0f*0.001f;

    uint32 moveTime = (uint32) (distance / m_moveSpeed);
    AI_SendMoveToPacket(x, y, z, moveTime, run);
    m_timeToMove = moveTime;
    // update every 300 msecs
    m_moveTimer =  UNIT_MOVEMENT_INTERPOLATE_INTERVAL;

    if(m_creatureState != MOVING)
        m_creatureState = MOVING;
#endif
}


bool Creature::addWaypoint(float x, float y, float z)
{
    if(m_nWaypoints+1 >= MAX_CREATURE_WAYPOINTS)
        return false;

    m_waypoints[m_nWaypoints][0] = x;
    m_waypoints[m_nWaypoints][1] = y;
    m_waypoints[m_nWaypoints][2] = z;

    m_nWaypoints++;

    return true;
}


void Creature::SaveToDB()
{
    std::stringstream ss;
    ss << "DELETE FROM creatures WHERE id=" << GetGUIDLow();
    sDatabase.Execute(ss.str().c_str());

    ss.rdbuf()->str("");
    ss << "INSERT INTO creatures (id, mapId, zoneId, name_id, positionX, positionY, positionZ, orientation, data) VALUES ( "
        << GetGUIDLow() << ", "
        << GetMapId() << ", "
        << GetZoneId() << ", "
        << GetUInt32Value(OBJECT_FIELD_ENTRY) << ", "
        << m_positionX << ", "
        << m_positionY << ", "
        << m_positionZ << ", "
        << m_orientation << ", '";

    for( uint16 index = 0; index < m_valuesCount; index ++ )
        ss << GetUInt32Value(index) << " ";

    ss << "' )";

    sDatabase.Execute( ss.str( ).c_str( ) );

    // TODO: save vendor items etc?
}


// TODO: use full guids
void Creature::LoadFromDB(uint32 guid)
{

    std::stringstream ss;
    ss << "SELECT * FROM creatures WHERE id=" << guid;

    QueryResult *result = sDatabase.Query( ss.str().c_str() );
    ASSERT(result);

    Field *fields = result->Fetch();

    // _Create( guid, 0 );

    Create(fields[8].GetUInt32(), objmgr.GetCreatureName(fields[8].GetUInt32())->Name.c_str(), fields[6].GetUInt32(),
        fields[1].GetFloat(), fields[2].GetFloat(), fields[3].GetFloat(), fields[4].GetFloat());

    m_zoneId = fields[5].GetUInt32();

    m_moveRandom = fields[9].GetBool();
    m_moveRun = fields[10].GetBool();

    LoadValues(fields[7].GetString());

    
    // UQ1: Added nameID.
    SetNameId(fields[8].GetUInt32());
    //m_nameId = fields[8].GetUInt32();
    //Log::getSingleton( ).outDebug( "Added creature %u (%u) - %s.", m_nameId, GetNameID(), objmgr.GetCreatureName(fields[8].GetUInt32())->Name.c_str() );

    delete result;

    if ( HasFlag( UNIT_NPC_FLAGS, UNIT_NPC_FLAG_VENDOR ) )
        _LoadGoods();

    if ( HasFlag( UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER ) )
        _LoadQuests();

    _LoadMovement();
}


void Creature::_LoadGoods()
{
    // remove items from vendor
    itemcount = 0;

    // load his goods
    std::stringstream query;
    query << "SELECT * FROM vendors WHERE vendorGuid=" << GetGUIDLow();

    QueryResult *result = sDatabase.Query( query.str().c_str() );
    if(result)
    {
        do
        {
            Field *fields = result->Fetch();

            if (getItemCount() >= MAX_CREATURE_ITEMS)
            {
                // this should never happen unless someone has been fucking with the dbs
                // complain and break :P
                Log::getSingleton( ).outError( "Vendor %u has too many items (%u >= %i). Check the DB!", GetGUIDLow(), getItemCount(), MAX_CREATURE_ITEMS );
                break;
            }

            setItemId(getItemCount() , fields[1].GetUInt32());
            setItemAmount(getItemCount() , fields[2].GetUInt32());
            increaseItemCount();

        }
        while( result->NextRow() );

        delete result;
    }
}


void Creature::_LoadQuests()
{
    // clean quests
    mQuestIds.clear();

    std::stringstream query;
    query << "SELECT * FROM creaturequestrelation WHERE creatureId=" << GetGUIDLow() << " ORDER BY questId";

    std::auto_ptr<QueryResult> result(sDatabase.Query( query.str().c_str() ));
    if(result.get() != NULL)
    {
        do
        {
            Field *fields = result->Fetch();
            addQuest(fields[1].GetUInt32());
        }
        while( result->NextRow() );
    }
}


void Creature::_LoadMovement()
{
    // clean waypoint list
    m_nWaypoints = 0;
    m_currentWaypoint = 0;

    std::stringstream query;
    query << "SELECT X,Y,Z FROM creatures_mov WHERE creatureId=" << GetGUIDLow();

    std::auto_ptr<QueryResult> result(sDatabase.Query( query.str().c_str() ));
    if(result.get() != NULL)
    {
        do
        {
            Field *fields = result->Fetch();

            addWaypoint( fields[0].GetFloat(), fields[1].GetFloat(), fields[2].GetFloat());
        }
        while( result->NextRow() );
    }
}


void Creature::DeleteFromDB()
{
    char sql[256];

    sprintf(sql, "DELETE FROM creatures WHERE id=%u", GetGUIDLow());
    sDatabase.Execute(sql);
    sprintf(sql, "DELETE FROM vendors WHERE vendorGuid=%u", GetGUIDLow());
    sDatabase.Execute(sql);
    sprintf(sql, "DELETE FROM trainers WHERE trainerGuid=%u", GetGUIDLow());
    sDatabase.Execute(sql);
    sprintf(sql, "DELETE FROM creaturequestrelation WHERE creatureId=%u", GetGUIDLow());
    sDatabase.Execute(sql);
}

//add by vendy Check Attack distance  
float Creature::GetAttackDistance(Unit *pl)
{
    uint16 playlevel     = (uint16)pl->GetUInt32Value(UNIT_FIELD_LEVEL);
    uint16 creaturelevel = (uint16)this->GetUInt32Value(UNIT_FIELD_LEVEL);
    int16 leveldif      = playlevel - creaturelevel;

    float RetDistance=10.0;

    if ( leveldif > 9 )
    { 
        RetDistance = 3;            
    }
    else
    {
        if (leveldif > 0)
            RetDistance =  10 *  GetFloatValue(UNIT_FIELD_COMBATREACH) - 2*(float)leveldif;
        else
            RetDistance = 10 *  GetFloatValue(UNIT_FIELD_COMBATREACH);
        RetDistance = RetDistance>50?50:RetDistance;
        RetDistance = RetDistance<3?3:RetDistance;
    }

    return RetDistance;
}
