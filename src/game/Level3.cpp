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

#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Opcodes.h"
#include "GameObject.h"
#include "Chat.h"
#include "Log.h"
#include "Guild.h"
#include "ObjectAccessor.h"
#include "MapManager.h"
#include "SpellAuras.h"
#include "ScriptCalls.h"
#include "Language.h"
#include "RedZoneDistrict.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Weather.h"

bool ChatHandler::HandleReloadCommand(const char* args)
{
    char* updatefield = strtok((char*)args, " ");

    char* value = strtok(NULL, " ");

    if (!updatefield || !value)
        return false;

    uint32 tupdatefield = (uint32)atoi(updatefield);
    uint32 tvalue = (uint32)atoi(value);

    Player *chr = m_session->GetPlayer();
    if (chr == NULL)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }
    chr->SetUInt32Value(tupdatefield, tvalue);
    return true;
}

bool ChatHandler::HandleLoadScriptsCommand(const char* args)
{
    if(!LoadScriptingModule(args)) return true;

    sWorld.SendWorldText(LANG_SCRIPTS_RELOADED, NULL);

    return true;
}

/** \brief GM command level 3 - Create a guild.
 *
 * This command allows a GM (level 3) to create a guild.
 *
 * The "args" parameter contains the name of the guild leader
 * and then the name of the guild.
 *
 */
bool ChatHandler::HandleSecurityCommand(const char* args)
{
    char* arg1 = strtok((char*)args, " ");
    if( !arg1 )
        return false;

    char* arg2 = 0;

    std::string targetName;
    uint32 targetAccountId = 0;
    uint32 targetSecurity = 0;


    Player* targetPlayer = getSelectedPlayer();
    if(targetPlayer)
    {
        targetName = targetPlayer->GetName();
        targetAccountId = targetPlayer->GetSession()->GetAccountId();
        targetSecurity = targetPlayer->GetSession()->GetSecurity();
        arg2 = arg1;
    }
    else
    {
        targetName = arg1;
        normalizePlayerName(targetName);
        targetPlayer = ObjectAccessor::Instance().FindPlayerByName(targetName.c_str());
        if(targetPlayer)
        {
            targetAccountId = targetPlayer->GetSession()->GetAccountId();
            targetSecurity = targetPlayer->GetSession()->GetSecurity();
        }
        else
        {
            uint64 targetGUID = objmgr.GetPlayerGUIDByName(targetName.c_str());
            if(!targetGUID)
            {
                SendSysMessage(LANG_PLAYER_NOT_FOUND);
                return true;
            }
            targetAccountId = objmgr.GetPlayerAccountIdByGUID(targetGUID);
            targetSecurity = objmgr.GetSecurityByAccount(targetAccountId);
        }

        arg2 = strtok(NULL, " ");
    }

    int32 gm = (int32)atoi(arg2);
    if ( gm < SEC_PLAYER || gm > SEC_ADMINISTRATOR )
    {
        SendSysMessage(LANG_BAD_VALUE);
        return true;
    }

    // can set security level only for target with less security and to less security that we have
    if(targetSecurity >= m_session->GetSecurity() || gm >= m_session->GetSecurity() )
    {
        SendSysMessage(LANG_YOURS_SECURITY_IS_LOW);
        return true;
    }

    if(targetPlayer)
    {
        if( targetPlayer != m_session->GetPlayer() )
        {
            WorldPacket data;
            char buf[256];
            sprintf((char*)buf,LANG_YOURS_SECURITY_CHANGED, m_session->GetPlayer()->GetName(), gm);
            FillSystemMessageData(&data, m_session, buf);
            targetPlayer->GetSession()->SendPacket(&data);
        }

        targetPlayer->GetSession()->SetSecurity(gm);
    }

    PSendSysMessage(LANG_YOU_CHANGE_SECURITY, targetName.c_str(), gm);
    loginDatabase.PExecute("UPDATE `account` SET `gmlevel` = '%i' WHERE `id` = '%u'", gm, targetAccountId);

    return true;
}

bool ChatHandler::HandleGoXYCommand(const char* args)
{
    Player* _player = m_session->GetPlayer();

    if(_player->isInFlight())
    {
        SendSysMessage(LANG_YOU_IN_FLIGHT);
        return true;
    }

    char* px = strtok((char*)args, " ");
    char* py = strtok(NULL, " ");
    char* pmapid = strtok(NULL, " ");

    if (!px || !py)
        return false;

    float x = (float)atof(px);
    float y = (float)atof(py);
    uint32 mapid;
    if (pmapid)
        mapid = (uint32)atoi(pmapid);
    else mapid = _player->GetMapId();

    if(!MapManager::IsValidMapCoord(mapid,x,y))
    {
        PSendSysMessage(LANG_INVALID_TARGET_COORD,x,y,mapid);
        return true;
    }

    Map *map = MapManager::Instance().GetMap(mapid, _player);
    float z = max(map->GetHeight(x, y, 0), map->GetWaterLevel(x, y));
    _player->SetRecallPosition(_player->GetMapId(),_player->GetPositionX(),_player->GetPositionY(),_player->GetPositionZ(),_player->GetOrientation());
    _player->TeleportTo(mapid, x, y, z, _player->GetOrientation());

    return true;
}

bool ChatHandler::HandleWorldPortCommand(const char* args)
{
    Player* _player = m_session->GetPlayer();

    if(_player->isInFlight())
    {
        SendSysMessage(LANG_YOU_IN_FLIGHT);
        return true;
    }

    char* pContinent = strtok((char*)args, " ");
    if (!pContinent)
        return false;

    char* px = strtok(NULL, " ");
    char* py = strtok(NULL, " ");
    char* pz = strtok(NULL, " ");

    if (!px || !py || !pz)
        return false;

    float x = atof(px);
    float y = atof(py);
    float z = atof(pz);
    uint32 mapid = atoi(pContinent);

    if(!MapManager::IsValidMapCoord(mapid,x,y))
    {
        PSendSysMessage(LANG_INVALID_TARGET_COORD,x,y,mapid);
        return true;
    }

    _player->SetRecallPosition(_player->GetMapId(),_player->GetPositionX(),_player->GetPositionY(),_player->GetPositionZ(),_player->GetOrientation());

    _player->TeleportTo(mapid, x, y, z,_player->GetOrientation());

    return true;
}

bool ChatHandler::HandleAllowMovementCommand(const char* args)
{
    if(sWorld.getAllowMovement())
    {
        sWorld.SetAllowMovement(false);
        SendSysMessage(LANG_CREATURE_MOVE_DISABLED);
    }
    else
    {
        sWorld.SetAllowMovement(true);
        SendSysMessage(LANG_CREATURE_MOVE_ENABLED);
    }
    return true;
}

bool ChatHandler::HandleGoCommand(const char* args)
{
    Player* _player = m_session->GetPlayer();

    if(_player->isInFlight())
    {
        SendSysMessage(LANG_YOU_IN_FLIGHT);
        return true;
    }

    char* px = strtok((char*)args, " ");
    char* py = strtok(NULL, " ");
    char* pz = strtok(NULL, " ");
    char* pmapid = strtok(NULL, " ");

    if (!px || !py || !pz || !pmapid)
        return false;

    float x = (float)atof(px);
    float y = (float)atof(py);
    float z = (float)atof(pz);
    uint32 mapid = (uint32)atoi(pmapid);

    if(!MapManager::IsValidMapCoord(mapid,x,y))
    {
        PSendSysMessage(LANG_INVALID_TARGET_COORD,x,y,mapid);
        return true;
    }

    _player->SetRecallPosition(_player->GetMapId(),_player->GetPositionX(),_player->GetPositionY(),_player->GetPositionZ(),_player->GetOrientation());

    _player->TeleportTo(mapid, x, y, z,_player->GetOrientation());

    return true;
}

bool ChatHandler::HandleMaxSkillCommand(const char* args)
{
    Player* SelectedPlayer = getSelectedPlayer();
    if(!SelectedPlayer)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }

    // each skills that have max skill value dependent from level seted to current level max skill value
    SelectedPlayer->UpdateSkillsToMaxSkillsForLevel();
    return true;
}

bool ChatHandler::HandleSetSkillCommand(const char* args)
{
    // number or [name] Shift-click form |color|Hskill:skill_id|h[name]|h|r
    char* skill_p = extractKeyFromLink((char*)args,"Hskill");
    if(!skill_p)
        return false;

    char *level_p = strtok (NULL, " ");

    if( !level_p)
        return false;

    char *max_p   = strtok (NULL, " ");

    int32 skill = atoi(skill_p);

    if (skill <= 0)
    {
        PSendSysMessage(LANG_INVALID_SKILL_ID, skill);
        return true;
    }

    int32 level = atol (level_p);

    Player * target = getSelectedPlayer();
    if(!target)
    {
        PSendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }

    SkillLineEntry const* sl = sSkillLineStore.LookupEntry(skill);
    if(!sl)
    {
        PSendSysMessage(LANG_INVALID_SKILL_ID, skill);
        return true;
    }

    if(!target->GetSkillValue(skill))
    {
        PSendSysMessage(LANG_SET_SKILL_ERROR, target->GetName(), skill, sl->name[0]);
        return true;
    }

    int32 max   = max_p ? atol (max_p) : target->GetMaxSkillValue(skill);

    if( level <= 0 || level > max || max <= 0 )
        return false;

    target->SetSkill(skill, level, max);
    PSendSysMessage(LANG_SET_SKILL, skill, sl->name[0], target->GetName(), level, max);

    return true;
}

bool ChatHandler::HandleUnLearnCommand(const char* args)
{
    if (!*args)
        return false;

    uint32 minS;
    uint32 maxS;

    // number or [name] Shift-click form |color|Hspell:spell_id|h[name]|h|r
    char* startS = extractKeyFromLink((char*)args,"Hspell");
    // number or [name] Shift-click form |color|Hspell:spell_id|h[name]|h|r
    char* tail = strtok(NULL,"");
    char* endS = tail ? extractKeyFromLink(tail,"Hspell") : NULL;

    if (!endS)
    {
        // number or [name] Shift-click form |color|Hspell:spell_id|h[name]|h|r
        minS = (uint32)atol(startS);
        maxS =  minS+1;
    }
    else
    {
        minS = (uint32)atol(startS);
        maxS = (uint32)atol(endS);
        if (maxS >= minS)
        {
            maxS=maxS+1;
        }
        else
        {
            std::swap(minS,maxS);
            maxS=minS+1;
        }
    }

    Player* target = getSelectedPlayer();
    if(!target)
    {
        PSendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }

    for(uint32 spell=minS;spell<maxS;spell++)
    {
        if (target->HasSpell(spell))
            target->removeSpell(spell);
        else
            SendSysMessage(LANG_FORGET_SPELL);
    }

    return true;
}

bool ChatHandler::HandleCooldownCommand(const char* args)
{
    Player* target = getSelectedPlayer();
    if(!target)
    {
        PSendSysMessage(LANG_PLAYER_NOT_FOUND);
        return true;
    }

    if (!*args)
    {
        target->RemoveAllSpellCooldown();
        PSendSysMessage(LANG_REMOVEALL_COOLDOWN, target->GetName());
    }
    else
    {
        // number or [name] Shift-click form |color|Hspell:spell_id|h[name]|h|r
        char* cId = extractKeyFromLink((char*)args,"Hspell");
        if(!cId)
            return false;

        uint32 spell_id = atol(cId);

        if(!sSpellStore.LookupEntry(spell_id))
        {
            PSendSysMessage(LANG_UNKNOWN_SPELL, target==m_session->GetPlayer() ? LANG_YOU : target->GetName());
            return true;
        }

        WorldPacket data( SMSG_CLEAR_COOLDOWN, (4+8+4) );
        data << uint32( spell_id );
        data << target->GetGUID();
        data << uint32(0);
        target->GetSession()->SendPacket(&data);
        target->RemoveSpellCooldown(spell_id);
        PSendSysMessage(LANG_REMOVE_COOLDOWN, spell_id, target==m_session->GetPlayer() ? LANG_YOU : target->GetName());
    }
    return true;
}

bool ChatHandler::HandleLearnAllCommand(const char* args)
{
    static const char *allSpellList[] =
    {
        "3365",
        "6233",
        "6247",
        "6246",
        "6477",
        "6478",
        "22810",
        "8386",
        "21651",
        "21652",
        "522",
        "7266",
        "8597",
        "2479",
        "22027",
        "6603",
        "5019",
        "133",
        "168",
        "227",
        "5009",
        "9078",
        "668",
        "203",
        "20599",
        "20600",
        "81",
        "20597",
        "20598",
        "20864",
        "1459",
        "5504",
        "587",
        "5143",
        "118",
        "5505",
        "597",
        "604",
        "1449",
        "1460",
        "2855",
        "1008",
        "475",
        "5506",
        "1463",
        "12824",
        "8437",
        "990",
        "5145",
        "8450",
        "1461",
        "759",
        "8494",
        "8455",
        "8438",
        "6127",
        "8416",
        "6129",
        "8451",
        "8495",
        "8439",
        "3552",
        "8417",
        "10138",
        "12825",
        "10169",
        "10156",
        "10144",
        "10191",
        "10201",
        "10211",
        "10053",
        "10173",
        "10139",
        "10145",
        "10192",
        "10170",
        "10202",
        "10054",
        "10174",
        "10193",
        "12826",
        "2136",
        "143",
        "145",
        "2137",
        "2120",
        "3140",
        "543",
        "2138",
        "2948",
        "8400",
        "2121",
        "8444",
        "8412",
        "8457",
        "8401",
        "8422",
        "8445",
        "8402",
        "8413",
        "8458",
        "8423",
        "8446",
        "10148",
        "10197",
        "10205",
        "10149",
        "10215",
        "10223",
        "10206",
        "10199",
        "10150",
        "10216",
        "10207",
        "10225",
        "10151",
        "116",
        "205",
        "7300",
        "122",
        "837",
        "10",
        "7301",
        "7322",
        "6143",
        "120",
        "865",
        "8406",
        "6141",
        "7302",
        "8461",
        "8407",
        "8492",
        "8427",
        "8408",
        "6131",
        "7320",
        "10159",
        "8462",
        "10185",
        "10179",
        "10160",
        "10180",
        "10219",
        "10186",
        "10177",
        "10230",
        "10181",
        "10161",
        "10187",
        "10220",
        "2018",
        "2663",
        "12260",
        "2660",
        "3115",
        "3326",
        "2665",
        "3116",
        "2738",
        "3293",
        "2661",
        "3319",
        "2662",
        "9983",
        "8880",
        "2737",
        "2739",
        "7408",
        "3320",
        "2666",
        "3323",
        "3324",
        "3294",
        "22723",
        "23219",
        "23220",
        "23221",
        "23228",
        "23338",
        "10788",
        "10790",
        "5611",
        "5016",
        "5609",
        "2060",
        "10963",
        "10964",
        "10965",
        "22593",
        "22594",
        "596",
        "996",
        "499",
        "768",
        "17002",
        "1448",
        "1082",
        "16979",
        "1079",
        "5215",
        "20484",
        "5221",
        "15590",
        "17007",
        "6795",
        "6807",
        "5487",
        "1446",
        "1066",
        "5421",
        "3139",
        "779",
        "6811",
        "6808",
        "1445",
        "5216",
        "1737",
        "5222",
        "5217",
        "1432",
        "6812",
        "9492",
        "5210",
        "3030",
        "1441",
        "783",
        "6801",
        "20739",
        "8944",
        "9491",
        "22569",
        "5226",
        "6786",
        "1433",
        "8973",
        "1828",
        "9495",
        "9006",
        "6794",
        "8993",
        "5203",
        "16914",
        "6784",
        "9635",
        "22830",
        "20722",
        "9748",
        "6790",
        "9753",
        "9493",
        "9752",
        "9831",
        "9825",
        "9822",
        "5204",
        "5401",
        "22831",
        "6793",
        "9845",
        "17401",
        "9882",
        "9868",
        "20749",
        "9893",
        "9899",
        "9895",
        "9832",
        "9902",
        "9909",
        "22832",
        "9828",
        "9851",
        "9883",
        "9869",
        "17406",
        "17402",
        "9914",
        "20750",
        "9897",
        "9848",
        "3127",
        "107",
        "204",
        "9116",
        "2457",
        "78",
        "18848",
        "331",
        "403",
        "2098",
        "1752",
        "11278",
        "11288",
        "11284",
        "6461",
        "2344",
        "2345",
        "6463",
        "2346",
        "2352",
        "775",
        "1434",
        "1612",
        "71",
        "2468",
        "2458",
        "2467",
        "7164",
        "7178",
        "7367",
        "7376",
        "7381",
        "21156",
        "5209",
        "3029",
        "5201",
        "9849",
        "9850",
        "20719",
        "22568",
        "22827",
        "22828",
        "22829",
        "6809",
        "8972",
        "9005",
        "9823",
        "9827",
        "6783",
        "9913",
        "6785",
        "6787",
        "9866",
        "9867",
        "9894",
        "9896",
        "6800",
        "8992",
        "9829",
        "9830",
        "780",
        "769",
        "6749",
        "6750",
        "9755",
        "9754",
        "9908",
        "20745",
        "20742",
        "20747",
        "20748",
        "9746",
        "9745",
        "9880",
        "9881",
        "5391",
        "842",
        "3025",
        "3031",
        "3287",
        "3329",
        "1945",
        "3559",
        "4933",
        "4934",
        "4935",
        "4936",
        "5142",
        "5390",
        "5392",
        "5404",
        "5420",
        "6405",
        "7293",
        "7965",
        "8041",
        "8153",
        "9033",
        "9034",
        //"9036", problems with ghost state
        "16421",
        "21653",
        "22660",
        "5225",
        "9846",
        "2426",
        "5916",
        "6634",
        //"6718", phasing stealth, annoing for learn all case.
        "6719",
        "8822",
        "9591",
        "9590",
        "10032",
        "17746",
        "17747",
        "8203",
        "11392",
        "12495",
        "16380",
        "23452",
        "4079",
        "4996",
        "4997",
        "4998",
        "4999",
        "5000",
        "6348",
        "6349",
        "6481",
        "6482",
        "6483",
        "6484",
        "11362",
        "11410",
        "11409",
        "12510",
        "12509",
        "12885",
        "13142",
        "21463",
        "23460",
        "11421",
        "11416",
        "11418",
        "1851",
        "10059",
        "11423",
        "11417",
        "11422",
        "11419",
        "11424",
        "11420",
        "27",
        "31",
        "33",
        "34",
        "35",
        "15125",
        "21127",
        "22950",
        "1180",
        "201",
        "12593",
        "12842",
        "16770",
        "6057",
        "12051",
        "18468",
        "12606",
        "12605",
        "18466",
        "12502",
        "12043",
        "15060",
        "12042",
        "12341",
        "12848",
        "12344",
        "12353",
        "18460",
        "11366",
        "12350",
        "12352",
        "13043",
        "11368",
        "11113",
        "12400",
        "11129",
        "16766",
        "12573",
        "15053",
        "12580",
        "12475",
        "12472",
        "12953",
        "12488",
        "11189",
        "12985",
        "12519",
        "16758",
        "11958",
        "12490",
        "11426",
        "3565",
        "3562",
        "18960",
        "3567",
        "3561",
        "3566",
        "3563",
        "1953",
        "2139",
        "12505",
        "13018",
        "12522",
        "12523",
        "5146",
        "5144",
        "5148",
        "8419",
        "8418",
        "10213",
        "10212",
        "10157",
        "12524",
        "13019",
        "12525",
        "13020",
        "12526",
        "13021",
        "18809",
        "13031",
        "13032",
        "13033",
        "4036",
        "3920",
        "3919",
        "3918",
        "7430",
        "3922",
        "3923",
        "3921",
        "7411",
        "7418",
        "7421",
        "13262",
        "7412",
        "7415",
        "7413",
        "7416",
        "13920",
        "13921",
        "7745",
        "7779",
        "7428",
        "7457",
        "7857",
        "7748",
        "7426",
        "13421",
        "7454",
        "13378",
        "7788",
        "14807",
        "14293",
        "7795",
        "6296",
        "20608",
        "755",
        "444",
        "427",
        "428",
        "442",
        "447",
        "3578",
        "3581",
        "19027",
        "3580",
        "665",
        "3579",
        "3577",
        "6755",
        "3576",
        "2575",
        "2577",
        "2578",
        "2579",
        "2580",
        "2656",
        "2657",
        "2576",
        "3564",
        "10248",
        "8388",
        "2659",
        "14891",
        "3308",
        "3307",
        "10097",
        "2658",
        "3569",
        "16153",
        "3304",
        "10098",
        "4037",
        "3929",
        "3931",
        "3926",
        "3924",
        "3930",
        "3977",
        "3925",
        "136",
        "228",
        "5487",
        "43",
        "202",
        "0"
    };

    int loop = 0;

    while (strcmp(allSpellList[loop], "0"))
    {
        uint32 spell = atol((char*)allSpellList[loop]);

        if (m_session->GetPlayer()->HasSpell(spell))
        {
            loop++;
            continue;
        }
        m_session->GetPlayer()->learnSpell((uint16)spell);

        loop++;
    }

    PSendSysMessage(LANG_COMMAND_LEARN_ALL_LANG, m_session->GetPlayer()->GetName());

    return true;
}

bool ChatHandler::HandleLearnAllGMCommand(const char* args)
{
    static const char *gmSpellList[] =
    {
        "24347", // Become A Fish, No Breath Bar
        "35132", // Visual Boom
        "38488", // Attack 4000-8000 AOE
        "38795", // Attack 2000 AOE + Slow Down 90%
        "15712", // Attack 200
        "1852",  // GM Spell Silence
        "31899", // Kill
        "31924", // Kill
        "29878", // Kill My Self
        "26644", // More Kill

        "28550", //Invisible 24
        "23452", //Invisible + Target
        "0"
    };

    uint16 gmSpellIter = 0;
    while( strcmp(gmSpellList[gmSpellIter], "0") )
    {
        uint32 spell = atol((char*)gmSpellList[gmSpellIter++]);

        m_session->GetPlayer()->learnSpell((uint16)spell);
    }

    PSendSysMessage(LANG_LEARNING_GM_SKILLS, m_session->GetPlayer()->GetName());
    return true;
}

bool ChatHandler::HandleLearnAllMyClassCommand(const char* args)
{
    uint32 family = 0;
    switch(m_session->GetPlayer()->getClass())
    {
    case CLASS_WARRIOR: family = SPELLFAMILY_WARRIOR; break;
    case CLASS_PALADIN: family = SPELLFAMILY_PALADIN; break;
    case CLASS_HUNTER: family = SPELLFAMILY_HUNTER; break;
    case CLASS_ROGUE: family = SPELLFAMILY_ROGUE; break;
    case CLASS_PRIEST: family = SPELLFAMILY_PRIEST; break;
    case CLASS_SHAMAN: family = SPELLFAMILY_SHAMAN; break;
    case CLASS_MAGE: family = SPELLFAMILY_MAGE; break;
    case CLASS_WARLOCK: family = SPELLFAMILY_WARLOCK; break;
    case CLASS_DRUID: family = SPELLFAMILY_DRUID; break;
    default: 
        return true;
    }

    for (uint32 i = 0; i < sSpellStore.GetNumRows(); i++)
    {
        SpellEntry const *spellInfo = sSpellStore.LookupEntry(i);
        SkillLineAbilityEntry const *skillLine = sSkillLineAbilityStore.LookupEntry(i);
        if (skillLine && spellInfo && spellInfo->SpellFamilyName == family )
            m_session->GetPlayer()->learnSpell((uint16)i);
    }

    PSendSysMessage(LANG_COMMAND_LEARN_CLASS_SPELLS, m_session->GetPlayer()->GetName());
    return true;
}

bool ChatHandler::HandleLearnAllLangCommand(const char* args)
{
    // skipping UNIVERSAL language (0)
    for(int i = 1; i < LANGUAGES_COUNT; ++i)
        m_session->GetPlayer()->learnSpell(lang_description[i].spell_id);

    PSendSysMessage(LANG_COMMAND_LEARN_ALL_LANG, m_session->GetPlayer()->GetName());
    return true;
}

bool ChatHandler::HandleLearnAllCraftCommand(const char* args)
{
    for (uint32 i = 0; i < sSkillLineStore.GetNumRows(); ++i)
    {
        SkillLineEntry const *skillInfo = sSkillLineStore.LookupEntry(i);
        if( !skillInfo )
            continue;

        if (skillInfo->categoryId == SKILL_CATEGORY_PROFESSION || skillInfo->categoryId == SKILL_CATEGORY_SECONDARY)
            for (uint32 j = 0; j < sSkillLineAbilityStore.GetNumRows(); ++j)
            {
                SkillLineAbilityEntry const *skillLine = sSkillLineAbilityStore.LookupEntry(j);
                if( !skillLine )
                    continue;
                if( skillLine->skillId == i && !skillLine->forward_spellid )
                    m_session->GetPlayer()->learnSpell((uint16)skillLine->spellId);
            }
    }

    PSendSysMessage(LANG_COMMAND_LEARN_ALL_CRAFT, m_session->GetPlayer()->GetName());
    return true;
}

bool ChatHandler::HandleLearnCommand(const char* args)
{
    Player* targetPlayer = getSelectedPlayer();

    if(!targetPlayer)
    {
        PSendSysMessage(LANG_PLAYER_NOT_FOUND);
        return true;
    }

    // number or [name] Shift-click form |color|Hspell:spell_id|h[name]|h|r
    char* cId = extractKeyFromLink((char*)args,"Hspell");
    if(!cId)
        return false;

    uint32 spell = (uint32)atol((char*)cId);

    if (targetPlayer->HasSpell(spell))
    {
        if(targetPlayer == m_session->GetPlayer())
            SendSysMessage(LANG_YOU_KNOWN_SPELL);
        else
            PSendSysMessage(LANG_TARGET_KNOWN_SPELL,targetPlayer->GetName());
        return true;
    }
    targetPlayer->learnSpell((uint16)spell);

    return true;
}

bool ChatHandler::HandleAddItemCommand(const char* args)
{
    if (!*args)
        return false;

    uint32 itemId = 0;

    if(args[0]=='[')                                        // [name] manual form
    {
        char* citemName = citemName = strtok((char*)args, "]");

        if(citemName && citemName[0])
        {
            std::string itemName = citemName+1;
            sDatabase.escape_string(itemName);
            QueryResult *result = sDatabase.PQuery("SELECT entry FROM item_template WHERE name = '%s'", itemName.c_str());
            if (!result)
            {
                PSendSysMessage(LANG_COMMAND_ADDITEMCOULDNOTFIND, citemName+1);
                return true;
            }
            itemId = result->Fetch()->GetUInt16();
            delete result;
        }
        else
            return false;
    }
    else if(args[0]=='|')                                   // [name] Shift-click form |color|Hitem:item_id:0:0:0|h[name]|h|r
    {
        strtok((char*)args, ":");
        char* citemId = strtok(NULL, ":");
        itemId = atol(citemId);
        strtok(NULL, "]");
        strtok(NULL, " ");
    }
    else                                                    // item_id form
    {
        char* citemId = strtok((char*)args, " ");
        itemId = atol(citemId);
    }

    char* ccount = strtok(NULL, " ");

    int32 count = 1;

    if (ccount) { count = atol(ccount); }
    if (count < 1) { count = 1; }

    Player* pl = m_session->GetPlayer();
    Player* plTarget = getSelectedPlayer();
    if(!plTarget)
        plTarget = pl;

    sLog.outDetail(LANG_ADDITEM, itemId, count);

    ItemPrototype const *pProto = objmgr.GetItemPrototype(itemId);
    if(!pProto)
    {
        PSendSysMessage(LANG_COMMAND_ITEMIDINVALID, itemId);
        return true;
    }

    uint32 countForStore = count;

    // item used in local operations and in add item notifier
    Item* item = NULL;

    // if possible create full stacks for better performance
    while(countForStore >= pProto->Stackable)
    {
        uint16 dest;
        uint8 msg = plTarget->CanStoreNewItem( NULL_BAG, NULL_SLOT, dest, itemId, pProto->Stackable, false );
        if( msg == EQUIP_ERR_OK )
        {
            item = plTarget->StoreNewItem( dest, itemId, pProto->Stackable, true, Item::GenerateItemRandomPropertyId(itemId));

            countForStore-= pProto->Stackable;

            // remove binding (let GM give it to another player later)
            if(pl==plTarget)
            {
                // remove binding from original stack
                Item* item1 = pl->GetItemByPos(dest);
                if(item1!=item)
                    item1->SetBinding( false );
                // and new stack
                item->SetBinding( false );
            }
        }
        else
            break;
    }

    // create remaining items
    if(countForStore > 0 && countForStore < pProto->Stackable)
    {
        uint16 dest;
        uint8 msg = plTarget->CanStoreNewItem( NULL_BAG, NULL_SLOT, dest, itemId, countForStore, false );

        // if can add all countForStore items
        if( msg == EQUIP_ERR_OK )
        {
            item = plTarget->StoreNewItem( dest, itemId, countForStore, true, Item::GenerateItemRandomPropertyId(itemId));
            countForStore = 0;

            // remove binding (let GM give it to another player later)
            if(pl==plTarget)
            {
                // remove binding from original stack
                Item* item1 = pl->GetItemByPos(dest);
                if(item1!=item)
                    item1->SetBinding( false );
                // and new stack
                item->SetBinding( false );
            }
        }
    }

    // ok search place for add only part from countForStore items in not full stacks
    while(countForStore > 0)
    {
        // find not full stack (last possable place for times after prev. checks)
        uint16 dest;
        uint8 msg = plTarget->CanStoreNewItem( NULL_BAG, NULL_SLOT, dest, itemId, 1, false );
        if( msg == EQUIP_ERR_OK )                           // found
        {
            // we can fill this stack to max stack size
            Item* itemStack = pl->GetItemByPos(dest);
            if(itemStack)
            {
                uint32 countForStack = pProto->Stackable - itemStack->GetCount();
                // recheck with real item amount
                uint8 msg = plTarget->CanStoreNewItem( itemStack->GetBagSlot(), itemStack->GetSlot(), dest, itemId, countForStack, false );
                if( msg == EQUIP_ERR_OK )
                {
                    item = plTarget->StoreNewItem( dest, itemId, countForStack, true, Item::GenerateItemRandomPropertyId(itemId));
                    countForStore-= countForStack;

                    // remove binding (let GM give it to another player later)
                    if(pl==plTarget)
                        item->SetBinding( false );
                }
                else
                    break;                                  // not possible with correct work
            }
            else
                break;                                      // not possible with correct work
        }
        else
            break;
    }

    if(count > countForStore && item)
    {
        pl->SendNewItem(item,count - countForStore,false,true);
        if(pl!=plTarget)
            plTarget->SendNewItem(item,count - countForStore,true,false);
    }

    if(countForStore > 0)
        PSendSysMessage(LANG_ITEM_CANNOT_CREATE, itemId, countForStore);

    return true;
}

bool ChatHandler::HandleAddItemSetCommand(const char* args)
{
    if (!*args)
        return false;

    char* cId = extractKeyFromLink((char*)args,"Hitemset");  // number or [name] Shift-click form |color|Hitemset:itemset_id|h[name]|h|r
    if (!cId)
        return false;

    uint32 itemsetId = atol(cId);

    // prevent generation all items with itemset field value '0'
    if (itemsetId == 0)
    {
        PSendSysMessage(LANG_NO_ITEMS_FROM_ITEMSET_FOUND,itemsetId);
        return true;
    }

    Player* pl = m_session->GetPlayer();
    Player* plTarget = getSelectedPlayer();
    if(!plTarget)
        plTarget = pl;

    sLog.outDetail(LANG_ADDITEMSET, itemsetId);

    QueryResult *result = sDatabase.PQuery("SELECT `entry` FROM `item_template` WHERE `itemset` = %u",itemsetId);

    if(!result)
    {
        PSendSysMessage(LANG_NO_ITEMS_FROM_ITEMSET_FOUND,itemsetId);

        return true;
    }

    do
    {
        Field *fields = result->Fetch();
        uint32 itemId = fields[0].GetUInt32();

        uint16 dest;

        uint8 msg = plTarget->CanStoreNewItem( NULL_BAG, NULL_SLOT, dest, itemId, 1, false );
        if( msg == EQUIP_ERR_OK )
        {
            Item* item = plTarget->StoreNewItem( dest, itemId, 1, true);

            // remove binding (let GM give it to another player later)
            if(pl==plTarget)
                item->SetBinding( false );

            pl->SendNewItem(item,1,false,true);
            if(pl!=plTarget)
                plTarget->SendNewItem(item,1,true,false);
        }
        else
        {
            pl->SendEquipError( msg, NULL, NULL );
            PSendSysMessage(LANG_ITEM_CANNOT_CREATE, itemId, 1);
        }

    }while( result->NextRow() );

    delete result;

    return true;
}

bool ChatHandler::HandleListItemCommand(const char* args)
{
    if(!*args)
        return false;

    char* c_item_id = strtok((char*)args, " ");
    uint32 item_id = atol(c_item_id);

    if(!item_id || !objmgr.GetItemPrototype(item_id))
    {
        PSendSysMessage(LANG_COMMAND_ITEMIDINVALID, item_id);
        return true;
    }

    char* c_count = strtok(NULL, " ");
    int count = c_count ? atol(c_count) : 10;

    if(count < 0)
        return false;

    QueryResult *result;

    // inventory case
    uint32 inv_count = 0;
    result=sDatabase.PQuery("SELECT COUNT(`item_template`) FROM `character_inventory` WHERE `item_template`='%u'",item_id);
    if(result)
    {
        inv_count = (*result)[0].GetUInt32();
        delete result;
    }

    result=sDatabase.PQuery(
    //           0              1           2           3                  4                     5
        "SELECT `ci`.`item`,`cibag`.`slot` AS `bag`,`ci`.`slot`,`ci`.`guid`,`character`.`account`,`character`.`name` "
        "FROM `character_inventory` AS `ci` LEFT JOIN `character_inventory` AS `cibag` ON (`cibag`.`item`=`ci`.`bag`),`character` "
        "WHERE `ci`.`item_template`='%u' AND `ci`.`guid` = `character`.`guid` LIMIT %u ",
        item_id,uint32(count));

    if(result)
    {
        do
        {
            Field *fields = result->Fetch();
            uint32 item_guid = fields[0].GetUInt32();
            uint32 item_bag = fields[1].GetUInt32();
            uint32 item_slot = fields[2].GetUInt32();
            uint32 owner_guid = fields[3].GetUInt32();
            uint32 owner_acc = fields[4].GetUInt32();
            std::string owner_name = fields[5].GetCppString();

            char const* item_pos = 0;
            if(Player::IsEquipmentPos(item_bag,item_slot))
                item_pos = "[equipped]";
            else if(Player::IsInventoryPos(item_bag,item_slot))
                item_pos = "[in inventory]";
            else if(Player::IsBankPos(item_bag,item_slot))
                item_pos = "[in bank]";
            else
                item_pos = "";

            PSendSysMessage(LANG_ITEMLIST_SLOT,
                item_guid,owner_name.c_str(),owner_guid,owner_acc,item_pos);
        } while (result->NextRow());

        uint64 res_count = result->GetRowCount();

        delete result;

        if(count > res_count)
            count-=res_count;
        else if(count)
            count = 0;
    }

    // mail case
    uint32 mail_count = 0;
    result=sDatabase.PQuery("SELECT COUNT(`item_template`) FROM `mail` WHERE `item_template`='%u'",item_id);
    if(result)
    {
        mail_count = (*result)[0].GetUInt32();
        delete result;
    }

    if(count > 0)
    {
        result=sDatabase.PQuery(
        //             0                  1               2                   3                  4               5                  6
            "SELECT `mail`.`item_guid`,`mail`.`sender`,`mail`.`receiver`,`char_s`.`account`,`char_s`.`name`,`char_r`.`account`,`char_r`.`name` "
            "FROM `mail`,`character` as `char_s`,`character` as `char_r` "
            "WHERE `mail`.`item_template`='%u' AND `char_s`.`guid` = `mail`.`sender` AND `char_r`.`guid` = `mail`.`receiver` LIMIT %u",
            item_id,uint32(count));
    }
    else
        result = NULL;

    if(result)
    {
        do
        {
            Field *fields = result->Fetch();
            uint32 item_guid        = fields[0].GetUInt32();
            uint32 item_s           = fields[1].GetUInt32();
            uint32 item_r           = fields[2].GetUInt32();
            uint32 item_s_acc       = fields[3].GetUInt32();
            std::string item_s_name = fields[4].GetCppString();
            uint32 item_r_acc       = fields[5].GetUInt32();
            std::string item_r_name = fields[6].GetCppString();

            char const* item_pos = "[in mail]";

            PSendSysMessage(LANG_ITEMLIST_MAIL,
                item_guid,item_s_name.c_str(),item_s,item_s_acc,item_r_name.c_str(),item_r,item_r_acc,item_pos);
        } while (result->NextRow());

        uint64 res_count = result->GetRowCount();

        delete result;

        if(count > res_count)
            count-=res_count;
        else if(count)
            count = 0;
    }

    // auction case
    uint32 auc_count = 0;
    result=sDatabase.PQuery("SELECT COUNT(`item_template`) FROM `auctionhouse` WHERE `item_template`='%u'",item_id);
    if(result)
    {
        auc_count = (*result)[0].GetUInt32();
        delete result;
    }

    if(count > 0)
    {
        result=sDatabase.PQuery(
        //                     0                         1                       2                     3
            "SELECT `auctionhouse`.`itemguid`,`auctionhouse`.`itemowner`,`character`.`account`,`character`.`name` "
            "FROM `auctionhouse`,`character` WHERE `auctionhouse`.`item_template`='%u' AND `character`.`guid` = `auctionhouse`.`itemowner` LIMIT %u",
            item_id,uint32(count));
    }
    else
        result = NULL;

    if(result)
    {
        do
        {
            Field *fields = result->Fetch();
            uint32 item_guid       = fields[0].GetUInt32();
            uint32 owner           = fields[1].GetUInt32();
            uint32 owner_acc       = fields[2].GetUInt32();
            std::string owner_name = fields[3].GetCppString();

            char const* item_pos = "[in auction]";

            PSendSysMessage(LANG_ITEMLIST_AUCTION, item_guid, owner_name.c_str(), owner, owner_acc,item_pos);
        } while (result->NextRow());

        delete result;
    }

    if(inv_count+mail_count+auc_count == 0)
    {
        SendSysMessage(LANG_COMMAND_NOITEMFOUND);
        return true;
    }

    PSendSysMessage(LANG_COMMAND_LISTITEMMESSAGE,item_id,inv_count+mail_count+auc_count,inv_count,mail_count,auc_count);

    return true;
}

bool ChatHandler::HandleListObjectCommand(const char* args)
{
    if(!*args)
        return false;

    char* cId = extractKeyFromLink((char*)args,"Hgameobject_entry");  // number or [name] Shift-click form |color|Hgameobject_entry:go_id|h[name]|h|r
    if(!cId)
        return false;

    uint32 go_id = atol(cId);

    GameObjectInfo const * gInfo = objmgr.GetGameObjectInfo(go_id);

    if(!go_id || !gInfo)
    {
        PSendSysMessage(LANG_COMMAND_LISTOBJINVALIDID, go_id);
        return true;
    }

    char* c_count = strtok(NULL, " ");
    int count = c_count ? atol(c_count) : 10;

    if(count < 0)
        return false;

    Player* pl = m_session->GetPlayer();
    QueryResult *result;

    uint32 obj_count = 0;
    result=sDatabase.PQuery("SELECT COUNT(`guid`) FROM `gameobject` WHERE `id`='%u'",go_id);
    if(result)
    {
        obj_count = (*result)[0].GetUInt32();
        delete result;
    }

    result = sDatabase.PQuery("SELECT `guid`, `id`, `position_x`, `position_y`, `position_z`, `orientation`, `map`, (POW(`position_x` - '%f', 2) + POW(`position_y` - '%f', 2) + POW(`position_z` - '%f', 2)) as `order` FROM `gameobject` WHERE `id` = '%u' ORDER BY `order` ASC LIMIT %u",
        pl->GetPositionX(), pl->GetPositionY(), pl->GetPositionZ(),go_id,uint32(count));

    if (result)
    {
        do
        {
            Field *fields = result->Fetch();
            uint32 guid = fields[0].GetUInt32();
            uint32 id = fields[1].GetUInt32();
            float x = fields[2].GetFloat();
            float y = fields[3].GetFloat();
            float z = fields[4].GetFloat();
            float o = fields[5].GetFloat();
            int mapid = fields[6].GetUInt16();

            PSendSysMessage(LANG_GO_LIST, guid, guid, gInfo->name, x, y, z, mapid);
        } while (result->NextRow());

        delete result;
    }

    PSendSysMessage(LANG_COMMAND_LISTOBJMESSAGE,go_id,obj_count);
    return true;
}

bool ChatHandler::HandleListCreatureCommand(const char* args)
{
    if(!*args)
        return false;

    char* cId = extractKeyFromLink((char*)args,"Hcreature_entry");  // number or [name] Shift-click form |color|Hcreature_entry:creature_id|h[name]|h|r
    if(!cId)
        return false;

    uint32 cr_id = atol(cId);

    CreatureInfo const* cInfo = objmgr.GetCreatureTemplate(cr_id);

    if(!cr_id || !cInfo)
    {
        PSendSysMessage(LANG_COMMAND_INVALIDCREATUREID, cr_id);
        return true;
    }

    char* c_count = strtok(NULL, " ");
    int count = c_count ? atol(c_count) : 10;

    if(count < 0)
        return false;

    Player* pl = m_session->GetPlayer();
    QueryResult *result;

    uint32 cr_count = 0;
    result=sDatabase.PQuery("SELECT COUNT(`guid`) FROM `creature` WHERE `id`='%u'",cr_id);
    if(result)
    {
        cr_count = (*result)[0].GetUInt32();
        delete result;
    }

    result = sDatabase.PQuery("SELECT `guid`, `id`, `position_x`, `position_y`, `position_z`, `orientation`, `map`, (POW(`position_x` - '%f', 2) + POW(`position_y` - '%f', 2) + POW(`position_z` - '%f', 2)) as `order` FROM `creature` WHERE `id` = '%u' ORDER BY `order` ASC LIMIT %u",
        pl->GetPositionX(), pl->GetPositionY(), pl->GetPositionZ(), cr_id,uint32(count));

    if (result)
    {
        do
        {
            Field *fields = result->Fetch();
            uint32 guid = fields[0].GetUInt32();
            uint32 id = fields[1].GetUInt32();
            float x = fields[2].GetFloat();
            float y = fields[3].GetFloat();
            float z = fields[4].GetFloat();
            float o = fields[5].GetFloat();
            int mapid = fields[6].GetUInt16();

            PSendSysMessage(LANG_CREATURE_LIST, guid, guid, cInfo->Name, x, y, z, mapid);
        } while (result->NextRow());

        delete result;
    }

    PSendSysMessage(LANG_COMMAND_LISTCREATUREMESSAGE,cr_id,cr_count);
    return true;
}

bool ChatHandler::HandleLookupItemCommand(const char* args)
{
    if(!*args)
        return false;
    std::string namepart = args;
    sDatabase.escape_string(namepart);

    QueryResult *result=sDatabase.PQuery("SELECT `entry`,`name` FROM `item_template` WHERE `name` LIKE \"%%%s%%\"",namepart.c_str());
    if(!result)
    {
        SendSysMessage(LANG_COMMAND_NOITEMFOUND);
        return true;
    }

    do
    {
        Field *fields = result->Fetch();
        uint32 id = fields[0].GetUInt32();
        std::string name = fields[1].GetCppString();
        // send item in "id - id [name]" format (??)
        PSendSysMessage(LANG_ITEM_LIST, id,id,name.c_str());
    } while (result->NextRow());

    delete result;
    return true;
}

bool ChatHandler::HandleLookupItemSetCommand(const char* args)
{
    if(!*args)
        return false;
    std::string namepart = args;
    uint32 counter = 0;                                     // Counter for figure out that we found smth.

    // Search in ItemSet.dbc
    for (uint32 id = 0; id < sItemSetStore.GetNumRows(); id++)
    {
        ItemSetEntry const *set = sItemSetStore.LookupEntry(id);
        if(set)
        {
            std::string name = set->name[sWorld.GetDBClang()];

            // converting name to lower case
            std::transform( name.begin(), name.end(), name.begin(), ::tolower );

            // converting string that we try to find to lower case
            std::transform( namepart.begin(), namepart.end(), namepart.begin(), ::tolower );

            if (name.find(namepart) != std::string::npos)
            {
                // send item set in "id - [namedlink]" format
                PSendSysMessage(LANG_ITEMSET_LIST,id,id,set->name[sWorld.GetDBClang()]);
                counter++;
            }
        }
    }
    if (counter == 0)                                       // if counter == 0 then we found nth
        SendSysMessage(LANG_COMMAND_NOITEMSETFOUND);
    return true;
}

bool ChatHandler::HandleLookupSkillCommand(const char* args)
{
    Player* target = getSelectedPlayer();
    if(!target)
    {
        PSendSysMessage(LANG_PLAYER_NOT_FOUND);
        return true;
    }

    if(!*args)
        return false;

    std::string namepart = args;
    uint32 counter = 0;                                     // Counter for figure out that we found smth.

    // Search in SkillLine.dbc
    for (uint32 id = 0; id < sSkillLineStore.GetNumRows(); id++)
    {
        SkillLineEntry const *skillInfo = sSkillLineStore.LookupEntry(id);
        if(skillInfo)
        {
            // name - is first name field from dbc (English localized)
            std::string name = skillInfo->name[sWorld.GetDBClang()];

            // converting SkillName to lower case
            std::transform( name.begin(), name.end(), name.begin(), ::tolower );
            // converting string that we try to find to lower case
            std::transform( namepart.begin(), namepart.end(), namepart.begin(), ::tolower );

            if (name.find(namepart) != std::string::npos)
            {
                uint16 skill = target->GetPureSkillValue(id);
                // send skill in "id - [namedlink]" format
                PSendSysMessage(LANG_SKILL_LIST "%s",id,id,skillInfo->name[sWorld.GetDBClang()],(skill == 0 ? "" : LANG_KNOWN));

                counter++;
            }
        }
    }
    if (counter == 0)                                       // if counter == 0 then we found nth
        SendSysMessage(LANG_COMMAND_NOSKILLFOUND);
    return true;
}

bool ChatHandler::HandleLookupSpellCommand(const char* args)
{
    Player* target = getSelectedPlayer();
    if( !target )
    {
        PSendSysMessage(LANG_PLAYER_NOT_FOUND);
        return true;
    }

    if(!*args)
        return false;
    std::string namepart = args;
    uint32 counter = 0;                                     // Counter for figure out that we found smth.

    // Search in Spell.dbc
    for (uint32 id = 0; id < sSpellStore.GetNumRows(); id++)
    {
        SpellEntry const *spellInfo = sSpellStore.LookupEntry(id);
        if(spellInfo)
        {
            // name - is first name field from dbc (English localized)
            std::string name = spellInfo->SpellName[sWorld.GetDBClang()];

            // converting SpellName to lower case
            std::transform( name.begin(), name.end(), name.begin(), ::tolower );

            // converting string that we try to find to lower case
            std::transform( namepart.begin(), namepart.end(), namepart.begin(), ::tolower );

            if (name.find(namepart) != std::string::npos)
            {
                uint32 rank = objmgr.GetSpellRank(id);      // unit32 used to prevent interpreting uint8 as char at output

                bool known = target->HasSpell(id);
                bool learn = (spellInfo->Effect[0] == SPELL_EFFECT_LEARN_SPELL);
                bool talent = (GetTalentSpellCost(id) > 0);
                bool passive = IsPassiveSpell(id);

                // send spell in "id - [name, rank N] [talent] [passive] [learn] [known]" format
                std::ostringstream ss;
                ss << id << " - |cffffffff|Hspell:" << id << "|h[" << spellInfo->SpellName[sWorld.GetDBClang()];

                // include rank in link name
                if(rank)
                    ss << LANG_SPELL_RANK << rank;

                ss << "]|h|r";

                if(talent)
                    ss << LANG_TALENT;
                if(passive)
                    ss << LANG_PASSIVE;
                if(learn)
                    ss << LANG_LEARN;
                if(known)
                    ss << LANG_KNOWN;

                SendSysMessage(ss.str().c_str());

                ++counter;
            }
        }
    }
    if (counter == 0)                                       // if counter == 0 then we found nth
        SendSysMessage(LANG_COMMAND_NOSPELLFOUND);
    return true;
}

bool ChatHandler::HandleLookupQuestCommand(const char* args)
{
    Player* target = getSelectedPlayer();    
    if( !target )
    {
        PSendSysMessage(LANG_PLAYER_NOT_FOUND);
        return true;
    }

    if(!*args)
        return false;

    std::string namepart = args;
    sDatabase.escape_string(namepart);

    QueryResult *result=sDatabase.PQuery("SELECT `entry`,`Title` FROM `quest_template` WHERE `Title` LIKE \"%%%s%%\" ORDER BY `entry`",namepart.c_str());
    if(!result)
    {
        SendSysMessage(LANG_COMMAND_NOQUESTFOUND);
        return true;
    }

    do
    {
        Field *fields = result->Fetch();
        uint16 id = fields[0].GetUInt16();
        std::string name = fields[1].GetCppString();

        QuestStatus status = target->GetQuestStatus(id);

        char const* statusStr = "";
        if(status == QUEST_STATUS_COMPLETE)
        {
            if(m_session->GetPlayer()->GetQuestRewardStatus(id))
                statusStr = LANG_COMMAND_QUEST_REWARDED;
            else
                statusStr = LANG_COMMAND_QUEST_COMPLETE;
        }
        else if(status == QUEST_STATUS_INCOMPLETE)
            statusStr = LANG_COMMAND_QUEST_ACTIVE;

        PSendSysMessage(LANG_QUEST_LIST "%s",id,id,name.c_str(),(status == QUEST_STATUS_COMPLETE ? LANG_COMPLETE : (status == QUEST_STATUS_INCOMPLETE ? LANG_ACTIVE : "") ));
    } while (result->NextRow());

    delete result;
    return true;
}

bool ChatHandler::HandleLookupCreatureCommand(const char* args)
{
    if(!*args)
        return false;

    std::string namepart = args;
    sDatabase.escape_string(namepart);

    QueryResult *result=sDatabase.PQuery("SELECT `entry`,`name` FROM `creature_template` WHERE `name` LIKE \"%%%s%%\"",namepart.c_str());
    if(!result)
    {
        SendSysMessage(LANG_COMMAND_NOCREATUREFOUND);
        return true;
    }

    do
    {
        Field *fields = result->Fetch();
        uint16 id = fields[0].GetUInt16();
        std::string name = fields[1].GetCppString();
        PSendSysMessage(LANG_CREATURE_ENTRY_LIST,id,id,name.c_str());
    } while (result->NextRow());

    delete result;
    return true;
}

bool ChatHandler::HandleLookupObjectCommand(const char* args)
{
    if(!*args)
        return false;

    std::string namepart = args;
    sDatabase.escape_string(namepart);

    QueryResult *result=sDatabase.PQuery("SELECT `entry`,`name` FROM `gameobject_template` WHERE `name` LIKE \"%%%s%%\"",namepart.c_str());
    if(!result)
    {
        SendSysMessage(LANG_COMMAND_NOGAMEOBJECTFOUND);
        return true;
    }

    do
    {
        Field *fields = result->Fetch();
        uint32 id = fields[0].GetUInt32();
        std::string name = fields[1].GetCppString();
        PSendSysMessage(LANG_GO_ENTRY_LIST,id,id,name.c_str());
    } while (result->NextRow());

    delete result;
    return true;
}

bool ChatHandler::HandleCreateGuildCommand(const char* args)
{
    Guild *guild;
    Player * player;
    char *lname,*gname;
    std::string guildname;

    if (!*args)
        return false;

    lname = strtok((char*)args, " ");
    gname = strtok(NULL, "");

    if(!lname)
        return false;
    else if(!gname)
    {
        SendSysMessage(LANG_INSERT_GUILD_NAME);
        return true;
    }

    guildname = gname;
    player = ObjectAccessor::Instance().FindPlayerByName(lname);

    if(!player)
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        return true;
    }

    if(!player->GetGuildId())
    {
        guild = new Guild;
        if(!guild->create(player->GetGUID(),guildname))
        {
            delete guild;
            SendSysMessage(LANG_GUILD_NOT_CREATED);
            return true;
        }

        objmgr.AddGuild(guild);
    }
    else
        SendSysMessage(LANG_PLAYER_IN_GUILD);

    return true;
}

//float max_creature_distance = 160;

bool ChatHandler::HandleGetDistanceCommand(const char* args)
{
    Unit* pUnit = getSelectedUnit();

    if(!pUnit)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        return true;
    }

    PSendSysMessage(LANG_DISTANCE, m_session->GetPlayer()->GetDistanceSq(pUnit));

    return true;
}

bool ChatHandler::HandleObjectCommand(const char* args)
{
    if (!*args)
        return false;

    uint32 display_id = atoi((char*)args);

    char* safe = strtok((char*)args, " ");

    Player *chr = m_session->GetPlayer();
    float x = chr->GetPositionX();
    float y = chr->GetPositionY();
    float z = chr->GetPositionZ();
    float o = chr->GetOrientation();

    GameObject* pGameObj = new GameObject(chr);
    if(!pGameObj->Create(objmgr.GenerateLowGuid(HIGHGUID_GAMEOBJECT), display_id, chr->GetMapId(), x, y, z, o, 0, 0, 0, 0, 0, 0))
    {
        delete pGameObj;
        return false;
    }
    sLog.outDebug(LANG_ADD_OBJ_LV3);

    if(strcmp(safe,"true") == 0)
        pGameObj->SaveToDB();

    MapManager::Instance().GetMap(pGameObj->GetMapId(), pGameObj)->Add(pGameObj);

    return true;
}

// FIX-ME!!!

bool ChatHandler::HandleAddWeaponCommand(const char* args)
{
    /*if (!*args)
        return false;

    uint64 guid = m_session->GetPlayer()->GetSelection();
    if (guid == 0)
    {
        SendSysMessage(LANG_NO_SELECTION);
        return true;
    }

    Creature *pCreature = ObjectAccessor::Instance().GetCreature(*m_session->GetPlayer(), guid);

    if(!pCreature)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        return true;
    }

    char* pSlotID = strtok((char*)args, " ");
    if (!pSlotID)
        return false;

    char* pItemID = strtok(NULL, " ");
    if (!pItemID)
        return false;

    uint32 ItemID = atoi(pItemID);
    uint32 SlotID = atoi(pSlotID);

    ItemPrototype* tmpItem = objmgr.GetItemPrototype(ItemID);

    bool added = false;
    if(tmpItem)
    {
        switch(SlotID)
        {
            case 1:
                pCreature->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_DISPLAY, ItemID);
                added = true;
                break;
            case 2:
                pCreature->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_DISPLAY_01, ItemID);
                added = true;
                break;
            case 3:
                pCreature->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_DISPLAY_02, ItemID);
                added = true;
                break;
            default:
                PSendSysMessage(LANG_ITEM_SLOT_NOT_EXIST,SlotID);
                added = false;
                break;
        }
        if(added)
        {
            PSendSysMessage(LANG_ITEM_ADDED_TO_SLOT,ItemID,tmpItem->Name1,SlotID);
        }
    }
    else
    {
        PSendSysMessage(LANG_ITEM_NOT_FOUND,ItemID);
        return true;
    }
    */
    return true;
}

bool ChatHandler::HandleGameObjectCommand(const char* args)
{
    if (!*args)
        return false;


    char* pParam1 = strtok((char*)args, " ");
    uint32 id = atoi((char*)pParam1);
    if(!id)
        return false;

    char* lootID = strtok(NULL, " ");
    char* spawntimeSecs = strtok(NULL, " ");

    const GameObjectInfo *goI = objmgr.GetGameObjectInfo(id);

    if (!goI)
    {
        PSendSysMessage(LANG_GAMEOBJECT_NOT_EXIST,id);
        return false;
    }

    Player *chr = m_session->GetPlayer();
    float x = float(chr->GetPositionX());
    float y = float(chr->GetPositionY());
    float z = float(chr->GetPositionZ());
    float o = float(chr->GetOrientation());

    float rot2 = sin(o/2);
    float rot3 = cos(o/2);

    GameObject* pGameObj = new GameObject(chr);
    uint32 lowGUID = objmgr.GenerateLowGuid(HIGHGUID_GAMEOBJECT);

    if(!pGameObj->Create(lowGUID, goI->id, chr->GetMapId(), x, y, z, o, 0, 0, rot2, rot3, 0, 0))
    {
        delete pGameObj;
        return false;
    }
    //pGameObj->SetZoneId(chr->GetZoneId());
    pGameObj->SetMapId(chr->GetMapId());
    //pGameObj->SetNameId(id);
    sLog.outDebug(LANG_GAMEOBJECT_CURRENT, goI->name, lowGUID, x, y, z, o);

    if( lootID )
    {
        uint32 value = atoi((char*)lootID);
        pGameObj->lootid = value;
        //sLog.outDebug("*** LOOT: %d", value);
    }

    if( spawntimeSecs )
    {
        uint32 value = atoi((char*)spawntimeSecs);
        pGameObj->SetRespawnTime(value);
        //sLog.outDebug("*** spawntimeSecs: %d", value);
    }

    pGameObj->SaveToDB();
    MapManager::Instance().GetMap(pGameObj->GetMapId(), pGameObj)->Add(pGameObj);

    PSendSysMessage(LANG_GAMEOBJECT_ADD,id,goI->name,x,y,z);

    return true;
}

bool ChatHandler::HandleAnimCommand(const char* args)
{
    if (!*args)
        return false;

    uint32 anim_id = atoi((char*)args);

    WorldPacket data( SMSG_EMOTE, (8+4) );
    data << anim_id << m_session->GetPlayer( )->GetGUID();
    WPAssert(data.size() == 12);
    MapManager::Instance().GetMap(m_session->GetPlayer()->GetMapId(), m_session->GetPlayer())->MessageBoardcast(m_session->GetPlayer(), &data, true);
    return true;
}

bool ChatHandler::HandleStandStateCommand(const char* args)
{
    if (!*args)
        return false;

    uint32 anim_id = atoi((char*)args);
    m_session->GetPlayer( )->SetUInt32Value( UNIT_NPC_EMOTESTATE , anim_id );

    return true;
}

bool ChatHandler::HandleDieCommand(const char* args)
{
    Unit* target = getSelectedUnit();

    if(!target || !m_session->GetPlayer()->GetSelection())
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        return true;
    }

    if( target->isAlive() )
    {
        m_session->GetPlayer()->DealDamage(target, target->GetHealth(), NULL, DIRECT_DAMAGE, 0, NULL, 0, false);
    }

    return true;
}

bool ChatHandler::HandleReviveCommand(const char* args)
{
    Player* SelectedPlayer = NULL;

    if (*args)
    {
        std::string name = args;
        normalizePlayerName(name);
        SelectedPlayer = objmgr.GetPlayer(name.c_str());
    }
    else
        SelectedPlayer = getSelectedPlayer();

    if(!SelectedPlayer)
    {
        PSendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }

    SelectedPlayer->ResurrectPlayer(0.5f);
    SelectedPlayer->SpawnCorpseBones();
    SelectedPlayer->SaveToDB();
    return true;
}

bool ChatHandler::HandleMorphCommand(const char* args)
{
    if (!*args)
        return false;

    uint16 display_id = (uint16)atoi((char*)args);

    Unit *target = getSelectedUnit();
    if(!target)
        target = m_session->GetPlayer();

    target->SetUInt32Value(UNIT_FIELD_DISPLAYID, display_id);

    return true;
}

bool ChatHandler::HandleAuraCommand(const char* args)
{
    char* px = strtok((char*)args, " ");
    if (!px)
        return false;

    Unit *target = getSelectedUnit();
    if(!target)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        return true;
    }

    uint32 spellID = (uint32)atoi(px);
    SpellEntry const *spellInfo = sSpellStore.LookupEntry( spellID );
    if(spellInfo)
    {
        for(uint32 i = 0;i<3;i++)
        {
            uint8 eff = spellInfo->Effect[i];
            if (eff>=TOTAL_SPELL_EFFECTS)
                continue;
            if (eff == SPELL_EFFECT_APPLY_AURA || eff == SPELL_EFFECT_APPLY_AREA_AURA || eff == SPELL_EFFECT_PERSISTENT_AREA_AURA)
            {
                Aura *Aur = new Aura(spellInfo, i, target);
                target->AddAura(Aur);
            }
        }
    }

    return true;
}

bool ChatHandler::HandleUnAuraCommand(const char* args)
{
    char* px = strtok((char*)args, " ");
    if (!px)
        return false;

    Unit *target = getSelectedUnit();
    if(!target)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        return true;
    }

    std::string argstr = args;
    if (argstr == "all")
    {
        target->RemoveAllAuras();
        return true;
    }

    uint32 spellID = (uint32)atoi(px);
    target->RemoveAurasDueToSpell(spellID);

    return true;
}

bool ChatHandler::HandleLinkGraveCommand(const char* args)
{
    if(!*args)
        return false;

    char* px = strtok((char*)args, " ");
    if (!px)
        return false;

    uint32 g_id = (uint32)atoi(px);

    uint32 g_team;

    char* px2 = strtok(NULL, " ");

    if (!px2)
        g_team = 0;
    else if (strncmp(px2,"horde",6)==0)
        g_team = HORDE;
    else if (strncmp(px2,"alliance",9)==0)
        g_team = ALLIANCE;
    else
        return false;

    WorldSafeLocsEntry const* graveyard =  sWorldSafeLocsStore.LookupEntry(g_id);

    if(!graveyard )
    {
        PSendSysMessage(LANG_COMMAND_GRAVEYARDNOEXIST, g_id);
        return true;
    }

    Player* player = m_session->GetPlayer();

    QueryResult *result = sDatabase.PQuery(
        "SELECT `id` FROM `game_graveyard_zone` WHERE `id` = %u AND `ghost_map` = %u AND `ghost_zone` = '%u' AND (`faction` = %u OR `faction` = 0 )",
        g_id,player->GetMapId(),player->GetZoneId(),g_team);

    if(result)
    {
        delete result;

        PSendSysMessage(LANG_COMMAND_GRAVEYARDALRLINKED, g_id,player->GetZoneId());
    }
    else
    {
        sDatabase.PExecuteLog("INSERT INTO `game_graveyard_zone` ( `id`,`ghost_map`,`ghost_zone`,`faction`) VALUES ('%u', '%u', '%u','%u')",
            g_id,player->GetMapId(),player->GetZoneId(),g_team);

        PSendSysMessage(LANG_COMMAND_GRAVEYARDLINKED, g_id,player->GetZoneId());
    }
    return true;
}

bool ChatHandler::HandleNearGraveCommand(const char* args)
{
    uint32 g_team;

    if(!*args)
        g_team = 0;
    else if (strncmp((char*)args,"horde",6)==0)
        g_team = HORDE;
    else if (strncmp((char*)args,"alliance",9)==0)
        g_team = ALLIANCE;
    else
        return false;

    Player* player = m_session->GetPlayer();

    WorldSafeLocsEntry const* graveyard = objmgr.GetClosestGraveYard(
        player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(),player->GetMapId(),player->GetTeam());

    if(graveyard)
    {
        uint32 g_id = graveyard->ID;

        QueryResult *result = sDatabase.PQuery("SELECT `faction` FROM `game_graveyard_zone` WHERE `id` = %u",g_id);
        if (!result)
        {
            PSendSysMessage(LANG_COMMAND_GRAVEYARDERROR,g_id);
            return true;
        }

        Field *fields = result->Fetch();
        g_team = fields[0].GetUInt32();
        delete result;

        std::string team_name = LANG_COMMAND_GRAVEYARD_NOTEAM;

        if(g_team == 0)
            team_name = LANG_COMMAND_GRAVEYARD_ANY;
        else if(g_team == HORDE)
            team_name = LANG_COMMAND_GRAVEYARD_HORDE;
        else if(g_team == ALLIANCE)
            team_name = LANG_COMMAND_GRAVEYARD_ALLIANCE;

        PSendSysMessage(LANG_COMMAND_GRAVEYARDNEAREST, g_id,team_name.c_str(),player->GetZoneId());
    }
    else
    {
        std::string team_name;

        if(g_team == 0)
            team_name = LANG_COMMAND_GRAVEYARD_ANY;
        else if(g_team == HORDE)
            team_name = LANG_COMMAND_GRAVEYARD_HORDE;
        else if(g_team == ALLIANCE)
            team_name = LANG_COMMAND_GRAVEYARD_ALLIANCE;

        if(g_team == ~uint32(0))
            PSendSysMessage(LANG_COMMAND_ZONENOGRAVEYARDS, player->GetZoneId());
        else
            PSendSysMessage(LANG_COMMAND_ZONENOGRAFACTION, player->GetZoneId(),team_name.c_str());
    }

    return true;
}

bool ChatHandler::HandleSpawnTransportCommand(const char* args)
{
    return true;
}

bool ChatHandler::HandleEmoteCommand(const char* args)
{
    uint32 emote = atoi((char*)args);

    Creature* target = getSelectedCreature();
    if(!target)
    {
        PSendSysMessage(LANG_SELECT_CREATURE);
        return true;
    }

    target->SetUInt32Value(UNIT_NPC_EMOTESTATE,emote);

    return true;
}

bool ChatHandler::HandleNpcInfoCommand(const char* args)
{
    Creature* target = getSelectedCreature();

    if(!target)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        return true;
    }

    uint32 faction = target->getFaction();
    uint32 npcflags = target->GetUInt32Value(UNIT_NPC_FLAGS);
    uint32 displayid = target->GetUInt32Value(UNIT_FIELD_DISPLAYID);
    uint32 nativeid = target->GetUInt32Value(UNIT_FIELD_NATIVEDISPLAYID);
    uint32 Entry = target->GetUInt32Value(OBJECT_FIELD_ENTRY);
    CreatureInfo const* cInfo = target->GetCreatureInfo();

    PSendSysMessage(LANG_NPCINFO_CHAR,  target->GetDBTableGUIDLow(), faction, npcflags, Entry, displayid, nativeid);
    PSendSysMessage(LANG_NPCINFO_LEVEL, target->getLevel());
    PSendSysMessage(LANG_NPCINFO_HEALTH,target->GetUInt32Value(UNIT_FIELD_BASE_HEALTH), target->GetMaxHealth(), target->GetHealth());
    PSendSysMessage(LANG_NPCINFO_FLAGS, target->GetUInt32Value(UNIT_FIELD_FLAGS), target->GetUInt32Value(UNIT_DYNAMIC_FLAGS), target->getFaction());
    PSendSysMessage(LANG_NPCINFO_LOOT,  cInfo->lootid,cInfo->pickpocketLootId,cInfo->SkinLootId);
    PSendSysMessage(LANG_NPCINFO_DUNGEON_ID, target->GetInstanceId());

    PSendSysMessage(LANG_NPCINFO_POSITION,float(target->GetPositionX()), float(target->GetPositionY()), float(target->GetPositionZ()));

    if ((npcflags & UNIT_NPC_FLAG_VENDOR) )
    {
        SendSysMessage(LANG_NPCINFO_VENDOR);
    }
    if ((npcflags & UNIT_NPC_FLAG_TRAINER) )
    {
        SendSysMessage(LANG_NPCINFO_TRAINER);
    }

    return true;
}

bool ChatHandler::HandleNpcInfoSetCommand(const char* args)
{
    uint32 entry = 0, testvalue = 0;

    Creature* target = getSelectedCreature();
    if(!target)
    {
        PSendSysMessage(LANG_SELECT_CREATURE);
        return true;
    }

    if(!args)
        return true;

    //m_session->GetPlayer( )->SetUInt32Value(PLAYER_FLAGS, (uint32)8);

    testvalue = uint32(atoi((char*)args));

    entry = target->GetUInt32Value( OBJECT_FIELD_ENTRY );

    //m_session->SendTestCreatureQueryOpcode( entry, target->GetGUID(), testvalue );

    return true;
}

bool ChatHandler::HandleExploreCheatCommand(const char* args)
{
    if (!*args)
        return false;

    int flag = atoi((char*)args);

    Player *chr = getSelectedPlayer();
    if (chr == NULL)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }

    if (flag != 0)
    {
        PSendSysMessage(LANG_YOU_SET_EXPLORE_ALL, chr->GetName());
    }
    else
    {
        PSendSysMessage(LANG_YOU_SET_EXPLORE_NOTHING, chr->GetName());
    }

    char buf[256];

    if (flag != 0)
    {
        sprintf((char*)buf,LANG_YOURS_EXPLORE_SET_ALL,
            m_session->GetPlayer()->GetName());
    }
    else
    {
        sprintf((char*)buf,LANG_YOURS_EXPLORE_SET_NOTHING,
            m_session->GetPlayer()->GetName());
    }

    WorldPacket data;
    FillSystemMessageData(&data, m_session, buf);
    chr->GetSession()->SendPacket(&data);

    for (uint8 i=0; i<64; i++)
    {
        if (flag != 0)
        {
            m_session->GetPlayer()->SetFlag(PLAYER_EXPLORED_ZONES_1+i,0xFFFFFFFF);
        }
        else
        {
            m_session->GetPlayer()->SetFlag(PLAYER_EXPLORED_ZONES_1+i,0);
        }
    }

    return true;
}

bool ChatHandler::HandleHoverCommand(const char* args)
{
    char* px = strtok((char*)args, " ");
    uint32 flag;
    if (!px)
        flag = 1;
    else
        flag = atoi(px);

    m_session->GetPlayer()->SetHover(flag);

    if (flag)
        SendSysMessage(LANG_HOVER_ENABLED);
    else
        PSendSysMessage(LANG_HOVER_DISABLED);

    return true;
}

bool ChatHandler::HandleLevelUpCommand(const char* args)
{
    char* px = strtok((char*)args, " ");
    char* py = strtok((char*)NULL, " ");

    // command format parsing
    char* pname = (char*)NULL;
    int addlevel = 1;

    if(px && py)                                            // .levelup name level
    {
        addlevel = atoi(py);
        pname = px;
    }
    else if(px && !py)                                      // .levelup name OR .levelup level
    {
        if(isalpha(px[0]))                                  // .levelup name
            pname = px;
        else                                                // .levelup level
            addlevel = atoi(px);
    }
    // else .levelup - nothing do for prepering

    // player
    Player *chr = NULL;
    uint64 chr_guid = 0;

    if(pname)                                               // player by name
    {
        std::string name = pname;
        normalizePlayerName(name);

        chr = objmgr.GetPlayer(name.c_str());
        if(!chr)                                            // not in game
        {
            chr_guid = objmgr.GetPlayerGUIDByName(name);
            if (chr_guid == 0)
            {
                SendSysMessage(LANG_PLAYER_NOT_FOUND);
                return true;
            }
        }
    }
    else                                                    // player by selection
    {
        chr = getSelectedPlayer();

        if (chr == NULL)
        {
            SendSysMessage(LANG_NO_CHAR_SELECTED);
            return true;
        }
    }

    assert(chr || chr_guid);

    int32 oldlevel = chr ? chr->getLevel() : Player::GetUInt32ValueFromDB(UNIT_FIELD_LEVEL,chr_guid);
    int32 newlevel = oldlevel + addlevel;
    if(newlevel < 1)
        newlevel = 1;
    if(newlevel > 255)                                      // hardcoded maximum level
        newlevel = 255;

    if(chr)
    {
        chr->GiveLevel(newlevel);
        chr->InitTalentForLevel();
        chr->SetUInt32Value(PLAYER_XP,0);

        WorldPacket data;

        if(oldlevel == newlevel)
            FillSystemMessageData(&data, chr->GetSession(), LANG_YOURS_LEVEL_PROGRESS_RESET);
        else
        if(oldlevel < newlevel)
            FillSystemMessageData(&data, chr->GetSession(), fmtstring(LANG_YOURS_LEVEL_UP,newlevel-oldlevel));
        else
        if(oldlevel > newlevel)
            FillSystemMessageData(&data, chr->GetSession(), fmtstring(LANG_YOURS_LEVEL_DOWN,newlevel-oldlevel));

        chr->GetSession()->SendPacket( &data );

        // give level to summoned pet
        Pet* pet = chr->GetPet();
        if(pet && pet->getPetType()==SUMMON_PET)
            pet->GivePetLevel(newlevel);
    }
    else
    {
        // update levle and XP at level, all other will be updated at loading
        std::vector<std::string> values;
        Player::LoadValuesArrayFromDB(values,chr_guid);
        Player::SetUInt32ValueInArray(values,UNIT_FIELD_LEVEL,newlevel);
        Player::SetUInt32ValueInArray(values,PLAYER_XP,0);
        Player::SaveValuesArrayInDB(values,chr_guid);
    }

    return true;
}

bool ChatHandler::HandleShowAreaCommand(const char* args)
{
    if (!*args)
        return false;

    int area = atoi((char*)args);

    Player *chr = getSelectedPlayer();
    if (chr == NULL)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }

    int offset = area / 32;
    uint32 val = (uint32)(1 << (area % 32));

    if(offset >= 64)
    {
        SendSysMessage(LANG_BAD_VALUE);
        return true;
    }

    uint32 currFields = chr->GetUInt32Value(PLAYER_EXPLORED_ZONES_1 + offset);
    chr->SetUInt32Value(PLAYER_EXPLORED_ZONES_1 + offset, (uint32)(currFields | val));

    SendSysMessage(LANG_EXPLORE_AREA);
    return true;
}

bool ChatHandler::HandleHideAreaCommand(const char* args)
{
    if (!*args)
        return false;

    int area = atoi((char*)args);

    Player *chr = getSelectedPlayer();
    if (chr == NULL)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }

    int offset = area / 32;
    uint32 val = (uint32)(1 << (area % 32));

    if(offset >= 64)
    {
        SendSysMessage(LANG_BAD_VALUE);
        return true;
    }

    uint32 currFields = chr->GetUInt32Value(PLAYER_EXPLORED_ZONES_1 + offset);
    chr->SetUInt32Value(PLAYER_EXPLORED_ZONES_1 + offset, (uint32)(currFields ^ val));

    SendSysMessage(LANG_UNEXPLORE_AREA);
    return true;
}

bool ChatHandler::HandleUpdate(const char* args)
{
    if(!*args)
        return false;

    uint32 updateIndex;
    uint32 value;

    char* pUpdateIndex = strtok((char*)args, " ");

    Unit* chr = getSelectedUnit();
    if (chr == NULL)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        return true;
    }

    if(!pUpdateIndex)
    {
        return true;
    }
    updateIndex = atoi(pUpdateIndex);
    //check updateIndex
    if(chr->GetTypeId() == TYPEID_PLAYER)
    {
        if (updateIndex>=PLAYER_END) return true;
    }
    else
    {
        if (updateIndex>=UNIT_END) return true;
    }

    char*  pvalue = strtok(NULL, " ");
    if (!pvalue)
    {
        value=chr->GetUInt32Value(updateIndex);

        PSendSysMessage(LANG_UPDATE, chr->GetGUIDLow(),updateIndex,value);
        return true;
    }

    value=atoi(pvalue);

    PSendSysMessage(LANG_UPDATE_CHANGE, chr->GetGUIDLow(),updateIndex,value);

    chr->SetUInt32Value(updateIndex,value);

    return true;
}

bool ChatHandler::HandleBankCommand(const char* args)
{
    m_session->SendShowBank( m_session->GetPlayer()->GetGUID() );

    return true;
}

bool ChatHandler::HandleChangeWeather(const char* args)
{
    if(!*args)
        return false;

    //Weather is OFF
    if (sWorld.getConfig(CONFIG_WEATHER) == 0)
    {
        SendSysMessage(LANG_WEATHER_DISABLED);
        return true;
    }

    //*Change the weather of a cell
    char* px = strtok((char*)args, " ");
    char* py = strtok(NULL, " ");

    if (!px || !py)
        return false;

    uint32 type = (uint32)atoi(px);                         //0 to 3, 0: fine, 1: rain, 2: snow, 3: sand
    float grade = (float)atof(py);                          //0 to 1, sending -1 is instand good weather

    Player *player = m_session->GetPlayer();
    uint32 zoneid = player->GetZoneId();

    Weather* wth = sWorld.FindWeather(zoneid);

    if(!wth)
        wth = sWorld.AddWeather(zoneid);
    if(!wth)
    {
        SendSysMessage(LANG_NO_WEATHER);
        return true;
    }

    wth->SetWeather(type, grade);

    return true;
}

bool ChatHandler::HandleSetValue(const char* args)
{
    if(!*args)
        return false;

    char* px = strtok((char*)args, " ");
    char* py = strtok(NULL, " ");
    char* pz = strtok(NULL, " ");

    if (!px || !py)
        return false;

    Unit* target = getSelectedUnit();
    if(!target)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        return true;
    }

    uint64 guid = target->GetGUID();

    uint32 Opcode = (uint32)atoi(px);
    if(Opcode >= target->GetValuesCount())
    {
        PSendSysMessage(LANG_TOO_BIG_INDEX, Opcode, GUID_LOPART(guid), target->GetValuesCount());
        return false;
    }
    uint32 iValue;
    float fValue;
    bool isint32 = true;
    if(pz)
        isint32 = (bool)atoi(pz);
    if(isint32)
    {
        iValue = (uint32)atoi(py);
        sLog.outDebug( LANG_SET_UINT, GUID_LOPART(guid), Opcode, iValue);
        target->SetUInt32Value( Opcode , iValue );
        PSendSysMessage(LANG_SET_UINT_FIELD, GUID_LOPART(guid), Opcode,iValue);
    }
    else
    {
        fValue = (float)atof(py);
        sLog.outDebug( LANG_SET_FLOAT, GUID_LOPART(guid), Opcode, fValue);
        target->SetFloatValue( Opcode , fValue );
        PSendSysMessage(LANG_SET_FLOAT_FIELD, GUID_LOPART(guid), Opcode,fValue);
    }

    return true;
}

bool ChatHandler::HandleGetValue(const char* args)
{
    if(!*args)
        return false;

    char* px = strtok((char*)args, " ");
    char* pz = strtok(NULL, " ");

    if (!px)
        return false;

    Unit* target = getSelectedUnit();
    if(!target)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        return true;
    }

    uint64 guid = target->GetGUID();

    uint32 Opcode = (uint32)atoi(px);
    if(Opcode >= target->GetValuesCount())
    {
        PSendSysMessage(LANG_TOO_BIG_INDEX, Opcode, GUID_LOPART(guid), target->GetValuesCount());
        return false;
    }
    uint32 iValue;
    float fValue;
    bool isint32 = true;
    if(pz)
        isint32 = (bool)atoi(pz);

    if(isint32)
    {
        iValue = target->GetUInt32Value( Opcode );
        sLog.outDebug( LANG_GET_UINT, GUID_LOPART(guid), Opcode, iValue);
        PSendSysMessage(LANG_GET_UINT_FIELD, GUID_LOPART(guid), Opcode,    iValue);
    }
    else
    {
        fValue = target->GetFloatValue( Opcode );
        sLog.outDebug( LANG_GET_FLOAT, GUID_LOPART(guid), Opcode, fValue);
        PSendSysMessage(LANG_GET_FLOAT_FIELD, GUID_LOPART(guid), Opcode, fValue);
    }

    return true;
}

bool ChatHandler::HandleSet32Bit(const char* args)
{
    if(!*args)
        return false;

    char* px = strtok((char*)args, " ");
    char* py = strtok(NULL, " ");

    if (!px || !py)
        return false;

    uint32 Opcode = (uint32)atoi(px);
    uint32 Value = (uint32)atoi(py);
    if (Value > 32)                                         //uint32 = 32 bits
        return false;

    sLog.outDebug( LANG_SET_32BIT , Opcode, Value);

    m_session->GetPlayer( )->SetUInt32Value( Opcode , 2^Value );

    PSendSysMessage(LANG_SET_32BIT_FIELD, Opcode,1);
    return true;
}

bool ChatHandler::HandleMod32Value(const char* args)
{
    if(!*args)
        return false;

    char* px = strtok((char*)args, " ");
    char* py = strtok(NULL, " ");

    if (!px || !py)
        return false;

    uint32 Opcode = (uint32)atoi(px);
    int Value = atoi(py);

    if(Opcode >= m_session->GetPlayer()->GetValuesCount())
    {
        PSendSysMessage(LANG_TOO_BIG_INDEX, Opcode, m_session->GetPlayer()->GetGUIDLow(), m_session->GetPlayer( )->GetValuesCount());
        return false;
    }

    sLog.outDebug( LANG_CHANGE_32BIT , Opcode, Value);

    int CurrentValue = (int)m_session->GetPlayer( )->GetUInt32Value( Opcode );

    CurrentValue += Value;
    m_session->GetPlayer( )->SetUInt32Value( Opcode , (uint32)CurrentValue );

    PSendSysMessage(LANG_CHANGE_32BIT_FIELD, Opcode,CurrentValue);

    return true;
}

bool ChatHandler::HandleAddTeleCommand(const char * args)
{
    if(!*args)
        return false;
    QueryResult *result;
    Player *player=m_session->GetPlayer();
    if (!player) return false;

    std::string name = args;
    sDatabase.escape_string(name);
    result = sDatabase.PQuery("SELECT `id` FROM `game_tele` WHERE `name` = '%s'",name.c_str());
    if (result)
    {
        SendSysMessage(LANG_COMMAND_TP_ALREADYEXIST);
        delete result;
        return true;
    }

    float x = player->GetPositionX();
    float y = player->GetPositionY();
    float z = player->GetPositionZ();
    float ort = player->GetOrientation();
    int mapid = player->GetMapId();

    if(sDatabase.PExecuteLog("INSERT INTO `game_tele` (`position_x`,`position_y`,`position_z`,`orientation`,`map`,`name`) VALUES (%f,%f,%f,%f,%d,'%s')",x,y,z,ort,mapid,name.c_str()))
    {
        SendSysMessage(LANG_COMMAND_TP_ADDED);
    }
    else
        SendSysMessage(LANG_COMMAND_TP_ADDEDERR);

    return true;
}

bool ChatHandler::HandleDelTeleCommand(const char * args)
{
    if(!*args)
        return false;

    std::string name = args;
    sDatabase.escape_string(name);

    QueryResult *result=sDatabase.PQuery("SELECT `id` FROM `game_tele` WHERE `name` = '%s'",name.c_str());
    if (!result)
    {
        SendSysMessage(LANG_COMMAND_TELE_NOTFOUND);
        return true;
    }
    delete result;

    if(sDatabase.PExecuteLog("DELETE FROM `game_tele` WHERE `name` = '%s'",name.c_str()))
    {
        SendSysMessage(LANG_COMMAND_TP_DELETED);
    }
    else
        SendSysMessage(LANG_COMMAND_TP_DELETEERR);
    return true;
}

bool ChatHandler::HandleListAurasCommand (const char * args)
{
    Unit *unit = getSelectedUnit();
    if(!unit)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        return true;
    }

    Unit::AuraMap& uAuras = unit->GetAuras();
    PSendSysMessage(LANG_COMMAND_TARGET_LISTAURAS, uAuras.size());
    for (Unit::AuraMap::iterator itr = uAuras.begin(); itr != uAuras.end(); ++itr)
    {
        PSendSysMessage(LANG_COMMAND_TARGET_AURADETAIL, itr->second->GetId(), itr->second->GetEffIndex(), itr->second->GetModifier()->m_auraname, itr->second->GetAuraDuration(), itr->second->GetSpellProto()->SpellName[0]);
    }
    for (int i = 0; i < TOTAL_AURAS; i++)
    {
        Unit::AuraList const& uAuraList = unit->GetAurasByType(i);
        if (!uAuraList.size()) continue;
        PSendSysMessage(LANG_COMMAND_TARGET_LISTAURATYPE, uAuraList.size(), i);
        for (Unit::AuraList::const_iterator itr = uAuraList.begin(); itr != uAuraList.end(); ++itr)
        {
            PSendSysMessage(LANG_COMMAND_TARGET_AURASIMPLE, (*itr)->GetId(), (*itr)->GetEffIndex(), (*itr)->GetSpellProto()->SpellName[0]);
        }
    }
    return true;
}

bool ChatHandler::HandleResetCommand (const char * args)
{
    if(!*args)
        return false;

    char* arg = strtok((char*)args, " ");
    std::string argstr = arg;
    char* pName = strtok(NULL, "");
    Player *player = NULL;
    if (pName)
    {
        std::string name = pName;
        normalizePlayerName(name);
        uint64 guid = objmgr.GetPlayerGUIDByName(name.c_str());
        player = objmgr.GetPlayer(guid);
    }
    else
        player = getSelectedPlayer();

    if(!player)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }

    if (argstr == "stats" || argstr == "level")
    {
        PlayerInfo const *info = objmgr.GetPlayerInfo(player->getRace(), player->getClass());
        if(!info) return false;

        ChrClassesEntry const* cEntry = sChrClassesStore.LookupEntry(player->getClass());
        if(!cEntry)
        {
            sLog.outError("Class %u not found in DB� (Wrong DBC files?)",player->getClass());
            return true;
        }

        uint8 powertype = cEntry->powerType;

        uint32 unitfield;
        if(powertype == POWER_RAGE)
            unitfield = 0x1100EE00;
        else if(powertype == POWER_ENERGY)
            unitfield = 0x00000000;
        else if(powertype == POWER_MANA)
            unitfield = 0x0000EE00;
        else
        {
            sLog.outError("Invalid default powertype %u for player (class %u)",powertype,player->getClass());
            return true;
        }

        // reset m_form if no aura
        if(!player->HasAuraType(SPELL_AURA_MOD_SHAPESHIFT))
            player->m_form = 0;

        player->SetFloatValue(UNIT_FIELD_BOUNDINGRADIUS, 0.388999998569489f );
        player->SetFloatValue(UNIT_FIELD_COMBATREACH, 1.5f   );

        player->setFactionForRace(player->getRace());

        player->SetUInt32Value(UNIT_FIELD_BYTES_0, ( ( player->getRace() ) | ( player->getClass() << 8 ) | ( player->getGender() << 16 ) | ( powertype << 24 ) ) );

        // reset only if player not in some form;
        if(!player->m_form)
        {
            switch(player->getGender())
            {
                case GENDER_FEMALE:
                    player->SetUInt32Value(UNIT_FIELD_DISPLAYID, info->displayId_f);
                    player->SetUInt32Value(UNIT_FIELD_NATIVEDISPLAYID, info->displayId_f);
                    break;
                case GENDER_MALE:
                    player->SetUInt32Value(UNIT_FIELD_DISPLAYID, info->displayId_m);
                    player->SetUInt32Value(UNIT_FIELD_NATIVEDISPLAYID, info->displayId_m);
                    break;
                default:
                    break;
            }
        }

        // set UNIT_FIELD_BYTES_1 to init state but preserve m_form value
        player->SetUInt32Value(UNIT_FIELD_BYTES_1, player->m_form<<16 | unitfield );

        player->SetUInt32Value(UNIT_FIELD_BYTES_2, 0x2800 ); // 0x2800, 0x2801 2.0.8...
        player->SetUInt32Value(UNIT_FIELD_FLAGS , UNIT_FLAG_UNKNOWN1 );

                                                            //-1 is default value
        player->SetUInt32Value(PLAYER_FIELD_WATCHED_FACTION_INDEX, uint32(-1));

        //player->SetUInt32Value(PLAYER_FIELD_BYTES, 0xEEE00000 );

        if(argstr == "level")
        {
            player->SetLevel(1);
            player->InitStatsForLevel(true);
            player->InitTalentForLevel();
            player->SetUInt32Value(PLAYER_XP,0);

            // reset level to summoned pet
            Pet* pet = player->GetPet();
            if(pet && pet->getPetType()==SUMMON_PET)
                pet->InitStatsForLevel(1);
        }
        else
        {
            player->InitStatsForLevel(true);
            player->InitTalentForLevel();
        }
    }
    else if (argstr == "talents")
        player->resetTalents(true);
    else if (argstr == "spells")
    {
        // make full copy of map (spells removed and marked as deleted at another spell remove
        // and we can't use original map for safe iterative with visit each spell at loop end
        PlayerSpellMap smap = player->GetSpellMap();

        for(PlayerSpellMap::const_iterator iter = smap.begin();iter != smap.end(); ++iter)
            player->removeSpell(iter->first);               // only iter->first can be accessed, object by iter->second can be deleted already

        PlayerInfo const *info = objmgr.GetPlayerInfo(player->getRace(),player->getClass());
        std::list<CreateSpellPair>::const_iterator spell_itr;
        for (spell_itr = info->spell.begin(); spell_itr!=info->spell.end(); spell_itr++)
        {
            uint16 tspell = spell_itr->first;
            if (tspell)
            {
                sLog.outDebug("PLAYER: Adding initial spell, id = %u",tspell);
                player->learnSpell(tspell);
            }
        }
    }
    else
        return false;

    return true;
}

bool ChatHandler::HandleShutDownCommand(const char* args)
{
    if(!*args)
        return false;

    if(std::string(args)=="cancel")
    {
        sWorld.ShutdownCancel();
    }
    else
    {
        int32 time = atoi(args);

        ///- Prevent interpret wrong arg value as 0 secs shutdown time
        if(time == 0 && (args[0]!='0' || args[1]!='\0') || time < 0)
            return false;

        sWorld.ShutdownServ(time);
    }
    return true;
}

bool ChatHandler::HandleIdleShutDownCommand(const char* args)
{
    if(!*args)
        return false;

    if(std::string(args)=="cancel")
    {
        sWorld.ShutdownCancel();
    }
    else
    {
        int32 time = atoi(args);

        ///- Prevent interpret wrong arg value as 0 secs shutdown time
        if(time == 0 && (args[0]!='0' || args[1]!='\0') || time < 0)
            return false;

        sWorld.ShutdownServ(time,true);
    }
    return true;
}

bool ChatHandler::HandleAddQuest(const char* args)
{
    Player* player = getSelectedPlayer();
    if(!player)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }

    // .addquest #entry'
    // number or [name] Shift-click form |color|Hquest:quest_id|h[name]|h|r
    char* cId = extractKeyFromLink((char*)args,"Hquest");
    if(!cId)
        return false;

    uint32 entry = atol(cId);

    ObjectMgr::QuestMap::iterator qIter = objmgr.QuestTemplates.find(entry);

    if(qIter == objmgr.QuestTemplates.end())
    {
        PSendSysMessage(LANG_COMMAND_QUEST_NOTFOUND,entry);
        return true;
    }

    // check item starting quest (it can work incorrectly if added without item in inventory)
    QueryResult *result = sDatabase.PQuery("SELECT `entry` FROM `item_template` WHERE `startquest` = '%u' LIMIT 1",entry);
    if(result)
    {
        Field* fields = result->Fetch();
        uint32 item_id = fields[0].GetUInt32();
        delete result;

        PSendSysMessage(LANG_COMMAND_QUEST_STARTFROMITEM, entry,item_id);
        return true;
    }

    // ok, normal (creature/GO starting) quest
    Quest* pQuest = qIter->second;
    if( player->CanAddQuest( pQuest, true ) )
    {
        player->AddQuest( pQuest, NULL );

        if ( player->CanCompleteQuest( entry ) )
            player->CompleteQuest( entry );
    }

    return true;
}

bool ChatHandler::HandleRemoveQuest(const char* args)
{
    Player* player = getSelectedPlayer();
    if(!player)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }

    // .removequest #entry'
    // number or [name] Shift-click form |color|Hquest:quest_id|h[name]|h|r
    char* cId = extractKeyFromLink((char*)args,"Hquest");
    if(!cId)
        return false;

    uint32 entry = atol(cId);

    ObjectMgr::QuestMap::iterator qIter = objmgr.QuestTemplates.find(entry);

    if(qIter == objmgr.QuestTemplates.end())
    {
        PSendSysMessage(LANG_COMMAND_QUEST_NOTFOUND, entry);
        return true;
    }

    // remove all quest entries for 'entry' from quest log
    for(uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot )
    {
        uint32 quest = player->GetUInt32Value(PLAYER_QUEST_LOG_1_1 + 3*slot + 0);
        if(quest==entry)
        {
            player->SetUInt32Value(PLAYER_QUEST_LOG_1_1 + 3*slot + 0, 0);
            player->SetUInt32Value(PLAYER_QUEST_LOG_1_1 + 3*slot + 1, 0);
            player->SetUInt32Value(PLAYER_QUEST_LOG_1_1 + 3*slot + 2, 0);

            // we ignore unequippable quest items in this case, its' still be equipped
            player->TakeQuestSourceItem( quest, false );
        }
    }

    // set quest status to not started (will updated in DB at next save)
    player->SetQuestStatus( entry, QUEST_STATUS_NONE);

    // reset rewarded for restart repeatable quest
    player->getQuestStatusMap()[entry].m_rewarded = false;

    SendSysMessage(LANG_COMMAND_QUEST_REMOVED);
    return true;
}

bool ChatHandler::HandleBanCommand(const char* args)
{
    if(!args)
        return false;

    char* type = strtok((char*)args, " ");

    if(!type)
        return false;
    char* nameOrIP = strtok(NULL, " ");

    if(!nameOrIP)
        return false;

    char* duration = strtok(NULL," ");
    char* reason;

    if(!duration || !atoi(duration))
        return false;
    else
        reason = strtok(NULL,"");
    if(!reason)
        return false;

    switch(sWorld.BanAccount(type, nameOrIP, duration, reason,m_session->GetPlayerName()))
    {
    case BAN_SUCCESS:
        if(atoi(duration)>0)
            PSendSysMessage(LANG_BAN_YOUBANNED,nameOrIP,secsToTimeString(TimeStringToSecs(duration),true).c_str(),reason);
        else
            PSendSysMessage(LANG_BAN_YOUPERMBANNED,nameOrIP,reason);
        break;
    case BAN_SYNTAX_ERROR:
        return false;
    case BAN_NOTFOUND:
        PSendSysMessage(LANG_BAN_NOTFOUND,type,nameOrIP);
        break;
    }

    return true;
}

bool ChatHandler::HandleUnBanCommand(const char* args)
{
    if(!args)
        return false;
    char* type = strtok((char*)args, " ");
    if(!type)
        return false;
    char* nameOrIP = strtok(NULL, " ");

    if(!nameOrIP)
        return false;

    if(sWorld.RemoveBanAccount(type,nameOrIP))
        PSendSysMessage(LANG_UNBAN_UNBANNED,nameOrIP);
    else
        PSendSysMessage(LANG_UNBAN_ERROR,nameOrIP);

    return true;
}

bool ChatHandler::HandleBanInfoCommand(const char* args)
{
    if(!args)
        return false;
    char* cType = strtok((char*)args, " ");
    char* cnameOrIP = strtok(NULL, "");
    if(!cType || !cnameOrIP)
        return false;
    std::string nameOrIP = cnameOrIP;
    std::string type = cType;
    if (!IsIPAddress(cnameOrIP) && type=="ip")
        return false;
    loginDatabase.escape_string(nameOrIP);
    Field *fields;
    if(type!="ip")
    {
        //look the accountid up
        uint32 accountid;
        std::string accountname;
        if(type == "account")
        {
            QueryResult *result = loginDatabase.PQuery("SELECT `id`, `username` FROM `account` WHERE `username` = '%s' ",nameOrIP.c_str());
            if (!result)
            {
                PSendSysMessage(LANG_BANINFO_NOACCOUNT);
                return true;
            }
            fields = result->Fetch();
            accountid = fields[0].GetUInt32();
            accountname = fields[1].GetCppString();
            delete result;
        }
        else if(type == "character")
        {
            normalizePlayerName(nameOrIP);
            QueryResult *result = sDatabase.PQuery("SELECT account FROM `character` WHERE name = '%s')", nameOrIP.c_str());
            if (!result)
            {
                PSendSysMessage(LANG_BANINFO_NOCHARACTER);
                return true;
            }
            fields = result->Fetch();
            accountid = fields[0].GetUInt32();
            delete result;
            result = loginDatabase.PQuery("SELECT `username` FROM `account` WHERE `id` = '%u'", accountid);
            if (!result)
            {
                PSendSysMessage(LANG_BANINFO_NOCHARACTER);
                return true;
            }
            fields = result->Fetch();
            accountname = fields[0].GetCppString();
            delete result;
        }
        else
            return false;

        QueryResult *result = loginDatabase.PQuery("SELECT FROM_UNIXTIME(`bandate`), `unbandate`-`bandate`, `active`, `unbandate`-UNIX_TIMESTAMP(),`banreason`,`bannedby` FROM `account_banned` WHERE `id` = '%u' ORDER BY `bandate` ASC",accountid);
        if(!result)
        {
            PSendSysMessage(LANG_BANINFO_NOACCOUNTBAN, accountname.c_str());
            return true;
        }
        PSendSysMessage(LANG_BANINFO_BANHISTORY,accountname.c_str());
        do{
            fields = result->Fetch();
            bool active = false;
            if(fields[2].GetBool() && (fields[1].GetUInt64() == (uint64)0 ||fields[3].GetUInt64() >= (uint64)0) )
                active = true;
            bool permanent = (fields[1].GetUInt64() == (uint64)0);
            std::string bantime = permanent?LANG_BANINFO_INFINITE:secsToTimeString(fields[1].GetUInt64(), true);
            PSendSysMessage(LANG_BANINFO_HISTORYENTRY,
                    fields[0].GetString(), bantime.c_str(), active ? LANG_BANINFO_YES:LANG_BANINFO_NO, fields[4].GetString(), fields[5].GetString());
        }while (result->NextRow());

        delete result;
        return true;
    }
    else
    {
        QueryResult *result = loginDatabase.PQuery("SELECT `ip`, FROM_UNIXTIME(`bandate`), FROM_UNIXTIME(`unbandate`), `unbandate`-UNIX_TIMESTAMP(), `banreason`,`bannedby`,`unbandate`-`bandate` FROM `ip_banned` WHERE `ip` = '%s'",nameOrIP.c_str());
        if(!result)
        {
            PSendSysMessage(LANG_BANINFO_NOIP);
            return true;
        }
        fields = result->Fetch();
        bool permanent = (fields[6].GetUInt64()==(uint64)0);
        PSendSysMessage(LANG_BANINFO_IPENTRY,
                fields[0].GetString(), fields[1].GetString(), permanent ? LANG_BANINFO_NEVER:fields[2].GetString(),
                permanent ? LANG_BANINFO_INFINITE:secsToTimeString(fields[3].GetUInt64(), true).c_str(), fields[4].GetString(), fields[5].GetString());
        delete result;
        return true;
    }
    return false;
}

bool ChatHandler::HandleBanListCommand(const char* args)
{
    loginDatabase.Execute("DELETE FROM `ip_banned` WHERE `unbandate`<=UNIX_TIMESTAMP() AND `unbandate`<>`bandate`");
    if(!*args)
        return false;
    char* cType = strtok((char*)args, " ");
    char* cFilter = strtok(NULL, "");
    if(!cType || !cFilter)
       return false;
    std::string Filter = cFilter;
    std::string Type = cType;
    loginDatabase.escape_string(Filter);


    QueryResult* result  = NULL;
    Field *fields = NULL;
    if(Type == "ip")
    {
        result = loginDatabase.PQuery("SELECT `ip` FROM `ip_banned` WHERE `ip` LIKE \"%%%s%%\"",Filter.c_str());
        if(!result)
        {
            PSendSysMessage(LANG_BANLIST_NOIP);
            return true;
        }
        PSendSysMessage(LANG_BANLIST_MATCHINGIP);
        do
        {
            fields = result->Fetch();
            PSendSysMessage("%s",fields[0].GetString());
        } while (result->NextRow());

        delete result;
        return true;
    }
    //lookup accountid
    if(Type == "account")
    {
        result = loginDatabase.PQuery("SELECT `id` FROM `account` WHERE `username` LIKE \"%%%s%%\" ",Filter.c_str());
        if (!result)
        {
            PSendSysMessage(LANG_BANLIST_NOACCOUNT);
            return true;
        }
        //do not delete result
    }
    else if(Type == "character")
    {
        result = sDatabase.PQuery("SELECT `account` FROM `character`, WHERE name LIKE \"%%%s%%\" ",Filter.c_str()); 
        if (!result)
        {
            PSendSysMessage(LANG_BANLIST_NOCHARACTER);
            return true;
        }
    }
    else
        return false;

    PSendSysMessage(LANG_BANLIST_MATCHINGACCOUNT);
    do
    {
        fields = result->Fetch();
        uint32 accountid = fields[0].GetUInt32();
        QueryResult* banresult = loginDatabase.PQuery("SELECT `account`.`username` FROM `account`,`account_banned` WHERE `account_banned`.`id`='%u' AND `account_banned`.`active` = '1' AND `account_banned`.`id`=`account`.`id`",accountid);
        if(banresult)
        {
            Field* fields2 = banresult->Fetch();
            PSendSysMessage("%s",fields2[0].GetString());
            delete banresult;
        }
    } while (result->NextRow());

    delete result;
    return true;
}

bool ChatHandler::HandleRespawnCommand(const char* args)
{
    Player* pl = m_session->GetPlayer();

    CellPair p(MaNGOS::ComputeCellPair(pl->GetPositionX(), pl->GetPositionY()));
    Cell cell = RedZone::GetZone(p);
    cell.data.Part.reserved = ALL_DISTRICT;
    cell.SetNoCreate();

    MaNGOS::RespawnDo u_do;
    MaNGOS::WorldObjectWorker<MaNGOS::RespawnDo> worker(u_do);

    TypeContainerVisitor<MaNGOS::WorldObjectWorker<MaNGOS::RespawnDo>, GridTypeMapContainer > obj_worker(worker);
    CellLock<GridReadGuard> cell_lock(cell, p);
    cell_lock->Visit(cell_lock, obj_worker, *MapManager::Instance().GetMap(pl->GetMapId(), pl));

    return true;
}

bool ChatHandler::HandleFlyModeCommand(const char* args)
{
    if(!args)
        return false;

    Unit *unit = getSelectedUnit();
    if (!unit || (unit->GetTypeId() != TYPEID_PLAYER))
        unit = m_session->GetPlayer();

    WorldPacket data(12);
    if (strncmp(args, "on", 3) == 0)
        data.SetOpcode(SMSG_FLY_MODE_START);
    else if (strncmp(args, "off", 4) == 0)
        data.SetOpcode(SMSG_FLY_MODE_STOP);
    else
    {
        PSendSysMessage(LANG_COMMAND_FLYMODE_WRONGARG);
        return false;
    }
    data.append(unit->GetPackGUID());
    data << uint32(0);                                      // unk
    unit->SendMessageToSet(&data, true);
    PSendSysMessage(LANG_COMMAND_FLYMODE_STATUS, unit->GetName(), args);
    return true;
}

bool ChatHandler::HandleSendOpcodeCommand(const char* args)
{
    if(!args)
        return false;

    Unit *unit = getSelectedUnit();
    if (!unit || (unit->GetTypeId() != TYPEID_PLAYER))
        unit = m_session->GetPlayer();

    uint16 opcode = atoi(args);
    /*WorldPacket data(opcode, 20);
    data.append(unit->GetPackGUID());
    data << urand(0, 1024);
    data << urand(0, 1024);
    data << urand(0, 1024);*/
    uint32 random = urand(1, 3);
    uint8 temp = 3;
    if(temp == 1)
    {
        WorldPacket data(SMSG_AI_REACTION, 8+4);
        data << unit->GetGUID();
        data << random;
        unit->SendMessageToSet(&data, true);
    }
    if(temp == 2)
    {
        WorldPacket data(SMSG_AI_UNKNOWN, 8+4);
        data << unit->GetGUID();
        data << random;
        unit->SendMessageToSet(&data, true);
    }
    if(temp == 3)
    {
        WorldPacket data;
        ((Player*)unit)->GetSession()->BuildArenaTeamEventPacket(&data, opcode, random, "str1", "str2", "str3");
        ((Player*)unit)->GetSession()->SendPacket(&data);
    }
    PSendSysMessage(LANG_COMMAND_OPCODESENT, opcode, unit->GetName());
    return true;
}

bool ChatHandler::HandleSellErrorCommand(const char* args)
{
    if(!args)
        return false;

    uint32 param = 0;
    uint8 msg = atoi(args);
    WorldPacket data( SMSG_SELL_ITEM, (8+8+4+1) );
    data << uint64(0);
    data << uint64(25);
    if( param > 0 )
        data << param;
    data << msg;
    m_session->SendPacket(&data);
    return true;
}

bool ChatHandler::HandleBuyErrorCommand(const char* args)
{
    if(!args)
        return false;

    uint8 msg = atoi(args);
    WorldPacket data( SMSG_BUY_FAILED, (8+8+4+1) );
    data << m_session->GetPlayer()->GetGUID();
    data << uint32(0);
    data << msg;
    m_session->SendPacket(&data);
    return true;
}

bool ChatHandler::HandleUpdateWorldStateCommand(const char* args)
{
    char* w = strtok((char*)args, " ");
    char* s = strtok(NULL, " ");

    if (!w || !s)
        return false;

    uint32 world = (uint32)atoi(w);
    uint32 state = (uint32)atoi(s);
    m_session->GetPlayer()->SendUpdateWorldState(world, state);
    return true;
}

bool ChatHandler::HandlePlaySound2Command(const char* args)
{
    if(!args)
        return false;

    uint32 soundid = atoi(args);
    m_session->GetPlayer()->PlaySound(soundid, false);
    return true;
}

bool ChatHandler::HandleSendChannelNotifyCommand(const char* args)
{
    if(!args)
        return false;

    const char *name = "test";
    uint8 code = atoi(args);

    WorldPacket data(SMSG_CHANNEL_NOTIFY, (1+10));
    data << code;                                           // notify type
    data << name;                                           // channel name
    data << uint32(0);
    data << uint32(0);
    m_session->SendPacket(&data);
    return true;
}

bool ChatHandler::HandleSendChatMsgCommand(const char* args)
{
    if(!args)
        return false;

    const char *msg = "testtest";
    uint8 type = atoi(args);

    WorldPacket data(SMSG_MESSAGECHAT, 100);
    data << type;                                           // message type
    data << uint32(0);                                      // lang
    data << m_session->GetPlayer()->GetGUID();              // guid
    data << uint32(0);
    data << uint64(0);
    data << uint32(9);                                      // msg len
    data << msg;                                            // msg
    data << uint8(0);                                       // chat tag
    m_session->SendPacket(&data);
    return true;
}

bool ChatHandler::HandleLoadPDumpCommand(const char *args)
{
    if(!args)
        return false;

    char * file = strtok((char*)args, " "); if(!file) return false;
    char * acc = strtok(NULL, " "); if(!acc) return false;
    char * name = strtok(NULL, " ");
    char * guid = name ? strtok(NULL, " ") : NULL;

    if(objmgr.LoadPlayerDump(file, atoi(acc), name ? name : "", guid ? atoi(guid) : 0))    
        PSendSysMessage(LANG_COMMAND_IMPORT_SUCCESS);
    else
        PSendSysMessage(LANG_COMMAND_IMPORT_FAILED);

    return true;
}

bool ChatHandler::HandleWritePDumpCommand(const char *args)
{
    if(!args)
        return false;

    char* file = strtok((char*)args, " ");
    char* guid = strtok(NULL, " ");

    if(!file || !guid) return false;

    if (objmgr.WritePlayerDump(file, atoi(guid)))
        PSendSysMessage(LANG_COMMAND_EXPORT_SUCCESS);
    else
        PSendSysMessage(LANG_COMMAND_EXPORT_FAILED);

    return true;
}
