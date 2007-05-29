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
#include "BattlegroundAV.h"
#include "Creature.h"
#include "Chat.h"
#include "Spell.h"
#include "ObjectMgr.h"
#include "MapManager.h"
#include "Language.h"

BattleGroundAV::BattleGroundAV()
{

}

BattleGroundAV::~BattleGroundAV()
{

}

void BattleGroundAV::Update(time_t diff)
{
    BattleGround::Update(diff);
}

void BattleGroundAV::RemovePlayer(Player *plr,uint64 guid)
{

}

void BattleGroundAV::HandleAreaTrigger(Player* Source, uint32 Trigger)
{
    // this is wrong way to implement these things. On official it done by gameobject spell cast.
    if(GetStatus() != STATUS_INPROGRESS)
        return;

    uint32 SpellId = 0;
    switch(Trigger)
    {
        case 0:
        default:
            sLog.outError("WARNING: Unhandled AreaTrigger in Battleground: %d", Trigger);
            Source->GetSession()->SendAreaTriggerMessage("Warning: Unhandled AreaTrigger in Battleground: %u", Trigger);
            break;
    }

    if(SpellId)
    {
        SpellEntry const *Entry = sSpellStore.LookupEntry(SpellId);

        if(!Entry)
        {
            sLog.outError("ERROR: Tried to add unknown spell id %d to plr.", SpellId);
            return;
        }

        Source->CastSpell(Source, Entry, true, 0);
    }
}

void BattleGroundAV::SetupBattleGround()
{

}
