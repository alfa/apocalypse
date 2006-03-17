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

#ifndef __WORLDSOCKETMGR_H
#define __WORLDSOCKETMGR_H

#include "Policies/Singleton.h"

class WorldSocket;

class WorldSocketMgr 
{
    public:
        WorldSocketMgr();

        void AddSocket(WorldSocket *s);
        void RemoveSocket(WorldSocket *s);
        void Update(time_t diff);

    private:
        typedef std::set<WorldSocket*> SocketSet;
        SocketSet m_sockets;
};

#define sWorldSocketMgr MaNGOS::Singleton<WorldSocketMgr>::Instance()
#endif
