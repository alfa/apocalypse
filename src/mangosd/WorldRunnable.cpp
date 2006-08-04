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
#include "Log.h"
#include "World.h"
#include "WorldRunnable.h"
#include "Master.h"
#include "Timer.h"

void WorldRunnable::run()
{
    mysql_thread_init();                                    // let thread do safe mySQL requests

    uint32 realCurrTime = 0, realPrevTime = 0;

    while (!Master::m_stopEvent)
    {

        if (realPrevTime > realCurrTime)
            realPrevTime = 0;

        realCurrTime = getMSTime();
        sWorld.Update( realCurrTime - realPrevTime );
        realPrevTime = realCurrTime;

	#ifdef WIN32
	ZThread::Thread::sleep(50);
	#else
        ZThread::Thread::sleep(100);
	#endif
    }

    mysql_thread_end();                                     // free mySQL thread resources
}
