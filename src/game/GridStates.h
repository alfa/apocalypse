/* GridStates.h
 *
 * Copyright (C) 2005 MaNGOS <https://opensvn.csie.org/traccgi/MaNGOS/trac.cgi/>
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

#ifndef MANGOS_GRIDSTATES_H
#define MANGOS_GRIDSTATES_H

/**
 * @page GridStates is a state manchine for the grid system. It implement using the
 * state pattern.  The grid has serveral states and each transitional state is done
 * by the state manchine.  At each state, each action result in different results.
 */

#include "Map.h"

/** GridState is an api for the state machine.  The transition from each state
 * for a particular action is state implementation dependent.
 */
class MANGOS_DLL_DECL GridState
{
    public:

/// Update grid state
        virtual void Update(Map &, GridType&, GridInfo &, const uint32 &x, const uint32 &y, const uint32 &t_diff) const = 0;
};

class MANGOS_DLL_DECL InvalidState : public GridState
{
    public:

        void Update(Map &, GridType &, GridInfo &, const uint32 &x, const uint32 &y, const uint32 &) const;
};

class MANGOS_DLL_DECL ActiveState : public GridState
{
    public:

        void Update(Map &, GridType&, GridInfo &, const uint32 &x, const uint32 &y, const uint32 &) const;
};

class MANGOS_DLL_DECL IdleState : public GridState
{
    public:

        void Update(Map &, GridType &, GridInfo &, const uint32 &x, const uint32 &y, const uint32 &) const;
};

class MANGOS_DLL_DECL RemovalState : public GridState
{
    public:

        void Update(Map &, GridType &, GridInfo &, const uint32 &x, const uint32 &y, const uint32 &) const;
};
#endif
