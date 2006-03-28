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

#include "ByteBuffer.h" 
#include "ReactorAI.h"
#include "Errors.h"
#include "Creature.h"
#include "TargetedMovementGenerator.h"
#include "FactionTemplateResolver.h"
#include "Log.h"

#define REACTOR_VISIBLE_RANGE (700.0f)   

int
ReactorAI::Permissible(const Creature *creature)
{
    FactionTemplateEntry *fact = sFactionTemplateStore.LookupEntry(creature->GetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE));
    FactionTemplateResolver fact_source(fact);
    if( fact_source.IsNeutralToAll() )
	return REACTIVE_PERMIT_BASE;

    return NO_PERMIT;
}


void 
ReactorAI::MoveInLineOfSight(Unit *) 
{
}

void 
ReactorAI::AttackStart(Unit *p) 
{
    if( i_pVictim == NULL )
    {
	DEBUG_LOG("Tag unit LowGUID(%d) HighGUID(%d) as a victim", p->GetGUIDLow(), p->GetGUIDHigh());
	i_creature.SetState(ATTACKING);
	i_creature.SetFlag(UNIT_FIELD_FLAGS, 0x80000);
	i_creature->Mutate(new TargetedMovementGenerator(*p));
	i_pVictim = p; 
    }
}

void 
ReactorAI::AttackStop(Unit *) 
{
    
}

void 
ReactorAI::HealBy(Unit *healer, uint32 amount_healed) 
{
}

void 
ReactorAI::DamageInflict(Unit *healer, uint32 amount_healed) 
{
}

bool
ReactorAI::IsVisible(Unit *pl) const 
{
    return false; 
}

void
ReactorAI::UpdateAI(const uint32 time_diff)
{
    
    if( i_pVictim != NULL )
    {
	if( needToStop() )
	{
	    DEBUG_LOG("Creature %d stopped attacking.", i_creature.GetGUIDLow());
	    stopAttack();
	}
	else if( i_creature.IsStopped() )
	{
	    if( i_creature.isAttackReady() )
	    {
		i_creature.AttackerStateUpdate(i_pVictim, 0);
		i_creature.setAttackTimer(0); 
		if( !i_creature.isAlive() || !i_pVictim->isAlive() )
		    stopAttack();
	    }
	}
    }
}

bool
ReactorAI::needToStop() const
{
    if( !i_pVictim->isAlive() || !i_creature.isAlive()  || i_pVictim->m_stealth)
	return true;

    float length_square = i_creature.GetDistanceSq(i_pVictim);
    if( length_square > REACTOR_VISIBLE_RANGE )
	return true;
    return false;
}

void
ReactorAI::stopAttack()
{
    if( i_pVictim != NULL )
    {
	i_creature.ClearState(ATTACKING);
	i_creature.RemoveFlag(UNIT_FIELD_FLAGS, 0x80000 );

	if( !i_creature.isAlive() )
	{
	    DEBUG_LOG("Creature stoped attacking cuz his dead [guid=%d]", i_creature.GetGUIDLow());
	    i_creature->Idle();
	}
	else if( i_pVictim->m_stealth )
  {
	DEBUG_LOG("Creature stopped attacking cuz his victim is stealth [guid=%d]", i_creature.GetGUIDLow());
	i_pVictim = NULL;
	static_cast<TargetedMovementGenerator *>(i_creature->top())->TargetedHome(i_creature); 
  }
	else 
	{
	    DEBUG_LOG("Creature stopped attacking due to target %s [guid=%d]", i_pVictim->isAlive() ? "out run him" : "is dead", i_creature.GetGUIDLow());
	    static_cast<TargetedMovementGenerator *>(i_creature->top())->TargetedHome(i_creature); 
	}

	i_pVictim = NULL;
    }
}
