/* 
 * Copyright (C) 2005 MaNGOS <http://www.magosproject.org/>
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


class Unit;

class MANGOS_DLL_DECL TargetedMovementGenerator : public MovementGenerator
{
public:

    TargetedMovementGenerator(Unit &target) : i_target(target), i_targetedHome(false), i_attackRadius(0) {}

    
    void Initialize(Creature &);
    void Reset(Creature &);
    void Update(Creature &, const uint32 &);

    
    void TargetedHome(Creature &);

private:

    void _setAttackRadius(Creature &);
    void _setTargetLocation(Creature &);
    Unit &i_target;
    float i_attackRadius;
    bool i_targetedHome; 
    DestinationHolder i_destinationHolder;
};

#endif
