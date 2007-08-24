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

#include "WorldSession.h"
#include "Log.h"
#include "Database/DatabaseEnv.h"
#include "Player.h"
#include "WorldPacket.h"
#include "ObjectMgr.h"

static void AttemptJoin(Player* _player)
{
    // skip not can autojoin cases and player group case
    if(!_player->m_lookingForGroup.canAutoJoin() || _player->groupInfo.group )
        return;

    ObjectAccessor::PlayersMapType const& players = ObjectAccessor::Instance().GetPlayers();

    for(ObjectAccessor::PlayersMapType::const_iterator iter = players.begin(); iter != players.end(); ++iter)
    {
        uint64 guid = iter->first;
        Player *plr = iter->second;

        // skip enemies and self
        if(!plr || plr==_player || plr->GetTeam() != _player->GetTeam())
            continue;

        // skip not auto add, not group leader cases
        if(!plr->GetSession()->LookingForGroup_auto_add || plr->groupInfo.group && plr->groupInfo.group->GetLeaderGUID()!=plr->GetGUID())
            continue;

        // skip non auto-join or empty slots, or non compatible slots
        if(!plr->m_lookingForGroup.more.canAutoJoin() || !_player->m_lookingForGroup.HaveInSlot(plr->m_lookingForGroup.more))
            continue;

        // attempt create group, or skip
        if(!plr->groupInfo.group)
        {
            Group* group = new Group;
            if(!group->Create(plr->GetGUID(), plr->GetName()))
            {
                delete group;
                continue;
            }

            objmgr.AddGroup(group);
        }

        // stop at success join
        if(plr->groupInfo.group->AddMember(_player->GetGUID(), _player->GetName()))
            break;
    }
}

static void AttemptAddMore(Player* _player)
{
    // skip not group leader case
    if(_player->groupInfo.group && _player->groupInfo.group->GetLeaderGUID()!=_player->GetGUID())
        return;

    if(!_player->m_lookingForGroup.more.canAutoJoin())
        return;

    ObjectAccessor::PlayersMapType const& players = ObjectAccessor::Instance().GetPlayers();

    for(ObjectAccessor::PlayersMapType::const_iterator iter = players.begin(); iter != players.end(); ++iter)
    {
        uint64 guid = iter->first;
        Player *plr = iter->second;

        // skip enemies and self
        if(!plr || plr==_player || plr->GetTeam() != _player->GetTeam())
            continue;

        // skip not auto join or in group
        if(!plr->GetSession()->LookingForGroup_auto_join || plr->groupInfo.group )
            continue;

        if(!plr->m_lookingForGroup.HaveInSlot(_player->m_lookingForGroup.more))
            continue;

        // attempt create group if need, or stop attempts
        if(!_player->groupInfo.group)
        {
            Group* group = new Group;
            if(!group->Create(_player->GetGUID(), _player->GetName()))
            {
                delete group;
                return;                                     // cann't create group (??)
            }

            objmgr.AddGroup(group);
        }

        // stop at join fail (full)
        if(!_player->groupInfo.group->AddMember(plr->GetGUID(), plr->GetName()))
            break;
    }
}

void WorldSession::HandleLfgAutoJoinOpcode( WorldPacket & recv_data )
{
    sLog.outDebug("CMSG_SET_LFG_AUTO_JOIN");
    LookingForGroup_auto_join = true;

    if(!_player)
        return;

    AttemptJoin(_player);
}

void WorldSession::HandleLfgCancelAutoJoinOpcode( WorldPacket & recv_data )
{
    sLog.outDebug("CMSG_UNSET_LFG_AUTO_JOIN");
    LookingForGroup_auto_join = false;
}

void WorldSession::HandleLfmAutoAddMembersOpcode( WorldPacket & recv_data )
{
    sLog.outDebug("CMSG_SET_LFM_AUTOADD");
    LookingForGroup_auto_add = true;

    if(!_player)
        return;

    AttemptAddMore(_player);
}

void WorldSession::HandleLfmCancelAutoAddmembersOpcode( WorldPacket & recv_data )
{
    sLog.outDebug("CMSG_UNSET_LFM_AUTOADD");
    LookingForGroup_auto_add = false;
}

void WorldSession::HandleLfgClearOpcode( WorldPacket & recv_data )
{
    sLog.outDebug("CMSG_LOOKING_FOR_GROUP_CLEAR");

    for(int i = 0; i < MAX_LOOKING_FOR_GROUP_SLOT; ++i)
        _player->m_lookingForGroup.slots[i].Clear();
}

void WorldSession::HandleLfmSetNoneOpcode( WorldPacket & recv_data )
{
    sLog.outDebug("CMSG_SET_LOOKING_FOR_NONE");

    _player->m_lookingForGroup.more.Clear();
}

void WorldSession::HandleLfmSetOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,4);

    sLog.outDebug("CMSG_SET_LOOKING_FOR_MORE");
    //recv_data.hexlike();
    uint32 temp, entry, type;

    recv_data >> temp;

    entry = ( temp & 0xFFFF);
    type = ( (temp >> 24) & 0xFFFF);

    _player->m_lookingForGroup.more.Set(entry,type);
    sLog.outDebug("LFM set: temp %u, zone %u, type %u", temp, entry, type);

    if(LookingForGroup_auto_add)
        AttemptAddMore(_player);

    SendLfgResult(type, entry, 1);
}

void WorldSession::HandleLfgSetCommentOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,1);

    sLog.outDebug("CMSG_SET_COMMENTARY");
    //recv_data.hexlike();

    std::string comment;
    recv_data >> comment;
    sLog.outDebug("LFG comment %s", comment.c_str());

    _player->m_lookingForGroup.comment = comment;
}

void WorldSession::HandleLookingForGroup(WorldPacket& recv_data)
{
    CHECK_PACKET_SIZE(recv_data,4+4+4);

    sLog.outDebug("MSG_LOOKING_FOR_GROUP");
    //recv_data.hexlike();
    uint32 type, entry, unk;

    recv_data >> type >> entry >> unk;
    sLog.outDebug("MSG_LOOKING_FOR_GROUP: type %u, entry %u, unk %u", type, entry, unk);

    if(LookingForGroup_auto_add)
        AttemptAddMore(_player);

    if(LookingForGroup_auto_join)
        AttemptJoin(_player);

    SendLfgResult(type, entry, 0);
}

void WorldSession::SendLfgResult(uint32 type, uint32 entry, uint8 lfg_type)
{
    uint32 number = 0;

    // start preper packet;
    WorldPacket data(MSG_LOOKING_FOR_GROUP);
    data << uint32(type);                                   // type
    data << uint32(entry);                                  // entry from LFGDungeons.dbc
    data << uint32(0);                                      // count, placeholder
    data << uint32(0);                                      // count again, strange, placeholder

    ObjectAccessor::PlayersMapType const& players = ObjectAccessor::Instance().GetPlayers();

    for(ObjectAccessor::PlayersMapType::const_iterator iter = players.begin(); iter != players.end(); ++iter)
    {
        uint64 guid = iter->first;
        Player *plr = iter->second;

        if(!plr || plr->GetTeam() != _player->GetTeam())
            continue;

        if(!plr->m_lookingForGroup.HaveInSlot(entry,type))
            continue;

        ++number;

        data.append(plr->GetPackGUID());                    // packed guid
        data << plr->getLevel();                            // level
        data << plr->GetZoneId();                           // current zone
        data << lfg_type;                                   // 0x00 - LFG, 0x01 - LFM

        for(uint8 j = 0; j < MAX_LOOKING_FOR_GROUP_SLOT; ++j)
        {
            data << uint32( plr->m_lookingForGroup.slots[j].entry | (plr->m_lookingForGroup.slots[j].type << 24) );
        }
        data << plr->m_lookingForGroup.comment;

        Group *group = plr->groupInfo.group;
        if(group)
        {
            data << group->GetMembersCount()-1;             // count of group members without group leader
            Group::MemberList const& members = group->GetMembers();
            for(Group::member_citerator itr = members.begin(); itr != members.end(); ++itr)
            {
                Player *member = objmgr.GetPlayer(itr->guid);
                if(member && member->GetGUID() != plr->GetGUID())
                {
                    data.append(member->GetPackGUID());     // packed guid
                    data << member->getLevel();             // player level
                }
            }
        }
        else
        {
            data << uint32(0x00);
        }
    }

    // fill count placeholders
    data.put<uint32>(4+4,  number);
    data.put<uint32>(4+4+4,number);

    SendPacket(&data);
}

void WorldSession::HandleSetLfgOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,4+4);

    sLog.outDebug("CMSG_SET_LOOKING_FOR_GROUP");
    //recv_data.hexlike();
    uint32 slot, temp, entry, type;

    recv_data >> slot >> temp;

    entry = ( temp & 0xFFFF);
    type = ( (temp >> 24) & 0xFFFF);

    if(slot >= MAX_LOOKING_FOR_GROUP_SLOT)
        return;

    _player->m_lookingForGroup.slots[slot].Set(entry,type);
    sLog.outDebug("LFG set: looknumber %u, temp %X, type %u, entry %u", slot, temp, type, entry);

    if(LookingForGroup_auto_join)
        AttemptJoin(_player);

    SendLfgResult(type, entry, 0);
}
