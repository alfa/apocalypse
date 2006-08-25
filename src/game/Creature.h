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

#ifndef MANGOSSERVER_CREATURE_H
#define MANGOSSERVER_CREATURE_H

#include "Common.h"
#include "Unit.h"
#include "UpdateMask.h"
#include "MotionMaster.h"
#include "ItemPrototype.h"
#include "LootMgr.h"

struct SpellEntry;

class CreatureAI;
class Quest;
class Player;
class WorldSession;

#define UNIT_MOVEMENT_INTERPOLATE_INTERVAL 300
#define MAX_CREATURE_WAYPOINTS 16
#define MAX_CREATURE_ITEMS 128

enum Gossip_Option
{
    GOSSIP_OPTION_NONE              = 0,                    //UNIT_NPC_FLAG_NONE              = 0,
    GOSSIP_OPTION_GOSSIP            = 1,                    //UNIT_NPC_FLAG_GOSSIP            = 1,
    GOSSIP_OPTION_QUESTGIVER        = 2,                    //UNIT_NPC_FLAG_QUESTGIVER        = 2,
    GOSSIP_OPTION_VENDOR            = 3,                    //UNIT_NPC_FLAG_VENDOR            = 4,
    GOSSIP_OPTION_TAXIVENDOR        = 4,                    //UNIT_NPC_FLAG_TAXIVENDOR        = 8,
    GOSSIP_OPTION_TRAINER           = 5,                    //UNIT_NPC_FLAG_TRAINER           = 16,
    GOSSIP_OPTION_SPIRITHEALER      = 6,                    //UNIT_NPC_FLAG_SPIRITHEALER      = 32,
    GOSSIP_OPTION_GUARD             = 7,                    //UNIT_NPC_FLAG_GUARD              = 64,
    GOSSIP_OPTION_INNKEEPER         = 8,                    //UNIT_NPC_FLAG_INNKEEPER         = 128,
    GOSSIP_OPTION_BANKER            = 9,                    //UNIT_NPC_FLAG_BANKER            = 256,
    GOSSIP_OPTION_PETITIONER        = 10,                   //UNIT_NPC_FLAG_PETITIONER        = 512,
    GOSSIP_OPTION_TABARDVENDOR      = 11,                   //UNIT_NPC_FLAG_TABARDVENDOR      = 1024,
    GOSSIP_OPTION_BATTLEFIELD       = 12,                   //UNIT_NPC_FLAG_BATTLEFIELDPERSON = 2048,
    GOSSIP_OPTION_AUCTIONEER        = 13,                   //UNIT_NPC_FLAG_AUCTIONEER        = 4096,
    GOSSIP_OPTION_STABLEPET         = 14,                   //UNIT_NPC_FLAG_STABLE            = 8192,
    GOSSIP_OPTION_ARMORER           = 15                    //UNIT_NPC_FLAG_ARMORER           = 16384,
};

enum Gossip_Guard
{
    GOSSIP_GUARD_BANK               = 32,
    GOSSIP_GUARD_RIDE               = 33,
    GOSSIP_GUARD_GUILD              = 34,
    GOSSIP_GUARD_INN                = 35,
    GOSSIP_GUARD_MAIL               = 36,
    GOSSIP_GUARD_AUCTION            = 37,
    GOSSIP_GUARD_WEAPON             = 38,
    GOSSIP_GUARD_STABLE             = 39,
    GOSSIP_GUARD_BATTLE             = 40,
    GOSSIP_GUARD_SPELLTRAINER       = 41,
    GOSSIP_GUARD_SKILLTRAINER       = 42
};

enum Gossip_Guard_Spell
{
    GOSSIP_GUARD_SPELL_WARRIOR      = 64,
    GOSSIP_GUARD_SPELL_PALADIN      = 65,
    GOSSIP_GUARD_SPELL_HUNTER       = 66,
    GOSSIP_GUARD_SPELL_ROGUE        = 67,
    GOSSIP_GUARD_SPELL_PRIEST       = 68,
    GOSSIP_GUARD_SPELL_UNKNOWN1     = 69,
    GOSSIP_GUARD_SPELL_SHAMAN       = 70,
    GOSSIP_GUARD_SPELL_MAGE         = 71,
    GOSSIP_GUARD_SPELL_WARLOCK      = 72,
    GOSSIP_GUARD_SPELL_UNKNOWN2     = 73,
    GOSSIP_GUARD_SPELL_DRUID        = 74
};

enum Gossip_Guard_Skill
{
    GOSSIP_GUARD_SKILL_ALCHEMY      = 80,
    GOSSIP_GUARD_SKILL_BLACKSMITH   = 81,
    GOSSIP_GUARD_SKILL_COOKING      = 82,
    GOSSIP_GUARD_SKILL_ENCHANT      = 83,
    GOSSIP_GUARD_SKILL_FIRSTAID     = 84,
    GOSSIP_GUARD_SKILL_FISHING      = 85,
    GOSSIP_GUARD_SKILL_HERBALISM    = 86,
    GOSSIP_GUARD_SKILL_LEATHER      = 87,
    GOSSIP_GUARD_SKILL_MINING       = 88,
    GOSSIP_GUARD_SKILL_SKINNING     = 89,
    GOSSIP_GUARD_SKILL_TAILORING    = 90,
    GOSSIP_GUARD_SKILL_ENGINERING   = 91
};

struct GossipOption
{
    uint32 Id;
    uint32 GossipId;
    uint32 NpcFlag;
    uint32 Icon;
    uint32 Action;
    std::string Option;
};

struct CreatureItem
{
    uint32 id;
    uint32 count;
    uint32 maxcount;
    uint32 incrtime;
    uint32 lastincr;
};

struct TrainerSpell
{
    SpellEntry* spell;
    uint32 spellcost;
    uint32 reqspell;
    uint32 reqskill;
    uint32 reqskillvalue;
};

// Only GCC 4.1.0 and later support #pragma pack(push,1) syntax
#if defined( __GNUC__ ) && (GCC_MAJOR < 4 || GCC_MAJOR == 4 && GCC_MINOR < 1)
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

struct CreatureInfo
{
    uint32  Entry;
    uint32  DisplayID;
    char*   Name;
    char*   SubName;
    uint32  maxhealth;
    uint32  maxmana;
    uint32  level;
    uint32  armor;
    uint32  faction;
    uint32  npcflag;
    float   speed;
    uint32  rank;
    float   mindmg;
    float   maxdmg;
    uint32  attackpower;
    uint32  baseattacktime;
    uint32  rangeattacktime;
    uint32  Flags;
    uint32  mount;
    uint32  level_max;
    uint32  dynamicflags;
    float   size;
    uint32  family;
    float   bounding_radius;
    uint32  trainer_type;
    uint32  trainer_spell;
    uint32  classNum;
    uint32  race;
    float   minrangedmg;
    float   maxrangedmg;
    uint32  rangedattackpower;
    float   combat_reach;
    uint32  type;
    uint32  civilian;
    uint32  flag1;
    uint32  equipmodel[3];
    uint32  equipinfo[3];
    uint32  equipslot[3];
    uint32  lootid;
    uint32  SkinLootId;
    uint32  resistance1;
    uint32  resistance2;
    uint32  resistance3;
    uint32  resistance4;
    uint32  resistance5;
    uint32  resistance6;
    uint32  spell1;
    uint32  spell2;
    uint32  spell3;
    uint32  spell4;
    uint32  mingold;
    uint32  maxgold;
    char const* AIName;
    char const* MovementGen;
    char const* ScriptName;
};

#if defined( __GNUC__ ) && (GCC_MAJOR < 4 || GCC_MAJOR == 4 && GCC_MINOR < 1)
#pragma pack()
#else
#pragma pack(pop)
#endif

enum UNIT_TYPE
{
    NOUNITTYPE = 0,
    BEAST      = 1,
    DRAGONSKIN = 2,
    DEMON      = 3,
    ELEMENTAL  = 4,
    GIANT      = 5,
    UNDEAD     = 6,
    HUMANOID   = 7,
    CRITTER    = 8,
    MECHANICAL = 9,
};

class MANGOS_DLL_SPEC Creature : public Unit
{
    CreatureAI *i_AI;
    MotionMaster i_motionMaster;

    public:

        Creature();
        virtual ~Creature();

        typedef std::list<TrainerSpell*> SpellsList;

        bool Create (uint32 guidlow, uint32 mapid, float x, float y, float z, float ang, uint32 Entry);
        bool CreateFromProto(uint32 guidlow,uint32 Entry);

        virtual void Update( uint32 time );
        inline void GetRespawnCoord(float &x, float &y, float &z) const { x = respawn_cord[0]; y = respawn_cord[1]; z = respawn_cord[2]; }

        bool isPet() const { return m_isPet; }
        bool isTotem() const { return m_isTotem; }
        bool isCivilian() const { return GetCreatureInfo()->civilian != 0; }
        bool isCanSwimOrFly() const;
        bool isCanWalkOrFly() const;

        void AIM_Update(const uint32 &);
        void AIM_Initialize(void);
        MotionMaster* operator->(void) { return &i_motionMaster; }

        void AI_SendMoveToPacket(float x, float y, float z, uint32 time, bool run, bool WalkBack);
        inline CreatureAI &AI(void) { return *i_AI; }

        inline void setMoveRandomFlag(bool f) { m_moveRandom = f; }
        inline void setMoveRunFlag(bool f) { m_moveRun = f; }
        inline bool getMoveRandomFlag() { return m_moveRandom; }
        inline bool getMoveRunFlag() { return m_moveRun; }
        inline bool IsStopped(void) const { return !(hasUnitState(UNIT_STAT_MOVING)); }
        inline void StopMoving(void)
        {
            clearUnitState(UNIT_STAT_MOVING);
            AI_SendMoveToPacket(GetPositionX(), GetPositionY(), GetPositionZ(), 0, true, false);
        }

        uint32 GetBlockValue() const                        //dunno mob block value
        {
            return getLevel()/2 + 1;
        }

        /*********************************************************/
        /***                    VENDOR SYSTEM                  ***/
        /*********************************************************/

        uint8 GetItemCount() const { return itemcount; }
        uint32 GetItemId( uint32 slot ) const { return item_list[slot].id; }
        uint32 GetItemCount( uint32 slot ) const { return item_list[slot].count; }
        uint32 GetMaxItemCount( uint32 slot ) const { return item_list[slot].maxcount; }
        uint32 GetItemIncrTime( uint32 slot ) const { return item_list[slot].incrtime; }
        uint32 GetItemLastIncr( uint32 slot ) const { return item_list[slot].lastincr; }
        void SetItemCount( uint32 slot, uint32 count ) { item_list[slot].count = count; }
        void SetItemLastIncr( uint32 slot, uint32 ptime ) { item_list[slot].lastincr = ptime; }
        void AddItem( uint32 item, uint32 maxcount, uint32 ptime)
        {
            item_list[itemcount].id = item;
            item_list[itemcount].count = maxcount;
            item_list[itemcount].maxcount = maxcount;
            item_list[itemcount].incrtime = ptime;
            item_list[itemcount].lastincr = (uint32)time(NULL);
            itemcount++;
        }

        CreatureInfo const *GetCreatureInfo() const;

        void CreateTrainerSpells();
        uint32 GetTrainerSpellsSize() const { return m_tspells.size(); }
        std::list<TrainerSpell*>::iterator GetTspellsBegin(){ return m_tspells.begin(); }
        std::list<TrainerSpell*>::iterator GetTspellsEnd(){ return m_tspells.end(); }

        uint32 getDialogStatus(Player *pPlayer, uint32 defstatus);

        bool hasQuest(uint32 quest_id);
        bool hasInvolvedQuest(uint32 quest_id);

        void prepareGossipMenu( Player *pPlayer,uint32 gossipid );
        void sendPreparedGossip( Player* player);
        void OnGossipSelect(Player* player, uint32 option);
        void OnPoiSelect(Player* player, GossipOption *gossip);

        uint32 GetGossipTextId(uint32 action, uint32 zoneid);
        uint32 GetNpcTextId();
        void LoadGossipOptions();
        std::string GetGossipTitle(uint8 type, uint32 id);
        GossipOption* GetGossipOption( uint32 id );
        uint32 GetGossipCount( uint32 gossipid );
        void addGossipOption(GossipOption *gso) { m_goptions.push_back(gso); }

        void generateMoneyLoot();
        void getSkinLoot();

        inline void setEmoteState(uint8 emote) { m_emoteState = emote; };

        void setDeathState(DeathState s)
        {
            if(s == JUST_DIED)
            {
                m_deathTimer = m_corpseDelay;

                if(!IsStopped()) StopMoving();
            }
            Unit::setDeathState(s);

            if(s == JUST_DIED)
                Unit::setDeathState(CORPSE);
        };

        void Say(char const* text, uint32 language);

        void SaveToDB();
        bool LoadFromDB(uint32 guid);
        void DeleteFromDB();

        Loot loot;
        bool pickPocketed;

        SpellEntry *reachWithSpellAttack(Unit *pVictim);
        uint32 m_spells[CREATURE_MAX_SPELLS];

        float GetAttackDistance(Unit *pl);
    protected:
        void _LoadGoods();
        void _LoadQuests();
        void _LoadMovement();
        void _RealtimeSetCreatureInfo();

        uint32 m_lootMoney;

        /// Timers
        uint32 m_deathTimer;                                // timer for death or corpse disappearance
        uint32 m_respawnTimer;                              // timer for respawn to happen
        uint32 m_respawnDelay;                              // delay between corpse disappearance and respawning
        uint32 m_corpseDelay;                               // delay between death and corpse disappearance
        float m_respawnradius;
        CreatureItem item_list[MAX_CREATURE_ITEMS];
        int itemcount;

        SpellsList m_tspells;

        uint32 mTaxiNode;

        std::list<GossipOption*> m_goptions;

        float respawn_cord[3];
        bool m_moveBackward;
        bool m_moveRandom;
        bool m_moveRun;

        uint8 m_emoteState;
        bool m_isPet;                                       // set only in Pet::Pet
        bool m_isTotem;                                     // set only in Totem::Totem
        void RegenerateMana();
        void RegenerateHealth();
        uint32 m_regenTimer;

};
#endif
