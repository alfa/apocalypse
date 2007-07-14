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

#include "PetAI.h"
#include "Errors.h"
#include "Pet.h"
#include "Player.h"
#include "TargetedMovementGenerator.h"
#include "Database/DBCStores.h"
#include "Spell.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Creature.h"

int PetAI::Permissible(const Creature *creature)
{
    if( creature->isPet())
        return PERMIT_BASE_SPECIAL;

    return PERMIT_BASE_NO;
}

PetAI::PetAI(Creature &c) : i_pet(c), i_victimGuid(0), i_tracker(TIME_INTERVAL_LOOK)
{
    m_AllySet.clear();
	UpdateAllies();
	m_updateAlliesTimer = 10000;	//update friendly targets every 10 seconds, lesser checks increase performance
}

void PetAI::MoveInLineOfSight(Unit *u)
{
    if( !i_pet.getVictim() && (i_pet.isPet() && ((Pet&)i_pet).HasReactState(REACT_AGGRESSIVE) || i_pet.isCharmed()) &&
        u->isTargetableForAttack() && i_pet.IsHostileTo( u )  &&
        u->isInAccessablePlaceFor(&i_pet))
    {
        float attackRadius = i_pet.GetAttackDistance(u);
        if(i_pet.IsWithinDistInMap(u, attackRadius) && i_pet.GetDistanceZ(u) <= CREATURE_Z_ATTACK_RANGE)
        {
            AttackStart(u);
            if(u->HasStealthAura())
                u->RemoveSpellsCausingAura(SPELL_AURA_MOD_STEALTH);
        }
    }
}

void PetAI::AttackStart(Unit *u)
{
    if( i_pet.getVictim() || !u || i_pet.isPet() && ((Pet&)i_pet).getPetType()==MINI_PET )
        return;

    DEBUG_LOG("Start to attack");
    if(i_pet.Attack(u))
    {
        i_pet.clearUnitState(UNIT_STAT_FOLLOW);
        i_pet->Clear();
        i_victimGuid = u->GetGUID();
        i_pet->Mutate(new TargetedMovementGenerator(*u));
    }
}

void PetAI::EnterEvadeMode()
{
}

bool PetAI::IsVisible(Unit *pl) const
{
    return _isVisible(pl);
}

bool PetAI::_needToStop() const
{
    if(!i_pet.getVictim() || !i_pet.isAlive())
        return true;

    // This is needed for charmed creatures, as once their target was reset other effects can trigger threat
    if(i_pet.isCharmed() && i_pet.getVictim() == i_pet.GetCharmer())
        return true;

    return !i_pet.getVictim()->isTargetableForAttack();
}

void PetAI::_stopAttack()
{
    if( !i_victimGuid )
        return;

    Unit* victim = ObjectAccessor::Instance().GetUnit(i_pet, i_victimGuid );

    if ( !victim )
        return;

    assert(!i_pet.getVictim() || i_pet.getVictim() == victim);

    if( !i_pet.isAlive() )
    {
        DEBUG_LOG("Creature stoped attacking cuz his dead [guid=%u]", i_pet.GetGUIDLow());
        i_pet.StopMoving();
        i_pet->Clear();
        i_pet->Idle();
        i_victimGuid = 0;
        i_pet.CombatStop(true);
        i_pet.DeleteInHateListOf();
        return;
    }
    else if( !victim  )
    {
        DEBUG_LOG("Creature stopped attacking because victim is non exist [guid=%u]", i_pet.GetGUIDLow());
    }
    else if( !victim->isAlive() )
    {
        DEBUG_LOG("Creature stopped attacking cuz his victim is dead [guid=%u]", i_pet.GetGUIDLow());
    }
    else if( victim->HasStealthAura() )
    {
        DEBUG_LOG("Creature stopped attacking cuz his victim is stealth [guid=%u]", i_pet.GetGUIDLow());
    }
    else if( victim->isInFlight() )
    {
        DEBUG_LOG("Creature stopped attacking cuz his victim is fly away [guid=%u]", i_pet.GetGUIDLow());
    }
    else
    {
        DEBUG_LOG("Creature stopped attacking due to target out run him [guid=%u]", i_pet.GetGUIDLow());
    }

    Unit* owner = i_pet.GetCharmerOrOwner();

    if(owner && (i_pet.isCharmed() || ((Pet*)&i_pet)->HasCommandState(COMMAND_FOLLOW)))	//charms always follow, pets can be set
    {
        i_pet.addUnitState(UNIT_STAT_FOLLOW);
        i_pet->Clear();
        i_pet->Mutate(new TargetedMovementGenerator(*owner,PET_FOLLOW_DIST,PET_FOLLOW_ANGLE));
    }
    else
    {
        i_pet.clearUnitState(UNIT_STAT_FOLLOW);
        i_pet.addUnitState(UNIT_STAT_STOPPED);
        i_pet->Clear();
        i_pet->Idle();
    }
    i_victimGuid = 0;
    i_pet.AttackStop();
}

void PetAI::UpdateAI(const uint32 diff)
{
    // update i_victimGuid if i_pet.getVictim() !=0 and changed
    if(i_pet.getVictim())
        i_victimGuid = i_pet.getVictim()->GetGUID();

    Unit* owner = i_pet.GetCharmerOrOwner();
    
    if(m_updateAlliesTimer <= diff)
    {
		UpdateAllies();
       	m_updateAlliesTimer = 10000;
    }
 	else
   		m_updateAlliesTimer -= diff;

    // i_pet.getVictim() can't be used for check in case stop fighting, i_pet.getVictim() clear�� at Unit death etc.
    if( i_victimGuid )
    {
        if( _needToStop() )
        {
            DEBUG_LOG("Pet AI stoped attacking [guid=%u]", i_pet.GetGUIDLow());
            _stopAttack();                                  // i_victimGuid == 0 && i_pet.getVictim() == NULL now
            return;
        }
        else if( i_pet.IsStopped() || i_pet.IsWithinDistInMap(i_pet.getVictim(), ATTACK_DISTANCE))
        {
            // required to be stopped cases
            if ( i_pet.IsStopped() && i_pet.m_currentSpell )
            {
                if( i_pet.hasUnitState(UNIT_STAT_FOLLOW) )
                    i_pet.m_currentSpell->cancel();
                else
                    return;
            }
            // not required to be stopped case
            else if( i_pet.isAttackReady() && i_pet.canReachWithAttack(i_pet.getVictim()) )
            {
                i_pet.AttackerStateUpdate(i_pet.getVictim());

                i_pet.resetAttackTimer();

                if ( !i_pet.getVictim() )
                    return;

                //if pet misses its target, it will also be the first in threat list
                i_pet.getVictim()->AddThreat(&i_pet,0.0f);

                if( _needToStop() )
                    _stopAttack();
            }
        }
    }
    else if(owner)
    {
        if(owner->isInCombat() && (i_pet.isCharmed() || !((Pet*)&i_pet)->HasReactState(REACT_PASSIVE)))	//charms always help automatically?
        {
            AttackStart(owner->getAttackerForHelper());
        }
        else if(i_pet.isCharmed() || ((Pet*)&i_pet)->HasCommandState(COMMAND_FOLLOW))
        {
            if (!i_pet.hasUnitState(UNIT_STAT_FOLLOW) )
            {
                i_pet.addUnitState(UNIT_STAT_FOLLOW);
                i_pet->Clear();
                i_pet->Mutate(new TargetedMovementGenerator(*owner,PET_FOLLOW_DIST,PET_FOLLOW_ANGLE));
            }
        }
    }
    
    //Autocast
	HM_NAMESPACE::hash_map<uint32, Unit*> targetMap;
	targetMap.clear();
	SpellCastTargets NULLtargets;

	if(i_pet.isPet())	//it's a pet, so it has an autospellmap and an allyset
	{
	    for(AutoSpellList::iterator itr = ((Pet*)&i_pet)->m_autospells.begin(); itr != ((Pet*)&i_pet)->m_autospells.end(); ++itr)
		{
			SpellEntry const *spellInfo = sSpellStore.LookupEntry(*itr);
			if(!spellInfo)
				continue;
			
			Spell *spell = new Spell(&i_pet, spellInfo, false, 0);
		    WPAssert(spell);

			if(!IsPositiveSpell(spellInfo->Id) && i_pet.getVictim() && !_needToStop() && !i_pet.hasUnitState(UNIT_STAT_FOLLOW) && spell->CanAutoCast(i_pet.getVictim()))
				targetMap[*itr] = i_pet.getVictim();
			else
			{
				spell->m_targets = NULLtargets;
				for(std::set<uint64>::iterator tar = m_AllySet.begin(); tar != m_AllySet.end(); ++tar)
				{
					Unit* Target = ObjectAccessor::Instance().GetUnit(i_pet,*tar);

					if(!Target || (!Target->isInCombat() && !IsNonCombatSpell(*itr)))	//only buff targets that are in combat, unless the spell can only be cast while out of combat
						continue;
					if(spell->CanAutoCast(Target))
						targetMap[*itr] = Target;
				}
			}
		}
	}
	else if(i_pet.isCharmed()) //charmed creature; all (active) spells autocast, because not controllable; for now no allyset, simply itself (selfcast spells allowed) and target
	{
		for(uint8 i = 0; i < CREATURE_MAX_SPELLS; ++i)
		{
			uint32 spell_id = i_pet.m_spells[i];
			SpellEntry const *spellInfo = sSpellStore.LookupEntry(spell_id);
			if(!spellInfo || IsPassiveSpell(spell_id))
				continue;

			Spell *spell = new Spell(&i_pet, spellInfo, false, 0);
			    WPAssert(spell);

			if(!IsPositiveSpell(spellInfo->Id) && i_pet.getVictim() && !_needToStop() && !i_pet.hasUnitState(UNIT_STAT_FOLLOW) && spell->CanAutoCast(i_pet.getVictim()))
				targetMap[spell_id] = i_pet.getVictim();
			else
			{
				spell->m_targets = NULLtargets;
				for(std::set<uint64>::iterator tar = m_AllySet.begin(); tar != m_AllySet.end(); ++tar)
				{
					Unit* Target = ObjectAccessor::Instance().GetUnit(i_pet,*tar);

					if(!Target || (!Target->isInCombat() && !IsNonCombatSpell(spell_id)))	//only buff targets that are in combat, unless the spell can only be cast while out of combat
						continue;
					if(spell->CanAutoCast(Target))
						targetMap[spell_id] = Target;
				}
			}
			/*else if(!i_pet.isInCombat() && !IsNonCombatSpell(spell_id))
			{
				spell->m_targets = NULLtargets;
				if(spell->CanAutoCast(&i_pet))	//only buff targets that are in combat, unless the spell can only be cast while out of combat
					targetMap[spell_id] = &i_pet;
			}*/
		}
	}

	if(targetMap.size() > 0)//found units to cast on to
	{
		uint32 index = urand(1, targetMap.size());
		HM_NAMESPACE::hash_map<uint32, Unit*>::iterator itr;
		uint32 i;
		for(itr = targetMap.begin(), i = 1; i < index; ++itr, ++i);

		SpellEntry const *spellInfo = sSpellStore.LookupEntry(itr->first);

        Spell *spell = new Spell(&i_pet, spellInfo, false, 0);
        WPAssert(spell);

   	    SpellCastTargets targets;
       	targets.setUnitTarget( itr->second );
       	if(!i_pet.HasInArc(M_PI, itr->second))
       	{
       		i_pet.SetInFront(itr->second);
			if( itr->second->GetTypeId() == TYPEID_PLAYER )
                i_pet.SendUpdateToPlayer( (Player*)itr->second );
   	        if(owner && owner->GetTypeId() == TYPEID_PLAYER)
       	    	i_pet.SendUpdateToPlayer( (Player*)owner );
        }

		i_pet.AddCreatureSpellCooldown(itr->first);
		((Pet*)&i_pet)->CheckLearning(itr->first);

        spell->prepare(&targets);
	}
	targetMap.clear();
}

bool PetAI::_isVisible(Unit *u) const
{
    return false;                                           //( ((Creature*)&i_pet)->GetDistanceSq(u) * 1.0<= sWorld.getConfig(CONFIG_SIGHT_GUARDER) && !u->m_stealth && u->isAlive());
}

void PetAI::UpdateAllies()
{
	Unit* owner = i_pet.GetCharmerOrOwner();
	Group *pGroup = NULL;

	if(!owner)
		return;
	else if(owner->GetTypeId() == TYPEID_PLAYER)
		pGroup = ((Player*)owner)->groupInfo.group;

   	if(m_AllySet.size() == 2 && !pGroup) //only pet and owner/not in group->ok
   		return;
   	else if(pGroup && !pGroup->isRaidGroup() && m_AllySet.size() == (pGroup->GetMembersCount() + 2)) //owner is in group; group members filled in already (no raid -> subgroupcount = whole count)
   		return;
   	else
   	{
		m_AllySet.clear();
		m_AllySet.insert(i_pet.GetGUID());
		if(pGroup)	//add group
		{
			Group::MemberList const& members = pGroup->GetMembers();
            for(Group::member_citerator itr = members.begin(); itr != members.end(); ++itr)
            {
                if(!pGroup->SameSubGroup(owner->GetGUID(), &*itr))
                    continue;

                Unit* Target = objmgr.GetPlayer(itr->guid);
                if(!Target || Target->GetGUID() == owner->GetGUID())
                    continue;
                    
                m_AllySet.insert(Target->GetGUID());
            }
		}
		else	//remove group
			m_AllySet.insert(owner->GetGUID());
	}

}
