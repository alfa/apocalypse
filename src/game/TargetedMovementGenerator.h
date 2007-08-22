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

#ifndef MANGOS_TARGETEDMOVEMENTGENERATOR_H
#define MANGOS_TARGETEDMOVEMENTGENERATOR_H

#include "MovementGenerator.h"
#include "DestinationHolder.h"
#include "Traveller.h"
#include "FollowerReference.h"

class MANGOS_DLL_SPEC TargetedMovementGenerator : public MovementGenerator
{
    public:

        TargetedMovementGenerator(Unit &target) : i_offset(0), i_angle(0)
        {
            i_target.link(&target, this);

            // volatile to prevent remove call at optimization
            volatile float size_dummy = target.GetObjectSize();
        }
        TargetedMovementGenerator(Unit &target, float offset, float angle) : i_offset(offset), i_angle(angle) { i_target.link(&target, this); }
        ~TargetedMovementGenerator() {}

        void Initialize(Creature &);
        void Reset(Creature &);
        bool Update(Creature &, const uint32 &);
        MovementGeneratorType GetMovementGeneratorType() { return TARGETED_MOTION_TYPE; }

        void stopFollowing() { };

    private:

        void _setTargetLocation(Creature &);

        FollowerReference i_target;
        float i_offset;
        float i_angle;
        DestinationHolder<Traveller<Creature> > i_destinationHolder;
};
#endif
