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
#include "Database/DatabaseEnv.h"
#include "RealmList.h"

#include "Config/ConfigEnv.h"
#include "Log.h"
#include "Network/SocketHandler.h"
#include "Network/ListenSocket.h"
#include "AuthSocket.h"
#include "SystemConfig.h"

#include <signal.h>

bool StartDB(std::string &dbstring);
void UnhookSignals();
void HookSignals();
//uint8 loglevel = DEFAULT_LOG_LEVEL;

bool stopEvent = false;
RealmList m_realmList;
DatabaseMysql dbRealmServer;

void usage(const char *prog)
{
    sLog.outString("Usage: \n %s -c config_file [%s]", prog, _REALMD_CONFIG);
}

int main(int argc, char **argv)
{
    char*   cfg_file=_REALMD_CONFIG;

    //Parse arguments
    switch(argc)
    {
        case 1:
            break;
        case 3:
            if(strcmp(argv[1], "-c")==0)
            {
                cfg_file=argv[2];
                break;
            }
        default:
            usage(argv[0]);
            return 1;
    }

    if (!sConfig.SetSource(cfg_file))
    {
        sLog.outError("Could not find configuration file %s.", cfg_file);
        return 1;
    }
    sLog.outString("Using configuration file %s.", cfg_file);

    // Non-critical warning about conf file version
    uint32 confVersion = sConfig.GetIntDefault("ConfVersion", 0);
    if (confVersion < _REALMDCONFVERSION)
    {
        sLog.outError("*****************************************************************************");
        sLog.outError(" WARNING: Your realmd.conf version indicates your conf file is out of date!");
        sLog.outError("          Please check for updates, as your current default values may cause");
        sLog.outError("          strange behavior.");
        sLog.outError("*****************************************************************************");
        clock_t pause = 3000 + clock();
        while (pause > clock());
    }

    sLog.outString( "MaNGOS realm daemon %s", _FULLVERSION );
    sLog.outString( "<Ctrl-C> to stop.\n" );

    std::string dbstring;
    if(!StartDB(dbstring))
        return 1;

    //loglevel = (uint8)sConfig.GetIntDefault("LogLevel", DEFAULT_LOG_LEVEL);

    port_t rmport = sConfig.GetIntDefault( "RealmServerPort", DEFAULT_REALMSERVER_PORT );

    m_realmList.GetAndAddRealms(dbstring);
    if (m_realmList.size() == 0)
    {
        sLog.outError("No valid realms specified.");
        return 1;
    }

    SocketHandler h;
    ListenSocket<AuthSocket> authListenSocket(h);
    if ( authListenSocket.Bind(rmport))
    {
        sLog.outError( "MaNGOS realmd can not bind to port %d", rmport );
        return 1;
    }

    h.Add(&authListenSocket);

    HookSignals();

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
                    sLog.outError("Processors marked in UseProcessors bitmask (hex) %x not accessable for realmd. Accessable processors bitmask (hex): %x",Aff,appAff);
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
                sLog.outString("realmd process priority class set to HIGH");
            else
                sLog.outError("ERROR: Can't set realmd process priority class.");
            sLog.outString("");
        }
    }
    #endif

    while (!stopEvent)
        h.Select(0, 100000);

    UnhookSignals();

    sLog.outString( "Halting process..." );
    return 0;
}

void OnSignal(int s)
{
    switch (s)
    {
        case SIGINT:
        case SIGQUIT:
        case SIGTERM:
        case SIGABRT:
            stopEvent = true;
            break;
        #ifdef _WIN32
        case SIGBREAK:
            stopEvent = true;
            break;
        #endif
    }

    signal(s, OnSignal);
}

bool StartDB(std::string &dbstring)
{
    if(!sConfig.GetString("LoginDatabaseInfo", &dbstring))
    {
        sLog.outError("Database not specified");
        return false;
    }

    sLog.outString("Database: %s", dbstring.c_str() );
    if(!dbRealmServer.Initialize(dbstring.c_str()))
    {
        sLog.outError("Cannot connect to database");
        return false;

    }

    return true;
}

void HookSignals()
{
    signal(SIGINT, OnSignal);
    signal(SIGQUIT, OnSignal);
    signal(SIGTERM, OnSignal);
    signal(SIGABRT, OnSignal);
    #ifdef _WIN32
    signal(SIGBREAK, OnSignal);
    #endif
}

void UnhookSignals()
{
    signal(SIGINT, 0);
    signal(SIGQUIT, 0);
    signal(SIGTERM, 0);
    signal(SIGABRT, 0);
    #ifdef _WIN32
    signal(SIGBREAK, 0);
    #endif

}
