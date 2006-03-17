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

#if !defined(DATABASE_H)
#define DATABASE_H

class Database : public Singleton<Database>
{
    protected:
        Database() {}

    public:
        
        virtual ~Database() {}

        
        virtual bool Initialize(const char *infoString) = 0;

        
        
        virtual QueryResult* Query(const char *sql) = 0;

        
        
        virtual bool Execute(const char *sql) = 0;

        
        virtual operator bool () const = 0;
};

#define sDatabase Database::getSingleton()
#endif
