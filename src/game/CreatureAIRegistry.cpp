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

#include "CreatureAIRegistry.h"
#include "NullCreatureAI.h"
#include "ReactorAI.h"
#include "AggressorAI.h"
#include "RandomMovementGenerator.h"
#include "CreatureAIImpl.h"
#include "MovementGeneratorImpl.h"
#include "MapManager.h"
#include "WaypointMovementGenerator.h"
#include "RedZoneDistrict.h"
#include "CreatureAIRegistry.h"
#include "WaypointMovementGenerator.h"

namespace AIRegistry
{
    void Initialize()
    {
        (new CreatureAIFactory<NullCreatureAI>("NullAI"))->RegisterSelf();
        (new CreatureAIFactory<AggressorAI>("AggressorAI"))->RegisterSelf();
        (new CreatureAIFactory<ReactorAI>("ReactorAI"))->RegisterSelf();
        (new CreatureAIFactory<ReactorAI>("GuardAI"))->RegisterSelf();

        (new MovementGeneratorFactory<RandomMovementGenerator>("Random"))->RegisterSelf();
        (new MovementGeneratorFactory<WaypointMovementGenerator>("Waypoint"))->RegisterSelf();
    }
}

namespace MaNGOS
{
    namespace Game
    {
        void Initialize()
        {
            MapManager::Instance().Initialize();
            Map::InitStateMachine();
            RedZone::Initialize();
            AIRegistry::Initialize();
            WaypointMovementGenerator::Initialize();
        }
    }
}
