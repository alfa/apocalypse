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

#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "UpdateMask.h"

void WorldSession::HandleAuctionHelloOpcode( WorldPacket & recv_data )
{
    uint64 guid;

    recv_data >> guid;

    Creature *unit = ObjectAccessor::Instance().GetCreature(*_player, guid);

    if (!unit)
    {
        sLog.outDebug( "WORLD: HandleAuctionHelloOpcode - (%u) NO SUCH UNIT! (GUID: %u)", uint32(GUID_LOPART(guid)), guid );
        return;
    }

    if( unit->IsHostileTo(_player))                         // do not talk with enemies
        return;

    if( !unit->isAuctioner())                               // it's not auctioner
        return;

    SendAuctionHello(guid);
}

void WorldSession::SendAuctionHello( uint64 guid )
{
    WorldPacket data;
    data.Initialize( MSG_AUCTION_HELLO );
    data << guid;
    data << uint32(0);

    SendPacket( &data );
}

void WorldSession::HandleAuctionListBidderItems( WorldPacket & recv_data )
{
    uint64 guid;
    float unknownAuction;

    recv_data >> guid;
    recv_data >> unknownAuction;

    WorldPacket data;

    data.Initialize( SMSG_AUCTION_BIDDER_LIST_RESULT );
    uint32 cnt = 0;
    Player *pl = GetPlayer();
    for (ObjectMgr::AuctionEntryMap::iterator itr = objmgr.GetAuctionsBegin();itr != objmgr.GetAuctionsEnd();itr++)
    {
        if (pl && itr->second->bidder == pl->GetGUIDLow())
            cnt++;
    }
    if (cnt < 51)
    {
        data << cnt;
    }
    else
    {
        data << uint32(50);
    }
    uint32 cnter = 1;
                                                            // added
    for (ObjectMgr::AuctionEntryMap::iterator itr = objmgr.GetAuctionsBegin();itr != objmgr.GetAuctionsEnd();itr++)
    {
        //cycle, which finds all players auctions
        AuctionEntry *Aentry = itr->second;
        Player *pl = GetPlayer();
        if( Aentry && pl && Aentry->bidder == pl->GetGUIDLow() && (cnter < 51))
        {
            data << Aentry->Id;
            Item *it = objmgr.GetAItem(Aentry->item);
            data << it->GetUInt32Value(OBJECT_FIELD_ENTRY);
            data << uint32(1);
            data << uint32(0);
            data << uint32(0);
            data << uint32(it->GetCount());
            data << uint32(0);
            data << it->GetOwnerGUID();
            data << Aentry->bid;
            data << uint32(0);
            data << Aentry->buyout;
            data << uint32((Aentry->time - time(NULL)) * 1000);
            data << uint64(0);
            data << Aentry->bid;
            cnter++;
        }
    }
    data << cnt;
    SendPacket(&data);
}

void WorldSession::HandleAuctionPlaceBid( WorldPacket & recv_data )
{
    uint64 auctioneer;
    uint32 auction;
    uint32 price;
    WorldPacket data;
    recv_data >> auctioneer;
    recv_data >> auction >> price;
    AuctionEntry *ah = objmgr.GetAuction(auction);
    Player *pl = GetPlayer();
    if ((ah) && (ah->owner != pl->GetGUIDLow()))
    {
        if ((price < ah->buyout) || (ah->buyout == 0))
        {
            if (ah->bidder > 0)
            {
                Mail* n = new Mail;
                n->messageID = objmgr.GenerateMailID();
                n->sender = ah->owner;
                n->receiver = ah->bidder;
                n->subject = "You have lost a bid";
                n->body = "";
                n->item = 0;
                n->time = time(NULL) + (30 * DAY);
                n->money = ah->bid;
                n->COD = 0;
                n->checked = 0;

                uint64 rc = ah->bidder;
                Player *rpl = objmgr.GetPlayer(rc);

                sDatabase.PExecute("DELETE FROM `mail` WHERE `id` = '%u'", n->messageID);
                sDatabase.PExecute("INSERT INTO `mail` (`id`,`sender`,`receiver`,`subject`,`body`,`item`,`time`,`money`,`cod`,`checked`) VALUES( '%u', '%u', '%u', '%s', '%s', '%u', '" I64FMTD "', '%u', '%u', '%u')", n->messageID , n->sender , n->receiver , n->subject.c_str() , n->body.c_str(), n->item , (uint64)n->time ,n->money ,n->COD ,n->checked);

                if (rpl)
                {
                    rpl->AddMail(n);
                }
            }

            ah->bidder = pl->GetGUIDLow();
            ah->bid = price;
            objmgr.RemoveAuction(ah->Id);
            objmgr.AddAuction(ah);

            // after this update we should save player!
            sDatabase.PExecute("UPDATE `auctionhouse` SET `buyguid` = '%u',`lastbid` = '%u' WHERE `id` = '%u';", ah->bidder, ah->bid, ah->Id);

            pl->ModifyMoney(-int32(price));

            uint64 guid = auctioneer;

            data.Initialize( SMSG_AUCTION_BIDDER_LIST_RESULT );
            uint32 cnt = 0;
            Player *pl = GetPlayer();
                                                            // added
            for (ObjectMgr::AuctionEntryMap::iterator itr = objmgr.GetAuctionsBegin();itr != objmgr.GetAuctionsEnd();itr++)
            {
                //we need count of auctions of this player
                AuctionEntry *Aentry = itr->second;
                if( Aentry && pl && Aentry->bidder == pl->GetGUIDLow())
                    cnt++;
            }
            if (cnt < 51)
            {
                data << cnt;
            }
            else
            {
                data << uint32(50);
            }
            uint32 cnter = 1;

            for (ObjectMgr::AuctionEntryMap::iterator itr = objmgr.GetAuctionsBegin();itr != objmgr.GetAuctionsEnd();itr++)
            {
                //prepare data to send
                AuctionEntry *Aentry = itr->second;
                if( Aentry && pl && Aentry->bidder == pl->GetGUIDLow() && (cnter < 51))
                {
                    data << Aentry->Id;
                    Item *it = objmgr.GetAItem(Aentry->item);
                    data << it->GetUInt32Value(OBJECT_FIELD_ENTRY);
                    data << uint32(1);
                    data << uint32(0);
                    data << uint32(0);
                    data << uint32(it->GetCount());
                    data << uint32(0);
                    data << it->GetOwnerGUID();
                    data << Aentry->bid;
                    data << uint32(0);
                    data << Aentry->buyout;
                    data << uint32((Aentry->time - time(NULL)) * 1000);
                    data << uint64(0);
                    data << Aentry->bid;
                    cnter++;
                }
            }

            data << cnt;
            SendPacket(&data);
            data.clear();
            data.Initialize( SMSG_AUCTION_LIST_RESULT );
            data << uint32(0);
            data << uint32(0);
            SendPacket(&data);
        }
        else
        {
            pl->ModifyMoney(-int32(ah->buyout));
            Mail *m = new Mail;
            m->messageID = objmgr.GenerateMailID();
            m->sender = ah->owner;
            m->receiver = pl->GetGUIDLow();
            m->subject = "You won an item!";
            m->body = "";
            m->item = ah->item;
            m->time = time(NULL) + (29 * DAY);
            m->money = 0;
            m->COD = 0;
            m->checked = 0;

            if (ah->bidder > 0)                             // mail to last bidder if there's one... + return money
            {
                Mail *mn2 = new Mail;
                mn2->messageID = objmgr.GenerateMailID();
                mn2->sender = ah->owner;
                mn2->receiver = ah->bidder;
                mn2->subject = "You lost a bid!";
                mn2->body = "Item has been bought";
                mn2->item = 0;
                mn2->time = time(NULL) + (30 * DAY);
                mn2->money = ah->bid;
                mn2->COD = 0;
                mn2->checked = 0;

                Player *rpl2 = objmgr.GetPlayer((uint64)ah->bidder);

                sDatabase.PExecute("DELETE FROM `mail` WHERE `id` = '%u'", mn2->messageID);
                sDatabase.PExecute("INSERT INTO `mail` (`id`,`sender`,`receiver`,`subject`,`body`,`item`,`time`,`money`,`cod`,`checked`) VALUES( '%u', '%u', '%u', '%s', '%s', '%u', '" I64FMTD "', '%u', '%u', '%u')", mn2->messageID , mn2->sender , mn2->receiver , mn2->subject.c_str() , mn2->body.c_str(), mn2->item , (uint64)mn2->time ,mn2->money ,mn2->COD ,mn2->checked);

                if (rpl2)
                {
                    rpl2->AddMail(mn2);
                }
            }

            sDatabase.PExecute("DELETE FROM `mail` WHERE `id` = '%u'", m->messageID);
            sDatabase.PExecute("INSERT INTO `mail` (`id`,`sender`,`receiver`,`subject`,`body`,`item`,`time`,`money`,`cod`,`checked`) VALUES ('%u', '%u', '%u', '%s', '%s', '%u', '" I64FMTD "', '%u', '%u', '%u')",m->messageID, pl->GetGUIDLow(), m->receiver, m->subject.c_str(), m->body.c_str(), m->item, (uint64)m->time, m->money, 0, 0);

            uint64 rcpl = m->receiver;
            Player *rpl = objmgr.GetPlayer(rcpl);
            if (rpl)
            {
                Item *it = objmgr.GetAItem(ah->item);
                rpl->AddMItem(it);
            }

            sDatabase.PExecute("DELETE FROM `auctionhouse` WHERE `id` = '%u'",ah->Id);

            data.Initialize( SMSG_AUCTION_LIST_RESULT );
            data << uint32(0);
            data << uint32(0);
            SendPacket(&data);

            Mail *mn = new Mail;
            mn->messageID = objmgr.GenerateMailID();
            mn->sender = 0;                                 //changed to 0, but there should be "Horde or Ali Auction House"
            mn->receiver = ah->owner;
            mn->subject = "Your item sold!";
            mn->body = "";
            mn->item = 0;
            mn->time = time(NULL) + (29 * DAY);
            mn->money = ah->buyout;
            mn->COD = 0;
            mn->checked = 0;

            sDatabase.PExecute("DELETE FROM `mail` WHERE `id` = '%u';", mn->messageID);
            sDatabase.PExecute("INSERT INTO `mail` (`id`,`sender`,`receiver`,`subject`,`body`,`item`,`time`,`money`,`cod`,`checked`) VALUES ('%u', '%u', '%u', '%s', '%s', '%u', '" I64FMTD "', '%u', '%u', '%u');", mn->messageID, mn->sender, mn->receiver, mn->subject.c_str(), mn->body.c_str(), mn->item, (uint64)mn->time, mn->money, 0, 0);

            rcpl = mn->receiver;
            Player *rpln = objmgr.GetPlayer(rcpl);
            if (rpln)
            {
                rpln->AddMail(mn);
            }
            objmgr.RemoveAItem(ah->item);
            objmgr.RemoveAuction(ah->Id);
            //now we can remove auction from ram...
            //delete ah; // should be used in objmgr::removeauction function
        }
    }
}

static uint8 AuctionerFactionToLocation(uint32 faction)
{
    switch (faction)
    {
        case 29:                                            // orc
        case 68:                                            //undead
        case 104:                                           //tauren
            return 1;
            break;
        case 12:                                            //human
        case 55:                                            //dwarf
        case 79:                                            //Nightelf
            return 2;
            break;
        default:                                            /* 85 and so on ... neutral*/
            return 3;
    }
}

void WorldSession::HandleAuctionSellItem( WorldPacket & recv_data )
{
    uint64 auctioneer, item;
    uint32 etime, bid, buyout;
    recv_data >> auctioneer >> item;
    recv_data >> bid >> buyout >> etime;
    Player *pl = GetPlayer();

    Creature *pCreature = ObjectAccessor::Instance().GetCreature(*_player, auctioneer);
    if(!pCreature||!pCreature->isAuctioner())
        return;

    uint8 location = AuctionerFactionToLocation(pCreature->getFaction());

    AuctionEntry *AH = new AuctionEntry;
    AH->Id = objmgr.GenerateAuctionID();
    AH->auctioneer = GUID_LOPART(auctioneer);
    AH->item = GUID_LOPART(item);
    AH->owner = pl->GetGUIDLow();
    AH->bid = bid;
    AH->bidder = 0;
    AH->buyout = buyout;
    time_t base = time(NULL);
    AH->time = ((time_t)(etime * 60)) + base;
    AH->location = location;

    sLog.outDetail("selling item %u to auctioneer %u with inital bid %u with buyout %u and with time %u (in minutes) in location: %u", GUID_LOPART(item), GUID_LOPART(auctioneer), bid, buyout, GUID_LOPART(time), location);
    objmgr.AddAuction(AH);
    uint16 pos = pl->GetPosByGuid(item);
    Item *it = pl->GetItemByPos( pos );

    // DB can have outdate auction item with same guid
    objmgr.RemoveAItem(GUID_LOPART(item));

    objmgr.AddAItem(it);

    pl->RemoveItem( (pos >> 8),(pos & 255), true);
    it->SaveToDB();
    sDatabase.PExecute("INSERT INTO `auctionhouse` (`id`,`auctioneerguid`,`itemguid`,`itemowner`,`buyoutprice`,`time`,`buyguid`,`lastbid`,`location`) VALUES ('%u', '%u', '%u', '%u', '%u', '" I64FMTD "', '%u', '%u', '%u')", AH->Id, AH->auctioneer, AH->item, AH->owner, AH->buyout, AH->time, AH->bidder, AH->bid, AH->location);
    pl->SaveToDB();

    WorldPacket data;
    data.Initialize( SMSG_AUCTION_OWNER_LIST_RESULT );
    data << uint32(0);                                      // initialize, but there should be count..
    uint32 count = 0;
    for (ObjectMgr::AuctionEntryMap::iterator itr = objmgr.GetAuctionsBegin();itr != objmgr.GetAuctionsEnd();itr++)
    {
        if (itr->second->owner == pl->GetGUIDLow() && (count < 51))
        {
            AuctionEntry *Aentry = itr->second;
            data << Aentry->Id;
            Item *it = objmgr.GetAItem(Aentry->item);
            data << it->GetUInt32Value(OBJECT_FIELD_ENTRY);
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
            data << uint32(it->GetCount());
            data << uint32(0);
            data << it->GetOwnerGUID();
            data << Aentry->bid;
            data << uint32(0);
            data << Aentry->buyout;
            time_t base = time(NULL);
            data << uint32((Aentry->time - base) * 1000);
            data << uint64(0);
            data << Aentry->bid;
            count++;
        }
    }
    data.put( 0, count );                                   // add count to placeholder
    data << count;
    SendPacket(&data);

}

void WorldSession::HandleAuctionRemoveItem( WorldPacket & recv_data )
{
    uint32 auctioneer;
    uint32 unk1;                                            // 0xF0001000
    uint32 auctionID;
    recv_data >> auctioneer >> unk1;
    recv_data >> auctionID;
    sLog.outError("DELETE AUCTION !!! auctioneer : %u, unknown: %u, AuctionID: %u", auctioneer, unk1, auctionID);

    WorldPacket data;
    data << unk1 << auctionID;                              //need fix here, this code does nothing
    SendPacket(&data);
}

void WorldSession::HandleAuctionListOwnerItems( WorldPacket & recv_data )
{
    uint32 count;
    uint64 guid;

    recv_data >> guid;

    WorldPacket data;
    data.Initialize( SMSG_AUCTION_OWNER_LIST_RESULT );

    count = 0;
    data << uint32(0);
    for (ObjectMgr::AuctionEntryMap::iterator itr = objmgr.GetAuctionsBegin();itr != objmgr.GetAuctionsEnd();itr++)
    {
        AuctionEntry *Aentry = itr->second;
        if( Aentry )
        {
            if( Aentry->owner == _player->GetGUIDLow() )
            {
                Item *item = objmgr.GetAItem(Aentry->item);
                if( item )
                {
                    ItemPrototype const *proto = item->GetProto();
                    if( proto )
                    {
                        count++;
                        data << Aentry->Id;
                        data << proto->ItemId;
                        data << uint32(0);
                        data << uint32(0);
                        data << uint32(0);
                        data << uint32(item->GetCount());
                        data << uint32(0);
                        data << item->GetOwnerGUID();
                        data << Aentry->bid;
                        data << uint32(0);
                        data << Aentry->buyout;
                                                            // May be need fixing
                        data << uint32((Aentry->time - time(NULL)) * 1000);
                        data << uint32(Aentry->bidder);
                        data << uint32(0);
                        data << Aentry->bid;
                        if( count == 50 )
                            break;
                    }
                }
            }
        }
    }
    data.put<uint32>(0, count);
    data << uint32(count);
    SendPacket(&data);
}

void WorldSession::HandleAuctionListItems( WorldPacket & recv_data )
{
    std::string searchedname, name;
    uint8 levelmin, levelmax, usable, location;
    uint32 count, unk1, auctionSlotID, auctionMainCategory, auctionSubCategory, quality;
    uint64 guid;

    recv_data >> guid;
    recv_data >> unk1;
    recv_data >> searchedname;
    recv_data >> levelmin >> levelmax;
    recv_data >> auctionSlotID >> auctionMainCategory >> auctionSubCategory;
    recv_data >> quality >> usable;

    Creature *pCreature = ObjectAccessor::Instance().GetCreature(*_player, guid);
    if(!pCreature||!pCreature->isAuctioner())
        return;

    location = AuctionerFactionToLocation(pCreature->getFaction());

    sLog.outDebug("Auctionhouse search guid: " I64FMTD ", unk1: %u, searchedname: %s, levelmin: %u, levelmax: %u, auctionSlotID: %u, auctionMainCategory: %u, auctionSubCategory: %u, quality: %u, usable: %u", guid, unk1, searchedname.c_str(), levelmin, levelmax, auctionSlotID, auctionMainCategory, auctionSubCategory, quality, usable);

    WorldPacket data;
    data.Initialize( SMSG_AUCTION_LIST_RESULT );
    count = 0;
    data << uint32(0);
    for (ObjectMgr::AuctionEntryMap::iterator itr = objmgr.GetAuctionsBegin();itr != objmgr.GetAuctionsEnd();itr++)
    {
        AuctionEntry *Aentry = itr->second;
        if( Aentry && Aentry->location == location)
        {
            Item *item = objmgr.GetAItem(Aentry->item);
            if( item )
            {
                ItemPrototype const *proto = item->GetProto();
                if( proto )
                {
                    if( auctionMainCategory == (0xffffffff) || proto->Class == auctionMainCategory )
                    {
                        if( auctionSubCategory == (0xffffffff) || proto->SubClass == auctionSubCategory )
                        {
                            if( auctionSlotID == (0xffffffff) || proto->InventoryType == auctionSlotID )
                            {
                                if( quality == (0xffffffff) || proto->Quality == quality )
                                {
                                    if( usable == (0x00) || _player->CanUseItem( item ) == EQUIP_ERR_OK )
                                    {
                                        if( ( levelmin == (0x00) || proto->RequiredLevel >= levelmin ) && ( levelmax == (0x00) || proto->RequiredLevel <= levelmax ) )
                                        {
                                            name = proto->Name1;
                                            std::transform( name.begin(), name.end(), name.begin(), ::tolower );
                                            std::transform( searchedname.begin(), searchedname.end(), searchedname.begin(), ::tolower );
                                            if( searchedname.empty() || name.find( searchedname ) != std::string::npos )
                                            {
                                                count++;
                                                data << Aentry->Id;
                                                data << proto->ItemId;
                                                data << uint32(0);
                                                data << uint32(0);
                                                data << uint32(0);
                                                data << uint32(item->GetCount());
                                                data << uint32(0);
                                                data << item->GetOwnerGUID();
                                                data << Aentry->bid;
                                                data << uint32(0);
                                                data << Aentry->buyout;
                                                data << uint32((Aentry->time - time(NULL)) * 1000);
                                                data << uint32(Aentry->bidder);
                                                data << uint32(0);
                                                data << Aentry->bid;
                                                if( count == 32 )
                                                    break;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    data.put<uint32>(0, count);
    data << uint32(count);
    SendPacket(&data);
}
