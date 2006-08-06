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

#include "Log.h"
#include "Database/DatabaseEnv.h"
#include "MapManager.h"
#include "ObjectAccessor.h"
#include "EventSystem.h"
#include "GlobalEvents.h"

void HandleCorpsesErase(void*)
{
    mysql_thread_init();                                    // let thread do safe mySQL requests

    //sLog.outBasic("Global Event (corpses/bones removal)");

    QueryResult *result = sDatabase.Query("SELECT * FROM `game_corpse` WHERE UNIX_TIMESTAMP()-UNIX_TIMESTAMP(`time`) > 1200 AND `bones_flag` = 1;");

    if(result)
    {
        do
        {

            Field *fields = result->Fetch();
            uint64 guid = fields[0].GetUInt64();
            float positionX = fields[2].GetFloat();
            float positionY = fields[3].GetFloat();
            //float positionZ = fields[4].GetFloat();
            //float ort       = fields[5].GetFloat();
            uint32 mapid    = fields[7].GetUInt32();

            sLog.outDebug("Removing corpse %u:%u (X:%u Y:%u Map:%u).",GUID_HIPART(guid),GUID_LOPART(guid),positionX,positionY,mapid);

            Corpse *m_pCorpse = ObjectAccessor::Instance().GetCorpse(positionX,positionY,mapid,guid);

            if(m_pCorpse)
            {
                ObjectAccessor::Instance().RemoveBonesFromPlayerView(m_pCorpse);
                MapManager::Instance().GetMap(m_pCorpse->GetMapId())->Remove(m_pCorpse,true);
            }
            else
                sLog.outDebug("Corpse %lu not found!",guid);

            sDatabase.PExecute("DELETE FROM `game_corpse` WHERE guid = '%lu';",(unsigned long)guid);

        } while (result->NextRow());

        delete result;
    }

    result = sDatabase.Query("SELECT * FROM `game_corpse` WHERE UNIX_TIMESTAMP()-UNIX_TIMESTAMP(`time`) > 259200 AND `bones_flag` = 0;");

    if(result)
    {

        do
        {
            Field *fields = result->Fetch();

            uint64 guid = fields[0].GetUInt64();
            float positionX = fields[2].GetFloat();
            float positionY = fields[3].GetFloat();
            //float positionZ = fields[4].GetFloat();
            //float ort       = fields[5].GetFloat();
            uint32 mapid    = fields[7].GetUInt32();

            sLog.outDebug("Removing corpse %u:%u (X:%u Y:%u Map:%u).",GUID_HIPART(guid),GUID_LOPART(guid),positionX,positionY,mapid);

            Corpse *m_pCorpse = ObjectAccessor::Instance().GetCorpse(positionX,positionY,mapid,guid);

            if(m_pCorpse)
            {
                ObjectAccessor::Instance().RemoveBonesFromPlayerView(m_pCorpse);
                MapManager::Instance().GetMap(m_pCorpse->GetMapId())->Remove(m_pCorpse,true);
            }
            else
                sLog.outDebug("Corpse %lu not found!",guid);

            sDatabase.PExecute("DELETE FROM `game_corpse` WHERE `guid` = '%lu';",(unsigned long)guid);
        } while (result->NextRow());

        delete result;
    }
    mysql_thread_end();                                     // free mySQL thread resources
}
