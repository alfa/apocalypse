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
#include "Bag.h"
#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "WorldPacket.h"
#include "UpdateData.h"
#include "WorldSession.h"

Bag::Bag( ): Item()
{
    m_objectType |= TYPE_CONTAINER;
    m_objectTypeId = TYPEID_CONTAINER;

    m_valuesCount = CONTAINER_END;

    memset(m_bagslot, 0, sizeof(Item *) * (20));            // Maximum 20 Slots
}

Bag::~Bag()
{
    for(int i = 0; i<20; i++)
    {
        if(m_bagslot[i])    delete m_bagslot[i];
    }
}

bool Bag::Create(uint32 guidlow, uint32 itemid, Player* owner)
{
    ItemPrototype const * itemProto = objmgr.GetItemPrototype(itemid);

    if(!itemProto || itemProto->ContainerSlots > 20)
        return false;

    Object::_Create( guidlow, HIGHGUID_CONTAINER );

    SetUInt32Value(OBJECT_FIELD_ENTRY, itemid);
    SetFloatValue(OBJECT_FIELD_SCALE_X, 1.0f);

    SetUInt64Value(ITEM_FIELD_OWNER, owner->GetGUID());
    SetUInt64Value(ITEM_FIELD_CONTAINED, owner->GetGUID());

    SetUInt32Value(ITEM_FIELD_MAXDURABILITY, itemProto->MaxDurability);
    SetUInt32Value(ITEM_FIELD_DURABILITY, itemProto->MaxDurability);
    SetUInt32Value(ITEM_FIELD_FLAGS, itemProto->Flags);
    SetUInt32Value(ITEM_FIELD_STACK_COUNT, 1);

    // Setting the number of Slots the Container has
    SetUInt32Value(CONTAINER_FIELD_NUM_SLOTS, itemProto->ContainerSlots);

    // Cleanning 20 slots
    for (uint8 i = 0; i < 20; i++)
    {
        SetUInt64Value(CONTAINER_FIELD_SLOT_1 + (i*2), 0);
        m_bagslot[i] = NULL;
    }

    return true;
}

void Bag::SaveToDB()
{
    Item::SaveToDB();
    sDatabase.PExecute("DELETE FROM `character_inventory` WHERE `guid` = '%u' AND `bag` = '%u'", GUID_LOPART(GetOwnerGUID()), GetSlot());
    for (uint8 i = 0; i < GetProto()->ContainerSlots; i++)
    {
        if (m_bagslot[i])
        {
            sDatabase.PExecute("INSERT INTO `character_inventory`  (`guid`,`bag`,`slot`,`item`,`item_template`) VALUES ('%u', '%u', '%u', '%u', '%u')", GUID_LOPART(GetOwnerGUID()), GetSlot(), i, m_bagslot[i]->GetGUIDLow(), m_bagslot[i]->GetEntry());
            m_bagslot[i]->SaveToDB();
        }
    }
}

bool Bag::LoadFromDB(uint32 guid, uint64 owner_guid, uint32 auctioncheck)
{
    if(!Item::LoadFromDB(guid, owner_guid, auctioncheck))
        return false;

    // cleanup bag content related item value fields (its will be filled correctly from `character_inventory`)
    for (uint32 i = 0; i < GetProto()->ContainerSlots; i++)
    {
        SetUInt64Value(CONTAINER_FIELD_SLOT_1 + (i*2), 0);
        if (m_bagslot[i])
        {
            delete m_bagslot[i];
            m_bagslot[i] = NULL;
        }
    }

    QueryResult *result = sDatabase.PQuery("SELECT `slot`,`item,`item_template` FROM `character_inventory` WHERE `guid` = '%u' AND `bag` = '%u'", GUID_LOPART(GetOwnerGUID()), GetSlot());

    if (result)
    {
        do
        {
            Field *fields = result->Fetch();
            uint8  slot      = fields[0].GetUInt8();
            uint32 item_guid = fields[1].GetUInt32();
            uint32 item_id   = fields[2].GetUInt32();

            ItemPrototype const *proto = objmgr.GetItemPrototype(item_id);

            if(!proto)
            {
                sLog.outError( "Bag::LoadFromDB: Player %d have unknown item (id: #%u) in bag #%u, skipped.", GUID_LOPART(GetOwnerGUID()), item_id, GetSlot());
                continue;
            }

            Item *item = NewItemOrBag(proto);
            item->SetSlot(slot);
            if(!item->LoadFromDB(item_guid, owner_guid, 1))
                continue;
            StoreItem( slot, item, true );
        } while (result->NextRow());

        delete result;
    }
    return true;
}

void Bag::DeleteFromDB()
{
    for (int i = 0; i < 20; i++)
    {
        if (m_bagslot[i])
        {
            m_bagslot[i]->DeleteFromDB();
        }
    }
}

uint8 Bag::FindFreeBagSlot() const
{
    uint32 ContainerSlots=GetProto()->ContainerSlots;
    for (uint8 i=0; i <ContainerSlots; i++)
        if (!m_bagslot[i])
            return i;

    return NULL_SLOT;
}

void Bag::RemoveItem( uint8 slot, bool update )
{
    m_bagslot[slot] = NULL;
    SetUInt64Value( CONTAINER_FIELD_SLOT_1 + (slot * 2), 0 );
}

void Bag::StoreItem( uint8 slot, Item *pItem, bool update )
{
    if( pItem )
    {
        m_bagslot[slot] = pItem;
        SetUInt64Value(CONTAINER_FIELD_SLOT_1 + (slot * 2), pItem->GetGUID());
        pItem->SetUInt64Value(ITEM_FIELD_CONTAINED, GetGUID());
        pItem->SetUInt64Value( ITEM_FIELD_OWNER, GetOwnerGUID() );
        pItem->SetSlot( NULL_SLOT );
    }
}

void Bag::BuildCreateUpdateBlockForPlayer( UpdateData *data, Player *target ) const
{
    Item::BuildCreateUpdateBlockForPlayer( data, target );

    for (int i = 0; i < 20; i++)
    {
        if(m_bagslot[i])
            m_bagslot[i]->BuildCreateUpdateBlockForPlayer( data, target );
    }
}

// If the bag is empty returns true
bool Bag::IsEmpty() const
{
    uint32 ContainerSlots=GetProto()->ContainerSlots;
    for(uint32 i=0; i < ContainerSlots; i++)
        if (m_bagslot[i]) return false;

    return true;
}

uint8 Bag::GetSlotByItemGUID(uint64 guid) const
{
    uint32 ContainerSlots=GetProto()->ContainerSlots;

    for(uint32 i=0;i<ContainerSlots;i++)
    {
        if(m_bagslot[i] != 0)
            if(m_bagslot[i]->GetGUID() == guid)
                return i;
    }
    return NULL_SLOT;
}

// Adds an item to a bag slot
// - slot can be NULL_SLOT, in that case function searchs for a free slot
// - Return values: 0 - item not added
//                  1 - item added to a free slot (and perhaps to a stack)
//                  2 - item added to a stack (item should be deleted)
Item* Bag::GetItemByPos( uint8 slot ) const
{
    ItemPrototype const *pBagProto = GetProto();
    if( pBagProto )
    {
        if( slot < pBagProto->ContainerSlots )
            return m_bagslot[slot];
    }
    return NULL;
}
