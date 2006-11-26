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

#ifndef MANGOSSERVER_QUEST_H
#define MANGOSSERVER_QUEST_H

#include "Platform/Define.h"
#include "Database/DatabaseEnv.h"
#include <string>
#include <vector>
using namespace std;

class Player;

class ObjectMgr;

// (PLAYER_VISIBLE_ITEM_1_CREATOR - PLAYER_QUEST_LOG_1_1)/3
#define MAX_QUEST_LOG_SIZE 20

#define QUEST_OBJECTIVES_COUNT 4
#define QUEST_SOURCE_ITEM_IDS_COUNT 4
#define QUEST_REWARD_CHOICES_COUNT 6
#define QUEST_REWARDS_COUNT 4
#define QUEST_DEPLINK_COUNT 10

enum
{
    INVALIDREASON_DONT_HAVE_REQ        = 0,
    INVALIDREASON_DONT_HAVE_REQLEVEL   = 1,
    INVALIDREASON_DONT_HAVE_RACE       = 6,
    INVALIDREASON_HAVE_TIMED_QUEST     = 12,
    INVALIDREASON_HAVE_QUEST           = 13,
    INVALIDREASON_DONT_HAVE_REQ_ITEMS  = 19+1,
    INVALIDREASON_DONT_HAVE_REQ_MONEY  = 21+1,              //1.12.1
};

enum
{
    QUEST_PARTY_MSG_SHARRING_QUEST  = 0,
    QUEST_PARTY_MSG_CANT_TAKE_QUEST = 1,
    QUEST_PARTY_MSG_ACCEPT_QUEST    = 2,
    QUEST_PARTY_MSG_REFUSE_QUEST    = 3,
    QUEST_PARTY_MSG_TO_FAR          = 4,
    QUEST_PARTY_MSG_BUSY            = 5,
    QUEST_PARTY_MSG_LOG_FULL        = 6,
    QUEST_PARTY_MSG_HAVE_QUEST      = 7,
    QUEST_PARTY_MSG_FINISH_QUEST    = 8,
};

enum __QuestTradeSkill
{
    QUEST_TRSKILL_NONE           = 0,
    QUEST_TRSKILL_ALCHEMY        = 1,
    QUEST_TRSKILL_BLACKSMITHING  = 2,
    QUEST_TRSKILL_COOKING        = 3,
    QUEST_TRSKILL_ENCHANTING     = 4,
    QUEST_TRSKILL_ENGINEERING    = 5,
    QUEST_TRSKILL_FIRSTAID       = 6,
    QUEST_TRSKILL_HERBALISM      = 7,
    QUEST_TRSKILL_LEATHERWORKING = 8,
    QUEST_TRSKILL_POISONS        = 9,
    QUEST_TRSKILL_TAILORING      = 10,
    QUEST_TRSKILL_MINING         = 11,
    QUEST_TRSKILL_FISHING        = 12,
    QUEST_TRSKILL_SKINNING       = 13,
    QUEST_TRSKILL_JEWELCRAFTING  = 14,
};

enum QuestStatus
{
    QUEST_STATUS_NONE           = 0,
    QUEST_STATUS_COMPLETE       = 1,
    QUEST_STATUS_UNAVAILABLE    = 2,
    QUEST_STATUS_INCOMPLETE     = 3,
    QUEST_STATUS_AVAILABLE      = 4,
    MAX_QUEST_STATUS
};

enum __QuestGiverStatus
{
    DIALOG_STATUS_NONE                     = 0,
    DIALOG_STATUS_UNAVAILABLE              = 1,
    DIALOG_STATUS_CHAT                     = 2,
    DIALOG_STATUS_INCOMPLETE               = 3,
    DIALOG_STATUS_REWARD_REP               = 4,
    DIALOG_STATUS_AVAILABLE                = 5,
    DIALOG_STATUS_REWARD_OLD               = 6,             // red dot on minimap
    DIALOG_STATUS_REWARD                   = 7,             // yellow dot on minimap
};

enum __QuestSpecialFlags                                    //according to mangos-db-11-02-2006-for_1_9_x;
{
    QUEST_SPECIAL_FLAGS_NONE          = 0,
    QUEST_SPECIAL_FLAGS_DELIVER       = 1,
    QUEST_SPECIAL_FLAGS_EXPLORATION   = 2,
    QUEST_SPECIAL_FLAGS_SPEAKTO       = 4,

    QUEST_SPECIAL_FLAGS_KILL_OR_CAST  = 8,
    QUEST_SPECIAL_FLAGS_TIMED         = 16,
    QUEST_SPECIAL_FLAGS_REPEATABLE    = 32,                 //?

    QUEST_SPECIAL_FLAGS_REPUTATION    = 64,
};

// This Quest class provides a convenient way to access a few pretotaled (cached) quest details,
// all base quest information, and any utility functions such as generating the amount of
// xp to give
class Quest
{
    friend class ObjectMgr;
    public:
        Quest(Field * questRecord);
        uint32 XPValue( Player *pPlayer );
        
        bool HasSpecialFlag( uint32 flag ) const { return (SpecialFlags & flag ) != 0; }

        // table data accessors:
        uint32 GetQuestId() { return QuestId; }
        uint32 GetZoneId() { return ZoneId; }
        uint32 GetQuestSort() { return QuestSort; }
        uint32 GetMinLevel() { return MinLevel; }
        uint32 GetQuestLevel() { return QuestLevel; }
        uint32 GetType() { return Type; }
        uint32 GetRequiredRaces() { return RequiredRaces; }
        uint32 GetRequiredClass() { return RequiredClass; }
        uint32 GetRequiredSkill() { return RequiredSkill; }
        uint32 GetRequiredSkillValue() { return RequiredSkillValue; }
        uint32 GetRequiredRepFaction() { return RequiredRepFaction; }
        uint32 GetRequiredRepValue() { return RequiredRepValue; }
        uint32 GetLimitTime() { return LimitTime; }
        uint32 GetNextQuestId() { return NextQuestId; }
        uint32 GetExclusiveGroup() { return ExclusiveGroup; }
        uint32 GetSrcItemId() { return SrcItemId; }
        uint32 GetSrcItemCount() { return SrcItemCount; }
        uint32 GetSrcSpell() { return SrcSpell; }
        const char* GetTitle() { return Title.c_str(); }
        const char* GetDetails() { return Details.c_str(); }
        const char* GetObjectives() { return Objectives.c_str(); }
        const char* GetOfferRewardText() { return OfferRewardText.c_str(); }
        const char* GetRequestItemsText() { return RequestItemsText.c_str(); }
        const char* GetEndText() { return EndText.c_str(); }
        uint32 GetRewRepFaction1() { return RewRepFaction1; }
        uint32 GetRewRepFaction2() { return RewRepFaction2; }
        int32  GetRewRepValue1() { return RewRepValue1; }
        int32  GetRewRepValue2() { return RewRepValue2; }
        int32  GetRewOrReqMoney() { return RewOrReqMoney; }
        uint32 GetRewXP() { return RewXP; }
        uint32 GetRewSpell() { return RewSpell; }
        uint32 GetPointMapId() { return PointMapId; }
        float GetPointX() { return PointX; }
        float GetPointY (){ return PointY; }
        uint32 GetPointOpt() { return PointOpt; }
        uint32 GetOfferRewardEmote() { return OfferRewardEmote; }
        uint32 GetRequestItemsEmote() { return RequestItemsEmote; }
        uint32 GetQuestCompleteScript() { return QuestCompleteScript; }
        uint32 GetHaveQuestId() { return HaveQuestId; }
        uint32 IsRepeatable() { return Repeatable; }
        uint32 GetSpecialFlags() { return SpecialFlags; }

        // multiple values
        vector<string> ObjectiveText;
        vector<uint32> ReqItemId;
        vector<uint32> ReqItemCount;
        vector<uint32> ReqSourceId;
        vector<uint32> ReqSourceRef;
        vector<int32> ReqCreatureOrGOId;     // >0 Creature <0 Gameobject
        vector<uint32> ReqCreatureOrGOCount;
        vector<uint32> ReqSpell;
        vector<uint32> RewChoiceItemId;
        vector<uint32> RewChoiceItemCount;
        vector<uint32> RewItemId;
        vector<uint32> RewItemCount;
         
        // multiple values
        /*vector<char*> GetObjectiveText() { return ObjectiveText; }
        vector<uint32> GetReqItemId() { return ReqItemId; }
        vector<uint32> GetReqItemCount() { return ReqItemCount; }
        vector<uint32> GetReqSourceId() { return ReqSourceId; }
        vector<uint32> GetReqSourceRef() { return ReqSourceRef; }
        vector<int32>  GetReqCreatureOrGOId() { return ReqCreatureOrGOId; }    // >0 Creature <0 Gameobject
        vector<uint32> GetReqCreatureOrGOCount() { return ReqCreatureOrGOCount; }
        vector<uint32> GetReqSpell() { return ReqSpell; }
        vector<uint32> GetRewChoiceItemId() { return RewChoiceItemId; }
        vector<uint32> GetRewChoiceItemCount() { return RewChoiceItemCount; }
        vector<uint32> GetRewItemId() { return RewItemId; }
        vector<uint32> GetRewItemCount() { return RewItemCount; }*/
        
        uint32 GetReqItemsCount() { return m_reqitemscount; }
        uint32 GetReqCreatureOrGOcount() { return m_reqCreatureOrGOcount; }
        uint32 GetRewChoiceItemsCount() { return m_rewchoiceitemscount; }
        uint32 GetRewItemsCount() { return m_rewitemscount; }

        vector<uint32> prevQuests;
    
    // cached data
    private:
        uint32 m_reqitemscount;
        uint32 m_reqCreatureOrGOcount;
        uint32 m_rewchoiceitemscount;
        uint32 m_rewitemscount;

        
    // table data
    protected:
        uint32 QuestId;
        uint32 ZoneId;
        uint32 QuestSort;
        uint32 MinLevel;
        uint32 QuestLevel;
        uint32 Type;
        uint32 RequiredRaces;
        uint32 RequiredClass;
        uint32 RequiredSkill;
        uint32 RequiredSkillValue;
        uint32 RequiredRepFaction;
        uint32 RequiredRepValue;
        uint32 LimitTime;
        uint32 SpecialFlags;
        uint32 PrevQuestId;
        uint32 NextQuestId;
        uint32 ExclusiveGroup;
        uint32 SrcItemId;
        uint32 SrcItemCount;
        uint32 SrcSpell;
        string Title;
        string Details;
        string Objectives;
        string OfferRewardText;
        string RequestItemsText;
        string EndText;
        uint32 RewRepFaction1;
        uint32 RewRepFaction2;
        int32  RewRepValue1;
        int32  RewRepValue2;
        int32  RewOrReqMoney;
        uint32 RewXP;
        uint32 RewSpell;
        uint32 PointMapId;
        float PointX;
        float PointY;
        uint32 PointOpt;
        uint32 OfferRewardEmote;
        uint32 RequestItemsEmote;
        uint32 QuestCompleteScript;
        uint32 HaveQuestId;
        uint32 Repeatable;
};

struct quest_status
{
    quest_status()
        : m_quest(NULL), m_status(QUEST_STATUS_NONE),m_rewarded(false),m_explored(false),
        m_completed_once(false), m_timer(0)
    {
        memset(m_itemcount,    0, QUEST_OBJECTIVES_COUNT * sizeof(uint32));
        memset(m_creatureOrGOcount, 0, QUEST_OBJECTIVES_COUNT * sizeof(uint32));
    }

    Quest *m_quest;

    QuestStatus m_status;
    bool m_rewarded;
    bool m_explored;
    bool m_completed_once;                                  // for repeatable quests
    uint32 m_timer;

    uint32 m_itemcount[ QUEST_OBJECTIVES_COUNT ];
    uint32 m_creatureOrGOcount[ QUEST_OBJECTIVES_COUNT ];
};
#endif
