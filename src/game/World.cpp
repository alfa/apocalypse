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
#include "Config/ConfigEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "WorldSocket.h"
#include "WorldSession.h"
#include "WorldPacket.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Group.h"
#include "UpdateData.h"
#include "Chat.h"
#include "Database/DBCStores.h"
#include "ChannelMgr.h"
#include "LootMgr.h"
#include "ProgressBar.hpp"
#include "MapManager.h"
#include "Policies/SingletonImp.h"


initialiseSingleton( World );

World::World()
{
    m_playerLimit = 0;
    m_allowMovement = true;
}


World::~World()
{
    mPrices.clear();
}


WorldSession* World::FindSession(uint32 id) const
{
    SessionMap::const_iterator itr = m_sessions.find(id);

    if(itr != m_sessions.end())
        return itr->second;
    else
        return 0;
}


void World::RemoveSession(uint32 id)
{
    SessionMap::iterator itr = m_sessions.find(id);

    if(itr != m_sessions.end())
    {
        delete itr->second;
        m_sessions.erase(itr);
    }
}


void World::AddSession(WorldSession* s)
{
    ASSERT(s);
    m_sessions[s->GetAccountId()] = s;
}

void World::SetInitialWorldSettings()
{
    
    if (sConfig.GetBoolDefault("LogWorld", false))
    {
        FILE *pFile = fopen("world.log", "w+");
        fclose(pFile);
    }

    srand((unsigned int)time(NULL));

    m_lastTick = time(NULL);

    
    time_t tiempo;
    char hour[3];
    char minute[3];
    char second[3];
    struct tm *tmPtr;
    tiempo = time(NULL);
    tmPtr = localtime(&tiempo);
    strftime( hour, 3, "%H", tmPtr );
    strftime( minute, 3, "%M", tmPtr );
    strftime( second, 3, "%S", tmPtr );
    
    m_gameTime = (3600*atoi(hour))+(atoi(minute)*60)+(atoi(second));

    
    
    
    mPrices[1] = 10;
    mPrices[4] = 80;
    mPrices[6] = 150;
    mPrices[8] = 200;
    mPrices[10] = 300;
    mPrices[12] = 800;
    mPrices[14] = 900;
    mPrices[16] = 1800;
    mPrices[18] = 2200;
    mPrices[20] = 2300;
    mPrices[22] = 3600;
    mPrices[24] = 4200;
    mPrices[26] = 6700;
    mPrices[28] = 7200;
    mPrices[30] = 8000;
    mPrices[32] = 11000;
    mPrices[34] = 14000;
    mPrices[36] = 16000;
    mPrices[38] = 18000;
    mPrices[40] = 20000;
    mPrices[42] = 27000;
    mPrices[44] = 32000;
    mPrices[46] = 37000;
    mPrices[48] = 42000;
    mPrices[50] = 47000;
    mPrices[52] = 52000;
    mPrices[54] = 57000;
    mPrices[56] = 62000;
    mPrices[58] = 67000;
    mPrices[60] = 7200;

    new ChannelMgr;

    
    Log::getSingleton( ).outString( "Loading Quests..." );
    objmgr.LoadQuests();

    
    Log::getSingleton( ).outString( "Loading NPC Texts..." );
    objmgr.LoadGossipText();

    
    Log::getSingleton( ).outString( "Loading Quest Area Triggers..." );
    objmgr.LoadAreaTriggerPoints();

    
    Log::getSingleton( ).outString( "Loading Items..." );
    objmgr.LoadItemPrototypes();
    objmgr.LoadAuctions();
    objmgr.LoadAuctionItems();
    objmgr.LoadMailedItems();
    
    Log::getSingleton( ).outString( "Loading Creatures..." );
    objmgr.LoadCreatureNames();

    
    Log::getSingleton( ).outString( "Loading Guilds..." );
    objmgr.LoadGuilds();

    
   
   
    Log::getSingleton( ).outString( "Loading Trainers..." );
    objmgr.LoadTrainerSpells();
    
    Log::getSingleton( ).outString( "Loading Teleport Coords..." );
    objmgr.LoadTeleportCoords();

    
    objmgr.SetHighestGuids();

	
    LootManager.LoadLootTables();

	Log::getSingleton( ).outString( "Loading Game Object Templates..." );
	objmgr.LoadGameobjectInfo();

    Log::getSingleton().outString("Initialize data stores...");
    barGoLink bar( 15 );
    bar.step();
    new SkillStore("DBC/SkillLineAbility.dbc");
    bar.step();
    new EmoteStore("DBC/EmotesText.dbc");
    bar.step();
    new SpellStore("DBC/Spell.dbc");
    bar.step();
    new RangeStore("DBC/SpellRange.dbc");
    bar.step();
    new CastTimeStore("DBC/SpellCastTimes.dbc");
    bar.step();
    new DurationStore("DBC/SpellDuration.dbc");
    bar.step();
    new RadiusStore("DBC/SpellRadius.dbc");
    bar.step();
    new TalentStore("DBC/Talent.dbc");
    bar.step();
    
    new AreaTableStore("DBC/AreaTable.dbc");
    bar.step();
    new WorldMapAreaStore("DBC/WorldMapArea.dbc");
    bar.step();
    new WorldMapOverlayStore("DBC/WorldMapOverlay.dbc");
	bar.step();
    new FactionStore("DBC/Faction.dbc");
	bar.step();
    new FactionTemplateStore("DBC/FactionTemplate.dbc");
	bar.step();
    
	new ItemDisplayTemplateStore("DBC/ItemDisplayInfo.dbc");
	bar.step();
    
	

	Log::getSingleton( ).outString( "" );
	Log::getSingleton( ).outString( ">> Loaded 15 data stores" );
    Log::getSingleton( ).outString( "" );

    
    m_timers[WUPDATE_OBJECTS].SetInterval(100);
    m_timers[WUPDATE_SESSIONS].SetInterval(100);
    m_timers[WUPDATE_AUCTIONS].SetInterval(1000);

    MapManager::Instance().Initialize();
    Log::getSingleton( ).outString( "WORLD: SetInitialWorldSettings done" );
}

void World::Update(time_t diff)
{
    for(int i = 0; i < WUPDATE_COUNT; i++)
        if(m_timers[i].GetCurrent()>=0)
            m_timers[i].Update(diff);
    else m_timers[i].SetCurrent(0);

    _UpdateGameTime();

    if (m_timers[WUPDATE_AUCTIONS].Passed())
    {
        m_timers[WUPDATE_AUCTIONS].Reset();
        ObjectMgr::AuctionEntryMap::iterator itr,next;
        for (itr = objmgr.GetAuctionsBegin(); itr != objmgr.GetAuctionsEnd();itr = next)
        {
            next = itr;
            next++;
            if (time(NULL) > (itr->second->time))
            {
                if (itr->second->bidder == 0)
                {
                    Mail *m = new Mail;
                    m->reciever = itr->second->owner;
                    m->body = "";
                    m->sender = itr->second->owner;
                    m->checked = 0;
                    m->COD = 0;
                    m->messageID = objmgr.GenerateMailID();
                    m->money = 0;
                    m->time = time(NULL) + (29 * 3600);
                    m->subject = "Your item failed to sell";
                    m->item = itr->second->item;
                    Item *it = objmgr.GetAItem(m->item);
                    objmgr.AddMItem(it);

                    std::stringstream ss;
                    ss << "INSERT INTO mailed_items (guid, data) VALUES ("
                        << it->GetGUIDLow() << ", '";
                    for(uint16 i = 0; i < it->GetValuesCount(); i++ )
                    {
                        ss << it->GetUInt32Value(i) << " ";
                    }
                    ss << "' )";
                    sDatabase.Execute( ss.str().c_str() );

                    std::stringstream md;
                    
                    md << "DELETE FROM mail WHERE mailID = " << m->messageID;
                    sDatabase.Execute( md.str().c_str( ) );

                    std::stringstream mi;
                    mi << "INSERT INTO mail (mailId,sender,reciever,subject,body,item,time,money,COD,checked) VALUES ( " <<
                        m->messageID << ", " << m->sender << ", " << m->reciever << ",' " << m->subject.c_str() << "' ,' " <<
                        m->body.c_str() << "', " << m->item << ", " << m->time << ", " << m->money << ", " << 0 << ", " << m->checked << " )";
                    sDatabase.Execute( mi.str().c_str( ) );

                    uint64 rcpl;
                    GUID_LOPART(rcpl) = m->reciever;
                    GUID_HIPART(rcpl) = 0;
                    std::string pname;
                    objmgr.GetPlayerNameByGUID(rcpl,pname);
                    Player *rpl = objmgr.GetPlayer(pname.c_str());
                    if (rpl)
                    {
                        rpl->AddMail(m);
                    }
                    std::stringstream delinvq;
                    std::stringstream id;
                    std::stringstream bd;
                    
                    delinvq << "DELETE FROM auctionhouse WHERE itemowner = " << m->reciever;
                    sDatabase.Execute( delinvq.str().c_str( ) );

                    
                    id << "DELETE FROM auctioned_items WHERE guid = " << m->item;
                    sDatabase.Execute( id.str().c_str( ) );

                    
                    bd << "DELETE FROM bids WHERE Id = " << itr->second->Id;
                    sDatabase.Execute( bd.str().c_str( ) );

                    objmgr.RemoveAuction(itr->second->Id);
                }
                else
                {
                    Mail *m = new Mail;
                    m->reciever = itr->second->owner;
                    m->body = "";
                    m->sender = itr->second->bidder;
                    m->checked = 0;
                    m->COD = 0;
                    m->messageID = objmgr.GenerateMailID();
                    m->money = itr->second->bid;
                    m->time = time(NULL) + (29 * 3600);
                    m->subject = "Your item sold!";
                    m->item = 0;
                    std::stringstream md;
                    
                    md << "DELETE FROM mail WHERE mailID = " << m->messageID;
                    sDatabase.Execute( md.str().c_str( ) );
                    std::stringstream mi;
                    mi << "INSERT INTO mail (mailId,sender,reciever,subject,body,item,time,money,COD,checked) VALUES ( " <<
                        m->messageID << ", " << m->sender << ", " << m->reciever << ",' " << m->subject.c_str() << "' ,' " <<
                        m->body.c_str() << "', " << m->item << ", " << m->time << ", " << m->money << ", " << 0 << ", " << m->checked << " )";
                    sDatabase.Execute( mi.str().c_str( ) );
                    uint64 rcpl;
                    GUID_LOPART(rcpl) = m->reciever;
                    GUID_HIPART(rcpl) = 0;
                    std::string pname;
                    objmgr.GetPlayerNameByGUID(rcpl,pname);
                    Player *rpl = objmgr.GetPlayer(pname.c_str());
                    if (rpl)
                    {
                        rpl->AddMail(m);
                    }

                    Mail *mn = new Mail;
                    mn->reciever = itr->second->bidder;
                    mn->body = "";
                    mn->sender = itr->second->owner;
                    mn->checked = 0;
                    mn->COD = 0;
                    mn->messageID = objmgr.GenerateMailID();
                    mn->money = 0;
                    mn->time = time(NULL) + (29 * 3600);
                    mn->subject = "Your won an item!";
                    mn->item = itr->second->item;
                    Item *it = objmgr.GetAItem(itr->second->item);
                    objmgr.AddMItem(it);

                    std::stringstream ss;
                    ss << "INSERT INTO mailed_items (guid, data) VALUES ("
                        << it->GetGUIDLow() << ", '";
                    for(uint16 i = 0; i < it->GetValuesCount(); i++ )
                    {
                        ss << it->GetUInt32Value(i) << " ";
                    }
                    ss << "' )";
                    sDatabase.Execute( ss.str().c_str() );

                    std::stringstream mdn;
                    
                    mdn << "DELETE FROM mail WHERE mailID = " << mn->messageID;
                    sDatabase.Execute( mdn.str().c_str( ) );
                    std::stringstream min;
                    min << "INSERT INTO mail (mailId,sender,reciever,subject,body,item,time,money,COD,checked) VALUES ( " <<
                        mn->messageID << ", " << mn->sender << ", " << mn->reciever << ",' " << mn->subject.c_str() << "' ,' " <<
                        mn->body.c_str() << "', " << mn->item << ", " << mn->time << ", " << mn->money << ", " << 0 << ", " << mn->checked << " )";
                    sDatabase.Execute( min.str().c_str( ) );
                    uint64 rcpl1;
                    GUID_LOPART(rcpl1) = mn->reciever;
                    GUID_HIPART(rcpl1) = 0;
                    std::string pname1;
                    objmgr.GetPlayerNameByGUID(rcpl1,pname1);
                    Player *rpl1 = objmgr.GetPlayer(pname1.c_str());
                    if (rpl1)
                    {
                        rpl1->AddMail(mn);
                    }
                    objmgr.RemoveAItem(itr->second->item);
                    objmgr.RemoveAuction(itr->second->Id);
                }
            }
        }
    }
    if (m_timers[WUPDATE_SESSIONS].Passed())
    {
        m_timers[WUPDATE_SESSIONS].Reset();

        SessionMap::iterator itr, next;
        for (itr = m_sessions.begin(); itr != m_sessions.end(); itr = next)
        {
            next = itr;
            next++;

            if(!itr->second->Update(diff))
            {
                delete itr->second;
                m_sessions.erase(itr);
            }
        }
    }

    if (m_timers[WUPDATE_OBJECTS].Passed())
    {
        m_timers[WUPDATE_OBJECTS].Reset();
	MapManager::Instance().Update(diff);
    }
}


void World::SendGlobalMessage(WorldPacket *packet, WorldSession *self)
{
    SessionMap::iterator itr;
    for (itr = m_sessions.begin(); itr != m_sessions.end(); itr++)
    {
        if (itr->second->GetPlayer() &&
            itr->second->GetPlayer()->IsInWorld()
            && itr->second != self)               
        {
            itr->second->SendPacket(packet);
        }
    }
}


void World::SendWorldText(const char* text, WorldSession *self)
{
    WorldPacket data;
    sChatHandler.FillSystemMessageData(&data, 0, text);
    SendGlobalMessage(&data, self);
}

