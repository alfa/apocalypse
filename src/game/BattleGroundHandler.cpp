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
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "Player.h"
#include "World.h"
#include "ObjectMgr.h"
#include "WorldSession.h"
#include "Auth/BigNumber.h"
#include "Auth/Sha1.h"
#include "UpdateData.h"
#include "LootMgr.h"
#include "Chat.h"
#include "ScriptCalls.h"
#include <zlib/zlib.h>
#include "MapManager.h"
#include "ObjectAccessor.h"
#include "Object.h"
#include "BattleGroundMgr.h"

void WorldSession::HandleBattleGroundHelloOpcode( WorldPacket & recv_data )
{
	uint64 guid;
	recv_data >> guid;
	sLog.outDebug( "WORLD: Recvd CMSG_BATTLEMASTER_HELLO Message from: %d" , guid);

	
	// For now we'll assume all battlefield npcs are mapid 489
	// gossip related
    uint32 MapId = 489;
    uint32 PlayerLevel = 1;

    PlayerLevel = GetPlayer()->GetUInt32Value(UNIT_FIELD_LEVEL);

    // TODO: Lookup npc entry code and find mapid

    WorldPacket data;
    data.Initialize(SMSG_BATTLEFIELD_LIST);
    data << uint64(guid);
    data << uint32(MapId);

    std::list<uint32> SendList;

    for(std::map<uint32, BattleGround*>::iterator itr = sBattleGroundMgr.GetBattleGroundsBegin();itr!= sBattleGroundMgr.GetBattleGroundsEnd();++itr)
	{
        if(itr->second->GetMapId() == MapId && (PlayerLevel >= itr->second->GetMinLevel()) && (PlayerLevel <= itr->second->GetMaxLevel()))
            SendList.push_back(itr->second->GetID());
	}

    uint32 size = SendList.size();

    data << uint32(size << 8);

    uint32 count = 1;

    for(std::list<uint32>::iterator i = SendList.begin();i!=SendList.end();++i)
    {
        data << uint32(count << 8);
        count++;
    }
    SendList.clear();
	data << uint8(0x00);
	GetPlayer( )->GetSession( )->SendPacket( &data );
}


void WorldSession::HandleBattleGroundJoinOpcode( WorldPacket & recv_data )
{
	uint64 guid;
	recv_data >> guid;// >> MapID >> Instance;
	sLog.outDebug( "WORLD: Recvd CMSG_BATTLEMASTER_JOIN Message from: %d" , guid);

	// We're in BG.
    GetPlayer()->m_bgBattleGroundID = 1;
	GetPlayer()->m_bgInBattleGround = true;

    // Calculate team
    GetPlayer()->m_bgTeam = sBattleGroundMgr.GenerateTeamByRace(GetPlayer()->getRace());
   
	//sBattleGroundMgr.BuildBattleGroundStatusPacket(GetPlayer(), sBattleGroundMgr.GetBattleGround(1)->GetMapId(), sBattleGroundMgr.GetBattleGround(1)->GetID(),3);

	GetPlayer()->m_bgEntryPointMap = GetPlayer()->GetMapId();
	GetPlayer()->m_bgEntryPointO = GetPlayer()->GetOrientation();
    GetPlayer()->m_bgEntryPointX = GetPlayer()->GetPositionX();
	GetPlayer()->m_bgEntryPointY = GetPlayer()->GetPositionY();
    GetPlayer()->m_bgEntryPointZ = GetPlayer()->GetPositionZ();


		// TODO: Find team based on faction
    uint32 TeamID = 0;

	sBattleGroundMgr.SendToBattleGround(GetPlayer(),TeamID,1);
	
    //sBattleGroundMgr.BuildBattleGroundStatusPacket(GetPlayer(), sBattleGroundMgr.GetBattleGround(1)->GetMapId(), sBattleGroundMgr.GetBattleGround(1)->GetID(), 0x03, 0x001D2A00);

	sBattleGroundMgr.BuildBattleGroundStatusPacket(GetPlayer(), sBattleGroundMgr.GetBattleGround(1)->GetMapId(), sBattleGroundMgr.GetBattleGround(1)->GetID(), 0x03, 0x00);

    // Adding Player to BattleGround id = 0
	sBattleGroundMgr.AddPlayerToBattleGround(GetPlayer(),1);
    
   // Bur: Not sure if we would have to send a position/score update.. maybe the client requests this automatically we'll have to see
}

void WorldSession::HandleBattleGroundPlayerPositionsOpcode( WorldPacket &recv_data )
{
	sLog.outDebug( "WORLD: Recvd MSG_BATTLEGROUND_PLAYER_POSITIONS Message");

	std::list<Player*> ListToSend;
    for(std::list<Player*>::iterator i=sBattleGroundMgr.GetBattleGround(GetPlayer()->m_bgBattleGroundID)->GetPlayersBegin();i!=sBattleGroundMgr.GetBattleGround(GetPlayer()->m_bgBattleGroundID)->GetPlayersEnd();++i)
    {
        if((*i) != GetPlayer())
            ListToSend.push_back(*i);
    }

    WorldPacket data;
    data.Initialize(MSG_BATTLEGROUND_PLAYER_POSITIONS);         // MSG_BATTLEGROUND_PLAYER_POSITIONS
    data << uint32(ListToSend.size());
	data << uint32(sBattleGroundMgr.GetBattleGround(GetPlayer()->m_bgBattleGroundID)->GetPlayersSize());
	for(std::list<Player*>::iterator itr=ListToSend.begin();itr!=ListToSend.end();++itr)
    {
        data << (uint64)(*itr)->GetGUID();
        data << (float)(*itr)->GetPositionX();
        data << (float)(*itr)->GetPositionY();
    }
    GetPlayer()->GetSession()->SendPacket(&data);
	sLog.outDebug( "WORLD: Send MSG_BATTLEGROUND_PLAYER_POSITIONS Message");
}

void WorldSession::HandleBattleGroundPVPlogdataOpcode( WorldPacket &recv_data )
{
	sLog.outDebug( "WORLD: Recvd MSG_PVP_LOG_DATA Message");
	WorldPacket data;
	data.Initialize(MSG_PVP_LOG_DATA); // MSG_PVP_LOG_DATA
    data << uint8(0x0);
    data << uint32(sBattleGroundMgr.GetBattleGround(GetPlayer()->m_bgBattleGroundID)->GetPlayerScoresSize());

    for(std::map<uint64, BattleGroundScore>::iterator itr=sBattleGroundMgr.GetBattleGround(GetPlayer()->m_bgBattleGroundID)->GetPlayerScoresBegin();itr!=sBattleGroundMgr.GetBattleGround(GetPlayer()->m_bgBattleGroundID)->GetPlayerScoresEnd();++itr)
    {
        data << (uint64)itr->first;
        data << (uint32)itr->second.Unk; //Rank
        data << (uint32)itr->second.KillingBlows;
		data << (uint32)itr->second.Deaths;
        data << (uint32)itr->second.HonorableKills;
        data << (uint32)itr->second.DishonorableKills;
        data << (uint32)itr->second.BonusHonor;
		data << (uint32)0;
		data << (uint32)0;
        /*data << itr->second.Rank;
        data << itr->second.Unk1;
        data << itr->second.Unk2;
        data << itr->second.Unk3;
        data << itr->second.Unk4;*/
    }
    GetPlayer()->GetSession()->SendPacket(&data);


	sLog.outDebug( "WORLD: Send MSG_PVP_LOG_DATA Message players:%d", sBattleGroundMgr.GetBattleGround(GetPlayer()->m_bgBattleGroundID)->GetPlayerScoresSize());

	//data << (uint8)0; ////Warsong Gulch
	/*data << (uint8)1; //
	data << (uint8)1; //

	
	//strangest thing 2 different 
	data << (uint32)NumberofPlayers;
	for (uint8 i = 0; i < NumberofPlayers; i++)
	{
		data << (uint64)8;//GUID     //8
		data << (uint32)0;//rank
		data << (uint32)0;//killing blows
		data << (uint32)0;//honorable kills
		data << (uint32)0;//deaths
		data << (uint32)0;//Bonus Honor
		data << (uint32)0;//I think Instance
		data << (uint32)0;
		data << (uint32)0;			//8*4 = 32+8=40
		
		
		//1 player is 40 bytes
	}
	data.hexlike();
	SendPacket(&data);*/
}

void WorldSession::HandleBattleGroundListOpcode( WorldPacket &recv_data )
{
	sLog.outDebug( "WORLD: Recvd CMSG_BATTLEFIELD_LIST Message");

	uint32 MapID;
	uint32 NumberOfInstances;
	WorldPacket data;
	
    recv_data.hexlike();

	recv_data >> MapID;

	//CMSG_BATTLEFIELD_LIST (0x023C);
	
	//MapID = 489;
	
	
	NumberOfInstances = 8;
	data.Initialize(SMSG_BATTLEFIELD_LIST);
	data << uint64(GetPlayer()->GetGUID());
	data << uint32(MapID);
	data << uint32(NumberOfInstances << 8);
	for (uint8 i = 0; i < NumberOfInstances; i++)
	{
		data << uint32(i << 8);
	}
	data << uint8(0x00);
	
	SendPacket(&data);
	
}


void WorldSession::HandleBattleGroundPlayerPortOpcode( WorldPacket &recv_data )
{
	sLog.outDebug( "WORLD: Recvd CMSG_BATTLEFIELD_PORT Message");
	//CMSG_BATTLEFIELD_PORT

	/*uint64 guid = 0;
	uint32 MapID = 0;
    uint32 Instance = 0;
	uint8 Enter = 0;

	WorldPacket data;
	recv_data.hexlike();
	
	//if que = 0 player logs in
	if (GetPlayer()->GetInstanceQue() == 0)
	{
		//recv_data >> guid >> MapID >> Instance;

		recv_data >> (uint32) MapID >> (uint8)Enter;

		if (Enter == 1)
		{
			//Enter the battleground
			//we can do some type of system. BattleGround GetPlayer(guid)->Getcurrentinstance..... enzzz

			Instance = GetPlayer()->GetCurrentInstance();
			//MapID = GetPlayer()->GetInstanceMapId();
		
			sLog.outDebug( "BATTLEGROUND: Sending Player:%d to Map:%d Instance %d",(uint32)guid, MapID, Instance);

			//data << uint32(0x000001E9) << float(1519.530273f) << float(1481.868408f) << float(352.023743f) << float(3.141593f);
			//Map* Map = MapManager::Instance().GetMap(GetPlayer()->GetInstanceMapId());
			Map* Map = MapManager::Instance().GetMap(MapID);
			float posz = Map->GetHeight(1021.24,1418.05);
			GetPlayer()->smsg_NewWorld(MapID, 1021.24, 1418.05,posz+0.5,0.0);

			SendBattleFieldStatus(GetPlayer(),MapID, Instance, 3, 0);
			//GetPlayer()->SendInitWorldStates(MapID);
			



		}
		else
		{

			//remove the battleground sign
			SendBattleFieldStatus(GetPlayer(),MapID, Instance, 0, 0);
		}

	}

	//if player que = in progress and this function is called again its becouse of it quits que
	//MapID and instance maybe not needed for removing
	if (GetPlayer()->GetInstanceQue() > 0)
	{
		sLog.outDebug( "BATTLEGROUND: Player is quiting que");

		SendBattleFieldStatus(GetPlayer(),GetPlayer()->GetInstanceMapId(), GetPlayer()->GetCurrentInstance(), 0, 0);
		
		GetPlayer()->SetInstanceQue(0);
		GetPlayer()->SetInstanceMapId(0);
		GetPlayer()->SetInstanceType(0);
	}
		

	//HORDE Coordinates
	//x:1021.24
	//y:1418.05
	//z:341.56
	
	//uint32 MapID = GetPlayer()->GetMapId();
 */   
}



void WorldSession::HandleBattleGroundLeaveOpcode( WorldPacket &recv_data )
{
	sLog.outDebug( "WORLD: Recvd CMSG_LEAVE_BATTLEFIELD Message"); //0x2E1
	uint32 MapID;
	WorldPacket data;

	recv_data >> MapID;

	/* Send Back to old xyz
	Map* Map = MapManager::Instance().GetMap(GetPlayer()->GetInstanceMapId());
	float posz = Map->GetHeight(1021.24,1418.05);
	GetPlayer()->smsg_NewWorld(MapID, 1021.24, 1418.05,posz+0.5,0.0);
	*/
	


}