/* 
 * Copyright (C) 2005 MaNGOS <http://www.magosproject.org/>
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
#include "GameObject.h"
#include "ObjectMgr.h"
#include "UpdateMask.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "Database/DatabaseEnv.h"
#include "MapManager.h"


GameObject::GameObject() : Object()
{
    m_objectType |= TYPE_GAMEOBJECT;
    m_objectTypeId = TYPEID_GAMEOBJECT;

    m_valuesCount = GAMEOBJECT_END;
    m_RespawnTimer = 0;
	lootid=0;
}

void GameObject::Create(uint32 guidlow, uint32 name_id, uint32 mapid, float x, float y, float z, float ang, float rotation0, float rotation1, float rotation2, float rotation3)
{
	GameObjectInfo*  goinfo = objmgr.GetGameObjectInfo(name_id);
    if(!goinfo)return;
    Object::_Create(guidlow, HIGHGUID_GAMEOBJECT);

	//
	m_positionX = x;
	m_positionY = y;
	m_positionZ = z;
	m_orientation = ang;

	m_mapId = mapid;
	



    SetUInt32Value(GAMEOBJECT_TIMESTAMP, (uint32)time(NULL));
    SetFloatValue(GAMEOBJECT_POS_X, x);
    SetFloatValue(GAMEOBJECT_POS_Y, y);
    SetFloatValue(GAMEOBJECT_POS_Z, z);
	SetFloatValue(GAMEOBJECT_FACING, ang); //this is not facing angle
    SetFloatValue(OBJECT_FIELD_SCALE_X, goinfo->size);
    
    SetUInt32Value(GAMEOBJECT_FACTION, goinfo->faction);
    SetUInt32Value(GAMEOBJECT_FLAGS, goinfo->flags);

	SetUInt32Value (OBJECT_FIELD_ENTRY, goinfo->id);
	
	
	
	SetUInt32Value (GAMEOBJECT_DISPLAYID, goinfo->displayId);

    
	SetUInt32Value (GAMEOBJECT_STATE, 1);
	SetUInt32Value (GAMEOBJECT_TYPE_ID, goinfo->type);
    
    
	SetFloatValue (GAMEOBJECT_ROTATION, rotation0);
	SetFloatValue (GAMEOBJECT_ROTATION+1, rotation1);
	SetFloatValue (GAMEOBJECT_ROTATION+2, rotation2);
	SetFloatValue (GAMEOBJECT_ROTATION+3, rotation3);

}


void GameObject::Update(uint32 p_time)
{
  
    if(m_RespawnTimer > 0)
    {
        if(m_RespawnTimer > p_time)
            m_RespawnTimer -= p_time;
        else
        {
			WorldPacket data;
            data.Initialize(SMSG_GAMEOBJECT_SPAWN_ANIM);
            data << GetGUID();
            SendMessageToSet(&data,true);
            SetUInt32Value(GAMEOBJECT_STATE,1);
            m_RespawnTimer = 0;
        }
    }

	
	
}


void GameObject::generateLoot()
{
	if(lootid)
		FillLoot(&loot,lootid);
}



void GameObject::SaveToDB()
{
    std::stringstream ss;
    sDatabase.PExecute("DELETE FROM gameobjects WHERE guid = '%u'", GetGUIDLow());

	
	const GameObjectInfo *goI = GetGOInfo();
	
	if (!goI)
		return;

    ss << "INSERT INTO gameobjects VALUES ( "
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
		<< m_RespawnTimer <<")";



    sDatabase.Execute( ss.str( ).c_str( ) );
}


void GameObject::LoadFromDB(uint32 guid)
{
    float rotation0, rotation1, rotation2, rotation3;

    QueryResult *result = sDatabase.PQuery("SELECT * FROM gameobjects WHERE guid = '%u';", guid);

    if( ! result )
    return;

    Field *fields = result->Fetch();
	uint32 entry = fields[1].GetUInt32();
	uint32 map_id=fields[2].GetUInt32();
	float x = fields[3].GetFloat();
    float y = fields[4].GetFloat();
    float z = fields[5].GetFloat();
    float ang = fields[6].GetFloat();
   
    rotation0 = fields[7].GetFloat();
    rotation1 = fields[8].GetFloat();
    rotation2 = fields[9].GetFloat();
    rotation3 = fields[10].GetFloat();
	lootid=fields[11].GetUInt32();
	m_RespawnTimer=fields[11].GetUInt32();
    Create(guid,entry, map_id, x, y, z, ang, rotation0, rotation1, rotation2, rotation3);  
	delete result;
}


void GameObject::DeleteFromDB()
{
    sDatabase.PExecute("DELETE FROM gameobjects WHERE guid = '%u'", GetGUIDLow());
}

GameObjectInfo *GameObject::GetGOInfo()
{ 
	return objmgr.GetGameObjectInfo(GetUInt32Value (OBJECT_FIELD_ENTRY));
}  
