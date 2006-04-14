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

#ifndef __SPELL_H
#define __SPELL_H

#include "GridDefines.h"
#include "Database/DBCStores.h"

class WorldSession;
class Unit;
class DynamicObj;
class Player;
class Item;
class GameObject;
class Group;
class Aura;

//SKILL_ID
#define SKILL_TAILORING         197
#define SKILL_ALCHEMY           171
#define SKILL_LEATHERWORKING    165
#define SKILL_ENGINERING        202
#define SKILL_FIRST_AID         129
#define SKILL_MINING            186
#define SKILL_COOKING           185
#define SKILL_POISONS           40
#define SKILL_SKINNING          393
#define SKILL_ENCHANTING        333
#define SKILL_BLACKSMITHING     164
#define SKILL_HERBALISM         182
#define SKILL_FISHING           356
#define SKILL_OPENLOCK			242

enum SpellCastTargetFlags
{
    TARGET_FLAG_SELF             = 0x0000,
    TARGET_FLAG_UNIT             = 0x0002,
    TARGET_FLAG_OBJECT           = 0x0800,
    TARGET_FLAG_ITEM             = 0x1010,
    TARGET_FLAG_SOURCE_LOCATION  = 0x0020,
    TARGET_FLAG_DEST_LOCATION    = 0x0040,
    TARGET_FLAG_STRING           = 0x2000
};

enum Targets
{
    TARGET_SELF         = 1,
    TARGET_PET          = 5,
    TARGET_S_E          = 6,
    TARGET_AE_E         = 15,
    TARGET_AE_E_INSTANT = 16,
    TARGET_AC_P         = 20,
    TARGET_S_F              = 21,
    TARGET_AC_E             = 22,
    TARGET_S_GO             = 23,
    TARGET_INFRONT      = 24,
    TARGET_DUELVSPLAYER = 25,
    TARGET_GOITEM           = 26,
    TARGET_AE_E_CHANNEL = 28,
    TARGET_MINION           = 32,
    TARGET_S_P              = 35,
    TARGET_SELF_FISHING = 39,
    TARGET_TOTEM_EARTH  = 41,
    TARGET_TOTEM_WATER  = 42,
    TARGET_TOTEM_AIR      = 43,
    TARGET_TOTEM_FIRE     = 44,
    TARGET_CHAIN            = 45,
    TARGET_DY_OBJ       = 47,                               //DynamicObject
    TARGET_AE_SELECTED  = 53,
    TARGET_S_F_2        = 57,                               //single friend
};

enum SpellCastFlags
{
    CAST_FLAG_UNKNOWN1           = 0x2,
    CAST_FLAG_UNKNOWN2           = 0x10,
    CAST_FLAG_AMMO               = 0x20
};

enum SpellNotifyPushType
{
    PUSH_IN_FRONT   = 0,
    PUSH_SELF_CENTER  = 1,
    PUSH_DEST_CENTER  = 2
};
struct TeleportCoords
{
    uint32 id;
    uint32 mapId;
    float x;
    float y;
    float z;
};

namespace MaNGOS
{
    struct SpellNotifierPlayer;
    struct SpellNotifierCreatureAndPlayer;
}

class SpellCastTargets
{

    public:
        SpellCastTargets();
        ~SpellCastTargets();

        void read ( WorldPacket * data,Unit *caster );
        void write ( WorldPacket * data);

        SpellCastTargets& operator=(const SpellCastTargets &target)
        {
            m_unitTarget = target.m_unitTarget;
            m_itemTarget = target.m_itemTarget;
            m_GOTarget   = target.m_GOTarget;

            m_srcX = target.m_srcX;
            m_srcY = target.m_srcY;
            m_srcZ = target.m_srcZ;

            m_destX = target.m_destX;
            m_destY = target.m_destY;
            m_destZ = target.m_destZ;

            m_strTarget = target.m_strTarget;

            m_targetMask = target.m_targetMask;

            return *this;
        }

        Unit *m_unitTarget;
        Item *m_itemTarget;
        GameObject *m_GOTarget;
        float m_srcX, m_srcY, m_srcZ;
        float m_destX, m_destY, m_destZ;
        std::string m_strTarget;

        uint16 m_targetMask;
};

enum SpellState
{
    SPELL_STATE_NULL      = 0,
    SPELL_STATE_PREPARING = 1,
    SPELL_STATE_CASTING   = 2,
    SPELL_STATE_FINISHED  = 3,
    SPELL_STATE_IDLE      = 4
};

enum ShapeshiftForm
{
    FORM_CAT              = 1,
    FORM_TREE             = 2,
    FORM_TRAVEL           = 3,
    FORM_AQUA             = 4,
    FORM_BEAR             = 5,
    FORM_AMBIENT          = 6,
    FORM_GHOUL            = 7,
    FORM_DIREBEAR         = 8,
    FORM_CREATUREBEAR     = 14,
    FORM_GHOSTWOLF        = 16,
    FORM_BATTLESTANCE     = 17,
    FORM_DEFENSIVESTANCE  = 18,
    FORM_BERSERKERSTANCE  = 19,
    FORM_SHADOW           = 28,
    FORM_STEALTH          = 30
};

enum SpellFailedReason
{
    CAST_FAIL_ALREADY_FULL_HEALTH = 1,
    CAST_FAIL_ALREADY_FULL_MANA = 2,
    //CAST_FAIL_ALREADY_FULL_RAGE = 2,
    CAST_FAIL_CREATURE_ALREADY_TAMING = 3,
    CAST_FAIL_ALREADY_HAVE_CHARMED = 4,
    CAST_FAIL_ALREADY_HAVE_SUMMON = 5,
    CAST_FAIL_ALREADY_OPEN = 6,
    CAST_FAIL_MORE_POWERFUL_SPELL_ACTIVE = 7,
    CAST_FAIL_FAILED = 8,
    CAST_FAIL_NO_TARGET = 9,
    CAST_FAIL_INVALID_TARGET = 10,
    CAST_FAIL_CANT_BE_CHARMED = 11,
    CAST_FAIL_CANT_BE_DISENCHANTED = 12,
    CAST_FAIL_TARGET_IS_TAPPED = 13,
    CAST_FAIL_CANT_START_DUEL_INVISIBLE = 14,
    CAST_FAIL_CANT_START_DUEL_STEALTHED = 15,
    CAST_FAIL_TOO_CLOSE_TO_ENEMY = 16,
    CAST_FAIL_CANT_DO_THAT_YET = 17,
    CAST_FAIL_YOU_ARE_DEAD = 18,
    CAST_FAIL_OBJECT_ALREADY_BEING_USED = 19,
    CAST_FAIL_CANT_DO_WHILE_CONFUSED = 20,
    CAST_FAIL_MUST_HAVE_ITEM_EQUIPPED = 22,
    CAST_FAIL_MUST_HAVE_XXXX_EQUIPPED = 23,
    CAST_FAIL_MUST_HAVE_XXXX_IN_MAINHAND = 24,
    CAST_FAIL_INTERNAL_ERROR = 25,
    CAST_FAIL_FIZZLED = 26,
    CAST_FAIL_YOU_ARE_FLEEING = 27,
    CAST_FAIL_FOOD_TOO_LOWLEVEL_FOR_PET = 28,
    CAST_FAIL_TARGET_IS_TOO_HIGH = 29,
    CAST_FAIL_IMMUNE = 32,                                  // was 31 in pre 1.7
    CAST_FAIL_INTERRUPTED = 33,                             // was 32 in pre 1.7
    //CAST_FAIL_INTERRUPTED_COMBAT = 31,
    CAST_FAIL_ITEM_ALREADY_ENCHANTED = 34,
    CAST_FAIL_ITEM_IS_GONE = 35,
    CAST_FAIL_ENCHANT_NOT_EXISTING_ITEM = 36,
    CAST_FAIL_ITEM_NOT_READY = 37,
    CAST_FAIL_YOU_ARE_NOT_HIGH_ENOUGH = 38,
    CAST_FAIL_NOT_IN_LINE_OF_SIGHT = 39,
    CAST_FAIL_TARGET_TOO_LOW = 40,
    CAST_FAIL_SKILL_NOT_HIGH_ENOUGH = 41,
    CAST_FAIL_WEAPON_HAND_IS_EMPTY = 42,
    CAST_FAIL_CANT_DO_WHILE_MOVING = 43,
    CAST_FAIL_NEED_AMMO_IN_PAPERDOLL_SLOT = 44,
    CAST_FAIL_REQUIRES_SOMETHING = 45,
    CAST_FAIL_NEED_EXOTIC_AMMO = 46,
    CAST_FAIL_NO_PATH_AVAILABLE = 47,
    CAST_FAIL_NOT_BEHIND_TARGET = 48,
    CAST_FAIL_DIDNT_LAND_IN_FISHABLE_WATER = 49,
    CAST_FAIL_CANT_BE_CAST_HERE = 50,
    CAST_FAIL_NOT_IN_FRONT_OF_TARGET = 51,
    CAST_FAIL_NOT_IN_CONTROL_OF_ACTIONS = 52,
    CAST_FAIL_SPELL_NOT_LEARNED = 53,
    CAST_FAIL_CANT_USE_WHEN_MOUNTED = 54,
    CAST_FAIL_YOU_ARE_IN_FLIGHT = 55,
    CAST_FAIL_YOU_ARE_ON_TRANSPORT = 56,
    CAST_FAIL_SPELL_NOT_READY_YET = 57,
    CAST_FAIL_CANT_DO_IN_SHAPESHIFT = 58,
    CAST_FAIL_HAVE_TO_BE_STANDING = 59,
    CAST_FAIL_CAN_USE_ONLY_ON_OWN_OBJECT = 60,              // rogues trying "enchant" other's weapon with poison
    CAST_FAIL_CANT_ENCHANT_TRADE_ITEM = 61,
    CAST_FAIL_HAVE_TO_BE_UNSHEATHED = 62,                   // yellow text
    CAST_FAIL_CANT_CAST_AS_GHOST = 63,
    CAST_FAIL_NO_AMMO = 64,
    CAST_FAIL_NO_CHARGES_REMAIN = 65,
    CAST_FAIL_COMBO_POINTS_REQUIRED = 66,
    CAST_FAIL_NO_DUELING_HERE = 67,
    CAST_FAIL_NOT_ENOUGH_ENDURANCE = 68,
    CAST_FAIL_THERE_ARENT_ANY_FISH_HERE = 69,
    CAST_FAIL_CANT_USE_WHILE_SHAPESHIFTED = 70,
    CAST_FAIL_CANT_MOUNT_HERE = 71,
    CAST_FAIL_YOU_DO_NOT_HAVE_PET = 72,
    CAST_FAIL_NOT_ENOUGH_MANA = 73,
    CAST_FAIL_CANT_USE_WHILE_SWIMMING = 74,
    CAST_FAIL_CAN_ONLY_USE_AT_DAY = 75,
    CAST_FAIL_CAN_ONLY_USE_INDOORS = 76,
    CAST_FAIL_CAN_ONLY_USE_MOUNTED = 77,
    CAST_FAIL_CAN_ONLY_USE_AT_NIGHT = 78,
    CAST_FAIL_CAN_ONLY_USE_OUTDOORS = 79,
    //CAST_FAIL_ONLY_SHAPESHIFTED = 80,			// didn't display
    CAST_FAIL_CAN_ONLY_USE_STEALTHED  = 81,
    CAST_FAIL_CAN_ONLY_USE_WHILE_SWIMMING = 82,
    CAST_FAIL_OUT_OF_RANGE = 83,
    CAST_FAIL_CANT_USE_WHILE_PACIFIED = 84,
    CAST_FAIL_YOU_ARE_POSSESSED = 85,
    CAST_FAIL_YOU_NEED_TO_BE_IN_XXX = 87,
    CAST_FAIL_REQUIRES_XXX = 88,
    CAST_FAIL_UNABLE_TO_MOVE = 89,
    CAST_FAIL_SILENCED = 90,
    CAST_FAIL_ANOTHER_ACTION_IS_IN_PROGRESS = 91,
    CAST_FAIL_ALREADY_LEARNED_THAT_SPELL = 92,
    CAST_FAIL_SPELL_NOT_AVAILABLE_TO_YOU = 93,
    CAST_FAIL_CANT_DO_WHILE_STUNNED = 94,
    CAST_FAIL_YOUR_TARGET_IS_DEAD = 95,
    CAST_FAIL_TARGET_IS_IN_COMBAT = 96,
    CAST_FAIL_CANT_DO_THAT_YET_2 = 97,
    CAST_FAIL_TARGET_IS_DUELING = 98,
    CAST_FAIL_TARGET_IS_HOSTILE = 99,
    CAST_FAIL_TARGET_IS_TOO_ENRAGED_TO_CHARM = 100,
    CAST_FAIL_TARGET_IS_FRIENDLY = 101,
    CAST_FAIL_TARGET_CANT_BE_IN_COMBAT = 102,
    CAST_FAIL_CANT_TARGET_PLAYERS = 103,
    CAST_FAIL_TARGET_IS_ALIVE = 104,
    CAST_FAIL_TARGET_NOT_IN_YOUR_PARTY = 104,
    CAST_FAIL_CREATURE_MUST_BE_LOOTED_FIRST = 106,
    CAST_FAIL_TARGET_IS_NOT_A_PLAYER = 107,
    CAST_FAIL_NO_POCKETS_TO_PICK = 108,
    CAST_FAIL_TARGET_HAS_NO_WEAPONS_EQUIPPED = 109,
    CAST_FAIL_NOT_SKINNABLE = 110,
    CAST_FAIL_TOO_CLOSE = 112,
    CAST_FAIL_TOO_MANY_OF_THAT_ITEM_ALREADY = 113,
    CAST_FAIL_NOT_ENOUGH_TRAINING_POINTS = 115,
    CAST_FAIL_FAILED_ATTEMPT = 116,
    CAST_FAIL_TARGET_NEED_TO_BE_BEHIND = 117,
    CAST_FAIL_TARGET_NEED_TO_BE_INFRONT = 118,
    CAST_FAIL_PET_DOESNT_LIKE_THAT_FOOD = 119,
    CAST_FAIL_CANT_CAST_WHILE_FATIGUED = 120,
    CAST_FAIL_TARGET_MUST_BE_IN_THIS_INSTANCE = 121,
    CAST_FAIL_CANT_CAST_WHILE_TRADING = 122,
    CAST_FAIL_TARGET_IS_NOT_PARTY_OR_RAID = 123,
    CAST_FAIL_CANT_DISENCHANT_WHILE_LOOTING = 124,
    CAST_FAIL_TARGET_IS_IN_FFA_PVP_COMBAT = 125,
    CAST_FAIL_NO_NEARBY_CORPSES_TO_EAT = 126,
    CAST_FAIL_CAN_ONLY_USE_IN_BATTLEGROUNDS = 127,
    CAST_FAIL_TARGET_IS_NOT_A_GHOST = 128,
    CAST_FAIL_YOUR_PET_CANT_LEARN_MORE_SKILLS = 129,
    CAST_FAIL_UNKNOWN_REASON = 130,
    CAST_FAIL_NUMREASONS
};

#define SPELL_SPELL_CHANNEL_UPDATE_INTERVAL 1000

class Spell
{
    friend struct MaNGOS::SpellNotifierPlayer;
    friend struct MaNGOS::SpellNotifierCreatureAndPlayer;
    public:

        void EffectNULL(uint32 );
        void EffectSchoolDMG(uint32 i);
        void EffectTepeportUnits(uint32 i);
        void EffectApplyAura(uint32 i);
        void EffectPowerDrain(uint32 i);
        void EffectHeal(uint32 i);
        void EffectCreateItem(uint32 i);
        void EffectPresistentAA(uint32 i);
        void EffectEnergize(uint32 i);
        void EffectOpenLock(uint32 i);
        void EffectOpenSecretSafe(uint32 i);
        void EffectApplyAA(uint32 i);
        void EffectLearnSpell(uint32 i);
        void EffectDispel(uint32 i);
        void EffectSummonWild(uint32 i);
        void EffectLearnSkill(uint32 i);
        void EffectEnchantItemPerm(uint32 i);
        void EffectEnchantItemTmp(uint32 i);
        void EffectSummonPet(uint32 i);
        void EffectWeaponDmg(uint32 i);
        void EffectWeaponDmgPerc(uint32 i);
        void EffectHealMaxHealth(uint32 i);
        void EffectInterruptCast(uint32 i);
        void EffectAddComboPoints(uint32 i);
        void EffectDuel(uint32 i);
        void EffectSummonTotem(uint32 i);
        void EffectEnchantHeldItem(uint32 i);
        void EffectSummonObject(uint32 i);
        void EffectResurrect(uint32 i);
        void EffectMomentMove(uint32 i);                    //by vendy
        void EffectTransmitted(uint32 i);
        void EffectTriggerSpell(uint32 i);
        void EffectSkinning(uint32 i);
        void EffectSkill(uint32);

        Spell( Unit* Caster, SpellEntry *info, bool triggered, Aura* Aur );

        void prepare(SpellCastTargets * targets);
        void cancel();
        void update(uint32 difftime);
        void cast();
        void finish();
        void TakePower();
        void TriggerSpell();
        uint8 CanCast();
        uint8 CheckItems();
        void RemoveItems();
        uint32 CalculateDamage(uint8 i);
        void HandleTeleport(uint32 id, Unit* Target);
        void Delayed(int32 delaytime);
        void reflect(Unit *refunit);
        inline uint32 getState() { return m_spellState; }

        void writeSpellGoTargets( WorldPacket * data );
        void FillTargetMap();
        void SetTargetMap(uint32 i,uint32 cur,std::list<Unit*> &TagUnitMap,std::list<Item*> &TagItemMap,std::list<GameObject*> &TagGOMap);

        void SendCastResult(uint8 result);
        void SendSpellStart();
        void SendSpellGo();
        void SendLogExecute();
        void SendInterrupted(uint8 result);
        void SendChannelUpdate(uint32 time);
        void SendChannelStart(uint32 duration);
        void SendResurrectRequest(Player* target);

        void HandleEffects(Unit *pUnitTarget,Item *pItemTarget,GameObject *pGOTarget,uint32 i);
        void HandleAddAura(Unit* Target);

        SpellEntry * m_spellInfo;
        Item* m_CastItem;
        SpellCastTargets m_targets;

        int32 casttime;

    protected:

        Unit* m_caster;

        // Current targets, to be used in SpellEffects
        Unit* unitTarget;
        Item* itemTarget;
        GameObject* gameObjTarget;
        // -------------------------------------------

        uint32 damage;

        // List of all Spell targets
        std::list<Unit*> m_targetUnits[3];
        std::list<Item*> m_targetItems[3];
        std::list<GameObject*> m_targetGOs[3];
        // -------------------------------------------

        // List of all targets that arent repeated. (Unique)
        uint8 m_targetCount;
        std::list<Unit*> UniqueTargets;
        std::list<GameObject*> UniqueGOsTargets;
        // -------------------------------------------

        uint32 m_spellState;
        int32 m_timer;
        uint32 m_intervalTimer;
        SpellEntry * m_TriggerSpell;

        float m_castPositionX;
        float m_castPositionY;
        float m_castPositionZ;
        bool m_Istriggeredpell;
        Aura* m_triggeredByAura;
        bool m_AreaAura;

        // List of all Objects to be Deleted in spell Finish
        std::list<DynamicObject*> m_dynObjToDel;
        std::list<GameObject*> m_ObjToDel;
        // -------------------------------------------
};

enum ReplenishType
{
    REPLENISH_UNDEFINED = 0,
    REPLENISH_HEALTH = 20,
    REPLENISH_MANA = 21,
    REPLENISH_RAGE = 22
};

namespace MaNGOS
{
    struct MANGOS_DLL_DECL SpellNotifierPlayer
    {
        std::list<Unit*> &i_data;
        Spell &i_spell;
        const uint32& i_index;
        SpellNotifierPlayer(Spell &spell, std::list<Unit*> &data, const uint32 &i) : i_data(data), i_spell(spell), i_index(i) {}
        inline void Visit(PlayerMapType &m)
        {
            for(PlayerMapType::iterator itr=m.begin(); itr != m.end(); ++itr)
            {
                if( !itr->second->isAlive() )
                    continue;
                if( itr->second->GetDistance(i_spell.m_targets.m_destX, i_spell.m_targets.m_destY, i_spell.m_targets.m_destZ)
                    < GetRadius(sSpellRadius.LookupEntry(i_spell.m_spellInfo->EffectRadiusIndex[i_index])) && itr->second->GetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE) != i_spell.m_caster->GetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE) )
                    i_data.push_back(itr->second);
            }
        }
    };

    struct MANGOS_DLL_DECL SpellNotifierCreatureAndPlayer
    {
        std::list<Unit*> &i_data;
        Spell &i_spell;
        const uint32& i_index;
        const uint32& i_push_type;
        SpellNotifierCreatureAndPlayer(Spell &spell, std::list<Unit*> &data, const uint32 &i,const uint32 &type)
            : i_data(data), i_spell(spell), i_index(i), i_push_type(type){}

        template<class T> inline void Visit(std::map<OBJECT_HANDLE, T *>  &m)
        {
            for(typename std::map<OBJECT_HANDLE, T*>::iterator itr=m.begin(); itr != m.end(); ++itr)
            {

                switch(i_push_type)
                {
                    case PUSH_IN_FRONT:
                        if(i_spell.m_caster->isInFront((Unit*)(itr->second),GetRadius(sSpellRadius.LookupEntry(i_spell.m_spellInfo->EffectRadiusIndex[i_index]))) && (itr->second)->GetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE) != i_spell.m_caster->GetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE))
                            i_data.push_back(itr->second);
                        break;
                    case PUSH_SELF_CENTER:
                        if(i_spell.m_caster->GetDistance( (Unit*)(itr->second) ) < GetRadius(sSpellRadius.LookupEntry(i_spell.m_spellInfo->EffectRadiusIndex[i_index])) && itr->second->GetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE) != i_spell.m_caster->GetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE) )
                            i_data.push_back(itr->second);
                        break;
                    case PUSH_DEST_CENTER:
                        if(itr->second->GetDistance(i_spell.m_targets.m_destX, i_spell.m_targets.m_destY, i_spell.m_targets.m_destZ) < GetRadius(sSpellRadius.LookupEntry(i_spell.m_spellInfo->EffectRadiusIndex[i_index])) && itr->second->GetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE) != i_spell.m_caster->GetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE) )
                            i_data.push_back(itr->second);
                        break;
                }
            }
        }

        #ifdef WIN32
        template<> inline void Visit(std::map<OBJECT_HANDLE, Corpse *> &m ) {}
        template<> inline void Visit(std::map<OBJECT_HANDLE, GameObject *> &m ) {}
        template<> inline void Visit(std::map<OBJECT_HANDLE, DynamicObject *> &m ) {}
        #endif
    };

    #ifndef WIN32
    template<> inline void SpellNotifierCreatureAndPlayer::Visit(std::map<OBJECT_HANDLE, Corpse *> &m ) {}
    template<> inline void SpellNotifierCreatureAndPlayer::Visit(std::map<OBJECT_HANDLE, GameObject *> &m ) {}
    template<> inline void SpellNotifierCreatureAndPlayer::Visit(std::map<OBJECT_HANDLE, DynamicObject *> &m ) {}
    #endif

}

typedef void(Spell::*pEffect)(uint32 i);
#endif
