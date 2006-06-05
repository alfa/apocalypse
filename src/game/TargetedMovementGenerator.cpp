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
#include "TargetedMovementGenerator.h"
#include "Errors.h"
#include "Creature.h"
#include "MapManager.h"
#include "Spell.h"

#define SMALL_ALPHA 0.05

#include <cmath>

struct StackCleaner
{
    Creature &i_creature;
    StackCleaner(Creature &creature) : i_creature(creature) {}
    void Done(void) { i_creature.StopMoving(); }
    ~StackCleaner()
    {
        i_creature->Clear();
    }
};

void
TargetedMovementGenerator::_setTargetLocation(Creature &owner, float offset)
{
    if(!&i_target || !&owner)
        return;
    owner.SetInFront(&i_target);
    float x, y, z;
    i_target.GetContactPoint( &owner, x, y, z );
    Traveller<Creature> traveller(owner);
    //i_destinationHolder.SetDestination(traveller, x, y, z, (owner.GetObjectSize() + i_target.GetObjectSize()));
    i_destinationHolder.SetDestination(traveller, x, y, z, (owner.GetObjectSize() + i_target.GetObjectSize()) + offset);
}

void
TargetedMovementGenerator::_setAttackRadius(Creature &owner)
{
    if(!&owner)
        return;
    float combat_reach = owner.GetFloatValue(UNIT_FIELD_COMBATREACH);
    if( combat_reach <= 0.0f )
        combat_reach = 1.0f;
    //float bounding_radius = owner.GetFloatValue(UNIT_FIELD_BOUNDINGRADIUS);
    i_attackRadius = combat_reach;                          // - SMALL_ALPHA);
}

void
TargetedMovementGenerator::Initialize(Creature &owner)
{
    if(!&owner)
        return;
    owner.setMoveRunFlag(true);
    _setAttackRadius(owner);
    _setTargetLocation(owner, 0);
    owner.addUnitState(UNIT_STAT_CHASE);
}

void
TargetedMovementGenerator::Reset(Creature &owner)
{
    Initialize(owner);
}

void
TargetedMovementGenerator::TargetedHome(Creature &owner)
{
    if(!&owner)
        return;
    DEBUG_LOG("Target home location %u", owner.GetGUIDLow());
    float x, y, z;
    owner.GetRespawnCoord(x, y, z);
    Traveller<Creature> traveller(owner);
    i_destinationHolder.SetDestination(traveller, x, y, z);
    i_targetedHome = true;
    owner.clearUnitState(UNIT_STAT_ALL_STATE);
    owner.addUnitState(UNIT_STAT_FLEEING);
}

void
TargetedMovementGenerator::Update(Creature &owner, const uint32 & time_diff)
{
    if(!&owner || !&i_target)
        return;
    if(owner.hasUnitState(UNIT_STAT_ROOT))
        return;

    if(!owner.isAlive())
        return;

    SpellEntry* spellInfo;
    if( owner.IsStopped() && i_target.isAlive())
    {
        if(!owner.hasUnitState(UNIT_STAT_FOLLOW) && owner.isInCombat())
        {
            if( spellInfo = owner.reachWithSpellAttack( &i_target))
            {
                _spellAtack(owner, spellInfo);
                return;
            }
        }
        if( !owner.canReachWithAttack( &i_target ) )
        {
            owner.addUnitState(UNIT_STAT_CHASE);
            _setTargetLocation(owner, 0);
            DEBUG_LOG("restart to chase");
        }
        //if (!owner.HasInArc( 2.0943951024, &i_target )) {
        //        if (!owner.HasInArc( (30/360) * 2 * M_PI, &i_target )) {
        // adjust facing
        /*        if ((i_target.GetDistance2dSq(&owner) > (owner.GetObjectSize() + i_target.GetObjectSize())) && (i_destinationHolder.HasArrived())) {
                    _setTargetLocation(owner, 0);
                }*/
        //if ((i_target.GetDistance2dSq(&owner) < (owner.GetObjectSize() * 2 + i_target.GetObjectSize())) && (i_destinationHolder.HasArrived())) {

        if ( i_target.GetDistanceSq(&owner) > 0 )
            _setTargetLocation(owner, 0);
        else if ( !i_target.HasInArc( 0.1, &owner ) )
            owner.SetInFront(&i_target);
        //            else  {
        /*float ang = owner.GetAngle(&i_target);
        ang -= owner.GetOrientation();
        if (ang > (2.0f * M_PI))
            ang -= 2.0f * M_PI;
        if (ang < (2.0f * M_PI * -1))
            ang += 2.0f * M_PI;
        if (ang <= M_PI) {
            // Facing different directions
            ang = ang;
        }*/
        //          }
        //if (i_target.GetDistance2dSq(&owner) == 0)
        //    _setTargetLocation(owner, -1 * (owner.GetObjectSize() + i_target.GetObjectSize()));

        //    sLog.outString("try to back up");
        // }
    }
    else
    {
        Traveller<Creature> traveller(owner);
        bool reach = i_destinationHolder.UpdateTraveller(traveller, time_diff, false);
        if(i_targetedHome)
            return;
        else if(!owner.hasUnitState(UNIT_STAT_FOLLOW) && owner.isInCombat() && (spellInfo = owner.reachWithSpellAttack(&i_target)) )
        {
            _spellAtack(owner, spellInfo);
            return;
        }
        if(reach)
        {
            if( owner.canReachWithAttack(&i_target) )
            {
                owner.SetInFront(&i_target);
                owner.StopMoving();
                if(!owner.hasUnitState(UNIT_STAT_FOLLOW))
                {
                    //owner.addUnitState(UNIT_STAT_ATTACKING);
                    owner.Attack(&i_target);                //??
                }
                owner.clearUnitState(UNIT_STAT_CHASE);
                DEBUG_LOG("UNIT IS THERE");
            }
            else
            {
                _setTargetLocation(owner, 0);
                DEBUG_LOG("Continue to chase");
            }
        }
    }
}

void TargetedMovementGenerator::_spellAtack(Creature &owner, SpellEntry* spellInfo)
{
    if(!spellInfo)
        return;
    owner.StopMoving();
    owner->Idle();
    if(owner.m_currentSpell)
    {
        if(owner.m_currentSpell->m_spellInfo->Id == spellInfo->Id )
            return;
        else
        {
            delete owner.m_currentSpell;
            owner.m_currentSpell = NULL;
        }
    }
    Spell *spell = new Spell(&owner, spellInfo, false, 0);
    spell->SetAutoRepeat(true);
    //owner.addUnitState(UNIT_STAT_ATTACKING);
    owner.Attack(&owner);                                   //??
    owner.clearUnitState(UNIT_STAT_CHASE);
    SpellCastTargets targets;
    targets.setUnitTarget( &i_target );
    spell->prepare(&targets);
    owner.m_canMove = false;
    DEBUG_LOG("Spell Attack.");
}
