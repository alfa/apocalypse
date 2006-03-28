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

#ifndef SQLSTORAGE_H
#define SQLSTORAGE_H

#include "Common.h"
#include "Database/DatabaseEnv.h"

class SQLStorage
{
public:

	SQLStorage(const char*fmt,const char * sqlname)
	{
		format=fmt;
		table=sqlname;
		data=NULL;
		pIndex=NULL;
		iNumFields =strlen(fmt);
	}

	char** pIndex;
	uint32 iNumRecords;
	uint32 iNumFields;
	void Load();
	void Free();
private:

	char *data;
	char *pOldData;
	char **pOldIndex;
	uint32 iOldNumRecords;
	const char *format;
	const char *table;
	//bool HasString;
};
#endif
