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
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "UpdateMask.h"
#include "Chat.h"
#include "MapManager.h"
#include "Policies/SingletonImp.h"

LanguageDesc lang_description[LANGUAGES_COUNT] =
{
    { LANG_UNIVERSAL2,      0, 0                       },
    { LANG_GLOBAL,          0, 0                       },
    { LANG_UNIVERSAL,       0, 0                       },
    { LANG_ORCISH,        669, SKILL_LANG_ORCISH       },
    { LANG_DARNASSIAN,    671, SKILL_LANG_DARNASSIAN   },
    { LANG_TAURAHE,       670, SKILL_LANG_TAURAHE      },
    { LANG_DWARVISH,      672, SKILL_LANG_DWARVEN      },
    { LANG_COMMON,        668, SKILL_LANG_COMMON       },
    { LANG_DEMONIC,       815, SKILL_LANG_DEMON_TONGUE },
    { LANG_TITAN,         816, SKILL_LANG_TITAN        },
    { LANG_THALASSIAN,    813, SKILL_LANG_THALASSIAN   },
    { LANG_DRACONIC,      814, SKILL_LANG_DRACONIC     },
    { LANG_KALIMAG,       817, SKILL_LANG_OLD_TONGUE   },
    { LANG_GNOMISH,      7340, SKILL_LANG_GNOMISH      },
    { LANG_TROLL,        7341, SKILL_LANG_TROLL        },
    { LANG_GUTTERSPEAK, 17737, SKILL_LANG_GUTTERSPEAK  },
    { LANG_DRAENEI,     29932, SKILL_LANG_DRAENEI      }
};

LanguageDesc const* GetLanguageDescByID(uint32 lang)
{
    for(int i = 0; i < LANGUAGES_COUNT; ++i)
    {
        if(uint32(lang_description[i].lang_id) == lang)
            return &lang_description[i];
    }

    return NULL;
}

LanguageDesc const* GetLanguageDescBySpell(uint32 spell_id)
{
    for(int i = 0; i < LANGUAGES_COUNT; ++i)
    {
        if(lang_description[i].spell_id == spell_id)
            return &lang_description[i];
    }

    return NULL;
}

LanguageDesc const* GetLanguageDescBySkill(uint32 skill_id)
{
    for(int i = 0; i < LANGUAGES_COUNT; ++i)
    {
        if(lang_description[i].skill_id == skill_id)
            return &lang_description[i];
    }

    return NULL;
}

INSTANTIATE_SINGLETON_1( ChatHandler );

ChatHandler::ChatHandler()
{

}

ChatHandler::~ChatHandler()
{

}

ChatCommand * ChatHandler::getCommandTable()
{

    static bool first_call = true;

    static ChatCommand modifyCommandTable[] =
    {
        { "hp",          SEC_MODERATOR,     &ChatHandler::HandleModifyHPCommand,         "",   NULL },
        { "mana",        SEC_MODERATOR,     &ChatHandler::HandleModifyManaCommand,       "",   NULL },
        { "rage",        SEC_MODERATOR,     &ChatHandler::HandleModifyRageCommand,       "",   NULL },
        { "energy",      SEC_MODERATOR,     &ChatHandler::HandleModifyEnergyCommand,     "",   NULL },
        { "money",       SEC_MODERATOR,     &ChatHandler::HandleModifyMoneyCommand,      "",   NULL },
        { "speed",       SEC_MODERATOR,     &ChatHandler::HandleModifySpeedCommand,      "",   NULL },
        { "swim",        SEC_MODERATOR,     &ChatHandler::HandleModifySwimCommand,       "",   NULL },
        { "scale",       SEC_MODERATOR,     &ChatHandler::HandleModifyScaleCommand,      "",   NULL },
        { "bit",         SEC_MODERATOR,     &ChatHandler::HandleModifyBitCommand,        "",   NULL },
        { "bwalk",       SEC_MODERATOR,     &ChatHandler::HandleModifyBWalkCommand,      "",   NULL },
        { "fly",         SEC_MODERATOR,     &ChatHandler::HandleModifyFlyCommand,        "",   NULL },
        { "aspeed",      SEC_MODERATOR,     &ChatHandler::HandleModifyASpeedCommand,     "",   NULL },
        { "faction",     SEC_MODERATOR,     &ChatHandler::HandleModifyFactionCommand,    "",   NULL },
        { "spell",       SEC_MODERATOR,     &ChatHandler::HandleModifySpellCommand,      "",   NULL },
        { "tp",          SEC_MODERATOR,     &ChatHandler::HandleModifyTalentCommand,     "",   NULL },
        { "titles",      SEC_MODERATOR,     &ChatHandler::HandleModifyKnownTitlesCommand, "",  NULL },
        { "mount",       SEC_MODERATOR,     &ChatHandler::HandleModifyMountCommand,       "",  NULL },
          
        { NULL,          0, NULL,                                        "",   NULL }
    };

    static ChatCommand wpCommandTable[] =
    {
        { "show",        SEC_GAMEMASTER,   &ChatHandler::HandleWpShowCommand,           "",   NULL },
        { "add",         SEC_GAMEMASTER,   &ChatHandler::HandleWpAddCommand,            "",   NULL },
        { "modify",      SEC_GAMEMASTER,   &ChatHandler::HandleWpModifyCommand,         "",   NULL },

        { NULL,          0, NULL,                                       "",   NULL }
    };

    static ChatCommand debugCommandTable[] =
    {
        { "inarc",       SEC_ADMINISTRATOR, &ChatHandler::HandleDebugInArcCommand,         "",   NULL },
        { "spellfail",   SEC_ADMINISTRATOR, &ChatHandler::HandleDebugSpellFailCommand,     "",   NULL },
        { "setpoi",      SEC_ADMINISTRATOR, &ChatHandler::HandleSetPoiCommand,             "",   NULL },
        { "qpartymsg",   SEC_ADMINISTRATOR, &ChatHandler::HandleSendQuestPartyMsgCommand,  "",   NULL },
        { "qinvalidmsg", SEC_ADMINISTRATOR, &ChatHandler::HandleSendQuestInvalidMsgCommand,"",   NULL },
        { "itemmsg",     SEC_ADMINISTRATOR, &ChatHandler::HandleSendItemErrorMsg,          "",   NULL },
        { "getitemstate",SEC_ADMINISTRATOR, &ChatHandler::HandleGetItemState,              "",   NULL },
        { NULL,          0, NULL,                                          "",   NULL }
    };

    static ChatCommand commandTable[] =
    {
        { "acct",        SEC_PLAYER,        &ChatHandler::HandleAcctCommand,             "",   NULL },
        { "addmove",     SEC_GAMEMASTER,    &ChatHandler::HandleAddMoveCommand,          "",   NULL },
        { "setmovetype", SEC_GAMEMASTER,    &ChatHandler::HandleSetMoveTypeCommand,      "",   NULL },
        { "anim",        SEC_ADMINISTRATOR, &ChatHandler::HandleAnimCommand,             "",   NULL },
        { "announce",    SEC_MODERATOR,     &ChatHandler::HandleAnnounceCommand,         "",   NULL },
        { "notify",      SEC_MODERATOR,     &ChatHandler::HandleNotifyCommand,           "",   NULL },
        { "go",          SEC_ADMINISTRATOR, &ChatHandler::HandleGoCommand,               "",   NULL },
        { "goname",      SEC_MODERATOR,     &ChatHandler::HandleGonameCommand,           "",   NULL },
        { "namego",      SEC_MODERATOR,     &ChatHandler::HandleNamegoCommand,           "",   NULL },
        { "aura",        SEC_ADMINISTRATOR, &ChatHandler::HandleAuraCommand,             "",   NULL },
        { "unaura",      SEC_ADMINISTRATOR, &ChatHandler::HandleUnAuraCommand,           "",   NULL },
        { "changelevel", SEC_GAMEMASTER,    &ChatHandler::HandleChangeLevelCommand,      "",   NULL },
        { "commands",    SEC_PLAYER,        &ChatHandler::HandleCommandsCommand,         "",   NULL },
        { "delete",      SEC_GAMEMASTER,    &ChatHandler::HandleDeleteCommand,           "",   NULL },
        { "demorph",     SEC_GAMEMASTER,    &ChatHandler::HandleDeMorphCommand,          "",   NULL },
        { "die",         SEC_ADMINISTRATOR, &ChatHandler::HandleDieCommand,              "",   NULL },
        { "revive",      SEC_ADMINISTRATOR, &ChatHandler::HandleReviveCommand,           "",   NULL },
        { "dismount",    SEC_PLAYER,        &ChatHandler::HandleDismountCommand,         "",   NULL },
        { "displayid",   SEC_GAMEMASTER,    &ChatHandler::HandleDisplayIdCommand,        "",   NULL },
        { "factionid",   SEC_GAMEMASTER,    &ChatHandler::HandleFactionIdCommand,        "",   NULL },
        { "gmlist",      SEC_PLAYER,        &ChatHandler::HandleGMListCommand,           "",   NULL },
        { "gmoff",       SEC_MODERATOR,     &ChatHandler::HandleGMOffCommand,            "",   NULL },
        { "gmon",        SEC_MODERATOR,     &ChatHandler::HandleGMOnCommand,             "",   NULL },
        { "gps",         SEC_MODERATOR,     &ChatHandler::HandleGPSCommand,              "",   NULL },
        { "guid",        SEC_GAMEMASTER,    &ChatHandler::HandleGUIDCommand,             "",   NULL },
        { "help",        SEC_PLAYER,        &ChatHandler::HandleHelpCommand,             "",   NULL },
        { "info",        SEC_PLAYER,        &ChatHandler::HandleInfoCommand,             "",   NULL },
        { "npcinfo",     SEC_ADMINISTRATOR, &ChatHandler::HandleNpcInfoCommand,          "",   NULL },
        { "npcinfoset",  SEC_ADMINISTRATOR, &ChatHandler::HandleNpcInfoSetCommand,       "",   NULL },
        { "addvendoritem",SEC_GAMEMASTER,   &ChatHandler::HandleAddVendorItemCommand,    "",   NULL },
        { "delvendoritem",SEC_GAMEMASTER,   &ChatHandler::HandleDelVendorItemCommand,    "",   NULL },
        { "itemmove",    SEC_GAMEMASTER,    &ChatHandler::HandleItemMoveCommand,         "",   NULL },
        { "kick",        SEC_GAMEMASTER,    &ChatHandler::HandleKickPlayerCommand,       "",   NULL },
        { "learn",       SEC_ADMINISTRATOR, &ChatHandler::HandleLearnCommand,            "",   NULL },
        { "cooldown",    SEC_ADMINISTRATOR, &ChatHandler::HandleCooldownCommand,         "",   NULL },
        { "unlearn",     SEC_ADMINISTRATOR, &ChatHandler::HandleUnLearnCommand,          "",   NULL },
        { "modify",      SEC_MODERATOR,     NULL,                                        "",   modifyCommandTable },
        { "debug",       SEC_MODERATOR,     NULL,                                        "",   debugCommandTable },
        { "morph",       SEC_ADMINISTRATOR, &ChatHandler::HandleMorphCommand,            "",   NULL },
        { "name",        SEC_GAMEMASTER,    &ChatHandler::HandleNameCommand,             "",   NULL },
        { "subname",     SEC_GAMEMASTER,    &ChatHandler::HandleSubNameCommand,          "",   NULL },
        { "npcflag",     SEC_GAMEMASTER,    &ChatHandler::HandleNPCFlagCommand,          "",   NULL },
        { "distance",    SEC_ADMINISTRATOR, &ChatHandler::HandleGetDistanceCommand,      "",   NULL },
        { "object",      SEC_ADMINISTRATOR, &ChatHandler::HandleObjectCommand,           "",   NULL },
        { "gameobject",  SEC_ADMINISTRATOR, &ChatHandler::HandleGameObjectCommand,       "",   NULL },
        { "addgo",       SEC_ADMINISTRATOR, &ChatHandler::HandleGameObjectCommand,       "",   NULL },
        { "prog",        SEC_GAMEMASTER,    &ChatHandler::HandleProgCommand,             "",   NULL },
        { "recall",      SEC_MODERATOR,     &ChatHandler::HandleRecallCommand,           "",   NULL },
        { "run",         SEC_GAMEMASTER,    &ChatHandler::HandleRunCommand,              "",   NULL },
        { "save",        SEC_PLAYER,        &ChatHandler::HandleSaveCommand,             "",   NULL },
        { "saveall",     SEC_MODERATOR,     &ChatHandler::HandleSaveAllCommand,          "",   NULL },
        { "security",    SEC_ADMINISTRATOR, &ChatHandler::HandleSecurityCommand,         "",   NULL },
        { "ban",         SEC_ADMINISTRATOR, &ChatHandler::HandleBanCommand,              "",   NULL },
        { "unban",       SEC_ADMINISTRATOR, &ChatHandler::HandleUnBanCommand,            "",   NULL },
        { "baninfo",     SEC_ADMINISTRATOR, &ChatHandler::HandleBanInfoCommand,          "",   NULL },
        { "banlist",     SEC_ADMINISTRATOR, &ChatHandler::HandleBanListCommand,          "",   NULL },
        { "AddSpawn",    SEC_GAMEMASTER,    &ChatHandler::HandleSpawnCommand,            "",   NULL },
        { "standstate",  SEC_ADMINISTRATOR, &ChatHandler::HandleStandStateCommand,       "",   NULL },
        { "start",       SEC_PLAYER,        &ChatHandler::HandleStartCommand,            "",   NULL },
        { "taxicheat",   SEC_MODERATOR,     &ChatHandler::HandleTaxiCheatCommand,        "",   NULL },
        { "goxy",        SEC_ADMINISTRATOR, &ChatHandler::HandleGoXYCommand     ,        "",   NULL },
        { "worldport",   SEC_ADMINISTRATOR, &ChatHandler::HandleWorldPortCommand,        "",   NULL },
        { "addweapon",   SEC_ADMINISTRATOR, &ChatHandler::HandleAddWeaponCommand,        "",   NULL },
        { "allowmove",   SEC_ADMINISTRATOR, &ChatHandler::HandleAllowMovementCommand,    "",   NULL },
        { "linkgrave",   SEC_ADMINISTRATOR, &ChatHandler::HandleLinkGraveCommand,        "",   NULL },
        { "neargrave",   SEC_ADMINISTRATOR, &ChatHandler::HandleNearGraveCommand,        "",   NULL },
        { "transport",   SEC_ADMINISTRATOR, &ChatHandler::HandleSpawnTransportCommand,   "",   NULL },
        { "explorecheat",SEC_ADMINISTRATOR, &ChatHandler::HandleExploreCheatCommand,     "",   NULL },
        { "hover",       SEC_ADMINISTRATOR, &ChatHandler::HandleHoverCommand,            "",   NULL },
        { "levelup",     SEC_ADMINISTRATOR, &ChatHandler::HandleLevelUpCommand,          "",   NULL },
        { "emote",       SEC_ADMINISTRATOR, &ChatHandler::HandleEmoteCommand,            "",   NULL },
        { "showarea",    SEC_ADMINISTRATOR, &ChatHandler::HandleShowAreaCommand,         "",   NULL },
        { "hidearea",    SEC_ADMINISTRATOR, &ChatHandler::HandleHideAreaCommand,         "",   NULL },
        { "addspw",      SEC_GAMEMASTER,    &ChatHandler::HandleAddSpwCommand,           "",   NULL },
        { "spawndist",   SEC_GAMEMASTER,    &ChatHandler::HandleSpawnDistCommand,        "",   NULL },
        { "spawntime",   SEC_GAMEMASTER,    &ChatHandler::HandleSpawnTimeCommand,        "",   NULL },
        { "additem",     SEC_ADMINISTRATOR, &ChatHandler::HandleAddItemCommand,          "",   NULL },
        { "additemset",  SEC_ADMINISTRATOR, &ChatHandler::HandleAddItemSetCommand,       "",   NULL },
        { "createguild", SEC_ADMINISTRATOR, &ChatHandler::HandleCreateGuildCommand,      "",   NULL },
        { "showhonor",   SEC_PLAYER,        &ChatHandler::HandleShowHonor,               "",   NULL },
        { "update",      SEC_ADMINISTRATOR, &ChatHandler::HandleUpdate,                  "",   NULL },
        { "bank",        SEC_ADMINISTRATOR, &ChatHandler::HandleBankCommand,             "",   NULL },
        { "wchange",     SEC_ADMINISTRATOR, &ChatHandler::HandleChangeWeather,           "",   NULL },
        { "reload",      SEC_ADMINISTRATOR, &ChatHandler::HandleReloadCommand,           "",   NULL },
        { "loadscripts", SEC_ADMINISTRATOR, &ChatHandler::HandleLoadScriptsCommand,      "",   NULL },
        { "tele",        SEC_MODERATOR,     &ChatHandler::HandleTeleCommand,             "",   NULL },
        { "lookuptele",  SEC_MODERATOR,     &ChatHandler::HandleLookupTeleCommand,       "",   NULL },
        { "addtele",     SEC_ADMINISTRATOR, &ChatHandler::HandleAddTeleCommand,          "",   NULL },
        { "deltele",     SEC_ADMINISTRATOR, &ChatHandler::HandleDelTeleCommand,          "",   NULL },
        { "listauras",   SEC_ADMINISTRATOR, &ChatHandler::HandleListAurasCommand,        "",   NULL },
        { "reset",       SEC_ADMINISTRATOR, &ChatHandler::HandleResetCommand,            "",   NULL },
        { "ticket",      SEC_GAMEMASTER,    &ChatHandler::HandleTicketCommand,           "",   NULL },
        { "delticket",   SEC_GAMEMASTER,    &ChatHandler::HandleDelTicketCommand,        "",   NULL },
        { "maxskill",    SEC_ADMINISTRATOR, &ChatHandler::HandleMaxSkillCommand,         "",   NULL },
        { "setskill",    SEC_ADMINISTRATOR, &ChatHandler::HandleSetSkillCommand,         "",   NULL },
        { "whispers",    SEC_MODERATOR,     &ChatHandler::HandleWhispersCommand,         "",   NULL },
        { "say",         SEC_MODERATOR,     &ChatHandler::HandleSayCommand,              "",   NULL },
        { "yell",        SEC_MODERATOR,     &ChatHandler::HandleYellCommand,             "",   NULL },
        { "emote2",      SEC_MODERATOR,     &ChatHandler::HandleEmote2Command,           "",   NULL },
        { "whisper2",    SEC_MODERATOR,     &ChatHandler::HandleWhisperCommand,          "",   NULL },
        { "gocreature",  SEC_GAMEMASTER,    &ChatHandler::HandleGoCreatureCommand,       "",   NULL },
        { "goobject",    SEC_GAMEMASTER,    &ChatHandler::HandleGoObjectCommand,         "",   NULL },
        { "targetobject",SEC_GAMEMASTER,    &ChatHandler::HandleTargetObjectCommand,     "",   NULL },
        { "delobject",   SEC_GAMEMASTER,    &ChatHandler::HandleDelObjectCommand,        "",   NULL },
        { "turnobject",  SEC_GAMEMASTER,    &ChatHandler::HandleTurnObjectCommand,       "",   NULL },
        { "movecreature",SEC_GAMEMASTER,    &ChatHandler::HandleMoveCreatureCommand,     "",   NULL },
        { "moveobject",  SEC_GAMEMASTER,    &ChatHandler::HandleMoveObjectCommand,       "",   NULL },
        { "idleshutdown",SEC_ADMINISTRATOR, &ChatHandler::HandleIdleShutDownCommand,     "",   NULL },
        { "shutdown",    SEC_ADMINISTRATOR, &ChatHandler::HandleShutDownCommand,         "",   NULL },
        { "pinfo",       SEC_GAMEMASTER,    &ChatHandler::HandlePInfoCommand,            "",   NULL },
        { "visible",     SEC_MODERATOR,     &ChatHandler::HandleVisibleCommand,          "",   NULL },
        { "playsound",   SEC_MODERATOR,     &ChatHandler::HandlePlaySoundCommand,        "",   NULL },
        { "listcreature",SEC_ADMINISTRATOR, &ChatHandler::HandleListCreatureCommand,     "",   NULL },
        { "listitem",    SEC_ADMINISTRATOR, &ChatHandler::HandleListItemCommand,         "",   NULL },
        { "listobject",  SEC_ADMINISTRATOR, &ChatHandler::HandleListObjectCommand,       "",   NULL },
        { "lookupitem",  SEC_ADMINISTRATOR, &ChatHandler::HandleLookupItemCommand,       "",   NULL },
        { "lookupitemset",SEC_ADMINISTRATOR,&ChatHandler::HandleLookupItemSetCommand,    "",   NULL },
        { "lookupskill", SEC_ADMINISTRATOR, &ChatHandler::HandleLookupSkillCommand,      "",   NULL },
        { "lookupspell", SEC_ADMINISTRATOR, &ChatHandler::HandleLookupSpellCommand,      "",   NULL },
        { "lookupquest", SEC_ADMINISTRATOR, &ChatHandler::HandleLookupQuestCommand,      "",   NULL },
        { "lookupcreature",SEC_ADMINISTRATOR, &ChatHandler::HandleLookupCreatureCommand, "",   NULL },
        { "lookupobject",SEC_ADMINISTRATOR, &ChatHandler::HandleLookupObjectCommand,     "",   NULL },
        { "money",       SEC_MODERATOR,     &ChatHandler::HandleModifyMoneyCommand,      "",   NULL },
        { "titles",      SEC_MODERATOR,     &ChatHandler::HandleModifyKnownTitlesCommand, "",   NULL },
        { "speed",       SEC_MODERATOR,     &ChatHandler::HandleModifySpeedCommand,      "",   NULL },
        { "addquest",    SEC_ADMINISTRATOR, &ChatHandler::HandleAddQuest,                "",   NULL },
        { "removequest", SEC_ADMINISTRATOR, &ChatHandler::HandleRemoveQuest,             "",   NULL },
        { "password",    SEC_PLAYER,        &ChatHandler::HandlePasswordCommand,         "",   NULL },
        { "lockaccount", SEC_PLAYER,        &ChatHandler::HandleLockAccountCommand,      "",   NULL },
        { "respawn",     SEC_ADMINISTRATOR, &ChatHandler::HandleRespawnCommand,          "",   NULL },
        { "wp",          SEC_GAMEMASTER,   NULL,                                        "",   wpCommandTable },
        { "flymode",     SEC_ADMINISTRATOR, &ChatHandler::HandleFlyModeCommand,          "",   NULL },
        { "sendopcode",  SEC_ADMINISTRATOR, &ChatHandler::HandleSendOpcodeCommand,       "",   NULL },
        { "sellerr",     SEC_ADMINISTRATOR, &ChatHandler::HandleSellErrorCommand,        "",   NULL },
        { "buyerr",      SEC_ADMINISTRATOR, &ChatHandler::HandleBuyErrorCommand,         "",   NULL },
        { "uws",         SEC_ADMINISTRATOR, &ChatHandler::HandleUpdateWorldStateCommand, "",   NULL },
        { "ps",          SEC_ADMINISTRATOR, &ChatHandler::HandlePlaySound2Command,       "",   NULL },
        { "scn",         SEC_ADMINISTRATOR, &ChatHandler::HandleSendChannelNotifyCommand,"",   NULL },
        { "scm",         SEC_ADMINISTRATOR, &ChatHandler::HandleSendChatMsgCommand,      "",   NULL },
        { "sendmail",    SEC_MODERATOR,     &ChatHandler::HandleSendMailCommand,         "",   NULL },
        { "rename",      SEC_GAMEMASTER,    &ChatHandler::HandleRenameCommand,           "",   NULL },
        { "nametele",    SEC_MODERATOR,     &ChatHandler::HandleNameTeleCommand,         "",   NULL },
        { "loadpdump",   SEC_ADMINISTRATOR, &ChatHandler::HandleLoadPDumpCommand,        "",   NULL },
        { "writepdump",  SEC_ADMINISTRATOR, &ChatHandler::HandleWritePDumpCommand,       "",   NULL },
        { "mute",        SEC_GAMEMASTER,    &ChatHandler::HandleMuteCommand,             "",   NULL },
        { "unmute",      SEC_GAMEMASTER,    &ChatHandler::HandleUnmuteCommand,           "",   NULL },

        //! Development Commands
        { "setvalue",    SEC_ADMINISTRATOR, &ChatHandler::HandleSetValue,                "",   NULL },
        { "getvalue",    SEC_ADMINISTRATOR, &ChatHandler::HandleGetValue,                "",   NULL },
        { "Mod32Value",  SEC_ADMINISTRATOR, &ChatHandler::HandleMod32Value,              "",   NULL },

        { NULL,          0, NULL,                                        "",   NULL }
    };

    if(first_call)
    {
        QueryResult *result = sDatabase.Query("SELECT `name`,`security`,`help` FROM `command`");
        if (result)
        {
            do
            {
                Field *fields = result->Fetch();
                std::string name = fields[0].GetCppString();
                for(uint32 i = 0; commandTable[i].Name != NULL; i++)
                {
                    if (name == commandTable[i].Name)
                    {
                        commandTable[i].SecurityLevel = (uint16)fields[1].GetUInt16();
                        commandTable[i].Help = fields[2].GetCppString();
                    }
                    if(commandTable[i].ChildCommands != NULL)
                    {
                        ChatCommand *ptable = commandTable[i].ChildCommands;
                        for(uint32 j = 0; ptable[j].Name != NULL; j++)
                        {
                            if (name == fmtstring("%s %s", commandTable[i].Name, ptable[j].Name))
                            {
                                ptable[j].SecurityLevel = (uint16)fields[1].GetUInt16();
                                ptable[j].Help = fields[2].GetCppString();
                            }
                        }
                    }
                }
            } while(result->NextRow());
            delete result;
        }
        first_call = false;
    }

    return commandTable;
}

bool ChatHandler::hasStringAbbr(const char* s1, const char* s2)
{
    for(;;)
    {
        if( !*s2 )
            return true;
        else if( !*s1 )
            return false;
        else if( tolower( *s1 ) != tolower( *s2 ) )
            return false;
        s1++; s2++;
    }
}

void ChatHandler::SendSysMultilineMessage(WorldSession* session, const char *str)
{
    char buf[256];
    WorldPacket data;

    const char* line = str;
    const char* pos = line;
    while((pos = strchr(line, '\n')) != NULL)
    {
        strncpy(buf, line, pos-line);
        buf[pos-line]=0;

        FillSystemMessageData(&data, session, buf);
        session->SendPacket(&data);

        line = pos+1;
    }

    FillSystemMessageData(&data, session, line);
    session->SendPacket(&data);
}

void ChatHandler::SendSysMessage(WorldSession* session, const char *str)
{
    WorldPacket data;
    FillSystemMessageData(&data, session, str);
    session->SendPacket(&data);
}

void ChatHandler::PSendSysMultilineMessage(WorldSession* session, const char *format, ...)
{
    va_list ap;
    char str [1024];
    va_start(ap, format);
    vsnprintf(str,1024,format, ap );
    va_end(ap);
    SendSysMultilineMessage(session,str);
}

void ChatHandler::PSendSysMessage(WorldSession* session, const char *format, ...)
{
    va_list ap;
    char str [1024];
    va_start(ap, format);
    vsnprintf(str,1024,format, ap );
    va_end(ap);
    SendSysMessage(session,str);
}

void ChatHandler::PSendSysMultilineMessage(const char *format, ...)
{
    va_list ap;
    char str [1024];
    va_start(ap, format);
    vsnprintf(str,1024,format, ap );
    va_end(ap);
    SendSysMultilineMessage(m_session,str);
}

void ChatHandler::PSendSysMessage(const char *format, ...)
{
    va_list ap;
    char str [1024];
    va_start(ap, format);
    vsnprintf(str,1024,format, ap );
    va_end(ap);
    SendSysMessage(m_session,str);
}

bool ChatHandler::ExecuteCommandInTable(ChatCommand *table, const char* text)
{
    std::string fullcmd = text;                             // original `text` can't be used. It content destroyed in command code processing.
    std::string cmd = "";

    while (*text != ' ' && *text != '\0')
    {
        cmd += *text;
        text++;
    }

    while (*text == ' ') text++;

    if(!cmd.length())
        return false;

    for(uint32 i = 0; table[i].Name != NULL; i++)
    {
        if(!hasStringAbbr(table[i].Name, cmd.c_str()))
            continue;

        if(m_session->GetSecurity() < table[i].SecurityLevel)
            continue;

        if(table[i].ChildCommands != NULL)
        {
            if(!ExecuteCommandInTable(table[i].ChildCommands, text))
            {
                if(table[i].Help != "")
                    SendSysMultilineMessage(table[i].Help.c_str());
                else
                    SendSysMessage(LANG_NO_SUBCMD);
            }

            return true;
        }

        if((this->*(table[i].Handler))(text))
        {
            if(table[i].SecurityLevel > 0)
            {
                Player* p = m_session->GetPlayer();
                Creature* c = ObjectAccessor::Instance().GetCreature(*p,p->GetSelection());
                sLog.outCommand("Command: %s [Player: %s (Account: %u) X: %f Y: %f Z: %f Map: %u Selected: %s %u)]",
                    fullcmd.c_str(),p->GetName(),m_session->GetAccountId(),p->GetPositionX(),p->GetPositionY(),p->GetPositionZ(),p->GetMapId(),
                    (c ? "creature (Entry: " : (GUID_HIPART(p->GetSelection())==HIGHGUID_PLAYER ? "player (GUID: " : "none (")),GUID_LOPART(p->GetSelection()),
                    (c ? c->GetEntry() : (GUID_HIPART(p->GetSelection())==HIGHGUID_PLAYER ? GUID_LOPART(p->GetSelection()) : 0)));
            }
        }
        else
        {
            if(table[i].Help != "")
                SendSysMultilineMessage(table[i].Help.c_str());
            else
                SendSysMessage(LANG_CMD_SYNTAX);
        }

        return true;
    }

    return false;
}

int ChatHandler::ParseCommands(const char* text, WorldSession *session)
{
    m_session = session;

    ASSERT(text);
    ASSERT(*text);

    //if(m_session->GetSecurity() == 0)
    //    return 0;

    if(text[0] != '!' && text[0] != '.')
        return 0;

    // ignore single . and ! in line
    if(strlen(text) < 2)
        return 0;

    // ignore messages staring from many dots.
    if(text[0] == '.' && text[1] == '.' || text[0] == '!' && text[1] == '!')
        return 0;

    text++;

    if(!ExecuteCommandInTable(getCommandTable(), text))
    {
        WorldPacket data;
        FillSystemMessageData(&data, m_session, "There is no such command.");
        m_session->SendPacket(&data);
    }
    return 1;
}

//Note: target_guid used only in CHAT_MSG_WHISPER_INFORM mode (in this case channelName ignored)
void ChatHandler::FillMessageData( WorldPacket *data, WorldSession* session, uint8 type, uint32 language, const char *channelName, uint64 target_guid, const char *message, Unit *speaker)
{
    uint32 messageLength = (message ? strlen(message) : 0) + 1;

    data->Initialize(SMSG_MESSAGECHAT, 100);                // guess size
    *data << (uint8)type;
    *data << uint32((type != CHAT_MSG_CHANNEL) && (type != CHAT_MSG_WHISPER) ? language : 0);

    if (type == CHAT_MSG_CHANNEL)
    {
        ASSERT(channelName);
        *data << channelName;
        *data << (uint32)6;                                 // unk
    }

    switch(type)
    {
        case CHAT_MSG_SAY:
        case CHAT_MSG_PARTY:
        case CHAT_MSG_RAID:
        case CHAT_MSG_GUILD:
        case CHAT_MSG_OFFICER:
        case CHAT_MSG_YELL:
        case CHAT_MSG_WHISPER:
        case CHAT_MSG_CHANNEL:
        case CHAT_MSG_RAID_LEADER:
        case CHAT_MSG_RAID_WARN:
        case CHAT_MSG_BATTLEGROUND:
        case CHAT_MSG_BATTLEGROUND_HORDE:
        case CHAT_MSG_BATTLEGROUND_ALLIANCE:
        case CHAT_MSG_BATTLEGROUND_CHAT:
        case CHAT_MSG_BATTLEGROUND_LEADER:
            target_guid = session ? session->GetPlayer()->GetGUID() : 0;
            break;
        case CHAT_MSG_MONSTER_SAY:
            *data << (uint64)(((Creature *)speaker)->GetGUID());
            *data << (uint32)(strlen(((Creature *)speaker)->GetCreatureInfo()->Name) + 1);
            *data << ((Creature *)speaker)->GetCreatureInfo()->Name;
            break;
        default:
            if (type != CHAT_MSG_WHISPER_INFORM && type != CHAT_MSG_IGNORED && type != CHAT_MSG_DND && type != CHAT_MSG_AFK)
                target_guid = 0;                            // only for CHAT_MSG_WHISPER_INFORM used original value target_guid
            break;
    }
    /*// in CHAT_MSG_WHISPER_INFORM mode used original target_guid
    if (type == CHAT_MSG_SAY || type == CHAT_MSG_CHANNEL || type == CHAT_MSG_WHISPER ||
        type == CHAT_MSG_YELL || type == CHAT_MSG_PARTY || type == CHAT_MSG_RAID ||
        type == CHAT_MSG_RAID_LEADER || type == CHAT_MSG_RAID_WARN ||
        type == CHAT_MSG_GUILD || type == CHAT_MSG_OFFICER ||
        type == CHAT_MSG_BATTLEGROUND_ALLIANCE || type == CHAT_MSG_BATTLEGROUND_HORDE ||
        type == CHAT_MSG_BATTLEGROUND_CHAT || type == CHAT_MSG_BATTLEGROUND_LEADER)
    {
        target_guid = session ? session->GetPlayer()->GetGUID() : 0;
    }
    else if (type == CHAT_MSG_MONSTER_SAY)
    {
        *data << (uint64)(((Creature *)speaker)->GetGUID());
        *data << (uint32)(strlen(((Creature *)speaker)->GetCreatureInfo()->Name) + 1);
        *data << ((Creature *)speaker)->GetCreatureInfo()->Name;
    }
    else if (type != CHAT_MSG_WHISPER_INFORM && type != CHAT_MSG_IGNORED && type != CHAT_MSG_DND && type != CHAT_MSG_AFK)
    {
        target_guid = 0;                                    // only for CHAT_MSG_WHISPER_INFORM used original value target_guid
    }*/

    *data << target_guid;

    if (type == CHAT_MSG_SAY || type == CHAT_MSG_YELL || type == CHAT_MSG_PARTY)
        *data << target_guid;

    *data << messageLength;
    *data << message;
    if(session != 0 && type != CHAT_MSG_WHISPER_INFORM && type != CHAT_MSG_DND && type != CHAT_MSG_AFK)
        *data << session->GetPlayer()->chatTag();
    else
        *data << uint8(0);
}

void ChatHandler::SpawnCreature(WorldSession *session, const char* name, uint32 level)
{
    /*
    //SpawnCreature is invallid, remains for educatial reasons
    Temp. disabled (c) Phantomas
        WorldPacket data;

        Player *chr = session->GetPlayer();
        float x = chr->GetPositionX();
        float y = chr->GetPositionY();
        float z = chr->GetPositionZ();
        float o = chr->GetOrientation();

        Creature* pCreature = new Creature();

        if(!pCreature->Create(objmgr.GenerateLowGuid(HIGHGUID_UNIT), name, chr->GetMapId(), x, y, z, o, objmgr.AddCreatureTemplate(pCreature->GetName(), displayId)))
        {
            delete pCreature;
            return false;
        }
        pCreature->SetZoneId(chr->GetZoneId());
        pCreature->SetUInt32Value(OBJECT_FIELD_ENTRY, objmgr.AddCreatureTemplate(pCreature->GetName(), displayId));
        pCreature->SetFloatValue(OBJECT_FIELD_SCALE_X, 1.0f);
        pCreature->SetUInt32Value(UNIT_FIELD_DISPLAYID, displayId);
        pCreature->SetHealth(28 + 30*level);
        pCreature->SetMaxHealth(28 + 30*level);
        pCreature->SetLevel(level);

        pCreature->SetFloatValue(UNIT_FIELD_COMBATREACH , 1.5f);
        pCreature->SetFloatValue(UNIT_FIELD_MAXDAMAGE ,  5.0f);
        pCreature->SetFloatValue(UNIT_FIELD_MINDAMAGE , 8.0f);
        pCreature->SetUInt32Value(UNIT_FIELD_BASEATTACKTIME, 1900);
        pCreature->SetUInt32Value(UNIT_FIELD_RANGEDATTACKTIME, 2000);
        pCreature->SetFloatValue(UNIT_FIELD_BOUNDINGRADIUS, 2.0f);
        pCreature->AIM_Initialize();
        sLog.outError("AddObject at Chat.cpp");

        MapManager::Instance().GetMap(pCreature->GetMapId())->Add(pCreature);
        pCreature->SaveToDB();
    */
}

Player * ChatHandler::getSelectedPlayer()
{
    uint64 guid  = m_session->GetPlayer()->GetSelection();

    if (guid == 0)
        return m_session->GetPlayer();

    return objmgr.GetPlayer(guid);
}

Unit* ChatHandler::getSelectedUnit()
{
    uint64 guid = m_session->GetPlayer()->GetSelection();

    if (guid == 0)
        return m_session->GetPlayer();

    return ObjectAccessor::Instance().GetUnit(*m_session->GetPlayer(),guid);
}

Creature* ChatHandler::getSelectedCreature()
{
    return ObjectAccessor::Instance().GetCreatureOrPet(*m_session->GetPlayer(),m_session->GetPlayer()->GetSelection());
}

char*     ChatHandler::extractKeyFromLink(char* text, char const* linkType)
{
    // skip empty (NULL is valid result if strtok(NULL, .. ) must be used
    if(text && !*text)
        return NULL;

    // return non link case 
    if(text && text[0]!='|')
        return strtok(text, " ");

    // [name] Shift-click form |color|linkType:key|h[name]|h|r
    // or
    // [name] Shift-click form |color|linkType:key:something1:...:somethingN|h[name]|h|r

    char* check = strtok(text, "|");                        // skip color
    if(!check)
        return NULL;                                        // end of data

    char* cLinkType = strtok(NULL, ":");                    // linktype

    if(strcmp(cLinkType,linkType) != 0)
    {
        strtok(NULL, " ");                                  // skip link tail (to allow continue strtok(NULL,s) use after retturn from function
        SendSysMessage(LANG_WRONG_LINK_TYPE);
        return NULL;
    }

    char* cKey = strtok(NULL, ":|");                        // extract key
    strtok(NULL, "]");                                      // skip name with possible spalces
    strtok(NULL, " ");                                      // skip link tail (to allow continue strtok(NULL,s) use after retturn from function
    return cKey;
}


char const *fmtstring( char const *format, ... )
{
    va_list        argptr;
    #define    MAX_FMT_STRING    32000
    static char        temp_buffer[MAX_FMT_STRING];
    static char        string[MAX_FMT_STRING];
    static int        index = 0;
    char    *buf;
    int len;

    va_start(argptr, format);
    vsnprintf(temp_buffer,MAX_FMT_STRING, format, argptr);
    va_end(argptr);

    len = strlen(temp_buffer);

    if( len >= MAX_FMT_STRING )
        return "ERROR";

    if (len + index >= MAX_FMT_STRING-1)
    {
        index = 0;
    }

    buf = &string[index];
    memcpy( buf, temp_buffer, len+1 );

    index += len + 1;

    return buf;
}
