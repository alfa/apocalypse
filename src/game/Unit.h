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

#ifndef __UNIT_H
#define __UNIT_H

#include "Common.h"
#include "Object.h"
#include "ObjectAccessor.h"
#include "Opcodes.h"
#include "Mthread.h"
#include "SpellAuraDefines.h"

#include <list>

#define CREATURE_MAX_SPELLS     4
#define PLAYER_MAX_SKILLS       127
#define PLAYER_SKILL(x)         (PLAYER_SKILL_INFO_START + ((x)*3))
// DWORD definitions gathered from windows api
#define SKILL_VALUE(x)          uint16(x)
#define SKILL_MAX(x)            uint16((uint32(x) >> 16))
#define MAKE_SKILL_VALUE(v, m) ((uint32)(((uint16)(v)) | (((uint32)((uint16)(m))) << 16)))

#define NOSWING                     0
#define SINGLEHANDEDSWING           1
#define TWOHANDEDSWING              2

#define VICTIMSTATE_UNKNOWN1        0
#define VICTIMSTATE_NORMAL          1
#define VICTIMSTATE_DODGE           2
#define VICTIMSTATE_PARRY           3
#define VICTIMSTATE_UNKNOWN2        4
#define VICTIMSTATE_BLOCKS          5
#define VICTIMSTATE_EVADES          6
#define VICTIMSTATE_IS_IMMUNE       7
#define VICTIMSTATE_DEFLECTS        8

#define HITINFO_NORMALSWING         0x00
#define HITINFO_NORMALSWING2        0x02
#define HITINFO_LEFTSWING           0x04
#define HITINFO_MISS                0x10
#define HITINFO_HITSTRANGESOUND1    0x20                    //maybe linked to critical hit
#define HITINFO_HITSTRANGESOUND2    0x40                    //maybe linked to critical hit
#define HITINFO_CRITICALHIT         0x80
#define HITINFO_GLANCING            0x4000
#define HITINFO_CRUSHING            0x8000
#define HITINFO_NOACTION            0x10000
#define HITINFO_SWINGNOHITSOUND     0x80000

#define NULL_BAG                    0
#define NULL_SLOT                   255

struct FactionTemplateEntry;
struct Modifier;
struct SpellEntry;

class Aura;
class Creature;
class Spell;
class DynamicObject;

struct DamageShield
{
    uint32 m_spellId;
    uint32 m_damage;
    Unit *m_caster;
};

struct ProcTriggerDamage
{
    uint64 caster;
    uint32 procDamage;
    uint32 procChance;
    uint32 procFlags;
    uint32 procCharges;
};

struct ProcTriggerSpell
{
    uint32 trigger;
    uint32 spellId;
    uint64 caster;
    uint32 procChance;
    uint32 procFlags;
    uint32 procCharges;
};

/*struct SpellCritSchool
{
    uint32 spellId;
    int32 school;
    int32 chance;
};*/

struct ReflectSpellSchool
{
    uint32 spellId;
    int32 school;
    int32 chance;
};

/*struct DamageDoneCreature
{
    uint32 spellId;
    uint32 creaturetype;
    int32 damage;
};

struct CreatureAttackPower
{
    uint32 spellId;
    uint32 creaturetype;
    int32 damage;
};

struct DamageDone
{
    uint32 spellId;
    int32 school;
    int32 damage;
};

struct DamageTaken
{
    uint32 spellId;
    int32 school;
    int32 damage;
};

struct PowerCostSchool
{
    uint32 spellId;
    int32 school;
    int32 damage;
};*/

struct SpellImmune
{
    uint32 type;
    uint32 spellId;
};

typedef std::list<SpellImmune*> SpellImmuneList;

enum DeathState
{
    ALIVE = 0,
    JUST_DIED,
    CORPSE,
    DEAD
};

enum UnitState
{
    UNIT_STAT_STOPPED       = 0,
    UNIT_STAT_DIED          = 1,
    UNIT_STAT_ATTACKING     = 2,                            // player is attacking someone
    UNIT_STAT_ATTACK_BY     = 4,                            // player is attack by someone
                                                            // player is in combat mode
    UNIT_STAT_IN_COMBAT     = (UNIT_STAT_ATTACKING | UNIT_STAT_ATTACK_BY),
    UNIT_STAT_STUNDED       = 8,
    UNIT_STAT_ROAMING       = 16,
    UNIT_STAT_CHASE         = 32,
    UNIT_STAT_SEARCHING     = 64,
    UNIT_STAT_FLEEING       = 128,
    UNIT_STAT_MOVING        = (UNIT_STAT_ROAMING | UNIT_STAT_CHASE | UNIT_STAT_SEARCHING | UNIT_STAT_FLEEING),
    UNIT_STAT_IN_FLIGHT     = 256,                          // player is i n flight mode
    UNIT_STAT_FOLLOW        = 512,
    UNIT_STAT_ROOT          = 1024,
    UNIT_STAT_CONFUSED      = 2048,
    UNIT_STAT_ALL_STATE     = 0xffff                        //(UNIT_STAT_STOPPED | UNIT_STAT_MOVING | UNIT_STAT_IN_COMBAT | UNIT_STAT_IN_FLIGHT)
};

enum WeaponAttackType
{
    BASE_ATTACK   = 0,
    OFF_ATTACK    = 1,
    RANGED_ATTACK = 2
};

// Value masks for UNIT_FIELD_FLAGS
enum UnitFlags
{
    UNIT_FLAG_NONE           = 0x00000000,
    UNIT_FLAG_DISABLE_MOVE   = 0x00000004,
    UNIT_FLAG_NOT_IN_PVP     = 0x00000008,
    UNIT_FLAG_RESTING        = 0x00000020,
    UNIT_FLAG_MOUNT          = 0x00002000,
    UNIT_FLAG_DISABLE_ROTATE = 0x00040000,
    UNIT_FLAG_ATTACKING      = 0x00080000,
    UNIT_FLAG_SKINNABLE      = 0x04000000,
    UNIT_FLAG_SHEATHE        = 0x40000000
};

//To all Immune system,if target has immunes,
//some spell that related to ImmuneToDispel or ImmuneToSchool or ImmuneToDamage type can't cast to it,
//some spell_effects that related to ImmuneToEffect<effect>(only this effect in the spell) can't cast to it,
//some aura(related to ImmuneToMechanic or ImmuneToState<aura>) can't apply to it.
enum SpellImmunity
{
    IMMUNITY_EFFECT                = 0,
    IMMUNITY_STATE                 = 1,
    IMMUNITY_SCHOOL                = 2,
    IMMUNITY_DAMAGE                = 3,
    IMMUNITY_DISPEL                = 4,
    IMMUNITY_MECHANIC              = 5,

};

enum ImmuneToMechanic
{
    IMMUNE_MECHANIC_CHARM            =1,
    IMMUNE_MECHANIC_CONFUSED         =2,
    IMMUNE_MECHANIC_DISARM           =3,
    IMMUNE_MECHANIC_UNKOWN4          =4,
    IMMUNE_MECHANIC_FEAR             =5,
    IMMUNE_MECHANIC_UNKOWN6          =6,
    IMMUNE_MECHANIC_ROOT             =7,
    IMMUNE_MECHANIC_UNKOWN8          =8,
    IMMUNE_MECHANIC_UNKOWN9          =9,
    IMMUNE_MECHANIC_SLEEP            =10,
    IMMUNE_MECHANIC_CHASE            =11,
    IMMUNE_MECHANIC_STUNDED          =12,
    IMMUNE_MECHANIC_FREEZE           =13,
    IMMUNE_MECHANIC_KNOCKOUT         =14,
    IMMUNE_MECHANIC_BLEED            =15,
    IMMUNE_MECHANIC_HEAL             =16,
    IMMUNE_MECHANIC_POLYMORPH        =17,
    IMMUNE_MECHANIC_BANISH           =18,
    IMMUNE_MECHANIC_SHIELDED         =19,
    IMMUNE_MECHANIC_UNKOWN20         =20,
    IMMUNE_MECHANIC_MOUNT            =21,
    IMMUNE_MECHANIC_UNKOWN22         =22,
    IMMUNE_MECHANIC_UNKOWN23         =23,
    IMMUNE_MECHANIC_HORROR           =24,
    IMMUNE_MECHANIC_INVULNERABILITY  =25,
    IMMUNE_MECHANIC_UNKOWN26         =26
};

enum ImmuneToDispel
{
    IMMUNE_DISPEL_MAGIC        =1,
    IMMUNE_DISPEL_CURSE        =2,
    IMMUNE_DISPEL_DISEASE      =3,
    IMMUNE_DISPEL_POISON       =4,
    IMMUNE_DISPEL_STEALTH      =5,
    IMMUNE_DISPEL_INVISIBILITY =6,
    IMMUNE_DISPEL_ALL          =7,
    IMMUNE_DISPEL_SPE_NPC_ONLY =8,
    IMMUNE_DISPEL_CRAZY        =9,
    IMMUNE_DISPEL_ZG_TICKET    =10
};

enum ImmuneToDamage
{
    IMMUNE_DAMAGE_PHYSICAL     =1,
    IMMUNE_DAMAGE_MAGIC        =126
};

enum ImmuneToSchool
{
    IMMUNE_SCHOOL_PHYSICAL     =1,
    IMMUNE_SCHOOL_HOLY         =2,
    IMMUNE_SCHOOL_FIRE         =4,
    IMMUNE_SCHOOL_NATURE       =8,
    IMMUNE_SCHOOL_FROST        =16,
    IMMUNE_SCHOOL_SHADOW       =32,
    IMMUNE_SCHOOL_ARCANE       =64,
    IMMUNE_SCHOOL_MAGIC        =126
};

inline SpellSchools immuneToSchool(ImmuneToSchool immune)
{
    switch(immune)
    {
        case IMMUNE_SCHOOL_PHYSICAL: return SPELL_SCHOOL_NORMAL;
        case IMMUNE_SCHOOL_HOLY:     return SPELL_SCHOOL_HOLY;
        case IMMUNE_SCHOOL_FIRE:     return SPELL_SCHOOL_FIRE;
        case IMMUNE_SCHOOL_NATURE:   return SPELL_SCHOOL_NATURE;
        case IMMUNE_SCHOOL_FROST:    return SPELL_SCHOOL_FROST;
        case IMMUNE_SCHOOL_SHADOW:   return SPELL_SCHOOL_SHADOW;
        case IMMUNE_SCHOOL_ARCANE:   return SPELL_SCHOOL_ARCANE;
        case IMMUNE_SCHOOL_MAGIC:
        default:
            break;
    }
    assert(false);
    return SPELL_SCHOOL_NORMAL;
}

struct Hostil
{
    Hostil(uint64 _UnitGuid, float _Hostility) : UnitGuid(_UnitGuid), Hostility(_Hostility) {}

    uint64 UnitGuid;
    float Hostility;
    bool operator <(Hostil item)
    {
        if(Hostility < item.Hostility)
            return true;
        else
            return false;
    };
};

typedef std::list<Hostil> HostilList;

enum MeleeHitOutcome
{
    MELEE_HIT_MISS, MELEE_HIT_DODGE, MELEE_HIT_BLOCK, MELEE_HIT_PARRY, MELEE_HIT_GLANCING,
    MELEE_HIT_CRIT, MELEE_HIT_CRUSHING, MELEE_HIT_NORMAL
};

class MANGOS_DLL_SPEC Unit : public Object
{
    public:
        typedef std::set<Unit*> AttackerSet;
        typedef std::pair<uint32, uint32> spellEffectPair;
        typedef std::map< spellEffectPair, Aura*> AuraMap;
        typedef std::list<Aura *> AuraList;
        virtual ~Unit ( );

        virtual void Update( uint32 time );

        void setAttackTimer(WeaponAttackType type, uint32 time) { m_attackTimer[type] = time; }
        void resetAttackTimer(WeaponAttackType type = BASE_ATTACK);
        uint32 getAttackTimer(WeaponAttackType type) const { return m_attackTimer[type]; }
        bool isAttackReady(WeaponAttackType type = BASE_ATTACK) const { return m_attackTimer[type] == 0; }
        bool haveOffhandWeapon() const;
        bool canReachWithAttack(Unit *pVictim) const;

        void _addAttacker(Unit *pAttacker)                  // must be called only from Unit::Attack(Unit*)
        {
            AttackerSet::iterator itr = m_attackers.find(pAttacker);
            if(itr == m_attackers.end())
                m_attackers.insert(pAttacker);
            addUnitState(UNIT_STAT_ATTACK_BY);
        }
        void _removeAttacker(Unit *pAttacker)               // must be called only from Unit::AttackStop()
        {
            AttackerSet::iterator itr = m_attackers.find(pAttacker);
            if(itr != m_attackers.end())
                m_attackers.erase(itr);

            if (m_attackers.size() == 0)
                clearUnitState(UNIT_STAT_ATTACK_BY);
        }
        Unit * getAttackerForHelper()                       // If someone wants to help, who to give them
        {
            if (getVictim() != NULL)
                return getVictim();

            if (m_attackers.size() > 0)
                return *(m_attackers.begin());

            return NULL;
        }
        bool Attack(Unit *victim);
        bool AttackStop();
        void RemoveAllAttackers();
        bool isInCombatWithPlayer() const;
        Unit* getVictim() const { return m_attacking; }

        void addUnitState(uint32 f) { m_state |= f; };
        bool hasUnitState(const uint32 f) const { return (m_state & f); }
        void clearUnitState(uint32 f) { m_state &= ~f; };

        uint32 getLevel() const { return GetUInt32Value(UNIT_FIELD_LEVEL); };
        void SetLevel(uint32 lvl) { SetUInt32Value(UNIT_FIELD_LEVEL,lvl); }
        uint8 getRace() const { return (uint8)m_uint32Values[ UNIT_FIELD_BYTES_0 ] & 0xFF; };
        uint32 getRaceMask() const { return 1 << (getRace()-1); };
        uint8 getClass() const { return (uint8)(m_uint32Values[ UNIT_FIELD_BYTES_0 ] >> 8) & 0xFF; };
        uint32 getClassMask() const { return 1 << (getClass()-1); };
        uint8 getGender() const { return (uint8)(m_uint32Values[ UNIT_FIELD_BYTES_0 ] >> 16) & 0xFF; };

        uint32 GetStat(Stats stat) const { return (uint32)GetFloatValue(UNIT_FIELD_STATS+stat); }
        void SetStat(Stats stat, uint32 val) { SetFloatValue(UNIT_FIELD_STATS+stat, val); }
        void ApplyStatMod(Stats stat, uint32 val, bool apply) { ApplyModFloatValue(UNIT_FIELD_STATS+stat, val, apply); }
        void ApplyStatPercentMod(Stats stat, float val, bool apply) { ApplyPercentModFloatValue(UNIT_FIELD_STATS+stat, val, apply); }

        uint32 GetArmor() const { return GetResistance(SPELL_SCHOOL_NORMAL) ; }
        void SetArmor(uint32 val) { SetResistance(SPELL_SCHOOL_NORMAL, val); }
        void ApplyArmorMod(uint32 val, bool apply) { ApplyResistanceMod(SPELL_SCHOOL_NORMAL, val, apply); }
        void ApplyArmorPercentMod(float val, bool apply) { ApplyResistancePercentMod(SPELL_SCHOOL_NORMAL, val, apply); }

        uint32 GetResistance(SpellSchools school) const { return (uint32)GetFloatValue(UNIT_FIELD_RESISTANCES+school); }
        void SetResistance(SpellSchools school, uint32 val) { SetFloatValue(UNIT_FIELD_RESISTANCES+school,val); }
        void ApplyResistanceMod(SpellSchools school, uint32 val, bool apply) { ApplyModFloatValue(UNIT_FIELD_RESISTANCES+school, val, apply); }
        void ApplyResistancePercentMod(SpellSchools school, float val, bool apply) { ApplyPercentModFloatValue(UNIT_FIELD_RESISTANCES+school, val, apply); }

        uint32 GetHealth()    const { return GetUInt32Value(UNIT_FIELD_HEALTH); }
        uint32 GetMaxHealth() const { return (uint32)GetFloatValue(UNIT_FIELD_MAXHEALTH); }
        void SetHealth(   uint32 val) { SetUInt32Value(UNIT_FIELD_HEALTH,val); }
        void SetMaxHealth(uint32 val) { SetFloatValue(UNIT_FIELD_MAXHEALTH,val); }
        void ApplyMaxHealthMod(uint32 val, bool apply) { ApplyModFloatValue(UNIT_FIELD_MAXHEALTH, val, apply); }
        void ApplyMaxHealthPercentMod(float val, bool apply) { ApplyPercentModFloatValue(UNIT_FIELD_MAXHEALTH, val, apply); }

        Powers getPowerType() const { return Powers((m_uint32Values[ UNIT_FIELD_BYTES_0 ] >> 24) & 0xFF); };
        void setPowerType(Powers power);
        uint32 GetPower(   Powers power) const { return (uint32)GetFloatValue(UNIT_FIELD_POWER1   +power); }
        uint32 GetMaxPower(Powers power) const { return (uint32)GetFloatValue(UNIT_FIELD_MAXPOWER1+power); }
        void SetPower(   Powers power, uint32 val) { SetFloatValue(UNIT_FIELD_POWER1   +power,val); }
        void SetMaxPower(Powers power, uint32 val) { SetFloatValue(UNIT_FIELD_MAXPOWER1+power,val); }
        void ApplyPowerMod(Powers power, uint32 val, bool apply) { ApplyModFloatValue(UNIT_FIELD_POWER1+power, val, apply); }
        void ApplyPowerPercentMod(Powers power, float val, bool apply) { ApplyPercentModFloatValue(UNIT_FIELD_POWER1+power, val, apply); }
        void ApplyMaxPowerMod(Powers power, uint32 val, bool apply) { ApplyModFloatValue(UNIT_FIELD_MAXPOWER1+power, val, apply); }
        void ApplyMaxPowerPercentMod(Powers power, float val, bool apply) { ApplyPercentModFloatValue(UNIT_FIELD_MAXPOWER1+power, val, apply); }

        uint32 GetAttackTime(WeaponAttackType att) const { return (uint32)GetFloatValue(UNIT_FIELD_BASEATTACKTIME+att); }
        void SetAttackTime(WeaponAttackType att, uint32 val) { SetFloatValue(UNIT_FIELD_BASEATTACKTIME+att,val); }
        void ApplyAttackTimePercentMod(WeaponAttackType att,float val, bool apply) { ApplyPercentModFloatValue(UNIT_FIELD_BASEATTACKTIME+att, val, apply); }

        // fuction template id
        uint32 getFaction() const { return GetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE); }
        void setFaction(uint32 faction) { SetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE, faction ); }
        FactionTemplateEntry* getFactionTemplateEntry() const;

        uint8 getStandState() const { return (uint8)m_uint32Values[ UNIT_FIELD_BYTES_1 ] & 0xFF; };

        void DealDamage(Unit *pVictim, uint32 damage, uint32 procFlag, bool durabilityLoss);
        void DoAttackDamage(Unit *pVictim, uint32 *damage, uint32 *blocked_amount, uint32 *damageType, uint32 *hitInfo, uint32 *victimState,uint32 *absorbDamage,uint32 *resist);
        uint32 CalDamageAbsorb(Unit *pVictim,uint32 School,const uint32 damage,uint32 *resist);
        void HandleEmoteCommand(uint32 anim_id);
        void AttackerStateUpdate (Unit *pVictim);

        float GetUnitDodgeChance()    const;
        float GetUnitParryChance()    const;
        float GetUnitBlockChance()    const;
        float GetUnitCriticalChance() const { return GetTypeId() == TYPEID_PLAYER ? GetFloatValue( PLAYER_CRIT_PERCENTAGE ) : 5; }

        virtual uint32 GetBlockValue() const =0;
        uint32 GetUnitMeleeSkill() const { return getLevel() * 5; }
        uint16 GetDefenceSkillValue() const;
        uint16 GetWeaponSkillValue() const;
        MeleeHitOutcome RollMeleeOutcomeAgainst (const Unit *pVictim) const;

        bool isVendor()       const { return HasFlag( UNIT_NPC_FLAGS, UNIT_NPC_FLAG_VENDOR ); }
        bool isTrainer()      const { return HasFlag( UNIT_NPC_FLAGS, UNIT_NPC_FLAG_TRAINER ); }
        bool isQuestGiver()   const { return HasFlag( UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER ); }
        bool isGossip()       const { return HasFlag( UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP ); }
        bool isTaxi()         const { return HasFlag( UNIT_NPC_FLAGS, UNIT_NPC_FLAG_TAXIVENDOR ); }
        bool isGuildMaster()  const { return HasFlag( UNIT_NPC_FLAGS, UNIT_NPC_FLAG_PETITIONER ); }
        bool isBattleMaster() const { return HasFlag( UNIT_NPC_FLAGS, UNIT_NPC_FLAG_BATTLEFIELDPERSON ); }
        bool isBanker()       const { return HasFlag( UNIT_NPC_FLAGS, UNIT_NPC_FLAG_BANKER ); }
        bool isInnkeeper()    const { return HasFlag( UNIT_NPC_FLAGS, UNIT_NPC_FLAG_INNKEEPER ); }
        bool isSpiritHealer() const { return HasFlag( UNIT_NPC_FLAGS, UNIT_NPC_FLAG_SPIRITHEALER ); }
        bool isTabardVendor() const { return HasFlag( UNIT_NPC_FLAGS, UNIT_NPC_FLAG_TABARDVENDOR ); }
        bool isAuctioner()    const { return HasFlag( UNIT_NPC_FLAGS, UNIT_NPC_FLAG_AUCTIONEER ); }
        bool isArmorer()      const { return HasFlag( UNIT_NPC_FLAGS, UNIT_NPC_FLAG_ARMORER ); }
        //Need fix or use this
        bool isGuard() const  { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GUARD); }

        bool isInFlight()  const { return hasUnitState(UNIT_STAT_IN_FLIGHT); }
        bool isInCombat()  const { return hasUnitState(UNIT_STAT_IN_COMBAT); }
        bool isAttacking() const { return hasUnitState(UNIT_STAT_ATTACKING); }
        bool isAttacked()  const { return hasUnitState(UNIT_STAT_ATTACK_BY); }

        bool HasAuraType(uint32 auraType) const;
        bool isStealth() const                              // cache this in a bool someday
        {
            return HasAuraType(SPELL_AURA_MOD_STEALTH);
        }
        bool isTargetableForAttack() const { return isAlive() && !isInFlight() /*&& !isStealth()*/; }

        void SendHealSpellOnPlayer(Unit *pVictim, uint32 SpellID, uint32 Damage);
        void SendHealSpellOnPlayerPet(Unit *pVictim, uint32 SpellID, uint32 Damage);
        void PeriodicAuraLog(Unit *pVictim, SpellEntry *spellProto, Modifier *mod);
        void SpellNonMeleeDamageLog(Unit *pVictim, uint32 spellID, uint32 damage);
        void CastSpell(Unit* Victim, uint32 spellId, bool triggered);

        void DeMorph();

        void SendAttackStop(uint64 victimGuid);
        void SendAttackStateUpdate(uint32 HitInfo, uint64 targetGUID, uint8 SwingType, uint32 DamageType, uint32 Damage, uint32 AbsorbDamage, uint32 Resist, uint32 TargetState, uint32 BlockedAmount);
        void SendSpellNonMeleeDamageLog(uint64 targetGUID,uint32 SpellID,uint32 Damage, uint8 DamageType,uint32 AbsorbedDamage, uint32 Resist,bool PhysicalDamage, uint32 Blocked);

        void SendMonsterMove(float NewPosX, float NewPosY, float NewPosZ, bool Walkback, bool Run, uint32 Time);

        virtual void DealWithSpellDamage(DynamicObject &);
        virtual void MoveOutOfRange(Player &) {  };

        bool isAlive() const { return (m_deathState == ALIVE); };
        bool isDead() const { return ( m_deathState == DEAD || m_deathState == CORPSE ); };
        DeathState getDeathState() { return m_deathState; };
        virtual void setDeathState(DeathState s)
        {
            if (s != ALIVE)
            {
                if (isInCombat())
                {
                    AttackStop();
                    RemoveAllAttackers();
                }
            }
            if (s == JUST_DIED)
            {
                RemoveAllAurasOnDeath();
            }
            if (m_deathState != ALIVE && s == ALIVE)
            {
                _ApplyAllAuraMods();
            }
            m_deathState = s;
        }

        uint64 GetPetGUID() const { return  GetUInt64Value(UNIT_FIELD_SUMMON); }
        uint64 GetCharmGUID() const { return  GetUInt64Value(UNIT_FIELD_CHARM); }
        Creature* GetPet() const;
        Creature* GetCharm() const;
        void SetPet(Creature* pet);
        void SetCharm(Creature* pet);

        bool AddAura(Aura *aur, bool uniq = false);

        void RemoveFirstAuraByDispel(uint32 dispel_type);
        void RemoveAura(AuraMap::iterator &i, bool onDeath = false);
        void RemoveAura(uint32 spellId, uint32 effindex);
        void RemoveAurasDueToSpell(uint32 spellId);
        void RemoveSpellsCausingAura(uint32 auraType);
        void RemoveRankAurasDueToSpell(uint32 spellId);
        bool RemoveNoStackAurasDueToAura(Aura *Aur);

        void RemoveAllAuras();
        void RemoveAllAurasOnDeath();
        //void SetAura(Aura* Aur){ m_Auras = Aur; }
        bool SetAurDuration(uint32 spellId, uint32 effindex, uint32 duration);
        uint32 GetAurDuration(uint32 spellId, uint32 effindex);

        void castSpell(Spell * pSpell);
        void InterruptSpell();
        Spell * m_currentSpell;
        Spell * m_currentMeleeSpell;
        uint32 m_addDmgOnce;
        uint64 m_TotemSlot1;
        uint64 m_TotemSlot2;
        uint64 m_TotemSlot3;
        uint64 m_TotemSlot4;
        uint32 m_triggerSpell;
        uint32 m_triggerDamage;
        uint32 m_canMove;
        uint32 m_detectStealth;
        uint32 m_stealthvalue;
        float m_speed;
        uint32 m_ShapeShiftForm;
        uint32 m_form;
        int32 m_modDamagePCT;
        int32 m_RegenPCT;
        int32 m_modHitChance;
        int32 m_modSpellHitChance;
        int32 m_baseSpellCritChance;
        int32 m_modCastSpeedPct;

        bool isInFront(Unit const* target,float distance);
        void SetInFront(Unit const* target);

        bool m_silenced;
        bool waterbreath;
        std::list<struct DamageShield> m_damageShields;
        std::list<struct DamageManaShield*> m_damageManaShield;
        //std::list<struct SpellCritSchool*> m_spellCritSchool;
        std::list<Aura *> *GetSingleCastAuras() { return &m_scAuras; }
        std::list<struct ReflectSpellSchool*> m_reflectSpellSchool;
        SpellImmuneList m_spellImmune[6];
        /*std::list<struct DamageDoneCreature*> m_damageDoneCreature;
        std::list<struct DamageDone*> m_damageDone;
        std::list<struct DamageTaken*> m_damageTaken;
        std::list<struct PowerCostSchool*> m_powerCostSchool;
        std::list<struct CreatureAttackPower*> m_creatureAttackPower;*/

        float GetHostility(uint64 guid) const;
        float GetHostilityDistance(uint64 guid) const { return GetHostility( guid )/(3.5f * getLevel()+1.0f); }
        HostilList& GetHostilList() { return m_hostilList; }
        void AddHostil(uint64 guid, float hostility);
        Unit* SelectHostilTarget();

        Aura* GetAura(uint32 spellId, uint32 effindex);
        AuraMap& GetAuras( ) {return m_Auras;}
        AuraList& GetAurasByType(uint8 type) {return m_modAuras[type];}
        long GetTotalAuraModifier(uint32 ModifierID);
        void SendMoveToPacket(float x, float y, float z, bool run);
        void AddItemEnchant(uint32 enchant_id,bool apply);
        void setTransForm(uint32 spellid) { m_transform = spellid;}
        uint32 getTransForm() { return m_transform;}
        void AddDynObject(DynamicObject* dynObj);
        void RemoveDynObject(uint32 spellid);
        void AddGameObject(GameObject* gameObj);
        void RemoveGameObject(uint32 spellid, bool del);
        uint32 CalculateDamage(bool ranged);
        void SetStateFlag(uint32 index, uint32 newFlag );
        void RemoveStateFlag(uint32 index, uint32 oldFlag );
        void ApplyStats(bool apply);
        void UnsummonTotem(int8 slot = -1);
        uint32 SpellDamageBonus(Unit *pVictim, SpellEntry *spellProto, uint32 damage);
        uint32 MeleeDamageBonus(Unit *pVictim, uint32 damage);
        void ApplySpellImmune(uint32 spellId, uint32 op, uint32 type, bool apply);

    protected:
        Unit ( );

        void _RemoveStatsMods();
        void _ApplyStatsMods();

        void _RemoveAllAuraMods();
        void _ApplyAllAuraMods();

        void _UpdateSpells(uint32 time);
        void _UpdateHostil( uint32 time );
        //void _UpdateAura();

        //Aura* m_aura;
        //uint32 m_auraCheck, m_removeAuraTimer;

        uint32 m_attackTimer[3];

        AttackerSet m_attackers;
        Unit* m_attacking;

        DeathState m_deathState;

        AuraMap m_Auras;

        std::list<Aura *> m_scAuras;                        // casted singlecast auras
        std::list<DynamicObject*> m_dynObj;
        std::list<GameObject*> m_gameObj;
        HostilList m_hostilList;
        uint32 m_transform;
        uint32 m_removedAuras;

        AuraList m_modAuras[TOTAL_AURAS];
        long m_AuraModifiers[TOTAL_AURAS];
        //std::list< spellEffectPair > AuraSpells[TOTAL_AURAS];  // TODO: use this if ok for mem

    private:
        uint32 m_state;                                     // Even derived shouldn't modify
};
#endif
