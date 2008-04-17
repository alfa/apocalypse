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
    CHECK_PACKET_SIZE(recvPacket,1+1+1+1+8);

    Player* pUser = _player;
    uint8 bagIndex, slot;
    uint8 spell_count;                                      // number of spells at item, not used
    uint8 cast_count;                                       // next cast if exists (single or not)
    uint64 item_guid;

    recvPacket >> bagIndex >> slot >> spell_count >> cast_count >> item_guid;

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

        // special learning case
        if(pItem->GetProto()->Spells[0].SpellId==SPELL_GENERIC_LEARN)
        {
            uint32 learning_spell_id = pItem->GetProto()->Spells[1].SpellId;

            SpellEntry const *spellInfo = sSpellStore.LookupEntry(SPELL_GENERIC_LEARN);
            if(!spellInfo)
            {
                sLog.outError("Item (Entry: %u) in have wrong spell id %u, ignoring ",proto->ItemId, SPELL_GENERIC_LEARN);
                pUser->SendEquipError(EQUIP_ERR_NONE,pItem,NULL);
                return;
            }

            Spell *spell = new Spell(pUser, spellInfo, false);
            spell->m_CastItem = pItem;
            spell->m_cast_count = cast_count;               //set count of casts
            spell->m_currentBasePoints[0] = learning_spell_id;
            spell->prepare(&targets);
            return;
        }

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
            //doors/buttons never really despawn, only reset to default state/flags
            obj->UseDoorOrButton();

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
                // a chair may have n slots. we have to calculate their positions and teleport the player to the nearest one

                // check if the db is sane
                if(info->chair.slots > 0)
                {
                    float lowestDist = DEFAULT_VISIBILITY_DISTANCE;
                    
                    float x_lowest = obj->GetPositionX();
                    float y_lowest = obj->GetPositionY();

                    // the object orientation+ 90�
                    // every slot will be on that straight line
                    float orthogonalOrientation = obj->GetOrientation()+M_PI*0.5f;
                    // find nearest slot
                    for(uint32 i=0; i<info->chair.slots; i++)
                    {
                        // the distance between this slot and the center of the go - imagine a 1D space
                        float relativeDistance = (info->size*i)-(info->size*(info->chair.slots-1)/2.0f);

                        float x_i = obj->GetPositionX() + relativeDistance * cos(orthogonalOrientation);
                        float y_i = obj->GetPositionY() + relativeDistance * sin(orthogonalOrientation);

                        // calculate the distance between the player and this slot
                        float thisDistance = _player->GetDistance2d(x_i, y_i);

                        /* debug code. It will spawn a npc on each slot to visualize them.
                        Creature* helper = _player->SummonCreature(14496, x_i, y_i, obj->GetPositionZ(), obj->GetOrientation(), TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 10000);
                        std::ostringstream output;
                        output << i << ": thisDist: " << thisDistance;
                        helper->MonsterSay(output.str().c_str(), LANG_UNIVERSAL, 0);
                        */

                        if(thisDistance <= lowestDist)
                        {
                            lowestDist = thisDistance;
                            x_lowest = x_i;
                            y_lowest = y_i;
                        }
                    }
                    _player->TeleportTo(obj->GetMapId(), x_lowest, y_lowest, obj->GetPositionZ(), obj->GetOrientation(),false,false);
                }
                else
                {
                    // fallback, will always work
                    _player->TeleportTo(obj->GetMapId(), obj->GetPositionX(), obj->GetPositionY(), obj->GetPositionZ(), obj->GetOrientation(),false,false);
                }
                _player->SetStandState(PLAYER_STATE_SIT_LOW_CHAIR+info->chair.height);
                return;
            }
            break;

            //big gun, its a spell/aura
        case GAMEOBJECT_TYPE_GOOBER:                        //10
            info = obj->GetGOInfo();
            spellId = info ? info->goober.spellId : 0;
            break;

        case GAMEOBJECT_TYPE_CAMERA:                        //13
            info = obj->GetGOInfo();
            if(info)
            {
                if(info->camera.cinematicId)
                {
                    WorldPacket data(SMSG_TRIGGER_CINEMATIC, 4);
                    data << info->camera.cinematicId;
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
                case GO_READY:                              // ready for loot
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

                        GameObject* ok = obj->LookupFishingHoleAround(DEFAULT_VISIBILITY_DISTANCE);
                        if (ok)
                        {
                            _player->SendLoot(ok->GetGUID(),LOOT_FISHINGHOLE);
                            obj->SetLootState(GO_JUST_DEACTIVATED);
                        }
                        else
                            _player->SendLoot(obj->GetGUID(),LOOT_FISHING);
                    }
                    else
                    {
                        // fish escaped, can be deleted now
                        obj->SetLootState(GO_JUST_DEACTIVATED);

                        WorldPacket data(SMSG_FISH_ESCAPED, 0);
                        SendPacket(&data);
                    }
                    break;
                }
                case GO_JUST_DEACTIVATED:                   // nothing to do, will be deleted at next update
                    break;
                default:
                {
                    obj->SetLootState(GO_JUST_DEACTIVATED);

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

        case GAMEOBJECT_TYPE_SUMMONING_RITUAL:              //18
        {
            Unit* caster = obj->GetOwner();

            info = obj->GetGOInfo();

            if( !caster || caster->GetTypeId()!=TYPEID_PLAYER )
                return;

            // accept only use by player from same group for caster except caster itself
            if(((Player*)caster)==GetPlayer() || !((Player*)caster)->IsInSameGroupWith(GetPlayer()))
                return;

            obj->AddUniqueUse(GetPlayer());

            // full amount unique participants including original summoner
            if(obj->GetUniqueUseCount() < info->summoningRitual.reqParticipants)
                return;

            // in case summoning ritual caster is GO creator
            spellCaster = caster;

            if(!caster->m_currentSpells[CURRENT_CHANNELED_SPELL])
                return;

            spellId = info->summoningRitual.spellId;

            // finish spell
            caster->m_currentSpells[CURRENT_CHANNELED_SPELL]->SendChannelUpdate(0);
            caster->m_currentSpells[CURRENT_CHANNELED_SPELL]->finish();

            // can be deleted now
            obj->SetLootState(GO_JUST_DEACTIVATED);

            // go to end function to spell casting
            break;
        }
        case GAMEOBJECT_TYPE_SPELLCASTER:                   //22
        {

            obj->SetUInt32Value(GAMEOBJECT_FLAGS,2);

            info = obj->GetGOInfo();
            if(!info)
                return;

            if(info->spellcaster.partyOnly)
            {
                Unit* caster = obj->GetOwner();
                if( !caster || caster->GetTypeId()!=TYPEID_PLAYER )
                    return;

                if(!GetPlayer()->IsInSameRaidWith((Player*)caster))
                    return;
            }

            spellId = info->spellcaster.spellId;
            if (spellId == 0)
                spellId = info->spellcaster.spellId2;

            obj->AddUse();
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
            if (level < info->meetingstone.minLevel || level > info->meetingstone.maxLevel)
                return;
            level = targetPlayer->getLevel();
            if (level < info->meetingstone.minLevel || level > info->meetingstone.maxLevel)
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

void WorldSession::HandleCancelCastOpcode(WorldPacket& recvPacket)
{
    CHECK_PACKET_SIZE(recvPacket,4);

    uint32 spellId;
    recvPacket >> spellId;

    //FIXME: hack, ignore unexpected client cancel Deadly Throw cast
    if(spellId==26679)
        return;

    if(_player->IsNonMeleeSpellCasted(false))
        _player->InterruptNonMeleeSpells(false,spellId);
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
