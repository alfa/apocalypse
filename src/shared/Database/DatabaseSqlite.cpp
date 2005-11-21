/* DatabaseSqlite.cpp
 *
 * Copyright (C) 2004 Wow Daemon
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

#include "DatabaseEnv.h"

DatabaseSqlite::DatabaseSqlite() : Database(), mSqlite(0)
{
}


DatabaseSqlite::~DatabaseSqlite()
{
    if (mSqlite)
        sqlite_close(mSqlite);
}


bool DatabaseSqlite::Initialize(const char *infoString)
{
    char *errmsg;

    mSqlite = sqlite_open(infoString, 0, &errmsg);

    if (!mSqlite)
    {
        // sLog.Log(L_CRITICAL, "Sqlite initialization failed: %s\n",
        //     errmsg ? errmsg : "<no error message>");
        if (errmsg)
            sqlite_freemem(errmsg);
        return false;
    }

    return true;
}


QueryResult* DatabaseSqlite::Query(const char *sql)
{
    char *errmsg;

    if (!mSqlite)
        return 0;

    char **tableData;
    int rowCount;
    int fieldCount;

    sqlite_get_table(mSqlite, sql, &tableData, &rowCount, &fieldCount, &errmsg);

    if (!rowCount)
        return 0;                                 // no result

    if (!tableData)
    {
        // sLog.Log(L_ERROR, "Query \"%s\" failed: %s\n",
        //     sql, errmsg ? errmsg : "<no error message>");
        if (errmsg)
            sqlite_freemem(errmsg);
        return 0;
    }

    QueryResultSqlite *queryResult = new QueryResultSqlite(tableData, rowCount, fieldCount);
    if(!queryResult)
    {
        // sLog.Log(L_ERROR, "Out of memory on query result allocation in query \"%s\"\n", sql);
        return 0;
    }

    queryResult->NextRow();

    return queryResult;
}


bool DatabaseSqlite::Execute(const char *sql)
{
    char *errmsg;

    if (!mSqlite)
        return false;

    if(sqlite_exec(mSqlite, sql, NULL, NULL, &errmsg) != SQLITE_OK)
        return false;

    return true;
}
