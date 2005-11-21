/* Item.cpp
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

#include "Common.h"
#include "Item.h"
#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"

Item::Item( )
{
    m_objectType |= TYPE_ITEM;
    m_objectTypeId = TYPEID_ITEM;

    m_valuesCount = ITEM_END;
}

uint32 GetRandPropertiesSeedfromDisplayInfoDBC(uint32 DisplayID)
{
	ItemDisplayTemplateEntry *itemDisplayTemplateEntry = sItemDisplayTemplateStore.LookupEntry( DisplayID );

	return itemDisplayTemplateEntry->seed;
}

uint32 GetRandPropertiesIDfromDisplayInfoDBC(uint32 DisplayID)
{
	ItemDisplayTemplateEntry *itemDisplayTemplateEntry = sItemDisplayTemplateStore.LookupEntry( DisplayID );

	return itemDisplayTemplateEntry->randomPropertyID;
}

char GetImageFilefromDisplayInfoDBC(uint32 DisplayID)
{
	ItemDisplayTemplateEntry *itemDisplayTemplateEntry = sItemDisplayTemplateStore.LookupEntry( DisplayID );

	return itemDisplayTemplateEntry->imageFile;
}

char GetInventoryImageFilefromDisplayInfoDBC(uint32 DisplayID)
{
	ItemDisplayTemplateEntry *itemDisplayTemplateEntry = sItemDisplayTemplateStore.LookupEntry( DisplayID );

	return itemDisplayTemplateEntry->invImageFile;
}

void Item::UpdateStats()
{// UQ1: Set up all stats for the item in realtime... ***FIX ME IF WE WANT FULL ITEM INFO***
	//return;
/*
	if (!this)
		return;

	if (!m_itemProto)
		return;

	//SetUInt32Value( OBJECT_FIELD_ENTRY, m_itemProto->DisplayInfoID);
	SetUInt32Value( ITEM_FIELD_MAXDURABILITY, m_itemProto->MaxDurability);
	
	// UQ1: FIXME: These values should be stored somewhere to update in realtime...
	//SetUInt32Value( ITEM_FIELD_DURABILITY, m_itemProto->MaxDurability);

	SetUInt32Value( ITEM_FIELD_SPELL_CHARGES, m_itemProto->SpellCharges[0]);
	SetUInt32Value( ITEM_FIELD_SPELL_CHARGES+1, m_itemProto->SpellCharges[1]);
	SetUInt32Value( ITEM_FIELD_SPELL_CHARGES+2, m_itemProto->SpellCharges[2]);
	SetUInt32Value( ITEM_FIELD_SPELL_CHARGES+3, m_itemProto->SpellCharges[3]);
	SetUInt32Value( ITEM_FIELD_SPELL_CHARGES+4, m_itemProto->SpellCharges[4]);

    SetUInt32Value( ITEM_FIELD_FLAGS, m_itemProto->Flags);
    SetUInt32Value( ITEM_FIELD_ITEM_TEXT_ID, m_itemProto->DisplayInfoID);
	SetUInt32Value( ITEM_FIELD_DURATION, m_itemProto->Delay);
	
	// Should already be set...
	//SetUInt32Value( ITEM_FIELD_ENCHANTMENT, m_itemProto->);//                  =   22,  //  21 ENCHANTMENT

	SetUInt32Value( ITEM_FIELD_PROPERTY_SEED, m_itemProto->DisplayInfoID);
	// UQ1: This isn't how to get the value... Don't know how...
	//SetUInt32Value( ITEM_FIELD_RANDOM_PROPERTIES_ID, GetRandPropertiesIDfromDisplayInfoDBC(m_itemProto->DisplayInfoID));

*/

/*
	ITEM_FIELD_OWNER
    ITEM_FIELD_CONTAINED
    ITEM_FIELD_CREATOR
    ITEM_FIELD_GIFTCREATOR
    ITEM_FIELD_STACK_COUNT
    ITEM_FIELD_DURATION
    ITEM_FIELD_SPELL_CHARGES                =   16,      //  5 of them
    ITEM_FIELD_FLAGS
    ITEM_FIELD_ENCHANTMENT                  =   22,      //  21 of them
    ITEM_FIELD_PROPERTY_SEED
    ITEM_FIELD_RANDOM_PROPERTIES_ID
    ITEM_FIELD_ITEM_TEXT_ID
    ITEM_FIELD_DURABILITY
    ITEM_FIELD_MAXDURABILITY
*/
}

void Item::Create( uint32 guidlow, uint32 itemid, Player *owner )
{
    Object::_Create( guidlow, HIGHGUID_ITEM );

    SetUInt32Value( OBJECT_FIELD_ENTRY, itemid );
    SetFloatValue( OBJECT_FIELD_SCALE_X, 1.0f );

    SetUInt64Value( ITEM_FIELD_OWNER, owner->GetGUID() );
    SetUInt64Value( ITEM_FIELD_CONTAINED, owner->GetGUID() );
    SetUInt32Value( ITEM_FIELD_STACK_COUNT, 1 );

    m_itemProto = objmgr.GetItemPrototype( itemid );
    ASSERT(m_itemProto);

    //for(int i=5;i<m_valuesCount;i++)
    //     SetUInt32Value( i, 1 );

    SetUInt32Value( ITEM_FIELD_MAXDURABILITY, m_itemProto->MaxDurability);
    SetUInt32Value( ITEM_FIELD_DURABILITY, m_itemProto->MaxDurability);

/*
    ITEM_FIELD_OWNER                        =    6,  //  2  UINT64
    ITEM_FIELD_CONTAINED                    =    8,  //  2  UINT64
    ITEM_FIELD_CREATOR                      =   10,  //  2  UINT64
    ITEM_FIELD_GIFTCREATOR                  =   12,  //  2  UINT64
    ITEM_FIELD_STACK_COUNT                  =   14,  //  1  UINT32
    ITEM_FIELD_DURATION                     =   15,  //  1  UINT32
    ITEM_FIELD_SPELL_CHARGES                =   16,  //  5  SPELLCHARGES
    ITEM_FIELD_FLAGS                        =   21,  //  1  UINT32
    ITEM_FIELD_ENCHANTMENT                  =   22,  //  21 ENCHANTMENT
    ITEM_FIELD_PROPERTY_SEED                =   43,  //  1  UINT32
    ITEM_FIELD_RANDOM_PROPERTIES_ID         =   44,  //  1  UINT32
    ITEM_FIELD_ITEM_TEXT_ID                 =   45,  //  1  UINT32 // goods writing serial number 
    ITEM_FIELD_DURABILITY                   =   46,  //  1  UINT32
    ITEM_FIELD_MAXDURABILITY                =   47,  //  1  UINT32
*/
    SetUInt32Value( ITEM_FIELD_SPELL_CHARGES, m_itemProto->SpellCharges[0]);
    SetUInt32Value( ITEM_FIELD_SPELL_CHARGES+1, m_itemProto->SpellCharges[1]);
    SetUInt32Value( ITEM_FIELD_SPELL_CHARGES+2, m_itemProto->SpellCharges[2]);
    SetUInt32Value( ITEM_FIELD_SPELL_CHARGES+3, m_itemProto->SpellCharges[3]);
    SetUInt32Value( ITEM_FIELD_SPELL_CHARGES+4, m_itemProto->SpellCharges[4]);
    SetUInt32Value( ITEM_FIELD_FLAGS, m_itemProto->Flags);
    SetUInt32Value( ITEM_FIELD_ITEM_TEXT_ID, m_itemProto->DisplayInfoID);
    m_owner = owner;

	// UQ1: FIXME: This one should be the GUID of the maker (NPC or Blacksmith...)
	// SetUInt64Value( ITEM_FIELD_CREATOR, owner->GetGUID()); // UQ1: Should be only used for player-made items...
	// UQ1: FIXME: This one should be the GUID of the gift's maker (NPC or Blacksmith...)
    // SetUInt64Value( ITEM_FIELD_GIFTCREATOR, owner->GetGUID());
	
	// UQ1: I disabled this again.. Think it's a running count, not the max...
	//SetUInt32Value( ITEM_FIELD_STACK_COUNT, m_itemProto->MaxCount);

	SetUInt32Value( ITEM_FIELD_DURATION, m_itemProto->Delay);
    // How we do this?? Need to save enchented items back to the DB per player ???
	//SetUInt32Value( ITEM_FIELD_ENCHANTMENT, m_itemProto->);//                  =   22,  //  21 ENCHANTMENT
    //SetUInt32Value( ITEM_FIELD_PROPERTY_SEED, GetRandPropertiesSeedfromDisplayInfoDBC(m_itemProto->DisplayInfoID));
	SetUInt32Value( ITEM_FIELD_PROPERTY_SEED, m_itemProto->DisplayInfoID);
	//SetUInt32Value( ITEM_FIELD_RANDOM_PROPERTIES_ID, GetRandPropertiesIDfromDisplayInfoDBC(m_itemProto->DisplayInfoID));

	/*
	CONTAINER_FIELD_NUM_SLOTS               =   48,
    CONTAINER_ALIGN_PAD                     =   49,
    CONTAINER_FIELD_SLOT_1                  =   50,      //  40 of them
	*/

	// UQ1: We can't do this!!!
/*	if (m_itemProto->ContainerSlots > 0)
	{
		SetUInt32Value( CONTAINER_FIELD_NUM_SLOTS, m_itemProto->ContainerSlots );
		//CONTAINER_ALIGN_PAD                     =   49,
		//CONTAINER_FIELD_SLOT_1                  =   50,      //  40 of them
	}*/

	// UQ1: Set these too??? I see theres no armor, or damage values above!
	/*
	UNIT_FIELD_ARMOR 
	UNIT_FIELD_MAXDAMAGE
	UNIT_FIELD_MINDAMAGE
	UNIT_FIELD_MAXRANGEDDAMAGE
	UNIT_FIELD_MINRANGEDDAMAGE
    UNIT_FIELD_MAXHEALTH
    UNIT_FIELD_MAXOFFHANDDAMAGE
    UNIT_FIELD_MINOFFHANDDAMAGE
	*/


/*	SetUInt32Value( UNIT_FIELD_ARMOR, m_itemProto->Armor);

	// UQ1: Calculate damage values...
	float dmg_min = 0.0f, dmg_max = 0.0f;

	for (int loop = 0; loop < 6; loop++)
	{
		dmg_max += m_itemProto->DamageMax[loop];
		dmg_min += m_itemProto->DamageMin[loop];
	}

	SetFloatValue( UNIT_FIELD_MAXDAMAGE, dmg_max);
	SetFloatValue( UNIT_FIELD_MINDAMAGE, dmg_min);
*/

	//SetUInt32Value( UNIT_FIELD_MAXRANGEDDAMAGE, m_itemProto->);
	//SetUInt32Value( UNIT_FIELD_MINRANGEDDAMAGE, m_itemProto->Armor);

}


void Item::SaveToDB()
{
    std::stringstream ss;
    ss << "DELETE FROM item_instances WHERE guid = " << GetGUIDLow();
    sDatabase.Execute( ss.str( ).c_str( ) );

    ss.rdbuf()->str("");
    ss << "INSERT INTO item_instances (guid, data) VALUES ("
        << GetGUIDLow() << ", '";                 // TODO: use full guids

    for(uint16 i = 0; i < m_valuesCount; i++ )
        ss << GetUInt32Value(i) << " ";

    ss << "' )";

    sDatabase.Execute( ss.str().c_str() );
}


void Item::LoadFromDB(uint32 guid, uint32 auctioncheck)
{
    std::stringstream ss;
    if (auctioncheck == 1)
    {
        ss << "SELECT data FROM item_instances WHERE guid = " << guid;
    }
    else if (auctioncheck == 2)
    {
        ss << "SELECT data FROM auctioned_items WHERE guid = " << guid;
    }
    else
    {
        ss << "SELECT data FROM mailed_items WHERE guid = " << guid;
    }

    QueryResult *result = sDatabase.Query( ss.str().c_str() );
    if(result==NULL)
        return;
    //ASSERT(result);

    Field *fields = result->Fetch();

    LoadValues( fields[0].GetString() );

    delete result;

    m_itemProto = objmgr.GetItemPrototype( GetUInt32Value(OBJECT_FIELD_ENTRY) );
    ASSERT(m_itemProto);

}


void Item::DeleteFromDB()
{
    std::stringstream ss;
    ss << "DELETE FROM item_instances WHERE guid = " << GetGUIDLow();
    sDatabase.Execute( ss.str( ).c_str( ) );
}
