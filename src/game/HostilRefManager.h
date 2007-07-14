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

#ifndef _HOSTILEREFMANAGER
#define _HOSTILEREFMANAGER

#include "Common.h"
#include "RefManager.h"

class Unit;
class ThreatManager;
class HostilReference;
struct SpellEntry;

//=================================================

class HostilRefManager : public RefManager<Unit, ThreatManager>
{
private:
    Unit *iOwner;
public:
    void setOwner(Unit *pOwner) { iOwner = pOwner; }

    Unit* getOwner() { return iOwner; }

    // send threat to all my hateres for the pVictim
    // The pVictim is hated than by them as well
    // use for buffs and healing threat functionality
    void threatAssist(Unit *pVictim, float threat, uint8 school, SpellEntry const *threatSpell = 0, bool pSingleTarget=false);

    void addThreatPercent(int32 pValue);

    // The references are not needed anymore
    // tell the source to remove them from the list and free the mem
    void deleteReferences();

    HostilReference* getFirst() { return ((HostilReference* ) RefManager<Unit, ThreatManager>::getFirst()); }

    void updateThreatTables();

    void setOnlineOfflineState(bool pIsOnline);

};
//=================================================


#endif
