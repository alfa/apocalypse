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

#include "PetAI.h"
#include "Errors.h"
#include "Pet.h"
#include "Player.h"
#include "TargetedMovementGenerator.h"
#include "Database/DBCStores.h"
#include "Spell.h"

int PetAI::Permissible(const Creature *creature)
{
    if( creature->isPet())
        return PERMIT_BASE_SPECIAL;

    return PERMIT_BASE_NO;
}

PetAI::PetAI(Creature &c) : i_pet(c), i_pVictim(NULL), i_tracker(TIME_INTERVAL_LOOK)
{
    i_owner = ObjectAccessor::Instance().GetCreature(c, c.GetUInt64Value(UNIT_FIELD_SUMMONEDBY));
    if(!i_owner)
        i_owner = ObjectAccessor::Instance().GetPlayer(c, c.GetUInt64Value(UNIT_FIELD_SUMMONEDBY));
}

void PetAI::MoveInLineOfSight(Unit *u)
{
}

void PetAI::AttackStart(Unit *u)
{
    if( i_pVictim == NULL && u )
        _taggedToKill(u);
}

void PetAI::AttackStop(Unit *)
{

}

void PetAI::HealBy(Unit *healer, uint32 amount_healed)
{
}

void PetAI::DamageInflict(Unit *healer, uint32 amount_healed)
{
}

bool PetAI::IsVisible(Unit *pl) const
{
    return _isVisible(pl);
}

bool PetAI::_needToStop() const
{
    if( !i_pVictim->isAlive() || !i_pet.isAlive() || i_pVictim->m_stealth)
        return true;
    return false;
}

void PetAI::_stopAttack()
{
    assert( i_pVictim != NULL );
    i_pet.clearUnitState(UNIT_STAT_ATTACKING);
    i_pet.RemoveFlag(UNIT_FIELD_FLAGS, 0x80000 );

    if( !i_pet.isAlive() )
    {
        DEBUG_LOG("Creature stoped attacking cuz his dead [guid=%d]", i_pet.GetGUIDLow());
        i_pet.StopMoving();
        i_pet->Idle();
    }
    else if( !i_pVictim->isAlive() )
    {
        DEBUG_LOG("Creature stopped attacking cuz his victim is dead [guid=%d]", i_pet.GetGUIDLow());
        if(((Pet*)&i_pet)->HasActState(STATE_RA_FOLLOW))
            i_pet->Mutate(new TargetedMovementGenerator(*i_owner));
        else
            i_pet->Idle();
    }
    else if( i_pVictim->m_stealth )
    {
        DEBUG_LOG("Creature stopped attacking cuz his victim is stealth [guid=%d]", i_pet.GetGUIDLow());
        if(((Pet*)&i_pet)->HasActState( STATE_RA_FOLLOW))
            i_pet->Mutate(new TargetedMovementGenerator(*i_owner));
        else
            i_pet->Idle();
    }
    else
    {
        DEBUG_LOG("Creature stopped attacking due to target out run him [guid=%d]", i_pet.GetGUIDLow());
        if(((Pet*)&i_pet)->HasActState(STATE_RA_FOLLOW))
            i_pet->Mutate(new TargetedMovementGenerator(*i_owner));
        else
            i_pet->Idle();
    }
    i_pVictim = NULL;
}

void PetAI::UpdateAI(const uint32 diff)
{
    if( i_pVictim != NULL )
    {
        if( _needToStop() )
        {
            DEBUG_LOG("Guard AI stoped attacking [guid=%d]", i_pet.GetGUIDLow());
            _stopAttack();
        }
        else if( i_pet.IsStopped() )
        {
			Spell* spell;
			if ( !i_pet.m_canMove && i_pet.m_currentSpell )
				return;
			else if( ((Pet*)&i_pet)->HasActState(STATE_RA_AUTOSPELL) && i_pet.hasUnitState(UNIT_STAT_IN_COMBAT) && (spell = i_pet.reachWithSpellAttack(i_pVictim)))
			{
                SpellCastTargets targets;
                targets.setUnitTarget( i_pVictim );
                spell->prepare(&targets);
                i_pet.m_canMove = false;
                DEBUG_LOG("Spell Attack.");
			}
            else if( i_pet.isAttackReady() )
            {
                i_pet.AttackerStateUpdate(i_pVictim, 0);
                i_pet.setAttackTimer(0);
                if( !i_pet.isAlive() || !i_pVictim->isAlive() )
                    _stopAttack();
            }
        }
    }
    else
    {
        if(i_owner->hasUnitState(UNIT_STAT_IN_COMBAT) && i_owner->getAttackerSet().size())
            AttackStart(*(i_owner->getAttackerSet().begin()));
    }
}

bool PetAI::_isVisible(Unit *u) const
{
    return ( ((Creature*)&i_pet)->GetDistanceSq(u) * 1.0<= IN_LINE_OF_SIGHT && !u->m_stealth );
}

void PetAI::_taggedToKill(Unit *u)
{
    assert( i_pVictim == NULL );
    i_pet.addUnitState(UNIT_STAT_ATTACKING);
    i_pet.SetFlag(UNIT_FIELD_FLAGS, 0x80000);
    i_pVictim = u;
	Spell *spell;
	if( ((Pet*)&i_pet)->HasActState(STATE_RA_AUTOSPELL) && (spell = i_pet.reachWithSpellAttack( u )))
	{
        SpellCastTargets targets;
        targets.setUnitTarget( u );
        spell->prepare(&targets);
        i_pet.m_canMove = false;
        DEBUG_LOG("Spell Attack.");
	}
	else
		i_pet->Mutate(new TargetedMovementGenerator(*u));
}
