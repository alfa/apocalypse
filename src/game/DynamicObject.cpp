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

#include "Common.h"
#include "GameObject.h"
#include "UpdateMask.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "MapManager.h"
#include "RedZoneDistrict.h"
#include "GridNotifiers.h"
#include "CellImpl.h"
#include "GridNotifiersImpl.h"

DynamicObject::DynamicObject() : Object()
{
    m_objectType |= TYPE_DYNAMICOBJECT;
    m_objectTypeId = TYPEID_DYNAMICOBJECT;

    m_valuesCount = DYNAMICOBJECT_END;
}

bool DynamicObject::Create( uint32 guidlow, Unit *caster, uint32 spellId, uint32 effIndex, float x, float y, float z, int32 duration, float radius )
{
    Object::_Create(guidlow, 0xF0007000, caster->GetMapId(), x, y, z, 0, (uint8)-1);

    SetUInt32Value( OBJECT_FIELD_ENTRY, spellId );
    SetFloatValue( OBJECT_FIELD_SCALE_X, 1 );
    SetUInt64Value( DYNAMICOBJECT_CASTER, caster->GetGUID() );
    SetUInt32Value( DYNAMICOBJECT_BYTES, 0x00000001 );
    SetUInt32Value( DYNAMICOBJECT_SPELLID, spellId );
    SetFloatValue( DYNAMICOBJECT_RADIUS, radius);
    SetFloatValue( DYNAMICOBJECT_POS_X, x );
    SetFloatValue( DYNAMICOBJECT_POS_Y, y );
    SetFloatValue( DYNAMICOBJECT_POS_Z, z );

    m_aliveDuration = duration;
    m_radius = radius;
    deleteThis = false;
    m_effIndex = effIndex;
    m_spellId = spellId;
    m_caster = caster;
    return true;
}

void DynamicObject::Update(uint32 p_time)
{
    if(m_aliveDuration > 0)
    {
        if(uint32(m_aliveDuration) > p_time)
            m_aliveDuration -= p_time;
        else
        {
            if(IsInWorld())
            {
                deleteThis = true;
            }
        }
    }

    // TODO: make a timer and update this in larger intervals

    CellPair p(MaNGOS::ComputeCellPair(GetPositionX(), GetPositionY()));
    Cell cell = RedZone::GetZone(p);
    cell.data.Part.reserved = ALL_DISTRICT;
    cell.SetNoCreate();

    MaNGOS::DynamicObjectUpdater notifier(*this);

    TypeContainerVisitor<MaNGOS::DynamicObjectUpdater, ContainerMapList<Player> > player_notifier(notifier);
    TypeContainerVisitor<MaNGOS::DynamicObjectUpdater, TypeMapContainer<AllObjectTypes> > object_notifier(notifier);

    CellLock<GridReadGuard> cell_lock(cell, p);
    cell_lock->Visit(cell_lock, player_notifier, *MapManager::Instance().GetMap(m_caster->GetMapId()));
    cell_lock->Visit(cell_lock, object_notifier, *MapManager::Instance().GetMap(m_caster->GetMapId()));
}

void DynamicObject::Delete()
{
    SendObjectDeSpawnAnim(GetGUID());
    SendDestroyObject(GetGUID());

    RemoveFromWorld();
    ObjectAccessor::Instance().AddObjectToRemoveList(this);
}

void DynamicObject::Delay(int32 delaytime)
{
    m_aliveDuration += delaytime;
    for(AffectedSet::iterator iunit= m_affected.begin();iunit != m_affected.end();++iunit)
        if (*iunit)
            (*iunit)->DelayAura(m_spellId, m_effIndex, delaytime);
}
