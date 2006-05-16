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

#include "QuestDef.h"
#include "ObjectMgr.h"
#include "ItemPrototype.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"

Quest::Quest()
{
    m_quest=new QuestInfo;
    m_qReqItemsCount=0;
    m_qReqMobsCount=0;
    m_qRewChoiceItemsCount=0;
    m_qRewItemsCount=0;
}

void Quest::LoadQuest(uint32 quest_id)
{
    QuestInfo *qi=objmgr.GetQuestInfo(quest_id);
    if(qi!=NULL)
        LoadQuest(qi);
}

void Quest::LoadQuest(QuestInfo *questinfo)
{
    if(!questinfo)
        return;
    m_quest=questinfo;
    m_qReqItemsCount=0;
    m_qReqMobsCount=0;
    m_qRewItemsCount=0;
    m_qRewChoiceItemsCount=0;
    for (int i=0; i < QUEST_REWARD_CHOICES_COUNT; i++)
    {
        if(i<QUEST_OBJECTIVES_COUNT)
        {
            if (questinfo->ReqItemId[i])
                m_qReqItemsCount++;
            if (questinfo->ReqKillMobId[i])
                m_qReqMobsCount++;
            if (questinfo->RewItemId[i])
                m_qRewItemsCount++;
        }
        if ( questinfo->RewChoiceItemId[i])
            m_qRewChoiceItemsCount++;
    }
}

uint32 Quest::XPValue(Player* _Player)
{
    uint32 fullxp = GetQuestInfo()->RewXP;
    if( fullxp<=0 )
        return 0;
    uint32 playerlvl = _Player->getLevel();
    uint32 questlvl = GetQuestInfo()->QuestLevel;
    if(playerlvl <= questlvl +  5 )
        return fullxp;
    else if(playerlvl == questlvl +  6 )
        return (uint32)((fullxp * 0.8f / 5.0f) * 5);
    else if(playerlvl == questlvl +  7 )
        return (uint32)((fullxp * 0.6f / 5.0f) * 5);
    else if(playerlvl == questlvl +  8 )
        return (uint32)((fullxp * 0.4f / 5.0f) * 5);
    else if(playerlvl == questlvl +  9 )
        return (uint32)((fullxp * 0.2f / 5.0f) * 5);
    return (uint32)((fullxp * 0.1f / 5.0f) * 5);
}

bool Quest::CanBeTaken( Player *_Player )
{
    return ( !RewardIsTaken( _Player ) && IsCompatible( _Player ) && LevelSatisfied( _Player ) && PreReqSatisfied( _Player ) );
}

bool Quest::RewardIsTaken( Player *_Player )
{
	return _Player->getQuestRewardStatus( GetQuestInfo()->QuestId );
}

bool Quest::IsCompatible( Player *_Player )
{
    return ( ReputationSatisfied ( _Player ) && RaceSatisfied ( _Player ) && ClassSatisfied ( _Player ) && TradeSkillSatisfied ( _Player ) );
}

bool Quest::ReputationSatisfied( Player *_Player )
{
    return true;
}

bool Quest::TradeSkillSatisfied( Player *_Player )
{
    return true;
}

bool Quest::RaceSatisfied( Player *_Player )
{
    if ( GetQuestInfo()->RequiredRaces == QUEST_RACE_NONE )
		return true;
    return (((GetQuestInfo()->RequiredRaces >> (_Player->getRace() - 1)) & 0x01) == 0x01);
}

bool Quest::ClassSatisfied( Player *_Player )
{
    if ( GetQuestInfo()->RequiredClass == QUEST_CLASS_NONE )
		return true;
    return (GetQuestInfo()->RequiredClass == _Player->getClass());
}

bool Quest::LevelSatisfied( Player *_Player )
{
    return ( _Player->getLevel() >= GetQuestInfo()->MinLevel );
}

bool Quest::CanShowUnsatified( Player *_Player )
{
    uint8 iPLevel;
    iPLevel = _Player->getLevel();
    return ( iPLevel >= GetQuestInfo()->MinLevel - 7 && iPLevel < GetQuestInfo()->MinLevel );
}

bool Quest::PreReqSatisfied( Player *_Player )
{
    if (GetQuestInfo()->PrevQuestId > 0  && !_Player->getQuestRewardStatus(  GetQuestInfo()->PrevQuestId ))
        return false;
    return true;
}
bool Quest::AddSrcItem( Player *_Player )
{
    uint32 srcitem = GetQuestInfo()->SrcItemId;
    if ( srcitem )
        if ( _Player->AddNewItem(srcitem,GetQuestInfo()->SrcItemCount,false) == 0 )
            return false;
        else
            return true;
    return true;
}
bool Quest::RemSrcItem( Player *_Player )
{
    uint32 srcitem = GetQuestInfo()->SrcItemId;
    if ( srcitem )
        _Player->RemoveItemFromInventory(srcitem, GetQuestInfo()->SrcItemCount);
    return true;
}
