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
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "UpdateMask.h"
#include "MapManager.h"

void WorldSession::HandleMoveWorldportAckOpcode( WorldPacket & recv_data )
{
    sLog.outDebug( "WORLD: got MSG_MOVE_WORLDPORT_ACK." );

    //MapManager::Instance().GetMap(_player->GetMapId())->Remove(_player, false);
    _player->RemoveFromWorld();

    WorldPacket data;
    data.Initialize(SMSG_SET_REST_START);
    data << uint32(8129);
    SendPacket(&data);
    GetPlayer()->SetDontMove(false);

    MapManager::Instance().GetMap(GetPlayer()->GetMapId())->Add(GetPlayer());
}

void WorldSession::HandleFallOpcode( WorldPacket & recv_data )
{
    uint32 flags, time;
    float x, y, z, orientation;
    Player *Target = GetPlayer();

    uint32 FallTime;

    uint64 guid;
    uint32 damage;

    if(Target->GetDontMove())
        return;

    recv_data >> flags >> time;
    recv_data >> x >> y >> z >> orientation;
    recv_data >> FallTime;
    if ( FallTime > 1100 && !Target->isDead())
    {
        uint32 MapID = Target->GetMapId();
        Map* Map = MapManager::Instance().GetMap(MapID);
        float posz = Map->GetWaterLevel(x,y);

        if (z > (posz - (float) 1))
        {
            guid = Target->GetGUID();
            damage = (uint32)((FallTime - 1100)/100)+1;
            Target->EnvironmentalDamage(guid,DAMAGE_FALL, damage);
        }
    }

    //handle fall and logout at the sametime
    if (Target->GetFlag(UNIT_FIELD_FLAGS, 0x40000))
    {
        Target->SetFlag(UNIT_FIELD_BYTES_1,PLAYER_STATE_SIT);
        // Can't move
        WorldPacket data;
        data.Initialize( SMSG_FORCE_MOVE_ROOT );
        data << (uint8)0xFF << Target->GetGUID() << (uint32)2;
        SendPacket( &data );
    }
}

void WorldSession::HandleMovementOpcodes( WorldPacket & recv_data )
{
    uint32 flags, time;
    float x, y, z, orientation;

    if(GetPlayer()->GetDontMove())
        return;

    recv_data >> flags >> time;
    recv_data >> x >> y >> z >> orientation;

    bool isSet = GetPlayer( )->SetPosition(x, y, z, orientation);
    if(recv_data.GetOpcode() != MSG_MOVE_JUMP)
    {
        WorldPacket data;
        data.Initialize( recv_data.GetOpcode() );
        data << uint8(0xFF) << GetPlayer()->GetGUID();
        data << flags << time;
        data << x << y << z << orientation;
        GetPlayer()->SendMessageToSet(&data, false);
    }

    uint32 MapID = GetPlayer()->GetMapId();
    Map* Map = MapManager::Instance().GetMap(MapID);
    float posz = Map->GetWaterLevel(x,y);
    uint8 flag1 = Map->GetTerrainType(x,y);

    //!Underwater check
    if ((z < (posz - (float)2)) && (flag1 & 0x01))
        GetPlayer()->m_isunderwater|= 0x01;
    else if (z > (posz - (float)2))
        GetPlayer()->m_isunderwater&= 0x7A;
    //!in lava check
    if ((z < (posz - (float)0)) && (flag1 & 0x02))
        GetPlayer()->m_isunderwater|= 0x80;
}

void WorldSession::HandleSetActiveMoverOpcode(WorldPacket &recv_data)
{
    //CMSG_SET_ACTIVE_MOVER
    sLog.outDebug("WORLD: Recvd CMSG_SET_ACTIVE_MOVER");

    uint32 guild, time;
    recv_data >> guild >> time;
}

void WorldSession::HandleMountSpecialAnimOpcode(WorldPacket &recvdata)
{
    //sLog.outDebug("WORLD: Recvd CMSG_MOUNTSPECIAL_ANIM");

    WorldPacket data;
    data.Initialize(SMSG_MOUNTSPECIAL_ANIM);
    data << uint64(GetPlayer()->GetGUID());

    GetPlayer()->SendMessageToSet(&data, false);
}
