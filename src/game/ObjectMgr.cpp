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

#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "UpdateMask.h"
#include "World.h"
#include "WorldSession.h"
#include "Group.h"
#include "Guild.h"
#include "ProgressBar.cpp"

createFileSingleton( ObjectMgr );

ObjectMgr::ObjectMgr()
{
    m_hiCharGuid = 1;
    m_hiCreatureGuid = 1;
    m_hiItemGuid = 1;
    m_hiGoGuid = 1;
    m_hiDoGuid = 1;
    m_hiNameGuid = 1;
}


ObjectMgr::~ObjectMgr()
{

    for( CreatureNameMap::iterator i = mCreatureNames.begin( ); i != mCreatureNames.end( ); ++ i )
    {
        delete i->second;
    }
    mCreatureNames.clear();

    for( QuestMap::iterator i = mQuests.begin( ); i != mQuests.end( ); ++ i )
    {
        delete i->second;
    }
    mQuests.clear( );

    for( ItemPrototypeMap::iterator i = mItemPrototypes.begin( ); i != mItemPrototypes.end( ); ++ i )
    {
        delete i->second;
    }
    mItemPrototypes.clear( );

    for( TrainerspellMap::iterator i = mTrainerspells.begin( ); i != mTrainerspells.end( ); ++ i )
    {
        delete i->second;
    }
    mTrainerspells.clear( );

    for( GossipTextMap::iterator i = mGossipText.begin( ); i != mGossipText.end( ); ++ i )
    {
        delete i->second;
    }
    mGossipText.clear( );

    for( AreaTriggerMap::iterator i = mAreaTriggerMap.begin( ); i != mAreaTriggerMap.end( ); ++ i )
    {
        delete i->second;
    }
    mAreaTriggerMap.clear( );

    for( GameObjectInfoMap::iterator iter = mGameObjectInfo.begin(); iter != mGameObjectInfo.end(); ++iter)
    {
        delete iter->second;
    }
    mGameObjectInfo.clear();

   
}






Group * ObjectMgr::GetGroupByLeader(const uint64 &guid) const
{
    GroupSet::const_iterator itr;
    for (itr = mGroupSet.begin(); itr != mGroupSet.end(); itr++)
    {
        if ((*itr)->GetLeaderGUID() == guid)
        {
            return *itr;
        }
    }

    return NULL;
}




Guild * ObjectMgr::GetGuildById(const uint32 GuildId) const
{
    GuildSet::const_iterator itr;
    for (itr = mGuildSet.begin(); itr != mGuildSet.end(); itr++)
    {
        if ((*itr)->GetId() == GuildId)
        {
            return *itr;
        }
    }

    return NULL;
}




CreatureInfo *ObjectMgr::GetCreatureName(uint32 id)
{
    CreatureNameMap::const_iterator itr = mCreatureNames.find( id );
    if( itr != mCreatureNames.end( ) )
        return itr->second;

    

    
    CreatureInfo *ci=new CreatureInfo;
    ci->Name = "Unknown Being";
    ci->Id=id;
    ci->SubName = "";
    ci->Type = 0;
    ci->unknown1 = 0;
    ci->unknown2 = 0;
    ci->unknown3 = 0;
    ci->unknown4 = 0;
    ci->DisplayID = 0;
    AddCreatureName(ci);
    return ci;
}


uint32 ObjectMgr::AddCreatureName(const char* name)
{
    for( CreatureNameMap::iterator i = mCreatureNames.begin( );
        i != mCreatureNames.end( ); ++ i )
    {
        if (strcmp(i->second->Name.c_str(),name) == 0)
            return i->second->Id;
    }

    uint32 id = m_hiNameGuid++;
    AddCreatureName(id, name);

    std::stringstream ss;
    ss << "INSERT INTO creaturetemplate (entryid,name) VALUES (" << id << ", '" << name << "')";
    sDatabase.Execute( ss.str( ).c_str( ) );

    return id;
}


uint32 ObjectMgr::AddCreatureName(const char* name, uint32 displayid)
{
    for( CreatureNameMap::iterator i = mCreatureNames.begin( );
        i != mCreatureNames.end( ); ++ i )
    {
        if (strcmp(i->second->Name.c_str(),name) == 0)
            return i->second->Id;
    }

    uint32 id = m_hiNameGuid++;
    AddCreatureName(id, name, displayid);

    std::stringstream ss;
    ss << "INSERT INTO creaturetemplate (entryid,name,modelid) VALUES (" << id << ", '" << name << "', '" << displayid << "')";
    sDatabase.Execute( ss.str( ).c_str( ) );

    return id;
}


uint32 ObjectMgr::AddCreatureSubName(const char* name, const char* subname, uint32 displayid)
{
    for( CreatureNameMap::iterator i = mCreatureNames.begin( );
        i != mCreatureNames.end( ); ++ i )
    {
        if (strcmp(i->second->Name.c_str(),name) == 0)
            if (i->second->DisplayID == displayid)
                if (strcmp(i->second->SubName.c_str(),subname) == 0)
                    return i->second->Id;
    }

    uint32 id = m_hiNameGuid++;

    CreatureInfo *cInfo = new CreatureInfo;
    cInfo->DisplayID = displayid;
    cInfo->Id = id;
    cInfo->Name = name;
    cInfo->SubName = subname;
    cInfo->Type = 0;
    cInfo->unknown1 = 0;
    cInfo->unknown2 = 0;
    cInfo->unknown3 = 0;
    cInfo->unknown4 = 0;
    AddCreatureName(cInfo);

    std::stringstream ss;
    ss << "INSERT INTO creaturetemplate (entryid,name,subname,modelid) VALUES (" << id << ", '" << name;
    ss << "', '" << subname << "', '" << displayid << "')";
    sDatabase.Execute( ss.str( ).c_str( ) );

    return id;
}


void ObjectMgr::AddCreatureName(CreatureInfo *cinfo)
{
    ASSERT( mCreatureNames.find(cinfo->Id) == mCreatureNames.end() );
    
    CreatureNameMap::iterator itr = mCreatureNames.find( cinfo->Id );
    
    if( itr != mCreatureNames.end( ) )
        mCreatureNames.erase(itr);
    mCreatureNames[cinfo->Id] = cinfo;
}


void ObjectMgr::AddCreatureName(uint32 id, const char* name)
{
    CreatureInfo *cinfo;
    cinfo = new CreatureInfo;
    cinfo->Id = id;
    cinfo->Name = name;
    cinfo->SubName = "";
    cinfo->Type = 0;
    cinfo->unknown1 = 0;
    cinfo->unknown2 = 0;
    cinfo->unknown3 = 0;
    cinfo->unknown4 = 0;
    cinfo->DisplayID = 0;

    ASSERT( name );
    ASSERT( mCreatureNames.find(id) == mCreatureNames.end() );
    mCreatureNames[id] = cinfo;
}


void ObjectMgr::AddCreatureName(uint32 id, const char* name, uint32 displayid)
{
    CreatureInfo *cinfo;
    cinfo = new CreatureInfo;
    cinfo->Id = id;
    cinfo->Name = name;
    cinfo->SubName = "";
    cinfo->Type = 0;
    cinfo->unknown1 = 0;
    cinfo->unknown2 = 0;
    cinfo->unknown3 = 0;
    cinfo->unknown4 = 0;
    cinfo->DisplayID = displayid;

    ASSERT( name );
    ASSERT( mCreatureNames.find(id) == mCreatureNames.end() );
    mCreatureNames[id] = cinfo;
}



void ObjectMgr::LoadCreatureNames()
{
    CreatureInfo *cn;
	uint32 count = 0;

    
    QueryResult *result = sDatabase.Query( "SELECT * FROM creaturetemplate" );
    if(result)
    {
		
		barGoLink bar( result->GetRowCount() );

        do
        {
            Field *fields = result->Fetch();

			
			bar.step();

			count++;

            cn = new CreatureInfo;
            
            
        
            cn->Id =				fields[0].GetUInt32();
            cn->DisplayID =			fields[1].GetUInt32();
            cn->Name =				fields[2].GetString();
            if (fields[3].GetString())
                cn->SubName =		fields[3].GetString();
            else
                cn->SubName = "";

            cn->maxhealth =			fields[4].GetUInt32();
            cn->maxmana =			fields[5].GetUInt32();
            cn->level =				fields[6].GetUInt32();
            cn->faction =			fields[7].GetUInt32();
            cn->flag =				fields[8].GetUInt32();
			
			if (fields[8].GetUInt32() == uint32(17)) 
			{
				
				cn->flag = uint32(18);
			}
			if (fields[8].GetUInt32() == uint32(5)) 
			{
				
				cn->flag = uint32(4);
			}
			if (fields[8].GetUInt32() == uint32(12)) 
			{
				
				cn->flag = uint32(4);
			}
			if (fields[8].GetUInt32() == uint32(16389)) 
			{
				
				cn->flag = uint32(4);
			}

            cn->scale =				fields[9].GetFloat();
            cn->speed =				fields[10].GetFloat();
            cn->rank =				fields[11].GetUInt32();
            cn->mindmg =			fields[12].GetFloat();
            cn->maxdmg =			fields[13].GetFloat();
            cn->baseattacktime =	fields[14].GetUInt32();
            cn->rangeattacktime =	fields[15].GetUInt32();
            cn->Type =				fields[16].GetUInt32();
            cn->mount =				fields[17].GetUInt32();
            cn->level_max =			fields[18].GetUInt32();
            cn->flags1 =			fields[19].GetUInt32();
            cn->size =				fields[20].GetFloat();
            cn->family =			fields[21].GetUInt32();
            cn->bounding_radius =	fields[22].GetFloat();
            cn->trainer_type =		fields[23].GetUInt32();
            cn->unknown1 =			fields[24].GetUInt32();
            cn->unknown2 =			fields[25].GetUInt32();
            cn->unknown3 =			fields[26].GetUInt32();
            cn->unknown4 =			fields[27].GetUInt32();
            cn->classNum =			fields[28].GetUInt32();
            cn->slot1model =		fields[29].GetUInt32();
            cn->slot1pos =			fields[30].GetUInt32();
            cn->slot2model =		fields[31].GetUInt32();
            cn->slot2pos =			fields[32].GetUInt32();
            cn->slot3model =		fields[33].GetUInt32();
            cn->slot3pos =			fields[34].GetUInt32();

            AddCreatureName( cn );
        } while( result->NextRow() );

        delete result;
    }

    result = sDatabase.Query( "SELECT MAX(entryid) FROM creaturetemplate" );
    if(result)
    {
        m_hiNameGuid = (*result)[0].GetUInt32();

        delete result;
    }

	sLog.outString( ">> Loaded %d creature definitions", count );
	sLog.outString( "" );
}

PlayerCreateInfo* ObjectMgr::GetPlayerCreateInfo(uint32 race, uint32 class_)
{
    uint32 createId;

    QueryResult *player_result, *items_result, *spells_result, *skills_result,*actions_result ;
    Field *player_fields, *items_fields, *spells_fields, *skills_fields, *actions_fields;
    PlayerCreateInfo *pPlayerCreateInfo;

    std::stringstream player_query,item_query,spell_query,skill_query,action_query;

    player_query << "SELECT * FROM playercreateinfo WHERE race = " << race << " AND class = " << class_;
    player_result = sDatabase.Query( player_query.str().c_str() );
    
    if( !player_result ) 
        return NULL;

    pPlayerCreateInfo = new PlayerCreateInfo;

    player_fields = player_result->Fetch();

    pPlayerCreateInfo->createId = player_fields[0].GetUInt8();
    createId = (uint32)pPlayerCreateInfo->createId;
    pPlayerCreateInfo->race = player_fields[1].GetUInt8();
    pPlayerCreateInfo->class_ = player_fields[2].GetUInt8();
    pPlayerCreateInfo->mapId = player_fields[3].GetUInt32();
    pPlayerCreateInfo->zoneId = player_fields[4].GetUInt32();
    pPlayerCreateInfo->positionX = player_fields[5].GetFloat();
    pPlayerCreateInfo->positionY = player_fields[6].GetFloat();
    pPlayerCreateInfo->positionZ = player_fields[7].GetFloat();
    pPlayerCreateInfo->displayId = player_fields[8].GetUInt16();
    pPlayerCreateInfo->strength = player_fields[9].GetUInt8();
    pPlayerCreateInfo->ability = player_fields[10].GetUInt8();
    pPlayerCreateInfo->stamina = player_fields[11].GetUInt8();
    pPlayerCreateInfo->intellect = player_fields[12].GetUInt8();
    pPlayerCreateInfo->spirit = player_fields[13].GetUInt8();
    pPlayerCreateInfo->health = player_fields[14].GetUInt32();
    pPlayerCreateInfo->mana = player_fields[15].GetUInt32();
    pPlayerCreateInfo->rage = player_fields[16].GetUInt32();
    pPlayerCreateInfo->focus = player_fields[17].GetUInt32();
    pPlayerCreateInfo->energy = player_fields[18].GetUInt32();
    pPlayerCreateInfo->attackpower = player_fields[19].GetUInt32();
    pPlayerCreateInfo->mindmg = player_fields[20].GetFloat();
    pPlayerCreateInfo->maxdmg = player_fields[21].GetFloat();

    delete player_result;


    item_query << "SELECT * FROM playercreateinfo_items WHERE createId = 0 OR createId = " << createId ;
    items_result = sDatabase.Query( item_query.str().c_str() );
    do 
    {
        if(!items_result) break;
        items_fields = items_result->Fetch();
        pPlayerCreateInfo->item.push_back(items_fields[1].GetUInt32());
        pPlayerCreateInfo->item_slot.push_back(items_fields[2].GetUInt8());

    } while( items_result->NextRow() );

    delete items_result;
    
    spell_query << "SELECT * FROM playercreateinfo_spells WHERE createId = 0 OR createId = " << createId ;
    spells_result = sDatabase.Query( spell_query.str().c_str() );
    do 
    {
        if(!spells_result) break;
        spells_fields = spells_result->Fetch();
        pPlayerCreateInfo->spell.push_back(spells_fields[1].GetUInt16());

    } while( spells_result->NextRow() );

    delete spells_result;
    
    skill_query << "SELECT * FROM playercreateinfo_skills WHERE createId = 0 OR createId = " << createId ;
    skills_result = sDatabase.Query( skill_query.str().c_str() );
    do 
    {
        if(!skills_result) break;
        skills_fields = skills_result->Fetch();
        pPlayerCreateInfo->skill[0].push_back(skills_fields[1].GetUInt16());
        pPlayerCreateInfo->skill[1].push_back(skills_fields[2].GetUInt16());
        pPlayerCreateInfo->skill[2].push_back(skills_fields[3].GetUInt16());

    } while( skills_result->NextRow() );

    delete skills_result;

    action_query << "SELECT * FROM playercreateinfo_actions WHERE createId = 0 OR createId = " << createId ;
    actions_result = sDatabase.Query( action_query.str().c_str() );
    do 
    {
        if(!actions_result) break;
        actions_fields = actions_result->Fetch();
        pPlayerCreateInfo->action[0].push_back(actions_fields[1].GetUInt16());
        pPlayerCreateInfo->action[1].push_back(actions_fields[2].GetUInt16());
        pPlayerCreateInfo->action[2].push_back(actions_fields[3].GetUInt16());
        pPlayerCreateInfo->action[3].push_back(actions_fields[4].GetUInt16());

    } while( actions_result->NextRow() );

    delete actions_result;

    return pPlayerCreateInfo; 
}

uint64 ObjectMgr::GetPlayerGUIDByName(const char *name) const
{
    uint64 guid = 0;
    std::stringstream query;
    query << "SELECT guid FROM characters WHERE name = '" << name << "'";

    QueryResult *result = sDatabase.Query( query.str().c_str() );
    if(result)
    {
        guid = (*result)[0].GetUInt32();

        delete result;
    }

    return guid;
}


bool ObjectMgr::GetPlayerNameByGUID(const uint64 &guid, std::string &name) const
{
    std::stringstream query;
    query << "SELECT name FROM characters WHERE guid=" << GUID_LOPART(guid);

    QueryResult *result = sDatabase.Query( query.str().c_str() );
    if(result)
    {
        name = (*result)[0].GetString();
        delete result;
        return true;
    }

    return false;
}


void ObjectMgr::LoadAuctions()
{
    QueryResult *result = sDatabase.Query( "SELECT * FROM auctionhouse" );

    if( !result )
        return;

    AuctionEntry *aItem;

    do
    {
        Field *fields = result->Fetch();

        aItem = new AuctionEntry;
        aItem->auctioneer = fields[0].GetUInt32();
        aItem->item = fields[1].GetUInt32();
        aItem->owner = fields[2].GetUInt32();
        aItem->buyout = fields[3].GetUInt32();
        aItem->time = fields[4].GetUInt32();
        aItem->bidder = fields[5].GetUInt32();
        aItem->bid = fields[6].GetUInt32();
        aItem->Id = fields[7].GetUInt32();
        AddAuction(aItem);
    } while (result->NextRow());
    delete result;

}

int num_item_prototypes = 0;
uint32 item_proto_ids[64550];





void CheckItemDamageValues ( ItemPrototype *itemProto )
{
	uint8 add_damage_types[7]; 
	int num_damages_added = 0;

	if ( itemProto->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD && (itemProto->Block <= 0 || itemProto->Block > 999) )
	{
		uint32 block;

		
		if (itemProto->ItemLevel > 40)
		{
			block = float(itemProto->ItemLevel+urand(0, 5));
		}
		else if (itemProto->ItemLevel > 30)
		{
			block = float(itemProto->ItemLevel+urand(0, 10));
		}
		else if (itemProto->ItemLevel > 20)
		{
			block = float(itemProto->ItemLevel+urand(0, 15));
		}
		else if (itemProto->ItemLevel > 10)
		{
			block = float(itemProto->ItemLevel+urand(0, 9));
		}
		else if (itemProto->ItemLevel > 5)
		{
			block = float(itemProto->ItemLevel+urand(0, 4));
		}
		else
		{
			block = float(itemProto->ItemLevel+1);
		}

		if (block <= 4)
			block = urand(0, 5);

		itemProto->Block = block;
	}

	if ((itemProto->Class == ITEM_CLASS_WEAPON || itemProto->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD)
		&& (itemProto->Sheath <= 0 || itemProto->Sheath > 7) )
	{

		
		if (itemProto->SubClass == ITEM_SUBCLASS_WEAPON_STAFF)
		{
			itemProto->Sheath = 1;
		}
		else if (itemProto->InventoryType == INVTYPE_2HWEAPON)
		{
			itemProto->Sheath = 1;
		}
		else if (itemProto->InventoryType == INVTYPE_SHIELD)
		{
			itemProto->Sheath = 4;
		}
		
		else if (itemProto->InventoryType == INVTYPE_WEAPONMAINHAND)
		{
			itemProto->Sheath = 3;
		}
		else if (itemProto->InventoryType == INVTYPE_WEAPONOFFHAND)
		{
			itemProto->Sheath = 3;
		}
		else if (itemProto->InventoryType == INVTYPE_RANGEDRIGHT)
		{
			itemProto->Sheath = 3;
		}
		else if (itemProto->InventoryType == INVTYPE_WEAPON)
		{
			itemProto->Sheath = 3;
		}
		else
		{
			itemProto->Sheath = 7;
		}
	}

	if ( itemProto->Class != ITEM_CLASS_WEAPON 
		&& itemProto->Class != ITEM_CLASS_PROJECTILE 
		 )
		return; 

	if (itemProto->ItemLevel > 255)
		itemProto->ItemLevel = 0; 




	memset(&add_damage_types, 0, sizeof(add_damage_types));

	for (int i = 0; i < 7; i++)
    {
		if (i == 0 
			 )
		{
			
			add_damage_types[NORMAL_DAMAGE] = 1; 

			if (itemProto->AllowableClass & CLASS_WARRIOR)
			{
				if (itemProto->ItemLevel > 48)
				{
					if (irand(0, 10) < 7)
					{
						add_damage_types[FIRE_DAMAGE] = 1;
					}
				}
				else if (itemProto->ItemLevel > 24)
				{
					if (irand(0, 10) < 3)
					{
						add_damage_types[FIRE_DAMAGE] = 1;
					}
				}
			}
			else if (itemProto->AllowableClass & CLASS_PALADIN)
			{
				if (itemProto->ItemLevel > 48)
				{
					if (irand(0, 10) < 6)
					{
						add_damage_types[FIRE_DAMAGE] = 1;
					}
				}
				else if (itemProto->ItemLevel > 24)
				{
					if (irand(0, 10) < 2)
					{
						add_damage_types[FIRE_DAMAGE] = 1;
					}
				}
			}
			else if (itemProto->AllowableClass & CLASS_HUNTER)
			{
				if (itemProto->ItemLevel > 48)
				{
					if (irand(0, 10) < 6)
					{
						add_damage_types[NATURE_DAMAGE] = 1;
					}
				}
				else if (itemProto->ItemLevel > 24)
				{
					if (irand(0, 10) < 2)
					{
						add_damage_types[NATURE_DAMAGE] = 1;
					}
				}
			}
			else if (itemProto->AllowableClass & CLASS_ROGUE)
			{
				int choice = irand(0, 4);

				
				if (choice == 0)
				{
					if (itemProto->ItemLevel > 48)
					{
						if (irand(0, 10) < 6)
						{
							add_damage_types[NATURE_DAMAGE] = 1;
						}
					}
					else if (itemProto->ItemLevel > 24)
					{
						if (irand(0, 10) < 2)
						{
							add_damage_types[NATURE_DAMAGE] = 1;
						}
					}
				}
				else if (choice == 2)
				{
					if (itemProto->ItemLevel > 48)
					{
						if (irand(0, 10) < 6)
						{
							add_damage_types[SHADOW_DAMAGE] = 1;
						}
					}
					else if (itemProto->ItemLevel > 24)
					{
						if (irand(0, 10) < 2)
						{
							add_damage_types[SHADOW_DAMAGE] = 1;
						}
					}
				}
				else if (choice == 3)
				{
					if (itemProto->ItemLevel > 48)
					{
						if (irand(0, 10) < 6)
						{
							add_damage_types[FROST_DAMAGE] = 1;
						}
					}
					else if (itemProto->ItemLevel > 24)
					{
						if (irand(0, 10) < 2)
						{
							add_damage_types[FROST_DAMAGE] = 1;
						}
					}
				}
				else
				{
					if (itemProto->ItemLevel > 48)
					{
						if (irand(0, 10) < 6)
						{
							add_damage_types[FIRE_DAMAGE] = 1;
						}
					}
					else if (itemProto->ItemLevel > 24)
					{
						if (irand(0, 10) < 2)
						{
							add_damage_types[FIRE_DAMAGE] = 1;
						}
					}
				}
			}
			else if (itemProto->AllowableClass & CLASS_PRIEST)
			{
				int choice = irand(0, 3);

				
				if (choice == 0)
				{
					if (itemProto->ItemLevel > 48)
					{
						if (irand(0, 10) < 6)
						{
							add_damage_types[SHADOW_DAMAGE] = 1;
						}
					}
					else if (itemProto->ItemLevel > 24)
					{
						if (irand(0, 10) < 2)
						{
							add_damage_types[SHADOW_DAMAGE] = 1;
						}
					}
				}
				else
				{
					if (itemProto->ItemLevel > 48)
					{
						if (irand(0, 8) < 6)
						{
							add_damage_types[HOLY_DAMAGE] = 1;
						}
					}
					else if (itemProto->ItemLevel > 24)
					{
						if (irand(0, 9) < 2)
						{
							add_damage_types[HOLY_DAMAGE] = 1;
						}
					}					
				}
			}
			else if (itemProto->AllowableClass & CLASS_SHAMAN)
			{
				int choice = irand(0, 2);

				
				if (choice == 0)
				{
					if (itemProto->ItemLevel > 48)
					{
						if (irand(0, 10) < 6)
						{
							add_damage_types[SHADOW_DAMAGE] = 1;
						}
					}
					else if (itemProto->ItemLevel > 24)
					{
						if (irand(0, 10) < 2)
						{
							add_damage_types[SHADOW_DAMAGE] = 1;
						}
					}
				}
				else if (choice == 1)
				{
					if (itemProto->ItemLevel > 48)
					{
						if (irand(0, 10) < 6)
						{
							add_damage_types[FROST_DAMAGE] = 1;
						}
					}
					else if (itemProto->ItemLevel > 24)
					{
						if (irand(0, 10) < 2)
						{
							add_damage_types[FROST_DAMAGE] = 1;
						}
					}
				}
				else
				{
					if (itemProto->ItemLevel > 48)
					{
						if (irand(0, 10) < 6)
						{
							add_damage_types[FIRE_DAMAGE] = 1;
						}
					}
					else if (itemProto->ItemLevel > 24)
					{
						if (irand(0, 10) < 2)
						{
							add_damage_types[FIRE_DAMAGE] = 1;
						}
					}
				}
			}
			else if (itemProto->AllowableClass & CLASS_MAGE)
			{
				int choice = irand(0, 2);

				
				if (choice == 0)
				{
					if (itemProto->ItemLevel > 48)
					{
						if (irand(0, 10) < 6)
						{
							add_damage_types[FROST_DAMAGE] = 1;
						}
					}
					else if (itemProto->ItemLevel > 24)
					{
						if (irand(0, 10) < 2)
						{
							add_damage_types[FROST_DAMAGE] = 1;
						}
					}
				}
				else if (choice == 1)
				{
					if (itemProto->ItemLevel > 48)
					{
						if (irand(0, 10) < 6)
						{
							add_damage_types[ARCANE_DAMAGE] = 1;
						}
					}
					else if (itemProto->ItemLevel > 24)
					{
						if (irand(0, 10) < 2)
						{
							add_damage_types[ARCANE_DAMAGE] = 1;
						}
					}
				}
				else
				{
					if (itemProto->ItemLevel > 48)
					{
						if (irand(0, 10) < 6)
						{
							add_damage_types[FIRE_DAMAGE] = 1;
						}
					}
					else if (itemProto->ItemLevel > 24)
					{
						if (irand(0, 10) < 2)
						{
							add_damage_types[FIRE_DAMAGE] = 1;
						}
					}
				}
			}
			else if (itemProto->AllowableClass & CLASS_WARLOCK)
			{
				int choice = irand(0, 1);

				
				if (choice == 0)
				{
					if (itemProto->ItemLevel > 48)
					{
						if (irand(0, 10) < 6)
						{
							add_damage_types[SHADOW_DAMAGE] = 1;
						}
					}
					else if (itemProto->ItemLevel > 24)
					{
						if (irand(0, 10) < 2)
						{
							add_damage_types[SHADOW_DAMAGE] = 1;
						}
					}
				}
				else
				{
					if (itemProto->ItemLevel > 48)
					{
						if (irand(0, 10) < 6)
						{
							add_damage_types[FIRE_DAMAGE] = 1;
						}
					}
					else if (itemProto->ItemLevel > 24)
					{
						if (irand(0, 10) < 2)
						{
							add_damage_types[FIRE_DAMAGE] = 1;
						}
					}
				}
			}
			else if (itemProto->AllowableClass & CLASS_DRUID)
			{
				if (itemProto->ItemLevel > 48)
				{
					if (irand(0, 10) < 6)
					{
						add_damage_types[NATURE_DAMAGE] = 1;
					}
				}
				else if (itemProto->ItemLevel > 24)
				{
					if (irand(0, 10) < 2)
					{
						add_damage_types[NATURE_DAMAGE] = 1;
					}
				}
			}
			else
			{
				uint8 damage_type = urand(NORMAL_DAMAGE, ARCANE_DAMAGE);

				if (itemProto->ItemLevel > 60)
				{
					if (irand(0, 8) < 6)
					{
						add_damage_types[damage_type] = 1;
					}

					uint8 old_type = damage_type;

					damage_type = urand(NORMAL_DAMAGE, ARCANE_DAMAGE);
					
					while (old_type == damage_type)
						damage_type = urand(NORMAL_DAMAGE, ARCANE_DAMAGE);

					if (irand(0, 10) < 6)
					{
						add_damage_types[damage_type] = 1;
					}

					uint8 old_type2 = damage_type;

					damage_type = urand(NORMAL_DAMAGE, ARCANE_DAMAGE);

					while (old_type == damage_type || old_type2 == damage_type)
						damage_type = urand(NORMAL_DAMAGE, ARCANE_DAMAGE);

					if (irand(0, 10) < 6)
					{
						add_damage_types[damage_type] = 1;
					}
				}
				else if (itemProto->ItemLevel > 54)
				{
					if (irand(0, 8) < 6)
					{
						add_damage_types[damage_type] = 1;
					}

					uint8 old_type = damage_type;

					damage_type = urand(NORMAL_DAMAGE, ARCANE_DAMAGE);
					
					while (old_type == damage_type)
						damage_type = urand(NORMAL_DAMAGE, ARCANE_DAMAGE);

					if (irand(0, 10) < 6)
					{
						add_damage_types[damage_type] = 1;
					}
				}
				else if (itemProto->ItemLevel > 48)
				{
					if (irand(0, 10) < 6)
					{
						add_damage_types[damage_type] = 1;
					}
				}
				else if (itemProto->ItemLevel > 24)
				{
					if (irand(0, 10) < 3)
					{
						add_damage_types[damage_type] = 1;
					}
				}
			}
		}

		if ( add_damage_types[i] >= 1 || i == 0 )
		{
			float mindmg;
			float maxdmg;

			
			if (itemProto->ItemLevel > 40)
			{
				mindmg = float(itemProto->ItemLevel-urand(0, 5));
			}
			else if (itemProto->ItemLevel > 30)
			{
				mindmg = float(itemProto->ItemLevel-urand(0, 10));
			}
			else if (itemProto->ItemLevel > 20)
			{
				mindmg = float(itemProto->ItemLevel-urand(0, 15));
			}
			else if (itemProto->ItemLevel > 10)
			{
				mindmg = float(itemProto->ItemLevel-urand(0, 9));
			}
			else if (itemProto->ItemLevel > 5)
			{
				mindmg = float(itemProto->ItemLevel-urand(0, 4));
			}
			else
			{
				mindmg = float(itemProto->ItemLevel-1);
			}
	    
			if (i != 0)
			{
				if (itemProto->ItemLevel > 48)
					mindmg *= 0.5;
				else
					mindmg *= 0.25;
			}

			if (mindmg <= 0)
				mindmg = float(1);

			
			if (itemProto->ItemLevel > 40)
			{
				maxdmg = float(itemProto->ItemLevel+urand( 1, (itemProto->ItemLevel+10) ));
			}
			else if (itemProto->ItemLevel > 20)
			{
				maxdmg = float(itemProto->ItemLevel+urand(1, 30));
			}
			else if (itemProto->ItemLevel > 10)
			{
				maxdmg = float(itemProto->ItemLevel+urand(1, 15));
			}
			else if (itemProto->ItemLevel > 5)
			{
				maxdmg = float(itemProto->ItemLevel+urand(1, 8));
			}
			else
			{
				maxdmg = float(itemProto->ItemLevel+urand(1, 4));
			}
	    
			if (i != 0)
			{
				if (itemProto->ItemLevel > 48)
					maxdmg *= 0.5;
				else
					maxdmg *= 0.25;
			}

			if (maxdmg <= 1)
				maxdmg = float(2);

			if (mindmg > maxdmg)
			{
				float max = mindmg;

				mindmg = maxdmg;
				maxdmg = max;
			}

			
			itemProto->DamageMin[num_damages_added] = float(mindmg);
			itemProto->DamageMax[num_damages_added] = float(maxdmg);
			itemProto->DamageType[num_damages_added] = i;
			num_damages_added++;

			if (num_damages_added > 5) 
				break;

			
		}
    }

	if (num_damages_added >= 1)
		return;

	if ( itemProto->DamageMin[0] <= 0 || itemProto->DamageMax[0] <= 0 || (itemProto->DamageMin[0] > itemProto->DamageMax[0]) )
	{
		float mindmg;
		float maxdmg;

		
		if (itemProto->ItemLevel > 40)
		{
			mindmg = float(itemProto->ItemLevel-urand(0, 5));
		}
		else if (itemProto->ItemLevel > 30)
		{
			mindmg = float(itemProto->ItemLevel-urand(0, 10));
		}
		else if (itemProto->ItemLevel > 20)
		{
			mindmg = float(itemProto->ItemLevel-urand(0, 15));
		}
		else if (itemProto->ItemLevel > 10)
		{
			mindmg = float(itemProto->ItemLevel-urand(0, 9));
		}
		else if (itemProto->ItemLevel > 5)
		{
			mindmg = float(itemProto->ItemLevel-urand(0, 4));
		}
		else
		{
			mindmg = float(itemProto->ItemLevel-1);
		}
	    
		if (mindmg <= 0)
			mindmg = float(1);

		
		if (itemProto->ItemLevel > 40)
		{
			maxdmg = float(itemProto->ItemLevel+urand( 1, (itemProto->ItemLevel+10) ));
		}
		else if (itemProto->ItemLevel > 20)
		{
			maxdmg = float(itemProto->ItemLevel+urand(1, 30));
		}
		else if (itemProto->ItemLevel > 10)
		{
			maxdmg = float(itemProto->ItemLevel+urand(1, 15));
		}
		else if (itemProto->ItemLevel > 5)
		{
			maxdmg = float(itemProto->ItemLevel+urand(1, 8));
		}
		else
		{
			maxdmg = float(itemProto->ItemLevel+urand(1, 4));
		}
	    
		if (maxdmg <= 1)
			maxdmg = float(2);

		if (mindmg > maxdmg)
		{
			float max = mindmg;

			mindmg = maxdmg;
			maxdmg = max;
		}

		itemProto->DamageMin[0] = mindmg;
		itemProto->DamageMax[0] = maxdmg;
		itemProto->DamageType[0] = NORMAL_DAMAGE;

		
	}

}

void ObjectMgr::LoadItemPrototypes()
{
	
	

    QueryResult *result = sDatabase.Query( "SELECT * FROM items" );

    if( !result )
        return;


    barGoLink bar( result->GetRowCount() );

    ItemPrototype *pItemPrototype;
    int i;

    memset(&item_proto_ids,-1,sizeof(item_proto_ids));

    if( result->GetFieldCount() < 113 )
    {
        Log::getSingleton().outError("DB: Items table has incorrect number of fields");
        delete result;
        return;
    }

    do
    {
        Field *fields = result->Fetch();

        bar.step();

        if( !fields[0].GetUInt32() )
        {
            Log::getSingleton().outBasic("DB: Incorrect item id found");
            continue;
        }

        pItemPrototype = new ItemPrototype;

        pItemPrototype->ItemId = fields[0].GetUInt32();

        
        item_proto_ids[num_item_prototypes] = fields[0].GetUInt32();
        

        pItemPrototype->Class = fields[2].GetUInt32();
        pItemPrototype->SubClass = fields[3].GetUInt32();
        pItemPrototype->Name1 = fields[4].GetString();
        pItemPrototype->Name2 = fields[5].GetString();
        pItemPrototype->Name3 = fields[6].GetString();
        pItemPrototype->Name4 = fields[7].GetString();
        pItemPrototype->DisplayInfoID = fields[8].GetUInt32();
        pItemPrototype->Quality = fields[9].GetUInt32();
        pItemPrototype->Flags = fields[10].GetUInt32();
        pItemPrototype->BuyPrice = fields[11].GetUInt32();
        pItemPrototype->SellPrice = fields[12].GetUInt32();
        pItemPrototype->InventoryType = fields[13].GetUInt32();
        pItemPrototype->AllowableClass = fields[14].GetUInt32();
        pItemPrototype->AllowableRace = fields[15].GetUInt32();
        pItemPrototype->ItemLevel = fields[16].GetUInt32();
        pItemPrototype->RequiredLevel = fields[17].GetUInt32();
        pItemPrototype->RequiredSkill = fields[18].GetUInt32();
        pItemPrototype->RequiredSkillRank = fields[19].GetUInt32();
        pItemPrototype->Field20 = fields[20].GetUInt32();
        pItemPrototype->Field21 = fields[21].GetUInt32();
        pItemPrototype->Field22 = fields[22].GetUInt32();
        pItemPrototype->Field23 = fields[23].GetUInt32();
        pItemPrototype->MaxCount = fields[24].GetUInt32();
        pItemPrototype->ContainerSlots = fields[25].GetUInt32();
        for(i = 0; i < 20; i+=2)
        {
            pItemPrototype->ItemStatType[i/2] = fields[26 + i].GetUInt32();
            pItemPrototype->ItemStatValue[i/2] = fields[27 + i].GetUInt32();
        }
        for(i = 0; i < 15; i+=3)
        {
            
            int *a=(int *)malloc(sizeof(int)); *a=fields[46 + i].GetUInt32();
            int *b=(int *)malloc(sizeof(int)); *b=fields[47 + i].GetUInt32();

            pItemPrototype->DamageMin[i/3] = *(float *)a;
            pItemPrototype->DamageMax[i/3] = *(float *)b;
            
            
            pItemPrototype->DamageType[i/3] = fields[48 + i].GetUInt32();

            free(a);free(b);
        }

        pItemPrototype->Armor = fields[61].GetUInt32();
        pItemPrototype->HolyRes = fields[62].GetUInt32();
        pItemPrototype->FireRes = fields[63].GetUInt32();
        pItemPrototype->NatureRes = fields[64].GetUInt32();
        pItemPrototype->FrostRes = fields[65].GetUInt32();
        pItemPrototype->ShadowRes = fields[66].GetUInt32();
        pItemPrototype->ArcaneRes = fields[67].GetUInt32();
        pItemPrototype->Delay = fields[68].GetUInt32();
        if (pItemPrototype->Delay <= 0)
            pItemPrototype->Delay = 1000; 

        pItemPrototype->Field69 = fields[69].GetUInt32();
        for(i = 0; i < 30; i+=6)
        {
            pItemPrototype->SpellId[i/6] = fields[70+i].GetUInt32();
            pItemPrototype->SpellTrigger[i/6] = fields[71+i].GetUInt32();
            pItemPrototype->SpellCharges[i/6] = fields[72+i].GetUInt32();
            pItemPrototype->SpellCooldown[i/6] = fields[73+i].GetUInt32();
            pItemPrototype->SpellCategory[i/6] = fields[74+i].GetUInt32();
            pItemPrototype->SpellCategoryCooldown[i/6] = fields[75+i].GetUInt32();
        }
        pItemPrototype->Bonding = fields[100].GetUInt32();
        pItemPrototype->Description = fields[101].GetString();
        pItemPrototype->Field102 = fields[102].GetUInt32();
        pItemPrototype->Field103 = fields[103].GetUInt32();
        pItemPrototype->Field104 = fields[104].GetUInt32();
        pItemPrototype->Field105 = fields[105].GetUInt32();
        pItemPrototype->Field106 = fields[106].GetUInt32();
        pItemPrototype->Field107 = fields[107].GetUInt32();
        pItemPrototype->Field108 = fields[108].GetUInt32();
        pItemPrototype->Field109 = fields[109].GetUInt32();
        pItemPrototype->Field110 = fields[110].GetUInt32();
        pItemPrototype->Field111 = fields[111].GetUInt32();
        pItemPrototype->MaxDurability = fields[112].GetUInt32();

		
		CheckItemDamageValues( pItemPrototype );

		
		

        AddItemPrototype(pItemPrototype);

		num_item_prototypes++;
    } while( result->NextRow() );

    delete result;

	sLog.outString( "" );
	sLog.outString( ">> Loaded %d item prototypes", num_item_prototypes );


}


uint32 default_trainer_guids[12];

void ObjectMgr::LoadTrainerSpells()
{
    QueryResult *result = sDatabase.Query( "SELECT * FROM trainer" );

    if( !result )
        return;

	int loop;
	uint32 count = 0;

	for (loop = 0; loop < 12; loop++)
	{
		default_trainer_guids[loop] = 0;
	}

    Trainerspell *TrainSpell;


    barGoLink bar( result->GetRowCount() );

    do
    {
        Field *fields = result->Fetch();

        bar.step();

		count++;

        TrainSpell = new Trainerspell;

        TrainSpell->Id = fields[0].GetUInt32();
        TrainSpell->skilline1 = fields[1].GetUInt32();
        TrainSpell->skilline2 = fields[2].GetUInt32();
        TrainSpell->skilline3 = fields[3].GetUInt32();
        TrainSpell->skilline4 = fields[4].GetUInt32();
        TrainSpell->skilline5 = fields[5].GetUInt32();
        TrainSpell->skilline6 = fields[6].GetUInt32();
        TrainSpell->skilline7 = fields[7].GetUInt32();
        TrainSpell->skilline8 = fields[8].GetUInt32();
        TrainSpell->skilline9 = fields[9].GetUInt32();
        TrainSpell->skilline10 = fields[10].GetUInt32();
        TrainSpell->skilline11 = fields[11].GetUInt32();
        TrainSpell->skilline12 = fields[12].GetUInt32();
        TrainSpell->skilline13 = fields[13].GetUInt32();
        TrainSpell->skilline14 = fields[14].GetUInt32();
        TrainSpell->skilline15 = fields[15].GetUInt32();
        TrainSpell->skilline16 = fields[16].GetUInt32();
        TrainSpell->skilline17 = fields[17].GetUInt32();
        TrainSpell->skilline18 = fields[18].GetUInt32();
        TrainSpell->skilline19 = fields[19].GetUInt32();
        TrainSpell->skilline20 = fields[20].GetUInt32();
        TrainSpell->maxlvl = fields[21].GetUInt32();
        TrainSpell->charclass = fields[22].GetUInt32();

		if ( default_trainer_guids[TrainSpell->charclass] == 0 
			&& TrainSpell->skilline1 && TrainSpell->skilline2 && TrainSpell->skilline3 )
			default_trainer_guids[TrainSpell->charclass] = TrainSpell->Id;
        
		AddTrainerspell(TrainSpell);
    } while (result->NextRow());
    delete result;


	sLog.outString( "" );
	sLog.outString( ">> Loaded %d trainer definitions", count );
}


void ObjectMgr::LoadAuctionItems()
{
    QueryResult *result = sDatabase.Query( "SELECT * FROM auctioned_items" );

    if( !result )
        return;
    Field *fields;
    do
    {
        fields = result->Fetch();
        Item* item = new Item;
        item->LoadFromDB(fields[0].GetUInt32(), 2);
        AddAItem(item);
    }
    while( result->NextRow() );

    delete result;
}


void ObjectMgr::LoadMailedItems()
{
    QueryResult *result = sDatabase.Query( "SELECT * FROM mailed_items" );

    if( !result )
        return;
    Field *fields;
    do
    {
        fields = result->Fetch();
        Item* item = new Item;
        item->LoadFromDB(fields[0].GetUInt32(), 3);
        AddMItem(item);
    }
    while( result->NextRow() );

    delete result;
}

void ObjectMgr::LoadGuilds()
{
	Guild *newguild;
	QueryResult *result = sDatabase.Query( "SELECT guildId FROM guilds" );
	uint32 count = 0;
	
	if( !result ) 
	{
		
		barGoLink bar( 1 );
		
		bar.step();

		sLog.outString( "" );
		sLog.outString( ">> Loaded %d guild definitions", count );
		return;
	}
	
	
	barGoLink bar( result->GetRowCount() );
	
	do
	{
		Field *fields = result->Fetch();
		
		
		bar.step();
		count++;
		
		newguild = new Guild;
		newguild->LoadGuildFromDB(fields[0].GetUInt32());
		AddGuild(newguild);
		
	}while( result->NextRow() );

	sLog.outString( "" );
	sLog.outString( ">> Loaded %d guild definitions", count );
}

void ObjectMgr::LoadQuests()
{
	QueryResult *result = sDatabase.Query( "SELECT * FROM quests" );

    if( !result ) return;

	
	barGoLink bar( result->GetRowCount() );

    Quest *pQuest;
	uint32 count = 0;
	int iCalc;
	int CiC;

    do {
        Field *fields = result->Fetch();

		
		bar.step();

        pQuest = new Quest;
		iCalc = 0;

		pQuest->m_qId        = fields[ iCalc++ ].GetUInt32();
		pQuest->m_qCategory  = fields[ iCalc++ ].GetUInt32();
		pQuest->m_qFlags     = fields[ iCalc++ ].GetUInt32();

		pQuest->m_qTitle         = fields[ iCalc++ ].GetString();
        pQuest->m_qDetails       = fields[ iCalc++ ].GetString();
        pQuest->m_qObjectives    = fields[ iCalc++ ].GetString();

		pQuest->m_qCompletionInfo  = fields[ iCalc++ ].GetString();
		pQuest->m_qIncompleteInfo  = fields[ iCalc++ ].GetString();
		pQuest->m_qEndInfo         = fields[ iCalc++ ].GetString();

		for (CiC = 0; CiC < QUEST_OBJECTIVES_COUNT; CiC++)
			pQuest->m_qObjectiveInfo[CiC]    = fields[ iCalc++ ].GetString();

		pQuest->m_qPlayerLevel      = fields[ iCalc++ ].GetUInt32();
		pQuest->m_qComplexityLevel  = fields[ iCalc++ ].GetUInt32();

		pQuest->m_qRequiredQuestsCount = fields[ iCalc++ ].GetUInt32();
		for ( CiC = 0; CiC < QUEST_DEPLINK_COUNT; CiC++)
		{ pQuest->m_qRequiredQuests[CiC] = fields[ iCalc++ ].GetUInt32(); }

        pQuest->m_qRequiredAbsQuestsCount = fields[ iCalc++ ].GetUInt32();
		for ( CiC = 0; CiC < QUEST_DEPLINK_COUNT; CiC++)
		{ pQuest->m_qRequiredAbsQuests[CiC] = fields[ iCalc++ ].GetUInt32(); }

		pQuest->m_qLockerQuestsCount = fields[ iCalc++ ].GetUInt32();
		for ( CiC = 0; CiC < QUEST_DEPLINK_COUNT; CiC++)
		{ pQuest->m_qLockerQuests[CiC] = fields[ iCalc++ ].GetUInt32(); }

		for (CiC = 0; CiC < QUEST_OBJECTIVES_COUNT; CiC++)
			pQuest->m_qObjItemId[CiC]    = fields[ iCalc++ ].GetUInt32();

		for (CiC = 0; CiC < QUEST_OBJECTIVES_COUNT; CiC++)
			pQuest->m_qObjItemCount[CiC]    = fields[ iCalc++ ].GetUInt32();

		for (CiC = 0; CiC < QUEST_OBJECTIVES_COUNT; CiC++)
			pQuest->m_qObjMobId[CiC]    = fields[ iCalc++ ].GetUInt32();

		for (CiC = 0; CiC < QUEST_OBJECTIVES_COUNT; CiC++)
			pQuest->m_qObjMobCount[CiC]    = fields[ iCalc++ ].GetUInt32();


		pQuest->m_qRewChoicesCount = fields[ iCalc++ ].GetUInt32();
		for ( CiC = 0; CiC < QUEST_REWARD_CHOICES_COUNT; CiC++)
		{ pQuest->m_qRewChoicesItemId[CiC] = fields[ iCalc++ ].GetUInt32(); }

		for ( CiC = 0; CiC < QUEST_REWARD_CHOICES_COUNT; CiC++)
		{ pQuest->m_qRewChoicesItemCount[CiC] = fields[ iCalc++ ].GetUInt32(); }

		pQuest->m_qRewCount = fields[ iCalc++ ].GetUInt32();
		for ( CiC = 0; CiC < QUEST_REWARDS_COUNT; CiC++)
		{ pQuest->m_qRewItemId[CiC] = fields[ iCalc++ ].GetUInt32(); }

		for ( CiC = 0; CiC < QUEST_REWARDS_COUNT; CiC++)
		{ pQuest->m_qRewItemCount[CiC] = fields[ iCalc++ ].GetUInt32(); }


		pQuest->m_qRewMoney         = fields[ iCalc++ ].GetUInt32();
		pQuest->m_qObjRepFaction_1  = fields[ iCalc++ ].GetUInt32();
        pQuest->m_qObjRepFaction_2  = fields[ iCalc++ ].GetUInt32();
		pQuest->m_qObjRepValue_1    = fields[ iCalc++ ].GetUInt32();
        pQuest->m_qObjRepValue_2    = fields[ iCalc++ ].GetUInt32();

		pQuest->m_qQuestItem     = fields[ iCalc++ ].GetUInt32();
		pQuest->m_qNextQuest     = fields[ iCalc++ ].GetUInt32();
		pQuest->m_qRewSpell      = fields[ iCalc++ ].GetUInt32();
		pQuest->m_qObjTime       = fields[ iCalc++ ].GetUInt32();

		pQuest->m_qType          = fields[ iCalc++ ].GetUInt32();
		pQuest->m_qRequiredRaces = fields[ iCalc++ ].GetUInt32();
		pQuest->m_qRequiredClass = fields[ iCalc++ ].GetUInt32();
		pQuest->m_qRequiredTradeskill   = fields[ iCalc++ ].GetUInt32();
		pQuest->m_qSpecialFlags  = fields[ iCalc++ ].GetUInt32();

		pQuest->m_qPointId		 = fields[ iCalc++ ].GetUInt32();
		pQuest->m_qPointX    	 = fields[ iCalc++ ].GetFloat();
		pQuest->m_qPointY		 = fields[ iCalc++ ].GetFloat();
		pQuest->m_qPointOpt		 = fields[ iCalc++ ].GetUInt32();

		count++;
        AddQuest(pQuest);
    }
    while( result->NextRow() );

    delete result;

	sLog.outString( "" );
	sLog.outString( ">> Loaded %d quest definitions", count );
}



void ObjectMgr::AddGossipText(GossipText *pGText)
{
	ASSERT( pGText->Text_ID );
    ASSERT( mGossipText.find(pGText->Text_ID) == mGossipText.end() );
    mGossipText[pGText->Text_ID] = pGText;
}


GossipText *ObjectMgr::GetGossipText(uint32 Text_ID)
{
    GossipTextMap::const_iterator itr;
    for (itr = mGossipText.begin(); itr != mGossipText.end(); itr++)
    {
        if(itr->second->Text_ID == Text_ID)
            return itr->second;
    }
    return NULL;
}


void ObjectMgr::LoadGossipText()
{
	GossipText *pGText;
	QueryResult *result = sDatabase.Query( "SELECT * FROM npc_text" );

	int count = 0;
	if( !result ) return;
	int cic;

	
	barGoLink bar( result->GetRowCount() );

	do {
		count++;
		cic = 0;

		Field *fields = result->Fetch();

		
		bar.step();

		pGText = new GossipText;
		pGText->Text_ID    = fields[cic++].GetUInt32();

		for (int i=0; i< 8; i++)
		{
			pGText->Options[i].Text_0           = fields[cic++].GetString();
			pGText->Options[i].Text_1           = fields[cic++].GetString();

			pGText->Options[i].Language         = fields[cic++].GetUInt32();
			pGText->Options[i].Probability      = fields[cic++].GetFloat();

			pGText->Options[i].Emotes[0]._Delay  = fields[cic++].GetUInt32();
			pGText->Options[i].Emotes[0]._Emote  = fields[cic++].GetUInt32();

			pGText->Options[i].Emotes[1]._Delay  = fields[cic++].GetUInt32();
			pGText->Options[i].Emotes[1]._Emote  = fields[cic++].GetUInt32();

			pGText->Options[i].Emotes[2]._Delay  = fields[cic++].GetUInt32();
			pGText->Options[i].Emotes[2]._Emote  = fields[cic++].GetUInt32();
		}
        
		if ( !pGText->Text_ID ) continue;
		AddGossipText( pGText );

	} while( result->NextRow() );

	sLog.outString( "" );
	sLog.outString( ">> Loaded %d npc texts", count );
	delete result;
}


ItemPage *ObjectMgr::RetreiveItemPageText(uint32 Page_ID)
{
	ItemPage *pIText;
	std::stringstream Query;

	Query << "SELECT * FROM item_pages WHERE pageid = " << Page_ID;
	QueryResult *result = sDatabase.Query( Query.str().c_str() );

	if( !result ) return NULL;
	int cic, count = 0;
	pIText = new ItemPage;

	do {
		count++;
		cic = 0;

		Field *fields = result->Fetch();


		pIText->Page_ID    = fields[cic++].GetUInt32();

		pIText->PageText   = fields[cic++].GetString();
		pIText->Next_Page  = fields[cic++].GetUInt32();

		if ( !pIText->Page_ID ) break;
	} while( result->NextRow() );

	delete result;
	return pIText;
}


void ObjectMgr::AddAreaTriggerPoint(AreaTriggerPoint *pArea)
{
	ASSERT( pArea->Trigger_ID );
	ASSERT( mAreaTriggerMap.find(pArea->Trigger_ID) == mAreaTriggerMap.end() );

	mAreaTriggerMap[pArea->Trigger_ID] = pArea;
}


AreaTriggerPoint *ObjectMgr::GetAreaTriggerQuestPoint(uint32 Trigger_ID)
{
    AreaTriggerMap::const_iterator itr;
    for (itr = mAreaTriggerMap.begin(); itr != mAreaTriggerMap.end(); itr++)
    {
        if(itr->second->Trigger_ID == Trigger_ID)
            return itr->second;
    }
    return NULL;
}


void ObjectMgr::LoadAreaTriggerPoints()
{
	int count = 0;
	QueryResult *result = sDatabase.Query( "SELECT * FROM triggerquestrelation" );
	AreaTriggerPoint *pArea;

	if( !result ) return;

	
	barGoLink bar( result->GetRowCount() );

	do {
		count++;

		
		bar.step();

		pArea = new AreaTriggerPoint;

		Field *fields = result->Fetch();

		
		
		pArea->Trigger_ID      = fields[0].GetUInt32();
		pArea->Quest_ID        = fields[1].GetUInt32();
		pArea->Creature_ID     = fields[2].GetUInt32();

		AddAreaTriggerPoint( pArea );

	} while( result->NextRow() );

	sLog.outString( "" );
	sLog.outString( ">> Loaded %d quest trigger points", count );
	delete result;
}









bool ObjectMgr::GetGlobalTaxiNodeMask( uint32 curloc, uint32 *Mask )
{
    
    std::stringstream query;
    query << "SELECT taxipath.destination FROM taxipath WHERE taxipath.source = " << curloc << " ORDER BY destination LIMIT 1";
    Log::getSingleton( ).outDebug(" STRING %s ",query.str().c_str());

    std::auto_ptr<QueryResult> result(sDatabase.Query( query.str().c_str() ));

    if( result.get() == NULL )
    {
        return 1;
    }
    Field *fields = result->Fetch();
    uint8 destination = fields[0].GetUInt8();
    uint8 field = (uint8)((destination - 1) / 32);
    Mask[field] |= 1 << ( (destination - 1 ) % 32 );

    return 1;
}


uint32 ObjectMgr::GetNearestTaxiNode( float x, float y, float z, uint32 mapid )
{
    

    std::stringstream query;

    query << "SELECT taxinodes.ID, SQRT(pow(taxinodes.x-(" << x << "),2)+pow(taxinodes.y-(" << y << "),2)+pow(taxinodes.z-(" << z << "),2)) as distance ";
    query << "FROM taxinodes WHERE taxinodes.continent = " << mapid << " ORDER BY distance LIMIT 1";

    

    std::auto_ptr<QueryResult> result(sDatabase.Query( query.str().c_str() ));

    if( result.get() == NULL )
    {
        return 0;
    }
    Field *fields = result->Fetch();
    return fields[0].GetUInt8();

    
}


void ObjectMgr::GetTaxiPath( uint32 source, uint32 destination, uint32 &path, uint32 &cost)
{
    

    std::stringstream query;

    query << "SELECT taxipath.price, taxipath.ID FROM taxipath WHERE taxipath.source = " << source << " AND taxipath.destination = " << destination;
    
    Log::getSingleton( ).outDebug(" STRING %s ",query.str().c_str());

    std::auto_ptr<QueryResult> result(sDatabase.Query( query.str().c_str() ));

    if( result.get() == NULL )
    {
        path = 0;
        cost = 0;
        return;
    }
    Field *fields = result->Fetch();
    cost = fields[0].GetUInt32();
    path = fields[1].GetUInt16();
}


uint16 ObjectMgr::GetTaxiMount( uint32 id )
{
    std::stringstream query;

    query << "SELECT taxinodes.mount FROM taxinodes WHERE taxinodes.ID = " << id;
    
    Log::getSingleton( ).outDebug(" STRING %s ",query.str().c_str());

    std::auto_ptr<QueryResult> result(sDatabase.Query( query.str().c_str() ));

    if( result.get() == NULL )
    {
        return 0;
    }

    Field *fields = result->Fetch();
    return fields[0].GetUInt16();

    
}


void ObjectMgr::GetTaxiPathNodes( uint32 path, Path *pathnodes )
{
    std::stringstream query;

    query << "SELECT taxipathnodes.X, taxipathnodes.Y, taxipathnodes.Z FROM taxipathnodes WHERE taxipathnodes.path = " << path;
    
    Log::getSingleton( ).outDebug(" STRING %s ",query.str().c_str());

    std::auto_ptr<QueryResult> result(sDatabase.Query( query.str().c_str() ));

    if( result.get() == NULL )
    {
         pathnodes->setLength( 0 );
    }
    
    uint16 count = result->GetRowCount();

    Log::getSingleton( ).outDebug(" ROW COUNT %u ",count);

    pathnodes->setLength( count );

    uint16 i = 0;

    do
    {
        Field *fields = result->Fetch();
        pathnodes->getNodes( )[ i ].x = fields[0].GetFloat();
        pathnodes->getNodes( )[ i ].y = fields[1].GetFloat();
        pathnodes->getNodes( )[ i ].z = fields[2].GetFloat();
        i++;
    } while( result->NextRow() );

    
}






GraveyardTeleport *ObjectMgr::GetClosestGraveYard(float x, float y, float z, uint32 MapId)
{
    std::stringstream query;
    query << "SELECT SQRT(POW(" << x << "-X,2)+POW(" << y << "-Y,2)+POW(" << z << "-Z,2)) as distance,X,Y,Z,mapId from graveyards where mapId = " << MapId << " ORDER BY distance ASC LIMIT 1";

    std::auto_ptr<QueryResult> result(sDatabase.Query( query.str().c_str() ));

    if( result.get() == NULL )
        return NULL;
    
    Field *fields = result->Fetch();
    GraveyardTeleport *pgrave = new GraveyardTeleport;
    
    pgrave->X = fields[1].GetFloat();
    pgrave->Y = fields[2].GetFloat();
    pgrave->Z = fields[3].GetFloat();
    pgrave->MapId = fields[4].GetUInt32();

    return pgrave;
}

AreaTrigger *ObjectMgr::GetAreaTrigger(uint32 trigger)
{
    std::stringstream query;

    query << "SELECT totrigger FROM areatriggers WHERE id = " << trigger;

    std::auto_ptr<QueryResult> result(sDatabase.Query(query.str().c_str()));

    if (result.get() != NULL )
    {
        Field *fields = result->Fetch();
        uint32 totrigger = fields[0].GetUInt32();
        if( totrigger != 0)
        {
            std::stringstream query1;

            query1 << "SELECT mapid,coord_x,coord_y,coord_z FROM areatriggers WHERE id = " << totrigger;

            std::auto_ptr<QueryResult> result1(sDatabase.Query(query1.str().c_str()));

            if ( result1.get() != NULL )
            {
                Field *fields1 = result1->Fetch();
                AreaTrigger *at = new AreaTrigger;

                at->mapId = fields1[0].GetUInt32();

                at->X = fields1[1].GetFloat();
                at->Y = fields1[2].GetFloat();
                at->Z = fields1[3].GetFloat();

                return at;
            }
        }
    }
    return NULL;
}

void ObjectMgr::LoadTeleportCoords()
{

    QueryResult *result = sDatabase.Query( "SELECT * FROM teleport_cords" );

    if( !result )
        return;

	uint32 count = 0;

    TeleportCoords *pTC;


    barGoLink bar( result->GetRowCount() );

    do
    {
        Field *fields = result->Fetch();

        bar.step();

		count++;

        pTC = new TeleportCoords;
        pTC->id = fields[0].GetUInt32();
        pTC->mapId = fields[1].GetUInt32();
        pTC->x = fields[2].GetFloat();
        pTC->y = fields[3].GetFloat();
        pTC->z = fields[4].GetFloat();

        AddTeleportCoords(pTC);

    } while( result->NextRow() );

    delete result;

	sLog.outString( "" );
	sLog.outString( ">> Loaded %d teleport definitions", count );
}


void ObjectMgr::SetHighestGuids()
{
    QueryResult *result;
    uint32 corpseshi=0;
    uint32 gameobjectshi=0;

    result = sDatabase.Query( "SELECT MAX(guid) FROM characters" );
    if( result )
    {
        m_hiCharGuid = (*result)[0].GetUInt32()+1;

        delete result;
    }

    result = sDatabase.Query( "SELECT MAX(id) FROM creatures" );
    if( result )
    {
        m_hiCreatureGuid = (*result)[0].GetUInt32()+1;

        delete result;
    }

    result = sDatabase.Query( "SELECT MAX(guid) FROM item_instances" );
    if( result )
    {
        m_hiItemGuid = (*result)[0].GetUInt32()+1;

        delete result;
    }

    
    
    result = sDatabase.Query( "SELECT MAX(modelid) FROM creaturetemplate" );
    if( result )
    {
        m_hiNameGuid = (*result)[0].GetUInt32()+1;

        delete result;
    }

    result = sDatabase.Query( "SELECT MAX(id) FROM gameobjects" );
    if( result )
    {
        gameobjectshi = (*result)[0].GetUInt32()+1;

        delete result;
    }

    result = sDatabase.Query( "SELECT MAX(Id) FROM auctionhouse" );
    if( result )
    {
        m_auctionid = (*result)[0].GetUInt32()+1;

        delete result;
    }
    else
    {
        m_auctionid = 0;
    }
    result = sDatabase.Query( "SELECT MAX(mailId) FROM mail" );
    if( result )
    {
        m_mailid = (*result)[0].GetUInt32()+1;

        delete result;
    }
    else
    {
        m_mailid = 0;
    }
    
    result = sDatabase.Query( "SELECT MAX(guid) FROM corpses" );
    if( result )
    {
        corpseshi = (*result)[0].GetUInt32()+1;

        delete result;
    }
    if(corpseshi > gameobjectshi )
    {
        m_hiGoGuid = corpseshi;
    }
    else
    {
        m_hiGoGuid = gameobjectshi;
    }
}


uint32 ObjectMgr::GenerateAuctionID()
{
    objmgr.m_auctionid++;
    return objmgr.m_auctionid;
}


uint32 ObjectMgr::GenerateMailID()
{
    objmgr.m_mailid++;
    return objmgr.m_mailid;
}


uint32 ObjectMgr::GenerateLowGuid(uint32 guidhigh)
{
    uint32 guidlow = 0;

    switch(guidhigh)
    {
        case HIGHGUID_ITEM          : guidlow = objmgr.m_hiItemGuid++;     break;
        
        case HIGHGUID_UNIT          : guidlow = objmgr.m_hiCreatureGuid++; break;
        case HIGHGUID_PLAYER        : guidlow = objmgr.m_hiCharGuid++;     break;
        case HIGHGUID_GAMEOBJECT    : guidlow = objmgr.m_hiGoGuid++;       break;
        case HIGHGUID_CORPSE        : guidlow = objmgr.m_hiGoGuid++;       break;
        case HIGHGUID_DYNAMICOBJECT : guidlow = objmgr.m_hiDoGuid++;       break;
        default                     : ASSERT(guidlow);
    }

    return guidlow;
}



GameObjectInfo ObjectMgr::si_UnknownGameObjectInfo;

void ObjectMgr::LoadGameobjectInfo()
{
	GameObjectInfo *goI;
	uint32 count = 0;
	std::stringstream query;
	query << "SELECT * FROM gameobjecttemplate";

	QueryResult* result = sDatabase.Query( query.str().c_str() );
	
	if (result) 
	{
		
		barGoLink bar( result->GetRowCount() );

		do {
			Field *fields = result->Fetch();

			
			bar.step();

			goI = new GameObjectInfo;
			goI->id = fields[0].GetUInt32();
			goI->type = fields[1].GetUInt32();
			goI->displayId = fields[2].GetUInt32();
			if (!fields[3].GetString())
			{
				
				goI->name = "Unnamed Gameobject";
			}
			else
			{
				goI->name = fields[3].GetString();
			}

			goI->faction = fields[4].GetUInt32();
			goI->flags = fields[5].GetUInt32();
			goI->size = fields[6].GetFloat();
			
			goI->sound0 = fields[7].GetUInt32();
			goI->sound1 = fields[8].GetUInt32();
			goI->sound2 = fields[9].GetUInt32();
			goI->sound3 = fields[10].GetUInt32();
			goI->sound4 = fields[11].GetUInt32();
			goI->sound5 = fields[12].GetUInt32();
			goI->sound6 = fields[13].GetUInt32();
			goI->sound7 = fields[14].GetUInt32();
			goI->sound8 = fields[15].GetUInt32();
			goI->sound9 = fields[16].GetUInt32();
			



			AddGameobjectInfo( goI );
			count++;
		} while (result->NextRow());
		delete result;
	}

	sLog.outString( ">> Loaded %d game object templates", count );
	sLog.outString( "" );
}

void ObjectMgr::AddGameobjectInfo (GameObjectInfo *goinfo)
{
	ASSERT( mGameObjectInfo.find(goinfo->id) == mGameObjectInfo.end() );

    
	GameObjectInfoMap::iterator itr = mGameObjectInfo.find( goinfo->id );
    
	
    if (itr != mGameObjectInfo.end())
		mGameObjectInfo.erase(itr);

	mGameObjectInfo[goinfo->id] = goinfo;
}


