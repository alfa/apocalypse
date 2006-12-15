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
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "UpdateMask.h"
#include "NPCHandler.h"
#include "ObjectAccessor.h"
#include "Pet.h"

void WorldSession::HandleNameQueryOpcode( WorldPacket & recv_data )
{
    uint64 guid;

    recv_data >> guid;

    sLog.outDebug( "Received CMSG_NAME_QUERY for: " I64FMTD, guid);

    Player *pChar = ObjectAccessor::Instance().FindPlayer(guid);

    WorldPacket data;
    data.Initialize( SMSG_NAME_QUERY_RESPONSE );

    if (pChar == NULL)
    {
        std::string name = "<Non-existed now character>";

        if (!objmgr.GetPlayerNameByGUID(guid, name))
            sLog.outError( "No player name found for this guid: %u", guid );

        data << guid;
        data << name;
        data << uint8(0);
        data << uint32(0) << uint32(0) << uint32(0);

    }
    else
    {
        data << guid;
        data << pChar->GetName();
        data << uint8(0);
        data << uint32(pChar->getRace());
        data << uint32(pChar->getGender());
        data << uint32(pChar->getClass());
    }

    SendPacket( &data );
}

void WorldSession::HandleQueryTimeOpcode( WorldPacket & recv_data )
{
    WorldPacket data;
    data.Initialize( SMSG_QUERY_TIME_RESPONSE );

    data << (uint32)time(NULL);
    //data << (uint32)getMSTime();
    SendPacket( &data );
}

void WorldSession::HandleCreatureQueryOpcode( WorldPacket & recv_data )
{
    WorldPacket data;
    uint32 entry;
    uint64 guid;

    recv_data >> entry;
    recv_data >> guid;

    Creature *unit = ObjectAccessor::Instance().GetCreature(*_player, guid);

    if (unit == NULL)
    {
        sLog.outDebug( "WORLD: HandleCreatureQueryOpcode - (%u) NO SUCH UNIT! (GUID: %u, ENTRY: %u)", uint32(GUID_LOPART(guid)), guid, entry );
        /*    data.Initialize( SMSG_CREATURE_QUERY_RESPONSE );
            data << (uint32)0;
            data << (uint32)0;
            data << (uint32)0;
            data << (uint32)0;
            data << (uint32)0;
            data << (uint32)0;
            data << (uint32)0;
            data << (uint32)0;
            data << (uint32)0;
            data << (uint32)0;
            data << (uint32)0;
            data << (uint16)0;
            SendPacket( &data );
            return;*/
    }

    //CreatureInfo const *ci = unit->GetCreatureInfo();
    CreatureInfo const *ci = objmgr.GetCreatureTemplate(entry);
    if (!ci)
    {
        sLog.outDebug( "WORLD: HandleCreatureQueryOpcode - (%u) NO CREATUREINFO! (GUID: %u, ENTRY: %u)", uint32(GUID_LOPART(guid)), guid, entry );
        data.Initialize( SMSG_CREATURE_QUERY_RESPONSE );
        data << (uint32)0;
        data << (uint32)0;
        data << (uint32)0;
        data << (uint32)0;
        data << (uint32)0;
        data << (uint32)0;
        data << (uint32)0;
        data << (uint32)0;
        data << (uint32)0;
        data << (uint32)0;
        data << (uint32)0;
        data << (uint16)0;
        SendPacket( &data );
        return;
    }

    sLog.outDetail("WORLD: CMSG_CREATURE_QUERY '%s' - Entry: %u - GUID: %u.", ci->Name, entry, guid);
    data.Initialize( SMSG_CREATURE_QUERY_RESPONSE );
    data << (uint32)entry;
    data << (unit ? unit->GetName() : ci->Name);

    data << uint8(0) << uint8(0) << uint8(0);
    //if (unit)
    //    data << ((unit->isPet()) ? "Pet" : ci->SubName);
    //else
    data << ci->SubName;

    uint32 wdbFeild11=0,wdbFeild12=0;

    data << ci->flag1;                                      //flag1          wdbFeild7=wad flags1
    if (unit)
        data << (uint32)((unit->isPet()) ? 0 : ci->type);   //creatureType   wdbFeild8
    else
        data << (uint32)ci->type;

    data << (uint32)ci->family;                             //family         wdbFeild9
    data << (uint32)ci->rank;                               //rank           wdbFeild10
    data << (uint32)wdbFeild11;                             //unknow         wdbFeild11
    data << (uint32)wdbFeild12;                             //unknow         wdbFeild12
    if (unit)
        data << unit->GetUInt32Value(UNIT_FIELD_DISPLAYID); //DisplayID      wdbFeild13
    else
        data << (uint32)ci->randomDisplayID();

    data << (uint16)ci->civilian;                           //wdbFeild14

    SendPacket( &data );

}

void WorldSession::SendCreatureQuery( uint32 entry, uint64 guid )
{
    WorldPacket data;

    Creature *unit = ObjectAccessor::Instance().GetCreature(*_player, guid);

    if (unit == NULL)
    {
        sLog.outDebug( "WORLD: SendCreatureQuery - (%u) NO SUCH UNIT! (GUID: %u, ENTRY: %u)", uint32(GUID_LOPART(guid)), guid, entry );
        //    return;
    }

    //CreatureInfo const *ci = unit->GetCreatureInfo();

    CreatureInfo const *ci = objmgr.GetCreatureTemplate(entry);

    if (!ci)
    {
        sLog.outDebug( "WORLD: SendCreatureQuery - (%u) NO CREATUREINFO! (GUID: %u, ENTRY: %u)", uint32(GUID_LOPART(guid)), guid, entry );
        return;
    }

    sLog.outDetail("WORLD: CMSG_CREATURE_QUERY '%s' - Entry: %u - GUID: %u.", ci->Name, entry, guid);

    data.Initialize( SMSG_CREATURE_QUERY_RESPONSE );
    data << (uint32)entry;
    data << (unit ? unit->GetName() : ci->Name);

    data << uint8(0) << uint8(0) << uint8(0);

    if (unit)
        data << ((unit->isPet()) ? "Pet" : ci->SubName);
    else
        data << ci->Name;

    uint32 wdbFeild11=0,wdbFeild12=0;

    data << ci->flag1;                                      //flags          wdbFeild7=wad flags1

    if (unit)
        data << (uint32)((unit->isPet()) ? 0 : ci->type);   //creatureType   wdbFeild8
    else
        data << (uint32) ci->type;

    data << (uint32)ci->family;                             //family         wdbFeild9
    data << (uint32)ci->rank;                               //rank           wdbFeild10
    data << (uint32)wdbFeild11;                             //unknow         wdbFeild11
    data << (uint32)wdbFeild12;                             //unknow         wdbFeild12
    if (unit)
        data << unit->GetUInt32Value(UNIT_FIELD_DISPLAYID); //DisplayID      wdbFeild13
    else
        data << (uint32)ci->randomDisplayID();

    data << (uint16)ci->civilian;                           //wdbFeild14

    SendPacket( &data );
    /*
        uint32 npcflags = unit->GetUInt32Value(UNIT_NPC_FLAGS);

        data.Initialize( SMSG_CREATURE_QUERY_RESPONSE );
        data << (uint32)entry;
        data << ci->Name.c_str();
        data << uint8(0) << uint8(0) << uint8(0);
        data << ci->SubName.c_str();

        data << (uint32)npcflags;

        if ((ci->Type & 2) > 0)
        {
            data << uint32(7);
        }
        else
        {
            data << uint32(0);
        }

        data << ci->Type;

        if (ci->level >= 16 && ci->level < 32)
            data << (uint32)CREATURE_ELITE_ELITE;
        else if (ci->level >= 32 && ci->level < 48)
            data << (uint32)CREATURE_ELITE_RAREELITE;
        else if (ci->level >= 48 && ci->level < 59)
            data << (uint32)CREATURE_ELITE_WORLDBOSS;
        else if (ci->level >= 60)
            data << (uint32)CREATURE_ELITE_RARE;
        else
            data << (uint32)CREATURE_ELITE_NORMAL;

        data << (uint32)ci->family;

        data << (uint32)0;
        data << ci->DisplayID;

        data << (uint32)0;
        data << (uint32)0;
        data << (uint32)0;
        data << (uint32)0;

        SendPacket( &data );
    */
}

void WorldSession::SendTestCreatureQueryOpcode( uint32 entry, uint64 guid, uint32 testvalue )
{
    WorldPacket data;

    Creature *unit = ObjectAccessor::Instance().GetCreature(*_player, guid);

    if (unit == NULL)
    {
        sLog.outDebug( "WORLD: SendTestCreatureQueryOpcode - (%u) NO SUCH UNIT! (GUID: %u, ENTRY: %u)", uint32(GUID_LOPART(guid)), guid, entry );
        //return;
    }

    //CreatureInfo const *ci = unit->GetCreatureInfo();
    CreatureInfo const *ci = objmgr.GetCreatureTemplate(entry);
    if (!ci)
    {
        sLog.outDebug( "WORLD: SendTestCreatureQueryOpcode - (%u) NO CREATUREINFO! (GUID: %u, ENTRY: %u)", uint32(GUID_LOPART(guid)), guid, entry );
        return;
    }

    sLog.outDetail("WORLD: CMSG_CREATURE_QUERY '%s' - Entry: %u - GUID: %u.", ci->Name, entry, guid);

    uint8 u8unk1=0,u8unk2=0,u8unk3=0;
    //------------------------------------------------------------------------
    data.Initialize( SMSG_CREATURE_QUERY_RESPONSE );
    data << (uint32)entry;
    data << ci->Name;
    data << uint8(u8unk1) << uint8(u8unk2) << uint8(u8unk3);
    data << ci->SubName;

    uint32 wdbFeild11=0,wdbFeild12=0;

    data << ci->flag1;                                      //flags          wdbFeild7=wad flags1
    data << uint32(ci->type);                               //creatureType   wdbFeild8
    data << (uint32)ci->family;                             //family         wdbFeild9
    data << (uint32)ci->rank;                               //unknow         wdbFeild10
    data << (uint32)wdbFeild11;                             //unknow         wdbFeild11
    data << (uint32)wdbFeild12;                             //unknow         wdbFeild12
    if (unit)
        data << unit->GetUInt32Value(UNIT_FIELD_DISPLAYID); //DisplayID      wdbFeild13
    else
        data << (uint32)ci->randomDisplayID();

    data << (uint16)ci->civilian;                           //wdbFeild14

    SendPacket( &data );
    //-----------------------------------------------------------------------
    /*
        data.Initialize( SMSG_CREATURE_QUERY_RESPONSE );
        data << (uint32)entry;
        data << ci->Name.c_str();
        data << uint8(0) << uint8(0) << uint8(0);
        data << ci->SubName.c_str();

        data << (uint32)npcflags;

        if ((ci->Type & 2) > 0)
        {
            data << uint32(7);
        }
        else
        {
            data << uint32(0);
        }

        data << ci->Type;

        if (ci->level >= 16 && ci->level < 32)
            data << (uint32)CREATURE_ELITE_ELITE;
        else if (ci->level >= 32 && ci->level < 48)
            data << (uint32)CREATURE_ELITE_RAREELITE;
        else if (ci->level >= 48 && ci->level < 59)
            data << (uint32)CREATURE_ELITE_WORLDBOSS;
        else if (ci->level >= 60)
            data << (uint32)CREATURE_ELITE_RARE;
        else
            data << (uint32)CREATURE_ELITE_NORMAL;

        data << (uint32)ci->family;

        data << (uint32)0;
        data << ci->DisplayID;

        data << (uint32)0;
        data << (uint32)0;
        data << (uint32)0;
        data << (uint32)0;

        SendPacket( &data );
    */
}

void WorldSession::HandleGameObjectQueryOpcode( WorldPacket & recv_data )
{

    WorldPacket data;
    uint32 entryID;
    uint64 guid;

    recv_data >> entryID;
    recv_data >> guid;

    sLog.outDetail("WORLD: CMSG_GAMEOBJECT_QUERY '%u'", guid);

    const GameObjectInfo *info = objmgr.GetGameObjectInfo(entryID);
    data.Initialize( SMSG_GAMEOBJECT_QUERY_RESPONSE );
    data << entryID;

    if( !info  )
    {
        sLog.outDebug( "Missing game object info for entry %u", entryID);

        data << uint64(0);
        data << uint64(0);
        data << uint64(0);
        data << uint64(0);
        data << uint64(0);
        data << uint64(0);
        data << uint64(0);
        data << uint64(0);
        data << uint64(0);
        data << uint32(0);
        data << uint16(0);
        data << uint8(0);

        data << uint64(0);                                  // Added in 1.12.x client branch
        data << uint64(0);                                  // Added in 1.12.x client branch
        data << uint64(0);                                  // Added in 1.12.x client branch
        data << uint64(0);                                  // Added in 1.12.x client branch
        data << uint8(0);                                   // Added in 1.12.x client branch
        SendPacket( &data );
        return;
    }

    data << (uint32)info->type;
    data << (uint32)info->displayId;
    data << info->name;
    data << uint16(0);                                      //unknown
    data << uint8(0);                                       //unknown
    data << uint8(0);                                       // Added in 1.12.x client branch
    data << uint32(info->sound0);
    data << uint32(info->sound1);
    data << uint32(info->sound2);
    data << uint32(info->sound3);
    data << uint32(info->sound4);
    data << uint32(info->sound5);
    data << uint32(info->sound6);
    data << uint32(info->sound7);
    data << uint32(info->sound8);
    data << uint32(info->sound9);

    data << uint64(0);
    data << uint64(0);
    data << uint64(0);

    data << uint64(0);                                      // Added in 1.12.x client branch
    data << uint64(0);                                      // Added in 1.12.x client branch
    data << uint64(0);                                      // Added in 1.12.x client branch
    data << uint64(0);                                      // Added in 1.12.x client branch
    SendPacket( &data );
}

void WorldSession::HandleCorpseQueryOpcode(WorldPacket &recv_data)
{
    WorldPacket data;

    sLog.outDetail("WORLD: Received MSG_CORPSE_QUERY");

    Corpse* corpse = GetPlayer()->GetCorpse();

    if(!corpse) return;

    data.Initialize(MSG_CORPSE_QUERY);
    data << uint8(0x01);
    data << uint32(0x01);
    data << corpse->GetPositionX();
    data << corpse->GetPositionY();
    data << corpse->GetPositionZ();
    data << uint32(0x01);
    SendPacket(&data);

}

void WorldSession::HandleNpcTextQueryOpcode( WorldPacket & recv_data )
{
    WorldPacket data;
    uint32 textID;
    uint32 uField0, uField1;
    GossipText *pGossip;
    std::string GossipStr;

    recv_data >> textID;
    sLog.outDetail("WORLD: CMSG_NPC_TEXT_QUERY ID '%u'", textID );

    recv_data >> uField0 >> uField1;
    GetPlayer()->SetUInt32Value(UNIT_FIELD_TARGET, uField0);
    GetPlayer()->SetUInt32Value(UNIT_FIELD_TARGET + 1, uField1);

    pGossip = objmgr.GetGossipText(textID);

    data.Initialize( SMSG_NPC_TEXT_UPDATE );
    data << textID;

    if (!pGossip)
    {
        data << uint32( 0 );
        data << "Greetings $N";
        data << "Greetings $N";
    } else

    for (int i=0; i<8; i++)
    {
        data << pGossip->Options[i].Probability;
        data << pGossip->Options[i].Text_0;

        if ( pGossip->Options[i].Text_1 == "" )
            data << pGossip->Options[i].Text_0; else
            data << pGossip->Options[i].Text_1;

        data << pGossip->Options[i].Language;

        data << pGossip->Options[i].Emotes[0]._Delay;
        data << pGossip->Options[i].Emotes[0]._Emote;

        data << pGossip->Options[i].Emotes[1]._Delay;
        data << pGossip->Options[i].Emotes[1]._Emote;

        data << pGossip->Options[i].Emotes[2]._Delay;
        data << pGossip->Options[i].Emotes[2]._Emote;
    }

    SendPacket( &data );

    sLog.outDetail( "WORLD: Sent SMSG_NPC_TEXT_UPDATE " );
}

void WorldSession::HandlePageQueryOpcode( WorldPacket & recv_data )
{
    WorldPacket data;
    uint32 pageID;

    recv_data >> pageID;
    sLog.outDetail("WORLD: Received CMSG_PAGE_TEXT_QUERY for pageID '%u'", pageID);

    while (pageID)
    {
        ItemPage *pPage = objmgr.RetreiveItemPageText( pageID );
        data.Initialize( SMSG_PAGE_TEXT_QUERY_RESPONSE );
        data << pageID;

        if (!pPage)
        {
            data << "Item page missing.";
            data << uint32(0);
            pageID = 0;
        }
        else
        {
            data << pPage->PageText;
            data << uint32(pPage->Next_Page);
            pageID = pPage->Next_Page;
        }
        SendPacket( &data );

        sLog.outDetail( "WORLD: Sent SMSG_PAGE_TEXT_QUERY_RESPONSE " );
    }
}
