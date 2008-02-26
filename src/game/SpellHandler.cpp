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
#include "Database/DBCStores.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Log.h"
#include "Opcodes.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "BattleGroundWS.h"
#include "MapManager.h"
#include "ScriptCalls.h"

void WorldSession::HandleUseItemOpcode(WorldPacket& recvPacket)
{
    // TODO: add targets.read() check
    CHECK_PACKET_SIZE(recvPacket,1+1+1+1);

    Player* pUser = _player;
    uint8 bagIndex, slot;
    uint8 spell_count;                                      // number of spells at item, not used
    uint8 cast_count;                                       // next cast if exists (single or not)

    recvPacket >> bagIndex >> slot >> spell_count >> cast_count;

    Item *pItem = pUser->GetItemByPos(bagIndex, slot);
    if(!pItem)
    {
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, NULL, NULL );
        return;
    }

    sLog.outDetail("WORLD: CMSG_USE_ITEM packet, bagIndex: %u, slot: %u, spell_count: %u , cast_count: %u, Item: %u, data length = %i", bagIndex, slot, spell_count, cast_count, pItem->GetEntry(), recvPacket.size());

    ItemPrototype const *proto = pItem->GetProto();
    if(!proto)
    {
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, pItem, NULL );
        return;
    }

    uint8 msg = pUser->CanUseItem(pItem);
    if( msg != EQUIP_ERR_OK )
    {
        pUser->SendEquipError( msg, pItem, NULL );
        return;
    }

    if (pUser->isInCombat())
    {
        for(int i = 0; i < 5; ++i)
        {
            if (IsNonCombatSpell(proto->Spells[i].SpellId))
            {
                pUser->SendEquipError(EQUIP_ERR_NOT_IN_COMBAT,pItem,NULL);
                return;
            }
        }
    }

    // check also  BIND_WHEN_PICKED_UP and BIND_QUEST_ITEM for .additem or .additemset case by GM (not binded at adding to inventory)
    if( pItem->GetProto()->Bonding == BIND_WHEN_USE || pItem->GetProto()->Bonding == BIND_WHEN_PICKED_UP || pItem->GetProto()->Bonding == BIND_QUEST_ITEM )
    {
        if (!pItem->IsSoulBound())
        {
            pItem->SetState(ITEM_CHANGED, pUser);
            pItem->SetBinding( true );
        }
    }

    SpellCastTargets targets;
    if(!targets.read(&recvPacket, pUser))
        return;

    //Note: If script stop casting it must send appropriate data to client to prevent stuck item in gray state.
    if(!Script->ItemUse(pUser,pItem,targets))
    {
        // no script or script not process request by self

        // use triggered flag only for items with many spell casts and for not first cast
        int count = 0;

        for(int i = 0; i < 5; ++i)
        {
            _Spell const& spellData = pItem->GetProto()->Spells[i];

            // no spell
            if(!spellData.SpellId)
                continue;

            // wrong triggering type
            if( spellData.SpellTrigger != ITEM_SPELLTRIGGER_ON_USE && spellData.SpellTrigger != ITEM_SPELLTRIGGER_ON_NO_DELAY_USE)
                continue;

            SpellEntry const *spellInfo = sSpellStore.LookupEntry(spellData.SpellId);
            if(!spellInfo)
            {
                sLog.outError("Item (Entry: %u) in have wrong spell id %u, ignoring ",proto->ItemId, spellData.SpellId);
                continue;
            }

            Spell *spell = new Spell(pUser, spellInfo, (count > 0));
            spell->m_CastItem = pItem;
            spell->m_cast_count = cast_count;               //set count of casts
            spell->prepare(&targets);

            ++count;
        }
    }
}

#define OPEN_CHEST 11437
#define OPEN_SAFE 11535
#define OPEN_CAGE 11792
#define OPEN_BOOTY_CHEST 5107
#define OPEN_STRONGBOX 8517

void WorldSession::HandleOpenItemOpcode(WorldPacket& recvPacket)
{
    CHECK_PACKET_SIZE(recvPacket,1+1);

    sLog.outDetail("WORLD: CMSG_OPEN_ITEM packet, data length = %i",recvPacket.size());

    Player* pUser = _player;
    uint8 bagIndex, slot;

    recvPacket >> bagIndex >> slot;

    sLog.outDetail("bagIndex: %u, slot: %u",bagIndex,slot);

    Item *pItem = pUser->GetItemByPos(bagIndex, slot);
    if(!pItem)
    {
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, NULL, NULL );
        return;
    }

    ItemPrototype const *proto = pItem->GetProto();
    if(!proto)
    {
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, pItem, NULL );
        return;
    }

    // locked item
    uint32 lockId = proto->LockID;
    if(lockId)
    {
        LockEntry const *lockInfo = sLockStore.LookupEntry(lockId);

        if (!lockInfo)
        {
            pUser->SendEquipError(EQUIP_ERR_ITEM_LOCKED, pItem, NULL );
            sLog.outError( "WORLD::OpenItem: item [guid = %u] has an unknown lockId: %u!", pItem->GetGUIDLow() , lockId);
            return;
        }

        // required picklocking
        if(lockInfo->requiredlockskill || lockInfo->requiredminingskill)
        {
            pUser->SendEquipError(EQUIP_ERR_ITEM_LOCKED, pItem, NULL );
            return;
        }
    }

    if(pItem->HasFlag(ITEM_FIELD_FLAGS, ITEM_FLAGS_WRAPPED))// wrapped?
    {
        QueryResult *result = CharacterDatabase.PQuery("SELECT entry, flags FROM character_gifts WHERE item_guid = '%u'", pItem->GetGUIDLow());
        if (result)
        {
            Field *fields = result->Fetch();
            uint32 entry = fields[0].GetUInt32();
            uint32 flags = fields[1].GetUInt32();

            pItem->SetUInt64Value(ITEM_FIELD_GIFTCREATOR, 0);
            pItem->SetUInt32Value(OBJECT_FIELD_ENTRY, entry);
            pItem->SetUInt32Value(ITEM_FIELD_FLAGS, flags);
            pItem->SetState(ITEM_CHANGED, pUser);
            delete result;
        }
        else
        {
            sLog.outError("Wrapped item %u don't have record in character_gifts table and will deleted", pItem->GetGUIDLow());
            pUser->DestroyItem(pItem->GetBagSlot(), pItem->GetSlot(), true);
            return;
        }
        CharacterDatabase.PExecute("DELETE FROM character_gifts WHERE item_guid = '%u'", pItem->GetGUIDLow());
    }
    else
        pUser->SendLoot(pItem->GetGUID(),LOOT_CORPSE);
}

void WorldSession::HandleGameObjectUseOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,8);

    uint64 guid;
    uint32 spellId = OPEN_CHEST;
    const GameObjectInfo *info;

    recv_data >> guid;

    sLog.outDebug( "WORLD: Recvd CMSG_GAMEOBJ_USE Message [guid=%u]", guid);
    GameObject *obj = ObjectAccessor::GetGameObject(*_player, guid);

    if(!obj) return;
    //obj->SetUInt32Value(GAMEOBJECT_FLAGS,2);
    //obj->SetUInt32Value(GAMEOBJECT_FLAGS,2);

    // default spell caster is player that use GO
    Unit* spellCaster = GetPlayer();

    if (Script->GOHello(_player, obj))
        return;

    switch(obj->GetGoType())
    {
        case GAMEOBJECT_TYPE_DOOR:                          //0
        case GAMEOBJECT_TYPE_BUTTON:                        //1
            obj->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_IN_USE | GO_FLAG_NODESPAWN);
            if(obj->GetUInt32Value(GAMEOBJECT_STATE))
            {
                obj->SetUInt32Value(GAMEOBJECT_STATE,0);    //if closed/inactive -> open/activate it
            }
            else
            {
                obj->SetUInt32Value(GAMEOBJECT_STATE,1);    //if open/active -> close/deactivate it
            }

            obj->SetLootState(GO_CLOSED);
            obj->SetRespawnTime(6);
            //doors/buttons never really despawn, only reset to default state/flags
            //no hard coded reset time for doors/buttons, this should be determined by time defined in `gameobject`.`spawntimesecs`

            // activate script
            sWorld.ScriptsStart(sGameObjectScripts, obj->GetDBTableGUIDLow(), spellCaster, obj);
            return;

        case GAMEOBJECT_TYPE_QUESTGIVER:                    //2
            _player->PrepareQuestMenu( guid );
            _player->SendPreparedQuest( guid );
            return;

            //Sitting: Wooden bench, chairs enzz
        case GAMEOBJECT_TYPE_CHAIR:                         //7

            info = obj->GetGOInfo();
            if(info)
            {
                //spellId = info->data0;                   // this is not a spell or offset
                _player->TeleportTo(obj->GetMapId(), obj->GetPositionX(), obj->GetPositionY(), obj->GetPositionZ(), obj->GetOrientation(),false,false);
                                                            // Using (3 + spellId) was wrong, this is a number of slot for chair/bench, not offset
                _player->SetFlag(UNIT_FIELD_BYTES_1,PLAYER_STATE_SIT_LOW_CHAIR);
                _player->SetStandState(PLAYER_STATE_SIT_LOW_CHAIR);
                return;
            }
            break;

            //big gun, its a spell/aura
        case GAMEOBJECT_TYPE_GOOBER:                        //10
            info = obj->GetGOInfo();
            spellId = info ? info->data10 : 0;
            break;

        case GAMEOBJECT_TYPE_SPELLCASTER:                   //22

            obj->SetUInt32Value(GAMEOBJECT_FLAGS,2);

            info = obj->GetGOInfo();
            if(info)
            {
                spellId = info->data0;
                if (spellId == 0)
                    spellId = info->data3;

                //guid=_player->GetGUID();
            }

            break;
        case GAMEOBJECT_TYPE_CAMERA:                        //13
            info = obj->GetGOInfo();
            if(info)
            {
                uint32 cinematic_id = info->data1;
                if(cinematic_id)
                {
                    WorldPacket data(SMSG_TRIGGER_CINEMATIC, 4);
                    data << cinematic_id;
                    _player->GetSession()->SendPacket(&data);
                }
                return;
            }
            break;

            //fishing bobber
        case GAMEOBJECT_TYPE_FISHINGNODE:                   //17
        {
            if(_player->GetGUID() != obj->GetOwnerGUID())
                return;

            switch(obj->getLootState())
            {
                case GO_CLOSED:                             // ready for loot
                {
                    // 1) skill must be >= base_zone_skill
                    // 2) if skill == base_zone_skill => 5% chance
                    // 3) chance is linear dependence from (base_zone_skill-skill)

                    int32 skill = _player->GetSkillValue(SKILL_FISHING);
                    int32 zone_skill = _player->FishingMinSkillForCurrentZone();
                    int32 chance = skill - zone_skill + 5;
                    int32 roll = irand(1,100);

                    DEBUG_LOG("Fishing check (skill: %i zone min skill: %i chance %i roll: %i",skill,zone_skill,chance,roll);

                    if(skill >= zone_skill && chance >= roll)
                    {
                        // prevent removing GO at spell cancel
                        _player->RemoveGameObject(obj,false);
                        obj->SetOwnerGUID(_player->GetGUID());

                        //fish catched
                        _player->UpdateFishingSkill();
                        _player->SendLoot(obj->GetGUID(),LOOT_FISHING);
                    }
                    else
                    {
                        // fish escaped
                        obj->SetLootState(GO_LOOTED);       // can be deleted now

                        WorldPacket data(SMSG_FISH_ESCAPED, 0);
                        SendPacket(&data);
                    }
                    break;
                }
                case GO_LOOTED:                             // nothing to do, will be deleted at next update
                    break;
                default:
                {
                    obj->SetLootState(GO_LOOTED);

                    WorldPacket data(SMSG_FISH_NOT_HOOKED, 0);
                    SendPacket(&data);
                    break;
                }
            }

            if(_player->m_currentSpells[CURRENT_CHANNELED_SPELL])
            {
                _player->m_currentSpells[CURRENT_CHANNELED_SPELL]->SendChannelUpdate(0);
                _player->m_currentSpells[CURRENT_CHANNELED_SPELL]->finish();
            }
            return;
        }

        case GAMEOBJECT_TYPE_SUMMONING_RITUAL:
        {
            Unit* caster = obj->GetOwner();

            info = obj->GetGOInfo();

            if( !caster || caster->GetTypeId()!=TYPEID_PLAYER )
                return;

            // accept only use by player from same group for caster except caster itself
            if(((Player*)caster)==GetPlayer() || !((Player*)caster)->IsInSameGroupWith(GetPlayer()))
                return;

            obj->AddUse(GetPlayer());

            // must 2 group members use GO, or only 1 when it is meeting stone summon
            if(obj->GetUniqueUseCount() < (info->data0 == 2 ? 1 : 2))
                return;

            // in case summoning ritual caster is GO creator
            spellCaster = caster;

            if(!caster->m_currentSpells[CURRENT_CHANNELED_SPELL])
                return;

            spellId = info->data1;

            // finish spell
            caster->m_currentSpells[CURRENT_CHANNELED_SPELL]->SendChannelUpdate(0);
            caster->m_currentSpells[CURRENT_CHANNELED_SPELL]->finish();

            // can be deleted now
            obj->SetLootState(GO_LOOTED);

            // go to end function to spell casting
            break;
        }
        case GAMEOBJECT_TYPE_MEETINGSTONE:                  //23
        {
            info = obj->GetGOInfo();

            Player* targetPlayer = ObjectAccessor::FindPlayer(_player->GetSelection());

            // accept only use by player from same group for caster except caster itself
            if(!targetPlayer || targetPlayer == GetPlayer() || !targetPlayer->IsInSameGroupWith(GetPlayer()))
                return;

            //required lvl checks!
            uint8 level = _player->getLevel();
            if (level < info->data0 || level > info->data1)
                return;
            level = targetPlayer->getLevel();
            if (level < info->data0 || level > info->data1)
                return;

            spellId = 23598;

            break;
        }

        case GAMEOBJECT_TYPE_FLAGSTAND:                     // 24
            if(_player->InBattleGround() &&                 // in battleground
                !_player->IsMounted() &&                    // not mounted
                !_player->HasStealthAura() &&               // not stealthed
                !_player->HasInvisibilityAura() &&          // not invisible
                _player->isAlive())                         // live player
            {
                BattleGround *bg = _player->GetBattleGround();
                if(!bg)
                    return;
                // BG flag click
                // AB:
                // 15001
                // 15002
                // 15003
                // 15004
                // 15005
                // WS:
                // 179830 - Silverwing Flag
                // 179831 - Warsong Flag
                // EotS:
                // 184141 - Netherstorm Flag
                info = obj->GetGOInfo();
                if(info)
                {
                    switch(info->id)
                    {
                        case 179830:
                            // check if it's correct bg
                            if(bg->GetTypeID() != BATTLEGROUND_WS)
                                return;
                            // check if flag dropped
                            if(((BattleGroundWS*)bg)->GetFlagState(ALLIANCE) != BG_WS_FLAG_STATE_ON_BASE)
                                return;
                            // check if it's correct flag
                            if(((BattleGroundWS*)bg)->m_BgObjects[BG_WS_OBJECT_A_FLAG] != obj->GetGUID())
                                return;
                            // check player team
                            if(_player->GetTeam() == ALLIANCE)
                                return;
                            spellId = 23335;                // Silverwing Flag
                            break;
                        case 179831:
                            // check if it's correct bg
                            if(bg->GetTypeID() != BATTLEGROUND_WS)
                                return;
                            // check if flag dropped
                            if(((BattleGroundWS*)bg)->GetFlagState(HORDE) != BG_WS_FLAG_STATE_ON_BASE)
                                return;
                            // check if it's correct flag
                            if(((BattleGroundWS*)bg)->m_BgObjects[BG_WS_OBJECT_H_FLAG] != obj->GetGUID())
                                return;
                            // check player team
                            if(_player->GetTeam() == HORDE)
                                return;
                            spellId = 23333;                // Warsong Flag
                            break;
                        case 184141:
                            // check if it's correct bg
                            if(bg->GetTypeID() != BATTLEGROUND_EY)
                                return;
                            spellId = 34976;                // Netherstorm Flag
                            break;
                    }
                }
            }
            break;
        case GAMEOBJECT_TYPE_FLAGDROP:                      // 26
            if(_player->InBattleGround() &&                 // in battleground
                !_player->IsMounted() &&                    // not mounted
                !_player->HasStealthAura() &&               // not stealthed
                !_player->HasInvisibilityAura() &&          // not invisible
                _player->isAlive())                         // live player
            {
                BattleGround *bg = _player->GetBattleGround();
                if(!bg)
                    return;
                // BG flag dropped
                // WS:
                // 179785 - Silverwing Flag
                // 179786 - Warsong Flag
                // EotS:
                // 184142 - Netherstorm Flag
                info = obj->GetGOInfo();
                if(info)
                {
                    switch(info->id)
                    {
                        case 179785:                        // Silverwing Flag
                            // check if it's correct bg
                            if(bg->GetTypeID() != BATTLEGROUND_WS)
                                return;
                            // check if flag dropped
                            if(((BattleGroundWS*)bg)->GetFlagState(ALLIANCE) != BG_WS_FLAG_STATE_ON_GROUND)
                                return;
                            obj->Delete();
                            if(_player->GetTeam() == ALLIANCE)
                            {
                                ((BattleGroundWS*)bg)->EventPlayerReturnedFlag(_player);
                                return;
                            }
                            else
                            {
                                _player->CastSpell(_player, 23335, true);
                                return;
                            }
                            break;
                        case 179786:                        // Warsong Flag
                            // check if it's correct bg
                            if(bg->GetTypeID() != BATTLEGROUND_WS)
                                return;
                            // check if flag dropped
                            if(((BattleGroundWS*)bg)->GetFlagState(HORDE) != BG_WS_FLAG_STATE_ON_GROUND)
                                return;
                            obj->Delete();
                            if(_player->GetTeam() == HORDE)
                            {
                                ((BattleGroundWS*)bg)->EventPlayerReturnedFlag(_player);
                                return;
                            }
                            else
                            {
                                _player->CastSpell(_player, 23333, true);
                                return;
                            }
                            break;
                    }
                }
                obj->Delete();
            }
            break;
        default:
            sLog.outDebug("Unknown Object Type %u", obj->GetGoType());
            break;
    }

    if (!spellId) return;

    SpellEntry const *spellInfo = sSpellStore.LookupEntry( spellId );
    if(!spellInfo)
    {
        sLog.outError("WORLD: unknown spell id %u", spellId);
        return;
    }

    Spell *spell = new Spell(spellCaster, spellInfo, false);

    // spell target is player that use GO
    SpellCastTargets targets;
    targets.setUnitTarget( _player );

    if(obj)
        targets.setGOTarget( obj );

    spell->prepare(&targets);
}

void WorldSession::HandleCastSpellOpcode(WorldPacket& recvPacket)
{
    CHECK_PACKET_SIZE(recvPacket,4+1+2);

    uint32 spellId;
    uint8  cast_count;
    recvPacket >> spellId;
    recvPacket >> cast_count;

    sLog.outDebug("WORLD: got cast spell packet, spellId - %u, cast_count: %u data length = %i",
        spellId, cast_count, recvPacket.size());

    SpellEntry const *spellInfo = sSpellStore.LookupEntry(spellId );

    if(!spellInfo)
    {
        sLog.outError("WORLD: unknown spell id %u", spellId);
        return;
    }

    // not have spell or spell passive and not casted by client
    if ( !_player->HasSpell (spellId) || IsPassiveSpell(spellId) )
    {
        //cheater? kick? ban?
        return;
    }

    // client provided targets
    SpellCastTargets targets;
    if(!targets.read(&recvPacket,_player))
        return;

    // auto-selection buff level base at target level (in spellInfo)
    if(targets.getUnitTarget())
    {
        SpellEntry const *actualSpellInfo = spellmgr.SelectAuraRankForPlayerLevel(spellInfo,targets.getUnitTarget()->getLevel());

        // if rank not found then function return NULL but in explicit cast case original spell can be casted and later failed with appropriate error message
        if(actualSpellInfo)
            spellInfo = actualSpellInfo;
    }

    Spell *spell = new Spell(_player, spellInfo, false);
    spell->m_cast_count = cast_count;                       //set count of casts
    spell->prepare(&targets);
}

void WorldSession::HandleCancelCastOpcode(WorldPacket&/* recvPacket*/)
{
    if(_player->IsNonMeleeSpellCasted(false))
        _player->InterruptNonMeleeSpells(false);
}

void WorldSession::HandleCancelAuraOpcode( WorldPacket& recvPacket)
{
    CHECK_PACKET_SIZE(recvPacket,4);

    uint32 spellId;
    recvPacket >> spellId;

    // not allow remove non positive spells
    if(!IsPositiveSpell(spellId))
        return;

    _player->RemoveAurasDueToSpell(spellId);

    if (spellId == 2584)                                    // Waiting to resurrect spell cancel, we must remove player from resurrect queue
    {
        BattleGround *bg = _player->GetBattleGround();
        if(!bg)
            return;
        bg->RemovePlayerFromResurrectQueue(_player->GetGUID());
    }
}

void WorldSession::HandlePetCancelAuraOpcode( WorldPacket& recvPacket)
{
    CHECK_PACKET_SIZE(recvPacket, 8+4);

    uint64 guid;
    uint32 spellId;

    recvPacket >> guid;
    recvPacket >> spellId;

    SpellEntry const *spellInfo = sSpellStore.LookupEntry(spellId );
    if(!spellInfo)
    {
        sLog.outError("WORLD: unknown PET spell id %u", spellId);
        return;
    }

    Creature* pet=ObjectAccessor::GetCreatureOrPet(*_player,guid);

    if(!pet)
    {
        sLog.outError( "Pet %u not exist.", uint32(GUID_LOPART(guid)) );
        return;
    }

    if(pet != GetPlayer()->GetPet() && pet != GetPlayer()->GetCharm())
    {
        sLog.outError( "HandlePetCancelAura.Pet %u isn't pet of player %s", uint32(GUID_LOPART(guid)),GetPlayer()->GetName() );
        return;
    }

    if(!pet->isAlive())
    {
        pet->SendPetActionFeedback(FEEDBACK_PET_DEAD);
        return;
    }

    pet->RemoveAurasDueToSpell(spellId);

    pet->AddCreatureSpellCooldown(spellId);
}

void WorldSession::HandleCancelGrowthAuraOpcode( WorldPacket& /*recvPacket*/)
{
    // nothing do
}

void WorldSession::HandleCancelAutoRepeatSpellOpcode( WorldPacket& /*recvPacket*/)
{
    // may be better send SMSG_CANCEL_AUTO_REPEAT?
    // cancel and prepare for deleting
    _player->InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
}

/// \todo Complete HandleCancelChanneling function
void WorldSession::HandleCancelChanneling( WorldPacket & /*recv_data */)
{
    /*
        CHECK_PACKET_SIZE(recv_data, 4);

        uint32 spellid;
        recv_data >> spellid;
    */
}

void WorldSession::HandleSelfResOpcode( WorldPacket & /*recv_data*/ )
{
    sLog.outDebug("WORLD: CMSG_SELF_RES");                  // empty opcode

    if(_player->GetUInt32Value(PLAYER_SELF_RES_SPELL))
    {
        SpellEntry const *spellInfo = sSpellStore.LookupEntry(_player->GetUInt32Value(PLAYER_SELF_RES_SPELL));
        if(spellInfo)
            _player->CastSpell(_player,spellInfo,false,0);

        _player->SetUInt32Value(PLAYER_SELF_RES_SPELL, 0);
    }
}
