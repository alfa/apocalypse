/* 
 * Copyright (C) 2005-2008 MaNGOS <http://www.mangosproject.org/>
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
#include "BattleGround.h"
#include "SpellAuras.h"
#include "Pet.h"

void WorldSession::HandleRepopRequestOpcode( WorldPacket & /*recv_data*/ )
{
    sLog.outDebug( "WORLD: Recvd CMSG_REPOP_REQUEST Message" );

    if(GetPlayer()->isAlive()||GetPlayer()->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
        return;

    //this is spirit release confirm?
    GetPlayer()->RemovePet(NULL,PET_SAVE_NOT_IN_SLOT, true);
    GetPlayer()->BuildPlayerRepop();
    GetPlayer()->RepopAtGraveyard();
}

void WorldSession::HandleWhoOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,4+4+1+1+4+4+4+4);

    sLog.outDebug( "WORLD: Recvd CMSG_WHO Message" );
    //recv_data.hexlike();

    uint32 clientcount = 0;

    uint32 level_min, level_max, racemask, classmask, zones_count, str_count;
    uint32 zoneids[10];                                     // 10 is client limit
    std::string player_name, guild_name;
    std::string str[4];                                     // 4 is client limit

    recv_data >> level_min;                                 // maximal player level, default 0
    recv_data >> level_max;                                 // minimal player level, default 100
    recv_data >> player_name;                               // player name, case sensitive...

    // recheck
    CHECK_PACKET_SIZE(recv_data,4+4+(player_name.size()+1)+1+4+4+4+4);

    recv_data >> guild_name;                                // guild name, case sensitive...

    // recheck
    CHECK_PACKET_SIZE(recv_data,4+4+(player_name.size()+1)+(guild_name.size()+1)+4+4+4+4);

    recv_data >> racemask;                                  // race mask
    recv_data >> classmask;                                 // class mask
    recv_data >> zones_count;                               // zones count, client limit=10 (2.0.10)

    if(zones_count > 10)
        return;                                             // can't be received from real client or broken packet

    // recheck
    CHECK_PACKET_SIZE(recv_data,4+4+(player_name.size()+1)+(guild_name.size()+1)+4+4+4+(4*zones_count)+4);

    for(uint32 i = 0; i < zones_count; i++)
    {
        uint32 temp;
        recv_data >> temp;                                  // zone id, 0 if zone is unknown...
        zoneids[i] = temp;
        sLog.outDebug("Zone %u: %u", i, zoneids[i]);
    }

    recv_data >> str_count;                                 // user entered strings count, client limit=4 (checked on 2.0.10)

    if(str_count > 4)
        return;                                             // can't be received from real client or broken packet

    // recheck
    CHECK_PACKET_SIZE(recv_data,4+4+(player_name.size()+1)+(guild_name.size()+1)+4+4+4+(4*zones_count)+4+(1*str_count));

    sLog.outDebug("Minlvl %u, maxlvl %u, name %s, guild %s, racemask %u, classmask %u, zones %u, strings %u", level_min, level_max, player_name.c_str(), guild_name.c_str(), racemask, classmask, zones_count, str_count);
    for(uint32 i = 0; i < str_count; i++)
    {
        // recheck (have one more byte)
        CHECK_PACKET_SIZE(recv_data,recv_data.rpos());

        std::string temp;
        recv_data >> temp;                                  // user entered string, it used as universal search pattern(guild+player name)?
        str[i] = temp;
        sLog.outDebug("String %u: %s", i, str[i].c_str());
    }

    // client send in case not set max level value 100 but mangos support 255 max level,
    // update it to show GMs with characters after 100 level
    if(level_max >= 100)
        level_max = 255;

    uint32 team = _player->GetTeam();
    uint32 security = GetSecurity();
    bool allowTwoSideWhoList = sWorld.getConfig(CONFIG_ALLOW_TWO_SIDE_WHO_LIST);
    bool gmInWhoList         = sWorld.getConfig(CONFIG_GM_IN_WHO_LIST) || security > SEC_PLAYER;

    WorldPacket data( SMSG_WHO, 50 );                       // guess size
    data << clientcount;                                    // clientcount place holder
    data << clientcount;                                    // clientcount place holder

    //TODO: Guard Player map
    HashMapHolder<Player>::MapType& m = ObjectAccessor::Instance().GetPlayers();
    for(HashMapHolder<Player>::MapType::iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        std::string gname = objmgr.GetGuildNameById(itr->second->GetGuildId());
        const char *pname = itr->second->GetName();
        uint32 lvl = itr->second->getLevel();
        uint32 class_ = itr->second->getClass();
        uint32 race = itr->second->getRace();
        uint32 pzoneid = itr->second->GetZoneId();
        bool z_show = true;
        bool s_show = true;

        for(uint32 i = 0; i < zones_count; i++)
        {
            if(zoneids[i] == pzoneid)
            {
                z_show = true;
                break;
            }

            z_show = false;
        }

        for(uint32 i = 0; i < str_count; i++)
        {
            s_show = str[i].length() ? strstr(gname.c_str(), str[i].c_str())!=0 : true;
            if(s_show)
                break;
            s_show = str[i].length() ? strstr(pname, str[i].c_str())!=0 : true;
            if(s_show)
                break;
        }

        // PLAYER see his team only and PLAYER can't see MODERATOR, GAME MASTER, ADMINISTRATOR characters
        // MODERATOR, GAME MASTER, ADMINISTRATOR can see all
        if( pname &&
            ( security > SEC_PLAYER || itr->second->GetTeam() == team || allowTwoSideWhoList ) &&
            (classmask & (1 << class_) ) && (racemask & (1 << race) ) &&
            (lvl >= level_min && lvl <= level_max) &&
            (guild_name.length()?strstr(gname.c_str(), guild_name.c_str())!=0 : true) &&
            (player_name.length()?strstr(pname, player_name.c_str())!=0 : true) &&
            z_show && s_show &&
            (itr->second->GetSession()->GetSecurity() == SEC_PLAYER || gmInWhoList && itr->second->IsVisibleGloballyFor(_player) ) &&
            clientcount < 49)
        {
            clientcount++;

            data << pname;                                  // player name
            data << gname;                                  // guild name
            data << uint32( lvl );                          // player level
            data << uint32( class_ );                       // player class
            data << uint32( race );                         // player race
            data << uint32( pzoneid );                      // player zone id
        }
    }

    data.put( 0,              clientcount );                //insert right count
    data.put( sizeof(uint32), clientcount );                //insert right count

    SendPacket(&data);
    sLog.outDebug( "WORLD: Send SMSG_WHO Message" );
}

void WorldSession::HandleLogoutRequestOpcode( WorldPacket & /*recv_data*/ )
{
    Player* Target = GetPlayer();

    uint32 security = Target->GetSession()->GetSecurity();

    sLog.outDebug( "WORLD: Recvd CMSG_LOGOUT_REQUEST Message, security - %u", security );

    //instant logout for admins, gm's, mod's
    if (security > SEC_PLAYER)
    {
        LogoutPlayer(true);
        return;
    }

    //Can not logout if...
    if( Target->isInCombat() ||                             //...is in combat
        Target->duel         ||                             //...is in Duel
                                                            //...is jumping ...is falling
        Target->HasMovementFlags( MOVEMENTFLAG_JUMPING | MOVEMENTFLAG_FALLING ))
    {
        WorldPacket data( SMSG_LOGOUT_RESPONSE, (2+4) ) ;
        data << (uint8)0xC;
        data << uint32(0);
        data << uint8(0);
        SendPacket( &data );
        LogoutRequest(0);
        return;
    }

    //instant logout in taverns/cities
    if(Target->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING))
    {
        LogoutPlayer(true);
        return;
    }

    // not set flags if player can't free move to prevent lost state at logout cancel
    if(GetPlayer()->CanFreeMove())
    {
        Target->SetFlag(UNIT_FIELD_BYTES_1,PLAYER_STATE_SIT);
        Target->SetStandState(PLAYER_STATE_SIT);

        WorldPacket data( SMSG_FORCE_MOVE_ROOT, (8+4) );    // guess size
        data.append(Target->GetPackGUID());
        data << (uint32)2;
        SendPacket( &data );
        Target->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_DISABLE_ROTATE);
    }

    WorldPacket data( SMSG_LOGOUT_RESPONSE, 5 );
    data << uint32(0);
    data << uint8(0);
    SendPacket( &data );
    LogoutRequest(time(NULL));
}

void WorldSession::HandlePlayerLogoutOpcode( WorldPacket & /*recv_data*/ )
{
    sLog.outDebug( "WORLD: Recvd CMSG_PLAYER_LOGOUT Message" );
}

void WorldSession::HandleLogoutCancelOpcode( WorldPacket & /*recv_data*/ )
{
    sLog.outDebug( "WORLD: Recvd CMSG_LOGOUT_CANCEL Message" );

    LogoutRequest(0);

    WorldPacket data( SMSG_LOGOUT_CANCEL_ACK, 0 );
    SendPacket( &data );

    // not remove flags if can't free move - its not set in Logout request code.
    if(GetPlayer()->CanFreeMove())
    {
        //!we can move again
        data.Initialize( SMSG_FORCE_MOVE_UNROOT, 8 );       // guess size
        data.append(GetPlayer()->GetPackGUID());
        data << uint32(0);
        SendPacket( &data );

        //! Stand Up
        //! Removes the flag so player stands
        GetPlayer()->RemoveFlag(UNIT_FIELD_BYTES_1,PLAYER_STATE_SIT);
        GetPlayer()->SetStandState(PLAYER_STATE_NONE);

        //! DISABLE_ROTATE
        GetPlayer()->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_DISABLE_ROTATE);
    }

    sLog.outDebug( "WORLD: sent SMSG_LOGOUT_CANCEL_ACK Message" );
}

void WorldSession::SendGMTicketGetTicket(uint32 status, char const* text)
{
    int len = text ? strlen(text) : 0;
    WorldPacket data( SMSG_GMTICKET_GETTICKET, (4+len+1+4+2+4+4) );
    data << status;                                         // standart 0x0A, 0x06 if text present
    if(status == 6)
    {
        data << text;                                       // ticket text
        data << uint8(0x7);                                 // ticket category
        data << uint32(0);                                  // time from ticket creation?
        data << uint16(0);                                  // const
        data << uint32(49024);                              // const
        data << uint32(49024);                              // const
    }
    SendPacket( &data );
}

void WorldSession::HandleGMTicketGetTicketOpcode( WorldPacket & /*recv_data*/ )
{
    WorldPacket data( SMSG_QUERY_TIME_RESPONSE, 4+4 );
    data << (uint32)time(NULL);
    data << (uint32)0;
    SendPacket( &data );

    uint64 guid;
    Field *fields;
    guid = GetPlayer()->GetGUID();

    QueryResult *result = CharacterDatabase.PQuery("SELECT COUNT(`ticket_id`) FROM `character_ticket` WHERE `guid` = '%u'", GUID_LOPART(guid));

    if (result)
    {
        int cnt;
        fields = result->Fetch();
        cnt = fields[0].GetUInt32();
        delete result;

        if ( cnt > 0 )
        {
            QueryResult *result2 = CharacterDatabase.PQuery("SELECT `ticket_text` FROM `character_ticket` WHERE `guid` = '%u'", GUID_LOPART(guid));
            if(result2)
            {
                Field *fields2 = result2->Fetch();
                SendGMTicketGetTicket(0x06,fields2[0].GetString());
                delete result2;
            }
        }
        else
            SendGMTicketGetTicket(0x0A,0);
    }
}

void WorldSession::HandleGMTicketUpdateTextOpcode( WorldPacket & recv_data )
{
    uint8 unk;
    std::string ticketText;
    recv_data >> unk;
    recv_data >> ticketText;

    CharacterDatabase.escape_string(ticketText);
    CharacterDatabase.PExecute("UPDATE `character_ticket` SET `ticket_text` = '%s' WHERE `guid` = '%u'", ticketText.c_str(), _player->GetGUIDLow());
}

void WorldSession::HandleGMTicketDeleteOpcode( WorldPacket & /*recv_data*/ )
{
    uint32 guid = GetPlayer()->GetGUIDLow();

    CharacterDatabase.PExecute("DELETE FROM `character_ticket` WHERE `guid` = '%u' LIMIT 1",guid);

    WorldPacket data( SMSG_GMTICKET_DELETETICKET, 8 );
    data << uint32(9);
    //data << uint32(0);
    SendPacket( &data );

    SendGMTicketGetTicket(0x0A, 0);
}

void WorldSession::HandleGMTicketCreateOpcode( WorldPacket & recv_data )
{
    uint32 map;
    float x, y, z;
    uint8 category;
    std::string ticketText = "";
    std::string unk_text;                                   // "Reserved for future use" text

    recv_data >> category >> map >> x >> y >> z;            // last check 2.0.12
    recv_data >> ticketText;
    recv_data >> unk_text;

    sLog.outDebug("TicketCreate: category %u, map %u, x %f, y %f, z %f, text %s, unk_text %s", category, map, x, y, z, ticketText.c_str(), unk_text.c_str());

    CharacterDatabase.escape_string(ticketText);

    QueryResult *result = CharacterDatabase.PQuery("SELECT COUNT(*) FROM `character_ticket` WHERE `guid` = '%u'", _player->GetGUIDLow());

    if (result)
    {
        int cnt;
        Field *fields = result->Fetch();
        cnt = fields[0].GetUInt32();
        delete result;

        if ( cnt > 0 )
        {
            WorldPacket data( SMSG_GMTICKET_CREATE, 4 );
            data << uint32(1);
            SendPacket( &data );
        }
        else
        {
            CharacterDatabase.PExecute("INSERT INTO `character_ticket` (`guid`,`ticket_text`,`ticket_category`) VALUES ('%u', '%s', '%u')", _player->GetGUIDLow(), ticketText.c_str(), category);

            WorldPacket data( SMSG_QUERY_TIME_RESPONSE, 4+4 );
            data << (uint32)time(NULL);
            data << (uint32)0;
            SendPacket( &data );

            data.Initialize( SMSG_GMTICKET_CREATE, 4 );
            data << uint32(2);
            SendPacket( &data );
            DEBUG_LOG("update the ticket\n");

            //TODO: Guard player map
            HashMapHolder<Player>::MapType &m = ObjectAccessor::Instance().GetPlayers();
            for(HashMapHolder<Player>::MapType::iterator itr = m.begin(); itr != m.end(); ++itr)
            {
                if(itr->second->GetSession()->GetSecurity() >= SEC_GAMEMASTER && itr->second->isAcceptTickets())
                    ChatHandler(itr->second).PSendSysMessage(LANG_COMMAND_TICKETNEW,GetPlayer()->GetName());
            }
        }
    }
}

void WorldSession::HandleGMTicketSystemStatusOpcode( WorldPacket & /*recv_data*/ )
{
    WorldPacket data( SMSG_GMTICKET_SYSTEMSTATUS,4 );
    data << uint32(1);                                      // we can also disable ticket system by sending 0 value

    SendPacket( &data );
}

void WorldSession::HandleTogglePvP(WorldPacket& /*recvPacket*/)
{
    GetPlayer()->ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP);

    if(GetPlayer()->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP))
    {
        if(!GetPlayer()->IsPvP() || GetPlayer()->pvpInfo.endTimer != 0)
            GetPlayer()->UpdatePvP(true, true);
    }
    else
    {
        if(!GetPlayer()->pvpInfo.inHostileArea && GetPlayer()->IsPvP())
            GetPlayer()->pvpInfo.endTimer = time(NULL);     // start toggle-off
    }
}

void WorldSession::HandleZoneUpdateOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,4);

    uint32 newZone;
    recv_data >> newZone;

    sLog.outDetail("WORLD: Recvd ZONE_UPDATE: %u", newZone);

    if(newZone != _player->GetZoneId())
        GetPlayer()->SendInitWorldStates();                 // only if really enters to new zone, not just area change, works strange...

    GetPlayer()->UpdateZone(newZone);
}

void WorldSession::HandleSetTargetOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,8);

    uint64 guid ;
    recv_data >> guid;

    if( !_player  )
        return;

    _player->SetTarget(guid);

    // update reputation list if need
    Unit* unit = ObjectAccessor::GetUnit(*_player, guid );
    if(!unit)
        return;

    _player->SetFactionVisibleForFactionTemplateId(unit->getFaction());
}

void WorldSession::HandleSetSelectionOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,8);

    uint64 guid;
    recv_data >> guid;

    if( !_player )
        return;

    _player->SetSelection(guid);

    // update reputation list if need
    Unit* unit = ObjectAccessor::GetUnit(*_player, guid );
    if(!unit)
        return;

    _player->SetFactionVisibleForFactionTemplateId(unit->getFaction());
}

void WorldSession::HandleStandStateChangeOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,1);

    sLog.outDebug( "WORLD: Received CMSG_STAND_STATE_CHANGE"  );
    if( GetPlayer() != 0 )
    {
        uint8 animstate;
        recv_data >> animstate;

        _player->SetStandState(animstate);

        uint32 bytes1 = _player->GetUInt32Value( UNIT_FIELD_BYTES_1 );
        bytes1 &=0xFFFFFF00;
        bytes1 |=animstate;
        _player->SetUInt32Value(UNIT_FIELD_BYTES_1 , bytes1);

        if (_player->IsStandState())
        {
            // cancel drinking / eating
            Unit::AuraMap& p_auras = _player->GetAuras();
            for (Unit::AuraMap::iterator itr = p_auras.begin(); itr != p_auras.end();)
            {
                if (itr->second && (itr->second->GetSpellProto()->AuraInterruptFlags & AURA_INTERRUPT_FLAG_NOT_SEATED) != 0)
                    _player->RemoveAura(itr);
                else
                    ++itr;
            }
        }
    }
}

void WorldSession::HandleFriendListOpcode( WorldPacket & /*recv_data*/ )
{
    sLog.outDebug( "WORLD: Received CMSG_FRIEND_LIST"  );

    GetPlayer()->SendFriendlist();
    GetPlayer()->SendIgnorelist();
}

void WorldSession::HandleAddFriendOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,1);

    sLog.outDebug( "WORLD: Received CMSG_ADD_FRIEND"  );

    std::string friendName  = objmgr.GetMangosString(LANG_FRIEND_IGNORE_UNKNOWN,GetSessionLocaleIndex());
    uint8 friendResult      = FRIEND_NOT_FOUND;

    Player *pFriend     = NULL;
    uint64 friendGuid   = 0;
    uint32 friendArea   = 0;
    uint32 friendLevel  = 0;
    uint32 friendClass  = 0;

    recv_data >> friendName;

    if(friendName.empty())
        return;

    normalizePlayerName(friendName);
    CharacterDatabase.escape_string(friendName);            // prevent SQL injection - normal name don't must changed by this call

    sLog.outDebug(  "WORLD: %s asked to add friend : '%s'",
        GetPlayer()->GetName(), friendName.c_str() );

    friendGuid = objmgr.GetPlayerGUIDByName(friendName);

    if(friendGuid)
    {
        pFriend = ObjectAccessor::FindPlayer(friendGuid);
        if(pFriend==GetPlayer())
            friendResult = FRIEND_SELF;
        else if(GetPlayer()->GetTeam()!=objmgr.GetPlayerTeamByGUID(friendGuid) && !sWorld.getConfig(CONFIG_ALLOW_TWO_SIDE_ADD_FRIEND))
            friendResult = FRIEND_ENEMY;
        else if(GetPlayer()->HasInFriendList(friendGuid))
            friendResult = FRIEND_ALREADY;
    }

    WorldPacket data( SMSG_FRIEND_STATUS, (1+8+1+4+4+4) );  // guess size

    if (friendGuid && friendResult==FRIEND_NOT_FOUND)
    {
        if( pFriend && pFriend->IsInWorld() && pFriend->IsVisibleGloballyFor(GetPlayer()))
        {
            friendResult = FRIEND_ADDED_ONLINE;
            friendArea   = pFriend->GetZoneId();
            friendLevel  = pFriend->getLevel();
            friendClass  = pFriend->getClass();
        }
        else
            friendResult = FRIEND_ADDED_OFFLINE;

        if(!GetPlayer()->AddToFriendList(friendGuid, friendName))
        {
            friendResult = FRIEND_LIST_FULL;
            sLog.outDebug(  "WORLD: %s's friend list is full.", GetPlayer()->GetName());
        }

        sLog.outDebug(  "WORLD: %s Guid found '%u' area:%u Level:%u Class:%u. ",
            friendName.c_str(), GUID_LOPART(friendGuid), friendArea, friendLevel, friendClass);
    }
    else if(friendResult==FRIEND_ALREADY)
    {
        sLog.outDebug(  "WORLD: %s Guid Already a Friend. ", friendName.c_str() );
    }
    else if(friendResult==FRIEND_SELF)
    {
        sLog.outDebug(  "WORLD: %s Guid can't add himself. ", friendName.c_str() );
    }
    else
    {
        sLog.outDebug(  "WORLD: %s Guid not found. ", friendName.c_str() );
    }

    data << (uint8)friendResult << (uint64)friendGuid << (uint8)0;
    if(friendResult == FRIEND_ADDED_ONLINE)
        data << (uint32)friendArea << (uint32)friendLevel << (uint32)friendClass;

    SendPacket( &data );

    sLog.outDebug( "WORLD: Sent (SMSG_FRIEND_STATUS)" );
}

void WorldSession::HandleDelFriendOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,8);

    uint64 FriendGUID;
    uint8 FriendResult = FRIEND_REMOVED;

    sLog.outDebug( "WORLD: Received CMSG_DEL_FRIEND"  );
    recv_data >> FriendGUID;

    GetPlayer()->RemoveFromFriendList(FriendGUID);

    WorldPacket data( SMSG_FRIEND_STATUS, 9 );
    data << (uint8)FriendResult << (uint64)FriendGUID;
    SendPacket( &data );

    sLog.outDebug( "WORLD: Sent motd (SMSG_FRIEND_STATUS)" );
}

void WorldSession::HandleAddIgnoreOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,1);

    sLog.outDebug( "WORLD: Received CMSG_ADD_IGNORE"  );

    std::string IgnoreName = objmgr.GetMangosString(LANG_FRIEND_IGNORE_UNKNOWN,GetSessionLocaleIndex());
    unsigned char ignoreResult = FRIEND_IGNORE_NOT_FOUND;
    uint64 IgnoreGuid = 0;

    recv_data >> IgnoreName;

    if(IgnoreName.empty())
        return;

    normalizePlayerName(IgnoreName);
    CharacterDatabase.escape_string(IgnoreName);            // prevent SQL injection - normal name don't must changed by this call

    sLog.outDebug(  "WORLD: %s asked to Ignore: '%s'",
        GetPlayer()->GetName(), IgnoreName.c_str() );

    IgnoreGuid = objmgr.GetPlayerGUIDByName(IgnoreName);

    if(IgnoreGuid)
    {
        if(IgnoreGuid==GetPlayer()->GetGUID())
            ignoreResult = FRIEND_IGNORE_SELF;
        else
        {
            if( GetPlayer()->HasInIgnoreList(IgnoreGuid) )
                ignoreResult = FRIEND_IGNORE_ALREADY;
        }
    }

    WorldPacket data( SMSG_FRIEND_STATUS, 9 );

    if (IgnoreGuid && ignoreResult == FRIEND_IGNORE_NOT_FOUND)
    {
        ignoreResult = FRIEND_IGNORE_ADDED;

        GetPlayer()->AddToIgnoreList(IgnoreGuid,IgnoreName);
    }
    else if(ignoreResult==FRIEND_IGNORE_ALREADY)
    {
        sLog.outDebug(  "WORLD: %s Guid Already Ignored. ", IgnoreName.c_str() );
    }
    else if(ignoreResult==FRIEND_IGNORE_SELF)
    {
        sLog.outDebug(  "WORLD: %s Guid can't add himself. ", IgnoreName.c_str() );
    }
    else
    {
        sLog.outDebug(  "WORLD: %s Guid not found. ", IgnoreName.c_str() );
    }

    data << (uint8)ignoreResult << (uint64)IgnoreGuid;

    SendPacket( &data );
    sLog.outDebug( "WORLD: Sent (SMSG_FRIEND_STATUS)" );
}

void WorldSession::HandleDelIgnoreOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,8);

    uint64 IgnoreGUID;

    sLog.outDebug( "WORLD: Received CMSG_DEL_IGNORE"  );
    recv_data >> IgnoreGUID;

    unsigned char IgnoreResult = FRIEND_IGNORE_REMOVED;

    WorldPacket data( SMSG_FRIEND_STATUS, 9 );

    data << (uint8)IgnoreResult << (uint64)IgnoreGUID;

    GetPlayer()->RemoveFromIgnoreList(IgnoreGUID);

    SendPacket( &data );

    sLog.outDebug( "WORLD: Sent motd (SMSG_FRIEND_STATUS)" );
}

void WorldSession::HandleBugOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,4+4+1+4+1);

    uint32 suggestion, contentlen;
    std::string content;
    uint32 typelen;
    std::string type;

    recv_data >> suggestion >> contentlen >> content;

    //recheck
    CHECK_PACKET_SIZE(recv_data,4+4+(content.size()+1)+4+1);

    recv_data >> typelen >> type;

    if( suggestion == 0 )
        sLog.outDebug( "WORLD: Received CMSG_BUG [Bug Report]" );
    else
        sLog.outDebug( "WORLD: Received CMSG_BUG [Suggestion]" );

    sLog.outDebug( type.c_str( ) );
    sLog.outDebug( content.c_str( ) );

    CharacterDatabase.escape_string(type);
    CharacterDatabase.escape_string(content);
    CharacterDatabase.PExecute ("INSERT INTO `bugreport` (`type`,`content`) VALUES('%s', '%s')", type.c_str( ), content.c_str( ));
}

void WorldSession::HandleCorpseReclaimOpcode(WorldPacket &recv_data)
{
    CHECK_PACKET_SIZE(recv_data,8);

    sLog.outDetail("WORLD: Received CMSG_RECLAIM_CORPSE");
    if (GetPlayer()->isAlive())
        return;

    // body not released yet
    if(!GetPlayer()->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
        return;

    Corpse *corpse = GetPlayer()->GetCorpse();

    if (!corpse )
        return;

    // prevent resurrect before 30-sec delay after body release not finished
    if(corpse->GetGhostTime() + CORPSE_RECLAIM_DELAY > time(NULL))
        return;

    float dist = corpse->GetDistance2dSq(GetPlayer());
    sLog.outDebug("Corpse 2D Distance: \t%f",dist);
    if (dist > CORPSE_RECLAIM_RADIUS*CORPSE_RECLAIM_RADIUS)
        return;

    uint64 guid;
    recv_data >> guid;

    // resurrect
    GetPlayer()->ResurrectPlayer(GetPlayer()->InBattleGround() ? 1.0f : 0.5f);

    // spawn bones
    GetPlayer()->SpawnCorpseBones();

    GetPlayer()->SaveToDB();
}

void WorldSession::HandleResurrectResponseOpcode(WorldPacket & recv_data)
{
    CHECK_PACKET_SIZE(recv_data,8+1);

    sLog.outDetail("WORLD: Received CMSG_RESURRECT_RESPONSE");

    if(GetPlayer()->isAlive())
        return;

    uint64 guid;
    uint8 status;
    recv_data >> guid;
    recv_data >> status;

    if(status == 0)
        return;

    if(GetPlayer()->m_resurrectGUID == 0)
        return;

    GetPlayer()->ResurrectPlayer(0.0f,false);

    if(GetPlayer()->GetMaxHealth() > GetPlayer()->m_resurrectHealth)
        GetPlayer()->SetHealth( GetPlayer()->m_resurrectHealth );
    else
        GetPlayer()->SetHealth( GetPlayer()->GetMaxHealth() );

    if(GetPlayer()->GetMaxPower(POWER_MANA) > GetPlayer()->m_resurrectMana)
        GetPlayer()->SetPower(POWER_MANA, GetPlayer()->m_resurrectMana );
    else
        GetPlayer()->SetPower(POWER_MANA, GetPlayer()->GetMaxPower(POWER_MANA) );

    GetPlayer()->SetPower(POWER_RAGE, 0 );

    GetPlayer()->SetPower(POWER_ENERGY, GetPlayer()->GetMaxPower(POWER_ENERGY) );

    GetPlayer()->SpawnCorpseBones();

    GetPlayer()->TeleportTo(GetPlayer()->GetMapId(), GetPlayer()->m_resurrectX, GetPlayer()->m_resurrectY, GetPlayer()->m_resurrectZ, GetPlayer()->GetOrientation());

    GetPlayer()->m_resurrectGUID = 0;
    GetPlayer()->m_resurrectHealth = GetPlayer()->m_resurrectMana = 0;
    GetPlayer()->m_resurrectX = GetPlayer()->m_resurrectY = GetPlayer()->m_resurrectZ = 0;

    GetPlayer()->SaveToDB();
}

void WorldSession::HandleAreaTriggerOpcode(WorldPacket & recv_data)
{
    CHECK_PACKET_SIZE(recv_data,4);

    sLog.outDebug("WORLD: Received CMSG_AREATRIGGER");

    uint32 Trigger_ID;

    recv_data >> Trigger_ID;
    sLog.outDebug("Trigger ID:%u",Trigger_ID);

    if(GetPlayer()->isInFlight())
    {
        sLog.outDebug("Player '%s' in flight, ignore Area Trigger ID:%u",GetPlayer()->GetName(),Trigger_ID);
        return;
    }

    AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(Trigger_ID);
    if(!atEntry)
    {
        sLog.outDebug("Player '%s' send unknown (by DBC) Area Trigger ID:%u",GetPlayer()->GetName(),Trigger_ID);
        return;
    }

    if (GetPlayer()->GetMapId()!=atEntry->mapid)
    {
        sLog.outDebug("Player '%s' too far (trigger map: %u player map: %u), ignore Area Trigger ID: %u", GetPlayer()->GetName(), atEntry->mapid, GetPlayer()->GetMapId(), Trigger_ID);
        return;
    }

    // delta is safe radius
    const float delta = 5.0f;
    // check if player in the range of areatrigger
    Player* pl = GetPlayer();

    if (atEntry->radius > 0)
    {
        // if we have radius check it
        float dist = pl->GetDistance(atEntry->x,atEntry->y,atEntry->z);
        if(dist > atEntry->radius + delta)
        {
            sLog.outDebug("Player '%s' too far (radius: %f distance: %f), ignore Area Trigger ID: %u",
                pl->GetName(), atEntry->radius, dist, Trigger_ID);
            return;
        }
    }
    else
    {
        // we have only extent
        float dx = pl->GetPositionX() - atEntry->x;
        float dy = pl->GetPositionY() - atEntry->y;
        float dz = pl->GetPositionZ() - atEntry->z;
        double es = sin(atEntry->box_orientation);
        double ec = cos(atEntry->box_orientation);
        // calc rotated vector based on extent axis
        double rotateDx = dx*ec - dy*es;
        double rotateDy = dx*es + dy*ec;

        if( (fabs(rotateDx) > atEntry->box_x/2 + delta) ||
            (fabs(rotateDy) > atEntry->box_y/2 + delta) ||
            (fabs(dz) > atEntry->box_z/2 + delta) )
        {
            sLog.outDebug("Player '%s' too far (1/2 box X: %f 1/2 box Y: %u 1/2 box Z: %u rotate dX: %f rotate dY: %f dZ:%f), ignore Area Trigger ID: %u",
                pl->GetName(), atEntry->box_x/2, atEntry->box_y/2, atEntry->box_z/2, rotateDx, rotateDy, dz, Trigger_ID);
            return;
        }
    }

    uint32 quest_id = objmgr.GetQuestForAreaTrigger( Trigger_ID );
    if( quest_id && GetPlayer()->isAlive() && GetPlayer()->IsActiveQuest(quest_id) )
    {
        Quest const* pQuest = objmgr.GetQuestTemplate(quest_id);
        if( pQuest )
        {
            if( !Script->scriptAreaTrigger( GetPlayer(), pQuest, Trigger_ID ) )
            {
                if(GetPlayer()->GetQuestStatus(quest_id) == QUEST_STATUS_INCOMPLETE)
                    GetPlayer()->AreaExploredOrEventHappens( quest_id );
            }
        }
    }

    if(objmgr.IsTavernAreaTrigger(Trigger_ID))
    {
        // set resting flag we are in the inn
        GetPlayer()->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING);
        GetPlayer()->InnEnter(time(NULL), atEntry->mapid, atEntry->x, atEntry->y, atEntry->z);
        GetPlayer()->SetRestType(REST_TYPE_IN_TAVERN);

        if(sWorld.IsFFAPvPRealm())
            GetPlayer()->RemoveFlag(PLAYER_FLAGS,PLAYER_FLAGS_FFA_PVP);

        return;
    }

    if(GetPlayer()->InBattleGround())
    {
        BattleGround* bg = GetPlayer()->GetBattleGround();
        if(bg)
            if(bg->GetStatus() == STATUS_IN_PROGRESS)
                bg->HandleAreaTrigger(GetPlayer(), Trigger_ID);

        return;
    }

    // NULL if all values default (non teleport trigger)
    AreaTrigger const* at = objmgr.GetAreaTrigger(Trigger_ID);

    if(at)
    {
        if(at->requiredItem)
        {
            uint32 ReqItem = at->requiredItem;
            ItemPrototype const *pProto = objmgr.GetItemPrototype(ReqItem);
            // pProto != NULL checked and fixed (if need with error output) at server load and don't must be happens here

            // item and level or GM
            if( (!pProto || GetPlayer()->HasItemCount(ReqItem, 1)) &&
                (GetPlayer()->getLevel() >= at->requiredLevel || sWorld.getConfig(CONFIG_INSTANCE_IGNORE_LEVEL))
                || GetPlayer()->isGameMaster() )
                GetPlayer()->TeleportTo(at->target_mapId,at->target_X,at->target_Y,at->target_Z,at->target_Orientation,true,false);
            else
            {
                std::stringstream msgstr;
                SendAreaTriggerMessage(objmgr.GetMangosString(LANG_LEVEL_MINREQUIRED_AND_ITEM,GetSessionLocaleIndex()),(uint32)at->requiredLevel,pProto->Name1);
            }
        }
        else
        {
            if(GetPlayer()->getLevel() >= at->requiredLevel || sWorld.getConfig(CONFIG_INSTANCE_IGNORE_LEVEL) || GetPlayer()->isGameMaster())
                GetPlayer()->TeleportTo(at->target_mapId,at->target_X,at->target_Y,at->target_Z,at->target_Orientation,true,false);
            else
            {
                SendAreaTriggerMessage(objmgr.GetMangosString(LANG_LEVEL_MINREQUIRED,GetSessionLocaleIndex()),(uint32)at->requiredLevel );
            }
        }
    }
}

void WorldSession::HandleUpdateAccountData(WorldPacket &/*recv_data*/)
{
    sLog.outDetail("WORLD: Received CMSG_UPDATE_ACCOUNT_DATA");
    //recv_data.hexlike();
}

void WorldSession::HandleRequestAccountData(WorldPacket& /*recv_data*/)
{
    sLog.outDetail("WORLD: Received CMSG_REQUEST_ACCOUNT_DATA");
    //recv_data.hexlike();
}

void WorldSession::HandleSetActionButtonOpcode(WorldPacket& recv_data)
{
    CHECK_PACKET_SIZE(recv_data,1+2+1+1);

    sLog.outDebug(  "WORLD: Received CMSG_SET_ACTION_BUTTON" );
    uint8 button, misc, type;
    uint16 action;
    recv_data >> button >> action >> misc >> type;
    sLog.outDetail( "BUTTON: %u ACTION: %u TYPE: %u MISC: %u", button, action, type, misc );
    if(action==0)
    {
        sLog.outDetail( "MISC: Remove action from button %u", button );

        GetPlayer()->removeActionButton(button);
    }
    else
    {
        if(type==ACTION_BUTTON_MACRO || type==ACTION_BUTTON_CMACRO)
        {
            sLog.outDetail( "MISC: Added Macro %u into button %u", action, button );
            GetPlayer()->addActionButton(button,action,type,misc);
        }
        else if(type==ACTION_BUTTON_SPELL)
        {
            sLog.outDetail( "MISC: Added Action %u into button %u", action, button );
            GetPlayer()->addActionButton(button,action,type,misc);
        }
        else if(type==ACTION_BUTTON_ITEM)
        {
            sLog.outDetail( "MISC: Added Item %u into button %u", action, button );
            GetPlayer()->addActionButton(button,action,type,misc);
        }
        else
            sLog.outError( "MISC: Unknown action button type %u for action %u into button %u", type, action, button );
    }
}

void WorldSession::HandleCompleteCinema( WorldPacket & /*recv_data*/ )
{
    DEBUG_LOG( "WORLD: Player is watching cinema" );
}

void WorldSession::HandleNextCinematicCamera( WorldPacket & /*recv_data*/ )
{
    DEBUG_LOG( "WORLD: Which movie to play" );
}

void WorldSession::HandleMoveTimeSkippedOpcode( WorldPacket & /*recv_data*/ )
{
    /*  WorldSession::Update( getMSTime() );*/
    DEBUG_LOG( "WORLD: Time Lag/Synchronization Resent/Update" );

    /*
        CHECK_PACKET_SIZE(recv_data,8+4);
        uint64 guid;
        uint32 time_skipped;
        recv_data >> guid;
        recv_data >> time_skipped;
        sLog.outDebug( "WORLD: CMSG_MOVE_TIME_SKIPPED" );

        /// TODO
        must be need use in mangos
        We substract server Lags to move time ( AntiLags )
        for exmaple
        GetPlayer()->ModifyLastMoveTime( -int32(time_skipped) );
    */
}

void WorldSession::HandleFeatherFallAck(WorldPacket &/*recv_data*/)
{
    DEBUG_LOG("WORLD: CMSG_MOVE_FEATHER_FALL_ACK");
}

void WorldSession::HandleMoveUnRootAck(WorldPacket&/* recv_data*/)
{
    /*
        CHECK_PACKET_SIZE(recv_data,8+8+4+4+4+4+4);

        sLog.outDebug( "WORLD: CMSG_FORCE_MOVE_UNROOT_ACK" );
        recv_data.hexlike();
        uint64 guid;
        uint64 unknown1;
        uint32 unknown2;
        float PositionX;
        float PositionY;
        float PositionZ;
        float Orientation;

        recv_data >> guid;
        recv_data >> unknown1;
        recv_data >> unknown2;
        recv_data >> PositionX;
        recv_data >> PositionY;
        recv_data >> PositionZ;
        recv_data >> Orientation;

        // TODO for later may be we can use for anticheat
        DEBUG_LOG("Guid " I64FMTD,guid);
        DEBUG_LOG("unknown1 " I64FMTD,unknown1);
        DEBUG_LOG("unknown2 %u",unknown2);
        DEBUG_LOG("X %f",PositionX);
        DEBUG_LOG("Y %f",PositionY);
        DEBUG_LOG("Z %f",PositionZ);
        DEBUG_LOG("O %f",Orientation);
    */
}

void WorldSession::HandleMoveRootAck(WorldPacket&/* recv_data*/)
{
    /*
        CHECK_PACKET_SIZE(recv_data,8+8+4+4+4+4+4);

        sLog.outDebug( "WORLD: CMSG_FORCE_MOVE_ROOT_ACK" );
        recv_data.hexlike();
        uint64 guid;
        uint64 unknown1;
        uint32 unknown2;
        float PositionX;
        float PositionY;
        float PositionZ;
        float Orientation;

        recv_data >> guid;
        recv_data >> unknown1;
        recv_data >> unknown2;
        recv_data >> PositionX;
        recv_data >> PositionY;
        recv_data >> PositionZ;
        recv_data >> Orientation;

        // for later may be we can use for anticheat
        DEBUG_LOG("Guid " I64FMTD,guid);
        DEBUG_LOG("unknown1 " I64FMTD,unknown1);
        DEBUG_LOG("unknown1 %u",unknown2);
        DEBUG_LOG("X %f",PositionX);
        DEBUG_LOG("Y %f",PositionY);
        DEBUG_LOG("Z %f",PositionZ);
        DEBUG_LOG("O %f",Orientation);
    */
}

void WorldSession::HandleMoveTeleportAck(WorldPacket&/* recv_data*/)
{
    /*
        CHECK_PACKET_SIZE(recv_data,8+4);

        sLog.outDebug("MSG_MOVE_TELEPORT_ACK");
        uint64 guid;
        uint32 flags, time;

        recv_data >> guid;
        recv_data >> flags >> time;
        DEBUG_LOG("Guid " I64FMTD,guid);
        DEBUG_LOG("Flags %u, time %u",flags, time/1000);
    */
}

void WorldSession::HandleSetActionBar(WorldPacket& recv_data)
{
    CHECK_PACKET_SIZE(recv_data,1);

    uint8 ActionBar;
    uint32 temp;

    recv_data >> ActionBar;

    temp = ((GetPlayer()->GetUInt32Value( PLAYER_FIELD_BYTES )) & 0xFFF0FFFF) + (ActionBar << 16);
    GetPlayer()->SetUInt32Value( PLAYER_FIELD_BYTES, temp);
}

void WorldSession::HandleWardenDataOpcode(WorldPacket& /*recv_data*/)
{
    /*
        CHECK_PACKET_SIZE(recv_data,1);

        uint8 tmp;
        recv_data >> tmp;
        sLog.outDebug("Received opcode CMSG_WARDEN_DATA, not resolve.uint8 = %u",tmp);
    */
}

void WorldSession::HandlePlayedTime(WorldPacket& /*recv_data*/)
{
    uint32 TotalTimePlayed = GetPlayer()->GetTotalPlayedTime();
    uint32 LevelPlayedTime = GetPlayer()->GetLevelPlayedTime();

    WorldPacket data(SMSG_PLAYED_TIME, 8);
    data << TotalTimePlayed;
    data << LevelPlayedTime;
    SendPacket(&data);
}

void WorldSession::HandleInspectOpcode(WorldPacket& recv_data)
{
    CHECK_PACKET_SIZE(recv_data, 8);

    uint64 guid;
    recv_data >> guid;
    DEBUG_LOG("Inspected guid is " I64FMTD, guid);

    if( _player != 0 )
        _player->SetSelection(guid);

    Player *plr = objmgr.GetPlayer(guid);
    if(!plr)                                                // wrong player
        return;

    uint32 talent_points = 0x3D;
    WorldPacket data(SMSG_INSPECT_TALENTS, 4+talent_points);
    data << uint32(talent_points);

    // fill by 0 talents array
    for(uint32 i = 0; i < talent_points; ++i)
        data << uint8(0);

    if(sWorld.getConfig(CONFIG_TALENTS_INSPECTING) || _player->isGameMaster())
    {
        // find class talent tabs (all players have 3 talent tabs)
        uint32 const* talentTabIds = GetTalentTabPages(plr->getClass());

        uint32 talentTabPos = 0;                                // pos of first talent rank in tab including all prev tabs
        for(uint32 i = 0; i < 3; ++i)
        {
            uint32 talentTabId = talentTabIds[i];

            // fill by real data
            for(uint32 talentId = 0; talentId < sTalentStore.GetNumRows(); ++talentId)
            {
                TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentId);
                if(!talentInfo)
                    continue;

                // skip another tab talents
                if(talentInfo->TalentTab != talentTabId)
                    continue;

                // find talent rank
                uint32 curtalent_maxrank = 0;
                for(uint32 k = 5; k > 0; --k)
                {
                    if(talentInfo->RankID[k-1] && plr->HasSpell(talentInfo->RankID[k-1]))
                    {
                        curtalent_maxrank = k;
                        break;
                    }
                }

                // not learned talent
                if(!curtalent_maxrank)
                    continue;

                // 1 rank talent bit index
                uint32 curtalent_index = talentTabPos + GetTalentInspectBitPosInTab(talentId);

                uint32 curtalent_rank_index = curtalent_index+curtalent_maxrank-1;

                // slot/offset in 7-bit bytes
                uint32 curtalent_rank_slot7   = curtalent_rank_index / 7;
                uint32 curtalent_rank_offset7 = curtalent_rank_index % 7;

                // rank pos with skipped 8 bit
                uint32 curtalent_rank_index2 = curtalent_rank_slot7 * 8 + curtalent_rank_offset7;

                // slot/offset in 8-bit bytes with skipped high bit
                uint32 curtalent_rank_slot = curtalent_rank_index2 / 8;
                uint32 curtalent_rank_offset =  curtalent_rank_index2 % 8;

                // apply mask
                uint32 val = data.read<uint8>(4 + curtalent_rank_slot);
                val |= (1 << curtalent_rank_offset);
                data.put<uint8>(4 + curtalent_rank_slot,val & 0xFF);
            }

            talentTabPos += GetTalentTabInspectBitSize(talentTabId);
        }
    }

    SendPacket(&data);
}

void WorldSession::HandleInspectHonorStatsOpcode(WorldPacket& recv_data)
{
    CHECK_PACKET_SIZE(recv_data, 8);

    uint64 guid;
    recv_data >> guid;

    Player *player = objmgr.GetPlayer(guid);

    if(!player)
    {
        sLog.outError("InspectHonorStats: WTF, player not found...");
        return;
    }

    WorldPacket data(MSG_INSPECT_HONOR_STATS, 8+1+4*4);
    data << uint64(player->GetGUID());
    data << uint8(player->GetUInt32Value(PLAYER_FIELD_HONOR_CURRENCY));
    data << uint32(player->GetUInt32Value(PLAYER_FIELD_KILLS));
    data << uint32(player->GetUInt32Value(PLAYER_FIELD_TODAY_CONTRIBUTION));
    data << uint32(player->GetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION));
    data << uint32(player->GetUInt32Value(PLAYER_FIELD_LIFETIME_HONORBALE_KILLS));
    SendPacket(&data);
}

void WorldSession::HandleWorldTeleportOpcode(WorldPacket& recv_data)
{
    CHECK_PACKET_SIZE(recv_data,4+4+4+4+4+4);

    // write in client console: worldport 469 452 6454 2536 180 or /console worldport 469 452 6454 2536 180
    // Received opcode CMSG_WORLD_TELEPORT
    // Time is ***, map=469, x=452.000000, y=6454.000000, z=2536.000000, orient=3.141593

    //sLog.outDebug("Received opcode CMSG_WORLD_TELEPORT");

    if(GetPlayer()->isInFlight())
    {
        sLog.outDebug("Player '%s' in flight, ignore worldport command.",GetPlayer()->GetName());
        return;
    }

    uint32 time;
    uint32 mapid;
    float PositionX;
    float PositionY;
    float PositionZ;
    float Orientation;

    recv_data >> time;                                      // time in m.sec.
    recv_data >> mapid;
    recv_data >> PositionX;
    recv_data >> PositionY;
    recv_data >> PositionZ;
    recv_data >> Orientation;                               // o (3.141593 = 180 degrees)
    DEBUG_LOG("Time %u sec, map=%u, x=%f, y=%f, z=%f, orient=%f", time/1000, mapid, PositionX, PositionY, PositionZ, Orientation);

    if (GetSecurity() >= SEC_ADMINISTRATOR)
        GetPlayer()->TeleportTo(mapid,PositionX,PositionY,PositionZ,Orientation);
    else
        SendNotification("You do not have permission to perform that function");
    sLog.outDebug("Received worldport command from player %s", GetPlayer()->GetName());
}

void WorldSession::HandleWhoisOpcode(WorldPacket& recv_data)
{
    CHECK_PACKET_SIZE(recv_data, 1);

    sLog.outDebug("Received opcode CMSG_WHOIS");
    std::string charname, acc, email, lastip, msg;
    recv_data >> charname;

    if (GetSecurity() < SEC_ADMINISTRATOR)
    {
        SendNotification("You do not have permission to perform that function");
        return;
    }

    if(charname.empty())
    {
        SendNotification("Please provide character name");
        return;
    }

    uint32 accid;
    Field *fields;

    Player *plr = objmgr.GetPlayer(charname.c_str());

    if(plr)
        accid = plr->GetSession()->GetAccountId();
    else
    {
        SendNotification("Player %s not found or offline", charname.c_str());
        return;
    }

    if(!accid)
    {
        SendNotification("Account for character %s not found", charname.c_str());
        return;
    }

    QueryResult *result = loginDatabase.PQuery("SELECT `username`,`email`,`last_ip` FROM `account` WHERE `id`=%u", accid);
    if(result)
    {
        fields = result->Fetch();
        acc = fields[0].GetCppString();
        if(acc.empty())
            acc = "Unknown";
        email = fields[1].GetCppString();
        if(email.empty())
            email = "Unknown";
        lastip = fields[2].GetCppString();
        if(lastip.empty())
            lastip = "Unknown";
        msg = charname + "'s " + "account is " + acc + ", e-mail: " + email + ", last ip: " + lastip;

        WorldPacket data(SMSG_WHOIS, msg.size()+1);
        data << msg;
        _player->GetSession()->SendPacket(&data);
    }
    else
        SendNotification("Account for character %s not found", charname.c_str());

    delete result;
    sLog.outDebug("Received whois command from player %s for character %s", GetPlayer()->GetName(), charname.c_str());
}

void WorldSession::HandleReportSpamOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data, 1+8);
    sLog.outDebug("WORLD: CMSG_REPORT_SPAM");
    recv_data.hexlike();

    uint8 spam_type;                                        // 0 - mail, 1 - chat
    uint64 spammer_guid;
    uint32 unk1, unk2, unk3, unk4 = 0;
    std::string description = "";
    recv_data >> spam_type;                                 // unk 0x01 const, may be spam type (mail/chat)
    recv_data >> spammer_guid;                              // player guid
    switch(spam_type)
    {
        case 0:
            CHECK_PACKET_SIZE(recv_data, recv_data.rpos()+4+4+4);
            recv_data >> unk1;                              // unk
            recv_data >> unk2;                              // probably mail id
            recv_data >> unk3;                              // unk
            break;
        case 1:
            CHECK_PACKET_SIZE(recv_data, recv_data.rpos()+4+4+4+4+1);
            recv_data >> unk1;                              // probably language
            recv_data >> unk2;                              // message type?
            recv_data >> unk3;                              // probably channel id
            recv_data >> unk4;                              // unk random value
            recv_data >> description;                       // spam description string (messagetype, channel name, player name, message)
            break;
    }

    // NOTE: all chat messages from this spammer automatically ignored by spam reporter until logout in case chat spam.
    // if it's mail spam - ALL mails from this spammer automatically removed by client

    // Complaint Received message
    WorldPacket data(SMSG_REPORT_SPAM_RESULT, 1);
    data << uint8(0);
    SendPacket(&data);

    sLog.outDebug("REPORT SPAM: type %u, guid %u, unk1 %u, unk2 %u, unk3 %u, unk4 %u, message %s", spam_type, GUID_LOPART(spammer_guid), unk1, unk2, unk3, unk4, description.c_str());
}

void WorldSession::HandleRealmStateRequestOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data, 4);

    sLog.outDebug("CMSG_REALM_SPLIT_INFO_REQUEST");

    uint32 unk;
    std::string split_date = "01/01/01";
    recv_data >> unk;

    WorldPacket data(SMSG_REALM_SPLIT_INFO_RESPONSE, 4+4+split_date.size()+1);
    data << unk;
    data << uint32(0x00000000);                             // realm split state
    // split states:
    // 0x0 realm normal
    // 0x1 realm split
    // 0x2 realm split pending
    data << split_date;
    SendPacket(&data);
    //sLog.outDebug("response sent %u", unk);
}

void WorldSession::HandleFarSightOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data, 1);

    sLog.outDebug("WORLD: CMSG_FAR_SIGHT");
    //recv_data.hexlike();

    uint8 unk;
    recv_data >> unk;

    switch(unk)
    {
        case 0:
            //WorldPacket data(SMSG_CLEAR_FAR_SIGHT_IMMEDIATE, 0)
            //SendPacket(&data);
            //_player->SetUInt64Value(PLAYER_FARSIGHT, 0);
            sLog.outDebug("Removed FarSight from player %u", _player->GetGUIDLow());
            break;
        case 1:
            sLog.outDebug("Added FarSight " I64FMTD " to player %u", _player->GetUInt64Value(PLAYER_FARSIGHT), _player->GetGUIDLow());
            break;
    }
}

void WorldSession::HandleChooseTitleOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data, 4);

    sLog.outDebug("CMSG_CHOOSE_TITLE");

    uint32 title;
    recv_data >> title;

    uint32 available = GetPlayer()->GetUInt32Value(PLAYER__FIELD_KNOWN_TITLES);
    if(!(available & (1<<title)))
        return;

    GetPlayer()->SetUInt32Value(PLAYER_CHOSEN_TITLE, title);
    /*
    PLAYER_FIELD_KNOWN_TITLES:
    1 << title# (0=none)
    */
}

void WorldSession::HandleAllowMoveAckOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data, 4+4);

    sLog.outDebug("CMSG_ALLOW_MOVE_ACK");

    uint32 counter, time_;
    recv_data >> counter >> time_;

    // time_ seems always more than getMSTime()
    uint32 diff = time_ - getMSTime();

    sLog.outDebug("response sent: counter %u, time %u (HEX: %X), ms. time %u, diff %u", counter, time_, time_, getMSTime(), diff);
}

void WorldSession::HandleResetInstancesOpcode( WorldPacket & /*recv_data*/ )
{
    sLog.outDebug("WORLD: CMSG_RESET_INSTANCES");
    /*
        uint32 mapid = 0;
        WorldPacket data(SMSG_RESET_INSTANCES_SUCCESS, 4);
        data << mapid;
        _player->GetSession()->SendPacket(&data);
    */
}

void WorldSession::HandleDungeonDifficultyOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data, 4);

    sLog.outDebug("MSG_SET_DUNGEON_DIFFICULTY");

    uint32 difficulty;
    recv_data >> difficulty;

    GetPlayer()->SetDungeonDifficulty(difficulty);
    GetPlayer()->SendDungeonDifficulty();
}

void WorldSession::HandleNewUnknownOpcode( WorldPacket & recv_data )
{
    sLog.outDebug("New Unknown Opcode %u", recv_data.GetOpcode());
    recv_data.hexlike();
    /*
    New Unknown Opcode 837
    STORAGE_SIZE: 60
    02 00 00 00 00 00 00 00 | 00 00 00 00 01 20 00 00
    89 EB 33 01 71 5C 24 C4 | 15 03 35 45 74 47 8B 42
    BA B8 1B 40 00 00 00 00 | 00 00 00 00 77 66 42 BF
    23 91 26 3F 00 00 60 41 | 00 00 00 00

    New Unknown Opcode 837
    STORAGE_SIZE: 44
    02 00 00 00 00 00 00 00 | 00 00 00 00 00 00 80 00
    7B 80 34 01 84 EA 2B C4 | 5F A1 36 45 C9 39 1C 42
    BA B8 1B 40 CE 06 00 00 | 00 00 80 3F
    */
}

void WorldSession::HandleDismountOpcode( WorldPacket & /*recv_data*/ )
{
    sLog.outDebug("WORLD: CMSG_DISMOUNT");
    //recv_data.hexlike();

    //If player is not mounted, so go out :)
    if (!_player->IsMounted())                              // not blizz like; no any messages on blizz
    {
        ChatHandler(this).SendSysMessage(LANG_CHAR_NON_MOUNTED);
        return;
    }

    if(_player->isInFlight())                               // not blizz like; no any messages on blizz
    {
        ChatHandler(this).SendSysMessage(LANG_YOU_IN_FLIGHT);
        return;
    }

    _player->Unmount();
    _player->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);
}

void WorldSession::HandleMoveFlyModeChangeAckOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data, 8+4+4);

    // fly mode on/off
    sLog.outDebug("WORLD: CMSG_MOVE_FLY_MODE_CHANGE_ACK");
    //recv_data.hexlike();

    uint64 guid;
    uint32 unk;
    uint32 flags;

    recv_data >> guid >> unk >> flags;

    _player->m_movementInfo.SetMovementFlags(flags);
    /*
    on:
    25 00 00 00 00 00 00 00 | 00 00 00 00 00 00 80 00
    85 4E A9 01 19 BA 7A C3 | 42 0D 70 44 44 B0 A8 42
    78 15 94 40 39 03 00 00 | 00 00 80 3F
    off:
    25 00 00 00 00 00 00 00 | 00 00 00 00 00 00 00 00
    10 FD A9 01 19 BA 7A C3 | 42 0D 70 44 44 B0 A8 42
    78 15 94 40 39 03 00 00 | 00 00 00 00
    */
}

void WorldSession::HandleRequestPetInfoOpcode( WorldPacket & /*recv_data */)
{
    /*
        sLog.outDebug("WORLD: CMSG_REQUEST_PET_INFO");
        recv_data.hexlike();
    */
}

void WorldSession::HandleSetTaxiBenchmarkOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data, 1);
    
    uint8 mode;
    recv_data >> mode;

    sLog.outDebug("Client use \"/timetest %d\" command", mode);
}
