/* QueryResultMysql.h
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

#if !defined(QUERYRESULTMYSQL_H)
#define QUERYRESULTMYSQL_H

#ifdef WIN32
#include <winsock2.h>
#include <mysql/mysql.h>
#else
#include <mysql.h>
#endif

class QueryResultMysql : public QueryResult
{
    public:
        QueryResultMysql(MYSQL_RES *result, uint64 rowCount, uint32 fieldCount);

        //! Frees resources used by QueryResult.
        ~QueryResultMysql();

        //! Selects the next row in the result of the current query.
        /*
        This will update any references to fields of the previous row, so use Field's copy constructor to keep a persistant field.
        @return 1 if the next row was successfully selected, else 0.
        */
        bool NextRow();

    private:
        enum Field::DataTypes ConvertNativeType(enum_field_types mysqlType) const;
        void EndQuery();

        MYSQL_RES *mResult;
};
#endif
