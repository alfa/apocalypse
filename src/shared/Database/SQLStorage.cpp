/* 
 * Copyright (C) 2005,2006,2007 MaNGOS <http://www.mangosproject.org/>
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

#include "SQLStorage.h"
#include "ProgressBar.h"
#include "Log.h"

#ifdef DO_POSTGRESQL
extern DatabasePostgre  sDatabase;
#else
extern DatabaseMysql  sDatabase;
#endif

const char CreatureInfofmt[]="iiissiiiiiiiiiififfiiiiiiiiiiiffiiiiiiiiiiiiiiiiiiisiiiiis";
const char CreatureDataAddonInfofmt[]="iiiiiiiiiii";
const char CreatureModelfmt[]="iffii";
const char CreatureInfoAddonInfofmt[]="iiiiiiiiiii";
const char EquipmentInfofmt[]="iiiiiiiiii";
const char GameObjectInfofmt[]="iiisiifiiiiiiiiiiiiiiiiiiiiiiiis";
const char ItemPrototypefmt[]="iiiisiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiffiffiffiffiffiiiiiiiiiifiiifiiiiiifiiiiiifiiiiiifiiiiiifiiiisiiiiiiiiiiiiiiiiiiiiiiiiiifsii";
const char PageTextfmt[]="isi";
const char SpellThreatfmt[]="ii";

SQLStorage sCreatureStorage(CreatureInfofmt,"entry","creature_template");
SQLStorage sCreatureDataAddonStorage(CreatureDataAddonInfofmt,"guid","creature_addon");
SQLStorage sCreatureModelStorage(CreatureModelfmt,"modelid","creature_model_info");
SQLStorage sCreatureInfoAddonStorage(CreatureInfoAddonInfofmt,"entry","creature_template_addon");
SQLStorage sEquipmentStorage(EquipmentInfofmt,"entry","creature_equip_template");
SQLStorage sGOStorage(GameObjectInfofmt,"entry","gameobject_template");
SQLStorage sItemStorage(ItemPrototypefmt,"entry","item_template");
SQLStorage sPageTextStore(PageTextfmt,"entry","page_text");
SQLStorage sSpellThreatStore(SpellThreatfmt,"entry","spell_threat");

void SQLStorage::Free ()
{
    uint32 offset=0;
    for(uint32 x=0;x<iNumFields;x++)
        if(format[x]==FT_STRING)
    {
        for(uint32 y=0;y<MaxEntry;y++)
            if(pIndex[y])
                delete [] *(char**)((char*)(pIndex[y])+offset);

        offset+=sizeof(char*);

    }else offset+=4;

    delete [] pIndex;
    delete [] data;
}

void SQLStorage::Load ()
{
    uint32 maxi;
    Field *fields;
    QueryResult *result  = sDatabase.PQuery("SELECT MAX(`%s`) FROM `%s`",entry_field,table);
    if(!result)
    {
        sLog.outError("Error loading `%s` table (not exist?)\n",table);
        exit(1);                                            // Stop server at loading non exited table or not accessable table
    }

    maxi= (*result)[0].GetUInt32()+1;
    delete result;

    result = sDatabase.PQuery("SELECT COUNT(*) FROM `%s`",table);
    if(result)
    {
        fields = result->Fetch();
        RecordCount=fields[0].GetUInt32();
        delete result;
    }
    else
        RecordCount = 0;

    result = sDatabase.PQuery("SELECT * FROM `%s`",table);

    if(!result)
    {
        sLog.outError("`%s` table is empty!\n",table);
        RecordCount = 0;
        return;
    }

    uint32 recordsize=0;
    uint32 offset=0;

    if(iNumFields!=result->GetFieldCount())
    {
        RecordCount = 0;
        sLog.outError("Error in `%s` table, probably sql file format was updated (there should be %d fields in sql).\n",table,iNumFields);
        delete result;
        exit(1);                                            // Stop server at loading broken or non-compatiable table.
    }

    if(sizeof(char*)==sizeof(uint32))
        recordsize=4*iNumFields;
    else
    {
        //get struct size
        uint32 sc=0;
        for(uint32 x=0;x<iNumFields;x++)
            if(format[x]==FT_STRING)
                sc++;
        recordsize=(iNumFields-sc)*4+sc*sizeof(char*);
    }

    char** newIndex=new char*[maxi];
    memset(newIndex,0,maxi*sizeof(char*));

    char * _data= new char[RecordCount *recordsize];
    uint32 count=0;
    barGoLink bar( RecordCount );
    do
    {
        fields = result->Fetch();
        bar.step();
        char *p=(char*)&_data[recordsize*count];
        newIndex[fields[0].GetUInt32()]=p;

        offset=0;
        for(uint32 x=0;x<iNumFields;x++)
            switch(format[x])
            {
                case FT_INT:
                    *((uint32*)(&p[offset]))=fields[x].GetUInt32();
                    offset+=sizeof(uint32);
                    break;
                case FT_FLOAT:
                    *((float*)(&p[offset]))=fields[x].GetFloat();
                    offset+=sizeof(float);
                    break;
                case FT_STRING:
                    char const* tmp = fields[x].GetString();
                    char* st;
                    if(!tmp)
                    {
                        st=new char[1];
                        *st=0;
                    }
                    else
                    {
                        uint32 l=strlen(tmp)+1;
                        st=new char[l];
                        memcpy(st,tmp,l);
                    }
                    *((char**)(&p[offset]))=st;
                    offset+=sizeof(char*);
                    break;
            }
        count++;
    }while( result->NextRow() );

    delete result;

    pIndex =newIndex;
    MaxEntry=maxi;
    data=_data;
}
