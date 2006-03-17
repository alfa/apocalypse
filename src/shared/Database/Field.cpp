/* Field.cpp
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

Field::Field() :
mValue(NULL), mName(NULL), mType(DB_TYPE_UNKNOWN)
{
}


Field::Field(Field &f)
{
    const char *value, *name;

    value = f.GetString();
    name = f.GetName();

    if (value && (mValue = new char[strlen(value) + 1]))
        strcpy(mValue, value);
    else
        mValue = 0;

    if (name && (mName = new char[strlen(name) + 1]))
        strcpy(mName, name);
    else
        mName = 0;

    mType = f.GetType();
}


Field::Field(const char *value, const char *name, enum Field::DataTypes type) :
mType(type)
{
    if (value && (mValue = new char[strlen(value) + 1]))
        strcpy(mValue, value);
    else
        mValue = NULL;

    if (name && (mName = new char[strlen(name) + 1]))
        strcpy(mName, name);
    else
        mName = NULL;
}


Field::~Field()
{
    if (mValue)
        delete [] mValue;
    if (mName)
        delete [] mName;
    mValue = mName = NULL;
}


void Field::SetValue(const char *value)
{
    if (mValue)
        delete [] mValue;
    
    if (value)
    {
        mValue = new char[strlen(value) + 1];
        if (mValue)
            strcpy(mValue, value);
    }
    else
	mValue = NULL;
}


void Field::SetName(const char *name)
{
    if (mName)
        delete [] mName;
    if (name)
    {
        mName = new char[strlen(name) + 1];
        if (mName)
            strcpy(mName, name);
    }
    else
        mName = NULL;
}
