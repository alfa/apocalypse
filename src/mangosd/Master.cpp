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

#include "Master.h"
#include "Network/SocketHandler.h"
#include "Network/ListenSocket.h"
#include "Network/TcpSocket.h"
//#include "AuthSocket.h"
#include "WorldSocket.h"
#include "RASocket.h"
#include "WorldSocketMgr.h"
#include "WorldRunnable.h"
#include "World.h"
//#include "RealmList.h"
#include "Log.h"
#include "Timer.h"
#include <signal.h>
#include "MapManager.h"
#include "Policies/SingletonImp.h"
#include "AddonHandler.h"

#ifdef ENABLE_CLI
#include "CliRunnable.h"

INSTANTIATE_SINGLETON_1( CliRunnable );
#endif

#pragma warning(disable:4305)

INSTANTIATE_SINGLETON_1( Master );

void Master::_OnSignal(int s)
{
    switch (s)
    {
        case SIGINT:
        case SIGQUIT:
        case SIGTERM:
        case SIGABRT:
            World::m_stopEvent = true;
            break;
        #ifdef _WIN32
        case SIGBREAK:
            World::m_stopEvent = true;
            break;
        #endif
    }

    signal(s, _OnSignal);
}

Master::Master()
{
}

Master::~Master()
{
}

bool Master::Run()
{
    sLog.outString( "MaNGOS daemon %s", _FULLVERSION );
    sLog.outString( "<Ctrl-C> to stop.\n\n" );

    sLog.outTitle( "MM   MM         MM   MM  MMMMM   MMMM   MMMMM");
    sLog.outTitle( "MM   MM         MM   MM MMM MMM MM  MM MMM MMM");
    sLog.outTitle( "MMM MMM         MMM  MM MMM MMM MM  MM MMM");
    sLog.outTitle( "MM M MM         MMMM MM MMM     MM  MM  MMM");
    sLog.outTitle( "MM M MM  MMMMM  MM MMMM MMM     MM  MM   MMM");
    sLog.outTitle( "MM M MM M   MMM MM  MMM MMMMMMM MM  MM    MMM");
    sLog.outTitle( "MM   MM     MMM MM   MM MM  MMM MM  MM     MMM");
    sLog.outTitle( "MM   MM MMMMMMM MM   MM MMM MMM MM  MM MMM MMM");
    sLog.outTitle( "MM   MM MM  MMM MM   MM  MMMMMM  MMMM   MMMMM");
    sLog.outTitle( "        MM  MMM http://www.mangosproject.org");
    sLog.outTitle( "        MMMMMM\n\n");

    _StartDB();

    //loglevel = (uint8)sConfig.GetIntDefault("LogLevel", DEFAULT_LOG_LEVEL);

    sWorld.SetPlayerLimit( sConfig.GetIntDefault("PlayerLimit", DEFAULT_PLAYER_LIMIT) );
    sWorld.SetMotd( sConfig.GetStringDefault("Motd", "Welcome to the Massive Network Game Object Server." ).c_str() );
    sWorld.SetInitialWorldSettings();

    port_t wsport, rmport;
    rmport = sWorld.getConfig(CONFIG_PORT_REALM);           //sConfig.GetIntDefault( "RealmServerPort", DEFAULT_REALMSERVER_PORT );
    wsport = sWorld.getConfig(CONFIG_PORT_WORLD);           //sConfig.GetIntDefault( "WorldServerPort", DEFAULT_WORLDSERVER_PORT );

    uint32 socketSelecttime;
                                                            //sConfig.GetIntDefault( "SocketSelectTime", DEFAULT_SOCKET_SELECT_TIME );
    socketSelecttime = sWorld.getConfig(CONFIG_SOCKET_SELECTTIME);

    //uint32 grid_clean_up_delay = sConfig.GetIntDefault("GridCleanUpDelay", 300);
    //sLog.outDebug("Setting Grid clean up delay to %d seconds.", grid_clean_up_delay);
    //grid_clean_up_delay *= 1000;
    //MapManager::Instance().SetGridCleanUpDelay(grid_clean_up_delay);

    //uint32 map_update_interval = sConfig.GetIntDefault("MapUpdateInterval", 100);
    //sLog.outDebug("Setting map update interval to %d milli-seconds.", map_update_interval);
    //MapManager::Instance().SetMapUpdateInterval(map_update_interval);

    //    sRealmList.setServerPort(wsport);
    //    sRealmList.GetAndAddRealms ();
    SocketHandler h;
    ListenSocket<WorldSocket> worldListenSocket(h);
    //    ListenSocket<AuthSocket> authListenSocket(h);

    if (worldListenSocket.Bind(wsport))
    {
        _StopDB();
        sLog.outError( "MaNGOS can not bind to that port" );
        exit(1);
    }

    h.Add(&worldListenSocket);
    //    h.Add(&authListenSocket);

    _HookSignals();

    ZThread::Thread t(new WorldRunnable);

    //#ifndef WIN32
    t.setPriority ((ZThread::Priority )2);
    //#endif

    #ifdef ENABLE_CLI
    ZThread::Thread td1(new CliRunnable);
    #endif

    #ifdef ENABLE_RA

    ListenSocket<RASocket> RAListenSocket(h);

    if (RAListenSocket.Bind(sConfig.GetIntDefault( "RA.Port", 3443 )))
    {

        sLog.outError( "MaNGOS can not bind to that port" );
        // exit(1); go on with no RA

    }

    h.Add(&RAListenSocket);
    #endif

    #ifdef WIN32
    {
        HANDLE hProcess = GetCurrentProcess();

        uint32 Aff = sConfig.GetIntDefault("UseProcessors", 0);
        if(Aff > 0)
        {
            uint32 appAff;
            uint32 sysAff;

            if(GetProcessAffinityMask(hProcess,&appAff,&sysAff))
            {
                uint32 curAff = Aff & appAff;               // remove non accassable processors

                if(!curAff )
                {
                    sLog.outError("Processors marked in UseProcessors bitmask (hex) %x not accessable for mangosd. Accessable processors bitmask (hex): %x",Aff,appAff);
                }
                else
                {
                    if(SetProcessAffinityMask(hProcess,curAff))
                        sLog.outString("Using processors (bitmask, hex): %x", curAff);
                    else
                        sLog.outError("Can't set used processors (hex): %x",curAff);
                }
            }
            sLog.outString("");
        }

        uint32 Prio = sConfig.GetIntDefault("ProcessPriority", 0);

        if(Prio)
        {
            if(SetPriorityClass(hProcess,HIGH_PRIORITY_CLASS))
                sLog.outString("mangosd process priority class set to HIGH");
            else
                sLog.outError("ERROR: Can't set mangosd process priority class.");
            sLog.outString("");
        }
    }
    #endif

    uint32 realCurrTime, realPrevTime;
    realCurrTime = realPrevTime = getMSTime();

    // maximum counter for next ping
    uint32 numLoops = (sConfig.GetIntDefault( "MaxPingTime", 30 ) * (MINUTE * 1000000 / socketSelecttime));
    uint32 loopCounter = 0;

    while (!World::m_stopEvent)
    {

        if (realPrevTime > realCurrTime)
            realPrevTime = 0;

        realCurrTime = getMSTime();
        sWorldSocketMgr.Update( realCurrTime - realPrevTime );
        realPrevTime = realCurrTime;

        //h.Select(0, 100000);
        h.Select(0, socketSelecttime);

        // ping if need
        if( (++loopCounter) == numLoops )
        {
            loopCounter = 0;
            sLog.outDetail("Ping MySQL to keep connection alive");
            sDatabase.Query("SELECT 1 FROM `command` LIMIT 1");
            loginDatabase.Query("SELECT 1 FROM `realmlist` LIMIT 1");
        }
    }

    sLog.outString( "WORLD: Saving Addons" );
    sAddOnHandler._SaveToDB();

    _UnhookSignals();

    t.wait();

    _StopDB();

    sLog.outString( "Halting process..." );

    return 0;
}

bool Master::_StartDB()
{
    std::string dbstring;
    if(!sConfig.GetString("WorldDatabaseInfo", &dbstring))
    {
        sLog.outError("Database not specified");
        exit(1);

    }

    sLog.outString("World Database: %s", dbstring.c_str() );
    if(!sDatabase.Initialize(dbstring.c_str()))
    {
        sLog.outError("Cannot connect to world database");
        exit(1);
    }

    if(!sConfig.GetString("LoginDatabaseInfo", &dbstring))
    {
        sLog.outError("Login database not specified");
        exit(1);

    }
    sLog.outString("Login Database: %s", dbstring.c_str() );
    if(!loginDatabase.Initialize(dbstring.c_str()))
    {
        sLog.outError("Cannot connect to login database");
        exit(1);
    }

    realmID = sConfig.GetIntDefault("RealmID", 0);
    if(!realmID)
    {
        sLog.outError("Realm ID not defined");
        exit(1);
    }
    else
    {
        sLog.outString("Realm running as realm ID %d", realmID);
    }

    clearOnlineAccounts();
    return true;
}

void Master::_StopDB()
{
    clearOnlineAccounts();
}

void Master::clearOnlineAccounts()
{
    // Cleanup online status for characters hosted at current realm
    sDatabase.PExecute("UPDATE `character` SET `online` = 0 WHERE `realm` = '%d'",realmID);

    loginDatabase.PExecute(
        "UPDATE `account`,`realmcharacters` SET `account`.`online` = 0 "
        "WHERE `account`.`online` > 0 AND `account`.`id` = `realmcharacters`.`acctid` "
        "  AND `realmcharacters`.`realmid` = '%d'",realmID);
}

void Master::_HookSignals()
{
    signal(SIGINT, _OnSignal);
    signal(SIGQUIT, _OnSignal);
    signal(SIGTERM, _OnSignal);
    signal(SIGABRT, _OnSignal);
    #ifdef _WIN32
    signal(SIGBREAK, _OnSignal);
    #endif
}

void Master::_UnhookSignals()
{
    signal(SIGINT, 0);
    signal(SIGQUIT, 0);
    signal(SIGTERM, 0);
    signal(SIGABRT, 0);
    #ifdef _WIN32
    signal(SIGBREAK, 0);
    #endif

}
