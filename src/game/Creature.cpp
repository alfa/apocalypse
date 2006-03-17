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

                m_positionX = m_destinationX;
                m_positionY = m_destinationY;
                m_positionZ = m_destinationZ;

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
                float q = (float)m_timeMoved / (float)m_timeToMove;
                m_positionX += (m_destinationX - m_positionX) * q;
                m_positionY += (m_destinationY - m_positionY) * q;
                m_positionZ += (m_destinationZ - m_positionZ) * q;

                m_timeToMove -= m_timeMoved;
                m_timeMoved = 0;

                AI_SendMoveToPacket(m_destinationX, m_destinationY, m_destinationZ, m_timeToMove, m_moveSpeed == 7.0*0.001);

                m_moveTimer = (UNIT_MOVEMENT_INTERPOLATE_INTERVAL < m_timeToMove) ? UNIT_MOVEMENT_INTERPOLATE_INTERVAL : m_timeToMove;
            }
        }                                         // still moving
    }
    // creature is stoped
    else if(m_creatureState == STOPPED && !m_moveTimer && !m_timeMoved)
    {
        if(sWorld.getAllowMovement() == false)    //is creature movement enabled?
            return;

    // if Spirit Healer don't move
        if(GetUInt32Value(UNIT_FIELD_DISPLAYID) == 5233)
            return;

        int destpoint;
        // If creature has no waypoints just wander aimlessly around spawnpoint
        if(m_nWaypoints==0)                       //no waypoints
        {
            if(m_moveRandom)
            {
                if((rand()%10)==0)
                {
                    float wanderDistance=rand()%4+2;
                    float wanderX=((wanderDistance*rand())/RAND_MAX)-wanderDistance/2;
                    float wanderY=((wanderDistance*rand())/RAND_MAX)-wanderDistance/2;
                    float wanderZ=0;              // FIX ME ( i dont know how to get apropriate Z coord, maybe use client height map data)

                    if(getdistance(m_positionX,m_positionY,respawn_cord[0],respawn_cord[1])>10)
                    {
                        //return home
                        AI_MoveTo(respawn_cord[0],respawn_cord[1],respawn_cord[2],false);
                    }
                    else
                    {
                        AI_MoveTo(m_positionX+wanderX,m_positionY+wanderY,m_positionZ+wanderZ,false);
                    }
                }
            }
            return;
        }
        else                                      //we do have waypoints
        {
            if(m_moveRandom)                      //is random move on if so move to a random waypoint
            {
                // if(m_nWaypoints > 1)
                destpoint = rand() % m_nWaypoints;
            }
            else                                  //random move is not on lets follow the path
            {
                // Are we on the last waypoint? if so walk back
                if (m_currentWaypoint == (m_nWaypoints-1))
                    m_moveBackward = true;
                if (m_currentWaypoint == 0)       // Are we on the first waypoint? if so lets goto the second waypoint
                    m_moveBackward = false;
                if (!m_moveBackward)              // going 0..n
                    destpoint = ++m_currentWaypoint;
                else                              // going (n-1)..0
                    destpoint = --m_currentWaypoint;
            }
            if(m_canMove)
                AI_MoveTo(m_waypoints[destpoint][0], m_waypoints[destpoint][1], m_waypoints[destpoint][2], m_moveRun);
        }
    }
}


void Creature::Update( uint32 p_time )
{
    Unit::Update( p_time );

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

            RemoveFromMap();

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
            PlaceOnMap();
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
    int i;
    
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
	    int item_level = pCurItem->ItemLevel;
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
                setAttackTimer(0);
                AttackerStateUpdate(closestTarget, 0);
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
}


void Creature::AI_MoveTo(float x, float y, float z, bool run)
{
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
                Log::getSingleton( ).outError( "Vendor %u has too many items. Check the DB!", GetGUIDLow() );
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
