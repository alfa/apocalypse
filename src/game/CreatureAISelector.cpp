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

#include "CreatureAIImpl.h"
#include "CreatureAISelector.h"
#include "NullCreatureAI.h"
#include "ObjectMgr.h"
#include "Policies/SingletonImp.h"
#include "MovementGenerator.h"

INSTANTIATE_SINGLETON_1(CreatureAIRegistry);
INSTANTIATE_SINGLETON_1(MovementGeneratorRegistry);

namespace FactorySelector
{
    CreatureAI* selectAI(Creature *creature)
    {
	CreatureAIRegistry &ai_registry(CreatureAIRepository::Instance());
	assert( creature->GetCreatureInfo() != NULL );
	const CreatureAICreator *ai_factory = ai_registry.GetRegistryItem( creature->GetCreatureInfo()->AIName);

	if( ai_factory == NULL  )
	{
	    int best_val = -1;
	    std::vector<std::string> l;
	    ai_registry.GetRegisteredItems(l);
	    for( std::vector<std::string>::iterator iter = l.begin(); iter != l.end(); ++iter)
	    {
		const CreatureAICreator *factory = ai_registry.GetRegistryItem((*iter).c_str());
		const SelectableAI *p = dynamic_cast<const SelectableAI *>(factory);
		assert( p != NULL );
		int val = p->Permit(creature);
		if( val > best_val )
		{
		    best_val = val;
		    ai_factory = p;
		}
	    }	    
	}

	return ( ai_factory == NULL ? new NullCreatureAI : ai_factory->Create(creature) );
    }

    MovementGenerator* selectMovementGenerator(Creature *creature)
    {
	MovementGeneratorRegistry &mv_registry(MovementGeneratorRepository::Instance());
	assert( creature->GetCreatureInfo() != NULL );	
	const MovementGeneratorCreator *mv_factory = mv_registry.GetRegistryItem( creature->GetCreatureInfo()->MovementGen);

	/* if( mv_factory == NULL  )
	{
	    int best_val = -1;
	    std::vector<std::string> l;
	    mv_registry.GetRegisteredItems(l);
	    for( std::vector<std::string>::iterator iter = l.begin(); iter != l.end(); ++iter)
	    {
		const MovementGeneratorCreator *factory = mv_registry.GetRegistryItem((*iter).c_str());
		const SelectableMovement *p = dynamic_cast<const SelectableMovement *>(factory);
		assert( p != NULL );
		int val = p->Permit(creature);
		if( val > best_val )
		{
		    best_val = val;
		    mv_factory = p;
		}
	    }	    
	}*/

	return ( mv_factory == NULL ? NULL : mv_factory->Create(creature) );

    }
}

