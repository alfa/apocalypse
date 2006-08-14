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

#include "GridNotifiers.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "UpdateData.h"
#include "Item.h"
#include "Utilities.h"
#include "Map.h"

using namespace MaNGOS;

void PlayerNotifier::Visit(PlayerMapType &m)
{
    BuildForMySelf();

    for(std::map<OBJECT_HANDLE, Player *>::iterator iter=m.begin(); iter != m.end(); ++iter)
    {
        if( iter->second == &i_player )
            continue;

        if( (i_player.isAlive() && iter->second->isAlive()) ||
            (i_player.isDead() && iter->second->isDead()) )
        {
            iter->second->SendUpdateToPlayer(&i_player);
            i_player.SendUpdateToPlayer(iter->second);
        }
        else
        {
            ObjectAccessor::Instance().RemovePlayerFromPlayerView(&i_player, iter->second);
        }
    }
}

void
PlayerNotifier::BuildForMySelf()
{
    if( !i_player.IsInWorld() )
    {
        WorldPacket packet;
        UpdateData data;
        sLog.outDetail("Creating player data for himself %u", i_player.GetGUIDLow());
        i_player.BuildCreateUpdateBlockForPlayer(&data, &i_player);
        data.BuildPacket(&packet);
        i_player.GetSession()->SendPacket(&packet);
        i_player.AddToWorld();
    }
}

void
VisibleNotifier::Notify()
{
    if( i_data.HasData() )
    {
        WorldPacket packet;
        i_data.BuildPacket(&packet);
        i_player.GetSession()->SendPacket(&packet);
    }
}

template<class T>
void
VisibleNotifier::Visit(std::map<OBJECT_HANDLE, T *> &m)
{
    for(typename std::map<OBJECT_HANDLE, T *>::iterator iter=m.begin(); iter != m.end(); ++iter)
        iter->second->BuildCreateUpdateBlockForPlayer(&i_data, &i_player);
}

void
NotVisibleNotifier::Notify()
{
    if( i_data.HasData() )
    {
        WorldPacket packet;
        i_data.BuildPacket(&packet);
        i_player.GetSession()->SendPacket(&packet);
    }
}

template<class T>
void
NotVisibleNotifier::Visit(std::map<OBJECT_HANDLE, T *> &m)
{
    for(typename std::map<OBJECT_HANDLE, T *>::iterator iter=m.begin(); iter != m.end(); ++iter)
        iter->second->BuildOutOfRangeUpdateBlock(&i_data);
}

void
NotVisibleNotifier::Visit(std::map<OBJECT_HANDLE, GameObject *> &m)
{
    for(std::map<OBJECT_HANDLE, GameObject*>::iterator iter=m.begin(); iter != m.end(); ++iter)
        // ignore transport gameobjects at same map
        if(i_player.GetMapId()!=iter->second->GetMapId() || !iter->second->IsTransport())
            iter->second->BuildOutOfRangeUpdateBlock(&i_data);
}

void
ObjectVisibleNotifier::Visit(PlayerMapType &m)
{
    for(std::map<OBJECT_HANDLE, Player *>::iterator iter=m.begin(); iter != m.end(); ++iter)
    {
        UpdateData update_data;
        WorldPacket packet;
        i_object.BuildCreateUpdateBlockForPlayer(&update_data, iter->second);
        update_data.BuildPacket(&packet);
        iter->second->GetSession()->SendPacket(&packet);
    }
}

void
ObjectNotVisibleNotifier::Visit(PlayerMapType &m)
{
    // ignore transport gameobjects at same map
    bool transport = (i_object.GetTypeId() == TYPEID_GAMEOBJECT && ((GameObject&)i_object).IsTransport());

    for(std::map<OBJECT_HANDLE, Player *>::iterator iter=m.begin(); iter != m.end(); ++iter)
        if(i_object.GetMapId()!=iter->second->GetMapId() || !transport)
            iter->second->SendOutOfRange(&i_object);
}

void
MessageDeliverer::Visit(PlayerMapType &m)
{
    for(PlayerMapType::iterator iter=m.begin(); iter != m.end(); ++iter)
    {
        if( (iter->second != &i_player || i_toSelf)
            && (!i_ownTeamOnly || iter->second->GetTeam() == i_player.GetTeam()) )
        {
            iter->second->GetSession()->SendPacket(i_message);
        }
    }
}

void
ObjectMessageDeliverer::Visit(PlayerMapType &m)
{
    for(PlayerMapType::iterator iter=m.begin(); iter != m.end(); ++iter)
    {
        iter->second->GetSession()->SendPacket(i_message);
    }
}

void
CreatureVisibleMovementNotifier::Visit(PlayerMapType &m)
{
    for(PlayerMapType::iterator iter=m.begin(); iter != m.end(); ++iter)
    {
        if( iter->second->isAlive() )
        {
            UpdateData update_data;
            WorldPacket packet;
            i_creature.BuildCreateUpdateBlockForPlayer(&update_data, iter->second);
            update_data.BuildPacket(&packet);
            iter->second->GetSession()->SendPacket(&packet);
        }
    }
}

void
CreatureNotVisibleMovementNotifier::Visit(PlayerMapType &m)
{
    for(PlayerMapType::iterator iter=m.begin(); iter != m.end(); ++iter)
        if( iter->second->isAlive() )
            iter->second->SendOutOfRange(&i_creature);
}

template<class T> void
ObjectUpdater::Visit(std::map<OBJECT_HANDLE, T *> &m)
{
    for(typename std::map<OBJECT_HANDLE, T*>::iterator iter=m.begin(); iter != m.end(); ++iter)
    {
        iter->second->Update(i_timeDiff);
    }
}

void GameObjectWithNameIn2DRangeChecker::Visit(std::map<OBJECT_HANDLE, GameObject *> &m)
{
    // already found
    if(i_object) return;

    for(std::map<OBJECT_HANDLE, GameObject *>::iterator itr=m.begin(); itr != m.end(); ++itr)
    {
        if(itr->second->GetGOInfo()->name == i_name)
        {
            if(itr->second->GetDistance2dSq(i_unit) <= i_dist2)
            {
                i_object = itr->second;
                return;
            }
        }
    }
}

template void VisibleNotifier::Visit<GameObject>(std::map<OBJECT_HANDLE, GameObject *> &);
template void VisibleNotifier::Visit<Corpse>(std::map<OBJECT_HANDLE, Corpse *> &);
template void VisibleNotifier::Visit<DynamicObject>(std::map<OBJECT_HANDLE, DynamicObject *> &);

template void NotVisibleNotifier::Visit<Corpse>(std::map<OBJECT_HANDLE, Corpse *> &);
template void NotVisibleNotifier::Visit<DynamicObject>(std::map<OBJECT_HANDLE, DynamicObject *> &);

template void ObjectUpdater::Visit<GameObject>(std::map<OBJECT_HANDLE, GameObject *> &);
template void ObjectUpdater::Visit<Corpse>(std::map<OBJECT_HANDLE, Corpse *> &);
template void ObjectUpdater::Visit<DynamicObject>(std::map<OBJECT_HANDLE, DynamicObject *> &);
