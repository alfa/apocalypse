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
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "Object.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "WorldSession.h"
#include "UpdateData.h"
#include "Util.h"
#include "MapManager.h"
#include "ObjectAccessor.h"

using namespace std;

Object::Object( )
{
    m_objectTypeId      = TYPEID_OBJECT;
    m_objectType        = TYPE_OBJECT;

    m_positionX         = 0.0f;
    m_positionY         = 0.0f;
    m_positionZ         = 0.0f;
    m_orientation       = 0.0f;

    m_mapId             = 0;

    m_uint32Values      = 0;

    m_inWorld           = false;

    m_minZ              = -500;

    m_valuesCount       = 0;

    m_speed             = 1.0f;
    m_moveType          = MOVE_STOP;

    mSemaphoreTeleport  = false;
    m_inWorld           = false;
    m_objectUpdated     = false;
}

Object::~Object( )
{
    if(m_uint32Values)
        delete [] m_uint32Values;
}

void Object::_Create( uint32 guidlow, uint32 guidhigh )
{
    if(!m_uint32Values) _InitValues();

    SetUInt32Value( OBJECT_FIELD_GUID, guidlow );
    SetUInt32Value( OBJECT_FIELD_GUID+1, guidhigh );
    SetUInt32Value( OBJECT_FIELD_TYPE, m_objectType );

}

void Object::_Create( uint32 guidlow, uint32 guidhigh, uint32 mapid, float x, float y, float z, float ang, uint32 nameId )
{
    if(!m_uint32Values) _InitValues();

    SetUInt32Value( OBJECT_FIELD_GUID, guidlow );
    SetUInt32Value( OBJECT_FIELD_GUID+1, guidhigh );
    SetUInt32Value( OBJECT_FIELD_TYPE, m_objectType );
    SetUInt32Value( OBJECT_FIELD_ENTRY,nameId);
    m_mapId = mapid;
    m_positionX = x;
    m_positionY = y;
    m_positionZ = z;
    m_orientation = ang;
}

void Object::BuildMovementUpdateBlock(UpdateData * data, uint32 flags ) const
{
    ByteBuffer buf(500);

    buf << uint8( UPDATETYPE_MOVEMENT );
    buf << GetGUID();

    _BuildMovementUpdate(&buf, flags, 0x00000000);

    data->AddUpdateBlock(buf);
}

void Object::BuildCreateUpdateBlockForPlayer(UpdateData *data, Player *target) const
{
    if(!target) return;

    ByteBuffer buf(500);
    buf << uint8( UPDATETYPE_CREATE_OBJECT );
    buf << uint8( 0xFF );
    buf << GetGUID() ;
    buf << m_objectTypeId;

    switch(m_objectTypeId)
    {
        case TYPEID_OBJECT:                                 //do nothing
            break;
        case TYPEID_ITEM:
        case TYPEID_CONTAINER:
            _BuildMovementUpdate( &buf, 0x10, 0x0 );
            break;
        case TYPEID_UNIT:
            _BuildMovementUpdate( &buf, 0x70, 0x800000 );
            break;
        case TYPEID_PLAYER:
        {
            if( target == this )                            //build for self
            {
                buf.clear();
                buf << uint8( UPDATETYPE_CREATE_OBJECT2 );
                buf << uint8( 0xFF );
                buf << GetGUID() ;
                buf << m_objectTypeId;
                _BuildMovementUpdate( &buf, 0x71, 0x2000 );
            }
            //build for other player
            else
            {
                _BuildMovementUpdate( &buf, 0x70, 0x0 );
            }
        }break;
        case TYPEID_CORPSE:
        case TYPEID_GAMEOBJECT:
        case TYPEID_DYNAMICOBJECT:
        {
            if(GUID_HIPART(GetGUID())==HIGHGUID_PLAYER_CORPSE)
                _BuildMovementUpdate( &buf, 0x52, 0x0 );
            else
                _BuildMovementUpdate( &buf, 0x50, 0x0 );
        }break;
        //case TYPEID_AIGROUP:
        //case TYPEID_AREATRIGGER:
        //break;
        default:                                            //know type
            sLog.outDetail("Unknow Object Type %d Create Update Block.\n", m_objectTypeId);
            break;
    }

    UpdateMask updateMask;
    updateMask.SetCount( m_valuesCount );
    _SetCreateBits( &updateMask, target );
    _BuildValuesUpdate( &buf, &updateMask );
    data->AddUpdateBlock(buf);

}

void Object::SendUpdateToPlayer(Player* player) const
{
    if (!player->IsInWorld()) return;

    UpdateData upd;
    WorldPacket packet;

    upd.Clear();
    BuildCreateUpdateBlockForPlayer(&upd, player);
    upd.BuildPacket(&packet);
    player->GetSession()->SendPacket(&packet);
}

void Object::BuildValuesUpdateBlockForPlayer(UpdateData *data, Player *target) const
{
    ByteBuffer buf(500);

    buf << (uint8) UPDATETYPE_VALUES;
    buf << (uint8) 0xFF;
    buf << GetGUID();

    UpdateMask updateMask;
    updateMask.SetCount( m_valuesCount );
    _SetUpdateBits( &updateMask, target );
    _BuildValuesUpdate( &buf, &updateMask );

    data->AddUpdateBlock(buf);
}

void Object::BuildOutOfRangeUpdateBlock(UpdateData * data) const
{
    data->AddOutOfRangeGUID(GetGUID());
}

void Object::DestroyForPlayer(Player *target) const
{
    ASSERT(target);

    WorldPacket data;
    data.Initialize( SMSG_DESTROY_OBJECT );
    data << GetGUID();

    target->GetSession()->SendPacket( &data );
}

void Object::_BuildMovementUpdate(ByteBuffer * data, uint8 flags, uint32 flags2 ) const
{
    *data << (uint8)flags;
    if( m_objectTypeId==TYPEID_PLAYER )
    {
        *data << (uint32)flags2;
        *data << (uint32)0xB74D85D1;
        *data << (float)m_positionX;
        *data << (float)m_positionY;
        *data << (float)m_positionZ;
        *data << (float)m_orientation;
        *data << (float)0;
        if(flags2 == 0x2000)                                //update self
        {
            *data << (float)0;
            *data << (float)1.0;
            *data << (float)0;
            *data << (float)0;
        }
        *data << GetSpeed( MOVE_WALK );
        *data << GetSpeed( MOVE_RUN );
        *data << GetSpeed( MOVE_SWIMBACK );
        *data << GetSpeed( MOVE_SWIM );
        *data << GetSpeed( MOVE_WALKBACK );
        *data << GetSpeed( MOVE_TURN );
    }
    if( m_objectTypeId==TYPEID_UNIT )
    {
        *data << (uint32)flags2;
        *data << (uint32)0xB5771D7F;
        *data << (float)m_positionX;
        *data << (float)m_positionY;
        *data << (float)m_positionZ;
        *data << (float)m_orientation;
        *data << (float)0;
        *data << GetSpeed( MOVE_WALK );
        *data << GetSpeed( MOVE_RUN );
        *data << GetSpeed( MOVE_SWIMBACK );
        *data << GetSpeed( MOVE_SWIM );
        *data << GetSpeed( MOVE_WALKBACK );
        *data << GetSpeed( MOVE_TURN );
        uint8 PosCount=0;
        if(flags2 & 0x400000)
        {
            *data << (uint32)0x0;
            *data << (uint32)0x659;
            *data << (uint32)0xB7B;
            *data << (uint32)0xFDA0B4;
            *data << (uint32)PosCount;
            for(int i=0;i<PosCount+1;i++)
            {
                *data << (float)0;                          //x
                *data << (float)0;                          //y
                *data << (float)0;                          //z
            }
        }
    }
    if( (m_objectTypeId==TYPEID_CORPSE) || (m_objectTypeId==TYPEID_GAMEOBJECT) || (m_objectTypeId==TYPEID_DYNAMICOBJECT))
    {
        *data << (float)m_positionX;
        *data << (float)m_positionY;
        *data << (float)m_positionZ;
        *data << (float)m_orientation;
    }

    *data << (uint32)0x6297848C;

    if(  GUID_HIPART(GetGUID()) == HIGHGUID_PLAYER_CORPSE)
        *data << (uint32)0xBD38BA14;                        //fix me
}

void Object::_BuildValuesUpdate(ByteBuffer * data, UpdateMask *updateMask) const
{
    WPAssert(updateMask && updateMask->GetCount() == m_valuesCount);

    *data << (uint8)updateMask->GetBlockCount();
    data->append( updateMask->GetMask(), updateMask->GetLength() );

    for( uint16 index = 0; index < m_valuesCount; index ++ )
    {
        if( updateMask->GetBit( index ) )
            *data << m_uint32Values[ index ];
    }
}

void Object::BuildHeartBeatMsg(WorldPacket *data) const
{
    data->Initialize(MSG_MOVE_HEARTBEAT);

    *data << GetGUID();

    *data << uint32(0);
    *data << uint32(0);

    *data << m_positionX;
    *data << m_positionY;
    *data << m_positionZ;

    *data << m_orientation;
}

void Object::BuildTeleportAckMsg(WorldPacket *data, float x, float y, float z, float ang) const
{
    data->Initialize(MSG_MOVE_TELEPORT_ACK);
    *data << uint8(0xFF);
    *data << GetGUID();
    *data << uint32(0x800000);
    *data << uint16(0x67EE);
    *data << uint16(0xD1EB);
    *data<< x;
    *data<< y;
    *data<< z;
    *data<< ang;
    *data<< uint32(0x0);
}

void Object::SendMessageToSet(WorldPacket *data, bool bToSelf)
{
    MapManager::Instance().GetMap(m_mapId)->MessageBoardcast(this, data);
}

void Object::LoadValues(const char* data)
{
    if(!m_uint32Values) _InitValues();

    vector<string> tokens = StrSplit(data, " ");

    vector<string>::iterator iter;
    int index;

    for (iter = tokens.begin(), index = 0;
        index < m_valuesCount && iter != tokens.end(); ++iter, ++index)
    {
        m_uint32Values[index] = atol((*iter).c_str());
    }
}

void Object::LoadTaxiMask(const char* data)
{
    vector<string> tokens = StrSplit(data, " ");

    int index;
    vector<string>::iterator iter;

    for (iter = tokens.begin(), index = 0;
        (index < 8) && (iter != tokens.end()); ++iter, ++index)
    {
        m_taximask[index] = atol((*iter).c_str());
    }
}

void Object::_SetUpdateBits(UpdateMask *updateMask, Player *target) const
{
    *updateMask = m_updateMask;
}

void Object::_SetCreateBits(UpdateMask *updateMask, Player *target) const
{
    for( uint16 index = 0; index < m_valuesCount; index++ )
    {
        if(GetUInt32Value(index) != 0)
            updateMask->SetBit(index);
    }
}

void Object::SetUInt32Value( const uint16 &index, const uint32 &value )
{
    ASSERT( index < m_valuesCount );
    m_uint32Values[ index ] = value;

    if(m_inWorld)
    {
        m_updateMask.SetBit( index );

        if(!m_objectUpdated)
        {
            ObjectAccessor::Instance().AddUpdateObject(this);
            m_objectUpdated = true;
        }
    }
}

void Object::SetUInt64Value( const uint16 &index, const uint64 &value )
{
    ASSERT( index + 1 < m_valuesCount );
    m_uint32Values[ index ] = *((uint32*)&value);
    m_uint32Values[ index + 1 ] = *(((uint32*)&value) + 1);

    if(m_inWorld)
    {
        m_updateMask.SetBit( index );
        m_updateMask.SetBit( index + 1 );

        if(!m_objectUpdated)
        {
            ObjectAccessor::Instance().AddUpdateObject(this);
            m_objectUpdated = true;
        }
    }
}

void Object::SetFloatValue( const uint16 &index, const float &value )
{
    ASSERT( index < m_valuesCount );
    m_floatValues[ index ] = value;

    if(m_inWorld)
    {
        m_updateMask.SetBit( index );

        if(!m_objectUpdated)
        {
            ObjectAccessor::Instance().AddUpdateObject(this);
            m_objectUpdated = true;
        }
    }
}

void Object::SetFlag( const uint16 &index, uint32 newFlag )
{
    ASSERT( index < m_valuesCount );
    m_uint32Values[ index ] |= newFlag;

    if(m_inWorld)
    {
        m_updateMask.SetBit( index );

        if(!m_objectUpdated)
        {
            ObjectAccessor::Instance().AddUpdateObject(this);
            m_objectUpdated = true;
        }
    }
}

void Object::RemoveFlag( const uint16 &index, uint32 oldFlag )
{
    ASSERT( index < m_valuesCount );
    m_uint32Values[ index ] &= ~oldFlag;

    if(m_inWorld)
    {
        m_updateMask.SetBit( index );

        if(!m_objectUpdated)
        {
            ObjectAccessor::Instance().AddUpdateObject(this);
            m_objectUpdated = true;
        }
    }
}

uint32 Object::GetZoneId( )
{
    return sAreaStore.LookupEntry(MapManager::Instance().GetMap(m_mapId)->GetAreaFlag(m_positionX,m_positionY))->zone;
}

float Object::GetDistanceSq(const Object* obj) const
{
    ASSERT(obj->GetMapId() == m_mapId);
    float size = GetObjectSize();
    float size1 = obj->GetObjectSize();
    float x1, y1, z1, x2, y2, z2;
    GetClosePoint( obj, x1, y1, z1 );
    obj->GetClosePoint( this, x2, y2, z2 );
    float dx  = x2 - x1;
    float dy  = y2 - y1;
    float dz  = z2 - z1;
    return (dx*dx) + (dy*dy) + (dz*dz) + size * size + size1 * size1;
}

float Object::GetDistanceSq(const float x, const float y, const float z) const
{
    float size = GetObjectSize();
    float x1, y1, z1;
    GetClosePoint( x, y, z, x1, y1, z1 );

    float dx  = x - x1;
    float dy  = y - y1;
    float dz  = z - z1;
    return (dx*dx) + (dy*dy) + (dz*dz) + size * size * 2;
}

float Object::GetDistance2dSq(const Object* obj) const
{
    ASSERT(obj->GetMapId() == m_mapId);
    float dx  = obj->GetPositionX() - GetPositionX();
    float dy  = obj->GetPositionY() - GetPositionY();
    return (dx*dx) + (dy*dy);
}

float Object::GetAngle(const Object* obj) const
{
    if(!obj) return 0;

    float dx = obj->GetPositionX() - GetPositionX();
    float dy = obj->GetPositionY() - GetPositionY();

    float ang = (float)atan2((double)dy, (double)dx);
    ang = (ang >= 0) ? ang : 2 * M_PI + ang;
    return ang;
}

float Object::GetAngle( const float x, const float y ) const
{
    if( x==0 && y==0) return 0;

    float dx = x - GetPositionX();
    float dy = y - GetPositionY();

    float ang = (float)atan2((double)dy, (double)dx);
    ang = (ang >= 0) ? ang : 2 * M_PI + ang;
    return ang;
}

bool Object::IsInArc(const float arcangle, const Object* obj) const
{
    float arc=arcangle;
    if(arcangle>2.0f * M_PI)
        arc=arcangle - 2.0f * M_PI;
    float angle = GetAngle( obj );
    float lborder = GetOrientation() - (arc/2.0f);
    float rborder = GetOrientation() + (arc/2.0f);
    if(lborder<0)
        return ((angle >= 2.0f * M_PI+lborder && angle <= 2.0f * M_PI) || (angle>=0 && angle<=rborder));
    return ( angle >= lborder ) && ( angle <= rborder );
}

void Object::GetClosePoint( const Object* victim, float &x, float &y, float &z ) const
{
    float angle, z1;
    if(!victim)
    {
        z1 = GetPositionZ();
        angle = m_orientation;
    }
    else
    {
        z1 = victim->GetPositionZ();
        angle = GetAngle( victim );
    }
    x = m_positionX + GetObjectSize() * cos(angle);
    y = m_positionY + GetObjectSize() * sin(angle);
    int mapid = GetMapId();
    z = MapManager::Instance ().GetMap(mapid)->GetHeight(x,y);
    if( abs( z - z1 ) > 15.0f )
        z = z1;
}

void Object::GetClosePoint( const float ox, const float oy, const float oz, float &x, float &y, float &z ) const
{
    float angle;
    if(ox==0 && oy ==0)
        angle = m_orientation;
    else
        angle = GetAngle( ox, oy );
    x = m_positionX + GetObjectSize() * cos(angle);
    y = m_positionY + GetObjectSize() * sin(angle);
    int mapid = GetMapId();
    z = MapManager::Instance ().GetMap(mapid)->GetHeight(x,y);
    if( abs( z - oz ) > 15.0f )
        z = oz;
}
