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

#include "Object.h"
#include "Player.h"
#include "BattleGround.h"
#include "BattleGroundBE.h"
#include "Creature.h"
#include "Chat.h"
#include "ObjectMgr.h"
#include "MapManager.h"
#include "Language.h"

BattleGroundBE::BattleGroundBE()
{
    m_bgobjects.resize(BG_BE_OBJECT_MAX);
}

BattleGroundBE::~BattleGroundBE()
{

}

void BattleGroundBE::Update(time_t diff)
{
    BattleGround::Update(diff);

    // after bg start we get there
    if(GetStatus() == STATUS_WAIT_JOIN && !isDoorsSpawned() && GetPlayersSize() >= 1)
    {
        for(uint32 i = 0; i <= BG_BE_OBJECT_DOOR_4; i++)
        {
            // respawn
            MapManager::Instance().GetMap(m_bgobjects[i].object->GetMapId(), m_bgobjects[i].object)->Add(m_bgobjects[i].object);
        }
        sLog.outDebug("Doors spawned...");

        SetDoorsSpawned(true);
        SetStartDelayTime(START_DELAY);

        WorldPacket data;
        const char *message = LANG_ARENA_ONE_MINUTE;
        sChatHandler.FillMessageData(&data, NULL, CHAT_MSG_BG_SYSTEM_NEUTRAL, LANG_UNIVERSAL, NULL, 0, message, NULL);
        SendPacketToAll(&data);
    }

    // after bg start and doors spawn we get there
    if(GetStatus() == STATUS_WAIT_JOIN && isDoorsSpawned())
    {
        ModifyStartDelayTime(diff);

        // delay expired (1 minute)
        if(GetStartDelayTime() < 0)
        {
            for(uint32 i = BG_BE_OBJECT_DOOR_1; i <= BG_BE_OBJECT_DOOR_2; i++)
            {
                // despawn
                MapManager::Instance().GetMap(m_bgobjects[i].object->GetMapId(), m_bgobjects[i].object)->Remove(m_bgobjects[i].object, false);
            }
            sLog.outDebug("Doors despawned...");

            WorldPacket data;
            const char *message = LANG_ARENA_BEGUN;
            sChatHandler.FillMessageData(&data, NULL, CHAT_MSG_BG_SYSTEM_NEUTRAL, LANG_UNIVERSAL, NULL, 0, message, NULL);
            SendPacketToAll(&data);

            SetStatus(STATUS_IN_PROGRESS);

            SetDoorsSpawned(false);
            SetStartDelayTime(0);

            for(std::map<uint64, BattleGroundPlayer>::const_iterator itr = GetPlayers()->begin(); itr != GetPlayers()->end(); ++itr)
            {
                Player *plr = objmgr.GetPlayer(itr->first);
                if(plr)
                    plr->RemoveAurasDueToSpell(SPELL_ARENA_PREPARATION);
            }
        }
    }

    if(GetStatus() == STATUS_IN_PROGRESS)
    {
        // update something
    }
}

void BattleGroundBE::RemovePlayer(Player *plr, uint64 guid)
{

}

void BattleGroundBE::HandleKillPlayer(Player *player, Player *killer)
{
    if(GetStatus() != STATUS_IN_PROGRESS)
        return;

    if(!killer)
    {
        sLog.outError("Killer player not found");
        return;
    }

    uint32 killer_team_index = GetTeamIndexByTeamId(killer->GetTeam());

    m_TeamKills[killer_team_index]++;              // add kills to killer's team

    if(m_TeamKills[killer_team_index] >= GetPlayersCountByTeam(player->GetTeam()))
    {
        // all opponents killed
        EndBattleGround(killer->GetTeam());
    }
}

void BattleGroundBE::HandleAreaTrigger(Player *Source, uint32 Trigger)
{
    // this is wrong way to implement these things. On official it done by gameobject spell cast.
    if(GetStatus() != STATUS_IN_PROGRESS)
        return;

    uint32 SpellId = 0;
    switch(Trigger)
    {
        case 4538:                                          // buff trigger?
        case 4539:                                          // buff trigger?
            break;
        default:
            sLog.outError("WARNING: Unhandled AreaTrigger in Battleground: %u", Trigger);
            Source->GetSession()->SendAreaTriggerMessage("Warning: Unhandled AreaTrigger in Battleground: %u", Trigger);
            break;
    }

    if(SpellId)
        Source->CastSpell(Source, SpellId, true);
}

bool BattleGroundBE::SetupBattleGround()
{
    // gates
    if(!SpawnObject(183971, BG_BE_OBJECT_DOOR_1, 6287.277, 282.1877, 3.810925, -2.260201, 0, 0, 0.9044551, -0.4265689))
        return false;
    if(!SpawnObject(183973, BG_BE_OBJECT_DOOR_2, 6189.546, 241.7099, 3.101481, 0.8813917, 0, 0, 0.4265689, 0.9044551))
        return false;
    if(!SpawnObject(183970, BG_BE_OBJECT_DOOR_3, 6299.116, 296.5494, 3.308032, 0.8813917, 0, 0, 0.4265689, 0.9044551))
        return false;
    if(!SpawnObject(183972, BG_BE_OBJECT_DOOR_4, 6177.708, 227.3481, 3.604374, -2.260201, 0, 0, 0.9044551, -0.4265689))
        return false;
    // buffs
    if(!SpawnObject(184663, BG_BE_OBJECT_BUFF_1, 6249.042, 275.3239, 11.22033, -1.448624, 0, 0, 0.6626201, -0.7489557, 34709))
        return false;
    if(!SpawnObject(184664, BG_BE_OBJECT_BUFF_2, 6228.26, 249.566, 11.21812, -0.06981307, 0, 0, 0.03489945, -0.9993908, 34709))
        return false;

    return true;
}
/*
21:45:46 id:231310 [S2C] SMSG_INIT_WORLD_STATES (706 = 0x02C2) len: 86
0000: 32 02 00 00 76 0e 00 00 00 00 00 00 09 00 f3 09  |  2...v...........
0010: 00 00 01 00 00 00 f1 09 00 00 01 00 00 00 f0 09  |  ................
0020: 00 00 02 00 00 00 d4 08 00 00 00 00 00 00 d8 08  |  ................
0030: 00 00 00 00 00 00 d7 08 00 00 00 00 00 00 d6 08  |  ................
0040: 00 00 00 00 00 00 d5 08 00 00 00 00 00 00 d3 08  |  ................
0050: 00 00 00 00 00 00                                |  ......
*/
