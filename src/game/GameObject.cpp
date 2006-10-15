/* 
 * Copyright (C) 2005,2006 MaNGOS <http://www.mangosproject.org/>
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
#include "QuestDef.h"
#include "GameObject.h"
#include "ObjectMgr.h"
#include "UpdateMask.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "Database/DatabaseEnv.h"
#include "MapManager.h"
#include "LootMgr.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"

GameObject::GameObject() : Object()
{
    m_objectType |= TYPE_GAMEOBJECT;
    m_objectTypeId = TYPEID_GAMEOBJECT;

    m_valuesCount = GAMEOBJECT_END;
    m_respawnTimer = 0;
    m_respawnDelayTime = 25000;
    m_lootState = GO_CLOSED;
    m_lootskill = true;

    lootid=0;
}

GameObject::~GameObject()
{
    if(m_uint32Values)                                      // field array can be not exist if GameOBject not loaded 
    {
        // crash possable at access to deleted GO in Unit::m_gameobj
        uint64 owner_guid = GetOwnerGUID();
        if(owner_guid)
        {
            Unit* owner = ObjectAccessor::Instance().GetUnit(*this,owner_guid);
            if(owner)
                owner->RemoveGameObject(this,false);
            else if(GUID_HIPART(owner_guid)!=HIGHGUID_PLAYER)
                sLog.outError("Delete GameObject (GUID: %u Entry: %u ) that have references in not found creature %u GO list. Crash possable later.",GetGUIDLow(),GetGOInfo()->id,GUID_LOPART(owner_guid));
        }
    }
}

bool GameObject::Create(uint32 guidlow, uint32 name_id, uint32 mapid, float x, float y, float z, float ang, float rotation0, float rotation1, float rotation2, float rotation3)
{
    m_positionX = x;
    m_positionY = y;
    m_positionZ = z;
    m_orientation = ang;

    m_mapId = mapid;

    if(!IsPositionValid())
    {
        sLog.outError("ERROR: Gameobject (GUID: %u Entry: %u ) not created. Suggested coordinates isn't valid (X: %d Y: ^%d)",guidlow,name_id,x,y);
        return false;
    }

    Object::_Create(guidlow, HIGHGUID_GAMEOBJECT);

    GameObjectInfo const* goinfo = objmgr.GetGameObjectInfo(name_id);

    if (!goinfo)
    {
        sLog.outError("Gameobject Create not exist entry. guidlow: %u id: %u map: %u  (X: %f Y: %f Z: %f) ang: %f rotation0: %f rotation1: %f ortation2: %f rotation3: %f",guidlow, name_id, mapid, x, y, z, ang, rotation0, rotation1, rotation2, rotation3);
        return false;
    }

    //    SetUInt32Value(GAMEOBJECT_TIMESTAMP, (uint32)time(NULL));
    SetFloatValue(GAMEOBJECT_POS_X, x);
    SetFloatValue(GAMEOBJECT_POS_Y, y);
    SetFloatValue(GAMEOBJECT_POS_Z, z);
    SetFloatValue(GAMEOBJECT_FACING, ang);                  //this is not facing angle
    SetFloatValue(OBJECT_FIELD_SCALE_X, goinfo->size);

    SetUInt32Value(GAMEOBJECT_FACTION, goinfo->faction);
    SetUInt32Value(GAMEOBJECT_FLAGS, goinfo->flags);
    m_flags = goinfo->flags;

    SetUInt32Value (OBJECT_FIELD_ENTRY, goinfo->id);

    SetUInt32Value (GAMEOBJECT_DISPLAYID, goinfo->displayId);

    SetUInt32Value (GAMEOBJECT_STATE, 1);
    SetUInt32Value (GAMEOBJECT_TYPE_ID, goinfo->type);

    SetFloatValue (GAMEOBJECT_ROTATION, rotation0);
    SetFloatValue (GAMEOBJECT_ROTATION+1, rotation1);
    SetFloatValue (GAMEOBJECT_ROTATION+2, rotation2);
    SetFloatValue (GAMEOBJECT_ROTATION+3, rotation3);
    return true;
}

void GameObject::Update(uint32 p_time)
{
    WorldPacket     data;

    switch (m_lootState)
    {
        case GO_CLOSED:
            if (m_respawnTimer > 0)
            {
                if (m_respawnTimer > p_time)
                {
                    m_respawnTimer -= p_time;
                }
                else
                {
                    m_respawnTimer = 0;
                    if (GetGoType() != GAMEOBJECT_TYPE_TRAP)
                        MapManager::Instance().GetMap(GetMapId())->Add(this);
                }
            }
            break;
        case GO_OPEN:
            break;
        case GO_LOOTED:
            loot.clear();
            SetLootState(GO_CLOSED);
            SetLootSkill(true);

            data.Initialize(SMSG_DESTROY_OBJECT);
            data << GetGUID();
            SendMessageToSet(&data, true);
            m_respawnTimer = m_respawnDelayTime;
            break;
    }

    SpellEntry *createSpell = sSpellStore.LookupEntry(m_spellId);
    if (!createSpell)
        return;
    int i;
    for (i = 0; i < 3; i++)
        if (createSpell->Effect[i] == SPELL_EFFECT_SUMMON_OBJECT_SLOT1)
            break;
    if (i<3)
    {
        // traps
        CellPair p(MaNGOS::ComputeCellPair(GetPositionX(),GetPositionY()));
        Cell cell = RedZone::GetZone(p);
        cell.data.Part.reserved = ALL_DISTRICT;

        Unit* ok = NULL, *owner = GetOwner();
        if (!owner)
        {
            m_respawnTimer = 0;
            return;
        }

        float radius = GetRadius(sSpellRadius.LookupEntry(createSpell->EffectRadiusIndex[i]));
        MaNGOS::AnyUnfriendlyUnitInObjectRangeCheck u_check(this, owner, radius);
        MaNGOS::UnitSearcher<MaNGOS::AnyUnfriendlyUnitInObjectRangeCheck> checker(ok, u_check);

        TypeContainerVisitor<MaNGOS::UnitSearcher<MaNGOS::AnyUnfriendlyUnitInObjectRangeCheck>, TypeMapContainer<AllObjectTypes> > object_checker(checker);
        CellLock<GridReadGuard> cell_lock(cell, p);
        cell_lock->Visit(cell_lock, object_checker, *MapManager::Instance().GetMap(GetMapId()));
        if (ok)
        {
            owner->CastSpell(ok, GetGOInfo()->castsSpell, true);
            // removed on unit update
            m_respawnTimer = 0;
        }
    }
}

void GameObject::Delete()
{
    WorldPacket data;
    data.Initialize(SMSG_GAMEOBJECT_SPAWN_ANIM);
    data << GetGUID();
    SendMessageToSet(&data, true);
    SetUInt32Value(GAMEOBJECT_STATE, 1);
    SetUInt32Value(GAMEOBJECT_FLAGS, m_flags);
    data.Initialize(SMSG_DESTROY_OBJECT);
    data << GetGUID();
    SendMessageToSet(&data,true);
    //TODO: set timestamp
    RemoveFromWorld();
    ObjectAccessor::Instance().AddObjectToRemoveList(this);
}

void GameObject::getFishLoot(Loot *fishloot)
{
    uint32 zone = GetZoneId();
    FillLoot(0,fishloot,zone,LootTemplates_Fishing);
}

void GameObject::SaveToDB()
{
    std::ostringstream ss;
    sDatabase.PExecute("DELETE FROM `gameobject` WHERE `guid` = '%u'", GetGUIDLow());

    const GameObjectInfo *goI = GetGOInfo();

    if (!goI)
        return;

    ss << "INSERT INTO `gameobject` VALUES ( "
        << GetGUIDLow() << ", "
        << GetUInt32Value (OBJECT_FIELD_ENTRY) << ", "
        << GetMapId() << ", "
        << GetFloatValue(GAMEOBJECT_POS_X) << ", "
        << GetFloatValue(GAMEOBJECT_POS_Y) << ", "
        << GetFloatValue(GAMEOBJECT_POS_Z) << ", "
        << GetFloatValue(GAMEOBJECT_FACING) << ", "
        << GetFloatValue(GAMEOBJECT_ROTATION) << ", "
        << GetFloatValue(GAMEOBJECT_ROTATION+1) << ", "
        << GetFloatValue(GAMEOBJECT_ROTATION+2) << ", "
        << GetFloatValue(GAMEOBJECT_ROTATION+3) << ", "
        << lootid <<", "
        << m_respawnDelayTime <<")";

    sDatabase.Execute( ss.str( ).c_str( ) );
}

bool GameObject::LoadFromDB(uint32 guid, QueryResult *result)
{
    bool external = (result != NULL);
    if (!external)
        result = sDatabase.PQuery("SELECT `id`,`map`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`loot`,`respawntimer` FROM `gameobject` WHERE `guid` = '%u'", guid);

    if( !result )
    {
        sLog.outError("ERROR: Gameobject (GUID: %u) not found in table `gameobject`, can't load. ",guid);
        return false;
    }

    Field *fields = result->Fetch();
    uint32 entry = fields[0].GetUInt32();
    uint32 map_id=fields[1].GetUInt32();
    float x = fields[2].GetFloat();
    float y = fields[3].GetFloat();
    float z = fields[4].GetFloat();
    float ang = fields[5].GetFloat();

    float rotation0 = fields[6].GetFloat();
    float rotation1 = fields[7].GetFloat();
    float rotation2 = fields[8].GetFloat();
    float rotation3 = fields[9].GetFloat();

    if (!Create(guid,entry, map_id, x, y, z, ang, rotation0, rotation1, rotation2, rotation3) )
    {
        if (!external) delete result;
        return false;
    }

    lootid=fields[10].GetUInt32();
    m_respawnDelayTime=fields[11].GetUInt32();
    if (!external) delete result;

    _LoadQuests();
    return true;
}

void GameObject::DeleteFromDB()
{
    sDatabase.PExecute("DELETE FROM `gameobject` WHERE `guid` = '%u'", GetGUIDLow());
}

GameObjectInfo const *GameObject::GetGOInfo() const
{
    return objmgr.GetGameObjectInfo(GetUInt32Value (OBJECT_FIELD_ENTRY));
}

/*********************************************************/
/***                    QUEST SYSTEM                   ***/
/*********************************************************/

void GameObject::_LoadQuests()
{
    for( std::list<Quest*>::iterator i = mQuests.begin( ); i != mQuests.end( ); i++ )
        delete *i;
    mQuests.clear();
    for( std::list<Quest*>::iterator i = mInvolvedQuests.begin( ); i != mInvolvedQuests.end( ); i++ )
        delete *i;
    mInvolvedQuests.clear();

    Field *fields;
    Quest *pQuest;

    QueryResult *result = sDatabase.PQuery("SELECT `quest` FROM `gameobject_questrelation` WHERE `id` = '%u'", GetEntry ());

    if(result)
    {
        do
        {
            fields = result->Fetch();
            pQuest = objmgr.NewQuest( fields[0].GetUInt32() );
            if (!pQuest) continue;

            addQuest(pQuest);
        }
        while( result->NextRow() );

        delete result;
    }

    QueryResult *result1 = sDatabase.PQuery("SELECT `quest` FROM `gameobject_involvedrelation` WHERE `id` = '%u'", GetEntry ());

    if(!result1) return;

    do
    {
        fields = result1->Fetch();
        pQuest = objmgr.NewQuest( fields[0].GetUInt32() );
        if (!pQuest) continue;

        addInvolvedQuest(pQuest);
    }
    while( result1->NextRow() );

    delete result1;
}

bool GameObject::IsTransport() const
{
    GameObjectInfo const * gInfo = GetGOInfo();
    if(!gInfo) return false;
    return gInfo->type == GAMEOBJECT_TYPE_TRANSPORT || gInfo->type == GAMEOBJECT_TYPE_MO_TRANSPORT;
}

Unit* GameObject::GetOwner() const
{
    return ObjectAccessor::Instance().GetUnit(*this, GetOwnerGUID());
}
