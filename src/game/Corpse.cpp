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
#include "Corpse.h"
#include "Player.h"
#include "UpdateMask.h"
#include "MapManager.h"
#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "Opcodes.h"
#include "WorldSession.h"
#include "WorldPacket.h"
#include "GossipDef.h"

Corpse::Corpse() : Object()
{
    m_objectType |= TYPE_CORPSE;
    m_objectTypeId = TYPEID_CORPSE;

    m_valuesCount = CORPSE_END;

    m_POI = false;
    m_type = CORPSE_RESURRECTABLE;
}

bool Corpse::Create( uint32 guidlow )
{
    Object::_Create(guidlow, HIGHGUID_CORPSE);
    return true;
}

bool Corpse::Create( uint32 guidlow, Player *owner, uint32 mapid, float x, float y, float z, float ang )
{
    Object::_Create(guidlow, HIGHGUID_CORPSE, mapid, x, y, z, ang, (uint8)-1);

    if(!IsPositionValid())
    {
        sLog.outError("ERROR: Corpse (guidlow %d, owner %s) not created. Suggested coordinates isn't valid (X: %d Y: ^%d)",guidlow,owner->GetName(),x,y);
        return false;
    }

    SetFloatValue( OBJECT_FIELD_SCALE_X, 1 );
    SetFloatValue( CORPSE_FIELD_POS_X, x );
    SetFloatValue( CORPSE_FIELD_POS_Y, y );
    SetFloatValue( CORPSE_FIELD_POS_Z, z );
    SetFloatValue( CORPSE_FIELD_FACING, ang );
    SetUInt64Value( CORPSE_FIELD_OWNER, owner->GetGUID() );

    return true;
}

void Corpse::SaveToDB()
{
    // prevent DB data inconsistance problems and duplicates
    DeleteFromDB();

    std::ostringstream ss;
    ss  << "INSERT INTO `corpse` (`guid`,`player`,`position_x`,`position_y`,`position_z`,`orientation`,`zone`,`map`,`data`,`time`,`bones_flag`) VALUES ("
        << GetGUIDLow() << ", " << GetOwnerGUID() << ", " << GetPositionX() << ", " << GetPositionY() << ", " << GetPositionZ() << ", "
        << GetOrientation() << ", "  << GetZoneId() << ", "  << GetMapId() << ", '";
    for(uint16 i = 0; i < m_valuesCount; i++ )
        ss << GetUInt32Value(i) << " ";
    ss << "', NOW(), " << int(GetType()) << ")";
    sDatabase.Execute( ss.str().c_str() );

    // update grid table
    sDatabase.PExecute(
        "INSERT INTO `corpse_grid` (`guid`,`map`,`position_x`,`position_y`,`cell_position_x`,`cell_position_y` ) "
        "SELECT `guid`,`map`,((`position_x`-%f)/%f) + %u,((`position_y`-%f)/%f) + %u,((`position_x`-%f)/%f) + %u,((`position_y`-%f)/%f) + %u "
        "FROM `corpse` WHERE `guid` = '%u'", CENTER_GRID_OFFSET, SIZE_OF_GRIDS, CENTER_GRID_ID, CENTER_GRID_OFFSET,SIZE_OF_GRIDS, CENTER_GRID_ID,
        CENTER_GRID_CELL_OFFSET,SIZE_OF_GRID_CELL, CENTER_GRID_CELL_ID, CENTER_GRID_CELL_OFFSET, SIZE_OF_GRID_CELL, CENTER_GRID_CELL_ID, GetGUIDLow()
        );
    sDatabase.PExecute("UPDATE `corpse_grid` SET `grid`=(`position_x`*%u) + `position_y`,`cell`=((`cell_position_y` * %u) + `cell_position_x`) WHERE `guid` = '%u'", MAX_NUMBER_OF_GRIDS, TOTAL_NUMBER_OF_CELLS_PER_MAP,GetGUIDLow());
}

void Corpse::DeleteFromWorld(bool remove)
{
    ObjectAccessor::Instance().RemoveBonesFromPlayerView(this);
    MapManager::Instance().GetMap(GetMapId())->Remove(this,false);

    RemoveFromWorld();

    if(remove)
        delete this;
}

void Corpse::DeleteFromDB()
{
    std::ostringstream ss;
    if(GetType() == CORPSE_BONES)
        // only specific bones
        ss  << "DELETE FROM `corpse` WHERE `guid` = '" << GetGUIDLow() << "'";
    else
        // all corpses (not bones)
        ss  << "DELETE FROM `corpse` WHERE `player` = '" << GetOwnerGUID() << "' AND `bones_flag` = '0'";
    sDatabase.Execute( ss.str().c_str() );

    sDatabase.PExecute( "DELETE FROM `corpse_grid` WHERE `guid` = '%u'",GetGUIDLow());
}

bool Corpse::LoadFromDB(uint32 guid)
{
    QueryResult *result = sDatabase.PQuery("SELECT * FROM `corpse` WHERE `guid` = '%u'",guid);

    if( ! result )
        return false;

    Field *fields = result->Fetch();
    //uint64 guid = fields[0].GetUInt64();
    float positionX = fields[2].GetFloat();
    float positionY = fields[3].GetFloat();
    float positionZ = fields[4].GetFloat();
    float ort       = fields[5].GetFloat();
    //uint32 zoneid   = fields[6].GetUInt32();
    uint32 mapid    = fields[7].GetUInt32();
    uint32 bones   = fields[10].GetUInt32();

    m_type = (bones == 0) ? CORPSE_RESURRECTABLE : CORPSE_BONES;

    if(!LoadValues( fields[8].GetString() ))
    {
        sLog.outError("ERROR: Corpse #%d have broken data in `data` field. Can't be loaded.",guid);
        delete result;
        return false;
    }

    // place
    SetMapId(mapid);
    Relocate(positionX,positionY,positionZ,ort);

    delete result;

    if(!IsPositionValid())
    {
        sLog.outError("ERROR: Corpse (guidlow %d, owner %d) not created. Suggested coordinates isn't valid (X: %d Y: ^%d)",GetGUIDLow(),GUID_LOPART(GetOwnerGUID()),GetPositionX(),GetPositionY());
        return false;
    }

    return true;
}

void Corpse::AddToWorld()
{
    Object::AddToWorld();

    if(GetType() == CORPSE_RESURRECTABLE)
    {
        ObjectAccessor::Instance().AddCorpse(this);

        if(Player* player = ObjectAccessor::Instance().FindPlayer(GetOwnerGUID()))
            UpdateForPlayer(player,true);
    }
}

void Corpse::RemoveFromWorld()
{
    Object::RemoveFromWorld();

    if(GetType() == CORPSE_RESURRECTABLE)
        ObjectAccessor::Instance().RemoveCorpse(this);
}

void Corpse::UpdateForPlayer(Player* player, bool first)
{
    if(player && player->GetGUID() == GetOwnerGUID() && player->GetMapId() == GetMapId())
    {
        bool POI_range = (GetDistance2dSq(player) > CORPSE_RECLAIM_RADIUS*CORPSE_RECLAIM_RADIUS);

        if(first || POI_range && !m_POI)
        {
            std::string corpsename = player->GetName();
            corpsename.append(" corpse.");
            player->PlayerTalkClass->SendPointOfInterest( GetPositionX(), GetPositionY(), 7, 6, 30, corpsename.c_str());
        }

        m_POI = POI_range;
    }
}

void Corpse::ConvertCorpseToBones()
{
    assert(GetType()==CORPSE_RESURRECTABLE);

    Player* player = ObjectAccessor::Instance().FindPlayer(GetOwnerGUID());

    // Removing outdated POI if at same map
    if(player && player->GetMapId() == GetMapId())
        player->PlayerTalkClass->SendPointOfInterest( GetPositionX(), GetPositionY(), 7, 0, 30, "" );

    DEBUG_LOG("Deleting Corpse and swpaning bones.\n");

    // remove corpse from player_guid -> corpse map
    ObjectAccessor::Instance().RemoveCorpse(this);

    // remove corpse from DB
    DeleteFromDB();

    // update data to bone state
    m_type = CORPSE_BONES;

    SetUInt32Value(CORPSE_FIELD_FLAGS, 5);
    SetUInt64Value(CORPSE_FIELD_OWNER, 0);

    for (int i = 0; i < EQUIPMENT_SLOT_END; i++)
    {
        if(GetUInt32Value(CORPSE_FIELD_ITEM + i))
            SetUInt32Value(CORPSE_FIELD_ITEM + i, 0);
    }

    // add bones to DB
    SaveToDB();

    if(player)
    {
        WorldPacket data;
        data.Initialize(SMSG_DESTROY_OBJECT);
        data << (uint64)GetGUID();
        player->GetSession()->SendPacket(&data);
    }
}
