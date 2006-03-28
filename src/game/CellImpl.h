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

#ifndef MANGOS_CELLIMPL_H
#define MANGOS_CELLIMPL_H

#include "Cell.h"
#include "Map.h"
#include "RedZoneDistrict.h"
#include <cmath>


template<class LOCK_TYPE,class T, class CONTAINER> 
inline void 
Cell::Visit(const CellLock<LOCK_TYPE> &l, TypeContainerVisitor<T, CONTAINER> &visitor, Map &m) const
{
    CellPair standing_cell = (const CellPair &)l;
    CellPair cell_iter;

    switch( (district_t)this->data.Part.reserved )
    {
    case ALL_DISTRICT:
	{
	    CellPair update_cell(standing_cell);
	    update_cell << 1;
	    update_cell -= 1;
	    for(; abs(int(standing_cell.x_coord - update_cell.x_coord)) < 2; update_cell >> 1)
	    {
		for(cell_iter=update_cell; abs(int(standing_cell.y_coord - cell_iter.y_coord)) < 2; cell_iter += 1)
		{		    
		    Cell r_zone = RedZone::GetZone(cell_iter);
		    r_zone.data.Part.nocreate = l->data.Part.nocreate;
		    CellLock<LOCK_TYPE> lock(r_zone, cell_iter);
		    m.Visit(lock, visitor);
		}
	    }
	    break;
	}
    case UPPER_LEFT_DISTRICT:
	{
	    CellPair update_cell(standing_cell);
	    standing_cell << 1;
	    standing_cell -= 1;

	    
	    for(cell_iter = update_cell; abs(int(standing_cell.x_coord - cell_iter.x_coord)) < 2; cell_iter >> 1)
	    {
		Cell r_zone = RedZone::GetZone(cell_iter);
		r_zone.data.Part.nocreate = l->data.Part.nocreate;
		CellLock<LOCK_TYPE> lock(r_zone, cell_iter);
		m.Visit(lock, visitor);
	    }

	    
	    for(cell_iter=update_cell, cell_iter += 1; abs(int(standing_cell.y_coord - cell_iter.y_coord)) < 2; cell_iter += 1)
	    {
		Cell r_zone = RedZone::GetZone(cell_iter);
		r_zone.data.Part.nocreate = l->data.Part.nocreate;
		CellLock<LOCK_TYPE> lock(r_zone, cell_iter);
		m.Visit(lock, visitor);
	    }
	    break;
	}
    case UPPER_RIGHT_DISTRICT:
	{
	    CellPair update_cell(standing_cell);
	    update_cell >> 1;
	    update_cell -= 1;

	    
	    for(cell_iter = update_cell; abs(int(standing_cell.x_coord - cell_iter.x_coord)) < 2; cell_iter << 1)
	    {
		Cell r_zone = RedZone::GetZone(cell_iter);
		r_zone.data.Part.nocreate = l->data.Part.nocreate;
		CellLock<LOCK_TYPE> lock(r_zone, cell_iter);
		m.Visit(lock, visitor);
	    }

	    
	    for(cell_iter=update_cell, cell_iter += 1; abs(int(standing_cell.y_coord - cell_iter.y_coord)) < 2; cell_iter += 1)
	    {
		Cell r_zone = RedZone::GetZone(cell_iter);
		r_zone.data.Part.nocreate = l->data.Part.nocreate;
		CellLock<LOCK_TYPE> lock(r_zone, cell_iter);
		m.Visit(lock, visitor);
	    }

	    break;
	}
    case LOWER_LEFT_DISTRICT:
	{
	    CellPair update_cell(standing_cell);
	    update_cell << 1;
	    update_cell += 1;

	    
	    for(cell_iter = update_cell; abs(int(standing_cell.x_coord - cell_iter.x_coord)) < 2; cell_iter >> 1)
	    {
		Cell r_zone = RedZone::GetZone(cell_iter);
		r_zone.data.Part.nocreate = l->data.Part.nocreate;
		CellLock<LOCK_TYPE> lock(r_zone, cell_iter);
		m.Visit(lock, visitor);
	    }

	    
	    for(cell_iter=update_cell, cell_iter -= 1; abs(int(standing_cell.y_coord - cell_iter.y_coord)) < 2; cell_iter -= 1)
	    {
		Cell r_zone = RedZone::GetZone(cell_iter);
		r_zone.data.Part.nocreate = l->data.Part.nocreate;
		CellLock<LOCK_TYPE> lock(r_zone, cell_iter);
		m.Visit(lock, visitor);
	    }

	    break;
	}
    case LOWER_RIGHT_DISTRICT:
	{
	    CellPair update_cell(standing_cell);
	    update_cell >> 1;
	    update_cell += 1;

	    
	    for(cell_iter=update_cell; abs(int(standing_cell.x_coord - cell_iter.x_coord)) < 2; cell_iter << 1)
	    {
		Cell r_zone = RedZone::GetZone(cell_iter);
		r_zone.data.Part.nocreate = l->data.Part.nocreate;
		CellLock<LOCK_TYPE> lock(r_zone, cell_iter);
		m.Visit(lock, visitor);
	    }

	    
	    for(cell_iter=update_cell, cell_iter -= 1; abs(int(standing_cell.y_coord - cell_iter.y_coord)) < 2; cell_iter -= 1)
	    {
		Cell r_zone = RedZone::GetZone(cell_iter);
		r_zone.data.Part.nocreate = l->data.Part.nocreate;
		CellLock<LOCK_TYPE> lock(r_zone, cell_iter);
		m.Visit(lock, visitor);
	    }

	    break;
	}
    case LEFT_DISTRICT:
	{
	    CellPair update_cell(standing_cell);
	    update_cell << 1;
	    update_cell -= 1;

	    
	    for(cell_iter=update_cell; abs(int(standing_cell.y_coord - cell_iter.y_coord)) < 2; cell_iter += 1)
	    {
		Cell r_zone = RedZone::GetZone(cell_iter);
		r_zone.data.Part.nocreate = l->data.Part.nocreate;
		CellLock<LOCK_TYPE> lock(r_zone, cell_iter);
		m.Visit(lock, visitor);
	    }

	    break;
	}
    case RIGHT_DISTRICT:
	{
	    CellPair update_cell(standing_cell);
	    update_cell >> 1;
	    update_cell -= 1;

	    
	    for(cell_iter=update_cell; abs(int(standing_cell.y_coord - cell_iter.y_coord)) < 2; cell_iter += 1)
	    {
		Cell r_zone = RedZone::GetZone(cell_iter);
		r_zone.data.Part.nocreate = l->data.Part.nocreate;
		CellLock<LOCK_TYPE> lock(r_zone, cell_iter);
		m.Visit(lock, visitor);
	    }
	    break;
	}
    case UPPER_DISTRICT:
	{
	    CellPair update_cell(standing_cell);
	    update_cell << 1;
	    update_cell -= 1;

	    
	    for(cell_iter=update_cell; abs(int(standing_cell.x_coord - cell_iter.x_coord)) < 2; cell_iter >> 1)
	    {
		Cell r_zone = RedZone::GetZone(cell_iter);
		r_zone.data.Part.nocreate = l->data.Part.nocreate;
		CellLock<LOCK_TYPE> lock(r_zone, cell_iter);
		m.Visit(lock, visitor);
	    }

	    break;
	}
    case LOWER_DISTRICT:
	{
	    CellPair update_cell(standing_cell);
	    update_cell << 1;
	    update_cell += 1;

	    
	    for(cell_iter=update_cell; abs(int(standing_cell.x_coord - cell_iter.x_coord)) < 2; cell_iter >> 1)
	    {
		Cell r_zone = RedZone::GetZone(cell_iter);
		r_zone.data.Part.nocreate = l->data.Part.nocreate;
		CellLock<LOCK_TYPE> lock(r_zone, cell_iter);
		m.Visit(lock, visitor);
	    }

	    break;
	}
    case CENTER_DISTRICT:
	{
	    m.Visit(l, visitor);
	    break;
	}
    default:
	{
	    assert( false );
	    break;
	}
    }    
}

#endif
