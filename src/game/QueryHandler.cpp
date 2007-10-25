/* 
 * Copyright (C) 2005,2006,2007 MaNGOS <http://www.mangosproject.org/>
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
#include "Language.h"
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

void WorldSession::SendNameQueryOpcode(Player *p)
{
    if(!p)
        return;

                                                            // guess size
    WorldPacket data( SMSG_NAME_QUERY_RESPONSE, (8+1+4+4+4+10) );
    data << p->GetGUID();
    data << p->GetName();
    data << uint8(0);                                       // realm name for cross realm BG usage
    data << uint32(p->getRace());
    data << uint32(p->getGender());
    data << uint32(p->getClass());

    SendPacket(&data);
}

void WorldSession::SendNameQueryOpcodeFromDB(uint64 guid)
{
    std::string name;
    if(!objmgr.GetPlayerNameByGUID(guid, name))
        name = LANG_NON_EXIST_CHARACTER;
    uint32 field = Player::GetUInt32ValueFromDB(UNIT_FIELD_BYTES_0, guid);

                                                            // guess size
    WorldPacket data( SMSG_NAME_QUERY_RESPONSE, (8+1+4+4+4+10) );
    data << guid;
    data << name;
    data << (uint8)0;
    data << (uint32)(field & 0xFF);
    data << (uint32)((field >> 16) & 0xFF);
    data << (uint32)((field >> 8) & 0xFF);

    SendPacket( &data );
}

void WorldSession::HandleNameQueryOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,8);

    uint64 guid;

    recv_data >> guid;

    Player *pChar = objmgr.GetPlayer(guid);

    if (pChar)
        SendNameQueryOpcode(pChar);
    else
        SendNameQueryOpcodeFromDB(guid);
}

void WorldSession::HandleQueryTimeOpcode( WorldPacket & /*recv_data*/ )
{
    WorldPacket data( SMSG_QUERY_TIME_RESPONSE, 4+4 );
    data << (uint32)time(NULL);
    data << (uint32)0;
    SendPacket( &data );
}

/// Only _static_ data send in this packet !!!
void WorldSession::HandleCreatureQueryOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,4+8);

    uint32 entry;
    recv_data >> entry;

    CreatureInfo const *ci = objmgr.GetCreatureTemplate(entry);
    if (ci)
    {
        std::string Name, SubName;
        Name = ci->Name;
        SubName = ci->SubName;

        uint8 m_language = GetSessionLanguage();
        if (m_language > 0)
        {
            CreatureLocale const *cl = objmgr.GetCreatureLocale(entry);
            if (cl)
            {
                if (!cl->Name[m_language].empty())          // default to english
                    Name = cl->Name[m_language];
                if (!cl->SubName[m_language].empty())       // same
                    SubName = cl->SubName[m_language];
            }
        }

        sLog.outDetail("WORLD: CMSG_CREATURE_QUERY '%s' - Entry: %u.", ci->Name, entry);

                                                            // guess size
        WorldPacket data( SMSG_CREATURE_QUERY_RESPONSE, 100 );
        data << (uint32)entry;                              // creature entry
        data << Name;
        data << uint8(0) << uint8(0) << uint8(0);           // name2, name3, name4, always empty
        data << SubName;
        data << ci->flag1;                                  // flags          wdbFeild7=wad flags1
        data << (uint32)ci->type;
        data << (uint32)ci->family;                         // family         wdbFeild9
        data << (uint32)ci->rank;                           // rank           wdbFeild10
        data << uint32(0);                                  // unknown        wdbFeild11
        data << uint32(0);                                  // Id from CreatureSpellData.dbc    wdbFeild12
        data << ci->DisplayID_A;                            // modelid_male1
        data << ci->DisplayID_H;                            // modelid_female1 ?
        data << ci->DisplayID_A2;                           // modelid_male2 ?
        data << ci->DisplayID_H2;                           // modelid_femmale2 ?
        data << (float)1.0f;                                // unk
        data << (float)1.0f;                                // unk
        data << uint8(ci->civilian);
        data << uint8(ci->RacialLeader);
        SendPacket( &data );
        sLog.outDetail( "WORLD: Sent SMSG_CREATURE_QUERY_RESPONSE " );
    }
    else
    {    
        uint64 guid;
        recv_data >> guid;
        
        sLog.outDetail( "WORLD: CMSG_CREATURE_QUERY - (%u) NO CREATURE INFO! (GUID: %u, ENTRY: %u)", uint32(GUID_LOPART(guid)), guid, entry );
        WorldPacket data( SMSG_CREATURE_QUERY_RESPONSE, 4 );
        data << uint32(entry | 0x80000000);
        SendPacket( &data );
        sLog.outDetail( "WORLD: Sent SMSG_CREATURE_QUERY_RESPONSE " );
    }
}

/// Only _static_ data send in this packet !!!
void WorldSession::HandleGameObjectQueryOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,4+8);

    uint32 entryID;
    recv_data >> entryID;

    const GameObjectInfo *info = objmgr.GetGameObjectInfo(entryID);
    if(info)
    {

        std::string Name;
        Name = info->name;

        uint8 m_language = GetSessionLanguage();
        if (m_language > 0)
        {
            GameObjectLocale const *gl = objmgr.GetGameObjectLocale(entryID);
            if (gl)
            {
                if (!gl->Name[m_language].empty())          // default to english
                    Name = gl->Name[m_language];
            }
        }
        sLog.outDetail("WORLD: CMSG_GAMEOBJECT_QUERY '%s' - Entry: %u. ", info->name, entryID);
        WorldPacket data ( SMSG_GAMEOBJECT_QUERY_RESPONSE, 150 );
        data << entryID;
        data << (uint32)info->type;
        data << (uint32)info->displayId;
        data << Name;
        data << uint8(0) << uint8(0) << uint8(0);           // name2, name3, name4
        data << uint8(0);                                   // 2.0.3, string
        data << uint8(0);                                   // 2.0.3, string
        data << uint8(0);                                   // 2.0.3, probably string
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
        data << uint32(info->sound10);
        data << uint32(info->sound11);
        data << uint32(info->sound12);
        data << uint32(info->sound13);
        data << uint32(info->sound14);
        data << uint32(info->sound15);
        data << uint32(info->sound16);
        data << uint32(info->sound17);
        data << uint32(info->sound18);
        data << uint32(info->sound19);
        data << uint32(info->sound20);
        data << uint32(info->sound21);
        data << uint32(info->sound22);
        data << uint32(info->sound23);
        SendPacket( &data );
        sLog.outDetail( "WORLD: Sent CMSG_GAMEOBJECT_QUERY " );
    }
    else
    {

        uint64 guid;
        recv_data >> guid;

        sLog.outDetail( "WORLD: CMSG_GAMEOBJECT_QUERY - (%u) Missing gameobject info for (GUID: %u, ENTRY: %u)", uint32(GUID_LOPART(guid)), guid, entryID );
        WorldPacket data ( SMSG_GAMEOBJECT_QUERY_RESPONSE, 4 );
        data << uint32(entryID | 0x80000000);
        SendPacket( &data );
        sLog.outDetail( "WORLD: Sent CMSG_GAMEOBJECT_QUERY " );
    }
}

void WorldSession::HandleCorpseQueryOpcode(WorldPacket & /*recv_data*/)
{
    sLog.outDetail("WORLD: Received MSG_CORPSE_QUERY");

    Corpse *corpse = GetPlayer()->GetCorpse();

    uint8 found = 1;
    if(!corpse)
        found = 0;

    WorldPacket data(MSG_CORPSE_QUERY, (1+found*(5*4)));
    data << uint8(found);
    if(found)
    {
        data << corpse->GetMapId();
        data << corpse->GetPositionX();
        data << corpse->GetPositionY();
        data << corpse->GetPositionZ();
        data << _player->GetMapId();
    }
    SendPacket(&data);
}

void WorldSession::HandleNpcTextQueryOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,4+4+4);

    uint32 textID;
    uint64 guid;
    GossipText *pGossip;
    std::string GossipStr;

    recv_data >> textID;
    sLog.outDetail("WORLD: CMSG_NPC_TEXT_QUERY ID '%u'", textID);

    recv_data >> guid;
    GetPlayer()->SetUInt64Value(UNIT_FIELD_TARGET, guid);

    pGossip = objmgr.GetGossipText(textID);

    WorldPacket data( SMSG_NPC_TEXT_UPDATE, 100 );          // guess size
    data << textID;

    if (!pGossip)
    {
        data << uint32( 0 );
        data << "Greetings $N";
        data << "Greetings $N";
    }
    else
    {
        std::string Text_0[8], Text_1[8];
        for (int i=0;i<8;i++)
        {
            Text_0[i]=pGossip->Options[i].Text_0;
            Text_1[i]=pGossip->Options[i].Text_1;
        }

        uint8 m_language = GetSessionLanguage();
        if (m_language > 0)
        {
            NpcTextLocale const *nl = objmgr.GetNpcTextLocale(textID);
            if (nl)
            {
                for (int i=0;i<8;i++)
                {
                    if (!nl->Text_0[i][m_language].empty())
                        Text_0[i]=nl->Text_0[i][m_language];
                    if (!nl->Text_1[i][m_language].empty())
                        Text_1[i]=nl->Text_1[i][m_language];
                }
            }
        }

        for (int i=0; i<8; i++)
        {
            data << pGossip->Options[i].Probability;

            if ( Text_0[i] == "" )
                data << Text_1[i];
            else
                data << Text_0[i];

            if ( Text_1[i] == "" )
                data << Text_0[i];
            else
                data << Text_1[i];

            data << pGossip->Options[i].Language;

            data << pGossip->Options[i].Emotes[0]._Delay;
            data << pGossip->Options[i].Emotes[0]._Emote;

            data << pGossip->Options[i].Emotes[1]._Delay;
            data << pGossip->Options[i].Emotes[1]._Emote;

            data << pGossip->Options[i].Emotes[2]._Delay;
            data << pGossip->Options[i].Emotes[2]._Emote;
        }
    }

    SendPacket( &data );

    sLog.outDetail( "WORLD: Sent SMSG_NPC_TEXT_UPDATE " );
}

void WorldSession::HandlePageQueryOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,4);

    uint32 pageID;

    recv_data >> pageID;
    sLog.outDetail("WORLD: Received CMSG_PAGE_TEXT_QUERY for pageID '%u'", pageID);

    while (pageID)
    {
        PageText const *pPage = sPageTextStore.LookupEntry<PageText>( pageID );
                                                            // guess size
        WorldPacket data( SMSG_PAGE_TEXT_QUERY_RESPONSE, 50 );
        data << pageID;

        if (!pPage)
        {
            data << "Item page missing.";
            data << uint32(0);
            pageID = 0;
        }
        else
        {
            std::string Text = pPage->Text;
            uint8 m_language = GetSessionLanguage();
            if (m_language > 0)
            {
                PageTextLocale const *pl = objmgr.GetPageTextLocale(pageID);
                if (pl)
                {
                    if (!pl->Text[m_language].empty())
                        Text = pl->Text[m_language];
                }
            }
            data << Text;
            data << uint32(pPage->Next_Page);
            pageID = pPage->Next_Page;
        }
        SendPacket( &data );

        sLog.outDetail( "WORLD: Sent SMSG_PAGE_TEXT_QUERY_RESPONSE " );
    }
}
