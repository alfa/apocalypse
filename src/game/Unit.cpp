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
#include "Log.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Unit.h"
#include "QuestDef.h"
#include "Player.h"
#include "Creature.h"
#include "Spell.h"
#include "Group.h"
#include "SpellAuras.h"
#include "MapManager.h"
#include "ObjectAccessor.h"
#include "CreatureAI.h"
#include "Formulas.h"
#include "Pet.h"
#include "Util.h"
#include "Totem.h"
#include "BattleGround.h"
#include "MovementGenerator.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"

#include <math.h>

float baseMoveSpeed[MAX_MOVE_TYPE] =
{
    2.5f,                                                   // MOVE_WALK
    7.0f,                                                   // MOVE_RUN
    1.25f,                                                  // MOVE_WALKBACK
    4.722222f,                                              // MOVE_SWIM
    4.5f,                                                   // MOVE_SWIMBACK
    3.141594f,                                              // MOVE_TURN
    7.0f,                                                   // MOVE_FLY
    4.5f,                                                   // MOVE_FLYBACK
};

// auraTypes contains attacker auras capable of proc'ing cast auras
static Unit::AuraTypeSet GenerateAttakerProcCastAuraTypes()
{
    static Unit::AuraTypeSet auraTypes;
    auraTypes.insert(SPELL_AURA_DUMMY);
    auraTypes.insert(SPELL_AURA_PROC_TRIGGER_SPELL);
    auraTypes.insert(SPELL_AURA_MOD_HASTE);
    return auraTypes;
}

// auraTypes contains victim auras capable of proc'ing cast auras
static Unit::AuraTypeSet GenerateVictimProcCastAuraTypes()
{
    static Unit::AuraTypeSet auraTypes;
    auraTypes.insert(SPELL_AURA_DUMMY);
    auraTypes.insert(SPELL_AURA_DUMMY_3);
    auraTypes.insert(SPELL_AURA_PROC_TRIGGER_SPELL);
    return auraTypes;
}

// auraTypes contains auras capable of proc effect/damage (but not cast) for attacker
static Unit::AuraTypeSet GenerateAttakerProcEffectAuraTypes()
{
    static Unit::AuraTypeSet auraTypes;
    auraTypes.insert(SPELL_AURA_MOD_CASTING_SPEED);
    auraTypes.insert(SPELL_AURA_PROC_TRIGGER_DAMAGE);
    auraTypes.insert(SPELL_AURA_MOD_DAMAGE_DONE);
    return auraTypes;
}

// auraTypes contains auras capable of proc effect/damage (but not cast) for victim
static Unit::AuraTypeSet GenerateVictimProcEffectAuraTypes()
{
    static Unit::AuraTypeSet auraTypes;
    auraTypes.insert(SPELL_AURA_PROC_TRIGGER_DAMAGE);
    auraTypes.insert(SPELL_AURA_MOD_PARRY_PERCENT);
    auraTypes.insert(SPELL_AURA_MOD_BLOCK_PERCENT);
    auraTypes.insert(SPELL_AURA_MOD_RESISTANCE);
    return auraTypes;
}

static Unit::AuraTypeSet attackerProcCastAuraTypes = GenerateAttakerProcCastAuraTypes();
static Unit::AuraTypeSet attackerProcEffectAuraTypes = GenerateAttakerProcEffectAuraTypes();

static Unit::AuraTypeSet victimProcCastAuraTypes = GenerateVictimProcCastAuraTypes();
static Unit::AuraTypeSet victimProcEffectAuraTypes   = GenerateVictimProcEffectAuraTypes();

// auraTypes contains auras capable of proc'ing for attacker and victim
static Unit::AuraTypeSet GenerateProcAuraTypes()
{
    Unit::AuraTypeSet auraTypes;
    auraTypes.insert(attackerProcCastAuraTypes.begin(),attackerProcCastAuraTypes.end());
    auraTypes.insert(attackerProcEffectAuraTypes.begin(),attackerProcEffectAuraTypes.end());
    auraTypes.insert(victimProcCastAuraTypes.begin(),victimProcCastAuraTypes.end());
    auraTypes.insert(victimProcEffectAuraTypes.begin(),victimProcEffectAuraTypes.end());
    return auraTypes;
}

static Unit::AuraTypeSet procAuraTypes = GenerateProcAuraTypes();

bool IsPassiveStackableSpell( uint32 spellId )
{
    if(!IsPassiveSpell(spellId))
        return false;

    SpellEntry const* spellProto = sSpellStore.LookupEntry(spellId);
    if(!spellProto)
        return false;

    for(int j = 0; j < 3; ++j)
    {
        if(std::find(procAuraTypes.begin(),procAuraTypes.end(),spellProto->EffectApplyAuraName[j])!=procAuraTypes.end())
            return false;
    }

    return true;
}

Unit::Unit( WorldObject *instantiator )
: WorldObject( instantiator ), i_motionMaster(this), m_ThreatManager(this), m_HostilRefManager(this)
{
    m_objectType |= TYPE_UNIT;
    m_objectTypeId = TYPEID_UNIT;
                                                            // 2.3.2 - 0x70
    m_updateFlag = (UPDATEFLAG_HIGHGUID | UPDATEFLAG_LIVING | UPDATEFLAG_HASPOSITION);

    m_attackTimer[BASE_ATTACK]   = 0;
    m_attackTimer[OFF_ATTACK]    = 0;
    m_attackTimer[RANGED_ATTACK] = 0;
    m_modAttackSpeedPct[BASE_ATTACK] = 1.0f;
    m_modAttackSpeedPct[OFF_ATTACK] = 1.0f;
    m_modAttackSpeedPct[RANGED_ATTACK] = 1.0f;

    m_state = 0;
    m_form = 0;
    m_deathState = ALIVE;

    for (uint32 i = 0; i < CURRENT_MAX_SPELL; i++)
        m_currentSpells[i] = NULL;
    m_addDmgOnce = 0;
    m_TotemSlot[0] = m_TotemSlot[1] = m_TotemSlot[2] = m_TotemSlot[3]  = 0;
    m_ObjectSlot[0] = m_ObjectSlot[1] = m_ObjectSlot[2] = m_ObjectSlot[3] = 0;
    //m_Aura = NULL;
    //m_AurasCheck = 2000;
    //m_removeAuraTimer = 4;
    //tmpAura = NULL;
    waterbreath = false;

    m_Visibility = VISIBILITY_ON;

    m_detectInvisibilityMask = 0;
    m_invisibilityMask = 0;
    m_transform = 0;
    m_ShapeShiftForm = 0;
    m_canModifyStats = false;

    for (int i = 0; i < MAX_SPELL_IMMUNITY; i++)
        m_spellImmune[i].clear();
    for (int i = 0; i < UNIT_MOD_END; i++)
    {
        m_auraModifiersGroup[i][BASE_VALUE] = 0.0f;
        m_auraModifiersGroup[i][BASE_PCT] = 1.0f;
        m_auraModifiersGroup[i][TOTAL_VALUE] = 0.0f;
        m_auraModifiersGroup[i][TOTAL_PCT] = 1.0f;
    }
                                                            // implement 50% base damage from offhand
    m_auraModifiersGroup[UNIT_MOD_DAMAGE_OFFHAND][TOTAL_PCT] = 0.5f;

    for (int i = 0; i < 3; i++)
    {
        m_weaponDamage[i][MINDAMAGE] = BASE_MINDAMAGE;
        m_weaponDamage[i][MAXDAMAGE] = BASE_MAXDAMAGE;
    }
    for (int i = 0; i < MAX_STATS; i++)
        m_createStats[i] = 0.0f;

    m_attacking = NULL;
    m_modMeleeHitChance = 0.0f;
    m_modRangedHitChance = 0.0f;
    m_modSpellHitChance = 0.0f;
    m_baseSpellCritChance = 5;

    m_CombatTimer = 0;
    m_lastManaUse = 0;

    //m_victimThreat = 0.0f;
    for (int i = 0; i < MAX_SPELL_SCHOOL; ++i)
        m_threatModifier[i] = 1.0f;
    m_isSorted = true;
    for (int i = 0; i < MAX_MOVE_TYPE; ++i)
        m_speed_rate[i] = 1.0f;

    m_removedAuras = 0;
    m_charmInfo = NULL;
    m_unit_movement_flags = 0;

    // remove aurastates allowing special moves
    for(int i=0; i < MAX_REACTIVE; ++i)
        m_reactiveTimer[i] = 0;
}

Unit::~Unit()
{
    // set current spells as deletable
    for (uint32 i = 0; i < CURRENT_MAX_SPELL; i++)
    {
                                                            // spell may be safely deleted now
        if (m_currentSpells[i]) m_currentSpells[i]->SetDeletable(true);
        m_currentSpells[i] = NULL;
    }

    // remove references to unit
    for(std::list<GameObject*>::iterator i = m_gameObj.begin(); i != m_gameObj.end();)
    {
        (*i)->SetOwnerGUID(0);
        (*i)->SetRespawnTime(0);
        (*i)->Delete();
        i = m_gameObj.erase(i);
    }

    RemoveAllDynObjects();

    if(m_charmInfo) delete m_charmInfo;
}

void Unit::RemoveAllDynObjects()
{
    while(!m_dynObjGUIDs.empty())
    {
        DynamicObject* dynObj = ObjectAccessor::GetDynamicObject(*this,*m_dynObjGUIDs.begin());
        if(dynObj)
            dynObj->Delete();
        m_dynObjGUIDs.erase(m_dynObjGUIDs.begin());
    }
}

void Unit::Update( uint32 p_time )
{
    /*if(p_time > m_AurasCheck)
    {
    m_AurasCheck = 2000;
    _UpdateAura();
    }else
    m_AurasCheck -= p_time;*/

    // WARNING! Order of execution here is important, do not change.
    // Spells must be processed with event system BEFORE they go to _UpdateSpells.
    // Or else we may have some SPELL_STATE_FINISHED spells stalled in pointers, that is bad.
    m_Events.Update( p_time );
    _UpdateSpells( p_time );

    //update combat timer only for players and pets
    if (isInCombat() && (GetTypeId() == TYPEID_PLAYER || ((Creature*)this)->isPet() || ((Creature*)this)->isCharmed()))
    {
        // Check UNIT_STAT_ATTACKING or UNIT_STAT_CHASE so pets can reach far away
        // targets without stopping half way there and running off.
        // These flags are reset after target dies or another command is given.
        if( m_HostilRefManager.isEmpty() && !hasUnitState(UNIT_STAT_ATTACKING | UNIT_STAT_CHASE) )
        {
            // m_CombatTimer set at aura start and it will be freeze until aura removing
            if ( m_CombatTimer <= p_time )
                ClearInCombat();
            else
                m_CombatTimer -= p_time;
        }
    }

    if(uint32 base_att = getAttackTimer(BASE_ATTACK))
    {
        setAttackTimer(BASE_ATTACK, (p_time >= base_att ? 0 : base_att - p_time) );
    }

    // update abilities avaiable only for fraction of time
    UpdateReactives( p_time );

    ModifyAuraState(AURA_STATE_HEALTHLESS_20_PERCENT, GetHealth() < GetMaxHealth()*0.20f);
    ModifyAuraState(AURA_STATE_HEALTHLESS_35_PERCENT, GetHealth() < GetMaxHealth()*0.35f);

    i_motionMaster.UpdateMotion(p_time);
}

bool Unit::haveOffhandWeapon() const
{
    if(GetTypeId() == TYPEID_PLAYER)
    {
        Item *tmpitem = ((Player*)this)->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);

        return tmpitem && !tmpitem->IsBroken() && (tmpitem->GetProto()->InventoryType == INVTYPE_WEAPON || tmpitem->GetProto()->InventoryType == INVTYPE_WEAPONOFFHAND);
    }
    else
        return false;
}

void Unit::SendMoveToPacket(float x, float y, float z, uint32 MovementFlags, uint32 transitTime)
{
    float dx = x - GetPositionX();
    float dy = y - GetPositionY();
    float dz = z - GetPositionZ();
    if (!transitTime)
    {
        float dist = ((dx*dx) + (dy*dy) + (dz*dz));
        if(dist<0)
            dist = 0;
        else
            dist = sqrt(dist);

        double speed = GetSpeed((MovementFlags & MOVEMENTFLAG_WALK_MODE) ? MOVE_WALK : MOVE_RUN);
        if(speed<=0)
            speed = 2.5f;
        speed *= 0.001f;
        transitTime = static_cast<uint32>(dist / speed + 0.5);
    }
    //float orientation = (float)atan2((double)dy, (double)dx);
    SendMonsterMove(x, y, z, 0, MovementFlags, transitTime);
}

void Unit::SendMonsterMove(float NewPosX, float NewPosY, float NewPosZ, uint8 type, uint32 MovementFlags, uint32 Time)
{
    WorldPacket data( SMSG_MONSTER_MOVE, (41 + GetPackGUID().size()) );
    data.append(GetPackGUID());

    // Point A, starting location
    data << GetPositionX() << GetPositionY() << GetPositionZ();
    // unknown field - unrelated to orientation
    // seems to increment about 1000 for every 1.7 seconds
    // for now, we'll just use mstime
    data << getMSTime();

    data << uint8(type);                                    // unknown
    switch(type)
    {
        case 0:                                             // normal packet
            break;
        case 1:                                             // stop packet
            SendMessageToSet( &data, true );
            return;
        case 3:                                             // not used currently
            data << uint64(0);                              // probably target guid
            break;
        case 4:                                             // not used currently
            data << float(0);                               // probably orientation
            break;
    }

    //Movement Flags (0x0 = walk, 0x100 = run, 0x200 = fly/swim)
    data << uint32(MovementFlags);

    data << Time;                                           // Time in between points
    data << uint32(1);                                      // 1 single waypoint
    data << NewPosX << NewPosY << NewPosZ;                  // the single waypoint Point B

    SendMessageToSet( &data, true );
}

void Unit::resetAttackTimer(WeaponAttackType type)
{
    if (GetTypeId() == TYPEID_PLAYER)
        m_attackTimer[type] = uint32(GetAttackTime(type) * m_modAttackSpeedPct[type]);
    else
        m_attackTimer[type] = uint32(BASE_ATTACK_TIME * m_modAttackSpeedPct[type]);
}

bool Unit::canReachWithAttack(Unit *pVictim) const
{
    assert(pVictim);
    float reach = GetFloatValue(UNIT_FIELD_COMBATREACH);
    if( reach <= 0.0f )
        reach = 1.0f;
    return IsWithinDistInMap(pVictim, reach);
}

void Unit::RemoveSpellsCausingAura(AuraType auraType)
{
    if (auraType >= TOTAL_AURAS) return;
    AuraList::iterator iter, next;
    for (iter = m_modAuras[auraType].begin(); iter != m_modAuras[auraType].end(); iter = next)
    {
        next = iter;
        ++next;

        if (*iter)
        {
            RemoveAurasDueToSpell((*iter)->GetId());
            if (!m_modAuras[auraType].empty())
                next = m_modAuras[auraType].begin();
            else
                return;
        }
    }
}

bool Unit::HasAuraType(AuraType auraType) const
{
    return (!m_modAuras[auraType].empty());
}

/* Called by DealDamage for auras that have a chance to be dispelled on damage taken. */
void Unit::RemoveSpellbyDamageTaken(AuraType auraType, uint32 damage)
{
    if(!HasAuraType(auraType))
        return;

    // The chance to dispell an aura depends on the damage taken with respect to the casters level.
    uint32 max_dmg = getLevel() > 8 ? 25 * getLevel() - 150 : 50;
    float chance = float(damage) / max_dmg * 100.0f;
    if (roll_chance_f(chance))
        RemoveSpellsCausingAura(auraType);
}

uint32 Unit::DealDamage(Unit *pVictim, uint32 damage, CleanDamage const* cleanDamage, DamageEffectType damagetype, SpellSchoolMask damageSchoolMask, SpellEntry const *spellProto, bool durabilityLoss)
{
    if (!pVictim->isAlive() || pVictim->isInFlight() || pVictim->GetTypeId() == TYPEID_UNIT && ((Creature*)pVictim)->IsInEvadeMode())
        return 0;

    //You don't lose health from damage taken from another player while in a sanctuary
    //You still see it in the combat log though
    if(pVictim != this && GetTypeId() == TYPEID_PLAYER && pVictim->GetTypeId() == TYPEID_PLAYER)
    {
        const AreaTableEntry *area = GetAreaEntryByAreaID(pVictim->GetAreaId());
        if(area && area->flags & AREA_FLAG_SANCTUARY)       //sanctuary
            return 0;
    }

    // remove affects from victim (including from 0 damage and DoTs)
    if(pVictim != this)
        pVictim->RemoveSpellsCausingAura(SPELL_AURA_MOD_STEALTH);

    // remove affects from attacker at any non-DoT damage (including 0 damage)
    if( damagetype != DOT)
    {
        RemoveSpellsCausingAura(SPELL_AURA_MOD_STEALTH);
        RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);

        if(pVictim != this)
            RemoveSpellsCausingAura(SPELL_AURA_MOD_INVISIBILITY);

        if(pVictim->GetTypeId() == TYPEID_PLAYER && !pVictim->IsStandState() && !pVictim->hasUnitState(UNIT_STAT_STUNDED))
            pVictim->SetStandState(PLAYER_STATE_NONE);
    }

    //Script Event damage Deal
    if( GetTypeId()== TYPEID_UNIT && ((Creature *)this)->AI())
        ((Creature *)this)->AI()->DamageDeal(pVictim, damage);
    //Script Event damage taken
    if( pVictim->GetTypeId()== TYPEID_UNIT && ((Creature *)pVictim)->AI() )
        ((Creature *)pVictim)->AI()->DamageTaken(this, damage);

    if(!damage)
    {
        // Rage from physical damage received .
        if(cleanDamage && cleanDamage->damage && (damageSchoolMask & SPELL_SCHOOL_MASK_NORMAL) && pVictim->GetTypeId() == TYPEID_PLAYER && (pVictim->getPowerType() == POWER_RAGE))
            ((Player*)pVictim)->RewardRage(cleanDamage->damage, 0, false);

        return 0;
    }

    pVictim->RemoveSpellbyDamageTaken(SPELL_AURA_MOD_FEAR, damage);
    // root type spells do not dispell the root effect
    if(!spellProto || spellProto->Mechanic != MECHANIC_ROOT)
        pVictim->RemoveSpellbyDamageTaken(SPELL_AURA_MOD_ROOT, damage);

    if(pVictim->GetTypeId() != TYPEID_PLAYER)
    {
        //pVictim->SetInFront(this);
        // no xp,health if type 8 /critters/
        if ( pVictim->GetCreatureType() == CREATURE_TYPE_CRITTER)
        {
            pVictim->setDeathState(JUST_DIED);
            pVictim->SetHealth(0);

            // allow loot only if has loot_id in creature_template
            CreatureInfo const* cInfo = ((Creature*)pVictim)->GetCreatureInfo();
            if(cInfo && cInfo->lootid)
                pVictim->SetUInt32Value(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE);

            // some critters required for quests
            if(GetTypeId() == TYPEID_PLAYER)
                ((Player*)this)->KilledMonster(pVictim->GetEntry(),pVictim->GetGUID());

            return damage;
        }

        if(!pVictim->isInCombat() && ((Creature*)pVictim)->AI())
            ((Creature*)pVictim)->AI()->AttackStart(this);
    }

    DEBUG_LOG("DealDamageStart");

    uint32 health = pVictim->GetHealth();
    sLog.outDetail("deal dmg:%d to health:%d ",damage,health);

    // duel ends when player has 1 or less hp
    bool duel_hasEnded = false;
    if(pVictim->GetTypeId() == TYPEID_PLAYER && ((Player*)pVictim)->duel && damage >= (health-1))
    {
        // prevent kill only if killed in duel and killed by opponent or opponent controlled creature
        if(((Player*)pVictim)->duel->opponent==this || ((Player*)pVictim)->duel->opponent->GetGUID() == GetOwnerGUID())
            damage = health-1;

        duel_hasEnded = true;
    }
    //Get in CombatState
    if(pVictim != this && damagetype != DOT)
    {
        SetInCombat();
        pVictim->SetInCombat();
    }

    // Rage from Damage made (only from direct weapon damage)
    if( cleanDamage && damagetype==DIRECT_DAMAGE && this != pVictim && GetTypeId() == TYPEID_PLAYER && (getPowerType() == POWER_RAGE))
    {
        uint32 weaponSpeedHitFactor;

        switch(cleanDamage->attackType)
        {
            case BASE_ATTACK:
            {
                if(cleanDamage->hitOutCome == MELEE_HIT_CRIT)
                    weaponSpeedHitFactor = uint32(GetAttackTime(cleanDamage->attackType)/1000.0f * 7);
                else
                    weaponSpeedHitFactor = uint32(GetAttackTime(cleanDamage->attackType)/1000.0f * 3.5f);

                ((Player*)this)->RewardRage(damage, weaponSpeedHitFactor, true);

                break;
            }
            case OFF_ATTACK:
            {
                if(cleanDamage->hitOutCome == MELEE_HIT_CRIT)
                    weaponSpeedHitFactor = uint32(GetAttackTime(cleanDamage->attackType)/1000.0f * 3.5f);
                else
                    weaponSpeedHitFactor = uint32(GetAttackTime(cleanDamage->attackType)/1000.0f * 1.75f);

                ((Player*)this)->RewardRage(damage, weaponSpeedHitFactor, true);

                break;
            }
            case RANGED_ATTACK:
                break;
        }
    }

    if(pVictim->GetTypeId() == TYPEID_PLAYER && GetTypeId() == TYPEID_PLAYER)
    {
        if(((Player*)pVictim)->InBattleGround())
        {
            Player *killer = ((Player*)this);
            if(killer != ((Player*)pVictim))
                if(BattleGround *bg = killer->GetBattleGround())
                    bg->UpdatePlayerScore(killer, SCORE_DAMAGE_DONE, damage);
        }
    }

    if (health <= damage)
    {
        // battleground things
        if(pVictim->GetTypeId() == TYPEID_PLAYER && (((Player*)pVictim)->InBattleGround()))
        {
            Player *killed = ((Player*)pVictim);
            Player *killer = NULL;
            if(GetTypeId() == TYPEID_PLAYER)
                killer = ((Player*)this);
            else if(GetTypeId() == TYPEID_UNIT && ((Creature*)this)->isPet())
            {
                Unit *owner = GetOwner();
                if(owner && owner->GetTypeId() == TYPEID_PLAYER)
                    killer = ((Player*)owner);
            }

            if(killer)
                if(BattleGround *bg = killed->GetBattleGround())
                    bg->HandleKillPlayer(killed, killer);   // drop flags and etc
        }

        DEBUG_LOG("DealDamage: victim just died");

        // find player: owner of controlled `this` or `this` itself maybe
        Player *player = NULL;
        if(GetCharmerOrOwnerGUID())                         // Pet or timed creature, or player
        {
            if(Unit* owner = GetCharmerOrOwner())
                if(owner->GetTypeId() == TYPEID_PLAYER)
                    player = (Player*)owner;
        }
        else if(GetTypeId() == TYPEID_PLAYER)               // not controlled player
            player = (Player*)this;

        // Reward player, his pets, and group/raid members
        // call kill spell proc event (before real die and combat stop to triggering auras removed at death/combat stop)
        if(player && player!=pVictim)
            if(player->RewardPlayerAndGroupAtKill(pVictim))
                player->ProcDamageAndSpell(pVictim,PROC_FLAG_KILL_XP_GIVER,PROC_FLAG_NONE);

        DEBUG_LOG("DealDamageAttackStop");

        // stop combat
        pVictim->CombatStop();
        pVictim->getHostilRefManager().deleteReferences();

        // clear combat state for killer dependent units (owner/pets)
        if(player)
        {
            if(player==this)                                // not controlled player
            {
                if(Unit* pet = GetPet())
                    pet->ClearInCombat();

                if(Unit* pet = GetCharm())
                    pet->ClearInCombat();
            }
            else                                            // Pet or timed creature, or player
                player->ClearInCombat();
        }

        // if talent known but not triggered (check priest class for speedup check)
        Aura* spiritOfRedemtionTalentReady = NULL;
        if( (!spellProto || spellProto->Id != 27795 ) &&    // not called from SPELL_AURA_SPIRIT_OF_REDEMPTION
            pVictim->GetTypeId()==TYPEID_PLAYER && pVictim->getClass()==CLASS_PRIEST )
        {
            AuraList const& vDummyAuras = pVictim->GetAurasByType(SPELL_AURA_DUMMY);
            for(AuraList::const_iterator itr = vDummyAuras.begin(); itr != vDummyAuras.end(); ++itr)
            {
                if((*itr)->GetSpellProto()->SpellIconID==1654)
                {
                    spiritOfRedemtionTalentReady = *itr;
                    break;
                }
            }
        }

        DEBUG_LOG("SET JUST_DIED");
        if(!spiritOfRedemtionTalentReady)
            pVictim->setDeathState(JUST_DIED);

        DEBUG_LOG("DealDamageHealth1");

        if(spiritOfRedemtionTalentReady)
        {
            // save value before aura remove
            uint32 ressSpellId = pVictim->GetUInt32Value(PLAYER_SELF_RES_SPELL);
            if(!ressSpellId)
                ressSpellId = ((Player*)pVictim)->GetResurrectionSpellId();

            //Remove all expected to remove at death auras (most important negative case like DoT or periodic triggers)
            pVictim->RemoveAllAurasOnDeath();

            // restore for use at real death
            pVictim->SetUInt32Value(PLAYER_SELF_RES_SPELL,ressSpellId);

            // FORM_SPIRITOFREDEMPTION and related auras
            pVictim->CastSpell(pVictim,27827,true,NULL,spiritOfRedemtionTalentReady);
        }
        else
            pVictim->SetHealth(0);

        // Call KilledUnit for creatures
        if (GetTypeId() == TYPEID_UNIT && ((Creature*)this)->AI())
            ((Creature*)this)->AI()->KilledUnit(pVictim);

        // 10% durability loss on death
        // clean InHateListOf
        if (pVictim->GetTypeId() == TYPEID_PLAYER)
        {
            // only if not player and not controlled by player pet. And not at BG
            if (durabilityLoss && !player && !((Player*)pVictim)->InBattleGround())
            {
                DEBUG_LOG("We are dead, loosing 10 percents durability");
                ((Player*)pVictim)->DurabilityLossAll(0.10);
                // durability lost message
                WorldPacket data(SMSG_DURABILITY_DAMAGE_DEATH, 0);
                ((Player*)pVictim)->GetSession()->SendPacket(&data);
            }

            Pet *pet = pVictim->GetPet();
            if(pet && pVictim->GetTypeId() != TYPEID_PLAYER)
            {
                pet->setDeathState(JUST_DIED);
                pet->SetHealth(0);
                pet->addUnitState(UNIT_STAT_DIED);
                pet->getHostilRefManager().deleteReferences();
            }
        }
        else                                                // creature died
        {
            DEBUG_LOG("DealDamageNotPlayer");

            if(!((Creature*)pVictim)->isPet())
            {
                pVictim->DeleteThreatList();
                pVictim->SetUInt32Value(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE);
            }
            // Call creature just died function
            if (((Creature*)pVictim)->AI())
                ((Creature*)pVictim)->AI()->JustDied(this);
        }

        // last damage from non duel opponent or opponent controlled creature
        if(duel_hasEnded)
        {
            assert(pVictim->GetTypeId()==TYPEID_PLAYER);
            Player *he = (Player*)pVictim;

            assert(he->duel);

            CombatStop();                                   // for case killed by pet
            if (IsNonMeleeSpellCasted(true))
                InterruptNonMeleeSpells(true);
            if(he->duel->opponent!=this)
            {
                he->duel->opponent->CombatStop();
                if(he->duel->opponent->IsNonMeleeSpellCasted(true))
                    he->duel->opponent->InterruptNonMeleeSpells(true);
            }
            he->CombatStop();
            if(he->IsNonMeleeSpellCasted(true))
                he->InterruptNonMeleeSpells(true);

            he->DuelComplete(DUEL_INTERUPTED);
        }
    }
    else                                                    // if (health <= damage)
    {
        DEBUG_LOG("DealDamageAlive");

        pVictim->ModifyHealth(- (int32)damage);

        // Check if health is below 20% (apply damage before to prevent case when after ProcDamageAndSpell health < damage
        if(pVictim->GetHealth()*5 < pVictim->GetMaxHealth())
        {
            uint32 procVictim = PROC_FLAG_NONE;

            // if just dropped below 20% (for CheatDeath)
            if((pVictim->GetHealth()+damage)*5 > pVictim->GetMaxHealth())
                procVictim = PROC_FLAG_LOW_HEALTH;

            ProcDamageAndSpell(pVictim,PROC_FLAG_TARGET_LOW_HEALTH,procVictim);
        }

        if(damagetype != DOT)
        {
            if(getVictim())
            {
                // if have target and damage pVictim just call AI recation
                if(pVictim != getVictim() && pVictim->GetTypeId()==TYPEID_UNIT && ((Creature*)pVictim)->AI())
                    ((Creature*)pVictim)->AI()->AttackedBy(this);
            }
            else
            {
                // if not have main target then attack state with target (including AI call)
                //start melee attacks only after melee hit
                Attack(pVictim,(damagetype == DIRECT_DAMAGE));
            }
        }

        // polymorphed and other negative transformed cases
        if(pVictim->getTransForm() && pVictim->hasUnitState(UNIT_STAT_CONFUSED))
            pVictim->RemoveAurasDueToSpell(pVictim->getTransForm());
        
        if(damagetype == DIRECT_DAMAGE|| damagetype == SPELL_DIRECT_DAMAGE)
            pVictim->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_DIRECT_DAMAGE);

        if (pVictim->GetTypeId() != TYPEID_PLAYER)
        {
            if(spellProto && IsDamageToThreatSpell(spellProto))
                pVictim->AddThreat(this, damage*2, damageSchoolMask, spellProto);
            else
                pVictim->AddThreat(this, damage, damageSchoolMask, spellProto);
        }
        else                                                // victim is a player
        {
            // Rage from damage received
            if(this != pVictim && pVictim->getPowerType() == POWER_RAGE)
            {
                uint32 rage_damage = damage + (cleanDamage ? cleanDamage->damage : 0);
                ((Player*)pVictim)->RewardRage(rage_damage, 0, false);
            }

            // random durability for items (HIT)
            if (urand(0,300) == 10)
            {
                DEBUG_LOG("HIT: We decrease durability with 5 percent");
                ((Player*)pVictim)->DurabilityLossAll(0.05f);
            }
        }

        // TODO: Store auras by interrupt flag to speed this up.
        AuraMap& vAuras = pVictim->GetAuras();
        for (AuraMap::iterator i = vAuras.begin(), next; i != vAuras.end(); i = next)
        {
            const SpellEntry *se = i->second->GetSpellProto();
            next = i; ++next;
            if( se->AuraInterruptFlags & AURA_INTERRUPT_FLAG_DAMAGE )
            {
                bool remove = true;
                if (se->procFlags & (1<<3))
                {
                    if (!roll_chance_i(se->procChance))
                        remove = false;
                }
                if (remove)
                {
                    pVictim->RemoveAurasDueToSpell(i->second->GetId());
                    // FIXME: this may cause the auras with proc chance to be rerolled several times
                    next = vAuras.begin();
                }
            }
        }

        if (damagetype != NODAMAGE)
        {
            if(pVictim->m_currentSpells[CURRENT_CHANNELED_SPELL] && pVictim->GetTypeId() == TYPEID_PLAYER && damage)
            {
                if (pVictim->m_currentSpells[CURRENT_CHANNELED_SPELL]->getState() == SPELL_STATE_CASTING)
                {
                    uint32 channelInterruptFlags = pVictim->m_currentSpells[CURRENT_CHANNELED_SPELL]->m_spellInfo->ChannelInterruptFlags;
                    if( channelInterruptFlags & CHANNEL_FLAG_DELAY )
                    {
                        if(pVictim!=this)                   //don't shorten the duration of channeling if you damage yourself
                        {
                            int32 delay = int32(0.25f * GetSpellDuration(pVictim->m_currentSpells[CURRENT_CHANNELED_SPELL]->m_spellInfo));
                            sLog.outDetail("Spell %u delayed (%d) at damage!",pVictim->m_currentSpells[CURRENT_CHANNELED_SPELL]->m_spellInfo->Id,delay);
                            pVictim->m_currentSpells[CURRENT_CHANNELED_SPELL]->DelayedChannel(delay);
                        }
                    }
                    else if( (channelInterruptFlags & (CHANNEL_FLAG_DAMAGE | CHANNEL_FLAG_DAMAGE2)) )
                    {
                        sLog.outDetail("Spell %u canceled at damage!",pVictim->m_currentSpells[CURRENT_CHANNELED_SPELL]->m_spellInfo->Id);
                        pVictim->InterruptSpell(CURRENT_CHANNELED_SPELL);
                    }
                }
                else if (pVictim->m_currentSpells[CURRENT_CHANNELED_SPELL]->getState() == SPELL_STATE_DELAYED)
                    // break channeled spell in delayed state on damage
                {
                    sLog.outDetail("Spell %u canceled at damage!",pVictim->m_currentSpells[CURRENT_CHANNELED_SPELL]->m_spellInfo->Id);
                    pVictim->InterruptSpell(CURRENT_CHANNELED_SPELL);
                }
            }
        }

        // last damage from duel opponent
        if(duel_hasEnded)
        {
            assert(pVictim->GetTypeId()==TYPEID_PLAYER);
            Player *he = (Player*)pVictim;

            assert(he->duel);

            he->SetHealth(1);
            CombatStop();                                   // for case killed by pet
            if(he->duel->opponent!=this)
                he->duel->opponent->CombatStop();
            he->CombatStop();

            he->CastSpell(he, 7267, true);                  // beg
            he->DuelComplete(DUEL_WON);
        }
    }

    DEBUG_LOG("DealDamageEnd returned %d damage", damage);

    return damage;
}

void Unit::CastStop(uint32 except_spellid)
{
    for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL; i++)
        if (m_currentSpells[i] && m_currentSpells[i]->m_spellInfo->Id!=except_spellid)
            InterruptSpell(i);
}

void Unit::CastSpell(Unit* Victim, uint32 spellId, bool triggered, Item *castItem, Aura* triggredByAura, uint64 originalCaster)
{
    SpellEntry const *spellInfo = sSpellStore.LookupEntry(spellId );

    if(!spellInfo)
    {
        sLog.outError("CastSpell: unknown spell id %i by caster: %s %u)", spellId,(GetTypeId()==TYPEID_PLAYER ? "player (GUID:" : "creature (Entry:"),(GetTypeId()==TYPEID_PLAYER ? GetGUIDLow() : GetEntry()));
        return;
    }

    CastSpell(Victim,spellInfo,triggered,castItem,triggredByAura, originalCaster);
}

void Unit::CastSpell(Unit* Victim,SpellEntry const *spellInfo, bool triggered, Item *castItem, Aura* triggredByAura, uint64 originalCaster)
{
    if(!spellInfo)
    {
        sLog.outError("CastSpell: unknown spell by caster: %s %u)", (GetTypeId()==TYPEID_PLAYER ? "player (GUID:" : "creature (Entry:"),(GetTypeId()==TYPEID_PLAYER ? GetGUIDLow() : GetEntry()));
        return;
    }

    if (castItem)
        DEBUG_LOG("WORLD: cast Item spellId - %i", spellInfo->Id);

    if(!originalCaster && triggredByAura)
        originalCaster = triggredByAura->GetCasterGUID();

    Spell *spell = new Spell(this, spellInfo, triggered, originalCaster );

    SpellCastTargets targets;
    targets.setUnitTarget( Victim );
    spell->m_CastItem = castItem;
    spell->prepare(&targets, triggredByAura);
}

void Unit::CastCustomSpell(Unit* Victim,uint32 spellId, int32 const* bp0, int32 const* bp1, int32 const* bp2, bool triggered, Item *castItem, Aura* triggredByAura, uint64 originalCaster)
{
    SpellEntry const *spellInfo = sSpellStore.LookupEntry(spellId );

    if(!spellInfo)
    {
        sLog.outError("CastCustomSpell: unknown spell id %i\n", spellId);
        return;
    }

    CastCustomSpell(Victim,spellInfo,bp0,bp1,bp2,triggered,castItem,triggredByAura, originalCaster);
}

void Unit::CastCustomSpell(Unit* Victim,SpellEntry const *spellInfo, int32 const* bp0, int32 const* bp1, int32 const* bp2, bool triggered, Item *castItem, Aura* triggredByAura, uint64 originalCaster)
{
    if(!spellInfo)
    {
        sLog.outError("CastCustomSpell: unknown spell");
        return;
    }

    if (castItem)
        DEBUG_LOG("WORLD: cast Item spellId - %i", spellInfo->Id);

    if(!originalCaster && triggredByAura)
        originalCaster = triggredByAura->GetCasterGUID();

    Spell *spell = new Spell(this, spellInfo, triggered, originalCaster);

    if(bp0)
        spell->m_currentBasePoints[0] = *bp0-int32(spellInfo->EffectBaseDice[0]);

    if(bp1)
        spell->m_currentBasePoints[1] = *bp1-int32(spellInfo->EffectBaseDice[1]);

    if(bp2)
        spell->m_currentBasePoints[2] = *bp2-int32(spellInfo->EffectBaseDice[2]);

    SpellCastTargets targets;
    targets.setUnitTarget( Victim );
    spell->m_CastItem = castItem;
    spell->prepare(&targets, triggredByAura);
}

void Unit::DealFlatDamage(Unit *pVictim, SpellEntry const *spellInfo, uint32 *damage, CleanDamage *cleanDamage, bool *crit, bool isTriggeredSpell)
{
    // TODO this in only generic way, check for exceptions
    DEBUG_LOG("DealFlatDamage (BEFORE) >> DMG:%u", *damage);

    // Per-damage calss calculation
    switch (spellInfo->DmgClass)
    {
        // Melee and Ranged Spells 
        case SPELL_DAMAGE_CLASS_RANGED:
        case SPELL_DAMAGE_CLASS_MELEE:
        {
            // Calculate physical outcome
            MeleeHitOutcome outcome = RollPhysicalOutcomeAgainst(pVictim, BASE_ATTACK, spellInfo);

            //Used to store the Hit Outcome
            cleanDamage->hitOutCome = outcome;

            // Return miss/evade first (sends miss message)
            switch(outcome)
            {
                case MELEE_HIT_EVADE:
                {
                    SendAttackStateUpdate(HITINFO_MISS, pVictim, 1, GetSpellSchoolMask(spellInfo), 0, 0,0,VICTIMSTATE_EVADES,0);
                    *damage = 0;
                    return;
                }
                case MELEE_HIT_MISS:
                {
                    SendAttackStateUpdate(HITINFO_MISS, pVictim, 1, GetSpellSchoolMask(spellInfo), 0, 0,0,VICTIMSTATE_NORMAL,0);
                    *damage = 0;

                    if(GetTypeId()== TYPEID_PLAYER)
                        ((Player*)this)->UpdateWeaponSkill(BASE_ATTACK);

                    CastMeleeProcDamageAndSpell(pVictim,0,BASE_ATTACK,MELEE_HIT_MISS,spellInfo,isTriggeredSpell);
                    return;
                }
            }

            //  Hitinfo, Victimstate
            uint32 hitInfo = HITINFO_NORMALSWING;
            VictimState victimState = VICTIMSTATE_NORMAL;

            // Physical Damage
            if ( GetSpellSchoolMask(spellInfo) & SPELL_SCHOOL_MASK_NORMAL )
            {
                uint32 modDamage=*damage;

                // apply spellmod to Done damage
                if(Player* modOwner = GetSpellModOwner())
                    modOwner->ApplySpellMod(spellInfo->Id, SPELLMOD_DAMAGE, *damage);

                //Calculate armor mitigation
                uint32 damageAfterArmor = CalcArmorReducedDamage(pVictim, *damage);
                cleanDamage->damage += *damage - damageAfterArmor;
                *damage = damageAfterArmor;
            } 
            // Magical Damage
            else 
            {
                // Calculate damage bonus
                *damage = SpellDamageBonus(pVictim, spellInfo, *damage, SPELL_DIRECT_DAMAGE);
            }

            // Classify outcome
            switch (outcome)
            {
                case MELEE_HIT_BLOCK_CRIT:
                case MELEE_HIT_CRIT:
                {
                    uint32 bonusDmg = *damage;

                    // Apply crit_damage bonus
                    if(Player* modOwner = GetSpellModOwner())
                        modOwner->ApplySpellMod(spellInfo->Id, SPELLMOD_CRIT_DAMAGE_BONUS, bonusDmg);

                    uint32 creatureTypeMask = pVictim->GetCreatureTypeMask();
                    AuraList const& mDamageDoneVersus = GetAurasByType(SPELL_AURA_MOD_CRIT_PERCENT_VERSUS);
                    for(AuraList::const_iterator i = mDamageDoneVersus.begin();i != mDamageDoneVersus.end(); ++i)
                        if(creatureTypeMask & uint32((*i)->GetModifier()->m_miscvalue))
                            bonusDmg = uint32(bonusDmg * ((*i)->GetModifier()->m_amount+100.0f)/100.0f);

                    *damage += bonusDmg;

                    // Resilience - reduce crit damage
                    if (pVictim->GetTypeId()==TYPEID_PLAYER)
                    {
                        uint32 resilienceReduction = ((Player*)pVictim)->GetMeleeCritDamageReduction(*damage);
                        cleanDamage->damage += resilienceReduction;
                        *damage -=  resilienceReduction;
                    }

                    *crit = true;
                    hitInfo |= HITINFO_CRITICALHIT;

                    ModifyAuraState(AURA_STATE_CRIT, true);
                    StartReactiveTimer( REACTIVE_CRIT );

                    if(getClass()==CLASS_HUNTER)
                    {
                        ModifyAuraState(AURA_STATE_HUNTER_CRIT_STRIKE, true);
                        StartReactiveTimer( REACTIVE_HUNTER_CRIT );
                    }

                    if ( outcome == MELEE_HIT_BLOCK_CRIT )
                    {
                        uint32 blocked_amount = uint32(pVictim->GetShieldBlockValue());
                        if (blocked_amount >= *damage)
                        {
                            hitInfo |= HITINFO_SWINGNOHITSOUND;
                            victimState = VICTIMSTATE_BLOCKS;
                            cleanDamage->damage += *damage; // To Help Calculate Rage
                            *damage = 0;
                        }
                        else
                        {
                            // To Help Calculate Rage
                            cleanDamage->damage += blocked_amount;
                            *damage = *damage - blocked_amount;
                        }

                        pVictim->ModifyAuraState(AURA_STATE_DEFENSE, true);
                        pVictim->StartReactiveTimer( REACTIVE_DEFENSE );
                    }
                    break;
                }
                case MELEE_HIT_PARRY:
                {
                    cleanDamage->damage += *damage;         // To Help Calculate Rage
                    *damage = 0;
                    victimState = VICTIMSTATE_PARRY;

                    // Counter-attack ( explained in Unit::DoAttackDamage() )
                    {
                        // Get attack timers
                        float offtime  = float(pVictim->getAttackTimer(OFF_ATTACK));
                        float basetime = float(pVictim->getAttackTimer(BASE_ATTACK));

                        // Reduce attack time
                        if (pVictim->haveOffhandWeapon() && offtime < basetime)
                        {
                            float percent20 = pVictim->GetAttackTime(OFF_ATTACK) * 0.20;
                            float percent60 = 3 * percent20;
                            if(offtime > percent20 && offtime <= percent60)
                            {
                                pVictim->setAttackTimer(OFF_ATTACK, uint32(percent20));
                            }
                            else if(offtime > percent60)
                            {
                                offtime -= 2 * percent20;
                                pVictim->setAttackTimer(OFF_ATTACK, uint32(offtime));
                            }
                        }
                        else
                        {
                            float percent20 = pVictim->GetAttackTime(BASE_ATTACK) * 0.20;
                            float percent60 = 3 * percent20;
                            if(basetime > percent20 && basetime <= percent60)
                            {
                                pVictim->setAttackTimer(BASE_ATTACK, uint32(percent20));
                            }
                            else if(basetime > percent60)
                            {
                                basetime -= 2 * percent20;
                                pVictim->setAttackTimer(BASE_ATTACK, uint32(basetime));
                            }
                        }
                    }

                    // Update victim defense ?
                    if(pVictim->GetTypeId() == TYPEID_PLAYER)
                        ((Player*)pVictim)->UpdateDefense();

                    // Set parry flags
                    pVictim->HandleEmoteCommand(EMOTE_ONESHOT_PARRYUNARMED);

                    // Mongoose bite - set only Counterattack here
                    if (pVictim->getClass() == CLASS_HUNTER)
                    {
                        pVictim->ModifyAuraState(AURA_STATE_HUNTER_PARRY,true);
                        pVictim->StartReactiveTimer( REACTIVE_HUNTER_PARRY );
                    }
                    else
                    {
                        pVictim->ModifyAuraState(AURA_STATE_DEFENSE, true);
                        pVictim->StartReactiveTimer( REACTIVE_DEFENSE );
                    }
                    break;
                }
                case MELEE_HIT_DODGE:
                {
                    if(pVictim->GetTypeId() == TYPEID_PLAYER)
                        ((Player*)pVictim)->UpdateDefense();

                    cleanDamage->damage += *damage;         // To Help Calculate Rage
                    *damage = 0;
                    hitInfo |= HITINFO_SWINGNOHITSOUND;
                    victimState = VICTIMSTATE_DODGE;

                    // Set dodge flags
                    pVictim->HandleEmoteCommand(EMOTE_ONESHOT_PARRYUNARMED);

                    // Overpower
                    if (GetTypeId() == TYPEID_PLAYER && getClass() == CLASS_WARRIOR)
                    {
                        ((Player*)this)->AddComboPoints(pVictim, 1);
                        StartReactiveTimer( REACTIVE_OVERPOWER );
                    }

                    // Riposte
                    if (pVictim->getClass() != CLASS_ROGUE)
                    {
                        pVictim->ModifyAuraState(AURA_STATE_DEFENSE, true);
                        pVictim->StartReactiveTimer( REACTIVE_DEFENSE );
                    }
                    break;
                }
                case MELEE_HIT_BLOCK:
                {
                    uint32 blocked_amount = uint32(pVictim->GetShieldBlockValue());
                    if (blocked_amount >= *damage)
                    {
                        hitInfo |= HITINFO_SWINGNOHITSOUND;
                        victimState = VICTIMSTATE_BLOCKS;
                        cleanDamage->damage += *damage;     // To Help Calculate Rage
                        *damage = 0;
                    }
                    else
                    {
                        // To Help Calculate Rage
                        cleanDamage->damage += blocked_amount;
                        *damage = *damage - blocked_amount;
                    }

                    pVictim->ModifyAuraState(AURA_STATE_DEFENSE, true);
                    pVictim->StartReactiveTimer( REACTIVE_DEFENSE );

                    break;

                }
                case MELEE_HIT_EVADE:                       // already processed early
                case MELEE_HIT_MISS:                        // already processed early
                case MELEE_HIT_GLANCING:
                case MELEE_HIT_CRUSHING:
                case MELEE_HIT_NORMAL:
                    break;
            }

            // do all damage=0 cases here
            if(*damage == 0)
                CastMeleeProcDamageAndSpell(pVictim,0,BASE_ATTACK,outcome,spellInfo,isTriggeredSpell);

            break;
        }
        // Magical Attacks
        case SPELL_DAMAGE_CLASS_NONE:
        case SPELL_DAMAGE_CLASS_MAGIC:
        {
            // Calculate damage bonus
            *damage = SpellDamageBonus(pVictim, spellInfo, *damage, SPELL_DIRECT_DAMAGE);

            *crit = isSpellCrit(pVictim, spellInfo, GetSpellSchoolMask(spellInfo), BASE_ATTACK);
            if (*crit)
            {
                *damage = SpellCriticalBonus(spellInfo, *damage, pVictim);

                // Resilience - reduce crit damage
                if (pVictim && pVictim->GetTypeId()==TYPEID_PLAYER)
                {
                    uint32 damage_reduction = ((Player *)pVictim)->GetSpellCritDamageReduction(*damage);
                    if(*damage > damage_reduction)
                        *damage -= damage_reduction;
                    else
                        *damage = 0;
                }

                cleanDamage->hitOutCome = MELEE_HIT_CRIT;
            }
            // spell proc all magic damage==0 case in this function
            if(*damage == 0)
            {
                // Procflags
                uint32 procAttacker = PROC_FLAG_HIT_SPELL;
                uint32 procVictim   = (PROC_FLAG_STRUCK_SPELL|PROC_FLAG_TAKE_DAMAGE);

                ProcDamageAndSpell(pVictim, procAttacker, procVictim, 0, spellInfo, isTriggeredSpell);
            }

            break;
        }
    }

    // TODO this in only generic way, check for exceptions
    DEBUG_LOG("DealFlatDamage (AFTER) >> DMG:%u", *damage);
}

uint32 Unit::SpellNonMeleeDamageLog(Unit *pVictim, uint32 spellID, uint32 damage, bool isTriggeredSpell, bool useSpellDamage)
{
    if(!this || !pVictim)
        return 0;
    if(!this->isAlive() || !pVictim->isAlive())
        return 0;

    SpellEntry const *spellInfo = sSpellStore.LookupEntry(spellID);
    if(!spellInfo)
        return 0;

    CleanDamage cleanDamage = CleanDamage(0, BASE_ATTACK, MELEE_HIT_NORMAL);
    bool crit = false;

    if (useSpellDamage)
        DealFlatDamage(pVictim, spellInfo, &damage, &cleanDamage, &crit, isTriggeredSpell);

    // If we actually dealt some damage (spell proc's for 0 damage (normal and magic) called in DealFlatDamage)
    if(damage > 0)
    {
        // Calculate absorb & resists
        uint32 absorb = 0;
        uint32 resist = 0;

        CalcAbsorbResist(pVictim,GetSpellSchoolMask(spellInfo), SPELL_DIRECT_DAMAGE, damage, &absorb, &resist);
        
        //No more damage left, target absorbed and/or resisted all damage
        if (damage > absorb + resist)
            damage -= absorb + resist;      //Remove Absorbed and Resisted from damage actually dealt
        else
        {
            uint32 HitInfo = HITINFO_SWINGNOHITSOUND;

            if (absorb)
                HitInfo |= HITINFO_ABSORB;
            if (resist)
            {
                HitInfo |= HITINFO_RESIST;
                ProcDamageAndSpell(pVictim, PROC_FLAG_TARGET_RESISTS, PROC_FLAG_RESIST_SPELL, 0, spellInfo,isTriggeredSpell);
            }

            //Send resist
            SendAttackStateUpdate(HitInfo, pVictim, 1, GetSpellSchoolMask(spellInfo), damage, absorb,resist,VICTIMSTATE_NORMAL,0);
            return 0;
        }

        // Deal damage done
        damage = DealDamage(pVictim, damage, &cleanDamage, SPELL_DIRECT_DAMAGE, GetSpellSchoolMask(spellInfo), spellInfo, true);

        // Send damage log
        sLog.outDetail("SpellNonMeleeDamageLog: %u (TypeId: %u) attacked %u (TypeId: %u) for %u dmg inflicted by %u,absorb is %u,resist is %u",
            GetGUIDLow(), GetTypeId(), pVictim->GetGUIDLow(), pVictim->GetTypeId(), damage, spellID, absorb,resist);

        // Actual log sent to client
        SendSpellNonMeleeDamageLog(pVictim, spellID, damage, GetSpellSchoolMask(spellInfo), absorb, resist, false, 0, crit);
        
        // Procflags
        uint32 procAttacker = PROC_FLAG_HIT_SPELL;
        uint32 procVictim   = (PROC_FLAG_STRUCK_SPELL|PROC_FLAG_TAKE_DAMAGE);

        if (crit)
        {
            procAttacker |= PROC_FLAG_CRIT_SPELL;
            procVictim   |= PROC_FLAG_STRUCK_CRIT_SPELL;
        }

        ProcDamageAndSpell(pVictim, procAttacker, procVictim, damage, spellInfo, isTriggeredSpell);

        return damage;
    }
    else
    {
        // all spell proc for 0 normal and magic damage called in DealFlatDamage

        //Check for rage
        if(cleanDamage.damage)
            // Rage from damage received.
            if(pVictim->GetTypeId() == TYPEID_PLAYER && (pVictim->getPowerType() == POWER_RAGE))
                ((Player*)pVictim)->RewardRage(cleanDamage.damage, 0, false);

        return 0;
    }
}

void Unit::HandleEmoteCommand(uint32 anim_id)
{
    WorldPacket data( SMSG_EMOTE, 12 );
    data << anim_id << GetGUID();
    WPAssert(data.size() == 12);

    SendMessageToSet(&data, true);
}

uint32 Unit::CalcArmorReducedDamage(Unit* pVictim, const uint32 damage)
{
    uint32 newdamage = 0;
    float armor = pVictim->GetArmor();
    // Ignore enemy armor by SPELL_AURA_MOD_TARGET_RESISTANCE aura
    armor += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_TARGET_RESISTANCE, SPELL_SCHOOL_MASK_NORMAL);

    if (armor<0.0f) armor=0.0f;

    float tmpvalue = 0.0f;
    if(getLevel() <= 59)                                    //Level 1-59
        tmpvalue = armor / (armor + 400.0f + 85.0f * getLevel());
    else if(getLevel() < 70)                                //Level 60-69
        tmpvalue = armor / (armor - 22167.5f + 467.5f * getLevel());
    else                                                    //Level 70+
        tmpvalue = armor / (armor + 10557.5f);

    if(tmpvalue < 0.0f)
        tmpvalue = 0.0f;
    if(tmpvalue > 0.75f)
        tmpvalue = 0.75f;
    newdamage = uint32(damage - (damage * tmpvalue));

    return (newdamage > 1) ? newdamage : 1;
}

void Unit::CalcAbsorbResist(Unit *pVictim,SpellSchoolMask schoolMask, DamageEffectType damagetype, const uint32 damage, uint32 *absorb, uint32 *resist)
{
    if(!pVictim || !pVictim->isAlive() || !damage)
        return;

    // Magic damage, check for resists
    if ((schoolMask & SPELL_SCHOOL_MASK_NORMAL)==0)
    {
        // Get base victim resistance for school
        float tmpvalue2 = (float)pVictim->GetResistance(GetFirstSchoolInMask(schoolMask));
        // Ignore resistance by self SPELL_AURA_MOD_TARGET_RESISTANCE aura
        tmpvalue2 += (float)GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_TARGET_RESISTANCE, schoolMask);

        tmpvalue2 *= (float)(0.15f / getLevel());
        if (tmpvalue2 < 0.0f)
            tmpvalue2 = 0.0f;
        if (tmpvalue2 > 0.75f)
            tmpvalue2 = 0.75f;
        uint32 ran = urand(0, 100);
        uint32 faq[4] = {24,6,4,6};
        uint8 m = 0;
        float Binom = 0.0f;
        for (uint8 i = 0; i < 4; i++)
        {
            Binom += 2400 *( powf(tmpvalue2, i) * powf( (1-tmpvalue2), (4-i)))/faq[i];
            if (ran > Binom )
                ++m;
            else
                break;
        }
        if (damagetype == DOT && m == 4)
            *resist += uint32(damage - 1);
        else
            *resist += uint32(damage * m / 4);
        if(*resist > damage)
            *resist = damage;
    }
    else
        *resist = 0;

    int32 RemainingDamage = damage - *resist;

    // absorb without mana cost
    int32 reflectDamage = 0;
    Aura* reflectAura = NULL;
    AuraList const& vSchoolAbsorb = pVictim->GetAurasByType(SPELL_AURA_SCHOOL_ABSORB);
    for(AuraList::const_iterator i = vSchoolAbsorb.begin(), next; i != vSchoolAbsorb.end() && RemainingDamage > 0; i = next)
    {
        next = i; ++next;

        if (((*i)->GetModifier()->m_miscvalue & schoolMask)==0)
            continue;

        // Cheat Death
        if((*i)->GetSpellProto()->SpellFamilyName==SPELLFAMILY_ROGUE && (*i)->GetSpellProto()->SpellIconID == 2109)
        {
            if (((Player*)pVictim)->HasSpellCooldown(31231))
                continue;
            if (pVictim->GetHealth() <= RemainingDamage)
            {
                int32 chance = (*i)->GetModifier()->m_amount;
                if (roll_chance_i(chance))
                {
                    pVictim->CastSpell(pVictim,31231,true);
                    ((Player*)pVictim)->AddSpellCooldown(31231,0,time(NULL)+60);
                    RemainingDamage = 0;
                }
            }
            continue;
        }

        int32 currentAbsorb;

        //Reflective Shield
        if ((pVictim != this) && (*i)->GetSpellProto()->SpellFamilyName == SPELLFAMILY_PRIEST && (*i)->GetSpellProto()->SpellFamilyFlags == 0x1)
        {
            if(Unit* caster = (*i)->GetCaster())
            {
                AuraList const& vOverRideCS = caster->GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
                for(AuraList::const_iterator k = vOverRideCS.begin(); k != vOverRideCS.end(); ++k)
                {
                    switch((*k)->GetModifier()->m_miscvalue)
                    {
                        case 5065:                          // Rank 1
                        case 5064:                          // Rank 2
                        case 5063:                          // Rank 3
                        case 5062:                          // Rank 4
                        case 5061:                          // Rank 5
                        {
                            if(RemainingDamage >= (*i)->GetModifier()->m_amount)
                                reflectDamage = (*i)->GetModifier()->m_amount * (*k)->GetModifier()->m_amount/100;
                            else
                                reflectDamage = (*k)->GetModifier()->m_amount * RemainingDamage/100;
                            reflectAura = *i;

                        } break;
                        default: break;
                    }

                    if(reflectDamage)
                        break;
                }
            }
        }

        if (RemainingDamage >= (*i)->GetModifier()->m_amount)
        {
            currentAbsorb = (*i)->GetModifier()->m_amount;
            pVictim->RemoveAurasDueToSpell((*i)->GetId());
            next = vSchoolAbsorb.begin();
        }
        else
        {
            currentAbsorb = RemainingDamage;
            (*i)->GetModifier()->m_amount -= RemainingDamage;
        }

        RemainingDamage -= currentAbsorb;
    }
    // do not cast spells while looping auras; auras can get invalid otherwise
    if (reflectDamage)
        pVictim->CastCustomSpell(this, 33619, &reflectDamage, NULL, NULL, true, NULL, reflectAura);

    // absorb by mana cost
    AuraList const& vManaShield = pVictim->GetAurasByType(SPELL_AURA_MANA_SHIELD);
    for(AuraList::const_iterator i = vManaShield.begin(), next; i != vManaShield.end() && RemainingDamage > 0; i = next)
    {
        next = i; ++next;

        // check damage school mask
        if(((*i)->GetModifier()->m_miscvalue & schoolMask)==0)
            continue;

        int32 currentAbsorb;
        if (RemainingDamage >= (*i)->GetModifier()->m_amount)
            currentAbsorb = (*i)->GetModifier()->m_amount;
        else
            currentAbsorb = RemainingDamage;

        float manaMultiplier = (*i)->GetSpellProto()->EffectMultipleValue[(*i)->GetEffIndex()];
        if(Player *modOwner = GetSpellModOwner())
            modOwner->ApplySpellMod((*i)->GetId(), SPELLMOD_MULTIPLE_VALUE, manaMultiplier);

        int32 maxAbsorb = int32(pVictim->GetPower(POWER_MANA) / manaMultiplier);
        if (currentAbsorb > maxAbsorb)
            currentAbsorb = maxAbsorb;

        (*i)->GetModifier()->m_amount -= currentAbsorb;
        if((*i)->GetModifier()->m_amount <= 0)
        {
            pVictim->RemoveAurasDueToSpell((*i)->GetId());
            next = vManaShield.begin();
        }

        int32 manaReduction = int32(currentAbsorb * manaMultiplier);
        pVictim->ApplyPowerMod(POWER_MANA, manaReduction, false);

        RemainingDamage -= currentAbsorb;
    }

    AuraList const& vSplitDamageFlat = pVictim->GetAurasByType(SPELL_AURA_SPLIT_DAMAGE_FLAT);
    for(AuraList::const_iterator i = vSplitDamageFlat.begin(), next; i != vSplitDamageFlat.end() && RemainingDamage >= 0; i = next)
    {
        next = i; ++next;

        // check damage school mask
        if(((*i)->GetModifier()->m_miscvalue & schoolMask)==0)
            continue;

        // Damage can be splitted only if aura has an alive caster
        Unit *caster = (*i)->GetCaster();
        if(!caster || caster == pVictim || !caster->IsInWorld() || !caster->isAlive())
            continue;

        int32 currentAbsorb;
        if (RemainingDamage >= (*i)->GetModifier()->m_amount)
            currentAbsorb = (*i)->GetModifier()->m_amount;
        else
            currentAbsorb = RemainingDamage;

        RemainingDamage -= currentAbsorb;

        pVictim->SendSpellNonMeleeDamageLog(caster, (*i)->GetSpellProto()->Id, currentAbsorb, GetSpellSchoolMask((*i)->GetSpellProto()), 0, 0, false, 0, false);

        CleanDamage cleanDamage = CleanDamage(currentAbsorb, BASE_ATTACK, MELEE_HIT_NORMAL);
        pVictim->DealDamage(caster, currentAbsorb, &cleanDamage, DIRECT_DAMAGE, GetSpellSchoolMask((*i)->GetSpellProto()), (*i)->GetSpellProto(), false);
    }

    AuraList const& vSplitDamagePct = pVictim->GetAurasByType(SPELL_AURA_SPLIT_DAMAGE_PCT);
    for(AuraList::const_iterator i = vSplitDamagePct.begin(), next; i != vSplitDamagePct.end() && RemainingDamage >= 0; i = next)
    {
        next = i; ++next;

        // check damage school mask
        if(((*i)->GetModifier()->m_miscvalue & schoolMask)==0)
            continue;

        // Damage can be splitted only if aura has an alive caster
        Unit *caster = (*i)->GetCaster();
        if(!caster || caster == pVictim || !caster->IsInWorld() || !caster->isAlive())
            continue;

        int32 splitted = int32(RemainingDamage * (*i)->GetModifier()->m_amount / 100.0f);

        RemainingDamage -= splitted;

        pVictim->SendSpellNonMeleeDamageLog(caster, (*i)->GetSpellProto()->Id, splitted, GetSpellSchoolMask((*i)->GetSpellProto()), 0, 0, false, 0, false);

        CleanDamage cleanDamage = CleanDamage(splitted, BASE_ATTACK, MELEE_HIT_NORMAL);
        pVictim->DealDamage(caster, splitted, &cleanDamage, DIRECT_DAMAGE, GetSpellSchoolMask((*i)->GetSpellProto()), (*i)->GetSpellProto(), false);
    }

    *absorb = damage - RemainingDamage - *resist;
}

void Unit::DoAttackDamage (Unit *pVictim, uint32 *damage, CleanDamage *cleanDamage, uint32 *blocked_amount, SpellSchoolMask damageSchoolMask, uint32 *hitInfo, VictimState *victimState, uint32 *absorbDamage, uint32 *resistDamage, WeaponAttackType attType, SpellEntry const *spellCasted, bool isTriggeredSpell)
{
    MeleeHitOutcome outcome;

    // If is casted Melee spell, calculate like physical
    if(!spellCasted)
        outcome = RollMeleeOutcomeAgainst (pVictim, attType);
    else
        outcome = RollPhysicalOutcomeAgainst (pVictim, attType, spellCasted);

    switch(outcome)
    {
        case MELEE_HIT_EVADE:
        {
            *hitInfo |= HITINFO_MISS;
            *damage = 0;
            cleanDamage->damage = 0;
            return;
        }
        case MELEE_HIT_MISS:
        {
            *hitInfo |= HITINFO_MISS;
            *damage = 0;
            cleanDamage->damage = 0;
            if(GetTypeId()== TYPEID_PLAYER)
                ((Player*)this)->UpdateWeaponSkill(attType);
            return;
        }
    }

    /// If this is a creature and it attacks from behind it has a probability to daze it's victim
    if( (outcome==MELEE_HIT_CRIT || outcome==MELEE_HIT_CRUSHING || outcome==MELEE_HIT_NORMAL || outcome==MELEE_HIT_GLANCING) &&
        GetTypeId() != TYPEID_PLAYER && !((Creature*)this)->GetCharmerOrOwnerGUID() && !pVictim->HasInArc(M_PI, this) )
    {
        // -probability is between 0% and 40%
        // 20% base chance
        float Probability = 20;

        //there is a newbie protection, at level 10 just 7% base chance; assuming linear function
        if( pVictim->getLevel() < 30 )
            Probability = 0.65f*pVictim->getLevel()+0.5;

        uint32 VictimDefense=pVictim->GetDefenseSkillValue();
        uint32 AttackerMeleeSkill=GetUnitMeleeSkill();

        Probability *= AttackerMeleeSkill/(float)VictimDefense;

        if(Probability > 40.0f)
            Probability = 40.0f;

        if(roll_chance_f(Probability))
            CastSpell(pVictim, 1604, true);
    }

    //Calculate the damage after armor mitigation if SPELL_SCHOOL_NORMAL
    if (damageSchoolMask & SPELL_SCHOOL_MASK_NORMAL)
    {
        uint32 damageAfterArmor = CalcArmorReducedDamage(pVictim, *damage);
        cleanDamage->damage += *damage - damageAfterArmor;
        *damage = damageAfterArmor;
    }

    if(GetTypeId() == TYPEID_PLAYER && pVictim->GetTypeId() != TYPEID_PLAYER && pVictim->GetCreatureType() != CREATURE_TYPE_CRITTER )
        ((Player*)this)->UpdateCombatSkills(pVictim, attType, outcome, false);

    if(GetTypeId() != TYPEID_PLAYER && pVictim->GetTypeId() == TYPEID_PLAYER)
        ((Player*)pVictim)->UpdateCombatSkills(this, attType, outcome, true);

    switch (outcome)
    {
        case MELEE_HIT_BLOCK_CRIT:
        case MELEE_HIT_CRIT:
        {
            //*hitInfo = 0xEA;
            // 0xEA
            *hitInfo  = HITINFO_CRITICALHIT | HITINFO_NORMALSWING2 | 0x8;

            // Crit bonus calc
            uint32 crit_bonus;
            crit_bonus = *damage;

            // Apply crit_damage bonus for melee spells
            if (spellCasted)
            {
                if(Player* modOwner = GetSpellModOwner())
                    modOwner->ApplySpellMod(spellCasted->Id, SPELLMOD_CRIT_DAMAGE_BONUS, crit_bonus);

                uint32 creatureTypeMask = pVictim->GetCreatureTypeMask();
                AuraList const& mDamageDoneVersus = GetAurasByType(SPELL_AURA_MOD_CRIT_PERCENT_VERSUS);
                for(AuraList::const_iterator i = mDamageDoneVersus.begin();i != mDamageDoneVersus.end(); ++i)
                    if(creatureTypeMask & uint32((*i)->GetModifier()->m_miscvalue))
                        crit_bonus = uint32(crit_bonus * ((*i)->GetModifier()->m_amount+100.0f)/100.0f);
            }

            *damage += crit_bonus;

            uint32 resilienceReduction = 0;

            if(attType == RANGED_ATTACK)
            {
                int32 mod = pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_RANGED_CRIT_DAMAGE);
                *damage = int32((*damage) * float((100.0f + mod)/100.0f));
                // Resilience - reduce crit damage
                if (pVictim->GetTypeId()==TYPEID_PLAYER)
                    resilienceReduction = ((Player*)pVictim)->GetRangedCritDamageReduction(*damage);
            }
            else
            {
                int32 mod = pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_MELEE_CRIT_DAMAGE);
                mod += GetTotalAuraModifier(SPELL_AURA_MOD_CRIT_DAMAGE_BONUS_MELEE);
                *damage = int32((*damage) * float((100.0f + mod)/100.0f));
                // Resilience - reduce crit damage
                if (pVictim->GetTypeId()==TYPEID_PLAYER)
                    resilienceReduction = ((Player*)pVictim)->GetMeleeCritDamageReduction(*damage);
            }

            *damage -= resilienceReduction;
            cleanDamage->damage += resilienceReduction;

            if(GetTypeId() == TYPEID_PLAYER && pVictim->GetTypeId() != TYPEID_PLAYER && pVictim->GetCreatureType() != CREATURE_TYPE_CRITTER )
                ((Player*)this)->UpdateWeaponSkill(attType);

            ModifyAuraState(AURA_STATE_CRIT, true);
            StartReactiveTimer( REACTIVE_CRIT );

            if(getClass()==CLASS_HUNTER)
            {
                ModifyAuraState(AURA_STATE_HUNTER_CRIT_STRIKE, true);
                StartReactiveTimer( REACTIVE_HUNTER_CRIT );
            }

            if ( outcome == MELEE_HIT_BLOCK_CRIT )
            {
                *blocked_amount = pVictim->GetShieldBlockValue();

                if (pVictim->GetUnitBlockChance())
                    pVictim->HandleEmoteCommand(EMOTE_ONESHOT_PARRYSHIELD);
                else
                    pVictim->HandleEmoteCommand(EMOTE_ONESHOT_PARRYUNARMED);

                //Only set VICTIMSTATE_BLOCK on a full block
                if (*blocked_amount >= uint32(*damage))
                {
                    *victimState = VICTIMSTATE_BLOCKS;
                    *blocked_amount = uint32(*damage);
                }

                if(pVictim->GetTypeId() == TYPEID_PLAYER)
                    ((Player*)pVictim)->UpdateDefense();

                pVictim->ModifyAuraState(AURA_STATE_DEFENSE,true);
                pVictim->StartReactiveTimer( REACTIVE_DEFENSE );
                break;
            }

            pVictim->HandleEmoteCommand(EMOTE_ONESHOT_WOUNDCRITICAL);
            break;
        }
        case MELEE_HIT_PARRY:
        {
            if(attType == RANGED_ATTACK)                    //range attack - no parry
            {
                outcome = MELEE_HIT_NORMAL;
                break;
            }

            cleanDamage->damage += *damage;
            *damage = 0;
            *victimState = VICTIMSTATE_PARRY;

            // instant (maybe with small delay) counter attack
            {
                float offtime  = float(pVictim->getAttackTimer(OFF_ATTACK));
                float basetime = float(pVictim->getAttackTimer(BASE_ATTACK));

                // after parry nearest next attack time will reduced at %40 from full attack time.
                // The delay cannot be reduced to less than 20% of your weapon�s base swing delay.
                if (pVictim->haveOffhandWeapon() && offtime < basetime)
                {
                    float percent20 = pVictim->GetAttackTime(OFF_ATTACK)*0.20;
                    float percent60 = 3*percent20;
                    // set to 20% if in range 20%...20+40% of full time
                    if(offtime > percent20 && offtime <= percent60)
                    {
                        pVictim->setAttackTimer(OFF_ATTACK,uint32(percent20));
                    }
                    // decrease at %40 from full time
                    else if(offtime > percent60)
                    {
                        offtime -= 2*percent20;
                        pVictim->setAttackTimer(OFF_ATTACK,uint32(offtime));
                    }
                    // ELSE not changed
                }
                else
                {
                    float percent20 = pVictim->GetAttackTime(BASE_ATTACK)*0.20;
                    float percent60 = 3*percent20;
                    // set to 20% if in range 20%...20+40% of full time
                    if(basetime > percent20 && basetime <= percent60)
                    {
                        pVictim->setAttackTimer(BASE_ATTACK,uint32(percent20));
                    }
                    // decrease at %40 from full time
                    else if(basetime > percent60)
                    {
                        basetime -= 2*percent20;
                        pVictim->setAttackTimer(BASE_ATTACK,uint32(basetime));
                    }
                    // ELSE not changed
                }
            }

            if(pVictim->GetTypeId() == TYPEID_PLAYER)
                ((Player*)pVictim)->UpdateDefense();

            pVictim->HandleEmoteCommand(EMOTE_ONESHOT_PARRYUNARMED);

            if (pVictim->getClass() == CLASS_HUNTER)
            {
                pVictim->ModifyAuraState(AURA_STATE_HUNTER_PARRY,true);
                pVictim->StartReactiveTimer( REACTIVE_HUNTER_PARRY );
            }
            else
            {
                pVictim->ModifyAuraState(AURA_STATE_DEFENSE, true);
                pVictim->StartReactiveTimer( REACTIVE_DEFENSE );
            }

            CastMeleeProcDamageAndSpell(pVictim, 0, attType, outcome, spellCasted, isTriggeredSpell);
            return;
        }
        case MELEE_HIT_DODGE:
        {
            if(attType == RANGED_ATTACK)                    //range attack - no dodge
            {
                outcome = MELEE_HIT_NORMAL;
                break;
            }

            cleanDamage->damage += *damage;
            *damage = 0;
            *victimState = VICTIMSTATE_DODGE;

            if(pVictim->GetTypeId() == TYPEID_PLAYER)
                ((Player*)pVictim)->UpdateDefense();

            pVictim->HandleEmoteCommand(EMOTE_ONESHOT_PARRYUNARMED);

            if (pVictim->getClass() != CLASS_ROGUE)         // Riposte
            {
                pVictim->ModifyAuraState(AURA_STATE_DEFENSE, true);
                pVictim->StartReactiveTimer( REACTIVE_DEFENSE );
            }

            // Overpower
            if (GetTypeId() == TYPEID_PLAYER && getClass() == CLASS_WARRIOR)
            {
                ((Player*)this)->AddComboPoints(pVictim, 1);
                StartReactiveTimer( REACTIVE_OVERPOWER );
            }

            CastMeleeProcDamageAndSpell(pVictim, 0, attType, outcome, spellCasted, isTriggeredSpell);
            return;
        }
        case MELEE_HIT_BLOCK:
        {
            *blocked_amount = pVictim->GetShieldBlockValue();

            if (pVictim->GetUnitBlockChance())
                pVictim->HandleEmoteCommand(EMOTE_ONESHOT_PARRYSHIELD);
            else
                pVictim->HandleEmoteCommand(EMOTE_ONESHOT_PARRYUNARMED);

            //Only set VICTIMSTATE_BLOCK on a full block
            if (*blocked_amount >= uint32(*damage))
            {
                *victimState = VICTIMSTATE_BLOCKS;
                *blocked_amount = uint32(*damage);
            }

            if(pVictim->GetTypeId() == TYPEID_PLAYER)
                ((Player*)pVictim)->UpdateDefense();

            pVictim->ModifyAuraState(AURA_STATE_DEFENSE,true);
            pVictim->StartReactiveTimer( REACTIVE_DEFENSE );

            break;
        }
        case MELEE_HIT_GLANCING:
        {
            float reducePercent = 1.0f;                     //damage factor

            // calculate base values and mods
            float baseLowEnd = 1.3;
            float baseHighEnd = 1.2;
            switch(getClass())                              // lowering base values for casters
            {
                case CLASS_SHAMAN:
                case CLASS_PRIEST:
                case CLASS_MAGE:
                case CLASS_WARLOCK:
                case CLASS_DRUID:
                    baseLowEnd  -= 0.7;
                    baseHighEnd -= 0.3;
                    break;
            }

            float maxLowEnd = 0.6;
            switch(getClass())                              // upper for melee classes
            {
                case CLASS_WARRIOR:
                case CLASS_ROGUE:
                    maxLowEnd = 0.91;                       //If the attacker is a melee class then instead the lower value of 0.91
            }

            // calculate values
            int32 diff = int32(pVictim->GetDefenseSkillValue()) - int32(GetWeaponSkillValue(attType));
            float lowEnd  = baseLowEnd - ( 0.05f * diff );
            float highEnd = baseHighEnd - ( 0.03f * diff );

            // apply max/min bounds
            if ( lowEnd < 0.01f )                           //the low end must not go bellow 0.01f
                lowEnd = 0.01f;
            else if ( lowEnd > maxLowEnd )                  //the smaller value of this and 0.6 is kept as the low end
                lowEnd = maxLowEnd;

            if ( highEnd < 0.2f )                           //high end limits
                highEnd = 0.2f;
            if ( highEnd > 0.99f )
                highEnd = 0.99f;

            if(lowEnd > highEnd)                            // prevent negative range size
                lowEnd = highEnd;

            reducePercent = lowEnd + rand_norm() * ( highEnd - lowEnd );

            *damage = uint32(reducePercent * *damage);
            cleanDamage->damage += *damage;
            *hitInfo |= HITINFO_GLANCING;
            break;
        }
        case MELEE_HIT_CRUSHING:
        {
            // 150% normal damage
            *damage += (*damage / 2);
            cleanDamage->damage = *damage;
            *hitInfo |= HITINFO_CRUSHING;
            // TODO: victimState, victim animation?
            break;
        }
        default:
            break;
    }

    // apply melee damage bonus and absorb only if base damage not fully blocked to prevent negative damage or damage with full block
    if(*victimState != VICTIMSTATE_BLOCKS)
    {
        MeleeDamageBonus(pVictim, damage,attType,spellCasted);
        CalcAbsorbResist(pVictim, damageSchoolMask, DIRECT_DAMAGE, *damage-*blocked_amount, absorbDamage, resistDamage);
    }

    if (*absorbDamage) *hitInfo |= HITINFO_ABSORB;
    if (*resistDamage) *hitInfo |= HITINFO_RESIST;

    cleanDamage->damage += *blocked_amount;

    if (*damage <= *absorbDamage + *resistDamage + *blocked_amount)
    {
        //*hitInfo = 0x00010020;
        //*hitInfo |= HITINFO_SWINGNOHITSOUND;
        //*damageType = 0;
        CastMeleeProcDamageAndSpell(pVictim, 0, attType, outcome, spellCasted, isTriggeredSpell);
        return;
    }

    // update at damage Judgement aura duration that applied by attacker at victim
    if(*damage)
    {
        AuraMap const& vAuras = pVictim->GetAuras();
        for(AuraMap::const_iterator itr = vAuras.begin(); itr != vAuras.end(); ++itr)
        {
            SpellEntry const *spellInfo = (*itr).second->GetSpellProto();
            if( (spellInfo->AttributesEx3 & 0x40000) && spellInfo->SpellFamilyName == SPELLFAMILY_PALADIN &&
                ((*itr).second->GetCasterGUID() == GetGUID() && (!spellCasted || spellCasted->Id == 35395)) )
            {
                (*itr).second->SetAuraDuration((*itr).second->GetAuraMaxDuration());
                (*itr).second->UpdateAuraDuration();
            }
        }
    }

    CastMeleeProcDamageAndSpell(pVictim, (*damage - *absorbDamage - *resistDamage - *blocked_amount), attType, outcome, spellCasted, isTriggeredSpell);

    // victim's damage shield
    // yet another hack to fix crashes related to the aura getting removed during iteration
    std::set<Aura*> alreadyDone;
    uint32 removedAuras = pVictim->m_removedAuras;
    AuraList const& vDamageShields = pVictim->GetAurasByType(SPELL_AURA_DAMAGE_SHIELD);
    for(AuraList::const_iterator i = vDamageShields.begin(), next = vDamageShields.begin(); i != vDamageShields.end(); i = next)
    {
        ++next;
        if (alreadyDone.find(*i) == alreadyDone.end())
        {
            alreadyDone.insert(*i);
            pVictim->SpellNonMeleeDamageLog(this, (*i)->GetId(), (*i)->GetModifier()->m_amount, false, false);
            if (pVictim->m_removedAuras > removedAuras)
            {
                removedAuras = pVictim->m_removedAuras;
                next = vDamageShields.begin();
            }
        }
    }

    if (pVictim->GetTypeId() == TYPEID_PLAYER && *damage)
    {
        for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL; i++)
        {
            // skip channeled spell (processed differently below)
            if (i == CURRENT_CHANNELED_SPELL)
                continue;

            if(pVictim->m_currentSpells[i])
            {
                int32 delay = int32(0.25f * pVictim->m_currentSpells[i]->GetCastTime());
                sLog.outDetail("Spell Delayed!%d",delay);
                pVictim->m_currentSpells[i]->Delayed(delay);
            }
        }

        // process channeled spell separately
        if (pVictim->m_currentSpells[CURRENT_CHANNELED_SPELL])
        {
            if (pVictim->m_currentSpells[CURRENT_CHANNELED_SPELL]->getState() == SPELL_STATE_CASTING)
            {
                uint32 channelInterruptFlags = pVictim->m_currentSpells[CURRENT_CHANNELED_SPELL]->m_spellInfo->ChannelInterruptFlags;
                if( channelInterruptFlags & CHANNEL_FLAG_DELAY )
                {
                    int32 delay = int32(0.25f * GetSpellDuration(pVictim->m_currentSpells[CURRENT_CHANNELED_SPELL]->m_spellInfo));
                    sLog.outDetail("Spell Delayed!%d",delay);
                    pVictim->m_currentSpells[CURRENT_CHANNELED_SPELL]->DelayedChannel(delay);
                    return;
                }
                else if( !(channelInterruptFlags & (CHANNEL_FLAG_DAMAGE | CHANNEL_FLAG_DAMAGE2)) )
                    return;

                sLog.outDetail("Spell Canceled!");
                pVictim->m_currentSpells[CURRENT_CHANNELED_SPELL]->cancel();
            }
            else if (pVictim->m_currentSpells[CURRENT_CHANNELED_SPELL]->getState() == SPELL_STATE_DELAYED)
            {
                // break channeled spell in delayed state on damage
                sLog.outDetail("Spell Canceled!");
                pVictim->m_currentSpells[CURRENT_CHANNELED_SPELL]->cancel();
            }
        }
    }
}

void Unit::AttackerStateUpdate (Unit *pVictim, WeaponAttackType attType )
{
    if(hasUnitState(UNIT_STAT_CONFUSED | UNIT_STAT_STUNDED | UNIT_STAT_FLEEING) || HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PACIFIED) )
        return;

    if (!pVictim->isAlive())
        return;

    if(IsNonMeleeSpellCasted(false))
        return;

    // melee attack spell casted at main hand attack only
    if (m_currentSpells[CURRENT_MELEE_SPELL] && attType == BASE_ATTACK)
    {
        m_currentSpells[CURRENT_MELEE_SPELL]->cast();
        return;
    }

    uint32 hitInfo;
    if (attType == BASE_ATTACK)
        hitInfo = HITINFO_NORMALSWING2;
    else if (attType == OFF_ATTACK)
        hitInfo = HITINFO_LEFTSWING;
    else
        return;

    VictimState victimState = VICTIMSTATE_NORMAL;

    CleanDamage cleanDamage = CleanDamage(0, BASE_ATTACK, MELEE_HIT_NORMAL );
    uint32   blocked_dmg = 0;
    uint32   absorbed_dmg = 0;
    uint32   resisted_dmg = 0;

    SpellSchoolMask meleeSchoolMask = GetMeleeDamageSchoolMask();

    if( (meleeSchoolMask & SPELL_SCHOOL_MASK_NORMAL) && pVictim->IsImmunedToPhysicalDamage() )
    {
        SendAttackStateUpdate (HITINFO_MISS, pVictim, 1, meleeSchoolMask, 0, 0, 0, VICTIMSTATE_IS_IMMUNE, 0);
        return;
    }

    uint32 damage = CalculateDamage (attType, false);

    DoAttackDamage (pVictim, &damage, &cleanDamage, &blocked_dmg, meleeSchoolMask, &hitInfo, &victimState, &absorbed_dmg, &resisted_dmg, attType);

    if (hitInfo & HITINFO_MISS)
        //send miss
        SendAttackStateUpdate (hitInfo, pVictim, 1, meleeSchoolMask, damage, absorbed_dmg, resisted_dmg, victimState, blocked_dmg);
    else
    {
        //do animation
        SendAttackStateUpdate (hitInfo, pVictim, 1, meleeSchoolMask, damage, absorbed_dmg, resisted_dmg, victimState, blocked_dmg);

        if (damage > (absorbed_dmg + resisted_dmg + blocked_dmg))
            damage -= (absorbed_dmg + resisted_dmg + blocked_dmg);
        else
            damage = 0;

        DealDamage (pVictim, damage, &cleanDamage, DIRECT_DAMAGE, meleeSchoolMask, NULL, true);

        if(GetTypeId() == TYPEID_PLAYER && pVictim->isAlive())
        {
            for(int i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; i++)
                ((Player*)this)->CastItemCombatSpell(((Player*)this)->GetItemByPos(INVENTORY_SLOT_BAG_0,i),pVictim,attType);
        }
    }

    if (GetTypeId() == TYPEID_PLAYER)
        DEBUG_LOG("AttackerStateUpdate: (Player) %u attacked %u (TypeId: %u) for %u dmg, absorbed %u, blocked %u, resisted %u.",
            GetGUIDLow(), pVictim->GetGUIDLow(), pVictim->GetTypeId(), damage, absorbed_dmg, blocked_dmg, resisted_dmg);
    else
        DEBUG_LOG("AttackerStateUpdate: (NPC)    %u attacked %u (TypeId: %u) for %u dmg, absorbed %u, blocked %u, resisted %u.",
            GetGUIDLow(), pVictim->GetGUIDLow(), pVictim->GetTypeId(), damage, absorbed_dmg, blocked_dmg, resisted_dmg);
}

MeleeHitOutcome Unit::RollPhysicalOutcomeAgainst (Unit const *pVictim, WeaponAttackType attType, SpellEntry const *spellInfo)
{
    // Miss chance based on melee
    float miss_chance = MeleeMissChanceCalc(pVictim, attType);

    // Critical hit chance
    float crit_chance = GetUnitCriticalChance(attType, pVictim);
    // this is to avoid compiler issue when declaring variables inside if
    float block_chance, parry_chance, dodge_chance;

    // cannot be dodged/parried/blocked
    if(spellInfo->Attributes & SPELL_ATTR_IMPOSSIBLE_DODGE_PARRY_BLOCK)
    {
        block_chance = 0.0f;
        parry_chance = 0.0f;
        dodge_chance = 0.0f;
    }
    else
    {
        // parry can be avoided only by some abilites
        parry_chance = pVictim->GetUnitParryChance();
        // block might be bypassed by it as well
        block_chance = pVictim->GetUnitBlockChance();
        // stunned target cannot dodge and this is check in GetUnitDodgeChance()
        dodge_chance = pVictim->GetUnitDodgeChance();
    }

    // Only players can have Talent&Spell bonuses
    if (GetTypeId() == TYPEID_PLAYER)
    {
        // Increase from SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL aura
        crit_chance += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL, spellInfo->SchoolMask);

        AuraList const& mCanNotBeDodge = GetAurasByType(SPELL_AURA_IGNORE_COMBAT_RESULT);
        for(AuraList::const_iterator i = mCanNotBeDodge.begin(); i != mCanNotBeDodge.end(); ++i)
        {
            if((*i)->GetModifier()->m_miscvalue == VICTIMSTATE_DODGE)       // can't be dodged rogue finishing move
            {
                if(spellInfo->SpellFamilyName==SPELLFAMILY_ROGUE && (spellInfo->SpellFamilyFlags & SPELLFAMILYFLAG_ROGUE__FINISHING_MOVE))
                {
                    dodge_chance = 0.0f;
                    break;
                }
            }
        }
    }

    // Spellmods
    if(Player* modOwner = GetSpellModOwner())
        modOwner->ApplySpellMod(spellInfo->Id, SPELLMOD_CRITICAL_CHANCE, crit_chance);

    float modHitChance = 0.0f;
    if (attType == RANGED_ATTACK) modHitChance = m_modRangedHitChance;
    else                          modHitChance = m_modMeleeHitChance;

    DEBUG_LOG("PHYSICAL OUTCOME: hit %f crit %f miss %f dodge %f parry %f block %f",modHitChance,crit_chance,miss_chance,dodge_chance,parry_chance, block_chance);

    return RollMeleeOutcomeAgainst(pVictim, attType, int32(crit_chance*100), int32(miss_chance*100), int32(modHitChance),int32(dodge_chance*100),int32(parry_chance*100),int32(block_chance*100), true);
}

MeleeHitOutcome Unit::RollMeleeOutcomeAgainst(const Unit *pVictim, WeaponAttackType attType) const
{
    // This is only wrapper

    // Miss chance based on melee
    float miss_chance = MeleeMissChanceCalc(pVictim, attType);

    // Critical hit chance
    float crit_chance = GetUnitCriticalChance(attType, pVictim);

    float modHitChance = 0.0f;
    if (attType == RANGED_ATTACK) modHitChance = m_modRangedHitChance;
    else                          modHitChance = m_modMeleeHitChance;

    // stunned target cannot dodge and this is check in GetUnitDodgeChance() (returned 0 in this case)
    float dodge_chance = pVictim->GetUnitDodgeChance();
    float block_chance = pVictim->GetUnitBlockChance();
    float parry_chance = pVictim->GetUnitParryChance();

    // Useful if want to specify crit & miss chances for melee, else it could be removed
    DEBUG_LOG("MELEE OUTCOME: hit %f crit %f miss %f dodge %f parry %f block %f", modHitChance,crit_chance,miss_chance,dodge_chance,parry_chance,block_chance);

    return RollMeleeOutcomeAgainst(pVictim, attType, int32(crit_chance*100), int32(miss_chance*100), int32(modHitChance),int32(dodge_chance*100),int32(parry_chance*100),int32(block_chance*100), false);
}

MeleeHitOutcome Unit::RollMeleeOutcomeAgainst (const Unit *pVictim, WeaponAttackType attType, int32 crit_chance, int32 miss_chance, int32 hit_chance, int32 dodge_chance, int32 parry_chance, int32 block_chance, bool SpellCasted ) const
{
    if(pVictim->GetTypeId()==TYPEID_UNIT && ((Creature*)pVictim)->IsInEvadeMode())
        return MELEE_HIT_EVADE;
    int32 attackerWeaponSkill = GetWeaponSkillValue(attType);
    // bonus from skills is 0.04%
    int32    skillBonus  = 4 * ( attackerWeaponSkill - int32(pVictim->GetMaxSkillValueForLevel()) );
    int32    skillBonus2 = 4 * ( int32(GetMaxSkillValueForLevel()) - int32(pVictim->GetDefenseSkillValue()));
    int32    sum = 0, tmp = 0;
    int32    roll = urand (0, 10000);

    DEBUG_LOG ("RollMeleeOutcomeAgainst: skill bonus of %d for attacker", skillBonus);
    DEBUG_LOG ("RollMeleeOutcomeAgainst: rolled %d, +hit %d, dodge %u, parry %u, block %u, crit %u",
        roll, hit_chance, (uint32)(pVictim->GetUnitDodgeChance()*100), (uint32)(pVictim->GetUnitParryChance()*100),
        (uint32)(pVictim->GetUnitBlockChance()*100), crit_chance);

    if(attType == RANGED_ATTACK)
    {
        int32 mod = pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_RANGED_HIT_CHANCE);
        miss_chance -= mod * 100;
    }
    else
    {
        int32 mod = pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_MELEE_HIT_CHANCE);
        miss_chance -= mod * 100;
    }

    // Inherit if passed
    tmp = miss_chance - skillBonus;

    if(tmp > 6000)
        tmp = 6000;

    if (tmp > 0 && roll < (sum += tmp ))
    {
        DEBUG_LOG ("RollMeleeOutcomeAgainst: MISS");
        return MELEE_HIT_MISS;
    }

    // always crit against a sitting target (except 0 crit chance)
    if( pVictim->GetTypeId() == TYPEID_PLAYER && crit_chance > 0 && !pVictim->IsStandState() )
    {
        DEBUG_LOG ("RollMeleeOutcomeAgainst: CRIT (sitting victim)");
        return MELEE_HIT_CRIT;
    }

    // Dodge chance

    // Reduce dodge chance by attacker expertise rating
    if (GetTypeId() == TYPEID_PLAYER)
        dodge_chance -= int32(((Player*)this)->GetExpertiseDodgeOrParryReduction(attType)*100);

    // Modify dodge chance by attacker SPELL_AURA_MOD_COMBAT_RESULT_CHANCE
    dodge_chance+= GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_COMBAT_RESULT_CHANCE, VICTIMSTATE_DODGE);

    tmp = dodge_chance;
    if (   (tmp > 0)                                        // check if unit _can_ dodge
        && ((tmp -= skillBonus) > 0)
        && roll < (sum += tmp))
    {
        DEBUG_LOG ("RollMeleeOutcomeAgainst: DODGE <%d, %d)", sum-tmp, sum);
        return MELEE_HIT_DODGE;
    }

    // check if attack comes from behind
    if (!pVictim->HasInArc(M_PI,this))
    {
        DEBUG_LOG ("RollMeleeOutcomeAgainst: attack came from behind.");
    }
    else
    {
        // Reduce parry chance by attacker expertise rating
        if (GetTypeId() == TYPEID_PLAYER)
            parry_chance-= int32(((Player*)this)->GetExpertiseDodgeOrParryReduction(attType)*100);
        int32 tmp = int32(parry_chance);
        if (   (tmp > 0)                                    // check if unit _can_ parry
            && ((tmp -= skillBonus) > 0)
            && (roll < (sum += tmp)))
        {
            DEBUG_LOG ("RollMeleeOutcomeAgainst: PARRY <%d, %d)", sum-tmp, sum);
            return MELEE_HIT_PARRY;
        }

        tmp = block_chance;
        if (   (tmp > 0)                                    // check if unit _can_ block
            && ((tmp -= skillBonus) > 0)
            && (roll < (sum += tmp)))
        {
            // Critical chance
            tmp = crit_chance + skillBonus2;
            if ( GetTypeId() == TYPEID_PLAYER && SpellCasted && tmp > 0 )
            {
                if ( roll_chance_i(tmp/100))
                {
                    DEBUG_LOG ("RollMeleeOutcomeAgainst: BLOCKED CRIT");
                    return MELEE_HIT_BLOCK_CRIT;
                }
            }
            DEBUG_LOG ("RollMeleeOutcomeAgainst: BLOCK <%d, %d)", sum-tmp, sum);
            return MELEE_HIT_BLOCK;
        }
    }

    // Critical chance
    tmp = crit_chance + skillBonus2;

    if (tmp > 0 && roll < (sum += tmp))
    {
        DEBUG_LOG ("RollMeleeOutcomeAgainst: CRIT <%d, %d)", sum-tmp, sum);
        return MELEE_HIT_CRIT;
    }

    // Max 40% chance to score a glancing blow against mobs that are higher level (can do only players and pets and not with ranged weapon)
    if( attType != RANGED_ATTACK && !SpellCasted &&
        (GetTypeId() == TYPEID_PLAYER || ((Creature*)this)->isPet()) &&
        pVictim->GetTypeId() != TYPEID_PLAYER && !((Creature*)pVictim)->isPet() &&
        getLevel() < pVictim->getLevel() )
    {
        // cap possible value (with bonuses > max skill)
        int32 skill = attackerWeaponSkill;
        int32 maxskill = GetMaxSkillValueForLevel();
        skill = (skill > maxskill) ? maxskill : skill;

        tmp = (10 + (int32(pVictim->GetDefenseSkillValue()) - skill)) * 100;
        tmp = tmp > 4000 ? 4000 : tmp;
        if (roll < (sum += tmp))
        {
            DEBUG_LOG ("RollMeleeOutcomeAgainst: GLANCING <%d, %d)", sum-4000, sum);
            return MELEE_HIT_GLANCING;
        }
    }

    // mobs can score crushing blows if they're 3 or more levels above victim
    // or when their weapon skill is 15 or more above victim's defense skill
    tmp = pVictim->GetDefenseSkillValue();
    int32 tmpmax = pVictim->GetMaxSkillValueForLevel();
    // having defense above your maximum (from items, talents etc.) has no effect
    tmp = tmp > tmpmax ? tmpmax : tmp;
    // tmp = mob's level * 5 - player's current defense skill
    tmp = GetMaxSkillValueForLevel() - tmp;
    if (GetTypeId() != TYPEID_PLAYER && !((Creature*)this)->isPet() && (tmp >= 15))
    {
        // add 2% chance per lacking skill point, min. is 15%
        tmp = tmp * 200 - 1500;
        if (roll < (sum += tmp))
        {
            DEBUG_LOG ("RollMeleeOutcomeAgainst: CRUSHING <%d, %d)", sum-tmp, sum);
            return MELEE_HIT_CRUSHING;
        }
    }

    DEBUG_LOG ("RollMeleeOutcomeAgainst: NORMAL");
    return MELEE_HIT_NORMAL;
}

uint32 Unit::CalculateDamage (WeaponAttackType attType, bool normalized)
{
    float min_damage, max_damage;

    if (normalized && GetTypeId()==TYPEID_PLAYER)
        ((Player*)this)->CalculateMinMaxDamage(attType,normalized,min_damage, max_damage);
    else
    {
        switch (attType)
        {
            case RANGED_ATTACK:
                min_damage = GetFloatValue(UNIT_FIELD_MINRANGEDDAMAGE);
                max_damage = GetFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE);
                break;
            case BASE_ATTACK:
                min_damage = GetFloatValue(UNIT_FIELD_MINDAMAGE);
                max_damage = GetFloatValue(UNIT_FIELD_MAXDAMAGE);
                break;
            case OFF_ATTACK:
                min_damage = GetFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE);
                max_damage = GetFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE);
                break;
                // Just for good manner
            default:
                min_damage = 0.0f;
                max_damage = 0.0f;
                break;
        }
    }

    if (min_damage > max_damage)
    {
        std::swap(min_damage,max_damage);
    }

    if(max_damage == 0.0f)
        max_damage = 5.0f;

    return urand((uint32)min_damage, (uint32)max_damage);
}

float Unit::CalculateLevelPenalty(SpellEntry const* spellProto) const
{
    if(spellProto->spellLevel <= 0)
        return 1.0f;

    float LvlPenalty = 0.0f;

    if(spellProto->spellLevel < 20)
        LvlPenalty = 20.0f - spellProto->spellLevel * 3.75f;
    float LvlFactor = (float(spellProto->spellLevel) + 6.0f) / float(getLevel());
    if(LvlFactor > 1.0f)
        LvlFactor = 1.0f;

    return (100.0f - LvlPenalty) * LvlFactor / 100.0f;
}

void Unit::SendAttackStart(Unit* pVictim)
{
    WorldPacket data( SMSG_ATTACKSTART, 16 );
    data << GetGUID();
    data << pVictim->GetGUID();

    SendMessageToSet(&data, true);
    DEBUG_LOG( "WORLD: Sent SMSG_ATTACKSTART" );
}

void Unit::SendAttackStop(Unit* victim)
{
    if(!victim)
        return;

    WorldPacket data( SMSG_ATTACKSTOP, (4+16) );            // we guess size
    data.append(GetPackGUID());
    data.append(victim->GetPackGUID());                     // can be 0x00...
    data << uint32(0);                                      // can be 0x1
    SendMessageToSet(&data, true);
    sLog.outDetail("%s %u stopped attacking %s %u", (GetTypeId()==TYPEID_PLAYER ? "player" : "creature"), GetGUIDLow(), (victim->GetTypeId()==TYPEID_PLAYER ? "player" : "creature"),victim->GetGUIDLow());

    /*if(victim->GetTypeId() == TYPEID_UNIT)
    ((Creature*)victim)->AI().EnterEvadeMode(this);*/
}

/*
// Melee based spells can be miss, parry or dodge on this step
// Crit or block - determined on damage calculation phase! (and can be both in some time)
float Unit::MeleeSpellMissChance(Unit *pVictim, WeaponAttackType attType, int32 skillDiff, SpellEntry const *spell)
{
    // Calculate hit chance (more correct for chance mod)
    int32 HitChance;

    // PvP - PvE melee chances
    int32 lchance = pVictim->GetTypeId() == TYPEID_PLAYER ? 5 : 7;
    int32 leveldif = pVictim->getLevel() - getLevel();
    if(leveldif < 3)
        HitChance = 95 - leveldif;
    else
        HitChance = 93 - (leveldif - 2) * lchance;

    // Hit chance depends from victim auras
    if(attType == RANGED_ATTACK)
        HitChance += pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_RANGED_HIT_CHANCE);
    else
        HitChance += pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_MELEE_HIT_CHANCE); 

    // Spellmod from SPELLMOD_RESIST_MISS_CHANCE
    if(Player *modOwner = GetSpellModOwner())
        modOwner->ApplySpellMod(spell->Id, SPELLMOD_RESIST_MISS_CHANCE, HitChance); 
    
    // Miss = 100 - hit
    float miss_chance= 100.0f - HitChance;

    // Bonuses from attacker aura and ratings
    if (attType == RANGED_ATTACK)
        miss_chance -= m_modRangedHitChance;
    else
        miss_chance -= m_modMeleeHitChance;

    // bonus from skills is 0.04%
    miss_chance -= skillDiff * 0.04f;

    if      (miss_chance <  1.0f) miss_chance =  1.0f;
    else if (miss_chance > 99.0f) miss_chance = 99.0f;
    return miss_chance;
}

// Melee based spells hit result calculations
SpellMissInfo Unit::MeleeSpellHitResult(Unit *pVictim, SpellEntry const *spell)
{
    WeaponAttackType attType = BASE_ATTACK;

    if (spell->DmgClass == SPELL_DAMAGE_CLASS_RANGED)
        attType = RANGED_ATTACK;

    // bonus from skills is 0.04% per skill Diff
    int32 skillDiff =  GetWeaponSkillValue(attType) - pVictim->GetMaxSkillValueForLevel();

    uint32 roll = urand (0, 10000);
    uint32 missChance = uint32(MeleeSpellMissChance(pVictim, attType, skillDiff, spell)*100.0f);

    // Roll miss
    uint32 tmp = missChance;
    if (roll < tmp)
        return SPELL_MISS_MISS;

    // Same spells cannot be parry/dodge
    if (spell->Attributes & SPELL_ATTR_IMPOSSIBLE_DODGE_PARRY_BLOCK)
        return SPELL_MISS_NONE;

    // Ranged attack can`t miss too
    if (attType == RANGED_ATTACK)
        return SPELL_MISS_NONE;
    
    bool attackFromBehind = !pVictim->HasInArc(M_PI,this);

    // Roll dodge
    int32 dodgeChance = int32(pVictim->GetUnitDodgeChance()*100.0f) - skillDiff * 4;
    // Reduce enemy dodge chance by SPELL_AURA_MOD_COMBAT_RESULT_CHANCE
    dodgeChance+= GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_COMBAT_RESULT_CHANCE, VICTIMSTATE_DODGE);

    // Reduce dodge chance by attacker expertise rating
    if (GetTypeId() == TYPEID_PLAYER)
        dodgeChance-=int32(((Player*)this)->GetExpertiseDodgeOrParryReduction(attType) * 100.0f);
    if (dodgeChance < 0)
        dodgeChance = 0;

    // Can`t dodge from behind in PvP (but its possible in PvE)
    if (GetTypeId() == TYPEID_PLAYER && pVictim->GetTypeId() == TYPEID_PLAYER && attackFromBehind)
        dodgeChance = 0;

    // Rogue talent`s cant be dodged 
    AuraList const& mCanNotBeDodge = GetAurasByType(SPELL_AURA_IGNORE_COMBAT_RESULT);
    for(AuraList::const_iterator i = mCanNotBeDodge.begin(); i != mCanNotBeDodge.end(); ++i)
    {
        if((*i)->GetModifier()->m_miscvalue == VICTIMSTATE_DODGE)       // can't be dodged rogue finishing move
        {
            if(spell->SpellFamilyName==SPELLFAMILY_ROGUE && (spell->SpellFamilyFlags & SPELLFAMILYFLAG_ROGUE__FINISHING_MOVE))
            {
                dodgeChance = 0;
                break;
            }
        }
    }

    tmp += dodgeChance;
    if (roll < tmp)
        return SPELL_MISS_DODGE;

    // Roll parry
    int32 parryChance = int32(pVictim->GetUnitParryChance()*100.0f)  - skillDiff * 4;
    // Reduce parry chance by attacker expertise rating
    if (GetTypeId() == TYPEID_PLAYER)
        parryChance-=int32(((Player*)this)->GetExpertiseDodgeOrParryReduction(attType) * 100.0f);
    // Can`t parry from behind
    if (parryChance < 0 || attackFromBehind)
        parryChance = 0;

    tmp += parryChance;
    if (roll < tmp)
        return SPELL_MISS_PARRY;
    
    return SPELL_MISS_NONE;
}*/

// TODO need use unit spell resistances in calculations
SpellMissInfo Unit::MagicSpellHitResult(Unit *pVictim, SpellEntry const *spell)
{
    // Can`t miss on dead target (on skinning for example)
    if (!pVictim->isAlive())
        return SPELL_MISS_NONE;

    SpellSchoolMask schoolMask = GetSpellSchoolMask(spell);
    // PvP - PvE spell misschances per leveldif > 2
    int32 lchance = pVictim->GetTypeId() == TYPEID_PLAYER ? 7 : 11;
    int32 leveldif = int32(pVictim->getLevel()) - int32(getLevel());

    // Base hit chance from attacker and victim levels
    int32 modHitChance;
    if(leveldif < 3)
        modHitChance = 96 - leveldif;
    else
        modHitChance = 94 - (leveldif - 2) * lchance;

    // Spellmod from SPELLMOD_RESIST_MISS_CHANCE
    if(Player *modOwner = GetSpellModOwner())
        modOwner->ApplySpellMod(spell->Id, SPELLMOD_RESIST_MISS_CHANCE, modHitChance);  
    // Increase from attacker SPELL_AURA_MOD_INCREASES_SPELL_PCT_TO_HIT auras
    modHitChance+=GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_INCREASES_SPELL_PCT_TO_HIT, schoolMask);
    // Chance hit from victim SPELL_AURA_MOD_ATTACKER_SPELL_HIT_CHANCE auras
    modHitChance+= pVictim->GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_ATTACKER_SPELL_HIT_CHANCE, schoolMask);
    // Reduce spell hit chance for Area of effect spells from victim SPELL_AURA_MOD_AOE_AVOIDANCE aura
    if (IsAreaOfEffectSpell(spell))
        modHitChance-=pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_AOE_AVOIDANCE);
    // Reduce spell hit chance for dispel mechanic spells from victim SPELL_AURA_MOD_DISPEL_RESIST
    if (IsDispelSpell(spell))
        modHitChance-=pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_DISPEL_RESIST);
    // Chance resist mechanic (select max value from every mechanic spell effect)
    int32 resist_mech = 0;
    // Get effects mechanic and chance
    for(int eff = 0; eff < 3; ++eff)
    {
        int32 effect_mech = GetEffectMechanic(spell, eff);
        if (effect_mech)
        {
            int32 temp = pVictim->GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_MECHANIC_RESISTANCE, effect_mech);
            if (resist_mech < temp)
                resist_mech = temp;
        }
    }
    // Apply mod
    modHitChance-=resist_mech;

    // Chance resist debuff
    modHitChance-=pVictim->GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_DEBUFF_RESISTANCE, int32(spell->Dispel));

    int32 HitChance = modHitChance * 100; 
    // Increase hit chance from attacker SPELL_AURA_MOD_SPELL_HIT_CHANCE and attacker ratings
    HitChance += int32(m_modSpellHitChance*100.0f);

    // Decreasee hit chance from victim rating bonus
    if (pVictim->GetTypeId()==TYPEID_PLAYER)
        HitChance -= int32(((Player*)pVictim)->GetRatingBonusValue(PLAYER_FIELD_HIT_TAKEN_SPELL_RATING)*100.0f);

    if (HitChance <  100) HitChance =  100;
    if (HitChance > 9900) HitChance = 9900;
    
    uint32 rand = urand(0,10000);
    if (rand > HitChance)
        return SPELL_MISS_RESIST;
    return SPELL_MISS_NONE;
}

SpellMissInfo Unit::SpellHitResult(Unit *pVictim, SpellEntry const *spell, bool CanReflect)
{
    // Return evade for units in evade mode
    if (pVictim->GetTypeId()==TYPEID_UNIT && ((Creature*)pVictim)->IsInEvadeMode())
        return SPELL_MISS_EVADE;

    // Check for immune (use charges)
    if (pVictim->IsImmunedToSpell(spell,true))
        return SPELL_MISS_IMMUNE;

    // All positive spells can`t miss
    // TODO: client not show miss log for this spells - so need find info for this in dbc and use it!
    if (IsPositiveSpell(spell->Id))
        return SPELL_MISS_NONE;

    // Check for immune (use charges)
    if (pVictim->IsImmunedToSpellDamage(spell,true))
        return SPELL_MISS_IMMUNE;

    // Try victim reflect spell
    if (CanReflect)
    {
        // specialized first
        Unit::AuraList const& mReflectSpellsSchool = pVictim->GetAurasByType(SPELL_AURA_REFLECT_SPELLS_SCHOOL);
        for(Unit::AuraList::const_iterator i = mReflectSpellsSchool.begin(); i != mReflectSpellsSchool.end(); ++i)
        {
            if((*i)->GetModifier()->m_miscvalue & GetSpellSchoolMask(spell))
            {
                int32 reflectchance = (*i)->GetModifier()->m_amount;
                if (reflectchance > 0 && roll_chance_i(reflectchance))
                {
                    if((*i)->m_procCharges > 0)
                    {
                        --(*i)->m_procCharges;
                        if((*i)->m_procCharges==0)
                            pVictim->RemoveAurasDueToSpell((*i)->GetId());
                    }
                    return SPELL_MISS_REFLECT;
                }
            }
        }

        // generic reflection
        Unit::AuraList const& mReflectSpells = pVictim->GetAurasByType(SPELL_AURA_REFLECT_SPELLS);
        for(Unit::AuraList::const_iterator i = mReflectSpells.begin(); i != mReflectSpells.end(); ++i)
        {
            int32 reflectchance = (*i)->GetModifier()->m_amount;
            if (reflectchance > 0 && roll_chance_i(reflectchance))
            {
                if((*i)->m_procCharges > 0)
                {
                    --(*i)->m_procCharges;
                    if((*i)->m_procCharges==0)
                        pVictim->RemoveAurasDueToSpell((*i)->GetId());
                }
                return SPELL_MISS_REFLECT;
            }
        }
    }

    // Temporary solution for melee based spells and spells vs SPELL_SCHOOL_NORMAL (hit result calculated after)
    for (int i=0;i<3;i++)
    {
        if (spell->Effect[i] == SPELL_EFFECT_WEAPON_DAMAGE ||
            spell->Effect[i] == SPELL_EFFECT_WEAPON_PERCENT_DAMAGE ||
            spell->Effect[i] == SPELL_EFFECT_NORMALIZED_WEAPON_DMG ||
            spell->Effect[i] == SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL)
            return SPELL_MISS_NONE;
    }

    // TODO need use this code for spell hit result calculation
    // now code commented for compotability
    switch (spell->DmgClass)
    {
        case SPELL_DAMAGE_CLASS_RANGED:
        case SPELL_DAMAGE_CLASS_MELEE:
//             return MeleeSpellHitResult(pVictim, spell);
            return SPELL_MISS_NONE;
        case SPELL_DAMAGE_CLASS_NONE:
        case SPELL_DAMAGE_CLASS_MAGIC:
            return MagicSpellHitResult(pVictim, spell);
    }
    return SPELL_MISS_NONE;
}

float Unit::MeleeMissChanceCalc(const Unit *pVictim, WeaponAttackType attType) const
{
    if(!pVictim)
        return 0.0f;

    // Base misschance 5%
    float misschance = 5.0f;

    // DualWield - Melee spells and physical dmg spells - 5% , white damage 24%
    if (haveOffhandWeapon())
    {
        bool isNormal = false;
        for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL; i++)
        {
            if( m_currentSpells[i] && (GetSpellSchoolMask(m_currentSpells[i]->m_spellInfo) & SPELL_SCHOOL_MASK_NORMAL) )
            {
                isNormal = true;
                break;
            }
        }
        if (isNormal || m_currentSpells[CURRENT_MELEE_SPELL])
        {
            misschance = 5.0f;
        }
        else
        {
            misschance = 24.0f;
        }
    }

    // PvP : PvE melee misschances per leveldif > 2
    int32 chance = pVictim->GetTypeId() == TYPEID_PLAYER ? 5 : 7;

    int32 leveldif = int32(pVictim->getLevel()) - int32(getLevel());
    if(leveldif < 0)
        leveldif = 0;

    // Hit chance from attacker based on ratings and auras
    float m_modHitChance;
    if (attType == RANGED_ATTACK)
        m_modHitChance = m_modRangedHitChance;
    else
        m_modHitChance = m_modMeleeHitChance;

    if(leveldif < 3)
        misschance += (leveldif - m_modHitChance);
    else
        misschance += ((leveldif - 2) * chance - m_modHitChance);

    // Hit chance for victim based on ratings
    if (pVictim->GetTypeId()==TYPEID_PLAYER)
    {
        if (attType == RANGED_ATTACK)
            misschance += ((Player*)pVictim)->GetRatingBonusValue(PLAYER_FIELD_HIT_TAKEN_RANGED_RATING);
        else
            misschance += ((Player*)pVictim)->GetRatingBonusValue(PLAYER_FIELD_HIT_TAKEN_MELEE_RATING);
    }
    return misschance > 60.f ? 60.f : misschance;
}

uint32 Unit::GetDefenseSkillValue() const
{
    if(GetTypeId() == TYPEID_PLAYER)
        return ((Player*)this)->GetSkillValue (SKILL_DEFENSE)+
            uint32(((Player*)this)->GetRatingBonusValue(PLAYER_FIELD_DEFENCE_RATING));
    else
        return GetUnitMeleeSkill();
}

uint32 Unit::GetBaseDefenseSkillValue() const
{
    if(GetTypeId() == TYPEID_PLAYER)
        return ((Player*)this)->GetBaseSkillValue(SKILL_DEFENSE);
    else
        return GetUnitMeleeSkill();
}

float Unit::GetUnitDodgeChance() const
{
    if(hasUnitState(UNIT_STAT_STUNDED))
        return 0.0f;
    if( GetTypeId() == TYPEID_PLAYER )
        return GetFloatValue(PLAYER_DODGE_PERCENTAGE);
    else
    {
        if(((Creature const*)this)->isTotem())
            return 0.0f;
        else
            return 5.0f;
    }
}

float Unit::GetUnitParryChance() const
{
    if ( IsNonMeleeSpellCasted(false) || hasUnitState(UNIT_STAT_STUNDED))
        return 0.0f;

    float chance = 0.0f;

    if(GetTypeId() == TYPEID_PLAYER)
    {
        Player const* player = (Player const*)this;
        if(player->CanParry() )
        {
            Item *tmpitem = ((Player*)this)->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
            if(!tmpitem || tmpitem->IsBroken() || !player->IsUseEquipedWeapon(true))
                tmpitem = ((Player*)this)->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);

            if(tmpitem && !tmpitem->IsBroken() && player->IsUseEquipedWeapon(tmpitem->GetSlot()==EQUIPMENT_SLOT_MAINHAND) && (
                tmpitem->GetProto()->InventoryType == INVTYPE_WEAPON ||
                tmpitem->GetProto()->InventoryType == INVTYPE_WEAPONOFFHAND ||
                tmpitem->GetProto()->InventoryType == INVTYPE_WEAPONMAINHAND ||
                tmpitem->GetProto()->InventoryType == INVTYPE_2HWEAPON))
                chance = GetFloatValue(PLAYER_PARRY_PERCENTAGE);
        }
    }
    else if(GetTypeId() == TYPEID_UNIT)
    {
        if(GetCreatureType() == CREATURE_TYPE_HUMANOID)
            chance = 5.0f;
    }

    return chance;
}

float Unit::GetUnitBlockChance() const
{
    if ( IsNonMeleeSpellCasted(false) || hasUnitState(UNIT_STAT_STUNDED))
        return 0.0f;

    if(GetTypeId() == TYPEID_PLAYER)
    {
        Item *tmpitem = ((Player const*)this)->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
        if(tmpitem && !tmpitem->IsBroken() && tmpitem->GetProto()->Block)
            return GetFloatValue(PLAYER_BLOCK_PERCENTAGE);
        else
            return 0.0f;
    }
    else
    {
        if(((Creature const*)this)->isTotem())
            return 0.0f;
        else
            return 5.0f;
    }
}

float Unit::GetUnitCriticalChance(WeaponAttackType attackType, const Unit *pVictim) const
{
    float crit;

    if(GetTypeId() == TYPEID_PLAYER)
    {
        switch(attackType)
        {
            case BASE_ATTACK:
                crit = GetFloatValue( PLAYER_CRIT_PERCENTAGE );
                crit+= pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_MELEE_CRIT_CHANCE);
                break;
            case OFF_ATTACK:
                crit = GetFloatValue( PLAYER_OFFHAND_CRIT_PERCENTAGE );
                crit+= pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_MELEE_CRIT_CHANCE);
                break;
            case RANGED_ATTACK:
                crit = GetFloatValue( PLAYER_RANGED_CRIT_PERCENTAGE );
                crit+= pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_RANGED_CRIT_CHANCE);
                break;
                // Just for good manner
            default:
                crit = 0.0f;
                break;
        }
    }
    else
        crit = 5.0f;

    // flat
    crit += pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_SPELL_AND_WEAPON_CRIT_CHANCE);

    // reduce crit chance from Rating for players
    if (pVictim->GetTypeId()==TYPEID_PLAYER)
    {
        if (attackType==RANGED_ATTACK)
            crit -= ((Player*)pVictim)->GetRatingBonusValue(PLAYER_FIELD_CRIT_TAKEN_RANGED_RATING);
        else
            crit -= ((Player*)pVictim)->GetRatingBonusValue(PLAYER_FIELD_CRIT_TAKEN_MELEE_RATING);
    }

    if (crit < 0.0f)
        crit = 0.0f;
    return crit;
}

uint32 Unit::GetWeaponSkillValue (WeaponAttackType attType) const
{
    uint32 value = 0;
    if(GetTypeId() == TYPEID_PLAYER)
    {
        uint16 slot = Player::GetWeaponSlotByAttack(attType);
        Item* item = ((Player*)this)->GetItemByPos (INVENTORY_SLOT_BAG_0, slot);

        if(slot != EQUIPMENT_SLOT_MAINHAND && (!item || item->IsBroken() ||
            item->GetProto()->Class != ITEM_CLASS_WEAPON || !((Player*)this)->IsUseEquipedWeapon(false) ))
            return 0;

        if(((Player*)this)->IsInFeralForm())
            return GetMaxSkillValueForLevel();              // always maximized SKILL_FERAL_COMBAT in fact
        // in range
        uint32  skill = item && !item->IsBroken() && ((Player*)this)->IsUseEquipedWeapon(attType==BASE_ATTACK)
            ? item->GetSkill() : SKILL_UNARMED;
        value = ((Player*)this)->GetSkillValue (skill);
        // Modify value from ratings
        value += uint32(((Player*)this)->GetRatingBonusValue(PLAYER_FIELD_ALL_WEAPONS_SKILL_RATING));
        switch (attType)
        {
            case BASE_ATTACK:   value+=uint32(((Player*)this)->GetRatingBonusValue(PLAYER_FIELD_MELEE_WEAPON_SKILL_RATING));break;
            case OFF_ATTACK:    value+=uint32(((Player*)this)->GetRatingBonusValue(PLAYER_FIELD_OFFHAND_WEAPON_SKILL_RATING));break;
            case RANGED_ATTACK: value+=uint32(((Player*)this)->GetRatingBonusValue(PLAYER_FIELD_RANGED_WEAPON_SKILL_RATING));break;
        }
    }
    else
        value = GetUnitMeleeSkill();
   return value;
}

uint32 Unit::GetBaseWeaponSkillValue (WeaponAttackType attType) const
{
    if(GetTypeId() == TYPEID_PLAYER)
    {
        uint16 slot = Player::GetWeaponSlotByAttack(attType);
        Item* item = ((Player*)this)->GetItemByPos (INVENTORY_SLOT_BAG_0, slot);

        if(slot != EQUIPMENT_SLOT_MAINHAND && (!item || item->IsBroken() ||
            item->GetProto()->Class != ITEM_CLASS_WEAPON || !((Player*)this)->IsUseEquipedWeapon(false) ))
            return 0;

        // in range
        uint32  skill = item && !item->IsBroken() && ((Player*)this)->IsUseEquipedWeapon(attType==BASE_ATTACK)
            ? item->GetSkill() : SKILL_UNARMED;
        return ((Player*)this)->GetBaseSkillValue(skill);
    }
    else
        return GetUnitMeleeSkill();
}

void Unit::_UpdateSpells( uint32 time )
{
    if(m_currentSpells[CURRENT_AUTOREPEAT_SPELL])
        _UpdateAutoRepeatSpell();

    // remove finished spells from current pointers
    for (uint32 i = 0; i < CURRENT_MAX_SPELL; i++)
    {
        if (m_currentSpells[i] && m_currentSpells[i]->getState() == SPELL_STATE_FINISHED)
        {
            m_currentSpells[i]->SetDeletable(true);         // spell may be safely deleted now
            m_currentSpells[i] = NULL;                      // remove pointer
        }
    }

    // TODO: Find a better way to prevent crash when multiple auras are removed.
    m_removedAuras = 0;
    for (AuraMap::iterator i = m_Auras.begin(); i != m_Auras.end(); ++i)
        if ((*i).second)
            (*i).second->SetUpdated(false);

    for (AuraMap::iterator i = m_Auras.begin(), next; i != m_Auras.end(); i = next)
    {
        next = i;
        ++next;
        if ((*i).second)
        {
            // prevent double update
            if ((*i).second->IsUpdated())
                continue;
            (*i).second->SetUpdated(true);
            (*i).second->Update( time );
            // several auras can be deleted due to update
            if (m_removedAuras)
            {
                if (m_Auras.empty()) break;
                next = m_Auras.begin();
                m_removedAuras = 0;
            }
        }
    }

    for (AuraMap::iterator i = m_Auras.begin(); i != m_Auras.end();)
    {
        if ((*i).second)
        {
            if ( !(*i).second->GetAuraDuration() && !((*i).second->IsPermanent() || ((*i).second->IsPassive())) )
            {
                RemoveAura(i);
            }
            else
            {
                ++i;
            }
        }
        else
        {
            ++i;
        }
    }

    if(!m_gameObj.empty())
    {
        std::list<GameObject*>::iterator ite1, dnext1;
        for (ite1 = m_gameObj.begin(); ite1 != m_gameObj.end(); ite1 = dnext1)
        {
            dnext1 = ite1;
            //(*i)->Update( difftime );
            if( !(*ite1)->isSpawned() )
            {
                (*ite1)->SetOwnerGUID(0);
                (*ite1)->SetRespawnTime(0);
                (*ite1)->Delete();
                dnext1 = m_gameObj.erase(ite1);
            }
            else
                ++dnext1;
        }
    }
}

void Unit::_UpdateAutoRepeatSpell()
{
    if(m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->getState() == SPELL_STATE_FINISHED)
    {
        //Auto Shot & Shoot
        if( m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->m_spellInfo->AttributesEx2 == 0x000020 && GetTypeId() == TYPEID_PLAYER )
        {
            // Auto Shot don't require ranged weapon cooldown at first cast, wand shoot does, so the 'FINISHED' state
            if(m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->m_spellInfo->Category == 351)
            {
                // Shoot
                resetAttackTimer( RANGED_ATTACK );
            }
            else
            {
                // Auto Shoot
                if (m_AutoRepeatFirstCast)
                {
                    // first cast only with recovery time (not less)
                    if (getAttackTimer( RANGED_ATTACK ) < m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->m_spellInfo->RecoveryTime)
                        setAttackTimer( RANGED_ATTACK, m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->m_spellInfo->RecoveryTime);
                    m_AutoRepeatFirstCast = false;
                }
                else
                {
                    // second or further casts
                    resetAttackTimer( RANGED_ATTACK );
                }
            }
        }
        else
        {
            setAttackTimer( RANGED_ATTACK, m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->m_spellInfo->RecoveryTime);
        }

        m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->setState(SPELL_STATE_IDLE);
    }
    else if(m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->getState() == SPELL_STATE_IDLE && isAttackReady(RANGED_ATTACK) )
    {
        // check if we can cast
        if (m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->CanCast(true) == 0)
        {
            // check movement in player case
            if(GetTypeId() == TYPEID_PLAYER && ((Player*)this)->isMoving())
            {
                // cancel wand shooting
                if(m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->m_spellInfo->Category == 351)
                    InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
                // ELSE delay auto-repeat ranged weapon until player movement stop
            }
            else
                // recheck range and req. items (ammo and gun, etc)
            if(m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->CheckRange(true) == 0 && m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->CheckItems() == 0 )
            {
                // check, if we are casting melee spell (it blocks autorepeat)
                if ( ! (m_currentSpells[CURRENT_MELEE_SPELL] &&
                    (m_currentSpells[CURRENT_MELEE_SPELL]->getState() != SPELL_STATE_FINISHED) &&
                    (m_currentSpells[CURRENT_MELEE_SPELL]->getState() != SPELL_STATE_DELAYED)) )
                {
                    // check, if we are casting something else, if no then run autorepeat spell
                    if (!IsNonMeleeSpellCasted(false, false, true))
                    {
                        // wee need clear target list before recasting 
                        m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->CleanupTargetList();
                        m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->setState(SPELL_STATE_PREPARING);
                        m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->ReSetTimer();
                    }
                }
            }
            else
            {
                InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
            }
        }
        else
        {
            InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
        }
    }
    else if (m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->getState() == SPELL_STATE_PREPARING)
    {
        // check, if some other incomplete spell exists (including melee) or ranged attack is not ready
        if ( m_currentSpells[CURRENT_MELEE_SPELL] ||
            m_currentSpells[CURRENT_GENERIC_SPELL] ||
            m_currentSpells[CURRENT_CHANNELED_SPELL] ||
            !isAttackReady(RANGED_ATTACK) )
        {
            // some other spell is here or ranged attack is not ready, break us to idle state
            m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->finish(false);
            m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->setState(SPELL_STATE_IDLE);
        }
    }
}

void Unit::SetCurrentCastedSpell( Spell * pSpell )
{
    assert(pSpell);                                         // NULL may be never passed here, use InterruptSpell or InterruptNonMeleeSpells

    uint32 CSpellType = pSpell->GetCurrentContainer();

    pSpell->SetDeletable(false);                            // spell will not be deleted until gone from current pointers
    if (pSpell == m_currentSpells[CSpellType]) return;      // avoid breaking self

    // break same type spell if it is not delayed
    InterruptSpell(CSpellType,false);

    // special breakage effects:
    switch (CSpellType)
    {
        case CURRENT_GENERIC_SPELL:
        {
            // generic spells always break channeled not delayed spells
            InterruptSpell(CURRENT_CHANNELED_SPELL,false);

            // autorepeat breaking
            if ( m_currentSpells[CURRENT_AUTOREPEAT_SPELL] )
            {
                // break autorepeat if not Auto Shot
                if (m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->m_spellInfo->Category == 351)
                    InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
            }
        } break;

        case CURRENT_CHANNELED_SPELL:
        {
            // channel spells always break generic non-delayed and any channeled spells
            InterruptSpell(CURRENT_GENERIC_SPELL,false);
            InterruptSpell(CURRENT_CHANNELED_SPELL);

            // it also does break autorepeat if not Auto Shot
            if ( m_currentSpells[CURRENT_AUTOREPEAT_SPELL] &&
                m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->m_spellInfo->Category == 351 )
                InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
        } break;

        case CURRENT_AUTOREPEAT_SPELL:
        {
            // only Auto Shoot does not break anything
            if (pSpell->m_spellInfo->Category == 351)
            {
                // generic autorepeats break generic non-delayed and channeled non-delayed spells
                InterruptSpell(CURRENT_GENERIC_SPELL,false);
                InterruptSpell(CURRENT_CHANNELED_SPELL,false);
            }
            else
            {
                // special action: set first cast flag for Auto Shoot
                m_AutoRepeatFirstCast = true;
            }
        } break;

        default:
        {
            // other spell types don't break anything now
        } break;
    }

    // current spell (if it is still here) may be safely deleted now
    if (m_currentSpells[CSpellType])
        m_currentSpells[CSpellType]->SetDeletable(true);

    // set new current spell
    m_currentSpells[CSpellType] = pSpell;
}

void Unit::InterruptSpell(uint32 spellType, bool withDelayed)
{
    assert(spellType < CURRENT_MAX_SPELL);

    if(m_currentSpells[spellType] && (withDelayed || m_currentSpells[spellType]->getState() != SPELL_STATE_DELAYED) )
    {
        // send autorepeat cancel message for autorepeat spells
        if (spellType == CURRENT_AUTOREPEAT_SPELL)
        {
            if(GetTypeId()==TYPEID_PLAYER)
                ((Player*)this)->SendAutoRepeatCancel();
        }

        if (m_currentSpells[spellType]->getState() != SPELL_STATE_FINISHED)
            m_currentSpells[spellType]->cancel();
        m_currentSpells[spellType]->SetDeletable(true);
        m_currentSpells[spellType] = NULL;
    }
}

bool Unit::IsNonMeleeSpellCasted(bool withDelayed, bool skipChanneled, bool skipAutorepeat) const
{
    // We don't do loop here to explicitly show that melee spell is excluded.
    // Maybe later some special spells will be excluded too.

    // generic spells are casted when they are not finished and not delayed
    if ( m_currentSpells[CURRENT_GENERIC_SPELL] &&
        (m_currentSpells[CURRENT_GENERIC_SPELL]->getState() != SPELL_STATE_FINISHED) &&
        (withDelayed || m_currentSpells[CURRENT_GENERIC_SPELL]->getState() != SPELL_STATE_DELAYED) )
        return(true);

    // channeled spells may be delayed, but they are still considered casted
    else if ( !skipChanneled && m_currentSpells[CURRENT_CHANNELED_SPELL] &&
        (m_currentSpells[CURRENT_CHANNELED_SPELL]->getState() != SPELL_STATE_FINISHED) )
        return(true);

    // autorepeat spells may be finished or delayed, but they are still considered casted
    else if ( !skipAutorepeat && m_currentSpells[CURRENT_AUTOREPEAT_SPELL] )
        return(true);

    return(false);
}

void Unit::InterruptNonMeleeSpells(bool withDelayed, uint32 spell_id)
{
    // generic spells are interrupted if they are not finished or delayed
    if (m_currentSpells[CURRENT_GENERIC_SPELL] && (!spell_id || m_currentSpells[CURRENT_GENERIC_SPELL]->m_spellInfo->Id==spell_id))
    {
        if  ( (m_currentSpells[CURRENT_GENERIC_SPELL]->getState() != SPELL_STATE_FINISHED) &&
            (withDelayed || m_currentSpells[CURRENT_GENERIC_SPELL]->getState() != SPELL_STATE_DELAYED) )
            m_currentSpells[CURRENT_GENERIC_SPELL]->cancel();
        m_currentSpells[CURRENT_GENERIC_SPELL]->SetDeletable(true);
        m_currentSpells[CURRENT_GENERIC_SPELL] = NULL;
    }

    // autorepeat spells are interrupted if they are not finished or delayed
    if (m_currentSpells[CURRENT_AUTOREPEAT_SPELL] && (!spell_id || m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->m_spellInfo->Id==spell_id))
    {
        // send disable autorepeat packet in any case
        if(GetTypeId()==TYPEID_PLAYER)
            ((Player*)this)->SendAutoRepeatCancel();

        if ( (m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->getState() != SPELL_STATE_FINISHED) &&
            (withDelayed || m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->getState() != SPELL_STATE_DELAYED) )
            m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->cancel();
        m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->SetDeletable(true);
        m_currentSpells[CURRENT_AUTOREPEAT_SPELL] = NULL;
    }

    // channeled spells are interrupted if they are not finished, even if they are delayed
    if (m_currentSpells[CURRENT_CHANNELED_SPELL] && (!spell_id || m_currentSpells[CURRENT_CHANNELED_SPELL]->m_spellInfo->Id==spell_id))
    {
        if (m_currentSpells[CURRENT_CHANNELED_SPELL]->getState() != SPELL_STATE_FINISHED)
            m_currentSpells[CURRENT_CHANNELED_SPELL]->cancel();
        m_currentSpells[CURRENT_CHANNELED_SPELL]->SetDeletable(true);
        m_currentSpells[CURRENT_CHANNELED_SPELL] = NULL;
    }
}

Spell* Unit::FindCurrentSpellBySpellId(uint32 spell_id) const
{
    for (uint32 i = 0; i < CURRENT_MAX_SPELL; i++)
        if(m_currentSpells[i] && m_currentSpells[i]->m_spellInfo->Id==spell_id)
            return m_currentSpells[i];
    return NULL;
}

bool Unit::isInFront(Unit const* target, float radius) const
{
    return IsWithinDistInMap(target, radius) && HasInArc( M_PI, target );
}

void Unit::SetInFront(Unit const* target)
{
    SetOrientation(GetAngle(target));
}

bool Unit::isInAccessablePlaceFor(Creature const* c) const
{
    if(IsInWater())
        return c->isCanSwimOrFly();
    else
        return c->isCanWalkOrFly();
}

bool Unit::IsInWater() const
{
    return MapManager::Instance().GetBaseMap(GetMapId())->IsInWater(GetPositionX(),GetPositionY(), GetPositionZ());
}

bool Unit::IsUnderWater() const
{
    return MapManager::Instance().GetBaseMap(GetMapId())->IsUnderWater(GetPositionX(),GetPositionY(),GetPositionZ());
}

void Unit::DeMorph()
{
    SetDisplayId(GetNativeDisplayId());
}

int32 Unit::GetTotalAuraModifier(AuraType auratype) const
{
    int32 modifier = 0;

    AuraList const& mTotalAuraList = GetAurasByType(auratype);
    for(AuraList::const_iterator i = mTotalAuraList.begin();i != mTotalAuraList.end(); ++i)
        modifier += (*i)->GetModifier()->m_amount;

    return modifier;
}

float Unit::GetTotalAuraMultiplier(AuraType auratype) const
{
    float multipler = 1.0f;

    AuraList const& mTotalAuraList = GetAurasByType(auratype);
    for(AuraList::const_iterator i = mTotalAuraList.begin();i != mTotalAuraList.end(); ++i)
        multipler *= (100.0f + (*i)->GetModifier()->m_amount)/100.0f;

    return multipler;
}

int32 Unit::GetMaxPositiveAuraModifier(AuraType auratype) const
{
    int32 modifier = 0;

    AuraList const& mTotalAuraList = GetAurasByType(auratype);
    for(AuraList::const_iterator i = mTotalAuraList.begin();i != mTotalAuraList.end(); ++i)
        if ((*i)->GetModifier()->m_amount > modifier)
            modifier = (*i)->GetModifier()->m_amount;

    return modifier;
}

int32 Unit::GetMaxNegativeAuraModifier(AuraType auratype) const
{
    int32 modifier = 0;

    AuraList const& mTotalAuraList = GetAurasByType(auratype);
    for(AuraList::const_iterator i = mTotalAuraList.begin();i != mTotalAuraList.end(); ++i)
        if ((*i)->GetModifier()->m_amount < modifier)
            modifier = (*i)->GetModifier()->m_amount;

    return modifier;
}

int32 Unit::GetTotalAuraModifierByMiscMask(AuraType auratype, uint32 misc_mask) const
{
    int32 modifier = 0;

    AuraList const& mTotalAuraList = GetAurasByType(auratype);
    for(AuraList::const_iterator i = mTotalAuraList.begin();i != mTotalAuraList.end(); ++i)
    {
        Modifier* mod = (*i)->GetModifier();
        if (mod->m_miscvalue & misc_mask)
            modifier += mod->m_amount;
    }
    return modifier;
}

float Unit::GetTotalAuraMultiplierByMiscMask(AuraType auratype, uint32 misc_mask) const
{
    float multipler = 1.0f;

    AuraList const& mTotalAuraList = GetAurasByType(auratype);
    for(AuraList::const_iterator i = mTotalAuraList.begin();i != mTotalAuraList.end(); ++i)
    {
        Modifier* mod = (*i)->GetModifier();
        if (mod->m_miscvalue & misc_mask)
            multipler *= (100.0f + mod->m_amount)/100.0f;
    }
    return multipler;
}

int32 Unit::GetMaxPositiveAuraModifierByMiscMask(AuraType auratype, uint32 misc_mask) const
{
    int32 modifier = 0;

    AuraList const& mTotalAuraList = GetAurasByType(auratype);
    for(AuraList::const_iterator i = mTotalAuraList.begin();i != mTotalAuraList.end(); ++i)
    {
        Modifier* mod = (*i)->GetModifier();
        if (mod->m_miscvalue & misc_mask && mod->m_amount > modifier)
            modifier = mod->m_amount;
    }

    return modifier;
}

int32 Unit::GetMaxNegativeAuraModifierByMiscMask(AuraType auratype, uint32 misc_mask) const
{
    int32 modifier = 0;

    AuraList const& mTotalAuraList = GetAurasByType(auratype);
    for(AuraList::const_iterator i = mTotalAuraList.begin();i != mTotalAuraList.end(); ++i)
    {
        Modifier* mod = (*i)->GetModifier();
        if (mod->m_miscvalue & misc_mask && mod->m_amount < modifier)
            modifier = mod->m_amount;
    }

    return modifier;
}

int32 Unit::GetTotalAuraModifierByMiscValue(AuraType auratype, int32 misc_value) const
{
    int32 modifier = 0;

    AuraList const& mTotalAuraList = GetAurasByType(auratype);
    for(AuraList::const_iterator i = mTotalAuraList.begin();i != mTotalAuraList.end(); ++i)
    {
        Modifier* mod = (*i)->GetModifier();
        if (mod->m_miscvalue == misc_value)
            modifier += mod->m_amount;
    }
    return modifier;
}

float Unit::GetTotalAuraMultiplierByMiscValue(AuraType auratype, int32 misc_value) const
{
    float multipler = 1.0f;

    AuraList const& mTotalAuraList = GetAurasByType(auratype);
    for(AuraList::const_iterator i = mTotalAuraList.begin();i != mTotalAuraList.end(); ++i)
    {
        Modifier* mod = (*i)->GetModifier();
        if (mod->m_miscvalue == misc_value)
            multipler *= (100.0f + mod->m_amount)/100.0f;
    }
    return multipler;
}

int32 Unit::GetMaxPositiveAuraModifierByMiscValue(AuraType auratype, int32 misc_value) const
{
    int32 modifier = 0;

    AuraList const& mTotalAuraList = GetAurasByType(auratype);
    for(AuraList::const_iterator i = mTotalAuraList.begin();i != mTotalAuraList.end(); ++i)
    {
        Modifier* mod = (*i)->GetModifier();
        if (mod->m_miscvalue == misc_value && mod->m_amount > modifier)
            modifier = mod->m_amount;
    }

    return modifier;
}

int32 Unit::GetMaxNegativeAuraModifierByMiscValue(AuraType auratype, int32 misc_value) const
{
    int32 modifier = 0;

    AuraList const& mTotalAuraList = GetAurasByType(auratype);
    for(AuraList::const_iterator i = mTotalAuraList.begin();i != mTotalAuraList.end(); ++i)
    {
        Modifier* mod = (*i)->GetModifier();
        if (mod->m_miscvalue == misc_value && mod->m_amount < modifier)
            modifier = mod->m_amount;
    }

    return modifier;
}

bool Unit::AddAura(Aura *Aur)
{
    // ghost spell check, allow apply any auras at player loading in ghost mode (will be cleanup after load)
    if( !isAlive() && Aur->GetId() != 20584 && Aur->GetId() != 8326 && Aur->GetId() != 2584 &&
        (GetTypeId()!=TYPEID_PLAYER || !((Player*)this)->GetSession()->PlayerLoading()) )
    {
        delete Aur;
        return false;
    }

    if(Aur->GetTarget() != this)
    {
        sLog.outError("Aura (spell %u eff %u) add to aura list of %s (lowguid: %u) but Aura target is %s (lowguid: %u)",
            Aur->GetId(),Aur->GetEffIndex(),(GetTypeId()==TYPEID_PLAYER?"player":"creature"),GetGUIDLow(),
            (Aur->GetTarget()->GetTypeId()==TYPEID_PLAYER?"player":"creature"),Aur->GetTarget()->GetGUIDLow());
        delete Aur;
        return false;
    }

    SpellEntry const* aurSpellInfo = Aur->GetSpellProto();

    spellEffectPair spair = spellEffectPair(Aur->GetId(), Aur->GetEffIndex());
    AuraMap::iterator i = m_Auras.find( spair );

    // take out same spell
    if (i != m_Auras.end())
    {
        // passive and persistent auras can stack with themselves any number of times
        if (!Aur->IsPassive() && !Aur->IsPersistent())
        {
            // replace aura if next will > spell StackAmount
            if(aurSpellInfo->StackAmount)
            {
                if(m_Auras.count(spair) >= aurSpellInfo->StackAmount)
                    RemoveAura(i);
            }
            // if StackAmount==0 not allow auras from same caster
            else
            {
                for(AuraMap::iterator i2 = m_Auras.lower_bound(spair); i2 != m_Auras.upper_bound(spair); ++i2)
                {
                    if(i2->second->GetCasterGUID()==Aur->GetCasterGUID())
                    {
                        // can be only single (this check done at _each_ aura add
                        RemoveAura(i2);
                        break;
                    }

                    bool stop = false;
                    switch(aurSpellInfo->EffectApplyAuraName[Aur->GetEffIndex()])
                    {
                        // DoT/HoT/etc
                        case SPELL_AURA_PERIODIC_DAMAGE:    // allow stack
                        case SPELL_AURA_PERIODIC_DAMAGE_PERCENT:
                        case SPELL_AURA_PERIODIC_LEECH:
                        case SPELL_AURA_PERIODIC_HEAL:
                        case SPELL_AURA_OBS_MOD_HEALTH:
                        case SPELL_AURA_PERIODIC_MANA_LEECH:
                        case SPELL_AURA_PERIODIC_ENERGIZE:
                        case SPELL_AURA_OBS_MOD_MANA:
                        case SPELL_AURA_POWER_BURN_MANA:    
                            break;
                        default:                            // not allow
                            // can be only single (this check done at _each_ aura add
                            RemoveAura(i2);
                            stop = true;
                            break;
                    }

                    if(stop)
                        break;
                }
            }
        }
    }

    // passive auras stack with all (except passive spell proc auras)
    if ((!Aur->IsPassive() || !IsPassiveStackableSpell(Aur->GetId())) &&
        !(Aur->GetId() == 20584 || Aur->GetId() == 8326))
    {
        if (!RemoveNoStackAurasDueToAura(Aur))
        {
            delete Aur;
            return false;                                   // couldnt remove conflicting aura with higher rank
        }
    }

    // update single target auras list (before aura add to aura list, to prevent unexpected remove recently added aura)
    if (IsSingleTargetSpell(aurSpellInfo) && Aur->GetTarget())
    {
        // caster pointer can be deleted in time aura remove, find it by guid at each iteration
        for(;;)
        {
            Unit* caster = Aur->GetCaster();
            if(!caster)                                     // caster deleted and not required adding scAura
                break;

            bool restart = false;
            AuraList& scAuras = caster->GetSingleCastAuras();
            for(AuraList::iterator itr = scAuras.begin(); itr != scAuras.end(); ++itr)
            {
                if( (*itr)->GetTarget() != Aur->GetTarget() &&
                    IsSingleTargetSpells((*itr)->GetSpellProto(),aurSpellInfo) )
                {
                    if ((*itr)->IsInUse())
                    {
                        sLog.outError("Aura (Spell %u Effect %u) is in process but attempt removed at aura (Spell %u Effect %u) adding, need add stack rule for IsSingleTargetSpell", (*itr)->GetId(), (*itr)->GetEffIndex(),Aur->GetId(), Aur->GetEffIndex());
                        continue;
                    }
                    (*itr)->GetTarget()->RemoveAura((*itr)->GetId(), (*itr)->GetEffIndex());
                    restart = true;
                    break;
                }
            }

            if(!restart)
            {
                // done
                scAuras.push_back(Aur);
                break;
            }
        }
    }

    // add aura, register in lists and arrays
    Aur->_AddAura();
    m_Auras.insert(AuraMap::value_type(spellEffectPair(Aur->GetId(), Aur->GetEffIndex()), Aur));
    if (Aur->GetModifier()->m_auraname < TOTAL_AURAS)
    {
        m_modAuras[Aur->GetModifier()->m_auraname].push_back(Aur);
    }

    Aur->ApplyModifier(true,true);
    sLog.outDebug("Aura %u now is in use", Aur->GetModifier()->m_auraname);
    return true;
}

void Unit::RemoveRankAurasDueToSpell(uint32 spellId)
{
    SpellEntry const *spellInfo = sSpellStore.LookupEntry(spellId);
    if(!spellInfo)
        return;
    AuraMap::iterator i,next;
    for (i = m_Auras.begin(); i != m_Auras.end(); i = next)
    {
        next = i;
        ++next;
        uint32 i_spellId = (*i).second->GetId();
        if((*i).second && i_spellId && i_spellId != spellId)
        {
            if(spellmgr.IsRankSpellDueToSpell(spellInfo,i_spellId))
            {
                RemoveAurasDueToSpell(i_spellId);

                if( m_Auras.empty() )
                    break;
                else
                    next =  m_Auras.begin();
            }
        }
    }
}

bool Unit::RemoveNoStackAurasDueToAura(Aura *Aur)
{
    if (!Aur)
        return false;

    SpellEntry const* spellProto = Aur->GetSpellProto();
    if (!spellProto)
        return false;

    uint32 spellId = Aur->GetId();
    uint32 effIndex = Aur->GetEffIndex();

    SpellSpecific spellId_spec = GetSpellSpecific(spellId);

    AuraMap::iterator i,next;
    for (i = m_Auras.begin(); i != m_Auras.end(); i = next)
    {
        next = i;
        ++next;
        if (!(*i).second) continue;

        SpellEntry const* i_spellProto = (*i).second->GetSpellProto();

        if (!i_spellProto)
            continue;

        uint32 i_spellId = i_spellProto->Id;

        if(IsPassiveSpell(i_spellId))
        {
            if(IsPassiveStackableSpell(i_spellId))
                continue;

            // passive non-stackable spells not stackable only with another rank of same spell
            if (!spellmgr.IsRankSpellDueToSpell(spellProto, i_spellId))
                continue;
        }

        uint32 i_effIndex = (*i).second->GetEffIndex();

        if(i_spellId == spellId) continue;

        bool is_triggered_by_spell = false;
        // prevent triggered aura of removing aura that triggered it
        for(int j = 0; j < 3; ++j)
            if (i_spellProto->EffectTriggerSpell[j] == spellProto->Id)
                is_triggered_by_spell = true;
        if (is_triggered_by_spell) continue;

        for(int j = 0; j < 3; ++j)
        {
            // prevent remove dummy triggered spells at next effect aura add
            switch(spellProto->Effect[j])                   // main spell auras added added after triggred spell
            {
                case SPELL_EFFECT_DUMMY:
                    switch(spellId)
                    {
                        case 5420: if(i_spellId==34123) is_triggered_by_spell = true; break;
                    }
                    break;
            }

            if(is_triggered_by_spell)
                break;

            // prevent remove form main spell by triggred passive spells
            switch(i_spellProto->EffectApplyAuraName[j])    // main aura added before triggered spell
            {
                case SPELL_AURA_MOD_SHAPESHIFT:
                    switch(i_spellId)
                    {
                        case 24858: if(spellId==24905)                  is_triggered_by_spell = true; break;
                        case 33891: if(spellId==5420 || spellId==34123) is_triggered_by_spell = true; break;
                        case 34551: if(spellId==22688)                  is_triggered_by_spell = true; break;
                    }
                    break;
            }
        }

        if(!is_triggered_by_spell)
        {
            SpellSpecific i_spellId_spec = GetSpellSpecific(i_spellId);

            bool is_sspc = IsSingleFromSpellSpecificPerCaster(spellId_spec,i_spellId_spec);

            if( is_sspc && Aur->GetCasterGUID() == (*i).second->GetCasterGUID() )
            {
                // cannot remove higher rank
                if (spellmgr.IsRankSpellDueToSpell(spellProto, i_spellId))
                    if(CompareAuraRanks(spellId, effIndex, i_spellId, i_effIndex) < 0)
                        return false;

                // Its a parent aura (create this aura in ApplyModifier)
                if ((*i).second->IsInUse())
                {
                    sLog.outError("Aura (Spell %u Effect %u) is in process but attempt removed at aura (Spell %u Effect %u) adding, need add stack rule for Unit::RemoveNoStackAurasDueToAura", i->second->GetId(), i->second->GetEffIndex(),Aur->GetId(), Aur->GetEffIndex());
                    continue;
                }
                RemoveAurasDueToSpell(i_spellId);

                if( m_Auras.empty() )
                    break;
                else
                    next =  m_Auras.begin();
            }
            else if( !is_sspc && spellmgr.IsNoStackSpellDueToSpell(spellId, i_spellId) )
            {
                // Its a parent aura (create this aura in ApplyModifier)
                if ((*i).second->IsInUse())
                {
                    sLog.outError("Aura (Spell %u Effect %u) is in process but attempt removed at aura (Spell %u Effect %u) adding, need add stack rule for Unit::RemoveNoStackAurasDueToAura", i->second->GetId(), i->second->GetEffIndex(),Aur->GetId(), Aur->GetEffIndex());
                    continue;
                }
                RemoveAurasDueToSpell(i_spellId);

                if( m_Auras.empty() )
                    break;
                else
                    next =  m_Auras.begin();
            }
            // Potions stack aura by aura (elixirs/flask already checked)
            else if( spellProto->SpellFamilyName == SPELLFAMILY_POTION && i_spellProto->SpellFamilyName == SPELLFAMILY_POTION )
            {
                if (IsNoStackAuraDueToAura(spellId, effIndex, i_spellId, i_effIndex))
                {
                    if(CompareAuraRanks(spellId, effIndex, i_spellId, i_effIndex) < 0)
                        return false;                       // cannot remove higher rank

                    // Its a parent aura (create this aura in ApplyModifier)
                    if ((*i).second->IsInUse())
                    {
                        sLog.outError("Aura (Spell %u Effect %u) is in process but attempt removed at aura (Spell %u Effect %u) adding, need add stack rule for Unit::RemoveNoStackAurasDueToAura", i->second->GetId(), i->second->GetEffIndex(),Aur->GetId(), Aur->GetEffIndex());
                        continue;
                    }
                    RemoveAura(i);
                    next = i;
                }
            }
        }
    }
    return true;
}

bool Unit::RemoveFirstAuraByDispel(uint32 dispel_type, Unit *pCaster)
{
    AuraMap::iterator i;
    // Create dispel mask by dispel type
    uint32 dispelMask = GetDispellMask( DispelType(dispel_type) );
    for (i = m_Auras.begin(); i != m_Auras.end();)
    {
        Aura *aur = (*i).second;
        if (aur && (1<<aur->GetSpellProto()->Dispel) & dispelMask)
        {
            SpellEntry const* spellInfo = (*i).second->GetSpellProto();
            uint32 eff = (*i).second->GetEffIndex();

            if(dispel_type == DISPEL_MAGIC)
            {
                bool positive = true;

                if (!(*i).second->IsPositive())
                    positive = false;
                else
                    positive = (spellInfo->AttributesEx & SPELL_ATTR_EX_NEGATIVE)==0;

                // do not remove positive auras if friendly target
                //               negative auras if non-friendly target
                if(positive == IsFriendlyTo(pCaster))
                {
                    ++i;
                    continue;
                }
            }

            // Unstable Affliction
            if (spellInfo->SpellFamilyName == SPELLFAMILY_WARLOCK && (spellInfo->SpellFamilyFlags & 0x010000000000LL))
            {
                int32 damage = (*i).second->GetModifier()->m_amount*9;
                CastCustomSpell(pCaster,31117,&damage,NULL,NULL,true,NULL,(*i).second);
            }

            // Lifebloom
            if ( spellInfo->SpellFamilyName == SPELLFAMILY_DRUID && (spellInfo->SpellFamilyFlags & 0x1000000000LL) )
            {
                AuraList const& m_dummyAuras = GetAurasByType(SPELL_AURA_DUMMY);
                for(AuraList::const_iterator i = m_dummyAuras.begin(); i != m_dummyAuras.end(); ++i)
                {
                    if((*i)->GetSpellProto()->SpellFamilyName == SPELLFAMILY_DRUID &&
                        (*i)->GetSpellProto()->SpellFamilyFlags & 0x1000000000LL)
                    {
                        int32 heal = (*i)->GetModifier()->m_amount;
                        CastCustomSpell(this,33778,&heal,NULL,NULL,true,NULL,*i,(*i)->GetCasterGUID());
                        break;
                    }
                }
            }

            RemoveAurasDueToSpell(spellInfo->Id);
            return true;
        }
        else
            ++i;
    }

    return false;
}

void Unit::RemoveAreaAurasByOthers(uint64 guid)
{
    int j = 0;
    for (AuraMap::iterator i = m_Auras.begin(); i != m_Auras.end();)
    {
        if (i->second && i->second->IsAreaAura())
        {
            uint64 casterGuid = i->second->GetCasterGUID();
            uint64 targetGuid = i->second->GetTarget()->GetGUID();
            // if area aura cast by someone else or by the specified caster
            if (casterGuid == guid || (guid == 0 && casterGuid != targetGuid))
            {
                for (j = 0; j < 4; j++)
                    if (m_TotemSlot[j] == casterGuid)
                        break;
                // and not by one of my totems
                if (j == 4)
                    RemoveAura(i);
                else
                    ++i;
            }
            else
                ++i;
        }
        else
            ++i;
    }
}

void Unit::RemoveAura(uint32 spellId, uint32 effindex, Aura* except)
{
    spellEffectPair spair = spellEffectPair(spellId, effindex);
    for(AuraMap::iterator iter = m_Auras.lower_bound(spair); iter != m_Auras.upper_bound(spair);)
    {
        if(iter->second!=except)
        {
            RemoveAura(iter);
            iter = m_Auras.lower_bound(spair);
        }
        else
            ++iter;
    }
}

void Unit::RemoveSingleAuraFromStack(uint32 spellId, uint32 effindex)
{
    AuraMap::iterator iter = m_Auras.find(spellEffectPair(spellId, effindex));
    if(iter != m_Auras.end())
        RemoveAura(iter);
}

void Unit::RemoveAurasDueToSpell(uint32 spellId, Aura* except)
{
    for (int i = 0; i < 3; ++i)
        RemoveAura(spellId,i,except);
}

void Unit::RemoveAurasDueToItemSpell(Item* castItem,uint32 spellId)
{
    for (int k=0; k < 3; ++k)
    {
        spellEffectPair spair = spellEffectPair(spellId, k);
        for (AuraMap::iterator iter = m_Auras.lower_bound(spair); iter != m_Auras.upper_bound(spair);)
        {
            if (iter->second->GetCastItemGUID() == castItem->GetGUID())
            {
                RemoveAura(iter);
                iter = m_Auras.upper_bound(spair);          // overwrite by more appropriate
            }
            else
                ++iter;
        }
    }
}

void Unit::RemoveAurasWithInterruptFlags(uint32 flags)
{
    for (AuraMap::iterator iter = m_Auras.begin(); iter != m_Auras.end(); )
    {
        if (iter->second->GetSpellProto()->AuraInterruptFlags & flags)
            RemoveAura(iter);
        else
            ++iter;
    }
}

void Unit::RemoveAura(AuraMap::iterator &i, bool onDeath)
{
    if (IsSingleTargetSpell((*i).second->GetSpellProto()))
    {
        if(Unit* caster = (*i).second->GetCaster())
        {
            AuraList& scAuras = caster->GetSingleCastAuras();
            scAuras.remove((*i).second);
        }
        else
        {
            sLog.outError("Couldn't find the caster of the single target aura, may crash later!");
        }
    }
    // remove aura from party members when the caster turns off the aura
    if((*i).second->IsAreaAura())
    {
        Unit *i_target = (*i).second->GetTarget();
        if((*i).second->GetCasterGUID() == i_target->GetGUID())
        {
            Unit* i_caster = i_target;

            Unit* owner = NULL;
            Group *pGroup = NULL;
            Player *pGroupOf = NULL;
            if (i_caster->GetTypeId() == TYPEID_PLAYER)
            {
                pGroupOf = (Player*)i_caster;
                pGroup = pGroupOf->GetGroup();
            }
            else if(((Creature*)i_caster)->isTotem() || ((Creature*)i_caster)->isPet() || i_caster->isCharmed())
            {
                owner = i_caster->GetCharmerOrOwner();
                if (owner && owner->GetTypeId() == TYPEID_PLAYER)
                {
                    pGroupOf = (Player*)owner;
                    pGroup = pGroupOf->GetGroup();
                }
            }

            //float radius =  GetRadius(sSpellRadiusStore.LookupEntry((*i).second->GetSpellProto()->EffectRadiusIndex[(*i).second->GetEffIndex()]));
            if(pGroup && pGroupOf)
            {
                for(GroupReference *itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
                {
                    Player* Target = itr->getSource();
                    if(!Target || !pGroup->SameSubGroup(pGroupOf, Target))
                        continue;

                    if(Target->GetGUID() == i_caster->GetGUID())
                        continue;
                    Aura *t_aura = Target->GetAura((*i).second->GetId(), (*i).second->GetEffIndex());
                    if (t_aura)
                        if (t_aura->GetCasterGUID() == i_caster->GetGUID())
                            Target->RemoveAura((*i).second->GetId(), (*i).second->GetEffIndex());
                }
            }
            else if(owner)
            {
                Aura *t_aura = owner->GetAura((*i).second->GetId(), (*i).second->GetEffIndex());
                if (t_aura)
                    if (t_aura->GetCasterGUID() == i_caster->GetGUID())
                        owner->RemoveAura((*i).second->GetId(), (*i).second->GetEffIndex());
            }
        }
    }
    if ((*i).second->GetModifier()->m_auraname < TOTAL_AURAS)
    {
        m_modAuras[(*i).second->GetModifier()->m_auraname].remove((*i).second);
    }
    (*i).second->SetRemoveOnDeath(onDeath);

    // remove from list before mods removing (prevent cyclic calls, mods added before including to aura list - use reverse order)
    Aura* Aur = i->second;

    DiminishingMechanics mech = DIMINISHING_NONE;
    if(Aur->GetSpellProto()->Mechanic || Aur->GetSpellProto()->EffectMechanic[Aur->GetEffIndex()])
    {
        mech = Unit::Mechanic2DiminishingMechanics(Aur->GetSpellProto()->EffectMechanic[Aur->GetEffIndex()]);

        if (mech == DIMINISHING_NONE)
            mech = Unit::Mechanic2DiminishingMechanics(Aur->GetSpellProto()->Mechanic);

        if(mech == DIMINISHING_MECHANIC_STUN || GetTypeId() == TYPEID_PLAYER && mech != DIMINISHING_NONE)
            UpdateDiminishingTime(mech);
    }

    // some ShapeshiftBoosts at remove trigger removing other auras including parent Shapeshift aura
    // remove aura from list before to prevent deleting it before
    m_Auras.erase(i);
    ++m_removedAuras;                                       // internal count used by unit update

    sLog.outDebug("Aura %u now is remove",Aur->GetModifier()->m_auraname);
    Aur->ApplyModifier(false,true);
    Aur->_RemoveAura();
    delete Aur;

    // only way correctly remove all auras from list
    if( m_Auras.empty() )
        i = m_Auras.end();
    else
        i = m_Auras.begin();
}

void Unit::RemoveAllAuras()
{
    while (!m_Auras.empty())
    {
        AuraMap::iterator iter = m_Auras.begin();
        RemoveAura(iter);
    }
}

void Unit::RemoveAllAurasOnDeath()
{
    // used just after dieing to remove all visible auras
    // and disable the mods for the passive ones
    for(AuraMap::iterator iter = m_Auras.begin(); iter != m_Auras.end();)
    {
        if (!iter->second->IsPassive() && !iter->second->IsDeathPersistent())
            RemoveAura(iter, true);
        else
            ++iter;
    }
}

void Unit::DelayAura(uint32 spellId, uint32 effindex, int32 delaytime)
{
    AuraMap::iterator iter = m_Auras.find(spellEffectPair(spellId, effindex));
    if (iter != m_Auras.end())
    {
        if (iter->second->GetAuraDuration() < delaytime)
            iter->second->SetAuraDuration(0);
        else
            iter->second->SetAuraDuration(iter->second->GetAuraDuration() - delaytime);
        iter->second->UpdateAuraDuration();
        sLog.outDebug("Aura %u partially interrupted on unit %u, new duration: %u ms",iter->second->GetModifier()->m_auraname, GetGUIDLow(), iter->second->GetAuraDuration());
    }
}

void Unit::_RemoveAllAuraMods()
{
    for (AuraMap::iterator i = m_Auras.begin(); i != m_Auras.end(); ++i)
    {
        (*i).second->ApplyModifier(false);
    }
}

void Unit::_ApplyAllAuraMods()
{
    for (AuraMap::iterator i = m_Auras.begin(); i != m_Auras.end(); ++i)
    {
        (*i).second->ApplyModifier(true);
    }
}

Aura* Unit::GetAura(uint32 spellId, uint32 effindex)
{
    AuraMap::iterator iter = m_Auras.find(spellEffectPair(spellId, effindex));
    if (iter != m_Auras.end())
        return iter->second;
    return NULL;
}

void Unit::AddDynObject(DynamicObject* dynObj)
{
    m_dynObjGUIDs.push_back(dynObj->GetGUID());
}

void Unit::RemoveDynObject(uint32 spellid)
{
    if(m_dynObjGUIDs.empty())
        return;
    for (DynObjectGUIDs::iterator i = m_dynObjGUIDs.begin(); i != m_dynObjGUIDs.end();)
    {
        DynamicObject* dynObj = ObjectAccessor::GetDynamicObject(*this,*m_dynObjGUIDs.begin());
        if(!dynObj)
        {
            i = m_dynObjGUIDs.erase(i);
        }
        else if(spellid == 0 || dynObj->GetSpellId() == spellid)
        {
            dynObj->Delete();
            i = m_dynObjGUIDs.erase(i);
        }
        else
            ++i;
    }
}

DynamicObject * Unit::GetDynObject(uint32 spellId, uint32 effIndex)
{
    for (DynObjectGUIDs::iterator i = m_dynObjGUIDs.begin(); i != m_dynObjGUIDs.end();)
    {
        DynamicObject* dynObj = ObjectAccessor::GetDynamicObject(*this,*m_dynObjGUIDs.begin());
        if(!dynObj)
        {
            i = m_dynObjGUIDs.erase(i);
            continue;
        }

        if (dynObj->GetSpellId() == spellId && dynObj->GetEffIndex() == effIndex)
            return dynObj;
        ++i;
    }
    return NULL;
}

DynamicObject * Unit::GetDynObject(uint32 spellId)
{
    for (DynObjectGUIDs::iterator i = m_dynObjGUIDs.begin(); i != m_dynObjGUIDs.end();)
    {
        DynamicObject* dynObj = ObjectAccessor::GetDynamicObject(*this,*m_dynObjGUIDs.begin());
        if(!dynObj)
        {
            i = m_dynObjGUIDs.erase(i);
            continue;
        }

        if (dynObj->GetSpellId() == spellId)
            return dynObj;
        ++i;
    }
    return NULL;
}

void Unit::AddGameObject(GameObject* gameObj)
{
    assert(gameObj && gameObj->GetOwnerGUID()==0);
    m_gameObj.push_back(gameObj);
    gameObj->SetOwnerGUID(GetGUID());
}

void Unit::RemoveGameObject(GameObject* gameObj, bool del)
{
    assert(gameObj && gameObj->GetOwnerGUID()==GetGUID());
    gameObj->SetOwnerGUID(0);
    m_gameObj.remove(gameObj);
    if(del)
    {
        gameObj->SetRespawnTime(0);
        gameObj->Delete();
    }
}

void Unit::RemoveGameObject(uint32 spellid, bool del)
{
    if(m_gameObj.empty())
        return;
    std::list<GameObject*>::iterator i, next;
    for (i = m_gameObj.begin(); i != m_gameObj.end(); i = next)
    {
        next = i;
        if(spellid == 0 || (*i)->GetSpellId() == spellid)
        {
            (*i)->SetOwnerGUID(0);
            if(del)
            {
                (*i)->SetRespawnTime(0);
                (*i)->Delete();
            }

            next = m_gameObj.erase(i);
        }
        else
            ++next;
    }
}

void Unit::SendSpellNonMeleeDamageLog(Unit *target,uint32 SpellID,uint32 Damage, SpellSchoolMask damageSchoolMask,uint32 AbsorbedDamage, uint32 Resist,bool PhysicalDamage, uint32 Blocked, bool CriticalHit)
{
    sLog.outDebug("Sending: SMSG_SPELLNONMELEEDAMAGELOG");
    WorldPacket data(SMSG_SPELLNONMELEEDAMAGELOG, (16+4+4+1+4+4+1+1+4+4+1)); // we guess size
    data.append(target->GetPackGUID());
    data.append(GetPackGUID());
    data << uint32(SpellID);
    data << uint32(Damage-AbsorbedDamage-Resist-Blocked);
    data << uint8(damageSchoolMask);                        // spell school
    data << uint32(AbsorbedDamage);                         // AbsorbedDamage
    data << uint32(Resist);                                 // resist
    data << uint8(PhysicalDamage);                          // if 1, then client show spell name (example: %s's ranged shot hit %s for %u school or %s suffers %u school damage from %s's spell_name
    data << uint8(0);                                       // unk isFromAura
    data << uint32(Blocked);                                // blocked
    data << uint32(CriticalHit ? 0x27 : 0x25);              // hitType, flags: 0x2 - SPELL_HIT_TYPE_CRIT, 0x10 - replace caster?
    data << uint8(0);                                       // isDebug?
    SendMessageToSet( &data, true );
}

void Unit::SendSpellMiss(Unit *target, uint32 spellID, SpellMissInfo missInfo)
{
    WorldPacket data(SMSG_SPELLLOGMISS, (4+8+1+4+8+1));
    data << uint32(spellID);
    data << uint64(GetGUID());
    data << uint8(0);                                       // can be 0 or 1
    data << uint32(1);                                      // target count
    // for(i = 0; i < target count; ++i)
    data << uint64(target->GetGUID());                      // target GUID
    data << uint8(missInfo);
    // end loop
    SendMessageToSet(&data, true);
}

void Unit::SendAttackStateUpdate(uint32 HitInfo, Unit *target, uint8 SwingType, SpellSchoolMask damageSchoolMask, uint32 Damage, uint32 AbsorbDamage, uint32 Resist, VictimState TargetState, uint32 BlockedAmount)
{
    sLog.outDebug("WORLD: Sending SMSG_ATTACKERSTATEUPDATE");

    WorldPacket data(SMSG_ATTACKERSTATEUPDATE, (16+45));    // we guess size
    data << (uint32)HitInfo;
    data.append(GetPackGUID());
    data.append(target->GetPackGUID());
    data << (uint32)(Damage-AbsorbDamage-Resist-BlockedAmount);

    data << (uint8)SwingType;                               // count?

    // for(i = 0; i < SwingType; ++i)
    data << (uint32)damageSchoolMask;
    data << (float)(Damage-AbsorbDamage-Resist-BlockedAmount);
    // still need to double check damage
    data << (uint32)(Damage-AbsorbDamage-Resist-BlockedAmount);
    data << (uint32)AbsorbDamage;
    data << (uint32)Resist;
    // end loop

    data << (uint32)TargetState;

    if( AbsorbDamage == 0 )                                 //also 0x3E8 = 0x3E8, check when that happens
        data << (uint32)0;
    else
        data << (uint32)-1;

    data << (uint32)0;
    data << (uint32)BlockedAmount;

    SendMessageToSet( &data, true );
}

void Unit::ProcDamageAndSpell(Unit *pVictim, uint32 procAttacker, uint32 procVictim, uint32 damage, SpellEntry const *procSpell, bool isTriggeredSpell, WeaponAttackType attType)
{
    sLog.outDebug("ProcDamageAndSpell: attacker flags are 0x%x, victim flags 0x%x", procAttacker, procVictim);
    if(procSpell)
        sLog.outDebug("ProcDamageAndSpell: invoked due to spell id %u %s", procSpell->Id, (isTriggeredSpell?"(triggered)":""));

    // Assign melee/ranged proc flags for magic attacks, that are actually melee/ranged abilities
    // not assign for spell proc triggered spell to prevent infinity (or unexpacted 2-3 times) melee damage spell proc call with melee damage effect
    // That is the question though if it's fully correct
    if(procSpell && !isTriggeredSpell)
    {
        if(procSpell->DmgClass == SPELL_DAMAGE_CLASS_MELEE)
        {
            if(procAttacker &  PROC_FLAG_HIT_SPELL) procAttacker |= PROC_FLAG_HIT_MELEE;
            if(procAttacker & PROC_FLAG_CRIT_SPELL) procAttacker |= PROC_FLAG_CRIT_MELEE;
            if(procVictim & PROC_FLAG_STRUCK_SPELL) procVictim |= PROC_FLAG_STRUCK_MELEE;
            if(procVictim & PROC_FLAG_STRUCK_CRIT_SPELL) procVictim |= PROC_FLAG_STRUCK_CRIT_MELEE;
            attType = BASE_ATTACK;                          // Melee abilities are assumed to be dealt with mainhand weapon
        }
        else if (procSpell->DmgClass == SPELL_DAMAGE_CLASS_RANGED)
        {
            if(procAttacker &  PROC_FLAG_HIT_SPELL) procAttacker |= PROC_FLAG_HIT_RANGED;
            if(procAttacker & PROC_FLAG_CRIT_SPELL) procAttacker |= PROC_FLAG_CRIT_RANGED;
            if(procVictim & PROC_FLAG_STRUCK_SPELL) procVictim |= PROC_FLAG_STRUCK_RANGED;
            if(procVictim & PROC_FLAG_STRUCK_CRIT_SPELL) procVictim |= PROC_FLAG_STRUCK_CRIT_RANGED;
            attType = RANGED_ATTACK;
        }
    }
    if(damage && (procVictim & (PROC_FLAG_STRUCK_MELEE|PROC_FLAG_STRUCK_RANGED|PROC_FLAG_STRUCK_SPELL)))
        procVictim |= (PROC_FLAG_TAKE_DAMAGE|PROC_FLAG_TOUCH);

    // Not much to do if no flags are set.
    if (procAttacker)
    {
        // procces auras that not generate casts at proc event before auras that generate casts to prevent proc aura added at prev. proc aura execute in set
        ProcDamageAndSpellFor(false,pVictim,procAttacker,attackerProcEffectAuraTypes,attType, procSpell, damage);
        ProcDamageAndSpellFor(false,pVictim,procAttacker,attackerProcCastAuraTypes,attType, procSpell, damage);
    }

    // Now go on with a victim's events'n'auras
    // Not much to do if no flags are set or there is no victim
    if(pVictim && pVictim->isAlive() && procVictim)
    {
        // procces auras that not generate casts at proc event before auras that generate casts to prevent proc aura added at prev. proc aura execute in set
        pVictim->ProcDamageAndSpellFor(true,this,procVictim,victimProcEffectAuraTypes,attType,procSpell, damage);
        pVictim->ProcDamageAndSpellFor(true,this,procVictim,victimProcCastAuraTypes,attType,procSpell, damage);
    }
}

void Unit::CastMeleeProcDamageAndSpell(Unit* pVictim, uint32 damage, WeaponAttackType attType, MeleeHitOutcome outcome, SpellEntry const *spellCasted, bool isTriggeredSpell)
{
    if(!pVictim)
        return;

    uint32 procAttacker = PROC_FLAG_NONE;
    uint32 procVictim   = PROC_FLAG_NONE;

    switch(outcome)
    {
        case MELEE_HIT_EVADE:
            return;
        case MELEE_HIT_MISS:
            if(attType == BASE_ATTACK || attType == OFF_ATTACK)
            {
                procAttacker = PROC_FLAG_MISS;
            }
            break;
        case MELEE_HIT_BLOCK_CRIT:
        case MELEE_HIT_CRIT:
            if(spellCasted && attType == BASE_ATTACK)
            {
                procAttacker |= PROC_FLAG_CRIT_SPELL;
                procVictim   |= PROC_FLAG_STRUCK_CRIT_SPELL;
                if ( outcome == MELEE_HIT_BLOCK_CRIT )
                {
                    procVictim |= PROC_FLAG_BLOCK;
                    procAttacker |= PROC_FLAG_TARGET_BLOCK;
                }
            }
            else if(attType == BASE_ATTACK || attType == OFF_ATTACK)
            {
                procAttacker = PROC_FLAG_HIT_MELEE | PROC_FLAG_CRIT_MELEE;
                procVictim = PROC_FLAG_STRUCK_MELEE | PROC_FLAG_STRUCK_CRIT_MELEE;
            }
            else
            {
                procAttacker = PROC_FLAG_HIT_RANGED | PROC_FLAG_CRIT_RANGED;
                procVictim = PROC_FLAG_STRUCK_RANGED | PROC_FLAG_STRUCK_CRIT_RANGED;
            }
            break;
        case MELEE_HIT_PARRY:
            procAttacker = PROC_FLAG_TARGET_DODGE_OR_PARRY;
            procVictim = PROC_FLAG_PARRY;
            break;
        case MELEE_HIT_BLOCK:
            procAttacker = PROC_FLAG_TARGET_BLOCK;
            procVictim = PROC_FLAG_BLOCK;
            break;
        case MELEE_HIT_DODGE:
            procAttacker = PROC_FLAG_TARGET_DODGE_OR_PARRY;
            procVictim = PROC_FLAG_DODGE;
            break;
        case MELEE_HIT_CRUSHING:
            if(attType == BASE_ATTACK || attType == OFF_ATTACK)
            {
                procAttacker = PROC_FLAG_HIT_MELEE | PROC_FLAG_CRIT_MELEE;
                procVictim = PROC_FLAG_STRUCK_MELEE | PROC_FLAG_STRUCK_CRIT_MELEE;
            }
            else
            {
                procAttacker = PROC_FLAG_HIT_RANGED | PROC_FLAG_CRIT_RANGED;
                procVictim = PROC_FLAG_STRUCK_RANGED | PROC_FLAG_STRUCK_CRIT_RANGED;
            }
            break;
        default:
            if(attType == BASE_ATTACK || attType == OFF_ATTACK)
            {
                procAttacker = PROC_FLAG_HIT_MELEE;
                procVictim = PROC_FLAG_STRUCK_MELEE;
            }
            else
            {
                procAttacker = PROC_FLAG_HIT_RANGED;
                procVictim = PROC_FLAG_STRUCK_RANGED;
            }
            break;
    }

    if(damage > 0)
        procVictim |= PROC_FLAG_TAKE_DAMAGE;

    if(procAttacker != PROC_FLAG_NONE || procVictim != PROC_FLAG_NONE)
        ProcDamageAndSpell(pVictim, procAttacker, procVictim, damage, spellCasted, isTriggeredSpell, attType);
}

void Unit::HandleDummyAuraProc(Unit *pVictim, SpellEntry const *dummySpell, uint32 effIndex, uint32 damage, Aura* triggeredByAura, SpellEntry const * procSpell, uint32 procFlag)
{
    Item* castItem = triggeredByAura->GetCastItemGUID() && GetTypeId()==TYPEID_PLAYER
        ? ((Player*)this)->GetItemByGuid(triggeredByAura->GetCastItemGUID()) : NULL;

    switch(dummySpell->SpellFamilyName)
    {
        case SPELLFAMILY_GENERIC:
        {
            switch (dummySpell->Id)
            {
                // Eye of Eye
                case 9799:
                case 25988:
                {
                    if(!pVictim || !pVictim->isAlive())
                        return;

                    // prevent damage back from weapon special attacks
                    if (!procSpell || procSpell->DmgClass != SPELL_DAMAGE_CLASS_MAGIC )
                        return;

                    // return damage % to attacker but < 50% own total health
                    int32 backDamage = triggeredByAura->GetModifier()->m_amount*int32(damage)/100;
                    if(backDamage > GetMaxHealth()/2)
                        backDamage = GetMaxHealth()/2;

                    CastCustomSpell(pVictim, 25997, &backDamage, NULL, NULL, true, castItem, triggeredByAura);

                    return;
                }
                // Sweeping Strikes
                case 12328:
                case 18765:
                case 35429:
                {
                    Unit* additionalTarget = SelectNearbyTarget();
                    if(!additionalTarget)
                        return;

                    // prevent chain of triggred spell from same triggred spell
                    if(procSpell && procSpell->Id==26654)
                        return;

                    CastSpell(additionalTarget, 26654, true, castItem,triggeredByAura);
                    return;
                }
                // Unstable Power
                case 24658:
                {
                    if (!procSpell || procSpell->Id == 24659)
                        return;
                    // Need remove one 24659 aura
                    RemoveSingleAuraFromStack(24659, 0);
                    RemoveSingleAuraFromStack(24659, 1);
                    return;
                }
                // Restless Strength
                case 24661:
                {
                    // Need remove one 24662 aura
                    RemoveSingleAuraFromStack(24662, 0);
                    return;
                }
                // Adaptive Warding (Frostfire Regalia set)
                case 28764:
                {
                    if(!procSpell || GetTypeId() != TYPEID_PLAYER)
                        return;

                    // find Mage Armor
                    bool found = false;
                    AuraList const& mRegenInterupt = GetAurasByType(SPELL_AURA_MOD_MANA_REGEN_INTERRUPT);
                    for(AuraList::const_iterator iter = mRegenInterupt.begin(); iter != mRegenInterupt.end(); ++iter)
                    {
                        if(SpellEntry const* iterSpellProto = (*iter)->GetSpellProto())
                        {
                            if(iterSpellProto->SpellFamilyName==SPELLFAMILY_MAGE && (iterSpellProto->SpellFamilyFlags & 0x10000000))
                            {
                                found=true;
                                break;
                            }
                        }
                    }
                    if(!found)
                        return;

                    uint32 spellId = 0;
                    switch(GetFirstSchoolInMask(GetSpellSchoolMask(procSpell)))
                    {
                        case SPELL_SCHOOL_NORMAL:
                        case SPELL_SCHOOL_HOLY:
                            return;                         // ignored
                        case SPELL_SCHOOL_FIRE:   spellId = 28765; break;
                        case SPELL_SCHOOL_NATURE: spellId = 28768; break;
                        case SPELL_SCHOOL_FROST:  spellId = 28766; break;
                        case SPELL_SCHOOL_SHADOW: spellId = 28769; break;
                        case SPELL_SCHOOL_ARCANE: spellId = 28770; break;
                        default:
                            return;
                    }

                    CastSpell(this, spellId, true, castItem, triggeredByAura);
                    return;
                }
                // Obsidian Armor (Justice Bearer`s Pauldrons shoulder)
                case 27539:
                {
                    if(!procSpell || GetTypeId() != TYPEID_PLAYER)
                        return;

                    if(!castItem)
                        return;

                    // not from DoT
                    bool found = false;
                    for(int j = 0; j < 3; ++j)
                    {
                        if(procSpell->EffectApplyAuraName[j]==SPELL_AURA_PERIODIC_DAMAGE||procSpell->EffectApplyAuraName[j]==SPELL_AURA_PERIODIC_DAMAGE_PERCENT)
                        {
                            found = true;
                            break;
                        }
                    }
                    if(found)
                        return;

                    uint32 spellId = 0;
                    switch(GetFirstSchoolInMask(GetSpellSchoolMask(procSpell)))
                    {
                        case SPELL_SCHOOL_NORMAL:
                            return;                         // ignore
                        case SPELL_SCHOOL_HOLY:   spellId = 27536;break;
                        case SPELL_SCHOOL_FIRE:   spellId = 27533;break;
                        case SPELL_SCHOOL_NATURE: spellId = 27538;break;
                        case SPELL_SCHOOL_FROST:  spellId = 27534;break;
                        case SPELL_SCHOOL_SHADOW: spellId = 27535;break;
                        case SPELL_SCHOOL_ARCANE: spellId = 27540;break;
                        default:
                            return;
                    }

                    CastSpell(this, spellId, true, castItem, triggeredByAura);
                    return;
                }
                // Mana Leech (Passive) (Priest Pet Aura)
                case 28305:
                {
                    // Cast on owner
                    if (Unit *owner = GetOwner())
                    {
                        int32 manaRegen = int32(damage * 2.5f);
                        CastCustomSpell(owner, 34650, &manaRegen, NULL, NULL, true, castItem, triggeredByAura);
                    }
                    return;
                }
                // Mark of Malice
                case 33493:
                {
                    // Cast finish spell at last charge
                    if (triggeredByAura->m_procCharges == 0)
                        CastSpell(this, 33494, true, castItem, triggeredByAura);
                    return;
                }
                // Twisted Reflection (boss spell)
                case 21063:
                    CastSpell(pVictim, 21064, true, castItem, triggeredByAura);
                    return;
                // Vampiric Aura (boss spell)
                case 38196:
                {
                    int32 basePoint = 3 * damage;           // 300%
                    if (basePoint >= 0)
                        CastCustomSpell(this, 31285, &basePoint, NULL, NULL, true, castItem, triggeredByAura);
                    return;
                }
                // Aura of Madness (Darkmoon Card: Madness trinket)
                //=====================================================
                // 39511 Sociopath: +35 strength (Paladin, Rogue, Druid, Warrior)
                // 40997 Delusional: +70 attack power (Rogue, Hunter, Paladin, Warrior, Druid)
                // 40998 Kleptomania: +35 agility (Warrior, Rogue, Paladin, Hunter, Druid)
                // 40999 Megalomania: +41 damage/healing (Druid, Shaman, Priest, Warlock, Mage, Paladin)
                // 41002 Paranoia: +35 spell/melee/ranged crit strike rating (All classes)
                // 41005 Manic: +35 haste (spell, melee and ranged) (All classes)
                // 41009 Narcissism: +35 intellect (Druid, Shaman, Priest, Warlock, Mage, Paladin, Hunter)
                // 41011 Martyr Complex: +35 stamina (All classes)
                // 41406 Dementia: Every 5 seconds either gives you +5% damage/healing. (Druid, Shaman, Priest, Warlock, Mage, Paladin)
                // 41409 Dementia: Every 5 seconds either gives you -5% damage/healing. (Druid, Shaman, Priest, Warlock, Mage, Paladin)
                case 39446:
                {
                    if(GetTypeId() != TYPEID_PLAYER)
                        return;

                    if(!castItem)
                        return;
                    // Select class defined buff
                    uint32 spellId = 0;
                    switch (getClass())
                    {
                        case CLASS_PALADIN:                 // 39511,40997,40998,40999,41002,41005,41009,41011,41409
                        case CLASS_DRUID:                   // 39511,40997,40998,40999,41002,41005,41009,41011,41409
                        {
                            uint32 RandomSpell[]={39511,40997,40998,40999,41002,41005,41009,41011,41409};
                            spellId = RandomSpell[ irand(0, sizeof(RandomSpell)/sizeof(uint32) - 1) ];
                            break;
                        }
                        case CLASS_ROGUE:                   // 39511,40997,40998,41002,41005,41011
                        case CLASS_WARRIOR:                 // 39511,40997,40998,41002,41005,41011
                        {
                            uint32 RandomSpell[]={39511,40997,40998,41002,41005,41011};
                            spellId = RandomSpell[ irand(0, sizeof(RandomSpell)/sizeof(uint32) - 1) ];
                            break;
                        }
                        case CLASS_PRIEST:                  // 40999,41002,41005,41009,41011,41406,41409
                        case CLASS_SHAMAN:                  // 40999,41002,41005,41009,41011,41406,41409
                        case CLASS_MAGE:                    // 40999,41002,41005,41009,41011,41406,41409
                        case CLASS_WARLOCK:                 // 40999,41002,41005,41009,41011,41406,41409
                        {
                            uint32 RandomSpell[]={40999,41002,41005,41009,41011,41406,41409};
                            spellId = RandomSpell[ irand(0, sizeof(RandomSpell)/sizeof(uint32) - 1) ];
                            break;
                        }
                        case CLASS_HUNTER:                  // 40997,40999,41002,41005,41009,41011,41406,41409
                        {
                            uint32 RandomSpell[]={40997,40999,41002,41005,41009,41011,41406,41409};
                            spellId = RandomSpell[ irand(0, sizeof(RandomSpell)/sizeof(uint32) - 1) ];
                            break;
                        }
                        default:
                            return;
                    }
                    CastSpell(this, spellId, true, castItem, triggeredByAura);
                    if (roll_chance_i(10))
                        ((Player*)this)->Say("This is Madness!", LANG_UNIVERSAL);
                    return;
                }
                /*
                // TODO: need find item for aura and triggered spells
                // Sunwell Exalted Caster Neck (??? neck)
                // cast ??? Light's Wrath if Exaled by Aldor
                // cast ??? Arcane Bolt if Exaled by Scryers
                case 46569:
                {
                    if(GetTypeId() != TYPEID_PLAYER)
                        return;

                    if(!castItem)
                        return;
                    // Get Aldor reputation rank 
                    if (((Player *)this)->GetReputationRank(932) == REP_EXALTED)
                        CastSpell(this, ???, true, castItem, triggeredByAura);
                    // Get Scryers reputation rank 
                    else if (((Player *)this)->GetReputationRank(934) == REP_EXALTED)
                        CastSpell(pVictim, ???, true, castItem, triggeredByAura);
                    return;
                }/**/
                // Sunwell Exalted Caster Neck (Shattered Sun Pendant of Acumen neck)
                // cast 45479 Light's Wrath if Exaled by Aldor
                // cast 45429 Arcane Bolt if Exaled by Scryers
                case 45481:
                {
                    if(GetTypeId() != TYPEID_PLAYER)
                        return;

                    if(!castItem)
                        return;
                    if( ((Player*)this)->HasSpellCooldown(dummySpell->Id))
                        return;
                    // Get Aldor reputation rank 
                    if (((Player *)this)->GetReputationRank(932) == REP_EXALTED)
                        CastSpell(this, 45479, true, castItem, triggeredByAura);
                    // Get Scryers reputation rank 
                    else if (((Player *)this)->GetReputationRank(934) == REP_EXALTED)
                        CastSpell(pVictim, 45429, true, castItem, triggeredByAura);
                    ((Player *)this)->AddSpellCooldown(dummySpell->Id, 0, time(NULL) + 45);
                    return;
                }
                // Sunwell Exalted Melee Neck (Shattered Sun Pendant of Might neck)
                // cast 45480 Light's Strength if Exaled by Aldor
                // cast 45428 Arcane Strike if Exaled by Scryers
                case 45482:
                {
                    if(GetTypeId() != TYPEID_PLAYER)
                        return;

                    if(!castItem)
                        return;
                    if( ((Player*)this)->HasSpellCooldown(dummySpell->Id))
                        return;
                    // Get Aldor reputation rank 
                    if (((Player *)this)->GetReputationRank(932) == REP_EXALTED)
                        CastSpell(this, 45480, true, castItem, triggeredByAura);
                    // Get Scryers reputation rank 
                    else if (((Player *)this)->GetReputationRank(934) == REP_EXALTED)
                        CastSpell(pVictim, 45428, true, castItem, triggeredByAura);
                    ((Player *)this)->AddSpellCooldown(dummySpell->Id, 0, time(NULL) + 45);
                    return;
                }
                // Sunwell Exalted Tank Neck (Shattered Sun Pendant of Resolve neck)
                // cast 45431 Arcane Insight if Exaled by Aldor
                // cast 45432 Light's Ward if Exaled by Scryers
                case 45483:
                {
                    if(GetTypeId() != TYPEID_PLAYER)
                        return;

                    if(!castItem)
                        return;
                    if( ((Player*)this)->HasSpellCooldown(dummySpell->Id))
                        return;
                    // Get Aldor reputation rank 
                    if (((Player *)this)->GetReputationRank(932) == REP_EXALTED)
                        CastSpell(this, 45432, true, castItem, triggeredByAura);
                    // Get Scryers reputation rank 
                    else if (((Player *)this)->GetReputationRank(934) == REP_EXALTED)
                        CastSpell(this, 45431, true, castItem, triggeredByAura);
                    ((Player *)this)->AddSpellCooldown(dummySpell->Id, 0, time(NULL) + 45);
                    return;
                }
                // Sunwell Exalted Healer Neck (Shattered Sun Pendant of Restoration neck)
                // cast 45478 Light's Salvation if Exaled by Aldor
                // cast 45430 Arcane Surge if Exaled by Scryers
                case 45484:
                {
                    if(GetTypeId() != TYPEID_PLAYER)
                        return;

                    if(!castItem)
                        return;
                    if( ((Player*)this)->HasSpellCooldown(dummySpell->Id))
                        return;
                    // Get Aldor reputation rank 
                    if (((Player *)this)->GetReputationRank(932) == REP_EXALTED)
                        CastSpell(this, 45478, true, castItem, triggeredByAura);
                    // Get Scryers reputation rank 
                    else if (((Player *)this)->GetReputationRank(934) == REP_EXALTED)
                        CastSpell(pVictim, 45430, true, castItem, triggeredByAura);
                    ((Player *)this)->AddSpellCooldown(dummySpell->Id, 0, time(NULL) + 45);
                    return;
                }
            }
            break;
        }
        case SPELLFAMILY_MAGE:
        {
            // Magic Absorption
            if (dummySpell->SpellIconID == 459)             // only this spell have SpellIconID == 459 and dummy aura
            {
                if (getPowerType() != POWER_MANA)
                    return;
                int32 manaReward = (triggeredByAura->GetModifier()->m_amount * GetMaxPower(POWER_MANA) / 100);
                CastCustomSpell(this, 29442, &manaReward, NULL, NULL, true, castItem, triggeredByAura);
                return;
            }
            // Master of Elements
            if (dummySpell->SpellIconID == 1920)
            {
                if(!procSpell)
                    return;

                int32 MEManaCostSave = procSpell->manaCost * triggeredByAura->GetModifier()->m_amount/100;

                if( MEManaCostSave > 0 )
                    CastCustomSpell(this, 29077, &MEManaCostSave, NULL, NULL, true, castItem, triggeredByAura);
                return;
            }
            switch(dummySpell->Id)
            {
                // Ignite
                case 11119:
                case 11120:
                case 12846:
                case 12847:
                case 12848:
                {
                    if(!pVictim || !pVictim->isAlive())
                        return;

                    int32 igniteDotBasePoints0;

                    switch (dummySpell->Id)
                    {
                        case 11119: igniteDotBasePoints0=int32(0.04f*damage); break;
                        case 11120: igniteDotBasePoints0=int32(0.08f*damage); break;
                        case 12846: igniteDotBasePoints0=int32(0.12f*damage); break;
                        case 12847: igniteDotBasePoints0=int32(0.16f*damage); break;
                        case 12848: igniteDotBasePoints0=int32(0.20f*damage); break;
                        default:
                            sLog.outError("Unit::HandleDummyAuraProc: non handled spell id: %u (IG)",dummySpell->Id);
                            return;
                    }
                    CastCustomSpell(pVictim, 12654, &igniteDotBasePoints0, NULL, NULL, true, castItem, triggeredByAura);
                    return;
                }
                // Combustion
                case 11129:
                {
                    CastSpell(this, 28682, true, castItem, triggeredByAura);
                    if (!(procFlag & PROC_FLAG_CRIT_SPELL))         //no crit
                        triggeredByAura->m_procCharges += 1;        //-> reincrease procCharge count since it was decreased before
                    else if (triggeredByAura->m_procCharges == 0)   //no more charges left and crit
                        RemoveAurasDueToSpell(28682);               //-> remove Combustion auras
                    return;
                }
            }
            break;
        }
        case SPELLFAMILY_WARRIOR:
        {
            if(dummySpell->SpellFamilyFlags==0x0000000800000000LL) // Retaliation
            {
                // check attack comes not from behind
                if (HasInArc(M_PI, pVictim))
                    CastSpell(pVictim, 22858,true,NULL,triggeredByAura);
                else
                    ++triggeredByAura->m_procCharges;       // Prevent drop charges
                return;
            }
            break;
        }
        case SPELLFAMILY_WARLOCK:
        {
            // Seed of Corruption
            if (dummySpell->SpellFamilyFlags & 0x0000001000000000LL)
            {
                Modifier* mod = triggeredByAura->GetModifier();
                // if damage is more than need or target die from damage deal finish spell
                // FOX ME: not triggred currently at death
                if( mod->m_amount <= damage || GetHealth() <= damage )
                {
                    // remember guid before aura delete
                    uint64 casterGuid = triggeredByAura->GetCasterGUID();

                    // Remove aura (before cast for prevent infinite loop handlers)
                    RemoveAurasDueToSpell(triggeredByAura->GetId());

                    // Cast finish spell (triggeredByAura already not exist!)
                    CastSpell(this, 27285, true, castItem, NULL, casterGuid);
                    return;
                }

                // Damage counting
                mod->m_amount-=damage;
                return;
            }
            // Seed of Corruption (Mobs cast) - no die req
            if (dummySpell->SpellFamilyFlags == 0x00LL && dummySpell->SpellIconID == 1932)
            {
                Modifier* mod = triggeredByAura->GetModifier();
                // if damage is more than need deal finish spell
                if( mod->m_amount <= damage )
                {
                    // remember guid before aura delete
                    uint64 casterGuid = triggeredByAura->GetCasterGUID();

                    // Remove aura (before cast for prevent infinite loop handlers)
                    RemoveAurasDueToSpell(triggeredByAura->GetId());

                    // Cast finish spell (triggeredByAura already not exist!)
                    CastSpell(this, 32865, true, castItem, NULL, casterGuid);
                    return;
                }
                // Damage counting
                mod->m_amount-=damage;
                return;
            }
            switch(dummySpell->Id)
            {
                // Nightfall
                case 18094:
                case 18095:
                {
                    CastSpell(this, 17941, true, castItem, triggeredByAura);
                    return;
                }
                //Soul Leech
                case 30293:
                case 30295:
                case 30296:
                {
                    if(!procSpell)
                        return;
                    int32 HealthBasePoints0 = int32(damage*triggeredByAura->GetModifier()->m_amount/100);
                    CastCustomSpell(this,30294,&HealthBasePoints0,NULL,NULL,true,castItem,triggeredByAura);
                    return;
                }
                // Shadowflame (Voidheart Raiment set bonus)
                case 37377:
                {
                    if(GetTypeId() != TYPEID_PLAYER || !pVictim || !pVictim->isAlive())
                        return;

                    CastSpell(pVictim,37379,true,castItem,triggeredByAura);
                    return;
                }
                // Pet Healing (Corruptor Raiment or Rift Stalker Armor)
                case 37381:
                {
                    if(GetTypeId() != TYPEID_PLAYER)
                        return;

                    if (Unit *pet = GetPet())
                    {
                        int32 healamount = damage * triggeredByAura->GetModifier()->m_amount/100;
                        CastCustomSpell(pet, 37382, &healamount, NULL, NULL, true, castItem, triggeredByAura);
                    }
                    return;
                }
                // Shadowflame Hellfire (Voidheart Raiment set bonus)
                case 39437:
                {
                    if(GetTypeId() != TYPEID_PLAYER || !pVictim || !pVictim->isAlive())
                        return;

                    CastSpell(pVictim,37378,true,castItem,triggeredByAura);
                    return;
                }
            }
            break;
        }
        case SPELLFAMILY_PRIEST:
        {
            // Vampiric Touch
            if( dummySpell->SpellFamilyFlags & 0x0000040000000000LL )
            {
                if(!pVictim || !pVictim->isAlive())
                    return;

                if(triggeredByAura->GetCasterGUID() == pVictim->GetGUID())
                {
                    int32 VTEnergizeBasePoints0 = triggeredByAura->GetModifier()->m_amount*damage/100;
                    pVictim->CastCustomSpell(pVictim,34919,&VTEnergizeBasePoints0,NULL,NULL,true,castItem, triggeredByAura);
                }
                return;
            }
            switch(dummySpell->Id)
            {
                // Vampiric Embrace
                case 15286:
                {
                    if(!pVictim || !pVictim->isAlive())
                        return;

                    if(triggeredByAura->GetCasterGUID() == pVictim->GetGUID())
                    {
                        int32 VEHealBasePoints0 = triggeredByAura->GetModifier()->m_amount*damage/100;
                        pVictim->CastCustomSpell(pVictim, 15290, &VEHealBasePoints0, NULL, NULL, true, castItem, triggeredByAura);
                    }
                    return;
                }
                // Priest Tier 6 Trinket
                case 40438:
                {
                    if(GetTypeId() != TYPEID_PLAYER)
                        return;

                    if(!castItem)
                        return;

                    uint32 triggerId;

                    // Shadow Word: Pain
                    if( procSpell->SpellFamilyFlags & 0x0000000000008000LL )
                        triggerId = 40441;
                    // Renew
                    else if( procSpell->SpellFamilyFlags & 0x0000000000000010LL )
                        triggerId = 40440;
                    else
                        return;

                    CastSpell(this, triggerId, true, castItem, triggeredByAura);
                    return;
                }
                // Oracle Healing Bonus ("Garments of the Oracle" set)
                case 26169:
                {
                    if( GetTypeId() != TYPEID_PLAYER )
                        return;

                    int32 healamount = int32(damage * 10/100);
                    CastCustomSpell(this, 26170, &healamount, NULL, NULL, true, castItem, triggeredByAura);
                    return;
                }
                // Frozen Shadoweave (Shadow's Embrace set) warning! its not only priest set
                case 39372:
                {
                    if( GetTypeId() != TYPEID_PLAYER )
                        return;

                    if(!procSpell || (GetSpellSchoolMask(procSpell) & (SPELL_SCHOOL_MASK_FROST | SPELL_SCHOOL_MASK_SHADOW))==0 )
                        return;

                    int32 healamount = int32(damage * 2 / 100);
                    CastCustomSpell(this, 39373, &healamount, NULL, NULL, true, castItem, triggeredByAura);
                    return;
                }
                // Vestments of Faith (Priest Tier 3) - 4 pieces bonus
                case 28809:
                {
                    if (!pVictim || !pVictim->isAlive())
                        return;
                    CastSpell(pVictim,28810,true,castItem,triggeredByAura);
                    return;
                }
            }

            //Prayer of Mending
            if(dummySpell->SpellFamilyFlags & 0x00002000000000LL)
            {
                int32 heal = triggeredByAura->GetModifier()->m_amount;

                // heal
                CastCustomSpell(this,33110,&heal,NULL,NULL,true,castItem,triggeredByAura,triggeredByAura->GetCasterGUID());

                // jump
                uint32 spellId;
                switch (dummySpell->Id)
                {
                    case 41635: spellId=33280; break;       // 1 jump
                    case 33280: spellId=35092; break;       // 2 jump
                    case 35092: spellId=35093; break;       // 3 jump
                    case 35093: spellId=35094; break;       // 4 jump
                    case 35094: return;                     // no jumps
                    default:
                        sLog.outError("Unit::HandleDummyAuraProc: non handled spell id: %u (PoM)", dummySpell->Id);
                        return;
                }

                CastCustomSpell(this,spellId,&heal,NULL,NULL,true,castItem,triggeredByAura,triggeredByAura->GetCasterGUID());

                // aura expire
                triggeredByAura->m_procCharges = 0;         // will removed
                return;
            }

            break;
        }
        case SPELLFAMILY_DRUID:
        {
            switch(dummySpell->Id)
            {
                // Healing Touch (Dreamwalker Raiment set)
                case 28719:
                {
                    if(GetTypeId() != TYPEID_PLAYER)
                        return;

                    int32 manaback = int32(procSpell->manaCost * 30 / 100);
                    CastCustomSpell(this, 28742, &manaback, NULL, NULL, true, castItem, triggeredByAura);
                    return;
                }
                // Healing Touch Refund (Idol of Longevity trinket)
                case 28847:
                {
                    if(GetTypeId() != TYPEID_PLAYER)
                        return;

                    if(!castItem)
                        return;

                    CastSpell(this, 28848, true, castItem,triggeredByAura);
                    return;
                }
                // Mana Restore (Malorne Raiment set / Malorne Regalia set)
                case 37288:
                case 37295:
                {
                    if(GetTypeId() != TYPEID_PLAYER)
                        return;

                    CastSpell(this, 37238, true, castItem, triggeredByAura);
                    return;
                }
                // Druid Tier 6 Trinket
                case 40442:
                {
                    if(GetTypeId() != TYPEID_PLAYER)
                        return;

                    if(!castItem)
                        return;

                    uint32 triggerId;
                    float  chance;

                    // Starfire
                    if( procSpell->SpellFamilyFlags & 0x0000000000000004LL )
                    {
                        triggerId = 40445; chance = 25.f;
                    }
                    // Rejuvenation
                    else if( procSpell->SpellFamilyFlags & 0x0000000000000010LL )
                    {
                        triggerId = 40446; chance = 25.f;
                    }
                    // Mangle (cat/bear)
                    else if( procSpell->SpellFamilyFlags & 0x0000044000000000LL )
                    {
                        triggerId = 40452; chance = 40.f;
                    }
                    else
                        return;

                    if (roll_chance_f(chance))
                        CastSpell(this, triggerId, true, castItem, triggeredByAura);
                    return;
                }
                // Maim Interrupt
                case 44835:
                {
                    if(!castItem || pVictim || !pVictim->isAlive())
                        return;
                    // Deadly Interrupt Effect
                    CastSpell(pVictim, 32747, true, castItem, triggeredByAura);
                    return;
                }
            }
            break;
        }
        case SPELLFAMILY_ROGUE:
        {
            switch(dummySpell->Id)
            {
                // Deadly Throw Interrupt
                case 32748:
                {
                    // Prevent cast Deadly Throw Interrupt on self from last effect (apply dummy) of Deadly Throw
                    if(GetTypeId() != TYPEID_PLAYER || this == pVictim)
                        return;

                    if(!castItem)
                        return;

                    CastSpell(pVictim, 32747, true, castItem, triggeredByAura);
                    return;
                }
                // Blade Flurry
                case 13877:
                case 33735:
                {
                    Unit* additionalTarget = SelectNearbyTarget();
                    if(!additionalTarget)
                        return;
                    int32 basePoint0 = damage;
                    CastCustomSpell(additionalTarget, 22482, &basePoint0, 0, 0, true, castItem, triggeredByAura);
                    return;
                }
            }
            // Quick Recovery
            if( dummySpell->SpellIconID == 2116 )
            {
                if(!procSpell)
                    return;

                // only rogue's finishing moves (maybe need additional checks)
                if( procSpell->SpellFamilyName!=SPELLFAMILY_ROGUE ||
                    (procSpell->SpellFamilyFlags & SPELLFAMILYFLAG_ROGUE__FINISHING_MOVE) == 0)
                    return;

                int32 QREnegyCostSave = procSpell->manaCost * triggeredByAura->GetModifier()->m_amount/100;
                if(QREnegyCostSave > 0)
                    CastCustomSpell(this, 31663, &QREnegyCostSave, NULL, NULL, true, castItem, triggeredByAura);

                return;
            }
            break;
        }
        case SPELLFAMILY_HUNTER:
        {
            // Thrill of the Hunt
            if ( dummySpell->SpellIconID == 2236 )
            {
                if(!procSpell)
                    return;
                int32 THManaCostSave = procSpell->manaCost * 40/100;
                if(THManaCostSave > 0)
                    CastCustomSpell(this,34720,&THManaCostSave,NULL,NULL,true,castItem, triggeredByAura);
                return;
            }
            break;
        }
        case SPELLFAMILY_PALADIN:
        {
            // Seal of Righteousness - melee proc dummy
            if (dummySpell->SpellFamilyFlags&0x000000008000000LL && triggeredByAura->GetEffIndex()==0)
            {
                if(GetTypeId() == TYPEID_PLAYER)
                {
                    uint32 spellId;
                    switch (triggeredByAura->GetId())
                    {
                        case 21084: spellId = 25742; break; // Rank 1
                        case 20287: spellId = 25740; break; // Rank 2
                        case 20288: spellId = 25739; break; // Rank 3
                        case 20289: spellId = 25738; break; // Rank 4
                        case 20290: spellId = 25737; break; // Rank 5
                        case 20291: spellId = 25736; break; // Rank 6
                        case 20292: spellId = 25735; break; // Rank 7
                        case 20293: spellId = 25713; break; // Rank 8
                        case 27155: spellId = 27156; break; // Rank 9
                        default:
                            sLog.outError("Unit::HandleDummyAuraProc: non handled possibly SoR (Id = %u)", triggeredByAura->GetId());
                            return;
                    }
                    Item *item = ((Player*)this)->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
                    float speed = (item ? item->GetProto()->Delay : BASE_ATTACK_TIME)/1000.0f;

                    float damageBasePoints;
                    if(item && item->GetProto()->InventoryType == INVTYPE_2HWEAPON) 
                        // two hand weapon
                        damageBasePoints=1.20f*triggeredByAura->GetModifier()->m_amount * 1.2f * 1.03f * speed/100.0f + 1;
                    else                                            
                        // one hand weapon/no weapon
                        damageBasePoints=0.85f*ceil(triggeredByAura->GetModifier()->m_amount * 1.2f * 1.03f * speed/100.0f) - 1;

                    int32 damagePoint = int32(damageBasePoints + 0.03f * (GetWeaponDamageRange(BASE_ATTACK,MINDAMAGE)+GetWeaponDamageRange(BASE_ATTACK,MAXDAMAGE))/2.0f) + 1;

                    // apply damage bonuses manually
                    if(damagePoint >= 0)
                        damagePoint = SpellDamageBonus(pVictim, dummySpell, damagePoint, SPELL_DIRECT_DAMAGE);

                    CastCustomSpell(pVictim,spellId,&damagePoint,NULL,NULL,true,NULL, triggeredByAura);
                }
                return;
            }
            // Seal of Blood do damage trigger
            if(dummySpell->SpellFamilyFlags & 0x0000040000000000LL)
            {
                switch(triggeredByAura->GetEffIndex())
                {
                    case 0:
                        // prevent chain triggering
                        if(procSpell && procSpell->Id==31893 )
                            return;

                        CastSpell(pVictim, 31893, true, castItem, triggeredByAura);
                        return;
                    case 1:
                    {
                        int32 damagePoint  = triggeredByAura->GetModifier()->m_amount * damage / 100;

                        CastCustomSpell(this, 32221, &damagePoint, NULL, NULL, true, castItem, triggeredByAura);
                        return;
                    }
                }
            }

            switch(dummySpell->Id)
            {
                // Holy Power (Redemption Armor set)
                case 28789:
                {
                    if(GetTypeId() != TYPEID_PLAYER || !pVictim || !pVictim->isAlive())
                        return;

                    // Set class defined buff
                    uint32 spellId = 0;
                    switch (pVictim->getClass())
                    {
                        case CLASS_PALADIN:
                        case CLASS_PRIEST:
                        case CLASS_SHAMAN:
                        case CLASS_DRUID:
                            spellId = 28795;                // Increases the friendly target's mana regeneration by $s1 per 5 sec. for $d.
                            break;
                        case CLASS_MAGE:
                        case CLASS_WARLOCK:
                            spellId = 28793;                // Increases the friendly target's spell damage and healing by up to $s1 for $d.
                            break;
                        case CLASS_HUNTER:
                        case CLASS_ROGUE:
                            spellId = 28791;                // Increases the friendly target's attack power by $s1 for $d.
                            break;
                        case CLASS_WARRIOR:
                            spellId = 28790;                // Increases the friendly target's armor
                            break;
                        default:
                            return;
                    }
                    CastSpell(pVictim, spellId, true, castItem, triggeredByAura);
                    return;
                }
                //Seal of Vengeance
                case 31801:
                {
                    if(effIndex != 0)                       // effect 1,2 used by seal unleashing code
                        return;

                    if(!pVictim)
                        return;

                    CastSpell(pVictim, 31803, true, castItem,triggeredByAura);

                    return;
                }
                // Spiritual Att.
                case 31785:
                case 33776:
                {
                    if(!pVictim)
                        return;

                    // if healed by another unit (pVictim)
                    if(this != pVictim)
                    {
                        int32 SAHealBasePoints0 = triggeredByAura->GetModifier()->m_amount*damage/100;
                        CastCustomSpell(this, 31786, &SAHealBasePoints0, NULL, NULL, true, castItem, triggeredByAura);
                    }

                    return;
                }
                // Paladin Tier 6 Trinket
                case 40470:
                {
                    if(GetTypeId() != TYPEID_PLAYER || !pVictim || !pVictim->isAlive())
                        return;

                    if(!castItem)
                        return;

                    uint32 triggerId = 0;
                    float  chance = 0.0f;

                    // Flash of light/Holy light
                    if( procSpell->SpellFamilyFlags & 0x00000000C0000000LL)
                    {
                        triggerId = 40471; chance = 15.f;
                    }
                    // Judgement
                    else if( procSpell->SpellFamilyFlags & 0x0000000000800000LL )
                    {
                        triggerId = 40472; chance = 50.f;
                    }

                    if (roll_chance_f(chance))
                        CastSpell(pVictim, triggerId, true, castItem, triggeredByAura);
                    return;
                }
            }
            break;
        }
        case SPELLFAMILY_SHAMAN:
        {
            switch(dummySpell->Id)
            {
                // Totemic Power (The Earthshatterer set)
                case 28823:
                {
                    if(GetTypeId() != TYPEID_PLAYER || !pVictim || !pVictim->isAlive())
                        return;

                    // Set class defined buff
                    uint32 spellId = 0;
                    switch (pVictim->getClass())
                    {
                        case CLASS_PALADIN:
                        case CLASS_PRIEST:
                        case CLASS_SHAMAN:
                        case CLASS_DRUID:
                            spellId = 28824;                // Increases the friendly target's mana regeneration by $s1 per 5 sec. for $d.
                            break;
                        case CLASS_MAGE:
                        case CLASS_WARLOCK:
                            spellId = 28825;                // Increases the friendly target's spell damage and healing by up to $s1 for $d.
                            break;
                        case CLASS_HUNTER:
                        case CLASS_ROGUE:
                            spellId = 28826;                // Increases the friendly target's attack power by $s1 for $d.
                            break;
                        case CLASS_WARRIOR:
                            spellId = 28827;                // Increases the friendly target's armor
                            break;
                        default:
                            return;
                    }
                    CastSpell(pVictim, spellId, true, castItem, triggeredByAura);
                    return;
                }
                // Lesser Healing Wave (Totem of Flowing Water Relic)
                case 28849:
                {
                    if(GetTypeId() != TYPEID_PLAYER)
                        return;

                    if(!castItem)
                        return;

                    CastSpell(this, 28850, true, castItem, triggeredByAura);
                    return;
                }
                // Windfury Weapon (Passive) 1-5 Ranks
                case 33757:
                {
                    if(GetTypeId()!=TYPEID_PLAYER)
                        return;

                    if(!castItem || !castItem->IsEquipped())
                        return;

                    uint32 spellId;
                    switch (castItem->GetEnchantmentId(EnchantmentSlot(TEMP_ENCHANTMENT_SLOT)))
                    {
                        case 283: spellId = 33757; break;   //1 Rank
                        case 284: spellId = 33756; break;   //2 Rank
                        case 525: spellId = 33755; break;   //3 Rank
                        case 1669:spellId = 33754; break;   //4 Rank
                        case 2636:spellId = 33727; break;   //5 Rank
                        default:
                        {
                            sLog.outError("Unit::HandleDummyAuraProc: non handled item enchantment (rank?) %u for spell id: %u (Windfury)",
                                castItem->GetEnchantmentId(EnchantmentSlot(TEMP_ENCHANTMENT_SLOT)),dummySpell->Id);
                            return;
                        }
                    }
                    if( ((Player*)this)->HasSpellCooldown(32910))
                        return;

                    SpellEntry const* windfurySpellEntry = sSpellStore.LookupEntry(spellId);

                    int32 addvalue = windfurySpellEntry->EffectBasePoints[0]+windfurySpellEntry->EffectBaseDice[0];

                    CastCustomSpell(this, 32910, &addvalue, NULL, NULL, true, castItem, triggeredByAura);

                    ((Player*)this)->AddSpellCooldown(32910,0,time(NULL) + 3);

                    return;
                }
                // Shaman Tier 6 Trinket
                case 40463:
                {
                    if(GetTypeId() != TYPEID_PLAYER)
                        return;

                    if(!castItem)
                        return;

                    uint32 triggerId = 0;
                    float  chance = 0.0f;
                    if (procSpell->SpellFamilyFlags & 0x0000000000000001LL)
                    {
                        triggerId = 40465; chance = 15.f;       // Lightning Bolt
                    }
                    else if (procSpell->SpellFamilyFlags & 0x0000000000000080LL)
                    {
                        triggerId = 40465; chance = 10.f;       // Lesser Healing Wave
                    }
                    else if (procSpell->SpellFamilyFlags & 0x0000001000000000LL)
                    {
                        triggerId = 40466; chance = 50.f;       // Stormstrike
                    }
                    else
                        return;

                    if (roll_chance_f(chance))
                        CastSpell(this, triggerId, true, castItem, triggeredByAura);

                    return;
                }
            }

            // Earth Shield
            if(dummySpell->SpellFamilyFlags==0x40000000000LL)
            {
                if(GetTypeId() != TYPEID_PLAYER)
                    return;

                if (((Player*)this)->HasSpellCooldown(379))
                    ++triggeredByAura->m_procCharges;       // restore charges for cooldown time proc
                else
                {
                    int32 HealBasePoints0 = triggeredByAura->GetModifier()->m_amount;
                    CastCustomSpell(this,379,&HealBasePoints0,NULL,NULL,true,castItem,triggeredByAura);
                    ((Player*)this)->AddSpellCooldown(379,0,time(NULL) + 3);
                }
                return;
            }
            // Lightning Overload
            if (dummySpell->SpellIconID == 2018)            // only this spell have SpellFamily Shaman SpellIconID == 2018 and dummy aura
            {
                if(!procSpell || GetTypeId() != TYPEID_PLAYER || !pVictim )
                    return;

                uint32 spellId = 0;
                // Every Lightning Bolt and Chain Lightning spell have dublicate vs half damage and zero cost
                switch (procSpell->Id)
                {
                    // Lightning Bolt
                    case   403: spellId = 45284; break;     // Rank  1
                    case   529: spellId = 45286; break;     // Rank  2
                    case   548: spellId = 45287; break;     // Rank  3
                    case   915: spellId = 45288; break;     // Rank  4
                    case   943: spellId = 45289; break;     // Rank  5
                    case  6041: spellId = 45290; break;     // Rank  6
                    case 10391: spellId = 45291; break;     // Rank  7
                    case 10392: spellId = 45292; break;     // Rank  8
                    case 15207: spellId = 45293; break;     // Rank  9
                    case 15208: spellId = 45294; break;     // Rank 10
                    case 25448: spellId = 45295; break;     // Rank 11
                    case 25449: spellId = 45296; break;     // Rank 12
                    // Chain Lightning
                    case   421: spellId = 45297; break;     // Rank  1
                    case   930: spellId = 45298; break;     // Rank  2
                    case  2860: spellId = 45299; break;     // Rank  3
                    case 10605: spellId = 45300; break;     // Rank  4
                    case 25439: spellId = 45301; break;     // Rank  5
                    case 25442: spellId = 45302; break;     // Rank  6
                    default:
                        sLog.outError("Unit::HandleDummyAuraProc: non handled spell id: %u (LO)", procSpell->Id);
                        return;
                }
                // No thread generated mod
                SpellModifier *mod = new SpellModifier;
                mod->op = SPELLMOD_THREAT;
                mod->value = -100;
                mod->type = SPELLMOD_PCT;
                mod->spellId = dummySpell->Id;
                mod->effectId = 0;
                mod->lastAffected = NULL;
                mod->mask = 0x0000000000000003LL;
                mod->charges = 0;
                ((Player*)this)->AddSpellMod(mod, true);

                // Remove cooldown (Chain Lightning - have Category Recovery time)
                if (procSpell->SpellFamilyFlags & 0x0000000000000002LL)
                    ((Player*)this)->RemoveSpellCooldown(spellId);

                // Hmmm.. in most case spells alredy set half basepoints but...
                // Lightning Bolt (2-10 rank) have full basepoint and half bonus from level
                // As on wiki:
                // BUG: Rank 2 to 10 (and maybe 11) of Lightning Bolt will proc another Bolt with FULL damage (not halved). This bug is known and will probably be fixed soon.
                // So - no add changes :)
                CastSpell(pVictim, spellId, true, castItem, triggeredByAura);

                ((Player*)this)->AddSpellMod(mod, false);
                return;
            }
            break;
        }
        default:
            break;
    }
}

void Unit::HandleProcTriggerSpell(Unit *pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const *procSpell, uint32 procFlags,WeaponAttackType attackType)
{
    SpellEntry const* auraSpellInfo = triggeredByAura->GetSpellProto();

    Item* castItem = triggeredByAura->GetCastItemGUID() && GetTypeId()==TYPEID_PLAYER
        ? ((Player*)this)->GetItemByGuid(triggeredByAura->GetCastItemGUID()) : NULL;

    uint32 trigger_spell_id = auraSpellInfo->EffectTriggerSpell[triggeredByAura->GetEffIndex()];

    switch(auraSpellInfo->SpellFamilyName)
    {
        case SPELLFAMILY_GENERIC:
        {
            switch(auraSpellInfo->Id)
            {
                // Elune's Touch (moonkin mana restore)
                case 24905:
                {
                    int32 amount = int32(0.3f * GetTotalAttackPowerValue(BASE_ATTACK));
                    CastCustomSpell(this, 33926, &amount, NULL, NULL, true, castItem, triggeredByAura);
                    return;
                }
                // Enlightenment
                case 29601:
                {
                    // only for cast with mana price
                    if(!procSpell || procSpell->powerType!=POWER_MANA || procSpell->manaCost==0 && procSpell->ManaCostPercentage==0 && procSpell->manaCostPerlevel==0)
                        return;

                    // fall through
                    break;                                          // normal cast
                }
                // Health Restore
                case 33510:
                {
                    // at melee hit call std triggered spell
                    if(procFlags & PROC_FLAG_HIT_MELEE)
                        break;

                    // else (at range hit) called custom case
                    CastSpell(this, 39557, true, castItem, triggeredByAura);
                    return;
                }
                // Augment Pain (Timbal's Focusing Crystal trinket bonus)
                case 45054:
                {
                    if(GetTypeId()!=TYPEID_PLAYER || !procSpell || !pVictim || !pVictim->isAlive() || !castItem)
                        return;

                    //only periodic damage can trigger spell
                    bool found = false;
                    for(int j = 0; j < 3; ++j)
                    {
                        if( procSpell->EffectApplyAuraName[j]==SPELL_AURA_PERIODIC_DAMAGE         ||
                            procSpell->EffectApplyAuraName[j]==SPELL_AURA_PERIODIC_DAMAGE_PERCENT ||
                            procSpell->EffectApplyAuraName[j]==SPELL_AURA_PERIODIC_LEECH          )
                        {
                            found = true;
                            break;
                        }
                    }
                    if(!found)
                        return;

                    if(((Player*)this)->HasSpellCooldown(auraSpellInfo->Id))
                        return;

                    CastSpell(pVictim, trigger_spell_id, true, castItem, triggeredByAura);
                    ((Player*)this)->AddSpellCooldown(auraSpellInfo->Id, 0, time(NULL) + 15);
                    return;
                }
                // Sunwell Dungeon Melee Trinket (Shard of Contempt trinket bonus)
                case 45354:
                {
                    if(GetTypeId()!=TYPEID_PLAYER || !castItem)
                        return;

                    if(((Player*)this)->HasSpellCooldown(auraSpellInfo->Id))
                        return;

                    CastSpell(this, trigger_spell_id, true, castItem, triggeredByAura);
                    ((Player*)this)->AddSpellCooldown(auraSpellInfo->Id, 0, time(NULL) + 45);
                    return;
                }
            }
            break;
        }
        case SPELLFAMILY_MAGE:
        {
            switch(auraSpellInfo->SpellIconID)
            {
                case 2127:
                    //Blazing Speed
                    CastSpell(this, 31643, true, castItem, triggeredByAura);
                    return;
            }
            break;
        }
        case SPELLFAMILY_WARRIOR:
        {
            //Rampage (overwrite non existing triggered spell call in spell.dbc
            if((auraSpellInfo->SpellFamilyFlags & 0x100000) && auraSpellInfo->SpellIconID==2006)
            {
                //all ranks have effect[0]==AURA (Proc Trigger Spell, non-existed)
                //and effect[1]==TriggerSpell

                if(auraSpellInfo->Effect[1]!=SPELL_EFFECT_TRIGGER_SPELL)
                {
                    sLog.outError("Unit::HandleProcTriggerSpell: Spell %u have wrong effect in RM",triggeredByAura->GetSpellProto()->Id);
                    return;
                }

                CastSpell(this, auraSpellInfo->EffectTriggerSpell[1], true, castItem, triggeredByAura);
                return;
            }
            break;
        }
        case SPELLFAMILY_WARLOCK:
        {
            // Pyroclasm
            if(auraSpellInfo->SpellFamilyFlags == 0x0000000000000000 && auraSpellInfo->SpellIconID==1137)
            {
                // last case for Hellfire that damage caster also but don't must stun caster
                if(!pVictim || !pVictim->isAlive() || pVictim == this )
                    return;

                float chance = 0;
                switch (triggeredByAura->GetId())
                {
                    case 18096: chance = 13.0f; break;
                    case 18073: chance = 26.0f; break;
                }
                if (roll_chance_f(chance))
                    CastSpell(pVictim, 18093, true, castItem, triggeredByAura);
                return;
            }
            // Drain Soul
            if(auraSpellInfo->SpellFamilyFlags & 0x0000000000004000)
            {
                Unit::AuraList const& mAddFlatModifier = GetAurasByType(SPELL_AURA_ADD_FLAT_MODIFIER);
                for(Unit::AuraList::const_iterator i = mAddFlatModifier.begin(); i != mAddFlatModifier.end(); ++i)
                {
                    //Improved Drain Soul
                    if ((*i)->GetModifier()->m_miscvalue == SPELLMOD_CHANCE_OF_SUCCESS && (*i)->GetSpellProto()->SpellIconID == 113)
                    {
                        int32 value2 = CalculateSpellDamage((*i)->GetSpellProto(),2,(*i)->GetSpellProto()->EffectBasePoints[2],this);
                        int32 impDrainSoulBasePoints0 = value2 * GetMaxPower(POWER_MANA) / 100;
                        CastCustomSpell(this, 18371, &impDrainSoulBasePoints0, NULL, NULL, true, castItem, triggeredByAura);
                        break;
                    }
                }
                return;
            }
            break;
        }
        case SPELLFAMILY_PRIEST:
        {
            //Blessed Recovery
            if(auraSpellInfo->SpellFamilyFlags == 0x00000000LL && auraSpellInfo->SpellIconID==1875)
            {
                uint32 EffectId = 0;
                switch (triggeredByAura->GetSpellProto()->Id)
                {
                case 27811: EffectId = 27813; break;
                case 27815: EffectId = 27817; break;
                case 27816: EffectId = 27818; break;
                default:
                    sLog.outError("Unit::HandleProcTriggerSpell: Spell %u not handled in BR",triggeredByAura->GetSpellProto()->Id);
                    return;
                }

                int32 heal_amount = damage * triggeredByAura->GetModifier()->m_amount / 100;
                int32 BRHealBasePoints0 = heal_amount/3;
                CastCustomSpell(this, EffectId, &BRHealBasePoints0, NULL, NULL, true, castItem, triggeredByAura);
                return;
            }
            // Shadowguard
            if((auraSpellInfo->SpellFamilyFlags & 0x80000000LL) && auraSpellInfo->SpellVisual==7958)
            {
                if(!pVictim || !pVictim->isAlive())
                    return;

                uint32 spell = 0;
                switch(triggeredByAura->GetSpellProto()->Id)
                {
                    case 18137: spell = 28377; break;       // Rank 1
                    case 19308: spell = 28378; break;       // Rank 2
                    case 19309: spell = 28379; break;       // Rank 3
                    case 19310: spell = 28380; break;       // Rank 4
                    case 19311: spell = 28381; break;       // Rank 5
                    case 19312: spell = 28382; break;       // Rank 6
                    case 25477: spell = 28385; break;       // Rank 7
                    default:
                        sLog.outError("Unit::HandleProcTriggerSpell: Spell %u not handled in SG",triggeredByAura->GetSpellProto()->Id);
                        return;
                }
                CastSpell(pVictim, spell, true, castItem, triggeredByAura);
                return;
            }
            break;
        }
        case SPELLFAMILY_DRUID:
        {
            // Leader of the Pack (triggering Improved Leader of the Pack heal)
            // Cooldown: 6 secs
            if(auraSpellInfo->SpellFamilyFlags & 0x00080000000000LL)
            {
                if (triggeredByAura->GetModifier()->m_amount == 0)
                    return;
                int32 improvedLotPBasePoints0 = triggeredByAura->GetModifier()->m_amount * GetMaxHealth() / 100;
                CastCustomSpell(this, 34299, &improvedLotPBasePoints0, NULL, NULL, true, castItem, triggeredByAura);
                if (GetTypeId() == TYPEID_PLAYER)
                    ((Player*)this)->AddSpellCooldown(34299,0,time(NULL) + 6);
                return;
            }
            break;
        }
        case SPELLFAMILY_ROGUE:
        {
            if(auraSpellInfo->SpellFamilyFlags == 0x0000000000000000LL)
            {
                switch(auraSpellInfo->SpellIconID)
                {
                    // Combat Potency
                    case 2260:
                    {
                        // skip non offhand attacks
                        if(attackType!=OFF_ATTACK)
                            return;

                        // fall through for offhand case
                        break;
                    }
                    // Armor Penetration
                    case 37173:
                    {
                        // hidden cooldown 30sec, check manually for passive spell
                        if (GetTypeId() == TYPEID_PLAYER && !((Player*)this)->HasSpellCooldown(auraSpellInfo->Id))
                        {
                            CastSpell(this, trigger_spell_id, true, NULL, triggeredByAura);
                            ((Player*)this)->AddSpellCooldown(auraSpellInfo->Id,0,time(NULL) + 30);
                        }
                        return;
                    }
                }
            }
            break;
        }
        case SPELLFAMILY_PALADIN:
        {
            if(auraSpellInfo->SpellFamilyFlags == 0x00000000LL)
            {
                switch(auraSpellInfo->Id)
                {
                    // Lightning Capacitor
                    case 37657:
                    {
                        // trinket ProcTriggerSpell but for safe checks for player
                        if(!castItem || !pVictim || !pVictim->isAlive() || GetTypeId()!=TYPEID_PLAYER)
                            return;

                        if(((Player*)this)->HasSpellCooldown(37657))
                            return;

                        // stacking
                        CastSpell(this, 37658, true, castItem, triggeredByAura);
                        // 2.5s cooldown before it can stack again, current system allow 1 sec step in cooldown
                        ((Player*)this)->AddSpellCooldown(37657,0,time(NULL)+(roll_chance_i(50) ? 2 : 3));
                        

                        // counting
                        uint32 count = 0;
                        AuraList const& dummyAura = GetAurasByType(SPELL_AURA_DUMMY);
                        for(AuraList::const_iterator itr = dummyAura.begin(); itr != dummyAura.end(); ++itr)
                            if((*itr)->GetId()==37658)
                                ++count;

                        // release at 3 aura in stack
                        if(count >2)
                        {
                            RemoveAurasDueToSpell(37658);
                            CastSpell(pVictim, 37661, true, castItem, triggeredByAura);
                        }
                        return;
                    }
                    // Healing Discount
                    case 37705:
                    {
                        CastSpell(this, 37706, true, castItem, triggeredByAura);
                        return;
                    }
                }
                switch(auraSpellInfo->SpellIconID)
                {
                    case 241:
                    {
                        switch(auraSpellInfo->EffectTriggerSpell[0])
                        {
                            //Illumination
                            case 18350:
                            {
                                if(!procSpell)
                                    return;

                                // procspell is triggered spell but we need mana cost of original casted spell
                                uint32 originalSpellId = procSpell->Id;
                                if(procSpell->SpellFamilyName == SPELLFAMILY_PALADIN)
                                {
                                    if(procSpell->SpellFamilyFlags & 0x00200000)
                                    {
                                        switch(procSpell->Id)
                                        {
                                            case 25914: originalSpellId = 20473; break;
                                            case 25913: originalSpellId = 20929; break;
                                            case 25903: originalSpellId = 20930; break;
                                            case 27175: originalSpellId = 27174; break;
                                            case 33074: originalSpellId = 33072; break;
                                            default:
                                                sLog.outError("Unit::HandleProcTriggerSpell: Spell %u not handled in HShock",procSpell->Id);
                                                return;
                                        }
                                    }
                                }

                                SpellEntry const *originalSpell = sSpellStore.LookupEntry(originalSpellId);
                                if(!originalSpell)
                                {
                                    sLog.outError("Unit::HandleProcTriggerSpell: Spell %u unknown but selected as original in Illu",originalSpellId);
                                    return;
                                }

                                // percent stored in effect 1 (class scripts) base points
                                int32 percent = auraSpellInfo->EffectBasePoints[1]+1;

                                int32 ILManaSpellBasePoints0 = originalSpell->manaCost*percent/100;
                                CastCustomSpell(this, 20272, &ILManaSpellBasePoints0, NULL, NULL, true, castItem, triggeredByAura);
                                return;
                            }
                        }
                        break;
                    }
                }
            }
            if(auraSpellInfo->SpellFamilyFlags & 0x00080000)
            {
                switch(auraSpellInfo->SpellIconID)
                {
                    //Judgement of Wisdom (overwrite non existing triggered spell call in spell.dbc
                    case 206:
                    {
                        if(!pVictim || !pVictim->isAlive())
                            return;

                        uint32 spell = 0;
                        switch(triggeredByAura->GetSpellProto()->Id)
                        {
                                                            // Rank 1
                            case 20186: spell = 20268; break;
                                                            // Rank 2
                            case 20354: spell = 20352; break;
                                                            // Rank 3
                            case 20355: spell = 20353; break;
                                                            // Rank 4
                            case 27164: spell = 27165; break;
                            default:
                                sLog.outError("Unit::HandleProcTriggerSpell: Spell %u not handled in JoW",triggeredByAura->GetSpellProto()->Id);
                                return;
                        }
                        pVictim->CastSpell(pVictim, spell, true, castItem, triggeredByAura);
                        return;
                    }
                    //Judgement of Light (overwrite non existing triggered spell call in spell.dbc
                    case 299:
                    {
                        if(!pVictim || !pVictim->isAlive())
                            return;

                        uint32 spell = 0;
                        switch(triggeredByAura->GetSpellProto()->Id)
                        {
                                                            // Rank 1
                            case 20185: spell = 20267; break;
                                                            // Rank 2
                            case 20344: spell = 20341; break;
                                                            // Rank 3
                            case 20345: spell = 20342; break;
                                                            // Rank 4
                            case 20346: spell = 20343; break;
                                                            // Rank 5
                            case 27162: spell = 27163; break;
                            default:
                                sLog.outError("Unit::HandleProcTriggerSpell: Spell %u not handled in JoL",triggeredByAura->GetSpellProto()->Id);
                                return;
                        }
                        pVictim->CastSpell(pVictim, spell, true, castItem, triggeredByAura);
                        return;
                    }
                }
            }
            break;
        }
        case SPELLFAMILY_SHAMAN:
        {
            if(auraSpellInfo->SpellFamilyFlags == 0x0000000000000000)
            {
                switch(auraSpellInfo->SpellIconID)
                {
                    // Mana Surge (Shaman T1 bonus)
                    case 87:
                    {
                        if(!procSpell)
                            return;

                        int32 manaSurgeSpellBasePoints0 = procSpell->manaCost * 35/100;
                        CastCustomSpell(this, 23571, &manaSurgeSpellBasePoints0, NULL, NULL, true, castItem, triggeredByAura);
                        return;
                    }
                    //Nature's Guardian
                    case 2013:
                    {
                        //Cooldown: 5 secs
                        if(GetTypeId()!=TYPEID_PLAYER)
                            return;

                        if(((Player*)this)->HasSpellCooldown(auraSpellInfo->Id))
                            return;

                        // damage taken that reduces below 30% health
                        if (10*(int32(GetHealth())-int32(damage)) < 3*GetMaxHealth() && 10*(GetHealth()) >= 3*GetMaxHealth())
                        {
                            int32 healPoints = triggeredByAura->GetModifier()->m_amount * GetMaxHealth() / 100;
                            CastCustomSpell(this, 31616, &healPoints, NULL, NULL, true, castItem, triggeredByAura);
                            if(pVictim && pVictim->isAlive())
                                pVictim->getThreatManager().modifyThreatPercent(this,-10);
                            ((Player*)this)->AddSpellCooldown(auraSpellInfo->Id, 0, time(NULL) + 5);
                        }
                        return;
                    }
                }
            }

            // Lightning Shield (overwrite non existing triggered spell call in spell.dbc
            if((auraSpellInfo->SpellFamilyFlags & 0x00000400) && auraSpellInfo->SpellVisual==37)
            {
                if(!pVictim || !pVictim->isAlive())
                    return;

                uint32 spell = 0;
                switch(triggeredByAura->GetSpellProto()->Id)
                {
                    case   324: spell = 26364; break;       // Rank 1
                    case   325: spell = 26365; break;       // Rank 2
                    case   905: spell = 26366; break;       // Rank 3
                    case   945: spell = 26367; break;       // Rank 4
                    case  8134: spell = 26369; break;       // Rank 5
                    case 10431: spell = 26370; break;       // Rank 6
                    case 10432: spell = 26363; break;       // Rank 7
                    case 25469: spell = 26371; break;       // Rank 8
                    case 25472: spell = 26372; break;       // Rank 9
                    default:
                        sLog.outError("Unit::HandleProcTriggerSpell: Spell %u not handled in LShield",triggeredByAura->GetSpellProto()->Id);
                        return;
                }

                if (GetTypeId() == TYPEID_PLAYER)
                {
                    if(((Player*)this)->HasSpellCooldown(spell))
                    {
                        ++triggeredByAura->m_procCharges;   // restore charges for proc triggered in cooldown time
                        return;
                    }

                    CastSpell(pVictim, spell, true, castItem, triggeredByAura);

                    // 3 secs cooldown
                    ((Player*)this)->AddSpellCooldown(spell,0,time(NULL) + 3);
                }
                else
                    CastSpell(pVictim, spell, true, castItem, triggeredByAura);

                return;
            }
            break;
        }
    }

    // standard non-dummy case
    if(!trigger_spell_id)
    {
        sLog.outError("Unit::HandleProcTriggerSpell: Spell %u have 0 in EffectTriggered[%d], not handled custom case?",auraSpellInfo->Id,triggeredByAura->GetEffIndex());
        return;
    }

    if(!sSpellStore.LookupEntry(trigger_spell_id))
    {
        sLog.outError("Unit::HandleProcTriggerSpell: Spell %u have not existed EffectTriggered[%d]=%u, not handled custom case?",auraSpellInfo->Id,triggeredByAura->GetEffIndex(),trigger_spell_id);
        return;
    }

    // custom check for proc spell
    switch(auraSpellInfo->Id)
    {
        // Mystical Skyfire Diamond trigger (Spell Focus Trigger)
        case 32837:
        {
            // hidden cooldown 45sec, check manually for passive spell
            if (GetTypeId() == TYPEID_PLAYER && !((Player*)this)->HasSpellCooldown(auraSpellInfo->Id))
            {
                CastSpell(this, trigger_spell_id, true, castItem, triggeredByAura);
                ((Player*)this)->AddSpellCooldown(auraSpellInfo->Id,0,time(NULL) + 45);
            }
            return;
        }

        // Thundering Skyfire Diamond trigger (Skyfire Swiftness)
        case 39958:
        {
            // hidden cooldown 40sec, check manually for passive spell
            if (GetTypeId() == TYPEID_PLAYER && !((Player*)this)->HasSpellCooldown(auraSpellInfo->Id))
            {
                CastSpell(this, trigger_spell_id, true, castItem, triggeredByAura);
                ((Player*)this)->AddSpellCooldown(auraSpellInfo->Id,0,time(NULL) + 40);
            }
            return;
        }

        // 10s cooldowns
        case 33746:                                         // Essence Infused Mushroom
        case 33759:                                         // Power Infused Mushroom
        {
            // hidden cooldown 10sec, check manually for passive spell
            if (GetTypeId() == TYPEID_PLAYER && !((Player*)this)->HasSpellCooldown(auraSpellInfo->Id))
            {
                CastSpell(this, trigger_spell_id, true, NULL, triggeredByAura);
                ((Player*)this)->AddSpellCooldown(auraSpellInfo->Id,0,time(NULL) + 10);
            }
            return;
        }

        // 60s cooldowns
        case 35077:                                         // Band of the Eternal Defender
        case 35080:                                         // Band of the Eternal Champion
        case 35083:                                         // Band of the Eternal Sage
        case 35086:                                         // Band of the Eternal Restorer
        case 37189:                                         // Recuced Holy Light Cast Time (name not have typos ;) )
        case 37227:                                         // Improved Healing Wave
        case 37655:                                         // Bonus Mana Regen
        case 38334:                                         // Proc Mana Regen
        {
            // hidden cooldown 60sec, check manually for passive spell
            if (GetTypeId() == TYPEID_PLAYER && !((Player*)this)->HasSpellCooldown(auraSpellInfo->Id))
            {
                CastSpell(this, trigger_spell_id, true, NULL, triggeredByAura);
                ((Player*)this)->AddSpellCooldown(auraSpellInfo->Id,0,time(NULL) + 60);
            }
            return;
        }

        // Bonus Healing (item spell)
        case 40971:
        {
            if(!pVictim || !pVictim->isAlive())
                return;

            // bonus if health < 50%
            if(pVictim->GetHealth() >= pVictim->GetMaxHealth()*triggeredByAura->GetModifier()->m_amount/100)
                return;

            // cast at target positive spell
            CastSpell(pVictim,trigger_spell_id,true,castItem,triggeredByAura);
            return;
        }
    }

    // but with dummy basepoints or other customs
    switch(trigger_spell_id)
    {
        // Setup
        case 15250:
        {
            // applied only for main target
            if(!pVictim || pVictim != getVictim())
                return;

            // continue normal case
            break;
        }
        // Seal of Command
        case 20424:
            // prevent chain of triggered spell from same triggered spell
            if(procSpell && procSpell->Id==20424)
                return;
            break;
            // Shamanistic Rage triggered spell
        case 30824:
        {
            int32 SRBasePoints0 = int32(GetTotalAttackPowerValue(BASE_ATTACK)*triggeredByAura->GetModifier()->m_amount/100);
            CastCustomSpell(this, 30824, &SRBasePoints0, NULL, NULL, true, castItem, triggeredByAura);
            return;
        }
        // Backlash triggered spell
        case 34936:
        {
            // need set custom cooldown
            if(isAlive() && GetTypeId()==TYPEID_PLAYER && !((Player*)this)->HasSpellCooldown(34936))
            {
                CastSpell(this,trigger_spell_id,true,castItem,triggeredByAura);
                ((Player*)this)->AddSpellCooldown(34936,0,time(NULL)+8);
            }
            return;
        }
    }

    // default case
    if(IsPositiveSpell(trigger_spell_id) && !(procFlags & PROC_FLAG_HEAL))
        CastSpell(this,trigger_spell_id,true,castItem,triggeredByAura);
    else if(pVictim && pVictim->isAlive())
        CastSpell(pVictim,trigger_spell_id,true,castItem,triggeredByAura);
}

void Unit::setPowerType(Powers new_powertype)
{
    SetByteValue(UNIT_FIELD_BYTES_0, 3, new_powertype);

    if(GetTypeId() == TYPEID_PLAYER)
    {
        if(((Player*)this)->GetGroup())
            ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_POWER_TYPE);
    }
    else if(((Creature*)this)->isPet())
    {
        Pet *pet = ((Pet*)this);
        if(pet->isControlled())
        {
            Unit *owner = GetOwner();
            if(owner && (owner->GetTypeId() == TYPEID_PLAYER) && ((Player*)owner)->GetGroup())
                ((Player*)owner)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_POWER_TYPE);
        }
    }

    switch(new_powertype)
    {
        default:
        case POWER_MANA:
            break;
        case POWER_RAGE:
            SetMaxPower(POWER_RAGE,GetCreatePowers(POWER_RAGE));
            SetPower(   POWER_RAGE,0);
            break;
        case POWER_FOCUS:
            SetMaxPower(POWER_FOCUS,GetCreatePowers(POWER_FOCUS));
            SetPower(   POWER_FOCUS,GetCreatePowers(POWER_FOCUS));
            break;
        case POWER_ENERGY:
            SetMaxPower(POWER_ENERGY,GetCreatePowers(POWER_ENERGY));
            SetPower(   POWER_ENERGY,0);
            break;
        case POWER_HAPPINESS:
            SetMaxPower(POWER_HAPPINESS,GetCreatePowers(POWER_HAPPINESS));
            SetPower(POWER_HAPPINESS,GetCreatePowers(POWER_HAPPINESS));
            break;
    }
}

FactionTemplateEntry const* Unit::getFactionTemplateEntry() const
{
    FactionTemplateEntry const* entry = sFactionTemplateStore.LookupEntry(getFaction());
    if(!entry)
    {
        static uint64 guid = 0;                             // prevent repeating spam same faction problem

        if(GetGUID() != guid)
        {
            if(GetTypeId() == TYPEID_PLAYER)
                sLog.outError("Player %s have invalid faction (faction template id) #%u", ((Player*)this)->GetName(), getFaction());
            else
                sLog.outError("Creature (template id: %u) have invalid faction (faction template id) #%u", ((Creature*)this)->GetCreatureInfo()->Entry, getFaction());
            guid = GetGUID();
        }
    }
    return entry;
}

bool Unit::IsHostileTo(Unit const* unit) const
{
    // always non-hostile to self
    if(unit==this)
        return false;

    // always non-hostile to GM in GM mode
    if(unit->GetTypeId()==TYPEID_PLAYER && ((Player*)unit)->isGameMaster())
        return false;

    // always hostile to enemy
    if(getVictim()==unit || unit->getVictim()==this)
        return true;

    // test pet/charm masters instead pers/charmeds
    Unit const* testerOwner = GetCharmerOrOwner();
    Unit const* targetOwner = unit->GetCharmerOrOwner();

    // always hostile to owner's enemy
    if(testerOwner && (testerOwner->getVictim()==unit || unit->getVictim()==testerOwner))
        return true;

    // always hostile to enemy owner
    if(targetOwner && (getVictim()==targetOwner || targetOwner->getVictim()==this))
        return true;

    // always hostile to owner of owner's enemy
    if(testerOwner && targetOwner && (testerOwner->getVictim()==targetOwner || targetOwner->getVictim()==testerOwner))
        return true;

    Unit const* tester = testerOwner ? testerOwner : this;
    Unit const* target = targetOwner ? targetOwner : unit;

    // always non-hostile to target with common owner, or to owner/pet
    if(tester==target)
        return false;

    // special cases (Duel, etc)
    if(tester->GetTypeId()==TYPEID_PLAYER && target->GetTypeId()==TYPEID_PLAYER)
    {
        // Duel
        if(((Player const*)tester)->duel && ((Player const*)tester)->duel->opponent == target && ((Player const*)tester)->duel->startTime != 0)
            return true;

        // Group
        if(((Player*)tester)->GetGroup() && ((Player*)tester)->GetGroup()==((Player*)target)->GetGroup())
            return false;

        // PvP FFA state
        if(((Player*)tester)->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_FFA_PVP) && ((Player*)target)->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_FFA_PVP))
            return true;

        //= PvP states
        // Green/Blue (can't attack)
        if(((Player*)tester)->GetTeam()==((Player*)target)->GetTeam())
            return false;

        // Red (can attack) if true, Blue/Yellow (can't attack) in another case
        return ((Player*)tester)->IsPvP() && ((Player*)target)->IsPvP();
    }

    // faction base cases
    FactionTemplateEntry const*tester_faction = tester->getFactionTemplateEntry();
    FactionTemplateEntry const*target_faction = target->getFactionTemplateEntry();
    if(!tester_faction || !target_faction)
        return false;

    // PvC forced reaction and reputation case
    if(tester->GetTypeId()==TYPEID_PLAYER)
    {
        // forced reaction
        ForcedReactions::const_iterator forceItr = ((Player*)tester)->m_forcedReactions.find(target_faction->faction);
        if(forceItr!=((Player*)tester)->m_forcedReactions.end())
        {
            return forceItr->second <= REP_HOSTILE;
        }

        // apply reputation state
        FactionEntry const* raw_target_faction = sFactionStore.LookupEntry(target_faction->faction);
        if(raw_target_faction && raw_target_faction->reputationListID >=0 )
        {
            if(((Player*)tester)->IsFactionAtWar(raw_target_faction))
                return true;
        }
    }
    // CvP forced reaction and reputation case
    else if(target->GetTypeId()==TYPEID_PLAYER)
    {
        // forced reaction
        ForcedReactions::const_iterator forceItr = ((Player*)target)->m_forcedReactions.find(tester_faction->faction);
        if(forceItr!=((Player*)target)->m_forcedReactions.end())
        {
            return forceItr->second <= REP_HOSTILE;
        }

        // apply reputation state
        FactionEntry const* raw_tester_faction = sFactionStore.LookupEntry(tester_faction->faction);
        if(raw_tester_faction && raw_tester_faction->reputationListID >=0 )
        {
            return ((Player*)target)->GetReputationRank(raw_tester_faction) <= REP_HOSTILE;
        }
    }

    // common faction based case (CvC,PvC,CvP)
    return tester_faction->IsHostileTo(*target_faction);
}

bool Unit::IsFriendlyTo(Unit const* unit) const
{
    // always friendly to self
    if(unit==this)
        return true;

    // always friendly to GM in GM mode
    if(unit->GetTypeId()==TYPEID_PLAYER && ((Player*)unit)->isGameMaster())
        return true;

    // always non-friendly to enemy
    if(getVictim()==unit || unit->getVictim()==this)
        return false;

    // test pet/charm masters instead pers/charmeds
    Unit const* testerOwner = GetCharmerOrOwner();
    Unit const* targetOwner = unit->GetCharmerOrOwner();

    // always non-friendly to owner's enemy
    if(testerOwner && (testerOwner->getVictim()==unit || unit->getVictim()==testerOwner))
        return false;

    // always non-friendly to enemy owner
    if(targetOwner && (getVictim()==targetOwner || targetOwner->getVictim()==this))
        return false;

    // always non-friendly to owner of owner's enemy
    if(testerOwner && targetOwner && (testerOwner->getVictim()==targetOwner || targetOwner->getVictim()==testerOwner))
        return false;

    Unit const* tester = testerOwner ? testerOwner : this;
    Unit const* target = targetOwner ? targetOwner : unit;

    // always friendly to target with common owner, or to owner/pet
    if(tester==target)
        return true;

    // special cases (Duel)
    if(tester->GetTypeId()==TYPEID_PLAYER && target->GetTypeId()==TYPEID_PLAYER)
    {
        // Duel
        if(((Player const*)tester)->duel && ((Player const*)tester)->duel->opponent == target && ((Player const*)tester)->duel->startTime != 0)
            return false;

        // Group
        if(((Player*)tester)->GetGroup() && ((Player*)tester)->GetGroup()==((Player*)target)->GetGroup())
            return true;

        // PvP FFA state
        if(((Player*)tester)->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_FFA_PVP) && ((Player*)target)->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_FFA_PVP))
            return false;

        //= PvP states
        // Green/Blue (non-attackable)
        if(((Player*)tester)->GetTeam()==((Player*)target)->GetTeam())
            return true;

        // Blue (friendly/non-attackable) if not PVP, or Yellow/Red in another case (attackable)
        return !((Player*)target)->IsPvP();
    }

    // faction base cases
    FactionTemplateEntry const*tester_faction = tester->getFactionTemplateEntry();
    FactionTemplateEntry const*target_faction = target->getFactionTemplateEntry();
    if(!tester_faction || !target_faction)
        return false;

    // PvC forced reaction and reputation case
    if(tester->GetTypeId()==TYPEID_PLAYER)
    {
        // forced reaction
        ForcedReactions::const_iterator forceItr = ((Player*)tester)->m_forcedReactions.find(target_faction->faction);
        if(forceItr!=((Player*)tester)->m_forcedReactions.end())
        {
            return forceItr->second >= REP_FRIENDLY;
        }

        // apply reputation state
        FactionEntry const* raw_target_faction = sFactionStore.LookupEntry(target_faction->faction);
        if(raw_target_faction && raw_target_faction->reputationListID >=0 )
        {
            if(((Player*)tester)->IsFactionAtWar(raw_target_faction))
                return false;
        }
    }
    // CvP forced reaction and reputation case
    else if(target->GetTypeId()==TYPEID_PLAYER)
    {
        // forced reaction
        ForcedReactions::const_iterator forceItr = ((Player*)target)->m_forcedReactions.find(tester_faction->faction);
        if(forceItr!=((Player*)target)->m_forcedReactions.end())
        {
            return forceItr->second >= REP_FRIENDLY;
        }

        // apply reputation state
        FactionEntry const* raw_tester_faction = sFactionStore.LookupEntry(tester_faction->faction);
        if(raw_tester_faction && raw_tester_faction->reputationListID >=0 )
        {
            return ((Player*)target)->GetReputationRank(raw_tester_faction) >= REP_FRIENDLY;
        }
    }

    // common faction based case (CvC,PvC,CvP)
    return tester_faction->IsFriendlyTo(*target_faction);
}

bool Unit::IsHostileToPlayers() const
{
    FactionTemplateEntry const* my_faction = getFactionTemplateEntry();
    if(!my_faction)
        return false;

    FactionEntry const* raw_faction = sFactionStore.LookupEntry(my_faction->faction);
    if(raw_faction && raw_faction->reputationListID >=0 )
        return false;

    return my_faction->IsHostileToPlayers();
}

bool Unit::IsNeutralToAll() const
{
    FactionTemplateEntry const* my_faction = getFactionTemplateEntry();
    if(!my_faction)
        return true;

    FactionEntry const* raw_faction = sFactionStore.LookupEntry(my_faction->faction);
    if(raw_faction && raw_faction->reputationListID >=0 )
        return false;

    return my_faction->IsNeutralToAll();
}

bool Unit::Attack(Unit *victim, bool playerMeleeAttack)
{
    if(!victim || victim == this)
        return false;

    // player cannot attack in mount state
    if(GetTypeId()==TYPEID_PLAYER && IsMounted())
        return false;

    // nobody can attack GM in GM-mode
    if(victim->GetTypeId()==TYPEID_PLAYER)
    {
        if(((Player*)victim)->isGameMaster())
            return false;
    }
    else
    {
        if(((Creature*)victim)->IsInEvadeMode())
            return false;
    }

    // remove SPELL_AURA_MOD_UNATTACKABLE at attack (in case non-interruptible spells stun aura applied also that not let attack)
    if(HasAuraType(SPELL_AURA_MOD_UNATTACKABLE))
        RemoveSpellsCausingAura(SPELL_AURA_MOD_UNATTACKABLE);

    if (m_attacking)
    {
        if (m_attacking == victim)
            return false;
        AttackStop();
    }

    //Set our target
    SetUInt64Value(UNIT_FIELD_TARGET, victim->GetGUID());

    addUnitState(UNIT_STAT_ATTACKING);
    SetInCombat();
    m_attacking = victim;
    m_attacking->_addAttacker(this);

    if(m_attacking->GetTypeId()==TYPEID_UNIT && ((Creature*)m_attacking)->AI())
        ((Creature*)m_attacking)->AI()->AttackedBy(this);

    if(GetTypeId()==TYPEID_UNIT)
    {
        WorldPacket data(SMSG_AI_REACTION, 12);
        data << GetGUID();
        data << uint32(AI_REACTION_AGGRO);                  // Aggro sound
        ((WorldObject*)this)->SendMessageToSet(&data, true);
    }

    if( GetTypeId()==TYPEID_UNIT && !(((Creature*)this)->isPet() || isCharmed()) )
    {
        ((Creature*)this)->CallAssistence();
    }

    // delay offhand weapon attack to next attack time
    if(haveOffhandWeapon())
        resetAttackTimer(OFF_ATTACK);

    if(GetTypeId()!=TYPEID_PLAYER || playerMeleeAttack)
        SendAttackStart(victim);

    return true;
}

bool Unit::AttackStop()
{
    if (!m_attacking)
        return false;

    Unit* victim = m_attacking;

    m_attacking->_removeAttacker(this);
    m_attacking = NULL;

    //Clear our target
    SetUInt64Value(UNIT_FIELD_TARGET, 0);

    clearUnitState(UNIT_STAT_ATTACKING);

    InterruptSpell(CURRENT_MELEE_SPELL);

    if( GetTypeId()==TYPEID_UNIT )
    {
        // reset call assistance
        ((Creature*)this)->SetNoCallAssistence(false);
    }

    SendAttackStop(victim);

    return true;
}

bool Unit::isAttackingPlayer() const
{
    if(getVictim())
    {
        if(getVictim()->GetTypeId() == TYPEID_PLAYER)
            return true;

        if(IS_PLAYER_GUID(getVictim()->GetOwnerGUID()))
            return true;
    }

    Pet* pet = GetPet();
    if(pet && pet->isAttackingPlayer())
        return true;

    Unit* charmed = GetCharm();
    if(charmed && charmed->isAttackingPlayer())
        return true;

    for (int8 i = 0; i < 4; i++)
    {
        if(m_TotemSlot[i])
        {
            Creature *totem = ObjectAccessor::GetCreature(*this, m_TotemSlot[i]);
            if(totem && totem->isAttackingPlayer())
                return true;
        }
    }

    return false;
}

void Unit::RemoveAllAttackers()
{
    while (!m_attackers.empty())
    {
        AttackerSet::iterator iter = m_attackers.begin();
        if(!(*iter)->AttackStop())
        {
            sLog.outError("WORLD: Unit has an attacker that isnt attacking it!");
            m_attackers.erase(iter);
        }
    }
}

void Unit::ModifyAuraState(AuraState flag, bool apply)
{
    if (apply)
    {
        if (!HasFlag(UNIT_FIELD_AURASTATE, 1<<(flag-1)))
        {
            SetFlag(UNIT_FIELD_AURASTATE, 1<<(flag-1));
            if(GetTypeId() == TYPEID_PLAYER)
            {
                const PlayerSpellMap& sp_list = ((Player*)this)->GetSpellMap();
                for (PlayerSpellMap::const_iterator itr = sp_list.begin(); itr != sp_list.end(); ++itr)
                {
                    if(itr->second->state == PLAYERSPELL_REMOVED) continue;
                    SpellEntry const *spellInfo = sSpellStore.LookupEntry(itr->first);
                    if (!spellInfo || !IsPassiveSpell(itr->first)) continue;
                    if (spellInfo->CasterAuraState == flag)
                        CastSpell(this, itr->first, true, NULL);
                }
            }
        }
    }
    else
    {
        if (HasFlag(UNIT_FIELD_AURASTATE,1<<(flag-1)))
        {
            RemoveFlag(UNIT_FIELD_AURASTATE, 1<<(flag-1));
            Unit::AuraMap& tAuras = GetAuras();
            for (Unit::AuraMap::iterator itr = tAuras.begin(); itr != tAuras.end();)
            {
                SpellEntry const* spellProto = (*itr).second->GetSpellProto();
                if (spellProto->CasterAuraState == flag)
                {
                    // exceptions (applied at state but not removed at state change)
                    if(spellProto->SpellIconID==2006 && spellProto->SpellFamilyName==SPELLFAMILY_WARRIOR && spellProto->SpellFamilyFlags==0x100000)
                    {
                        ++itr;
                        continue;
                    }

                    RemoveAura(itr);
                }
                else
                    ++itr;
            }
        }
    }
}

Unit *Unit::GetOwner() const
{
    uint64 ownerid = GetOwnerGUID();
    if(!ownerid)
        return NULL;
    return ObjectAccessor::GetUnit(*this, ownerid);
}

Unit *Unit::GetCharmer() const
{
    uint64 charmerid = GetCharmerGUID();
    if(!charmerid)
        return NULL;
    return ObjectAccessor::GetUnit(*this, charmerid);
}

Pet* Unit::GetPet() const
{
    uint64 pet_guid = GetPetGUID();
    if(pet_guid)
    {
        Pet* pet = ObjectAccessor::GetPet(pet_guid);
        if(!pet)
        {
            sLog.outError("Unit::GetPet: Pet %u not exist.",GUID_LOPART(pet_guid));
            const_cast<Unit*>(this)->SetPet(0);
            return NULL;
        }
        return pet;
    }

    return NULL;
}

Unit* Unit::GetCharm() const
{
    uint64 charm_guid = GetCharmGUID();
    if(charm_guid)
    {
        Unit* pet = ObjectAccessor::GetUnit(*this, charm_guid);
        if(!pet)
        {
            sLog.outError("Unit::GetCharm: Charmed creature %u not exist.",GUID_LOPART(charm_guid));
            const_cast<Unit*>(this)->SetCharm(0);
        }
        return pet;
    }
    else
        return NULL;
}

void Unit::SetPet(Pet* pet)
{
    SetUInt64Value(UNIT_FIELD_SUMMON,pet ? pet->GetGUID() : 0);

    if(pet)
    {
        for(int i = 0; i < MAX_MOVE_TYPE; ++i)
        {
            pet->SetSpeed(UnitMoveType(i),m_speed_rate[i],true);
        }
    }
}

void Unit::SetCharm(Unit* charmed)
{
    SetUInt64Value(UNIT_FIELD_CHARM,charmed ? charmed->GetGUID() : 0);
}

void Unit::UnsummonAllTotems()
{
    for (int8 i = 0; i < 4; ++i)
    {
        if(!m_TotemSlot[i])
            continue;

        Creature *OldTotem = ObjectAccessor::GetCreature(*this, m_TotemSlot[i]);
        if (OldTotem && OldTotem->isTotem())
            ((Totem*)OldTotem)->UnSummon();
    }
}

void Unit::SendHealSpellLog(Unit *pVictim, uint32 SpellID, uint32 Damage, bool critical)
{
    // we guess size
    WorldPacket data(SMSG_SPELLHEALLOG, (8+8+4+4+1));
    data.append(pVictim->GetPackGUID());
    data.append(GetPackGUID());
    data << uint32(SpellID);
    data << uint32(Damage);
    data << uint8(critical ? 1 : 0);
    data << uint8(0);                                       // unused in client?
    SendMessageToSet(&data, true);
}

void Unit::SendEnergizeSpellLog(Unit *pVictim, uint32 SpellID, uint32 Damage, Powers powertype, bool critical)
{
    WorldPacket data(SMSG_SPELLENERGIZELOG, (8+8+4+4+4+1));
    data.append(pVictim->GetPackGUID());
    data.append(GetPackGUID());
    data << uint32(SpellID);
    data << uint32(powertype);
    data << uint32(Damage);
    //data << uint8(critical ? 1 : 0);                      // removed in 2.4.0
    SendMessageToSet(&data, true);
}

uint32 Unit::SpellDamageBonus(Unit *pVictim, SpellEntry const *spellProto, uint32 pdamage, DamageEffectType damagetype)
{
    if(!spellProto || !pVictim || damagetype==DIRECT_DAMAGE )
        return pdamage;

    // For totems get damage bonus from owner
    if( GetTypeId()==TYPEID_UNIT && ((Creature*)this)->isTotem())
        if(Unit* owner = GetOwner())
            return owner->SpellDamageBonus(pVictim, spellProto, pdamage, damagetype);

    bool Channeled = IsChanneledSpell(spellProto);

    // Damage Done
    uint32 CastingTime;
    if ( !Channeled )
        CastingTime = GetSpellCastTime(sCastTimesStore.LookupEntry(spellProto->CastingTimeIndex));
    else
        CastingTime = GetSpellDuration(spellProto);

    if (CastingTime > 7000) CastingTime = 7000;             // Plus Damage efficient maximum 200% ( 7.0 seconds )
    if (CastingTime < 1500) CastingTime = 1500;

    // Taken/Done fixed damage bonus auras
    int32 DoneAdvertisedBenefit  = SpellBaseDamageBonus(GetSpellSchoolMask(spellProto));
    int32 TakenAdvertisedBenefit = SpellBaseDamageBonusForVictim(GetSpellSchoolMask(spellProto), pVictim);

    // Damage over Time spells bonus calculation
    float DotFactor = 1.0f;
    if(damagetype == DOT)
    {
        if ( !Channeled )
            CastingTime = 3500;

        int32 DotDuration = GetSpellDuration(spellProto);
        // 200% limit
        if(DotDuration > 0)
        {
            if(DotDuration > 30000) DotDuration = 30000;
            if(!Channeled) DotFactor = DotDuration / 15000.0f;
            int x = 0;
            for(int j = 0; j < 3; j++)
            {
                if(spellProto->Effect[j] == SPELL_EFFECT_APPLY_AURA &&
                    (spellProto->EffectApplyAuraName[j] == SPELL_AURA_PERIODIC_DAMAGE ||
                    spellProto->EffectApplyAuraName[j] == SPELL_AURA_PERIODIC_LEECH) )
                {
                    x = j;
                    break;
                }
            }
            int DotTicks = 6;
            if(spellProto->EffectAmplitude[x] != 0)
                DotTicks = DotDuration / spellProto->EffectAmplitude[x];
            if(DotTicks)
            {
                DoneAdvertisedBenefit /= DotTicks;
                TakenAdvertisedBenefit /= DotTicks;
            }
        }
    }

    // Taken/Done total percent damage auras
    float DoneTotalMod = 1.0f;
    float TakenTotalMod = 1.0f;

    // ..done
    AuraList const& mModDamagePercentDone = GetAurasByType(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
    for(AuraList::const_iterator i = mModDamagePercentDone.begin(); i != mModDamagePercentDone.end(); ++i)
    {
        if( ((*i)->GetModifier()->m_miscvalue & GetSpellSchoolMask(spellProto)) &&
            (*i)->GetSpellProto()->EquippedItemClass == -1 &&
                                                            // -1 == any item class (not wand then)
            (*i)->GetSpellProto()->EquippedItemInventoryTypeMask == 0 )
                                                            // 0 == any inventory type (not wand then)
        {
            DoneTotalMod *= ((*i)->GetModifier()->m_amount+100.0f)/100.0f;
        }
    }

    uint32 creatureTypeMask = pVictim->GetCreatureTypeMask();
    AuraList const& mDamageDoneVersus = GetAurasByType(SPELL_AURA_MOD_DAMAGE_DONE_VERSUS);
    for(AuraList::const_iterator i = mDamageDoneVersus.begin();i != mDamageDoneVersus.end(); ++i)
        if(creatureTypeMask & uint32((*i)->GetModifier()->m_miscvalue))
            DoneTotalMod *= ((*i)->GetModifier()->m_amount+100.0f)/100.0f;

    // ..taken
    AuraList const& mModDamagePercentTaken = pVictim->GetAurasByType(SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN);
    for(AuraList::const_iterator i = mModDamagePercentTaken.begin(); i != mModDamagePercentTaken.end(); ++i)
        if( (*i)->GetModifier()->m_miscvalue & GetSpellSchoolMask(spellProto) )
            TakenTotalMod *= ((*i)->GetModifier()->m_amount+100.0f)/100.0f;

    // .. taken pct: scripted (increases damage of * against targets *)
    AuraList const& mOverrideClassScript = GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
    for(AuraList::const_iterator i = mOverrideClassScript.begin(); i != mOverrideClassScript.end(); ++i)
    {
        switch((*i)->GetModifier()->m_miscvalue)
        {
            //Molten Fury
            case 4920: case 4919:
                if(pVictim->HasAuraState(AURA_STATE_HEALTHLESS_20_PERCENT))
                    TakenTotalMod *= (100.0f+(*i)->GetModifier()->m_amount)/100.0f; break;
        }
    }

    // .. taken pct: dummy auras
    AuraList const& mDummyAuras = pVictim->GetAurasByType(SPELL_AURA_DUMMY);
    for(AuraList::const_iterator i = mDummyAuras.begin(); i != mDummyAuras.end(); ++i)
        if( (*i)->GetSpellProto()->SpellIconID == 2109 )    // Cheat Death
            if( ((*i)->GetModifier()->m_miscvalue & GetSpellSchoolMask(spellProto)) )
                TakenTotalMod *= ((*i)->GetModifier()->m_amount+100.0f)/100.0f;
    
    // Distribute Damage over multiple effects, reduce by AoE
    CastingTime = GetCastingTimeForBonus( spellProto, damagetype, CastingTime );   

    // 50% for damage and healing spells for leech spells from damage bonus and 0% from healing
    for(int j = 0; j < 3; ++j)
    {
        if( spellProto->Effect[j] == SPELL_EFFECT_HEALTH_LEECH ||
            spellProto->Effect[j] == SPELL_EFFECT_APPLY_AURA && spellProto->EffectApplyAuraName[j] == SPELL_AURA_PERIODIC_LEECH )
        {
            CastingTime /= 2;
            break;
        }
    }

    switch(spellProto->SpellFamilyName)
    {
        case SPELLFAMILY_MAGE:
            // Ignite - do not modify, it is (8*Rank)% damage of procing Spell
            if(spellProto->Id==12654)
            {
                return pdamage;
            }
            // Ice Lance
            else if((spellProto->SpellFamilyFlags & 0x20000LL) && spellProto->SpellIconID == 186)
            {
                CastingTime /= 3;                           // applied 1/3 bonuses in case generic target
                if(pVictim->isFrozen())                     // and compensate this for frozen target.
                    TakenTotalMod *= 3.0f;
            }
            // Pyroblast - 115% of Fire Damage, DoT - 20% of Fire Damage
            else if((spellProto->SpellFamilyFlags & 0x400000LL) && spellProto->SpellIconID == 184 )
            {
                DotFactor = damagetype == DOT ? 0.2f : 1.0f;
                CastingTime = damagetype == DOT ? 3500 : 4025;
            }
            // Fireball - 100% of Fire Damage, DoT - 0% of Fire Damage
            else if((spellProto->SpellFamilyFlags & 0x1LL) && spellProto->SpellIconID == 185)
            {
                CastingTime = 3500;
                DotFactor = damagetype == DOT ? 0.0f : 1.0f;
            }
            // Molten armor
            else if (spellProto->SpellFamilyFlags & 0x0000000800000000LL)
            {
                CastingTime = 0;
            }
            // Arcane Missiles triggered spell
            else if ((spellProto->SpellFamilyFlags & 0x200000LL) && spellProto->SpellIconID == 225)
            {
                CastingTime = 1000;
            }
            // Blizzard triggered spell
            else if ((spellProto->SpellFamilyFlags & 0x80080LL) && spellProto->SpellIconID == 285)
            {
                CastingTime = 500;
            }
            break;
        case SPELLFAMILY_WARLOCK:
            // Life Tap
            if((spellProto->SpellFamilyFlags & 0x40000LL) && spellProto->SpellIconID == 208)
            {
                CastingTime = 2800;                         // 80% from +shadow damage
                DoneTotalMod = 1.0f;
                TakenTotalMod = 1.0f;
            }
            // Dark Pact
            else if((spellProto->SpellFamilyFlags & 0x80000000LL) && spellProto->SpellIconID == 154 && GetPetGUID())
            {
                CastingTime = 3360;                         // 96% from +shadow damage
                DoneTotalMod = 1.0f;
                TakenTotalMod = 1.0f;
            }
            // Soul Fire - 115% of Fire Damage
            else if((spellProto->SpellFamilyFlags & 0x8000000000LL) && spellProto->SpellIconID == 184)
            {
                CastingTime = 4025;
            }
            // Curse of Agony - 120% of Shadow Damage
            else if((spellProto->SpellFamilyFlags & 0x0000000400LL) && spellProto->SpellIconID == 544)
            {
                DotFactor = 1.2f;
            }
            // Drain Mana - 0% of Shadow Damage
            else if((spellProto->SpellFamilyFlags & 0x10LL) && spellProto->SpellIconID == 548)
            {
                CastingTime = 0;
            } 
            // Drain Soul 214.3%
            else if ((spellProto->SpellFamilyFlags & 0x4000LL) && spellProto->SpellIconID == 113 )
            {
                CastingTime = 7500;
            }
            // Hellfire
            else if ((spellProto->SpellFamilyFlags & 0x40LL) && spellProto->SpellIconID == 937)
            {
                CastingTime = damagetype == DOT ? 5000 : 500; // self damage seems to be so
            }
            break;
        case SPELLFAMILY_PALADIN:
            // Consecration - 95% of Holy Damage
            if((spellProto->SpellFamilyFlags & 0x20LL) && spellProto->SpellIconID == 51)
            {
                DotFactor = 0.95f; 
                CastingTime = 3500; 
            }
            // Seal of Righteousness - 10.2%/9.8% ( based on weapon type ) of Holy Damage, multiplied by weapon speed
            else if((spellProto->SpellFamilyFlags & 0x8000000LL) && spellProto->SpellIconID == 25)
            {
                Item *item = ((Player*)this)->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
                float wspeed = GetAttackTime(BASE_ATTACK)/1000.0f;

                if( item && item->GetProto()->InventoryType == INVTYPE_2HWEAPON)
                   CastingTime = uint32(wspeed*3500*0.102f);
                else
                   CastingTime = uint32(wspeed*3500*0.098f);
            }
            // Judgement of Righteousness - 73%
            else if ((spellProto->SpellFamilyFlags & 1024) && spellProto->SpellIconID == 25)
            {
                CastingTime = 2555; 
            }
            // Seal of Vengeance - 17% per Fully Stacked Tick - 5 Applications
            else if ((spellProto->SpellFamilyFlags & 0x80000000000LL) && spellProto->SpellIconID == 2040)
            {
                DotFactor = 0.17f;
                CastingTime = 3500;
            } 
            // Holy shield - 5% of Holy Damage
            else if ((spellProto->SpellFamilyFlags & 0x4000000000LL) && spellProto->SpellIconID == 453)
            {
                CastingTime = 175; 
            }
            // Blessing of Sanctuary - 0%
            else if ((spellProto->SpellFamilyFlags & 0x10000000LL) && spellProto->SpellIconID == 29)
            {
                CastingTime = 0; 
            }
            // Seal of Righteousness trigger - already computed for parent spell
            else if ( spellProto->SpellFamilyName==SPELLFAMILY_PALADIN && spellProto->SpellIconID==25 && spellProto->AttributesEx4 & 0x00800000LL )
            {
                return pdamage;
            }
            break;
        case  SPELLFAMILY_SHAMAN:
            // totem attack
            if (spellProto->SpellFamilyFlags & 0x000040000000LL)
            {
                if (spellProto->SpellIconID == 33)          // Fire Nova totem attack must be 21.4%(untested)
                    CastingTime = 749;                      // ignore CastingTime and use as modifier
                else if (spellProto->SpellIconID == 680)    // Searing Totem attack 8%
                    CastingTime = 280;                      // ignore CastingTime and use as modifier
                else if (spellProto->SpellIconID == 37)     // Magma totem attack must be 6.67%(untested)
                    CastingTime = 234;                      // ignore CastingTimePenalty and use as modifier
            }
            // Lightning Shield 33% per charge
            else if (spellProto->SpellFamilyFlags & 0x00000000400LL)
                CastingTime = 1155;                         // ignore CastingTimePenalty and use as modifier
            break;
        case SPELLFAMILY_PRIEST:
            // Mana Burn - 0% of Shadow Damage
            if((spellProto->SpellFamilyFlags & 0x10LL) && spellProto->SpellIconID == 212)
            {
                CastingTime = 0;
            }
            // Mind Flay - 59% of Shadow Damage
            else if((spellProto->SpellFamilyFlags & 0x800000LL) && spellProto->SpellIconID == 548)
            {
                CastingTime = 2065;
            }
            // Holy Fire - 86.71%, DoT - 16.5% 
            else if ((spellProto->SpellFamilyFlags & 0x100000LL) && spellProto->SpellIconID == 156) 
            {
                DotFactor = damagetype == DOT ? 0.165f : 1.0f;
                CastingTime = damagetype == DOT ? 3500 : 3035;
            }
            // Shadowguard - 28% per charge
            else if ((spellProto->SpellFamilyFlags & 0x2000000LL) && spellProto->SpellIconID == 19) 
            {
                CastingTime = 980;
            }
            // Touch of Weakeness - 10%
            else if ((spellProto->SpellFamilyFlags & 0x80000LL) && spellProto->SpellIconID == 1591) 
            {
                CastingTime = 350;
            }
            // Reflective Shield (back damage) - 0% (other spells fit to check not have damage effects/auras)
            else if (spellProto->SpellFamilyFlags == 0 && spellProto->SpellIconID == 566) 
            {
                CastingTime = 0;
            }
            break;
        case SPELLFAMILY_DRUID:
            // Hurricane triggered spell
            if((spellProto->SpellFamilyFlags & 0x400000LL) && spellProto->SpellIconID == 220)
            {
                CastingTime = 500;
            }
            break;
        case SPELLFAMILY_WARRIOR:
        case SPELLFAMILY_HUNTER:
        case SPELLFAMILY_ROGUE:
            CastingTime = 0;
            break;
        default:
            break;
    }

    float LvlPenalty = CalculateLevelPenalty(spellProto);

    // Spellmod SpellDamage
    float SpellModSpellDamage = 100.0f;

    if(Player* modOwner = GetSpellModOwner())
        modOwner->ApplySpellMod(spellProto->Id,SPELLMOD_SPELL_BONUS_DAMAGE,SpellModSpellDamage);

    SpellModSpellDamage /= 100.0f;

    float DoneActualBenefit = DoneAdvertisedBenefit * (CastingTime / 3500.0f) * DotFactor * SpellModSpellDamage * LvlPenalty;
    float TakenActualBenefit = TakenAdvertisedBenefit * (CastingTime / 3500.0f) * DotFactor * LvlPenalty;

    float tmpDamage = (float(pdamage)+DoneActualBenefit)*DoneTotalMod;

    // Add flat bonus from spell damage versus
    tmpDamage += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_FLAT_SPELL_DAMAGE_VERSUS, creatureTypeMask);

    // apply spellmod to Done damage
    if(Player* modOwner = GetSpellModOwner())
        modOwner->ApplySpellMod(spellProto->Id, SPELLMOD_DAMAGE, tmpDamage);

    tmpDamage = (tmpDamage+TakenActualBenefit)*TakenTotalMod;

    if( GetTypeId() == TYPEID_UNIT && !((Creature*)this)->isPet() )
        tmpDamage *= ((Creature*)this)->GetSpellDamageMod(((Creature*)this)->GetCreatureInfo()->rank);

    return tmpDamage > 0 ? uint32(tmpDamage) : 0;
}

int32 Unit::SpellBaseDamageBonus(int32 SchoolMask)
{
    int32 DoneAdvertisedBenefit = 0;

    // ..done
    AuraList const& mDamageDone = GetAurasByType(SPELL_AURA_MOD_DAMAGE_DONE);
    for(AuraList::const_iterator i = mDamageDone.begin();i != mDamageDone.end(); ++i)
        if(((*i)->GetModifier()->m_miscvalue & SchoolMask) != 0 &&
        (*i)->GetSpellProto()->EquippedItemClass == -1 &&
                                                            // -1 == any item class (not wand then)
        (*i)->GetSpellProto()->EquippedItemInventoryTypeMask == 0 )
                                                            // 0 == any inventory type (not wand then)
            DoneAdvertisedBenefit += (*i)->GetModifier()->m_amount;

    if (GetTypeId() == TYPEID_PLAYER)
    {
        // Damage bonus from stats
        AuraList const& mDamageDoneOfStatPercent = GetAurasByType(SPELL_AURA_MOD_SPELL_DAMAGE_OF_STAT_PERCENT);
        for(AuraList::const_iterator i = mDamageDoneOfStatPercent.begin();i != mDamageDoneOfStatPercent.end(); ++i)
        {
            if((*i)->GetModifier()->m_miscvalue & SchoolMask)
            {
                SpellEntry const* iSpellProto = (*i)->GetSpellProto();
                uint8 eff = (*i)->GetEffIndex();

                // stat used dependent from next effect aura SPELL_AURA_MOD_SPELL_HEALING presence and misc value (stat index)
                Stats usedStat = STAT_INTELLECT;
                if(eff < 2 && iSpellProto->EffectApplyAuraName[eff+1]==SPELL_AURA_MOD_SPELL_HEALING_OF_STAT_PERCENT)
                    usedStat = Stats(iSpellProto->EffectMiscValue[eff+1]);

                DoneAdvertisedBenefit += int32(GetStat(usedStat) * (*i)->GetModifier()->m_amount / 100.0f);
            }
        }
        // ... and attack power
        AuraList const& mDamageDonebyAP = GetAurasByType(SPELL_AURA_MOD_SPELL_DAMAGE_OF_ATTACK_POWER);
        for(AuraList::const_iterator i =mDamageDonebyAP.begin();i != mDamageDonebyAP.end(); ++i)
            if ((*i)->GetModifier()->m_miscvalue & SchoolMask)
                DoneAdvertisedBenefit += int32(GetTotalAttackPowerValue(BASE_ATTACK) * (*i)->GetModifier()->m_amount / 100.0f);

    }
    return DoneAdvertisedBenefit;
}

int32 Unit::SpellBaseDamageBonusForVictim(int32 SchoolMask, Unit *pVictim)
{
    uint32 creatureTypeMask = pVictim->GetCreatureTypeMask();

    int32 TakenAdvertisedBenefit = 0;
    // ..done (for creature type by mask) in taken
    AuraList const& mDamageDoneCreature = GetAurasByType(SPELL_AURA_MOD_DAMAGE_DONE_CREATURE);
    for(AuraList::const_iterator i = mDamageDoneCreature.begin();i != mDamageDoneCreature.end(); ++i)
        if(creatureTypeMask & uint32((*i)->GetModifier()->m_miscvalue))
            TakenAdvertisedBenefit += (*i)->GetModifier()->m_amount;

    // ..taken
    AuraList const& mDamageTaken = pVictim->GetAurasByType(SPELL_AURA_MOD_DAMAGE_TAKEN);
    for(AuraList::const_iterator i = mDamageTaken.begin();i != mDamageTaken.end(); ++i)
        if(((*i)->GetModifier()->m_miscvalue & SchoolMask) != 0)
            TakenAdvertisedBenefit += (*i)->GetModifier()->m_amount;

    return TakenAdvertisedBenefit;
}

bool Unit::isSpellCrit(Unit *pVictim, SpellEntry const *spellProto, SpellSchoolMask schoolMask, WeaponAttackType attackType)
{
    // not criting spell
    if((spellProto->AttributesEx2 & SPELL_ATTR_EX2_CANT_CRIT))
        return false;

    float crit_chance = 0.0f;
    switch(spellProto->DmgClass)
    {
        case SPELL_DAMAGE_CLASS_NONE:
            return false;
        case SPELL_DAMAGE_CLASS_MAGIC:
        {
            if (schoolMask & SPELL_SCHOOL_MASK_NORMAL)
                crit_chance = 0.0f;
            // For other schools
            else if (GetTypeId() == TYPEID_PLAYER)
                crit_chance = GetFloatValue( PLAYER_SPELL_CRIT_PERCENTAGE1 + GetFirstSchoolInMask(schoolMask));
            else
            {
                crit_chance = m_baseSpellCritChance;
                crit_chance += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL, schoolMask);
            }
            // taken
            if (pVictim)
            {
                // Modify critical chance by victim SPELL_AURA_MOD_ATTACKER_SPELL_CRIT_CHANCE
                crit_chance += pVictim->GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_ATTACKER_SPELL_CRIT_CHANCE, schoolMask);
                // Modify critical chance by victim SPELL_AURA_MOD_ATTACKER_SPELL_AND_WEAPON_CRIT_CHANCE
                crit_chance += pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_SPELL_AND_WEAPON_CRIT_CHANCE);
                // Modify by player victim PLAYER_FIELD_CRIT_TAKEN_SPELL_RATING
                if (pVictim->GetTypeId() == TYPEID_PLAYER)
                    crit_chance -= ((Player*)pVictim)->GetRatingBonusValue(PLAYER_FIELD_CRIT_TAKEN_SPELL_RATING);
                // scripted (increase crit chance ... against ... target by x%
                if(pVictim->isFrozen()) // Shatter
                {
                    AuraList const& mOverrideClassScript = GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
                    for(AuraList::const_iterator i = mOverrideClassScript.begin(); i != mOverrideClassScript.end(); ++i)
                    {
                        switch((*i)->GetModifier()->m_miscvalue)
                        {
                            case 849: crit_chance+= 10.0f; break; //Shatter Rank 1
                            case 910: crit_chance+= 20.0f; break; //Shatter Rank 2
                            case 911: crit_chance+= 30.0f; break; //Shatter Rank 3
                            case 912: crit_chance+= 40.0f; break; //Shatter Rank 4
                            case 913: crit_chance+= 50.0f; break; //Shatter Rank 5
                        }
                    }
                }
                // Victim Recilience reductin
                if (pVictim->GetTypeId() == TYPEID_PLAYER)
                    crit_chance -= ((Player*)pVictim)->GetRatingBonusValue(PLAYER_FIELD_CRIT_TAKEN_SPELL_RATING);
            }
            break;
        }
        case SPELL_DAMAGE_CLASS_MELEE:
        case SPELL_DAMAGE_CLASS_RANGED:
        {
            if (pVictim)
            {
                crit_chance = GetUnitCriticalChance(attackType, pVictim);
                crit_chance+= (int32(GetMaxSkillValueForLevel()) - int32(pVictim->GetDefenseSkillValue())) * 0.04f;
                crit_chance+= GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL, schoolMask);
            }
            break;
        }
        default:
            return false;
    }
    // percent done
    // only players use intelligence for critical chance computations
    if(Player* modOwner = GetSpellModOwner())
        modOwner->ApplySpellMod(spellProto->Id, SPELLMOD_CRITICAL_CHANCE, crit_chance);
  
    crit_chance = crit_chance > 0.0f ? crit_chance : 0.0f;
    if (roll_chance_f(crit_chance))
        return true;
    return false;
}

uint32 Unit::SpellCriticalBonus(SpellEntry const *spellProto, uint32 damage, Unit *pVictim)
{
    // Calculate critical bonus           
    int32 crit_bonus;
    switch(spellProto->DmgClass)
    {
        case SPELL_DAMAGE_CLASS_MELEE:                      // for melee based spells is 100%
        case SPELL_DAMAGE_CLASS_RANGED:
            // TODO: write here full calculation for melee/ranged spells 
            crit_bonus = damage;
            break;
        default:
            crit_bonus = damage / 2;                        // for spells is 50%
            break;
    }

    // adds additional damage to crit_bonus (from talents)
    if(Player* modOwner = GetSpellModOwner())
        modOwner->ApplySpellMod(spellProto->Id, SPELLMOD_CRIT_DAMAGE_BONUS, crit_bonus);

    if(pVictim)
    {
        uint32 creatureTypeMask = pVictim->GetCreatureTypeMask();
        crit_bonus = int32(crit_bonus * GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_CRIT_PERCENT_VERSUS, creatureTypeMask));
    }

    if(crit_bonus > 0)
        damage += crit_bonus;

    return damage;
}

uint32 Unit::SpellHealingBonus(SpellEntry const *spellProto, uint32 healamount, DamageEffectType damagetype, Unit *pVictim)
{
    // For totems get healing bonus from owner
    if( GetTypeId()==TYPEID_UNIT && ((Creature*)this)->isTotem())
        if(Unit* owner = GetOwner())
            return owner->SpellHealingBonus(spellProto, healamount, damagetype, pVictim);

    // Healing Done

    // These Spells are doing fixed amount of healing (TODO found less hack-like check)
    if(spellProto->Id == 15290 || spellProto->Id == 39373 || spellProto->Id == 33778 || spellProto->Id == 379 || spellProto->Id == 38395)
        return healamount;

    bool Channeled = IsChanneledSpell(spellProto);
    int32 AdvertisedBenefit = SpellBaseHealingBonus(GetSpellSchoolMask(spellProto));
    uint32 CastingTime = GetSpellCastTime(sCastTimesStore.LookupEntry(spellProto->CastingTimeIndex));
    if (CastingTime > 7000) CastingTime = 7000;
    if (CastingTime < 1500) CastingTime = 1500;

    // Healing Taken
    AdvertisedBenefit += SpellBaseHealingBonusForVictim(GetSpellSchoolMask(spellProto), pVictim);

    // Blessing of Light dummy effects healing taken from Holy Light and Flash of Light
    if (spellProto->SpellFamilyName == SPELLFAMILY_PALADIN && (spellProto->SpellFamilyFlags & 0x00000000C0000000LL))
    {
        AuraList const& mDummyAuras = pVictim->GetAurasByType(SPELL_AURA_DUMMY);
        for(AuraList::const_iterator i = mDummyAuras.begin();i != mDummyAuras.end(); ++i)
        {
            if((*i)->GetSpellProto()->SpellVisual == 9180)
            {
                                                            // Flash of Light
                if ((spellProto->SpellFamilyFlags & 0x0000000040000000LL) && (*i)->GetEffIndex() == 1)
                    AdvertisedBenefit += (*i)->GetModifier()->m_amount;
                                                            // Holy Light
                else if ((spellProto->SpellFamilyFlags & 0x0000000080000000LL) && (*i)->GetEffIndex() == 0)
                    AdvertisedBenefit += (*i)->GetModifier()->m_amount;
            }
        }
    }

    // Healing over Time spells
    float DotFactor = 1.0f;
    if(damagetype == DOT)
    {
        if(!Channeled) CastingTime = 3500;
        int32 DotDuration = GetSpellDuration(spellProto);
        if(DotDuration > 0)
        {
            // 200% limit
            if(DotDuration > 30000) DotDuration = 30000;
            if(!Channeled) DotFactor = DotDuration / 15000.0f;
            int x = 0;
            for(int j = 0; j < 3; j++)
            {
                if(spellProto->Effect[j] == SPELL_EFFECT_APPLY_AURA &&
                    (spellProto->EffectApplyAuraName[j] == SPELL_AURA_PERIODIC_HEAL ||
                    spellProto->EffectApplyAuraName[j] == SPELL_AURA_PERIODIC_LEECH) )
                {
                    x = j;
                    break;
                }
            }
            int DotTicks = 6;
            if(spellProto->EffectAmplitude[x] != 0)
                DotTicks = DotDuration / spellProto->EffectAmplitude[x];
            if(DotTicks)
                AdvertisedBenefit /= DotTicks;
        }
    }

    // distribute healing to all effects, reduce AoE damage
    CastingTime = GetCastingTimeForBonus( spellProto, damagetype, CastingTime );   

    // 0% bonus for damage and healing spells for leech spells from healing bonus
    for(int j = 0; j < 3; ++j)
    {
        if( spellProto->Effect[j] == SPELL_EFFECT_HEALTH_LEECH ||
            spellProto->Effect[j] == SPELL_EFFECT_APPLY_AURA && spellProto->EffectApplyAuraName[j] == SPELL_AURA_PERIODIC_LEECH )
        {
            CastingTime = 0;
            break;
        }
    }

    // Exception
    switch (spellProto->SpellFamilyName)
    {
        case  SPELLFAMILY_SHAMAN:
                                                            // Healing stream from totem (add 6% per tick from hill bonus owner)
            if (spellProto->SpellFamilyFlags & 0x000000002000LL)
                CastingTime = 210;                          //
            // Earth Shield 30% per charge
            else if (spellProto->SpellFamilyFlags & 0x40000000000LL)
                CastingTime = 1050;
            break;
        case  SPELLFAMILY_DRUID:
            // Lifebloom final heal
            if (spellProto->SpellFamilyFlags & 0x1000000000LL && damagetype != DOT)
                CastingTime = 1500;
            // Tranquilitiy triggered spell
            else if (spellProto->SpellFamilyFlags & 0x80LL)
                CastingTime = 500;
            break;
        case SPELLFAMILY_PALADIN:
            // Seal and Judgement of Light
            if ( spellProto->SpellFamilyFlags & 0x100040000LL )
                CastingTime = 0;
            break;
        case SPELLFAMILY_WARRIOR:
        case SPELLFAMILY_HUNTER:
        case SPELLFAMILY_ROGUE:
            CastingTime = 0;
            break;
    }

    float LvlPenalty = CalculateLevelPenalty(spellProto);

    // Spellmod SpellDamage
    float SpellModSpellDamage = 100.0f;

    if(Player* modOwner = GetSpellModOwner())
        modOwner->ApplySpellMod(spellProto->Id,SPELLMOD_SPELL_BONUS_DAMAGE,SpellModSpellDamage);

    SpellModSpellDamage /= 100.0f;

    float ActualBenefit = (float)AdvertisedBenefit * ((float)CastingTime / 3500.0f) * DotFactor * SpellModSpellDamage * LvlPenalty;

    // use float as more appropriate for negative values and percent applying
    float heal = healamount + ActualBenefit;

    // TODO: check for ALL/SPELLS type
    // Healing done percent
    AuraList const& mHealingDonePct = GetAurasByType(SPELL_AURA_MOD_HEALING_DONE_PERCENT);
    for(AuraList::const_iterator i = mHealingDonePct.begin();i != mHealingDonePct.end(); ++i)
        heal *= (100.0f + (*i)->GetModifier()->m_amount) / 100.0f;

    // apply spellmod to Done amount
    if(Player* modOwner = GetSpellModOwner())
        modOwner->ApplySpellMod(spellProto->Id, SPELLMOD_DAMAGE, heal);

    // Healing Wave cast
    if (spellProto->SpellFamilyName == SPELLFAMILY_SHAMAN && spellProto->SpellFamilyFlags & 0x0000000000000040LL)
    {
        // Search for Healing Way on Victim (stack up to 3 time)
        int32 pctMod = 0;
        Unit::AuraList const& auraDummy = pVictim->GetAurasByType(SPELL_AURA_DUMMY);
        for(Unit::AuraList::const_iterator itr = auraDummy.begin(); itr!=auraDummy.end(); ++itr)
            if((*itr)->GetId() == 29203)
                pctMod += (*itr)->GetModifier()->m_amount;
        // Apply bonus
        if (pctMod)
            heal = heal * (100 + pctMod) / 100;
    }

    // Healing taken percent
    float minval = pVictim->GetMaxNegativeAuraModifier(SPELL_AURA_MOD_HEALING_PCT);
    if(minval)
        heal *= (100.0f + minval) / 100.0f;

    float maxval = pVictim->GetMaxPositiveAuraModifier(SPELL_AURA_MOD_HEALING_PCT);
    if(maxval)
        heal *= (100.0f + maxval) / 100.0f;

    if (heal < 0) heal = 0;

    return uint32(heal);
}

int32 Unit::SpellBaseHealingBonus(int32 SchoolMask)
{
    int32 AdvertisedBenefit = 0;

    AuraList const& mHealingDone = GetAurasByType(SPELL_AURA_MOD_HEALING_DONE);
    for(AuraList::const_iterator i = mHealingDone.begin();i != mHealingDone.end(); ++i)
        if(((*i)->GetModifier()->m_miscvalue & SchoolMask) != 0)
            AdvertisedBenefit += (*i)->GetModifier()->m_amount;

    // Healing bonus of spirit, intellect and strength
    if (GetTypeId() == TYPEID_PLAYER)
    {
        // Healing bonus from stats
        AuraList const& mHealingDoneOfStatPercent = GetAurasByType(SPELL_AURA_MOD_SPELL_HEALING_OF_STAT_PERCENT);
        for(AuraList::const_iterator i = mHealingDoneOfStatPercent.begin();i != mHealingDoneOfStatPercent.end(); ++i)
        {
            // stat used dependent from misc value (stat index)
            Stats usedStat = Stats((*i)->GetSpellProto()->EffectMiscValue[(*i)->GetEffIndex()]);
            AdvertisedBenefit += int32(GetStat(usedStat) * (*i)->GetModifier()->m_amount / 100.0f);
        }

        // ... and attack power
        AuraList const& mHealingDonebyAP = GetAurasByType(SPELL_AURA_MOD_SPELL_HEALING_OF_ATTACK_POWER);
        for(AuraList::const_iterator i = mHealingDonebyAP.begin();i != mHealingDonebyAP.end(); ++i)
            if ((*i)->GetModifier()->m_miscvalue & SchoolMask)
                AdvertisedBenefit += int32(GetTotalAttackPowerValue(BASE_ATTACK) * (*i)->GetModifier()->m_amount / 100.0f);
    }
    return AdvertisedBenefit;
}

int32 Unit::SpellBaseHealingBonusForVictim(int32 SchoolMask, Unit *pVictim)
{
    int32 AdvertisedBenefit = 0;
    AuraList const& mDamageTaken = pVictim->GetAurasByType(SPELL_AURA_MOD_HEALING);
    for(AuraList::const_iterator i = mDamageTaken.begin();i != mDamageTaken.end(); ++i)
        if(((*i)->GetModifier()->m_miscvalue & SchoolMask) != 0)
            AdvertisedBenefit += (*i)->GetModifier()->m_amount;
    return AdvertisedBenefit;
}

bool Unit::IsImmunedToPhysicalDamage() const
{
    //If m_immuneToDamage type contain magic, IMMUNE damage.
    SpellImmuneList const& damageImmList = m_spellImmune[IMMUNITY_DAMAGE];
    for (SpellImmuneList::const_iterator itr = damageImmList.begin(); itr != damageImmList.end(); ++itr)
        if(itr->type & IMMUNE_DAMAGE_PHYSICAL)
            return true;

    //If m_immuneToSchool type contain this school type, IMMUNE damage.
    SpellImmuneList const& spellImmList = m_spellImmune[IMMUNITY_SCHOOL];
    for (SpellImmuneList::const_iterator itr = spellImmList.begin(); itr != spellImmList.end(); ++itr)
        if(itr->type & IMMUNE_SCHOOL_PHYSICAL)
            return true;

    return false;
}

bool Unit::IsImmunedToSpellDamage(SpellEntry const* spellInfo, bool useCharges)
{
    // no charges dependent checks

    //If m_immuneToSchool type contain this school type, IMMUNE damage.
    SpellImmuneList const& schoolList = m_spellImmune[IMMUNITY_SCHOOL];
    for (SpellImmuneList::const_iterator itr = schoolList.begin(); itr != schoolList.end(); ++itr)
        if(itr->type & GetSpellSchoolMask(spellInfo))
            return true;

    // charges dependent checks

    //If m_immuneToDamage type contain magic, IMMUNE damage.
    SpellImmuneList const& damageList = m_spellImmune[IMMUNITY_DAMAGE];
    for (SpellImmuneList::const_iterator itr = damageList.begin(); itr != damageList.end(); ++itr)
    {
        if(itr->type & GetSpellSchoolMask(spellInfo))
        {
            if(useCharges)
            {
                AuraList const& auraDamageImmunity = GetAurasByType(SPELL_AURA_DAMAGE_IMMUNITY);
                for(AuraList::const_iterator auraItr = auraDamageImmunity.begin(); auraItr != auraDamageImmunity.end(); ++auraItr)
                {
                    if((*auraItr)->GetId()==itr->spellId)
                    {
                        if((*auraItr)->m_procCharges > 0)
                        {
                            --(*auraItr)->m_procCharges;
                            if((*auraItr)->m_procCharges==0)
                                RemoveAurasDueToSpell(itr->spellId);
                        }
                        break;
                    }
                }
            }
            return true;
        }
    }

    return false;
}

bool Unit::IsImmunedToSpell(SpellEntry const* spellInfo, bool useCharges)
{
    if (!spellInfo)
        return false;

    // no charges first

    // don't get feared if stunned
    if (spellInfo->Mechanic == MECHANIC_FEAR )
    {
        if ( hasUnitState(UNIT_STAT_STUNDED) )
            return true;
    }

    // Handle dummy aura of Hypothermia to make immune to Ice Block
    if (spellInfo->SpellFamilyName == SPELLFAMILY_MAGE && spellInfo->SpellFamilyFlags & 0x8000000000LL &&
        HasAura(SPELLID_MAGE_HYPOTHERMIA,0))
        return true;

    // not have spells with charges currently
    SpellImmuneList const& dispelList = m_spellImmune[IMMUNITY_DISPEL];
    for(SpellImmuneList::const_iterator itr = dispelList.begin(); itr != dispelList.end(); ++itr)
        if(itr->type == spellInfo->Dispel)
            return true;

    if( !(spellInfo->AttributesEx & SPELL_ATTR_EX_UNAFFECTED_BY_SCHOOL_IMMUNE))               // unaffected by school immunity
    {
        // not have spells with charges currently
        SpellImmuneList const& schoolList = m_spellImmune[IMMUNITY_SCHOOL];
        for(SpellImmuneList::const_iterator itr = schoolList.begin(); itr != schoolList.end(); ++itr)
            if( !(IsPositiveSpell(itr->spellId) && IsPositiveSpell(spellInfo->Id)) &&
                (itr->type & GetSpellSchoolMask(spellInfo)) )
                return true;
    }

    // charges dependent checks

    SpellImmuneList const& mechanicList = m_spellImmune[IMMUNITY_MECHANIC];
    for(SpellImmuneList::const_iterator itr = mechanicList.begin(); itr != mechanicList.end(); ++itr)
    {
        // Spell taht must be affected by MECHANIC_INVULNERABILITY not have this mechanic, check its by another data
        if(itr->type == spellInfo->Mechanic || itr->type == MECHANIC_INVULNERABILITY && IsMechanicInvulnerabilityImmunityToSpell(spellInfo))
        {
            if(useCharges)
            {
                AuraList const& auraMechImmunity = GetAurasByType(SPELL_AURA_MECHANIC_IMMUNITY);
                for(AuraList::const_iterator auraItr = auraMechImmunity.begin(); auraItr != auraMechImmunity.end(); ++auraItr)
                {
                    if((*auraItr)->GetId()==itr->spellId)
                    {
                        if((*auraItr)->m_procCharges > 0)
                        {
                            --(*auraItr)->m_procCharges;
                            if((*auraItr)->m_procCharges==0)
                                RemoveAurasDueToSpell(itr->spellId);
                        }
                        break;
                    }
                }
            }
            return true;
        }
    }

    return false;
}

bool Unit::IsImmunedToSpellEffect(uint32 effect, uint32 mechanic) const
{
    //If m_immuneToEffect type contain this effect type, IMMUNE effect.
    SpellImmuneList const& effectList = m_spellImmune[IMMUNITY_EFFECT];
    for (SpellImmuneList::const_iterator itr = effectList.begin(); itr != effectList.end(); ++itr)
        if(itr->type == effect)
            return true;

    SpellImmuneList const& mechanicList = m_spellImmune[IMMUNITY_MECHANIC];
    for (SpellImmuneList::const_iterator itr = mechanicList.begin(); itr != mechanicList.end(); ++itr)
        if(itr->type == mechanic)
            return true;

    return false;
}

bool Unit::IsDamageToThreatSpell(SpellEntry const * spellInfo) const
{
    if(!spellInfo)
        return false;

    uint32 family = spellInfo->SpellFamilyName;
    uint64 flags = spellInfo->SpellFamilyFlags;

    if((family == 5 && flags == 256) ||                     //Searing Pain
        (family == 6 && flags == 8192) ||                   //Mind Blast
        (family == 11 && flags == 1048576))                 //Earth Shock
        return true;

    return false;
}

void Unit::MeleeDamageBonus(Unit *pVictim, uint32 *pdamage,WeaponAttackType attType, SpellEntry const *spellProto)
{
    if(!pVictim) return;

    if(*pdamage == 0)
        return;

    uint32 creatureTypeMask = pVictim->GetCreatureTypeMask();

    // Taken/Done fixed damage bonus auras
    int32 DoneFlatBenefit = 0;
    int32 TakenFlatBenefit = 0;

    // ..done (for creature type by mask) in taken
    AuraList const& mDamageDoneCreature = this->GetAurasByType(SPELL_AURA_MOD_DAMAGE_DONE_CREATURE);
    for(AuraList::const_iterator i = mDamageDoneCreature.begin();i != mDamageDoneCreature.end(); ++i)
        if(creatureTypeMask & uint32((*i)->GetModifier()->m_miscvalue))
            DoneFlatBenefit += (*i)->GetModifier()->m_amount;

    // ..done
    // SPELL_AURA_MOD_DAMAGE_DONE included in weapon damage

    // ..done (base at attack power for marked target and base at attack power for creature type)
    int32 APbonus = 0;
    if(attType == RANGED_ATTACK)
    {
        APbonus += pVictim->GetTotalAuraModifier(SPELL_AURA_RANGED_ATTACK_POWER_ATTACKER_BONUS);

        // ..done (base at attack power and creature type)
        AuraList const& mCreatureAttackPower = GetAurasByType(SPELL_AURA_MOD_RANGED_ATTACK_POWER_VERSUS);
        for(AuraList::const_iterator i = mCreatureAttackPower.begin();i != mCreatureAttackPower.end(); ++i)
            if(creatureTypeMask & uint32((*i)->GetModifier()->m_miscvalue))
                APbonus += (*i)->GetModifier()->m_amount;
    }
    else
    {
        APbonus += pVictim->GetTotalAuraModifier(SPELL_AURA_MELEE_ATTACK_POWER_ATTACKER_BONUS);

        // ..done (base at attack power and creature type)
        AuraList const& mCreatureAttackPower = GetAurasByType(SPELL_AURA_MOD_MELEE_ATTACK_POWER_VERSUS);
        for(AuraList::const_iterator i = mCreatureAttackPower.begin();i != mCreatureAttackPower.end(); ++i)
            if(creatureTypeMask & uint32((*i)->GetModifier()->m_miscvalue))
                APbonus += (*i)->GetModifier()->m_amount;
    }

    if (APbonus!=0)                                         // Can be negative
    {
        bool normalized = false;
        if(spellProto)
        {
            for (uint8 i = 0; i<3;i++)
            {
                if (spellProto->Effect[i] == SPELL_EFFECT_NORMALIZED_WEAPON_DMG)
                {
                    normalized = true;
                    break;
                }
            }
        }

        DoneFlatBenefit += int32(APbonus/14.0f * GetAPMultiplier(attType,normalized));
    }

    // ..taken
    AuraList const& mDamageTaken = pVictim->GetAurasByType(SPELL_AURA_MOD_DAMAGE_TAKEN);
    for(AuraList::const_iterator i = mDamageTaken.begin();i != mDamageTaken.end(); ++i)
        if((*i)->GetModifier()->m_miscvalue & IMMUNE_SCHOOL_PHYSICAL)
            TakenFlatBenefit += (*i)->GetModifier()->m_amount;

    if(attType!=RANGED_ATTACK)
        TakenFlatBenefit += pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_MELEE_DAMAGE_TAKEN);
    else
        TakenFlatBenefit += pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_RANGED_DAMAGE_TAKEN);

    // Done/Taken total percent damage auras
    float DoneTotalMod = 1;
    float TakenTotalMod = 1;

    // ..done
    // SPELL_AURA_MOD_DAMAGE_PERCENT_DONE included in weapon damage
    // SPELL_AURA_MOD_OFFHAND_DAMAGE_PCT  included in weapon damage

    AuraList const& mDamageDoneVersus = this->GetAurasByType(SPELL_AURA_MOD_DAMAGE_DONE_VERSUS);
    for(AuraList::const_iterator i = mDamageDoneVersus.begin();i != mDamageDoneVersus.end(); ++i)
        if(creatureTypeMask & uint32((*i)->GetModifier()->m_miscvalue))
            DoneTotalMod *= ((*i)->GetModifier()->m_amount+100.0f)/100.0f;

    // ..taken
    AuraList const& mModDamagePercentTaken = pVictim->GetAurasByType(SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN);
    for(AuraList::const_iterator i = mModDamagePercentTaken.begin(); i != mModDamagePercentTaken.end(); ++i)
        if((*i)->GetModifier()->m_miscvalue & IMMUNE_SCHOOL_PHYSICAL)
            TakenTotalMod *= ((*i)->GetModifier()->m_amount+100.0f)/100.0f;

    // .. taken pct: dummy auras
    AuraList const& mDummyAuras = pVictim->GetAurasByType(SPELL_AURA_DUMMY);
    for(AuraList::const_iterator i = mDummyAuras.begin(); i != mDummyAuras.end(); ++i)
        if((*i)->GetSpellProto()->SpellIconID == 2109)      // Cheat Death
            if((*i)->GetModifier()->m_miscvalue & IMMUNE_SCHOOL_PHYSICAL)
                TakenTotalMod *= ((*i)->GetModifier()->m_amount+100.0f)/100.0f;

    if(attType != RANGED_ATTACK)
    {
        AuraList const& mModMeleeDamageTakenPercent = pVictim->GetAurasByType(SPELL_AURA_MOD_MELEE_DAMAGE_TAKEN_PCT);
        for(AuraList::const_iterator i = mModMeleeDamageTakenPercent.begin(); i != mModMeleeDamageTakenPercent.end(); ++i)
            TakenTotalMod *= ((*i)->GetModifier()->m_amount+100.0f)/100.0f;
    }
    else
    {
        AuraList const& mModRangedDamageTakenPercent = pVictim->GetAurasByType(SPELL_AURA_MOD_RANGED_DAMAGE_TAKEN_PCT);
        for(AuraList::const_iterator i = mModRangedDamageTakenPercent.begin(); i != mModRangedDamageTakenPercent.end(); ++i)
            TakenTotalMod *= ((*i)->GetModifier()->m_amount+100.0f)/100.0f;
    }

    float tmpDamage = float(int32(*pdamage) + DoneFlatBenefit) * DoneTotalMod;

    // apply spellmod to Done damage
    if(spellProto)
    {
        if(Player* modOwner = GetSpellModOwner())
            modOwner->ApplySpellMod(spellProto->Id, SPELLMOD_DAMAGE, tmpDamage);
    }

    tmpDamage = (tmpDamage + TakenFlatBenefit)*TakenTotalMod;

    // bonus result can be negative
    *pdamage =  tmpDamage > 0 ? uint32(tmpDamage) : 0;
}

void Unit::ApplySpellImmune(uint32 spellId, uint32 op, uint32 type, bool apply)
{
    if (apply)
    {
        for (SpellImmuneList::iterator itr = m_spellImmune[op].begin(), next; itr != m_spellImmune[op].end(); itr = next)
        {
            next = itr; ++next;
            if(itr->type == type)
            {
                m_spellImmune[op].erase(itr);
                next = m_spellImmune[op].begin();
            }
        }
        SpellImmune Immune;
        Immune.spellId = spellId;
        Immune.type = type;
        m_spellImmune[op].push_back(Immune);
    }
    else
    {
        for (SpellImmuneList::iterator itr = m_spellImmune[op].begin(); itr != m_spellImmune[op].end(); ++itr)
        {
            if(itr->spellId == spellId)
            {
                m_spellImmune[op].erase(itr);
                break;
            }
        }
    }

}

float Unit::GetWeaponProcChance() const
{
    // normalized proc chance for weapon attack speed
    // (odd formulae...)
    if(isAttackReady(BASE_ATTACK))
        return (GetAttackTime(BASE_ATTACK) * 1.8f / 1000.0f);
    else if (haveOffhandWeapon() && isAttackReady(OFF_ATTACK))
        return (GetAttackTime(OFF_ATTACK) * 1.6f / 1000.0f);
    return 0;
}

float Unit::GetPPMProcChance(uint32 WeaponSpeed, float PPM) const
{
    // proc per minute chance calculation
    if (PPM <= 0) return 0.0f;
    uint32 result = uint32((WeaponSpeed * PPM) / 600.0f);   // result is chance in percents (probability = Speed_in_sec * (PPM / 60))
    return result;
}

void Unit::Mount(uint32 mount)
{
    if(!mount)
        return;

    RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_MOUNTING);

    SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, mount);

    SetFlag( UNIT_FIELD_FLAGS, UNIT_FLAG_MOUNT );

    // unsummon pet
    if(GetTypeId() == TYPEID_PLAYER)
    {
        Pet* pet = GetPet();
        if(pet)
        {
            if(pet->isControlled())
            {
                ((Player*)this)->SetOldPetNumber(pet->GetCharmInfo()->GetPetNumber());
                ((Player*)this)->SetOldPetSpell(pet->GetUInt32Value(UNIT_CREATED_BY_SPELL));
            }

            ((Player*)this)->RemovePet(NULL,PET_SAVE_NOT_IN_SLOT);
        }
        else
            ((Player*)this)->SetOldPetNumber(0);
    }
}

void Unit::Unmount()
{
    if(!IsMounted())
        return;

    RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_NOT_MOUNTED);

    SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, 0);
    RemoveFlag( UNIT_FIELD_FLAGS, UNIT_FLAG_MOUNT );

    if(GetTypeId() == TYPEID_PLAYER && ((Player*)this)->GetOldPetNumber() && isAlive())
    {
        Pet* NewPet = new Pet(this);
        if(!NewPet->LoadPetFromDB(this, 0, ((Player*)this)->GetOldPetNumber(), true))
            delete NewPet;

        ((Player*)this)->SetOldPetNumber(0);
    }
}

void Unit::SetInCombat()
{
    m_CombatTimer = 5000;
    SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IN_COMBAT);

    if(isCharmed() || (GetTypeId()!=TYPEID_PLAYER && ((Creature*)this)->isPet()))
        SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PET_IN_COMBAT);
}

void Unit::ClearInCombat()
{
    m_CombatTimer = 0;
    RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IN_COMBAT);

    if(isCharmed() || (GetTypeId()!=TYPEID_PLAYER && ((Creature*)this)->isPet()))
        RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PET_IN_COMBAT);
}

bool Unit::isTargetableForAttack() const
{
    if (GetTypeId()==TYPEID_PLAYER && ((Player *)this)->isGameMaster())
        return false;

    if(HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE))
        return false;

    return isAlive() && !hasUnitState(UNIT_STAT_DIED)&& !isInFlight() /*&& !isStealth()*/;
}

int32 Unit::ModifyHealth(int32 dVal)
{
    int32 gain = 0;

    if(dVal==0)
        return 0;

    int32 curHealth = (int32)GetHealth();

    int32 val = dVal + curHealth;
    if(val <= 0)
    {
        SetHealth(0);
        return -curHealth;
    }

    int32 maxHealth = (int32)GetMaxHealth();

    if(val < maxHealth)
    {
        SetHealth(val);
        gain = val - curHealth;
    }
    else if(curHealth != maxHealth)
    {
        SetHealth(maxHealth);
        gain = maxHealth - curHealth;
    }

    return gain;
}

int32 Unit::ModifyPower(Powers power, int32 dVal)
{
    int32 gain = 0;

    if(dVal==0)
        return 0;

    int32 curPower = (int32)GetPower(power);

    int32 val = dVal + curPower;
    if(val <= 0)
    {
        SetPower(power,0);
        return -curPower;
    }

    int32 maxPower = (int32)GetMaxPower(power);

    if(val < maxPower)
    {
        SetPower(power,val);
        gain = val - curPower;
    }
    else if(curPower != maxPower)
    {
        SetPower(power,maxPower);
        gain = maxPower - curPower;
    }

    return gain;
}

bool Unit::isVisibleForOrDetect(Unit const* u, bool detect, bool inVisibleList) const
{
    if(!u)
        return false;

    // Always can see self
    if (u==this)
        return true;

    // player visible for other player if not logout and at same transport
    // including case when player is out of world
    bool at_same_transport =
        GetTypeId() == TYPEID_PLAYER &&  u->GetTypeId()==TYPEID_PLAYER &&
        !((Player*)this)->GetSession()->PlayerLogout() && !((Player*)u)->GetSession()->PlayerLogout() &&
        !((Player*)this)->GetSession()->PlayerLoading() && !((Player*)u)->GetSession()->PlayerLoading() &&
        ((Player*)this)->GetTransport() && ((Player*)this)->GetTransport() == ((Player*)u)->GetTransport();

    // not in world
    if(!at_same_transport && (!IsInWorld() || !u->IsInWorld()))
        return false;

    // always seen by owner
    if(GetCharmerOrOwnerGUID()==u->GetGUID())
        return true;

    // Grid dead/alive checks
    if( u->GetTypeId()==TYPEID_PLAYER)
    {
        // non visible at grid for any stealth state
        if(!IsVisibleInGridForPlayer((Player *)u))
            return false;

        // if player is dead then he can't detect anyone in anycases
        if(!u->isAlive())
            detect = false;
    }
    else
    {
        // all dead creatures/players not visible for any creatures
        if(!u->isAlive() || !isAlive())
            return false;
    }

    // different visible distance checks
    if(u->isInFlight())                                     // what see player in flight
    {
        // use object grey distance for all (only see objects any way)
        if (!IsWithinDistInMap(u,World::GetMaxVisibleDistanceInFlight()+(inVisibleList ? World::GetVisibleObjectGreyDistance() : 0.0f)))
            return false;
    }
    else if(!isAlive())                                     // distance for show body
    {
        if (!IsWithinDistInMap(u,World::GetMaxVisibleDistanceForObject()+(inVisibleList ? World::GetVisibleObjectGreyDistance() : 0.0f)))
            return false;
    }
    else if(GetTypeId()==TYPEID_PLAYER)                     // distance for show player
    {
        if(u->GetTypeId()==TYPEID_PLAYER)
        {
            // Players far than max visible distance for player or not in our map are not visible too
            if (!at_same_transport && !IsWithinDistInMap(u,World::GetMaxVisibleDistanceForPlayer()+(inVisibleList ? World::GetVisibleUnitGreyDistance() : 0.0f)))
                return false;
        }
        else
        {
            // Units far than max visible distance for creature or not in our map are not visible too
            if (!IsWithinDistInMap(u,World::GetMaxVisibleDistanceForCreature()+(inVisibleList ? World::GetVisibleUnitGreyDistance() : 0.0f)))
                return false;
        }
    }
    else if(GetCharmerOrOwnerGUID())                        // distance for show pet/charmed
    {
        // Pet/charmed far than max visible distance for player or not in our map are not visible too
        if (!IsWithinDistInMap(u,World::GetMaxVisibleDistanceForPlayer()+(inVisibleList ? World::GetVisibleUnitGreyDistance() : 0.0f)))
            return false;
    }
    else                                                    // distance for show creature
    {
        // Units far than max visible distance for creature or not in our map are not visible too
        if (!IsWithinDistInMap(u,World::GetMaxVisibleDistanceForCreature()+(inVisibleList ? World::GetVisibleUnitGreyDistance() : 0.0f)))
            return false;
    }

    // Visible units, always are visible for all units, except for units under invisibility
    if (m_Visibility == VISIBILITY_ON && u->GetVisibility()!= VISIBILITY_GROUP_INVISIBILITY)
        return true;

    // GMs see any players, not higher GMs and all units
    if (u->GetTypeId() == TYPEID_PLAYER && ((Player *)u)->isGameMaster())
    {
        if(GetTypeId() == TYPEID_PLAYER)
            return ((Player *)this)->GetSession()->GetSecurity() <= ((Player *)u)->GetSession()->GetSecurity();
        else
            return true;
    }

    // non faction visibility non-breakable for non-GMs
    if (m_Visibility == VISIBILITY_OFF)
        return false;

    // Invisible units, always are visible for units under same invisibility type
    if(m_invisibilityMask & u->m_invisibilityMask)
        return true;

    // Invisible units, always are visible for unit that can detect this invisibility (have appropriate level for detect)
    if(u->canDetectInvisibilityOf(this))
        return true;

    // Units that can detect invisibility always are visible for units that can be detected
    if(canDetectInvisibilityOf(u))
        return true;

    // Stealth/invisible not hostile units, not visible (except Player-with-Player case)
    if (!u->IsHostileTo(this))
    {
        // player see other player with stealth/invisibility only if he in same group or raid or same team (raid/team case dependent from conf setting)
        if(GetTypeId()==TYPEID_PLAYER && u->GetTypeId()==TYPEID_PLAYER)
        {
            if(((Player*)this)->IsGroupVisibleFor(((Player*)u)))
                return true;

            // else apply same rules as for hostile case (detecting check)
        }
    }
    else
    {
        // Hunter mark functionality
        AuraList const& auras = GetAurasByType(SPELL_AURA_MOD_STALKED);
        for(AuraList::const_iterator iter = auras.begin(); iter != auras.end(); ++iter)
            if((*iter)->GetCasterGUID()==u->GetGUID())
                return true;
    }

    // unit got in stealth in this moment and must ignore old detected state
    // invisibility not have chance for detection
    if (m_Visibility == VISIBILITY_ON || m_Visibility == VISIBILITY_GROUP_NO_DETECT || m_Visibility == VISIBILITY_GROUP_INVISIBILITY)
        return false;

    // NOW ONLY STEALTH CASE

    // stealth and detected and visible for some seconds
    if (u->GetTypeId() == TYPEID_PLAYER  && ((Player*)u)->m_DetectInvTimer > 300 && ((Player*)u)->HaveAtClient(this))
        return true;

    //if in non-detect mode then invisible for unit
    if (!detect)
        return false;

    // Special cases

    // If is attacked then stealth is lost, some creature can use stealth too
    if(this->isAttacked())
        return true;

    // If there is collision rogue is seen regardless of level difference
    // TODO: check sizes in DB
    float distance = GetDistance(u);
    if (distance < 0.24f)
        return true;

    //If a mob or player is stunned he will not be able to detect stealth
    if (u->hasUnitState(UNIT_STAT_STUNDED) && (u != this))
        return false;

    // Creature can detect target only in aggro radius
    if(u->GetTypeId() != TYPEID_PLAYER)
    {
        //Always invisible from back and out of aggro range
        bool isInFront = u->isInFront(this,((Creature const*)u)->GetAttackDistance(this));
        if(!isInFront)
            return false;
    }
    else
    {
        //Always invisible from back
        bool isInFront = u->isInFront(this,(GetTypeId()==TYPEID_PLAYER || GetCharmerOrOwnerGUID()) ? World::GetMaxVisibleDistanceForPlayer() : World::GetMaxVisibleDistanceForCreature());
        if(!isInFront)
            return false;
    }

    //Calculation if target is in front

    //Visible distance based on stealth value (stealth rank 4 300MOD, 10.5 - 3 = 7.5)
    float visibleDistance = 10.5f - (GetTotalAuraModifier(SPELL_AURA_MOD_STEALTH)/100.0f);

    //Visible distance is modified by
    //-Level Diff (every level diff = 1.0f in visible distance)
    visibleDistance += int32(u->getLevel()) - int32(this->getLevel());

    //This allows to check talent tree and will add addition stealth dependent on used points)
    int32 stealthMod = GetTotalAuraModifier(SPELL_AURA_MOD_STEALTH_LEVEL);
    if(stealthMod < 0)
        stealthMod = 0;

    //-Stealth Mod(positive like Master of Deception) and Stealth Detection(negative like paranoia)
    //based on wowwiki every 5 mod we have 1 more level diff in calculation
    visibleDistance += (int32(u->GetTotalAuraModifier(SPELL_AURA_MOD_DETECT)) - stealthMod)/5.0f;

    if(distance > visibleDistance)
        return false;

    // Now check is target visible with LoS
    float ox,oy,oz;
    u->GetPosition(ox,oy,oz);
    return IsWithinLOS(ox,oy,oz);
}

void Unit::SetVisibility(UnitVisibility x)
{
    m_Visibility = x;

    if(IsInWorld())
    {
        Map *m = MapManager::Instance().GetMap(GetMapId(), this);

        if(GetTypeId()==TYPEID_PLAYER)
            m->PlayerRelocation((Player*)this,GetPositionX(),GetPositionY(),GetPositionZ(),GetOrientation());
        else
            m->CreatureRelocation((Creature*)this,GetPositionX(),GetPositionY(),GetPositionZ(),GetOrientation());
    }
}

bool Unit::canDetectInvisibilityOf(Unit const* u) const
{
    if(uint32 mask = (m_detectInvisibilityMask & u->m_invisibilityMask))
    {
        for(uint32 i = 0; i < 10; ++i)
        {
            if(((1 << i) & mask)==0)
                continue;

            // find invisibility level
            uint32 invLevel = 0;
            Unit::AuraList const& iAuras = u->GetAurasByType(SPELL_AURA_MOD_INVISIBILITY);
            for(Unit::AuraList::const_iterator itr = iAuras.begin(); itr != iAuras.end(); ++itr)
                if(((*itr)->GetModifier()->m_miscvalue)==i && invLevel < (*itr)->GetModifier()->m_amount)
                    invLevel = (*itr)->GetModifier()->m_amount;

            // find invisibility detect level
            uint32 detectLevel = 0;
            Unit::AuraList const& dAuras = GetAurasByType(SPELL_AURA_MOD_INVISIBILITY_DETECTION);
            for(Unit::AuraList::const_iterator itr = dAuras.begin(); itr != dAuras.end(); ++itr)
                if(((*itr)->GetModifier()->m_miscvalue)==i && detectLevel < (*itr)->GetModifier()->m_amount)
                    detectLevel = (*itr)->GetModifier()->m_amount;

            if(i==6 && GetTypeId()==TYPEID_PLAYER)          // special drunk detection case
            {
                detectLevel = ((Player*)this)->GetDrunkValue();
            }

            if(invLevel <= detectLevel)
                return true;
        }
    }

    return false;
}

void Unit::UpdateSpeed(UnitMoveType mtype, bool forced)
{
    int32 main_speed_mod  = 0;
    float stack_bonus     = 1.0f;
    float non_stack_bonus = 1.0f;

    switch(mtype)
    {
        case MOVE_WALK:
            return;
        case MOVE_RUN:
        {
            if (IsMounted()) // Use on mount auras
            {
                main_speed_mod  = GetMaxPositiveAuraModifier(SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED);
                stack_bonus     = GetTotalAuraMultiplier(SPELL_AURA_MOD_MOUNTED_SPEED_ALWAYS);
                non_stack_bonus = (100.0f + GetMaxPositiveAuraModifier(SPELL_AURA_MOD_MOUNTED_SPEED_NOT_STACK))/100.0f;
            }
            else
            {
                main_speed_mod  = GetMaxPositiveAuraModifier(SPELL_AURA_MOD_INCREASE_SPEED);
                stack_bonus     = GetTotalAuraMultiplier(SPELL_AURA_MOD_SPEED_ALWAYS);
                non_stack_bonus = (100.0f + GetMaxPositiveAuraModifier(SPELL_AURA_MOD_SPEED_NOT_STACK))/100.0f;
            }
            break;
        }
        case MOVE_WALKBACK:
            return;
        case MOVE_SWIM:
        {
            main_speed_mod  = GetMaxPositiveAuraModifier(SPELL_AURA_MOD_INCREASE_SWIM_SPEED);
            break;
        }
        case MOVE_SWIMBACK:
            return;
        case MOVE_FLY:
        {
            if (IsMounted()) // Use on mount auras
                main_speed_mod  = GetMaxPositiveAuraModifier(SPELL_AURA_MOD_INCREASE_FLIGHT_SPEED);
            else             // Use not mount (shapeshift for example) auras (should stack)
                main_speed_mod  = GetTotalAuraModifier(SPELL_AURA_MOD_SPEED_FLIGHT);
            stack_bonus     = GetTotalAuraMultiplier(SPELL_AURA_MOD_FLIGHT_SPEED_ALWAYS);
            non_stack_bonus = (100.0 + GetMaxPositiveAuraModifier(SPELL_AURA_MOD_FLIGHT_SPEED_NOT_STACK))/100.0f;
            break;
        }
        case MOVE_FLYBACK:
            return;
        default:
            sLog.outError("Unit::UpdateSpeed: Unsupported move type (%d)", mtype);
            return;
    }
    
    float bonus = non_stack_bonus > stack_bonus ? non_stack_bonus : stack_bonus;
    // now we ready for speed calculation
    float speed  = main_speed_mod ? bonus*(100.0f + main_speed_mod)/100.0f : bonus;

    switch(mtype)
    {
        case MOVE_RUN:
        case MOVE_SWIM:
        case MOVE_FLY:
        {
            // Normalize speed by 191 aura SPELL_AURA_USE_NORMAL_MOVEMENT_SPEED if need
            // TODO: possible affect only on MOVE_RUN
            if(int32 normalization = GetMaxPositiveAuraModifier(SPELL_AURA_USE_NORMAL_MOVEMENT_SPEED))
            {
                // Use speed from aura
                float max_speed = normalization / baseMoveSpeed[mtype];
                if (speed > max_speed)
                    speed = max_speed;
            }
            break;
        }
        default:
            break;
    }

    // Apply strongest slow aura mod to speed
    int32 slow = GetMaxNegativeAuraModifier(SPELL_AURA_MOD_DECREASE_SPEED);
    if (slow)
        speed *=(100.0f + slow)/100.0f;
    SetSpeed(mtype, speed, forced);
}

float Unit::GetSpeed( UnitMoveType mtype ) const
{
    return m_speed_rate[mtype]*baseMoveSpeed[mtype];
}

void Unit::SetSpeed(UnitMoveType mtype, float rate, bool forced)
{
    if (rate < 0)
        rate = 0.0f;

    // Update speed only on change
    if (m_speed_rate[mtype] == rate)
        return;

    m_speed_rate[mtype] = rate;

    propagateSpeedChange();

    // Send speed change packet only for player
    if (GetTypeId()!=TYPEID_PLAYER)
        return;

    WorldPacket data;
    if(!forced)
    {
        switch(mtype)
        {
            case MOVE_WALK:
                data.Initialize(MSG_MOVE_SET_WALK_SPEED, 8+4+1+4+4+4+4+4+4+4);
                break;
            case MOVE_RUN:
                data.Initialize(MSG_MOVE_SET_RUN_SPEED, 8+4+1+4+4+4+4+4+4+4);
                break;
            case MOVE_WALKBACK:
                data.Initialize(MSG_MOVE_SET_RUN_BACK_SPEED, 8+4+1+4+4+4+4+4+4+4);
                break;
            case MOVE_SWIM:
                data.Initialize(MSG_MOVE_SET_SWIM_SPEED, 8+4+1+4+4+4+4+4+4+4);
                break;
            case MOVE_SWIMBACK:
                data.Initialize(MSG_MOVE_SET_SWIM_BACK_SPEED, 8+4+1+4+4+4+4+4+4+4);
                break;
            case MOVE_TURN:
                data.Initialize(MSG_MOVE_SET_TURN_RATE, 8+4+1+4+4+4+4+4+4+4);
                break;
            case MOVE_FLY:
                data.Initialize(MSG_MOVE_SET_FLIGHT_SPEED, 8+4+1+4+4+4+4+4+4+4);
                break;
            case MOVE_FLYBACK:
                data.Initialize(MSG_MOVE_SET_FLIGHT_BACK_SPEED, 8+4+1+4+4+4+4+4+4+4);
                break;
            default:
                sLog.outError("Unit::SetSpeed: Unsupported move type (%d), data not sent to client.",mtype);
                return;
        }

        data.append(GetPackGUID());
        data << uint32(0);                                  //movement flags
        data << uint8(0);                                   //unk
        data << uint32(getMSTime());
        data << float(GetPositionX());
        data << float(GetPositionY());
        data << float(GetPositionZ());
        data << float(GetOrientation());
        data << uint32(0);                                  //flag unk
        data << float(GetSpeed(mtype));
        SendMessageToSet( &data, true );
    }
    else
    {
        // register forced speed changes for WorldSession::HandleForceSpeedChangeAck
        // and do it only for real sent packets and use run for run/mounted as client expected
        ++((Player*)this)->m_forced_speed_changes[mtype];
        switch(mtype)
        {
            case MOVE_WALK:
                data.Initialize(SMSG_FORCE_WALK_SPEED_CHANGE, 16);
                break;
            case MOVE_RUN:
                data.Initialize(SMSG_FORCE_RUN_SPEED_CHANGE, 17);
                break;
            case MOVE_WALKBACK:
                data.Initialize(SMSG_FORCE_RUN_BACK_SPEED_CHANGE, 16);
                break;
            case MOVE_SWIM:
                data.Initialize(SMSG_FORCE_SWIM_SPEED_CHANGE, 16);
                break;
            case MOVE_SWIMBACK:
                data.Initialize(SMSG_FORCE_SWIM_BACK_SPEED_CHANGE, 16);
                break;
            case MOVE_TURN:
                data.Initialize(SMSG_FORCE_TURN_RATE_CHANGE, 16);
                break;
            case MOVE_FLY:
                data.Initialize(SMSG_FORCE_FLIGHT_SPEED_CHANGE, 16);
                break;
            case MOVE_FLYBACK:
                data.Initialize(SMSG_FORCE_FLIGHT_BACK_SPEED_CHANGE, 16);
                break;
            default:
                sLog.outError("Unit::SetSpeed: Unsupported move type (%d), data not sent to client.",mtype);
                return;
        }
        data.append(GetPackGUID());
        data << (uint32)0;
        if (mtype == MOVE_RUN)
            data << uint8(0);                               // new 2.1.0
        data << float(GetSpeed(mtype));
        SendMessageToSet( &data, true );
    }
    if(Pet* pet = GetPet())
        pet->SetSpeed(MOVE_RUN, m_speed_rate[mtype],forced);
}

void Unit::SetHover(bool on)
{
    if(on)
        CastSpell(this,11010,true);
    else
        RemoveAurasDueToSpell(11010);
}

void Unit::setDeathState(DeathState s)
{
    if (s != ALIVE && s!= JUST_ALIVED)
    {
        CombatStop();
        DeleteThreatList();
        ClearComboPointHolders();                           // any combo points pointed to unit lost at it death

        if(IsNonMeleeSpellCasted(false))
            InterruptNonMeleeSpells(false);
    }

    if (s == JUST_DIED)
    {
        RemoveAllAurasOnDeath();
        UnsummonAllTotems();

        ModifyAuraState(AURA_STATE_HEALTHLESS_20_PERCENT, false);
        ModifyAuraState(AURA_STATE_HEALTHLESS_35_PERCENT, false);
        // remove aurastates allowing special moves
        ClearAllReactives();
    }
    else if(s == JUST_ALIVED)
    {
        RemoveFlag (UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE); // clear skinnable for creature and player (at battleground)
    }

    if (m_deathState != ALIVE && s == ALIVE)
    {
        //_ApplyAllAuraMods();
    }
    m_deathState = s;
}

/*########################################
########                          ########
########       AGGRO SYSTEM       ########
########                          ########
########################################*/
bool Unit::CanHaveThreatList() const
{
    if(GetTypeId() == TYPEID_UNIT && !((Creature*)this)->isPet() && !((Creature*)this)->isTotem() )
        return true;
    else
        return false;
}

//======================================================================

float Unit::ApplyTotalThreatModifier(float threat, SpellSchoolMask schoolMask)
{
    if(!HasAuraType(SPELL_AURA_MOD_THREAT))
        return threat;

    SpellSchools school = GetFirstSchoolInMask(schoolMask);

    return threat * m_threatModifier[school];
}

//======================================================================

void Unit::AddThreat(Unit* pVictim, float threat, SpellSchoolMask schoolMask, SpellEntry const *threatSpell)
{
    // Only mobs can manage threat lists
    if(CanHaveThreatList())
        m_ThreatManager.addThreat(pVictim, threat, schoolMask, threatSpell);
}

//======================================================================

void Unit::DeleteThreatList()
{
    m_ThreatManager.clearReferences();
}

//======================================================================

void Unit::TauntApply(Unit* taunter)
{
    assert(GetTypeId()== TYPEID_UNIT);

    if(!taunter || (taunter->GetTypeId() == TYPEID_PLAYER && ((Player*)taunter)->isGameMaster()))
        return;

    if(!CanHaveThreatList())
        return;

    Unit *target = getVictim();
    if(target && target == taunter)
        return;

    SetInFront(taunter);
    if (((Creature*)this)->AI())
        ((Creature*)this)->AI()->AttackStart(taunter);

    m_ThreatManager.tauntApply(taunter);
}

//======================================================================

void Unit::TauntFadeOut(Unit *taunter)
{
    assert(GetTypeId()== TYPEID_UNIT);

    if(!taunter || (taunter->GetTypeId() == TYPEID_PLAYER && ((Player*)taunter)->isGameMaster()))
        return;

    if(!CanHaveThreatList())
        return;

    Unit *target = getVictim();
    if(!target || target != taunter)
        return;

    if(m_ThreatManager.isThreatListEmpty())
    {
        if(((Creature*)this)->AI())
            ((Creature*)this)->AI()->EnterEvadeMode();
        return;
    }

    m_ThreatManager.tauntFadeOut(taunter);
    target = m_ThreatManager.getHostilTarget();

    if (target && target != taunter)
    {
        SetInFront(target);
        if (((Creature*)this)->AI())
            ((Creature*)this)->AI()->AttackStart(target);
    }
}

//======================================================================

bool Unit::SelectHostilTarget()
{
    //function provides main threat functionality
    //next-victim-selection algorithm and evade mode are called
    //threat list sorting etc.

    assert(GetTypeId()== TYPEID_UNIT);
    Unit* target = NULL;

    //This function only useful once AI has been initilazied
    if (!((Creature*)this)->AI())
        return false;

    if(!m_ThreatManager.isThreatListEmpty())
    {
        if(!HasAuraType(SPELL_AURA_MOD_TAUNT))
        {
            target = m_ThreatManager.getHostilTarget();
        }
    }

    if(target)
    {
        if(!hasUnitState(UNIT_STAT_STUNDED))
            SetInFront(target);
        ((Creature*)this)->AI()->AttackStart(target);
        return true;
    }

    // no target but something prevent go to evade mode
    if( !isInCombat() || HasAuraType(SPELL_AURA_MOD_TAUNT) || !CanFreeMove() )
        return false;

    // last case when creature don't must go to evade mode:
    // it in combat but attacker not make any damage and not enter to aggro radius to have record in threat list
    // for example at owner command to pet attack some far away creature
    // Note: creature not have targeted movement generator but have attacker in this case
    if( GetMotionMaster()->empty() || GetMotionMaster()->top()->GetMovementGeneratorType() != TARGETED_MOTION_TYPE )
    {
        for(AttackerSet::const_iterator itr = m_attackers.begin(); itr != m_attackers.end(); ++itr)
        {
            if( (*itr)->IsInMap(this) && (*itr)->isTargetableForAttack() && (*itr)->isInAccessablePlaceFor((Creature*)this) )
                return false;
        }
    }

    // enter in evade mode in other case
    ((Creature*)this)->AI()->EnterEvadeMode();

    return false;
}

//======================================================================
//======================================================================
//======================================================================

int32 Unit::CalculateSpellDamage(SpellEntry const* spellProto, uint8 effect_index, int32 effBasePoints, Unit const* target)
{
    Player* unitPlayer = (GetTypeId() == TYPEID_PLAYER) ? (Player*)this : NULL;

    uint8 comboPoints = unitPlayer ? unitPlayer->GetComboPoints() : 0;

    int32 level = int32(getLevel()) - int32(spellProto->spellLevel);
    if (level > spellProto->maxLevel && spellProto->maxLevel > 0)
        level = spellProto->maxLevel;

    float basePointsPerLevel = spellProto->EffectRealPointsPerLevel[effect_index];
    float randomPointsPerLevel = spellProto->EffectDicePerLevel[effect_index];
    int32 basePoints = int32(effBasePoints + level * basePointsPerLevel);
    int32 randomPoints = int32(spellProto->EffectDieSides[effect_index] + level * randomPointsPerLevel);
    float comboDamage = spellProto->EffectPointsPerComboPoint[effect_index];

    // prevent random generator from getting confused by spells casted with Unit::CastCustomSpell
    int32 randvalue = spellProto->EffectBaseDice[effect_index] >= randomPoints ? spellProto->EffectBaseDice[effect_index]:irand(spellProto->EffectBaseDice[effect_index], randomPoints);
    int32 value = basePoints + randvalue;
    //random damage
    if(int32(spellProto->EffectBaseDice[effect_index]) != randomPoints && GetTypeId() == TYPEID_UNIT && ((Creature*)this)->isPet())
        value += ((Pet*)this)->GetBonusDamage();            //bonus damage only on spells without fixed basePoints?)

    if(comboDamage != 0 && unitPlayer && target && (target->GetGUID() == unitPlayer->GetComboTarget()))
        value += (int32)(comboDamage * comboPoints);

    if(Player* modOwner = GetSpellModOwner())
    {
        modOwner->ApplySpellMod(spellProto->Id,SPELLMOD_ALL_EFFECTS, value);
        switch(effect_index)
        {
            case 0:
                modOwner->ApplySpellMod(spellProto->Id,SPELLMOD_EFFECT1, value);
                break;
            case 1:
                modOwner->ApplySpellMod(spellProto->Id,SPELLMOD_EFFECT2, value);
                break;
            case 2:
                modOwner->ApplySpellMod(spellProto->Id,SPELLMOD_EFFECT3, value);
                break;
        }
        if( spellProto->Effect[effect_index] == SPELL_EFFECT_APPLY_AURA &&
            (spellProto->EffectApplyAuraName[effect_index] == SPELL_AURA_PERIODIC_DAMAGE ||
            spellProto->EffectApplyAuraName[effect_index] == SPELL_AURA_PERIODIC_HEAL ||
            spellProto->EffectApplyAuraName[effect_index] == SPELL_AURA_PERIODIC_LEECH) )
            modOwner->ApplySpellMod(spellProto->Id, SPELLMOD_DOT, value);
    }

    return value;
}

int32 Unit::CalculateSpellDuration(SpellEntry const* spellProto, uint8 effect_index, Unit const* target)
{
    Player* unitPlayer = (GetTypeId() == TYPEID_PLAYER) ? (Player*)this : NULL;

    uint8 comboPoints = unitPlayer ? unitPlayer->GetComboPoints() : 0;

    int32 minduration = GetSpellDuration(spellProto);
    int32 maxduration = GetSpellMaxDuration(spellProto);

    int32 duration;

    if( minduration != -1 && minduration != maxduration )
        duration = minduration + int32((maxduration - minduration) * comboPoints / 5);
    else
        duration = minduration;

    if (duration > 0)
    {
        int32 mechanic = GetEffectMechanic(spellProto, effect_index);
        // Find total mod value (negative bonus)
        int32 durationMod_always = target->GetTotalAuraModifierByMiscValue(SPELL_AURA_MECHANIC_DURATION_MOD, mechanic);
        // Find max mod (negative bonus)
        int32 durationMod_not_stack = target->GetMaxNegativeAuraModifierByMiscValue(SPELL_AURA_MECHANIC_DURATION_MOD_NOT_STACK, mechanic);

        int32 durationMod = 0;
        // Select strongest negative mod
        if (durationMod_always > durationMod_not_stack)
            durationMod = durationMod_not_stack;
        else
            durationMod = durationMod_always;

        if (durationMod != 0)
            duration = int32(int64(duration) * (100+durationMod) /100);

        if (duration < 0) duration = 0;
    }

    return duration;
}

void Unit::AddDiminishing(DiminishingMechanics mech, uint32 hitTime, uint32 hitCount)
{
    m_Diminishing.push_back(DiminishingReturn(mech,hitTime,hitCount));
}

DiminishingLevels Unit::GetDiminishing(DiminishingMechanics mech)
{
    for(Diminishing::iterator i = m_Diminishing.begin(); i != m_Diminishing.end(); ++i)
    {
        if(i->Mechanic != mech) continue;
        if(!i->hitCount) return DIMINISHING_LEVEL_1;
        if(!i->hitTime)  return DIMINISHING_LEVEL_1;
        // If last spell was casted more than 15 seconds ago - reset the count.
        if(getMSTimeDiff(i->hitTime,getMSTime()) > 15000)
        {
            i->hitCount = DIMINISHING_LEVEL_1;
            return DIMINISHING_LEVEL_1;
        }
        // or else increase the count.
        else
        {
            if(i->hitCount > DIMINISHING_LEVEL_2)
            {
                i->hitCount = DIMINISHING_LEVEL_IMMUNE;
                return DIMINISHING_LEVEL_IMMUNE;
            }
            else return DiminishingLevels(i->hitCount);
        }
    }
    return DIMINISHING_LEVEL_1;
}

void Unit::IncrDiminishing(DiminishingMechanics mech, uint32 duration)
{
    // Checking for existing in the table
    bool IsExist = false;
    for(Diminishing::iterator i = m_Diminishing.begin(); i != m_Diminishing.end(); ++i)
    {
        if(i->Mechanic != mech)
            continue;

        IsExist = true;
        if(i->hitCount < DIMINISHING_LEVEL_IMMUNE)
        {
            i->hitCount += 1;
            switch(i->hitCount)
            {
                case DIMINISHING_LEVEL_2:       i->hitTime = uint32(getMSTime() + duration); break;
                case DIMINISHING_LEVEL_3:       i->hitTime = uint32(getMSTime() + duration*0.5); break;
                case DIMINISHING_LEVEL_IMMUNE:  i->hitTime = uint32(getMSTime() + duration*0.25); break;
                default: break;
            }
        }
        break;
    }

    if(!IsExist)
        AddDiminishing(mech,uint32(getMSTime() + duration),DIMINISHING_LEVEL_2);
}

void Unit::UpdateDiminishingTime(DiminishingMechanics mech)
{
    // Checking for existing in the table
    bool IsExist = false;
    for(Diminishing::iterator i = m_Diminishing.begin(); i != m_Diminishing.end(); ++i)
    {
        if(i->Mechanic != mech)
            continue;

        IsExist = true;
        i->hitTime = getMSTime();
        break;
    }

    if(!IsExist)
        AddDiminishing(mech,getMSTime(),DIMINISHING_LEVEL_1);
}

DiminishingMechanics Unit::Mechanic2DiminishingMechanics(uint32 mech)
{
    switch(mech)
    {
        case MECHANIC_CHARM: case MECHANIC_FEAR: case MECHANIC_SLEEP:
            return DIMINISHING_MECHANIC_CHARM;

        case MECHANIC_CONFUSED: case MECHANIC_KNOCKOUT: case MECHANIC_POLYMORPH:
            return DIMINISHING_MECHANIC_CONFUSE;

        case MECHANIC_ROOT: case MECHANIC_FREEZE:
            return DIMINISHING_MECHANIC_ROOT;

        case MECHANIC_STUN:
            return DIMINISHING_MECHANIC_STUN;

        case MECHANIC_SNARE:
            return DIMINISHING_MECHANIC_SNARE;

        case MECHANIC_BANISH:
            return DIMINISHING_MECHANIC_BANISH;
        default:
            break;
    }
    return DIMINISHING_NONE;
}

void Unit::ApplyDiminishingToDuration(DiminishingMechanics  mech, int32 &duration,Unit* caster)
{
    if(duration == -1 || mech == DIMINISHING_NONE || caster->IsFriendlyTo(this) )
        return;

    // Duration of crowd control abilities on pvp target is limited by 10 sec. (2.2.0)
    if(duration > 10000)
    {
        // test pet/charm masters instead pets/charmeds
        Unit const* targetOwner = GetCharmerOrOwner();
        Unit const* casterOwner = caster->GetCharmerOrOwner();

        Unit const* target = targetOwner ? targetOwner : this;
        Unit const* source = casterOwner ? casterOwner : caster;

        if(target->GetTypeId() == TYPEID_PLAYER && source->GetTypeId() == TYPEID_PLAYER)
            duration = 10000;
    }

    float mod = 1.0f;

    // Stun diminishing is applies to mobs too
    if(mech == DIMINISHING_MECHANIC_STUN || GetTypeId() == TYPEID_PLAYER)
    {
        DiminishingLevels diminish = GetDiminishing(mech);
        switch(diminish)
        {
            case DIMINISHING_LEVEL_1: IncrDiminishing(mech, duration); break;
            case DIMINISHING_LEVEL_2: IncrDiminishing(mech, duration); mod = 0.5f; break;
            case DIMINISHING_LEVEL_3: IncrDiminishing(mech, duration); mod = 0.25f; break;
            case DIMINISHING_LEVEL_IMMUNE: mod = 0.0f; break;
            default: break;
        }
    }

    duration = int32(duration * mod);
}

Unit* Unit::GetUnit(WorldObject& object, uint64 guid)
{
    return ObjectAccessor::GetUnit(object,guid);
}

bool Unit::isVisibleForInState( Player const* u, bool inVisibleList ) const
{
    return isVisibleForOrDetect(u,false,inVisibleList);
}

uint32 Unit::GetCreatureType() const
{
    if(GetTypeId() == TYPEID_PLAYER)
    {
        SpellShapeshiftEntry const* ssEntry = sSpellShapeshiftStore.LookupEntry(((Player*)this)->m_form);
        if(ssEntry && ssEntry->creatureType > 0)
            return ssEntry->creatureType;
        else
            return CREATURE_TYPE_HUMANOID;
    }
    else
        return ((Creature*)this)->GetCreatureInfo()->type;
}

/*#######################################
########                         ########
########       STAT SYSTEM       ########
########                         ########
#######################################*/

bool Unit::HandleStatModifier(UnitMods unitMod, UnitModifierType modifierType, float amount, bool apply)
{
    if(unitMod >= UNIT_MOD_END || modifierType >= MODIFIER_TYPE_END)
    {
        sLog.outError("ERROR in HandleStatModifier(): nonexisted UnitMods or wrong UnitModifierType!");
        return false;
    }

    float val = 1.0f;

    switch(modifierType)
    {
        case BASE_VALUE:
        case TOTAL_VALUE:
            m_auraModifiersGroup[unitMod][modifierType] += apply ? amount : -amount;
            break;
        case BASE_PCT:
        case TOTAL_PCT:
            if(amount <= -100.0f)                           //small hack-fix for -100% modifiers
                amount = -200.0f;

            val = (100.0f + amount) / 100.0f;
            m_auraModifiersGroup[unitMod][modifierType] *= apply ? val : (1.0f/val);
            break;

        default:
            break;
    }

    if(!CanModifyStats())
        return false;

    switch(unitMod)
    {
        case UNIT_MOD_STAT_STRENGTH:
        case UNIT_MOD_STAT_AGILITY:
        case UNIT_MOD_STAT_STAMINA:
        case UNIT_MOD_STAT_INTELLECT:
        case UNIT_MOD_STAT_SPIRIT:         UpdateStats(GetStatByAuraGroup(unitMod));  break;

        case UNIT_MOD_ARMOR:               UpdateArmor();           break;
        case UNIT_MOD_HEALTH:              UpdateMaxHealth();       break;

        case UNIT_MOD_MANA:
        case UNIT_MOD_RAGE:
        case UNIT_MOD_FOCUS:
        case UNIT_MOD_ENERGY:
        case UNIT_MOD_HAPPINESS:           UpdateMaxPower(GetPowerTypeByAuraGroup(unitMod));         break;

        case UNIT_MOD_RESISTANCE_HOLY:
        case UNIT_MOD_RESISTANCE_FIRE:
        case UNIT_MOD_RESISTANCE_NATURE:
        case UNIT_MOD_RESISTANCE_FROST:
        case UNIT_MOD_RESISTANCE_SHADOW:
        case UNIT_MOD_RESISTANCE_ARCANE:   UpdateResistances(GetSpellSchoolByAuraGroup(unitMod));      break;

        case UNIT_MOD_ATTACK_POWER:        UpdateAttackPowerAndDamage();         break;
        case UNIT_MOD_ATTACK_POWER_RANGED: UpdateAttackPowerAndDamage(true);     break;

        case UNIT_MOD_DAMAGE_MAINHAND:     UpdateDamagePhysical(BASE_ATTACK);    break;
        case UNIT_MOD_DAMAGE_OFFHAND:      UpdateDamagePhysical(OFF_ATTACK);     break;
        case UNIT_MOD_DAMAGE_RANGED:       UpdateDamagePhysical(RANGED_ATTACK);  break;

        default:
            break;
    }

    return true;
}

float Unit::GetModifierValue(UnitMods unitMod, UnitModifierType modifierType) const
{
    if( unitMod >= UNIT_MOD_END || modifierType >= MODIFIER_TYPE_END)
    {
        sLog.outError("ERROR: trial to access nonexisted modifier value from UnitMods!");
        return 0.0f;
    }

    if(modifierType == TOTAL_PCT && m_auraModifiersGroup[unitMod][modifierType] <= 0.0f)
        return 0.0f;

    return m_auraModifiersGroup[unitMod][modifierType];
}

float Unit::GetTotalStatValue(Stats stat) const
{
    UnitMods unitMod = UnitMods(UNIT_MOD_STAT_START + stat);

    if(m_auraModifiersGroup[unitMod][TOTAL_PCT] <= 0.0f)
        return 0.0f;

    // value = ((base_value * base_pct) + total_value) * total_pct
    float value  = m_auraModifiersGroup[unitMod][BASE_VALUE] + GetCreateStat(stat);
    value *= m_auraModifiersGroup[unitMod][BASE_PCT];
    value += m_auraModifiersGroup[unitMod][TOTAL_VALUE];
    value *= m_auraModifiersGroup[unitMod][TOTAL_PCT];

    return value;
}

float Unit::GetTotalAuraModValue(UnitMods unitMod) const
{
    if(unitMod >= UNIT_MOD_END)
    {
        sLog.outError("ERROR: trial to access nonexisted UnitMods in GetTotalAuraModValue()!");
        return 0.0f;
    }

    if(m_auraModifiersGroup[unitMod][TOTAL_PCT] <= 0.0f)
        return 0.0f;

    float value  = m_auraModifiersGroup[unitMod][BASE_VALUE];
    value *= m_auraModifiersGroup[unitMod][BASE_PCT];
    value += m_auraModifiersGroup[unitMod][TOTAL_VALUE];
    value *= m_auraModifiersGroup[unitMod][TOTAL_PCT];

    return value;
}

SpellSchools Unit::GetSpellSchoolByAuraGroup(UnitMods unitMod) const
{
    SpellSchools school = SPELL_SCHOOL_NORMAL;

    switch(unitMod)
    {
        case UNIT_MOD_RESISTANCE_HOLY:     school = SPELL_SCHOOL_HOLY;          break;
        case UNIT_MOD_RESISTANCE_FIRE:     school = SPELL_SCHOOL_FIRE;          break;
        case UNIT_MOD_RESISTANCE_NATURE:   school = SPELL_SCHOOL_NATURE;        break;
        case UNIT_MOD_RESISTANCE_FROST:    school = SPELL_SCHOOL_FROST;         break;
        case UNIT_MOD_RESISTANCE_SHADOW:   school = SPELL_SCHOOL_SHADOW;        break;
        case UNIT_MOD_RESISTANCE_ARCANE:   school = SPELL_SCHOOL_ARCANE;        break;

        default:
            break;
    }

    return school;
}

Stats Unit::GetStatByAuraGroup(UnitMods unitMod) const
{
    Stats stat = STAT_STRENGTH;

    switch(unitMod)
    {
        case UNIT_MOD_STAT_STRENGTH:    stat = STAT_STRENGTH;      break;
        case UNIT_MOD_STAT_AGILITY:     stat = STAT_AGILITY;       break;
        case UNIT_MOD_STAT_STAMINA:     stat = STAT_STAMINA;       break;
        case UNIT_MOD_STAT_INTELLECT:   stat = STAT_INTELLECT;     break;
        case UNIT_MOD_STAT_SPIRIT:      stat = STAT_SPIRIT;        break;

        default:
            break;
    }

    return stat;
}

Powers Unit::GetPowerTypeByAuraGroup(UnitMods unitMod) const
{
    Powers power = POWER_MANA;

    switch(unitMod)
    {
        case UNIT_MOD_MANA:       power = POWER_MANA;       break;
        case UNIT_MOD_RAGE:       power = POWER_RAGE;       break;
        case UNIT_MOD_FOCUS:      power = POWER_FOCUS;      break;
        case UNIT_MOD_ENERGY:     power = POWER_ENERGY;     break;
        case UNIT_MOD_HAPPINESS:  power = POWER_HAPPINESS;  break;

        default:
            break;
    }

    return power;
}

float Unit::GetTotalAttackPowerValue(WeaponAttackType attType) const
{
    UnitMods unitMod = (attType == RANGED_ATTACK) ? UNIT_MOD_ATTACK_POWER_RANGED : UNIT_MOD_ATTACK_POWER;

    float val = GetTotalAuraModValue(unitMod);
    if(val < 0.0f)
        val = 0.0f;

    return val;
}

float Unit::GetWeaponDamageRange(WeaponAttackType attType ,WeaponDamageRange type) const
{
    if (attType == OFF_ATTACK && !haveOffhandWeapon())
        return 0.0f;

    return m_weaponDamage[attType][type];
}

void Unit::SetLevel(uint32 lvl)
{
    SetUInt32Value(UNIT_FIELD_LEVEL, lvl);

    // group update
    if ((GetTypeId() == TYPEID_PLAYER) && ((Player*)this)->GetGroup())
        ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_LEVEL);
}

void Unit::SetHealth(uint32 val)
{
    uint32 maxHealth = GetMaxHealth();
    if(maxHealth < val)
        val = maxHealth;

    SetUInt32Value(UNIT_FIELD_HEALTH, val);

    // group update
    if(GetTypeId() == TYPEID_PLAYER)
    {
        if(((Player*)this)->GetGroup())
            ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_CUR_HP);
    }
    else if(((Creature*)this)->isPet())
    {
        Pet *pet = ((Pet*)this);
        if(pet->isControlled())
        {
            Unit *owner = GetOwner();
            if(owner && (owner->GetTypeId() == TYPEID_PLAYER) && ((Player*)owner)->GetGroup())
                ((Player*)owner)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_CUR_HP);
        }
    }
}

void Unit::SetMaxHealth(uint32 val)
{
    uint32 health = GetHealth();
    SetUInt32Value(UNIT_FIELD_MAXHEALTH, val);

    // group update
    if(GetTypeId() == TYPEID_PLAYER)
    {
        if(((Player*)this)->GetGroup())
            ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_MAX_HP);
    }
    else if(((Creature*)this)->isPet())
    {
        Pet *pet = ((Pet*)this);
        if(pet->isControlled())
        {
            Unit *owner = GetOwner();
            if(owner && (owner->GetTypeId() == TYPEID_PLAYER) && ((Player*)owner)->GetGroup())
                ((Player*)owner)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_MAX_HP);
        }
    }

    if(val < health)
        SetHealth(val);
}

void Unit::SetPower(Powers power, uint32 val)
{
    uint32 maxPower = GetMaxPower(power);
    if(maxPower < val)
        val = maxPower;

    SetStatInt32Value(UNIT_FIELD_POWER1 + power, val);

    // group update
    if(GetTypeId() == TYPEID_PLAYER)
    {
        if(((Player*)this)->GetGroup())
            ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_CUR_POWER);
    }
    else if(((Creature*)this)->isPet())
    {
        Pet *pet = ((Pet*)this);
        if(pet->isControlled())
        {
            Unit *owner = GetOwner();
            if(owner && (owner->GetTypeId() == TYPEID_PLAYER) && ((Player*)owner)->GetGroup())
                ((Player*)owner)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_CUR_POWER);
        }

        // Update the pet's character sheet with happiness damage bonus
        if(pet->getPetType() == HUNTER_PET && power == POWER_HAPPINESS)
        {
            pet->UpdateDamagePhysical(BASE_ATTACK);
        }
    }
}

void Unit::SetMaxPower(Powers power, uint32 val)
{
    uint32 cur_power = GetPower(power);
    SetStatInt32Value(UNIT_FIELD_MAXPOWER1 + power, val);

    // group update
    if(GetTypeId() == TYPEID_PLAYER)
    {
        if(((Player*)this)->GetGroup())
            ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_MAX_POWER);
    }
    else if(((Creature*)this)->isPet())
    {
        Pet *pet = ((Pet*)this);
        if(pet->isControlled())
        {
            Unit *owner = GetOwner();
            if(owner && (owner->GetTypeId() == TYPEID_PLAYER) && ((Player*)owner)->GetGroup())
                ((Player*)owner)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_MAX_POWER);
        }
    }

    if(val < cur_power)
        SetPower(power, val);
}

void Unit::ApplyPowerMod(Powers power, uint32 val, bool apply)
{
    ApplyModUInt32Value(UNIT_FIELD_POWER1+power, val, apply);

    // group update
    if(GetTypeId() == TYPEID_PLAYER)
    {
        if(((Player*)this)->GetGroup())
            ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_CUR_POWER);
    }
    else if(((Creature*)this)->isPet())
    {
        Pet *pet = ((Pet*)this);
        if(pet->isControlled())
        {
            Unit *owner = GetOwner();
            if(owner && (owner->GetTypeId() == TYPEID_PLAYER) && ((Player*)owner)->GetGroup())
                ((Player*)owner)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_CUR_POWER);
        }
    }
}

void Unit::ApplyMaxPowerMod(Powers power, uint32 val, bool apply)
{
    ApplyModUInt32Value(UNIT_FIELD_MAXPOWER1+power, val, apply);

    // group update
    if(GetTypeId() == TYPEID_PLAYER)
    {
        if(((Player*)this)->GetGroup())
            ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_MAX_POWER);
    }
    else if(((Creature*)this)->isPet())
    {
        Pet *pet = ((Pet*)this);
        if(pet->isControlled())
        {
            Unit *owner = GetOwner();
            if(owner && (owner->GetTypeId() == TYPEID_PLAYER) && ((Player*)owner)->GetGroup())
                ((Player*)owner)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_MAX_POWER);
        }
    }
}

void Unit::ApplyAuraProcTriggerDamage( Aura* aura, bool apply )
{
    AuraList& tAuraProcTriggerDamage = m_modAuras[SPELL_AURA_PROC_TRIGGER_DAMAGE];
    if(apply)
        tAuraProcTriggerDamage.push_back(aura);
    else
        tAuraProcTriggerDamage.remove(aura);
}

uint32 Unit::GetCreatePowers( Powers power ) const
{
    // POWER_FOCUS and POWER_HAPPINESS only have hunter pet
    switch(power)
    {
        case POWER_MANA:      return GetCreateMana();
        case POWER_RAGE:      return 1000;
        case POWER_FOCUS:     return (GetTypeId()==TYPEID_PLAYER || !((Creature const*)this)->isPet() || ((Pet const*)this)->getPetType()!=HUNTER_PET ? 0 : 100);
        case POWER_ENERGY:    return 100;
        case POWER_HAPPINESS: return (GetTypeId()==TYPEID_PLAYER || !((Creature const*)this)->isPet() || ((Pet const*)this)->getPetType()!=HUNTER_PET ? 0 : 1050000);
    }

    return 0;
}

void Unit::CleanupsBeforeDelete()
{
    if(m_uint32Values)                                      // only for fully created object
    {
        InterruptNonMeleeSpells(true);
        m_Events.KillAllEvents();
        CombatStop();
        ClearComboPointHolders();
        DeleteThreatList();
        getHostilRefManager().setOnlineOfflineState(false);
        RemoveAllAuras();
    }
    RemoveFromWorld();
}

CharmInfo* Unit::InitCharmInfo(Unit *charm)
{
    if(!m_charmInfo)
        m_charmInfo = new CharmInfo(charm);
    return m_charmInfo;
}

CharmInfo::CharmInfo(Unit* unit)
: m_unit(unit), m_CommandState(COMMAND_STAY), m_ReactSate(REACT_PASSIVE), m_petnumber(0)
{
    for(int i =0; i<4; ++i)
    {
        m_charmspells[i].spellId = 0;
        m_charmspells[i].active = ACT_DISABLED;
    }
}

void CharmInfo::InitPetActionBar()
{
    // the first 3 SpellOrActions are attack, follow and stay
    for(uint32 i = 0; i < 3; i++)
    {
        PetActionBar[i].Type = ACT_COMMAND;
        PetActionBar[i].SpellOrAction = COMMAND_ATTACK - i;

        PetActionBar[i + 7].Type = ACT_REACTION;
        PetActionBar[i + 7].SpellOrAction = COMMAND_ATTACK - i;
    }
    for(uint32 i=0; i < 4; i++)
    {
        PetActionBar[i + 3].Type = ACT_DISABLED;
        PetActionBar[i + 3].SpellOrAction = 0;
    }
}

void CharmInfo::InitEmptyActionBar()
{
    for(uint32 x = 1; x < 10; ++x)
    {
        PetActionBar[x].Type = ACT_CAST;
        PetActionBar[x].SpellOrAction = 0;
    }
    PetActionBar[0].Type = ACT_COMMAND;
    PetActionBar[0].SpellOrAction = COMMAND_ATTACK;
}

void CharmInfo::InitPossessCreateSpells()
{
    if(m_unit->GetTypeId() == TYPEID_PLAYER)
        return;

    InitEmptyActionBar();                                   //charm action bar

    for(uint32 x = 0; x < CREATURE_MAX_SPELLS; ++x)
    {
        if (IsPassiveSpell(((Creature*)m_unit)->m_spells[x]))
            m_unit->CastSpell(m_unit, ((Creature*)m_unit)->m_spells[x], true);
        else
            AddSpellToAB(0, ((Creature*)m_unit)->m_spells[x], ACT_CAST);
    }
}

void CharmInfo::InitCharmCreateSpells()
{
    if(m_unit->GetTypeId() == TYPEID_PLAYER)                //charmed players don't have spells
    {
        InitEmptyActionBar();
        return;
    }

    InitPetActionBar();

    for(uint32 x = 0; x < CREATURE_MAX_SPELLS; ++x)
    {
        uint32 spellId = ((Creature*)m_unit)->m_spells[x];
        m_charmspells[x].spellId = spellId;

        if(!spellId)
            continue;

        if (IsPassiveSpell(spellId))
        {
            m_unit->CastSpell(m_unit, spellId, true);
            m_charmspells[x].active = ACT_PASSIVE;
        }
        else
        {
            ActiveStates newstate;
            bool onlyselfcast = true;
            SpellEntry const *spellInfo = sSpellStore.LookupEntry(spellId);

            if(!spellInfo) onlyselfcast = false;
            for(uint32 i = 0;i<3 && onlyselfcast;++i)       //non existent spell will not make any problems as onlyselfcast would be false -> break right away
            {
                if(spellInfo->EffectImplicitTargetA[i] != TARGET_SELF && spellInfo->EffectImplicitTargetA[i] != 0)
                    onlyselfcast = false;
            }

            if(onlyselfcast || !IsPositiveSpell(spellId))   //only self cast and spells versus enemies are autocastable
                newstate = ACT_DISABLED;
            else
                newstate = ACT_CAST;

            AddSpellToAB(0, spellId, newstate);
        }
    }
}

bool CharmInfo::AddSpellToAB(uint32 oldid, uint32 newid, ActiveStates newstate)
{
    for(uint8 i = 0; i < 10; i++)
    {
        if((PetActionBar[i].Type == ACT_DISABLED || PetActionBar[i].Type == ACT_ENABLED || PetActionBar[i].Type == ACT_CAST) && PetActionBar[i].SpellOrAction == oldid)
        {
            PetActionBar[i].SpellOrAction = newid;
            if(!oldid)
            {
                if(newstate == ACT_DECIDE)
                    PetActionBar[i].Type = ACT_DISABLED;
                else
                    PetActionBar[i].Type = newstate;
            }

            return true;
        }
    }
    return false;
}

void CharmInfo::ToggleCreatureAutocast(uint32 spellid, bool apply)
{
    if(IsPassiveSpell(spellid))
        return;

    for(uint32 x = 0; x < CREATURE_MAX_SPELLS; ++x)
    {
        if(spellid == m_charmspells[x].spellId)
        {
            m_charmspells[x].active = apply ? ACT_ENABLED : ACT_DISABLED;
        }
    }
}

void CharmInfo::SetPetNumber(uint32 petnumber, bool statwindow)
{
    m_petnumber = petnumber;
    if(statwindow)
        m_unit->SetUInt32Value(UNIT_FIELD_PETNUMBER, m_petnumber);
    else
        m_unit->SetUInt32Value(UNIT_FIELD_PETNUMBER, 0);
}

bool Unit::isFrozen() const
{
    AuraList const& mRoot = GetAurasByType(SPELL_AURA_MOD_ROOT);
    for(AuraList::const_iterator i = mRoot.begin(); i != mRoot.end(); ++i)
        if( GetSpellSchoolMask((*i)->GetSpellProto()) & SPELL_SCHOOL_MASK_FROST)
            return true;
    return false;
}

struct ProcTriggeredData
{
    ProcTriggeredData(SpellEntry const * _spellInfo, uint32 _spellParam, Aura* _triggeredByAura)
        : spellInfo(_spellInfo), spellParam(_spellParam), triggeredByAura(_triggeredByAura),
        triggeredByAura_SpellPair(Unit::spellEffectPair(triggeredByAura->GetId(),triggeredByAura->GetEffIndex()))
        {}

    SpellEntry const * spellInfo;
    uint32 spellParam;
    Aura* triggeredByAura;
    Unit::spellEffectPair triggeredByAura_SpellPair;
};

typedef std::list< ProcTriggeredData > ProcTriggeredList;

void Unit::ProcDamageAndSpellFor( bool isVictim, Unit * pTarget, uint32 procFlag, AuraTypeSet const& procAuraTypes, WeaponAttackType attType, SpellEntry const * procSpell, uint32 damage )
{
    for(AuraTypeSet::const_iterator aur = procAuraTypes.begin(); aur != procAuraTypes.end(); ++aur)
    {
        // List of spells (effects) that proceed. Spell prototype and aura-specific value (damage for TRIGGER_DAMAGE)
        ProcTriggeredList procTriggered;

        AuraList const& auras = GetAurasByType(*aur);
        for(AuraList::const_iterator i = auras.begin(), next; i != auras.end(); i = next)
        {
            next = i; ++next;

            SpellEntry const *spellProto = (*i)->GetSpellProto();
            if(!spellProto)
                continue;

            SpellProcEventEntry const *spellProcEvent = spellmgr.GetSpellProcEvent(spellProto->Id);
            if(!spellProcEvent)
            {
                // used to prevent spam in log about same non-handled spells
                static std::set<uint32> nonHandledSpellProcSet;

                if(spellProto->procFlags != 0 && nonHandledSpellProcSet.find(spellProto->Id)==nonHandledSpellProcSet.end())
                {
                    sLog.outError("ProcDamageAndSpell: spell %u (%s aura source) not have record in `spell_proc_event`)",spellProto->Id,(isVictim?"a victim's":"an attacker's"));
                    nonHandledSpellProcSet.insert(spellProto->Id);
                }

                // spell.dbc use totally different flags, that only can create problems if used.
                continue;
            }

            // Check spellProcEvent data requirements
            if(!SpellMgr::IsSpellProcEventCanTriggeredBy(spellProcEvent, procSpell,procFlag))
                continue;

            // Check if current equipment allows aura to proc
            if(!isVictim && GetTypeId() == TYPEID_PLAYER )
            {
                if(spellProto->EquippedItemClass == ITEM_CLASS_WEAPON)
                {
                    uint8 slot = Player::GetWeaponSlotByAttack(attType);
                    Item *item = ((Player*)this)->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);

                    if (!((Player*)this)->IsUseEquipedWeapon(attType==BASE_ATTACK))
                        continue;

                    if(!item || item->IsBroken() || item->GetProto()->Class != ITEM_CLASS_WEAPON || !((1<<item->GetProto()->SubClass) & spellProto->EquippedItemSubClassMask))
                        continue;
                }
                else if(spellProto->EquippedItemClass == ITEM_CLASS_ARMOR)
                {
                    // Check if player is wearing shield
                    Item *item = ((Player*)this)->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
                    if(!item || item->IsBroken() || item->GetProto()->Class != ITEM_CLASS_ARMOR || !((1<<item->GetProto()->SubClass) & spellProto->EquippedItemSubClassMask))
                        continue;
                }
            }

            float chance = (float)spellProto->procChance;

            if(Player* modOwner = GetSpellModOwner())
                modOwner->ApplySpellMod(spellProto->Id,SPELLMOD_CHANCE_OF_SUCCESS,chance);

            if(!isVictim && spellProcEvent->ppmRate != 0)
            {
                uint32 WeaponSpeed = GetAttackTime(attType);
                chance = GetPPMProcChance(WeaponSpeed, spellProcEvent->ppmRate);
            }

            if(roll_chance_f(chance))
            {
                if((*i)->m_procCharges > 0)
                    (*i)->m_procCharges -= 1;

                uint32 i_spell_eff = (*i)->GetEffIndex();

                int32 i_spell_param;
                switch(*aur)
                {
                    case SPELL_AURA_PROC_TRIGGER_SPELL: i_spell_param = procFlag;    break;
                    case SPELL_AURA_DUMMY:
                    case SPELL_AURA_DUMMY_3:
                    case SPELL_AURA_MOD_HASTE:          i_spell_param = i_spell_eff; break;
                    default: i_spell_param = (*i)->GetModifier()->m_amount;          break;
                }

                procTriggered.push_back( ProcTriggeredData(spellProto,i_spell_param,*i) );
            }
        }

        // Handle effects proceed this time
        for(ProcTriggeredList::iterator i = procTriggered.begin(); i != procTriggered.end(); ++i)
        {
            // Some auras can be deleted in function called in this loop (except first, ofc)
            // Until storing auras in std::multimap to hard check deleting by another way
            if(i != procTriggered.begin())
            {
                bool found = false;
                AuraMap::const_iterator lower = GetAuras().lower_bound(i->triggeredByAura_SpellPair);
                AuraMap::const_iterator upper = GetAuras().upper_bound(i->triggeredByAura_SpellPair);
                for(AuraMap::const_iterator itr = lower; itr!= upper; ++itr)
                {
                    if(itr->second==i->triggeredByAura)
                    {
                        found = true;
                        break;
                    }
                }

                if(!found)
                {
                    sLog.outError("Spell aura %u (id:%u effect:%u) has been deleted before call spell proc event handler",*aur,i->triggeredByAura_SpellPair.first,i->triggeredByAura_SpellPair.second);
                    sLog.outError("It can be deleted one from early proccesed auras:");
                    for(ProcTriggeredList::iterator i2 = procTriggered.begin(); i != i2; ++i2)
                        sLog.outError("     Spell aura %u (id:%u effect:%u)",*aur,i2->triggeredByAura_SpellPair.first,i2->triggeredByAura_SpellPair.second);
                    sLog.outError("     <end of list>");
                    continue;
                }
            }

            // save charges existence before processing to prevent crash at access to deleted triggered aura after
            bool triggeredByAuraWithCharges =  i->triggeredByAura->m_procCharges > 0;

            switch(*aur)
            {
                case SPELL_AURA_PROC_TRIGGER_SPELL:
                {
                    sLog.outDebug("ProcDamageAndSpell: casting spell %u (triggered by %s aura of spell %u)", i->spellInfo->Id,(isVictim?"a victim's":"an attacker's"),i->triggeredByAura->GetId());
                    HandleProcTriggerSpell(pTarget, damage, i->triggeredByAura, procSpell,i->spellParam,attType);
                    break;
                }
                case SPELL_AURA_PROC_TRIGGER_DAMAGE:
                {
                    sLog.outDebug("ProcDamageAndSpell: doing %u damage from spell id %u (triggered by %s aura of spell %u)", i->spellParam, i->spellInfo->Id,(isVictim?"a victim's":"an attacker's"),i->triggeredByAura->GetId());
                    uint32 damage = i->spellParam;
                    SpellNonMeleeDamageLog(pTarget, i->spellInfo->Id, damage, true, true);
                    break;
                }
                case SPELL_AURA_DUMMY:
                case SPELL_AURA_DUMMY_3:
                {
                    sLog.outDebug("ProcDamageAndSpell: casting spell id %u (triggered by %s dummy aura of spell %u)", i->spellInfo->Id,(isVictim?"a victim's":"an attacker's"),i->triggeredByAura->GetId());
                    HandleDummyAuraProc(pTarget, i->spellInfo, i->spellParam, damage, i->triggeredByAura, procSpell, procFlag);
                    break;
                }
                case SPELL_AURA_MOD_HASTE:
                {
                    sLog.outDebug("ProcDamageAndSpell: casting spell id %u (triggered by %s haste aura of spell %u)", i->spellInfo->Id,(isVictim?"a victim's":"an attacker's"),i->triggeredByAura->GetId());
                    HandleDummyAuraProc(pTarget, i->spellInfo, i->spellParam, damage, i->triggeredByAura, procSpell, procFlag);
                    break;
                }
                default:
                {
                    // nothing do, just charges counter
                    break;
                }
            }
            // Update charge (aura can be removed by triggers)
            if(triggeredByAuraWithCharges)
            {
                // need found aura (can be dropped by triggers)
                AuraMap::const_iterator lower = GetAuras().lower_bound(i->triggeredByAura_SpellPair);
                AuraMap::const_iterator upper = GetAuras().upper_bound(i->triggeredByAura_SpellPair);
                for(AuraMap::const_iterator itr = lower; itr!= upper; ++itr)
                {
                    if(itr->second == i->triggeredByAura)
                    {
                        i->triggeredByAura->UpdateAuraCharges();
                        break;
                    }
                }
            }
        }

        // Safely remove auras with zero charges
        for(AuraList::const_iterator i = auras.begin(), next; i != auras.end(); i = next)
        {
            next = i; ++next;
            if((*i)->m_procCharges == 0)
            {
                RemoveAurasDueToSpell((*i)->GetId());
                next = auras.begin();
            }
        }
    }
}

SpellSchoolMask Unit::GetMeleeDamageSchoolMask() const
{
    return SPELL_SCHOOL_MASK_NORMAL;
}

Player* Unit::GetSpellModOwner()
{
    if(GetTypeId()==TYPEID_PLAYER)
        return (Player*)this;
    if(((Creature*)this)->isPet() || ((Creature*)this)->isTotem())
    {
        Unit* owner = GetOwner();
        if(owner && owner->GetTypeId()==TYPEID_PLAYER)
            return (Player*)owner;
    }
    return NULL;
}

///----------Pet responses methods-----------------
void Unit::SendPetCastFail(uint32 spellid, uint8 msg)
{
    Unit *owner = GetCharmerOrOwner();
    if(!owner || owner->GetTypeId() != TYPEID_PLAYER)
        return;

    WorldPacket data(SMSG_PET_CAST_FAILED, (4+1));
    data << uint32(spellid);
    data << uint8(msg);
    ((Player*)owner)->GetSession()->SendPacket(&data);
}

void Unit::SendPetActionFeedback (uint8 msg)
{
    Unit* owner = GetOwner();
    if(!owner || owner->GetTypeId() != TYPEID_PLAYER)
        return;

    WorldPacket data(SMSG_PET_ACTION_FEEDBACK, 1);
    data << uint8(msg);
    ((Player*)owner)->GetSession()->SendPacket(&data);
}

void Unit::SendPetTalk (uint32 pettalk)
{
    Unit* owner = GetOwner();
    if(!owner || owner->GetTypeId() != TYPEID_PLAYER)
        return;

    WorldPacket data(SMSG_PET_ACTION_SOUND, 8+4);
    data << uint64(GetGUID());
    data << uint32(pettalk);
    ((Player*)owner)->GetSession()->SendPacket(&data);
}

void Unit::SendPetSpellCooldown (uint32 spellid, time_t cooltime)
{
    Unit* owner = GetOwner();
    if(!owner || owner->GetTypeId() != TYPEID_PLAYER)
        return;

    WorldPacket data(SMSG_SPELL_COOLDOWN, 8+1+4+4);
    data << uint64(GetGUID());
    data << uint8(0x0);
    data << uint32(spellid);
    data << uint32(cooltime);

    ((Player*)owner)->GetSession()->SendPacket(&data);
}

void Unit::SendPetClearCooldown (uint32 spellid)
{
    Unit* owner = GetOwner();
    if(!owner || owner->GetTypeId() != TYPEID_PLAYER)
        return;

    WorldPacket data(SMSG_CLEAR_COOLDOWN, (4+8));
    data << uint32(spellid);
    data << uint64(GetGUID());
    ((Player*)owner)->GetSession()->SendPacket(&data);
}

void Unit::SendPetAIReaction(uint64 guid)
{
    Unit* owner = GetOwner();
    if(!owner || owner->GetTypeId() != TYPEID_PLAYER)
        return;

    WorldPacket data(SMSG_AI_REACTION, 12);
    data << uint64(guid) << uint32(00000002);
    ((Player*)owner)->GetSession()->SendPacket(&data);
}

///----------End of Pet responses methods----------

void Unit::StopMoving()
{
    clearUnitState(UNIT_STAT_MOVING);

    // send explicit stop packet
    SendMonsterMove(GetPositionX(), GetPositionY(), GetPositionZ(),0, GetUnitMovementFlags(), 0);

    // update position and orientation;
    WorldPacket data;
    BuildHeartBeatMsg(&data);
    SendMessageToSet(&data,false);
}

bool Unit::IsStandState() const
{
    uint8 s = getStandState();
    return s != PLAYER_STATE_SIT_CHAIR && s != PLAYER_STATE_SIT_LOW_CHAIR &&
        s != PLAYER_STATE_SIT_MEDIUM_CHAIR && s != PLAYER_STATE_SIT_HIGH_CHAIR &&
        s != PLAYER_STATE_SIT && s != PLAYER_STATE_SLEEP &&
        s != PLAYER_STATE_KNEEL;
}

void Unit::SetStandState(uint8 state)
{
    SetByteValue(UNIT_FIELD_BYTES_1, 0, state);

    if(GetTypeId()==TYPEID_PLAYER)
    {
        WorldPacket data(SMSG_STANDSTATE_UPDATE, 1);
        data << (uint8)state;
        ((Player*)this)->GetSession()->SendPacket(&data);
    }
}

bool Unit::IsPolymorphed() const
{
    return GetSpellSpecific(getTransForm())==SPELL_MAGE_POLYMORPH;
}

void Unit::SetDisplayId(uint32 modelId)
{
    SetUInt32Value(UNIT_FIELD_DISPLAYID, modelId);

    if(GetTypeId() == TYPEID_UNIT && ((Creature*)this)->isPet())
    {
        Pet *pet = ((Pet*)this);
        if(!pet->isControlled())
            return;
        Unit *owner = GetOwner();
        if(owner && (owner->GetTypeId() == TYPEID_PLAYER) && ((Player*)owner)->GetGroup())
            ((Player*)owner)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_MODEL_ID);
    }
}

void Unit::ClearComboPointHolders()
{
    while(!m_ComboPointHolders.empty())
    {
        uint32 lowguid = *m_ComboPointHolders.begin();

        Player* plr = objmgr.GetPlayer(MAKE_NEW_GUID(lowguid, 0, HIGHGUID_PLAYER));
        if(plr && plr->GetComboTarget()==GetGUID())         // recheck for safe
            plr->ClearComboPoints();                        // remove also guid from m_ComboPointHolders;
        else
            m_ComboPointHolders.erase(lowguid);             // or remove manually
    }
}

void Unit::ClearAllReactives()
{

    for(int i=0; i < MAX_REACTIVE; ++i)
        m_reactiveTimer[i] = 0;

    if (HasAuraState( AURA_STATE_DEFENSE))
        ModifyAuraState(AURA_STATE_DEFENSE, false);
    if (getClass() == CLASS_HUNTER && HasAuraState( AURA_STATE_HUNTER_PARRY))
        ModifyAuraState(AURA_STATE_HUNTER_PARRY, false);
    if (HasAuraState( AURA_STATE_CRIT))
        ModifyAuraState(AURA_STATE_CRIT, false);
    if (getClass() == CLASS_HUNTER && HasAuraState( AURA_STATE_HUNTER_CRIT_STRIKE)  )
        ModifyAuraState(AURA_STATE_HUNTER_CRIT_STRIKE, false);

    if(getClass() == CLASS_WARRIOR && GetTypeId() == TYPEID_PLAYER)
        ((Player*)this)->ClearComboPoints();
}

void Unit::UpdateReactives( uint32 p_time )
{
    for(int i = 0; i < MAX_REACTIVE; ++i)
    {
        ReactiveType reactive = ReactiveType(i);

        if(!m_reactiveTimer[reactive])
            continue;

        if ( m_reactiveTimer[reactive] <= p_time)
        {
            m_reactiveTimer[reactive] = 0;

            switch ( reactive )
            {
                case REACTIVE_DEFENSE:
                    if (HasAuraState(AURA_STATE_DEFENSE))
                        ModifyAuraState(AURA_STATE_DEFENSE, false);
                    break;
                case REACTIVE_HUNTER_PARRY:
                    if ( getClass() == CLASS_HUNTER && HasAuraState(AURA_STATE_HUNTER_PARRY))
                        ModifyAuraState(AURA_STATE_HUNTER_PARRY, false);
                    break;
                case REACTIVE_CRIT:
                    if (HasAuraState(AURA_STATE_CRIT))
                        ModifyAuraState(AURA_STATE_CRIT, false);
                    break;
                case REACTIVE_HUNTER_CRIT:
                    if ( getClass() == CLASS_HUNTER && HasAuraState(AURA_STATE_HUNTER_CRIT_STRIKE) )
                        ModifyAuraState(AURA_STATE_HUNTER_CRIT_STRIKE, false);
                    break;
                case REACTIVE_OVERPOWER:
                    if(getClass() == CLASS_WARRIOR && GetTypeId() == TYPEID_PLAYER)
                        ((Player*)this)->ClearComboPoints();
                    break;
                default:
                    break;
            }
        }
        else
        {
            m_reactiveTimer[reactive] -= p_time;
        }
    }
}

Unit* Unit::SelectNearbyTarget() const
{
    CellPair p(MaNGOS::ComputeCellPair(GetPositionX(), GetPositionY()));
    Cell cell(p);
    cell.data.Part.reserved = ALL_DISTRICT;
    cell.SetNoCreate();

    std::list<Unit *> targets;

    {
        MaNGOS::AnyUnfriendlyUnitInObjectRangeCheck u_check(this, this, ATTACK_DISTANCE);
        MaNGOS::UnitListSearcher<MaNGOS::AnyUnfriendlyUnitInObjectRangeCheck> searcher(targets, u_check);

        TypeContainerVisitor<MaNGOS::UnitListSearcher<MaNGOS::AnyUnfriendlyUnitInObjectRangeCheck>, WorldTypeMapContainer > world_unit_searcher(searcher);
        TypeContainerVisitor<MaNGOS::UnitListSearcher<MaNGOS::AnyUnfriendlyUnitInObjectRangeCheck>, GridTypeMapContainer >  grid_unit_searcher(searcher);

        CellLock<GridReadGuard> cell_lock(cell, p);
        cell_lock->Visit(cell_lock, world_unit_searcher, *MapManager::Instance().GetMap(GetMapId(), this));
        cell_lock->Visit(cell_lock, grid_unit_searcher, *MapManager::Instance().GetMap(GetMapId(), this));
    }

    // remove current target
    if(getVictim())
        targets.remove(getVictim());

    // remove not LoS targets
    for(std::list<Unit *>::iterator tIter = targets.begin(); tIter != targets.end();)
    {
        if(!IsWithinLOSInMap(*tIter))
        {
            std::list<Unit *>::iterator tIter2 = tIter;
            ++tIter;
            targets.erase(tIter2);
        }
        else
            ++tIter;
    }

    // no appropriate targets
    if(targets.empty())
        return NULL;

    // select random
    uint32 rIdx = urand(0,targets.size()-1);
    std::list<Unit *>::const_iterator tIter = targets.begin();
    for(uint32 i = 0; i < rIdx; ++i)
        ++tIter;

    return *tIter;
}

void Unit::ApplyAttackTimePercentMod( WeaponAttackType att,float val, bool apply )
{
    if(val > 0)
    {
        ApplyPercentModFloatVar(m_modAttackSpeedPct[att], val, !apply);
        ApplyPercentModFloatValue(UNIT_FIELD_BASEATTACKTIME+att,val,!apply);
    }
    else
    {
        ApplyPercentModFloatVar(m_modAttackSpeedPct[att], -val, apply);
        ApplyPercentModFloatValue(UNIT_FIELD_BASEATTACKTIME+att,-val,apply);
    }
}

void Unit::ApplyCastTimePercentMod(float val, bool apply )
{
    if(val > 0)
        ApplyPercentModFloatValue(UNIT_MOD_CAST_SPEED,val,!apply);
    else
        ApplyPercentModFloatValue(UNIT_MOD_CAST_SPEED,-val,apply);
}

uint32 Unit::GetCastingTimeForBonus( SpellEntry const *spellProto, DamageEffectType damagetype, uint32 CastingTime )
{
    int32 overTime    = 0;
    uint8 effects     = 0;
    bool DirectDamage = false;
    bool AreaEffect   = false;

    for ( uint32 i=0; i<3;i++)
    {
        switch ( spellProto->Effect[i] )
        {
            case SPELL_EFFECT_SCHOOL_DAMAGE:
            case SPELL_EFFECT_MANA_DRAIN:
            case SPELL_EFFECT_HEALTH_LEECH:
            case SPELL_EFFECT_ENVIRONMENTAL_DAMAGE:
            case SPELL_EFFECT_POWER_BURN:
            case SPELL_EFFECT_HEAL:
                DirectDamage = true;
                break;
            case SPELL_EFFECT_APPLY_AURA:
                switch ( spellProto->EffectApplyAuraName[i] )
                {
                    case SPELL_AURA_PERIODIC_DAMAGE:
                    case SPELL_AURA_PERIODIC_HEAL:
                    case SPELL_AURA_PERIODIC_LEECH:
                        if ( GetSpellDuration(spellProto) )
                            overTime = GetSpellDuration(spellProto);
                        break;
                    default: 
                        // -5% per additional effect
                        ++effects;
                        break;
                }
            default: 
                break;
        }

        if(IsAreaEffectTarget(Targets(spellProto->EffectImplicitTargetA[i])) || IsAreaEffectTarget(Targets(spellProto->EffectImplicitTargetB[i])))
            AreaEffect = true;
    }

    // Combined Spells with Both Over Time and Direct Damage
    if ( overTime > 0 && CastingTime > 0 && DirectDamage )
    {
        // mainly for DoTs which are 3500 here otherwise
        uint32 OriginalCastTime = GetSpellCastTime(sCastTimesStore.LookupEntry(spellProto->CastingTimeIndex));
        if (OriginalCastTime > 7000) OriginalCastTime = 7000;
        if (OriginalCastTime < 1500) OriginalCastTime = 1500;
        // Portion to Over Time
        float PtOT = (overTime / 15000.f) / ((overTime / 15000.f) + (OriginalCastTime / 3500.f));

        if ( damagetype == DOT )
            CastingTime = uint32(CastingTime * PtOT);
        else if ( PtOT < 1.0f )
            CastingTime  = uint32(CastingTime * (1 - PtOT));
        else 
            CastingTime = 0;
    }

    // Area Effect Spells receive only half of bonus
    if ( AreaEffect )
        CastingTime /= 2;

    // -5% of total per any additional effect
    for ( uint8 i=0; i<effects; ++i)
    {
        if ( CastingTime > 175 ) 
        {
            CastingTime -= 175;
        }
        else 
        {
            CastingTime = 0;
            break;
        }
    }

    return CastingTime;
}

void Unit::UpdateAuraForGroup(uint8 slot)
{
    if(GetTypeId() == TYPEID_PLAYER)
    {
        Player* player = (Player*)this;
        if(player->GetGroup())
        {
            player->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_AURAS);
            player->SetAuraUpdateMask(slot);
        }
    }
    else if(GetTypeId() == TYPEID_UNIT && ((Creature*)this)->isPet())
    {
        Pet *pet = ((Pet*)this);
        if(pet->isControlled())
        {
            Unit *owner = GetOwner();
            if(owner && (owner->GetTypeId() == TYPEID_PLAYER) && ((Player*)owner)->GetGroup())
            {
                ((Player*)owner)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_AURAS);
                pet->SetAuraUpdateMask(slot);
            }
        }
    }
}

float Unit::GetAPMultiplier(WeaponAttackType attType, bool normalized)
{
    if (!normalized || GetTypeId() != TYPEID_PLAYER)
        return float(GetAttackTime(attType))/1000.0f;

    Item *Weapon = ((Player*)this)->GetItemByPos(INVENTORY_SLOT_BAG_0,Player::GetWeaponSlotByAttack(attType));
    if (!Weapon || Weapon->GetProto()->Class != ITEM_CLASS_WEAPON)
        return 2.4;                                         // fist attack

    switch (Weapon->GetProto()->InventoryType)
    {
        case INVTYPE_2HWEAPON:
            return 3.3;
        case INVTYPE_RANGED:
        case INVTYPE_RANGEDRIGHT:
        case INVTYPE_THROWN:
            return 2.8;
        case INVTYPE_WEAPON:
        case INVTYPE_WEAPONMAINHAND:
        case INVTYPE_WEAPONOFFHAND:
        default:
            return Weapon->GetProto()->SubClass==ITEM_SUBCLASS_WEAPON_DAGGER ? 1.7 : 2.4;
    }
}

Aura* Unit::GetDummyAura( uint32 spell_id ) const
{
    Unit::AuraList const& mDummy = GetAurasByType(SPELL_AURA_DUMMY);
    for(Unit::AuraList::const_iterator itr = mDummy.begin(); itr != mDummy.end(); ++itr)
        if ((*itr)->GetId() == spell_id)
            return *itr;

    return NULL;
}

bool Unit::IsUnderLastManaUseEffect() const
{
    return  getMSTimeDiff(m_lastManaUse,getMSTime()) < 5000;
}
