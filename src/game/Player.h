/* Player.h
 *
 * Copyright (C) 2004 Wow Daemon
 * Copyright (C) 2005 MaNGOS <https://opensvn.csie.org/traccgi/MaNGOS/trac.cgi/>
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

#ifndef _PLAYER_H
#define _PLAYER_H

#include "ItemPrototype.h"
#include "Unit.h"
#include "Database/DatabaseEnv.h"
#include "NPCHandler.h"
#include "QuestDef.h"

struct Mail;
class Channel;
class DynamicObject;
class Creature;
class PlayerMenu;

//====================================================================
//  Inventory
//  Holds the display id and item type id for objects in
//  a character's inventory
//====================================================================

enum Classes
{
    WARRIOR = 1,
    PALADIN = 2,
    HUNTER = 3,
    ROGUE = 4,
    PRIEST = 5,
    SHAMAN = 7,
    MAGE = 8,
    WARLOCK = 9,
    DRUID = 11,
};

enum Races
{
    HUMAN = 1,
    ORC = 2,
    DWARF = 3,
    NIGHTELF = 4,
    UNDEAD_PLAYER = 5,
    TAUREN = 6,
    GNOME = 7,
    TROLL = 8,
};

struct spells
{
    uint16 spellId;
    uint16 slotId;
};

struct actions
{
    uint8 button;
    uint8 type;
    uint8 misc;
    uint16 action;
};

struct skilllines
{
    uint32 lineId;
    uint16 currVal;
    uint16 maxVal;
    uint16 posStatCurrVal;
    uint16 posstatMaxVal;
};
struct PlayerCreateInfo
{
    uint8 createId;
    uint8 race;
    uint8 class_;
    uint32 mapId;
    uint32 zoneId;
    float positionX;
    float positionY;
    float positionZ;
    uint16 displayId;
    uint8 strength;
    uint8 ability;
    uint8 stamina;
    uint8 intellect;
    uint8 spirit;
    uint32 health;
    uint32 mana;
    uint32 rage;
    uint32 focus;
    uint32 energy;
    uint32 attackpower;
    float mindmg;
    float maxdmg;
    std::list<uint32> item;
    std::list<uint8> item_slot;
    std::list<uint16> spell;
    std::list<uint16> skill[3];
    std::list<uint16> action[4];
};

struct Areas
{
    uint32 areaID;
    uint32 areaFlag;
    float x1;
    float x2;
    float y1;
    float y2;
};

struct Factions
{
	uint32 ID;        //to lacate into dbc file
	uint32 ReputationListID;
	uint32 Flags;     //1 = Visible, 2 = AtWar
	uint32 Standing;
};

enum PlayerMovementType
{
    MOVE_ROOT       = 1,
    MOVE_UNROOT     = 2,
    MOVE_WATER_WALK = 3,
    MOVE_LAND_WALK  = 4,
};

enum PlayerSpeedType
{
    RUN      = 1,
    RUNBACK  = 2,
    SWIM     = 3,
    SWIMBACK = 4,
    WALK     = 5,
};

class Quest;
class Spell;
class Item;
class WorldSession;

#define EQUIPMENT_SLOT_START         0
#define EQUIPMENT_SLOT_HEAD          0
#define EQUIPMENT_SLOT_NECK          1
#define EQUIPMENT_SLOT_SHOULDERS     2
#define EQUIPMENT_SLOT_BODY          3
#define EQUIPMENT_SLOT_CHEST         4
#define EQUIPMENT_SLOT_WAIST         5
#define EQUIPMENT_SLOT_LEGS          6
#define EQUIPMENT_SLOT_FEET          7
#define EQUIPMENT_SLOT_WRISTS        8
#define EQUIPMENT_SLOT_HANDS         9
#define EQUIPMENT_SLOT_FINGER1       10
#define EQUIPMENT_SLOT_FINGER2       11
#define EQUIPMENT_SLOT_TRINKET1      12
#define EQUIPMENT_SLOT_TRINKET2      13
#define EQUIPMENT_SLOT_BACK          14
#define EQUIPMENT_SLOT_MAINHAND      15
#define EQUIPMENT_SLOT_OFFHAND       16
#define EQUIPMENT_SLOT_RANGED        17
#define EQUIPMENT_SLOT_TABARD        18
#define EQUIPMENT_SLOT_END           19

#define INVENTORY_SLOT_BAG_START     19
#define INVENTORY_SLOT_BAG_1         19
#define INVENTORY_SLOT_BAG_2         20
#define INVENTORY_SLOT_BAG_3         21
#define INVENTORY_SLOT_BAG_4         22
#define INVENTORY_SLOT_BAG_END       23

#define INVENTORY_SLOT_ITEM_START    23
#define INVENTORY_SLOT_ITEM_1        23
#define INVENTORY_SLOT_ITEM_2        24
#define INVENTORY_SLOT_ITEM_3        25
#define INVENTORY_SLOT_ITEM_4        26
#define INVENTORY_SLOT_ITEM_5        27
#define INVENTORY_SLOT_ITEM_6        28
#define INVENTORY_SLOT_ITEM_7        29
#define INVENTORY_SLOT_ITEM_8        30
#define INVENTORY_SLOT_ITEM_9        31
#define INVENTORY_SLOT_ITEM_10       32
#define INVENTORY_SLOT_ITEM_11       33
#define INVENTORY_SLOT_ITEM_12       34
#define INVENTORY_SLOT_ITEM_13       35
#define INVENTORY_SLOT_ITEM_14       36
#define INVENTORY_SLOT_ITEM_15       37
#define INVENTORY_SLOT_ITEM_16       38
#define INVENTORY_SLOT_ITEM_END      39

#define BANK_SLOT_ITEM_START         39
#define BANK_SLOT_ITEM_1             39
#define BANK_SLOT_ITEM_2             40
#define BANK_SLOT_ITEM_3             41
#define BANK_SLOT_ITEM_4             42
#define BANK_SLOT_ITEM_5             43
#define BANK_SLOT_ITEM_6             44
#define BANK_SLOT_ITEM_7             45
#define BANK_SLOT_ITEM_8             46
#define BANK_SLOT_ITEM_9             47
#define BANK_SLOT_ITEM_10            48
#define BANK_SLOT_ITEM_11            49
#define BANK_SLOT_ITEM_12            50
#define BANK_SLOT_ITEM_13            51
#define BANK_SLOT_ITEM_14            52
#define BANK_SLOT_ITEM_15            53
#define BANK_SLOT_ITEM_16            54
#define BANK_SLOT_ITEM_17            55
#define BANK_SLOT_ITEM_18            56
#define BANK_SLOT_ITEM_19            57
#define BANK_SLOT_ITEM_20            58
#define BANK_SLOT_ITEM_21            59
#define BANK_SLOT_ITEM_22            60
#define BANK_SLOT_ITEM_23            61
#define BANK_SLOT_ITEM_24            62
#define BANK_SLOT_ITEM_END           63

#define BANK_SLOT_BAG_START          63
#define BANK_SLOT_BAG_1              63
#define BANK_SLOT_BAG_2              64
#define BANK_SLOT_BAG_3              65
#define BANK_SLOT_BAG_4              66
#define BANK_SLOT_BAG_5              67
#define BANK_SLOT_BAG_6              68
#define BANK_SLOT_BAG_END            69
//====================================================================
//  Player
//  Class that holds every created character on the server.
//
//  TODO:  Attach characters to user accounts
//====================================================================
class Player : public Unit
{
    friend class WorldSession;
    public:
        Player (WorldSession *session);
        ~Player ( );

        void AddToWorld();
        void RemoveFromWorld();

        void Create ( uint32 guidlow, WorldPacket &data );

        void Update( uint32 time );

        void BuildEnumData( WorldPacket * p_data );

        uint8 ToggleAFK() { m_afk = !m_afk; return m_afk; };
        const char* GetName() { return m_name.c_str(); };

        void Die();
        void GiveXP(uint32 xp, const uint64 &guid);

        // Taxi
        void setDismountTimer(uint32 time) { m_dismountTimer = time; };
        void setDismountCost(uint32 money) { m_dismountCost = money; };
        void setMountPos(float x, float y, float z)
        {
            m_mount_pos_x = x;
            m_mount_pos_y = y;
            m_mount_pos_z = z;
        }

        // Quests
		uint32 getQuestStatus(uint32 quest_id);
		  bool getQuestRewardStatus(uint32 quest_id);
		uint32 addNewQuest(uint32 quest_id, uint32 status = QUEST_STATUS_AVAILABLE);
		  void loadExistingQuest(struct quest_status qs);
		  void setQuestStatus(uint32 quest_id, uint32 new_status, bool new_rewarded);
		  bool checkQuestStatus(uint32 quest_id);
  quest_status getQuestStatusStruct(uint32 quest_id);

		bool isQuestComplete(uint32 quest_id, Creature *pCreature);
		bool isQuestTakable(uint32 quest_id);
		
		void finishExplorationQuest( Quest *pQuest );
		void sendPreparedGossip( uint32 textid, QEmote em, std::string QTitle, uint64 guid);

		uint16 getOpenQuestSlot();
		uint16 getQuestSlot(uint32 quest_id);
		uint16 getQuestSlotById(uint32 slot_id);

		void RemovedItemFromBackpack(uint32 entry);
		void AddedItemToBackpack(uint32 entry, uint32 count);
		void KilledMonster(uint32 entry, uint64 guid);

		inline uint32 GetMoney() { return GetUInt32Value (PLAYER_FIELD_COINAGE); }
		inline void ModifyMoney (int32 d) { SetMoney (GetMoney() + d); }
		void SetMoney (uint32 value) { SetUInt32Value (PLAYER_FIELD_COINAGE, value); }

		uint32 GetTutorialInt(uint32 intId )
		{
			ASSERT( (intId < 8) );
			return m_Tutorials[intId];
		}

		void SetTutorialInt(uint32 intId, uint32 value)
		{
			ASSERT( (intId < 8) );
			m_Tutorials[intId] = value;
		}
		/* Support Functions */

		bool AddItemToBackpack (uint32 itemId, uint32 count = 1) { return false; }
		bool RemoveItemFromBackpack (uint32 itemId, uint32 count = 1) { return false; }
		bool HasItemInBackpack (uint32 itemId, uint32 count = 1) { return false; }
		bool HasSpaceForItemInBackpack (uint32 itemId, uint32 count = 1) { return false; }


        void AddMail(Mail *m);

        // sets the needed bits for any quests in the player's log
        // void setQuestLogBits(UpdateMask *updateMask);
        std::map<uint32, struct quest_status> getQuestStatusMap() { return mQuestStatus; };

        const uint64& GetSelection( ) const { return m_curSelection; }
        const uint64& GetTarget( ) const { return m_curTarget; }

        void SetSelection(const uint64 &guid) { m_curSelection = guid; }
        void SetTarget(const uint64 &guid) { m_curTarget = guid; }

        uint32 GetMailSize() { return m_mail.size();};
        Mail* GetMail(uint32 id);
        void RemoveMail(uint32 id);
        std::list<Mail*>::iterator GetmailBegin() { return m_mail.begin();};
        std::list<Mail*>::iterator GetmailEnd() { return m_mail.end();};
        void AddBid(bidentry *be);
        bidentry* GetBid(uint32 id);
        std::list<bidentry*>::iterator GetBidBegin() { return m_bids.begin();};
        std::list<bidentry*>::iterator GetBidEnd() { return m_bids.end();};

        // spells
        bool HasSpell(uint32 spell);
        void smsg_InitialSpells();
        void addSpell(uint16 spell_id, uint16 slot_id=0xffff);
        bool removeSpell(uint16 spell_id);        //add by vendy 2005/10/9 22:43
        void DealWithSpellDamage(DynamicObject &);
        inline std::list<struct spells> getSpellList() { return m_spells; };
        void setResurrect(uint64 guid,float X, float Y, float Z, uint32 health, uint32 mana)
        {
            m_resurrectGUID = guid;
            m_resurrectX = X;
            m_resurrectY = Y;
            m_resurrectZ = Z;
            m_resurrectHealth = health;
            m_resurrectMana = mana;
        };

        uint32 getFaction()
        {
            return m_faction;
        };

        void SetPvP(bool b)
        {
            pvpOn = b;
        };

        bool GetPvP()
        {
            return pvpOn;
        };

        void setGold(int gold)
        {
            uint32 moneyuser = GetUInt32Value(PLAYER_FIELD_COINAGE);
            SetUInt32Value( PLAYER_FIELD_COINAGE, moneyuser + gold );
        };

        void setFaction(uint8 race, uint32 faction);

        //action bar
        inline std::list<struct actions> getActionList() { return m_actions; };
        void addAction(uint8 button, uint16 action, uint8 type, uint8 misc);
        void removeAction(uint8 button);
        void smsg_InitialActions();

        // groups
        void SetInvited() { m_isInvited = true; }
        void SetInGroup() { m_isInGroup = true; }
        void SetLeader(const uint64 &guid) { m_groupLeader = guid; }

        int  IsInGroup() { return m_isInGroup; }
        int  IsInvited() { return m_isInvited; }
        const uint64& GetGroupLeader() const { return m_groupLeader; }

        void UnSetInvited() { m_isInvited = false; }
        void UnSetInGroup() { m_isInGroup = false; }

		//Guild
		void SetGuildIdInvited(uint32 GuildId) { m_GuildIdInvited = GuildId; }
		void SetInGuild(uint32 GuildId) { SetUInt32Value(PLAYER_GUILDID, GuildId);  }
		void SetRank(uint32 rankId){ SetUInt32Value(PLAYER_GUILDRANK, rankId); }

		uint32 GetGuildId() { return GetUInt32Value(PLAYER_GUILDID);  }
		uint32 GetRank(){ return GetUInt32Value(PLAYER_GUILDRANK); }
		int GetGuildIdInvited() { return m_GuildIdInvited; }
		
        //duel
        void SetDuelVsGUID(const uint64 &guid) { m_duelGUID = guid; }
        void SetInDuel(const bool &val) { m_isInDuel = val; }
        void SetDuelSenderGUID(const uint64 &guid) { m_duelSenderGUID = guid; }
        void SetDuelFlagGUID(const uint64 &guid) { m_duelFlagGUID = guid; }

        // Deadknight isGroupMember(plyr)
        bool IsGroupMember(Player *plyr);

        // Items
        void UpdateSlot(uint8 slot)
        {
            //Isso eh pra atualizar o item... evita q ele fique cinza e inacessivel
            Item* Up = RemoveItemFromSlot(slot);
            if (Up != NULL) AddItemToSlot(slot, Up);
        }
        void SwapItemSlots(uint8 srcslot, uint8 dstslot);
        Item* GetItemBySlot(uint8 slot) const
        {
            /* ASSERT(slot < INVENTORY_SLOT_ITEM_END); */
            ASSERT(slot < BANK_SLOT_BAG_END);     /* Bank Support */
            return m_items[slot];
        }
        uint32 GetSlotByItemID(uint32 ID);
        uint32 GetSlotByItemGUID(uint64 guid);
        void AddItemToSlot(uint8 slot, Item *item);
        Item* RemoveItemFromSlot(uint8 slot);
        uint8 FindFreeItemSlot(uint32 type);
        int CountFreeBagSlot();
        uint8 CanEquipItemInSlot(uint8 slot, ItemPrototype* item);
		bool CanEquipItem (ItemPrototype * proto);
		bool CanUseItem (ItemPrototype* proto);

        // looting
        const uint64& GetLootGUID() const { return m_lootGuid; }
        void SetLootGUID(const uint64 &guid) { m_lootGuid = guid; }

        WorldSession* GetSession() const { return m_session; }
        void SetSession(WorldSession *s) { m_session = s; }

        void CreateYourself( );
        void DestroyYourself( );

        // These functions build a specific type of A9 packet
        void BuildCreateUpdateBlockForPlayer( UpdateData *data, Player *target ) const;
        void DestroyForPlayer( Player *target ) const;
        void SendDelayResponse(const uint32);

        // Misc
        bool SetPosition(const float &x, const float &y, const float &z, const float &orientation);
        void SendMessageToSet(WorldPacket *data, bool self);
		void SetSheath (uint32 sheathed);

        // Serialize character to db
        void SaveToDB();
        void LoadFromDB(uint32 guid);
        void DeleteFromDB();

#ifndef __NO_PLAYERS_ARRAY__
        // UQ1: Positional Array Update...
        void SetPlayerPositionArray();
#endif                                        //__NO_PLAYERS_ARRAY__

        // Death Stuff
        void SpawnCorpseBody();
        void SpawnCorpseBones();
        void CreateCorpse();
        void KillPlayer();
        void ResurrectPlayer();
        void BuildPlayerRepop();
        void DeathDurabilityLoss(double percent);
        void RepopAtGraveyard();
        void DuelComplete();

        // Movement stuff
        void SetMovement(uint8 pType);
        void SetPlayerSpeed(uint8 SpeedType, float value, bool forced=false);

        // Channel stuff
        void JoinedChannel(Channel *c);
        void LeftChannel(Channel *c);
        void CleanupChannels();

        // friends stuff
        void BroadcastToFriends(std::string msg);

        // skilllines
		uint32 GetSkillByProto( ItemPrototype *proto );
        bool HasSkillLine(uint32 id);
        void AddSkillLine(uint32 id, uint16 currVal, uint16 maxVal);
        void AddSkillLine(uint32 id, uint16 currVal, uint16 maxVal, bool sendUpdate);
        void RemoveSkillLine(uint32 id);
        inline std::list<struct skilllines>getSkillLines() { return m_skilllines; }

        void SetDontMove(bool dontMove);
        bool GetDontMove() { return m_dontMove; }

        //Explore Area System
        void CheckExploreSystem(void);
        // Initialize the possible areas that player will discover.
        void InitExploreSystem(void);

		//Reputation
		bool SetStanding(uint32 FTemplate, int standing);
		void LoadReputationFromDBC(void);
		void UpdateReputation(void);

		//Honor
		void UpdateHonor(void);
		void CalculateHonor(Player *pVictim);
		int  GetHonorRank();

		void ApplyItemMods(Item *item,uint8 slot,bool apply)
		{
			_ApplyItemMods(item, slot, apply);
		};

		void _ApplyItemMods(Item *item,uint8 slot,bool apply);
        void _RemoveAllItemMods();
        void _ApplyAllItemMods();

		PlayerMenu* PlayerTalkClass;

protected:
        void _SetCreateBits(UpdateMask *updateMask, Player *target) const;
        void _SetUpdateBits(UpdateMask *updateMask, Player *target) const;
        void _SetVisibleBits(UpdateMask *updateMask, Player *target) const;

        void _SaveMail();
        void _LoadMail();
        void _SaveInventory();
        void _SaveSpells();
        void _SaveActions();
		void _SaveTutorials();
        void _SaveQuestStatus();
        void _SaveAffects();
        void _SaveBids();
        void _LoadBids();
        void _SaveAuctions();
        void _LoadInventory();
        void _LoadSpells();
        void _LoadActions();
        void _LoadQuestStatus();
        void _LoadAffects();
		void _LoadTutorials();
		
		void _LoadReputation(void);
		void _SaveReputation(void);
		bool FactionIsInTheList(uint32 faction);

        /*void _ApplyItemMods(Item *item,uint8 slot,bool apply);
        void _RemoveAllItemMods();
        void _ApplyAllItemMods();*/

        uint64 m_lootGuid;

        std::string m_name;                       // max 21 character name
        uint32 m_race;
        uint32 m_class;
        uint32 m_faction;
		uint32 m_team; //Alliance, Horde or Steamwheedle Cartel

        uint8 m_outfitId;

        //uint16 m_guildId;
        uint16 m_petInfoId;                       // pet display info id
        uint16 m_petLevel;                        // pet experience level
        uint16 m_petFamilyId;                     // pet creature family id

        uint32 m_dismountTimer;
        uint32 m_dismountCost;
        float m_mount_pos_x;
        float m_mount_pos_y;
        float m_mount_pos_z;

        uint32 m_nextSave;

        // Inventory and equipment
        Item* m_items[BANK_SLOT_BAG_END];

        // AFK status
        uint8 m_afk;

        // guid of current target
        uint64 m_curTarget;

        // guid of current selection
        uint64 m_curSelection;

        // current quest statuses
        typedef std::map<uint32, struct quest_status> StatusMap;
        StatusMap mQuestStatus;

        // Group
        uint64 m_groupLeader;
        bool m_isInGroup;
        bool m_isInvited;

		//HONOR SYSTEM
		//Medals
		int m_pvp_medals;
		//Kills
		int m_session_kills;
		int m_yesterday_kills;
		int m_last_week_kills;
		int m_this_week_kills;
		//Contribution
		int m_this_week_contribution;
		int m_last_week_contribution;
		int m_yesterday_contribution;
		//Life time
		int m_lifetime_honorbale_kills;
		int m_lifetime_dishonorbale_kills;
		//Rank
		int m_last_week_rank;
		int m_honor_points;

		// Guild
		uint32 m_GuildIdInvited; 

        //If Player is in combat
        bool inCombat;
        //Time to logout after a combat
        int logoutDelay;

		//Areas to descover
        std::list<struct Areas> areas;

		//Reputation system
		std::list<struct Factions> factions;

        // items the player has bid on
        std::list<bidentry*> m_bids;

        // pieces of mail the player has
        std::list<Mail*> m_mail;

        // Player Spells
        std::list<struct spells> m_spells;

        // Player Action Bar
        std::list<struct actions> m_actions;

        // Player Skilllines
        std::list<struct skilllines> m_skilllines;

        // vars for ressurection
        uint64 m_resurrectGUID;
        float m_resurrectX, m_resurrectY, m_resurrectZ;
        uint32 m_resurrectHealth, m_resurrectMana;

        // Pointer to this char's game client
        WorldSession *m_session;

        // Channels
        std::list<Channel*> m_channels;

        bool m_dontMove;

        Player *pTrader;
        bool acceptTrade;
        int tradeItems[7];
        uint32 tradeGold;

        bool pvpOn;

        //duel
        uint64 m_duelGUID;
        bool   m_isInDuel;
        uint64 m_duelSenderGUID;
        uint64 m_duelFlagGUID;

        time_t m_nextThinkTime;
		uint32 m_timedQuest;
		uint32 m_Tutorials[8];

};

// UQ1: Defined in Player.cpp... CHECKME: Move somewhere else???
int irand(int min, int max);
uint32 urand(uint32 min, uint32 max);
#endif
