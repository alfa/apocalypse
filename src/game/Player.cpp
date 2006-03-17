/* Player.cpp
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
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "ObjectMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "UpdateMask.h"
#include "Player.h"
#include "Quest.h"
#include "Spell.h"
#include "Stats.h"
#include "UpdateData.h"
#include "Channel.h"
#include "ZoneMapper.h"
//Groupcheck
#include "Group.h"

Player::Player ( ): Unit()
{
    m_objectType |= TYPE_PLAYER;
    m_objectTypeId = TYPEID_PLAYER;

    m_valuesCount = PLAYER_END;

    m_session = 0;

    m_afk = 0;
    m_curTarget = 0;
    m_curSelection = 0;
    m_lootGuid = 0;
    m_guildId = 0;
    m_petInfoId = 0;
    m_petLevel = 0;
    m_petFamilyId = 0;

    m_regenTimer = 0;
    m_dismountTimer = 0;
    m_dismountCost = 0;
    m_mount_pos_x = 0;
    m_mount_pos_y = 0;
    m_mount_pos_z = 0;

    m_nextSave = 900000;                          /* 15 min after create */

    m_currentSpell = NULL;
    m_resurrectGUID = NULL;
    m_resurrectX = m_resurrectY = m_resurrectZ = NULL;
    m_resurrectHealth = m_resurrectMana = NULL;

    // memset(m_items, 0, sizeof(Item*)*INVENTORY_SLOT_ITEM_END);
    memset(m_items, 0, sizeof(Item*)*BANK_SLOT_BAG_END);

    m_groupLeader = 0;
    m_isInGroup = false;
    m_isInvited = false;

    m_dontMove = false;
}


Player::~Player ( )
{
    ASSERT(!IsInWorld());
    // for(int i = 0; i < INVENTORY_SLOT_ITEM_END; i++)
    for(int i = 0; i < BANK_SLOT_BAG_END; i++)
    {
        if(m_items[i])
            delete m_items[i];
    }
    CleanupChannels();
}


///====================================================================
///  Create
///  params: p_newChar
///  desc:   data from client to create a new character
//====================================================================
void Player::Create( uint32 guidlow, WorldPacket& data )
{
    int i;
    uint8 race,class_,gender,skin,face,hairStyle,hairColor,facialHair,outfitId;
    uint32 baseattacktime[2];

    Object::_Create(guidlow, HIGHGUID_PLAYER);

    // for (i = 0; i < INVENTORY_SLOT_ITEM_END; i++)
    for (i = 0; i < BANK_SLOT_BAG_END; i++)
        m_items[i] = NULL;

    // unpack data into member variables
    data >> m_name;
    data >> race >> class_ >> gender >> skin >> face;
    data >> hairStyle >> hairColor >> facialHair >> outfitId;

    //////////  Constant for everyone  ////////////////
    // Starting Locs
    // Human(1): 0, -8949.95, -132.493, 83.5312
    // Orc(2): 1, -618.518, -4251.67, 38.718
    // Dwarf(3): 0, -6240.32, 331.033, 382.758
    // Night Elf(4): 1, 10311.3, 832.463, 1326.41
    // Undead(5): 0, 1676.35, 1677.45, 121.67
    // Tauren(6): 1, -2917.58, -257.98, 52.9968
    // Gnome(7): See Dwarf
    // Troll(8): See Orc

    // LEFT SIDE
    // Head        0
    // Neck        1
    // Shoulders   2
    // Back        14
    // Chest       4
    // Shirt       3
    // Tabard      18
    // Wrists      8

    // RIGHT SIDE
    // Hands       9
    // Waist       5
    // Legs        6
    // Feet        7
    // Finger A    10
    // Finger B    11
    // Trinket A   12
    // Trinket B   13

    // WIELDED
    // Main hand   15
    // Offhand     16
    // Ranged      17

    PlayerCreateInfo *info = objmgr.GetPlayerCreateInfo(race, class_);
    ASSERT(info);

    baseattacktime[0] = 2000;
    baseattacktime[1] = 2000;

    m_mapId = info->mapId;
    m_zoneId = info->zoneId;
    m_positionX = info->positionX;
    m_positionY = info->positionY;
    m_positionZ = info->positionZ;
    memset(m_taximask, 0, sizeof(m_taximask));

    uint8 powertype = 0;
    switch(class_)
    {
        case WARRIOR : powertype = 1; break;      // Rage
        case PALADIN : powertype = 0; break;      // Mana
        case HUNTER  : powertype = 0; break;
        case ROGUE   : powertype = 3; break;      // Energy
        case PRIEST  : powertype = 0; break;
        case SHAMAN  : powertype = 0; break;
        case MAGE    : powertype = 0; break;
        case WARLOCK : powertype = 0; break;
        case DRUID   : powertype = 0; break;
    }                                             // 2 = Focus (unused)

    // Set Starting stats for char
    SetFloatValue(OBJECT_FIELD_SCALE_X, 1.0f);
    SetUInt32Value(UNIT_FIELD_HEALTH, info->health);
    SetUInt32Value(UNIT_FIELD_POWER1, info->mana );
    SetUInt32Value(UNIT_FIELD_POWER2, 0 );        // this gets devided by 10
    SetUInt32Value(UNIT_FIELD_POWER3, info->focus );
    SetUInt32Value(UNIT_FIELD_POWER4, info->energy );
    //SetUInt32Value(UNIT_FIELD_POWER5, 0xEEEEEEEE );
    SetUInt32Value(UNIT_FIELD_MAXHEALTH, info->health);
    SetUInt32Value(UNIT_FIELD_MAXPOWER1, info->mana );
    SetUInt32Value(UNIT_FIELD_MAXPOWER2, info->rage );
    SetUInt32Value(UNIT_FIELD_MAXPOWER3, info->focus );
    SetUInt32Value(UNIT_FIELD_MAXPOWER4, info->energy );
    //SetUInt32Value(UNIT_FIELD_MAXPOWER5, 5 );
    SetUInt32Value(UNIT_FIELD_LEVEL, 1 );
    SetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE, 1 );
    SetUInt32Value(UNIT_FIELD_BYTES_0, ( ( race ) | ( class_ << 8 ) | ( gender << 16 ) | ( powertype << 24 ) ) );
    SetUInt32Value(UNIT_FIELD_BYTES_1, 0x0011EE00 );
    SetUInt32Value(UNIT_FIELD_BYTES_2, 0xEEEEEE00 );
    SetUInt32Value(UNIT_FIELD_FLAGS , 0x08 );
    SetUInt32Value(UNIT_FIELD_STR, info->strength );
    SetUInt32Value(UNIT_FIELD_AGILITY, info->ability );
    SetUInt32Value(UNIT_FIELD_STAMINA, info->stamina );
    SetUInt32Value(UNIT_FIELD_IQ, info->intellect );
    SetUInt32Value(UNIT_FIELD_SPIRIT, info->spirit );
    SetUInt32Value(UNIT_FIELD_BASEATTACKTIME, baseattacktime[0] );
    SetUInt32Value(UNIT_FIELD_BASEATTACKTIME+1, baseattacktime[1]  );
    SetFloatValue(UNIT_FIELD_BOUNDINGRADIUS, 0.388999998569489f );
    SetFloatValue(UNIT_FIELD_COMBATREACH, 1.5f   );
    SetUInt32Value(UNIT_FIELD_DISPLAYID, info->displayId + gender );
    SetUInt32Value(UNIT_FIELD_NATIVEDISPLAYID, info->displayId + gender );
    SetFloatValue(UNIT_FIELD_MINDAMAGE, info->mindmg );
    SetFloatValue(UNIT_FIELD_MAXDAMAGE, info->maxdmg );
    SetUInt32Value(UNIT_FIELD_ATTACKPOWER, info->attackpower );
    SetUInt32Value(PLAYER_BYTES, ((skin) | (face << 8) | (hairStyle << 16) | (hairColor << 24)));
    SetUInt32Value(PLAYER_BYTES_2, (facialHair | (0xEE << 8) | (0x01 << 16) | (0x01 << 24)));
    SetUInt32Value(PLAYER_NEXT_LEVEL_XP, 400);
    SetUInt32Value(PLAYER_FIELD_BYTES, 0xEEEE0000 );

    SetFloatValue(PLAYER_FIELD_MOD_DAMAGE_DONE_PCT, 1.00);
    SetUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG, 0);
    SetUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS, 0);

    Item *item;
    for (i=0; i<10; i++)
    {
        if ( (info->item[i]!=0) && (info->item_slot[i]!=0) )
        {
            item = new Item();
            item->Create(objmgr.GenerateLowGuid(HIGHGUID_ITEM), info->item[i], this);
            AddItemToSlot(info->item_slot[i], item);
        }
    }

    for (i=0; i<10; i++)
    {
        if ( info->spell[i]!=0 )
        {
            addSpell(info->spell[i], 0);
        }
    }

    // Not worrying about this stuff for now
    m_guildId = 0;
    m_petInfoId = 0;
    m_petLevel = 0;
    m_petFamilyId = 0;

    // adding skilllines
    // TODO: Maybe add ids of skills in sql db ?
    switch(class_)
    {
        case WARRIOR :
            AddSkillLine(413, 1, 1);              // Mail
            AddSkillLine(414, 1, 1);              // Leather
            AddSkillLine(415, 1, 1);              // Cloth
            AddSkillLine(433, 1, 1);              // Shields
            AddSkillLine(26, 1, 1);               // Arms
            AddSkillLine(256, 1, 1);              // Fury
            AddSkillLine(257, 1, 1);              // Protection
            switch (race)
            {
                case 1:                           // Human
                    AddSkillLine(43, 1, 50);      // Swords
                    AddSkillLine(44, 1, 50);      // Axes
                    AddSkillLine(54, 1, 50);      // Maces
                    AddSkillLine(55, 1, 50);      // Two-Handed Swords
                    break;
                case 2:                           // Orc
                    AddSkillLine(44, 1, 50);      // Axes
                    AddSkillLine(172, 1, 50);     // Two-Handed Axes
                    break;
                case 3:                           // Dwarf
                    AddSkillLine(44, 1, 50);      // Axes
                    AddSkillLine(54, 1, 50);      // Maces
                    AddSkillLine(172, 1, 50);     // Two-Handed Axes
                    break;
                case 4:                           // Night Elf
                    AddSkillLine(43, 1, 50);      // Swords
                    AddSkillLine(54, 1, 50);      // Maces
                    AddSkillLine(173, 1, 50);     // Daggers
                    break;
                case 5:                           // Undead
                    AddSkillLine(43, 1, 50);      // Swords
                    AddSkillLine(55, 1, 50);      // Two-Handed Swords
                    AddSkillLine(173, 1, 50);     // Daggers
                    break;
                case 6:                           // Tauren
                    AddSkillLine(44, 1, 50);      // Axes
                    AddSkillLine(54, 1, 50);      // Maces
                    AddSkillLine(160, 1, 50);     // Two-Handed Maces
                    break;
                case 7:                           // Gnome
                    AddSkillLine(43, 1, 50);      // Swords
                    AddSkillLine(54, 1, 50);      // Maces
                    AddSkillLine(173, 1, 50);     // Daggers
                    break;
                case 8:                           // Troll
                    AddSkillLine(44, 1, 50);      // Axes
                    AddSkillLine(173, 1, 50);     // Daggers
                    AddSkillLine(176, 1, 50);     // Thrown
                    break;
            }
            break;
        case PALADIN :
            AddSkillLine(413, 1, 1);              // Mail
            AddSkillLine(414, 1, 1);              // Leather
            AddSkillLine(415, 1, 1);              // Cloth
            AddSkillLine(433, 1, 1);              // Shields
            AddSkillLine(184, 1, 1);              // Retribution
            AddSkillLine(267, 1, 1);              // Protection
            AddSkillLine(594, 1, 1);              // Holy
            switch (race)
            {
                case 1:                           // Human
                    AddSkillLine(43, 1, 50);      // Swords
                    AddSkillLine(54, 1, 50);      // Maces
                    AddSkillLine(55, 1, 50);      // Two-Handed Swords
                    AddSkillLine(160, 1, 50);     // Two-Handed Maces
                    break;
                case 3:                           // Dwarf
                    AddSkillLine(54, 1, 50);      // Maces
                    AddSkillLine(160, 1, 50);     // Two-Handed Maces
                    break;
            }
            break;
        case HUNTER  :
            AddSkillLine(414, 1, 1);              // Leather
            AddSkillLine(415, 1, 1);              // Cloth
            AddSkillLine(50, 1, 1);               // Beast Mastery
            AddSkillLine(51, 1, 1);               // Survival
            AddSkillLine(163, 1, 1);              // Marksmanship
            switch (race)
            {
                case 2:                           // Orc
                    AddSkillLine(44, 1, 50);      // Axes
                    AddSkillLine(45, 1, 50);      // Bows
                    break;
                case 3:                           // Dwarf
                    AddSkillLine(44, 1, 50);      // Axes
                    AddSkillLine(46, 1, 50);      // Guns
                    break;
                case 4:                           // Night Elf
                    AddSkillLine(44, 1, 50);      // Axes
                    AddSkillLine(45, 1, 50);      // Bows
                    AddSkillLine(137, 1, 50);     // Daggers
                    break;
                case 6:                           // Tauren
                    AddSkillLine(44, 1, 50);      // Axes
                    AddSkillLine(46, 1, 50);      // Guns
                    break;
                case 8:                           // Troll
                    AddSkillLine(44, 1, 50);      // Axes
                    AddSkillLine(45, 1, 50);      // Bows
                    break;
            }
            break;
        case ROGUE   :
            AddSkillLine(414, 1, 1);              // Leather
            AddSkillLine(415, 1, 1);              // Cloth
            AddSkillLine(38, 1, 1);               // Combat
            AddSkillLine(39, 1, 1);               // Subtlety
            AddSkillLine(253, 1, 1);              // Assassination
            AddSkillLine(176, 1, 50);             // Thrown
            AddSkillLine(173, 1, 1);              // Daggers
            break;
        case PRIEST  :
            AddSkillLine(415, 1, 1);              // Cloth
            AddSkillLine(56, 1, 1);               // Holy
            AddSkillLine(78, 1, 1);               // Shadow Magic
            AddSkillLine(613, 1, 1);              // Discipline
            AddSkillLine(228, 1, 50);             // Wands
            AddSkillLine(54, 1, 1);               // Maces
            break;
        case SHAMAN  :
            AddSkillLine(414, 1, 1);              // Leather
            AddSkillLine(415, 1, 1);              // Cloth
            AddSkillLine(433, 1, 1);              // Shields
            AddSkillLine(373, 1, 1);              // Enhancement
            AddSkillLine(374, 1, 1);              // Restoration
            AddSkillLine(375, 1, 1);              // Elemental Combat
            switch (race)
            {
                case 2:                           // Orc
                    AddSkillLine(44, 1, 50);      // Axes
                    AddSkillLine(54, 1, 50);      // Maces
                    AddSkillLine(136, 1, 50);     // Staves
                    AddSkillLine(172, 1, 50);     // Two-Handed Axes
                    break;
                case 6:                           // Tauren
                    AddSkillLine(54, 1, 50);      // Maces
                    AddSkillLine(136, 1, 50);     // Staves
                    break;
                case 8:                           // Troll
                    AddSkillLine(54, 1, 50);      // Maces
                    AddSkillLine(136, 1, 50);     // Staves
                    break;
            }
            break;
        case MAGE    :
            AddSkillLine(415, 1, 1);              // Cloth
            AddSkillLine(6, 1, 1);                // Frost
            AddSkillLine(8, 1, 1);                // Fire
            AddSkillLine(237, 1, 1);              // Arcane Magic
            AddSkillLine(228, 1, 50);             // Wands
            AddSkillLine(136, 1, 50);             // Staves
            break;
        case WARLOCK :
            AddSkillLine(415, 1, 1);              // Cloth
            AddSkillLine(354, 1, 1);              // Demonology
            AddSkillLine(355, 1, 1);              // Affliction
            AddSkillLine(593, 1, 1);              // Destruction
            AddSkillLine(228, 1, 50);             // Wands
            AddSkillLine(173, 1, 1);              // Daggers
            break;
        case DRUID   :
            AddSkillLine(414, 1, 1);              // Leather
            AddSkillLine(415, 1, 1);              // Cloth
            AddSkillLine(134, 1, 1);              // Feral Combat
            AddSkillLine(573, 1, 1);              // Restoration
            AddSkillLine(574, 1, 1);              // Balance
            AddSkillLine(433, 1, 50);             // Bows
            AddSkillLine(228, 1, 50);             // Wands
            switch (race)
            {
                case 4:                           // Night Elf
                    AddSkillLine(136, 1, 50);     // Staves
                    AddSkillLine(137, 1, 50);     // Daggers
                    break;
                case 6:                           // Tauren
                    AddSkillLine(54, 1, 1);       // Maces
                    AddSkillLine(136, 1, 50);     // Staves
                    break;
            }
            break;
    }

    AddSkillLine(95, 1, 50);                      // Defense
    AddSkillLine(162, 1, 50);                     // Unarmed

    switch (race)
    {
        case 1:                                   // Human
            AddSkillLine(98, 300, 300);           // Laguage: Common
            break;
        case 2:                                   // Orc
            AddSkillLine(109, 300, 300);          // Laguage: Orcish
            break;
        case 3:                                   // Dwarf
            AddSkillLine(98, 300, 300);           // Laguage: Common
            AddSkillLine(111, 300, 300);          // Laguage: Dwarven
            break;
        case 4:                                   // Night Elf
            AddSkillLine(98, 300, 300);           // Laguage: Common
            AddSkillLine(113, 300, 300);          // Laguage: Darnassian
            break;
        case 5:                                   // Undead
            AddSkillLine(109, 300, 300);          // Laguage: Orcish
            AddSkillLine(673, 300, 300);          // Laguage: Gutterspeak
            break;
        case 6:                                   // Tauren
            AddSkillLine(109, 300, 300);          // Laguage: Orcish
            AddSkillLine(115, 300, 300);          // Laguage: Taurane
            break;
        case 7:                                   // Gnome
            AddSkillLine(98, 300, 300);           // Laguage: Common
            AddSkillLine(313, 300, 300);          // Laguage: Gnomish
            break;
        case 8:                                   // Troll
            AddSkillLine(109, 300, 300);          // Laguage: Orcish
            AddSkillLine(315, 300, 300);          // Laguage: Trollish
            break;
    }
}


void Player::Update( uint32 p_time )
{
    if(!IsInWorld())
        return;

    WorldPacket data;

    Unit::Update( p_time );

    if (m_state & UF_ATTACKING)
    {
        // In combat!
        if (isAttackReady())
        {
            Unit *pVictim = NULL;

            pVictim = objmgr.GetObject<Creature>(m_curSelection);

            if (!pVictim)
            {
                Log::getSingleton( ).outDetail("Player::Update:  No valid current selection to attack, stopping attack\n");
                this->setRegenTimer(5000);        //prevent clicking off creature for a quick heal
                clearStateFlag(UF_ATTACKING);
                smsg_AttackStop(m_curSelection);
            }
            else
            {
                if(!canReachWithAttack(pVictim))
                {
                    setAttackTimer(uint32(1000));
                    data.Initialize(SMSG_CAST_RESULT);
                    data << uint32(0);
                    data << uint8(2);
                    data << uint8(0x52);          // Target out of Range
                    GetSession()->SendPacket(&data);
/*
                }
                else if(!isInFront(pVictim,10.00000f))
                {
                    setAttackTimer(uint32(1000));
                    data.Initialize(SMSG_CAST_RESULT);
                    data << uint32(0);
                    data << uint8(2);
                    data << uint8(0x75);    // Target not in Front
                    GetSession()->SendPacket(&data);
*/
                }
                else
                {
                    setAttackTimer(0);
                    AttackerStateUpdate(pVictim, 0);
                }
            }
        }
    }

    // only regenerate if alive
    if (isAlive())
    {
        RegenerateAll();
    }

    // Dead System
    if (m_deathState == JUST_DIED)
    {
        KillPlayer();
    }

    /* Auto-Dismount after Taxiride */
    if(m_dismountTimer > 0)
    {
        if(p_time >= m_dismountTimer)
        {
            m_dismountTimer = 0;

            SetUInt32Value( PLAYER_FIELD_COINAGE , m_dismountCost);
            m_dismountCost = 0;

            SetPosition( m_mount_pos_x,
                m_mount_pos_y,
                m_mount_pos_z, true );

            SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID , 0);
            RemoveFlag( UNIT_FIELD_FLAGS, 0x002000 );

            /* Remove the "player locked" flag, to allow movement */
            if (GetUInt32Value(UNIT_FIELD_FLAGS) & 0x000004 )
                RemoveFlag( UNIT_FIELD_FLAGS, 0x000004 );

            SetPlayerSpeed(RUN,7.5f,true);
            /* SetPlayerSpeed(RUN,7.5f,false); */

        }
        else
        {
            m_dismountTimer -= p_time;
        }
    }

    /* Auto-Save after 10min */
    if(m_nextSave > 0)
    {
        if(p_time >= m_nextSave)
        {
            m_nextSave = 600000;                  /* around about 10 min */
            SaveToDB();
            Log::getSingleton().outBasic("Player '%u' '%s' Saved", this->GetGUID(), this->GetName());
        }
        else
        {
            m_nextSave -= p_time;
        }
    }

/*
    // This was replaced by teleporting to spirit healer.
    if (m_timeOfDeath > 0 && (uint32)time(NULL) > m_timeOfDeath + m_corpseDelay)
    {
        m_timeOfDeath = 0;
        m_respawnDelay = 5000;
        session->GetPlayer( )->SetUInt32Value( PLAYER_BYTES_2, (0xffffffff - 0x10) & session->GetPlayer( )->GetUInt32Value( PLAYER_BYTES_2 ) );
        session->GetPlayer( )->SetUInt32Value( UNIT_FIELD_FLAGS, (0xffffffff - 65536) & session->GetPlayer( )->GetUInt32Value( UNIT_FIELD_FLAGS ) );
        session->GetPlayer()->setDeathState(ALIVE);
    }

    // UpdateObject();
*/

}


void Player::BuildEnumData( WorldPacket * p_data )
{
    *p_data << GetGUID();
    *p_data << m_name;

    uint32 bytes = GetUInt32Value(UNIT_FIELD_BYTES_0);
    *p_data << uint8(bytes & 0xff);               // race
    *p_data << uint8((bytes >> 8) & 0xff);        // class
    *p_data << uint8((bytes >> 16) & 0xff);       // gender

    bytes = GetUInt32Value(PLAYER_BYTES);
    *p_data << uint8(bytes & 0xff);               //skin
    *p_data << uint8((bytes >> 8) & 0xff);        //face
    *p_data << uint8((bytes >> 16) & 0xff);       //hairstyle
    *p_data << uint8((bytes >> 24) & 0xff);       //haircolor

    bytes = GetUInt32Value(PLAYER_BYTES_2);
    *p_data << uint8(bytes & 0xff);               //facialhair

    //level
    *p_data << uint8(GetUInt32Value(UNIT_FIELD_LEVEL));

    *p_data << GetZoneId();
    *p_data << GetMapId();

    *p_data << m_positionX;
    *p_data << m_positionY;
    *p_data << m_positionZ;

    *p_data << (uint32)0;                         // guild
    *p_data << (uint32)0;                         // unknown
    *p_data << (uint8)1;                          // rest state
    *p_data << (uint32)m_petInfoId;               // pet info id
    *p_data << (uint32)m_petLevel;                // pet level
    *p_data << (uint32)m_petFamilyId;             // pet family id

    for (int i = 0; i < 20; i++)
    {
        if (m_items[i] != NULL)
        {
            *p_data << (uint32)m_items[i]->GetProto()->DisplayInfoID;
            *p_data << (uint8)m_items[i]->GetProto()->InventoryType;
        }
        else
        {
            *p_data << (uint32)0;
            *p_data << (uint8)0;
        }
    }

    // assert( p_data->getLength() <= 176 );
}


/////////////////////////////////// QUESTS ////////////////////////////////////////////
uint32 Player::getQuestStatus(uint32 quest_id)
{
    if( mQuestStatus.find( quest_id ) == mQuestStatus.end( ) ) return 0;
    return mQuestStatus[quest_id].status;
}


uint32 Player::addNewQuest(uint32 quest_id, uint32 status)
{
    quest_status qs;
    qs.quest_id = quest_id;
    qs.status = status;

    mQuestStatus[quest_id] = qs;
    return status;
};

void Player::loadExistingQuest(quest_status qs)
{
    mQuestStatus[qs.quest_id] = qs;
}


void Player::setQuestStatus(uint32 quest_id, uint32 new_status)
{
    assert( mQuestStatus.find( quest_id ) != mQuestStatus.end( ) );
    mQuestStatus[quest_id].status = new_status;
}


uint16 Player::getOpenQuestSlot()
{
    uint16 start = PLAYER_QUEST_LOG_1_1;
    uint16 end = PLAYER_QUEST_LOG_1_1 + 80;
    for (uint16 i = start; i <= end; i+=4)
        if (GetUInt32Value(i) == 0)
            return i;

    return 0;
}


uint16 Player::getQuestSlot(uint32 quest_id)
{
    uint16 start = PLAYER_QUEST_LOG_1_1;
    uint16 end = PLAYER_QUEST_LOG_1_1 + 80;
    for (uint16 i = start; i <= end; i+=4)
        if (GetUInt32Value(i) == quest_id)
            return i;

    return 0;
}


/*
void Player::setQuestLogBits(UpdateMask *updateMask)
{
    for( StatusMap::iterator i = mQuestStatus.begin( ); i != mQuestStatus.end( ); ++ i )
    {
        if (i->second.status == 3)
        {
            // incomplete, put the quest in the log
            uint16 log_slot = getQuestSlot(i->second.quest_id);
            struct quest_status qs = i->second;

            if (log_slot == 0)
            {
                // in case this quest hasnt been added to the updateValues (but it shoudl have been!)
                log_slot = getOpenQuestSlot();
                SetUInt32Value(log_slot, qs.quest_id);
                SetUInt32Value(log_slot+1, 0x337);
            }

            updateMask->setBit(log_slot);
            updateMask->setBit(log_slot+1);

            if (qs.m_questMobCount[0] > 0 || qs.m_questMobCount[1] > 0 ||
            qs.m_questMobCount[2] > 0 || qs.m_questMobCount[3] > 0)
            {
                updateMask->setBit(log_slot+2);
            }
        }
    }
}
*/


void Player::KilledMonster(uint32 entry, const uint64 &guid)
{
    for( StatusMap::iterator i = mQuestStatus.begin( ); i != mQuestStatus.end( ); ++ i )
    {
        if (i->second.status == 3)
        {
            Quest *pQuest = objmgr.GetQuest(i->first);
            for (int j=0; j<4; j++)
            {
                if (pQuest->m_questMobId[j] == entry)
                {
                    if (i->second.m_questMobCount[j]+1 <= pQuest->m_questMobCount[j])
                    {
                        i->second.m_questMobCount[j]++ ;

                        // Send Update quest update kills message
                        WorldPacket data;
                        data.Initialize(SMSG_QUESTUPDATE_ADD_KILL);
                        data << pQuest->m_questId;
                        data << uint32(pQuest->m_questMobId[j]);
                        data << uint32(i->second.m_questMobCount[j]);
                        data << uint32(pQuest->m_questMobCount[j]);
                        data << guid;
                        GetSession()->SendPacket(&data);

                        // update journal
                        // this is crazy.  each bit corresponds to a kill, set multiple bits to signify multiple kills
                        uint32 start_bit=0;
                        if (j-1 < 0) start_bit = 0;
                        else
                            for (int n=j-1; n>=0; n--)
                                start_bit += pQuest->m_questMobCount[n];

                        uint16 log_slot = getQuestSlot(pQuest->m_questId);
                        uint32 kills = GetUInt32Value(log_slot+2);

                        int exp = start_bit + i->second.m_questMobCount[j]-1;
                        kills |= 1 << exp;
                        SetUInt32Value(log_slot+2, kills);
                    }

                    checkQuestStatus(i->second.quest_id);
                    // Ehh, I think a packet should be sent here, but I havent found one in the official logs yet

                    return;
                }                                 // end if mobId == entry
            }                                     // end for each mobId
        }                                         // end if status == 3
    }                                             // end for each quest
}


//======================================================
///  Check to see if all the required monsters and items
///  have been killed and collected.
//======================================================
bool Player::checkQuestStatus(uint32 quest_id)
{
    assert( mQuestStatus.find( quest_id ) != mQuestStatus.end( ) );
    quest_status qs = mQuestStatus[quest_id];
    Quest *pQuest = objmgr.GetQuest(quest_id);

    if (qs.m_questItemCount[0] == pQuest->m_questItemCount[0] &&
        qs.m_questItemCount[1] == pQuest->m_questItemCount[1] &&
        qs.m_questItemCount[2] == pQuest->m_questItemCount[2] &&
        qs.m_questItemCount[3] == pQuest->m_questItemCount[3] &&
        qs.m_questMobCount[0] == pQuest->m_questMobCount[0] &&
        qs.m_questMobCount[1] == pQuest->m_questMobCount[1] &&
        qs.m_questMobCount[2] == pQuest->m_questMobCount[2] &&
        qs.m_questMobCount[3] == pQuest->m_questMobCount[3])
    {
        // Quest complete!
        return true;
    }

    return false;
}


///  This function sends the message displaying the purple XP gain for the char
///  It assumes you will send out an UpdateObject packet at a later time.

void Player::GiveXP(uint32 xp, const uint64 &guid)
{
    if ( xp < 1 )
        return;

    WorldPacket data;
    if (guid != 0)
    {
        // Send out purple XP gain message, but ONLY if a valid GUID was passed in
        // This message appear to be only for gaining XP from a death
        data.Initialize( SMSG_LOG_XPGAIN );
        data << guid;
        data << uint32(xp) << uint8(0);
        data << uint16(xp) << uint8(0);
        data << uint8(0);
        GetSession()->SendPacket(&data);
    }

    uint32 curXP = GetUInt32Value(PLAYER_XP);
    uint32 nextLvlXP = GetUInt32Value(PLAYER_NEXT_LEVEL_XP);
    uint32 newXP = curXP + xp;
    uint16 level = (uint16)GetUInt32Value(UNIT_FIELD_LEVEL);
    bool levelup = false;
    uint32 TotalHealthGain = 0, TotalManaGain = 0;
    uint32 manaGain = GetUInt32Value(UNIT_FIELD_SPIRIT) / 2;
    uint32 newMana = GetUInt32Value(UNIT_FIELD_MAXPOWER1);
    uint32 healthGain = GetUInt32Value(UNIT_FIELD_STAMINA) / 2;
    uint32 newHealth = GetUInt32Value(UNIT_FIELD_MAXHEALTH);

    // Check for level-up
    while (newXP >= nextLvlXP)
    {
        levelup = true;
        // Level-Up!
        newXP -= nextLvlXP;                       // reset XP to 0, but add extra from this xp add

        level += 1;                               // increment the level

        // nextlevel XP Formulas
        if( level > 0 && level <= 30 )
        {
            nextLvlXP = ((int)((((double)(8 * level * ((level * 5) + 45)))/100)+0.5))*100;
        }
        else if( level = 31 )
        {
            nextLvlXP = ((int)((((double)(((8 * level) + 3) * ((level * 5) + 45)))/100)+0.5))*100;
        }
        else if( level = 32 )
        {
            nextLvlXP = ((int)((((double)(((8 * level) + 6) * ((level * 5) + 45)))/100)+0.5))*100;
        }
        else
        {
            nextLvlXP = ((int)((((double)(((8 * level) + ((level - 30) * 5)) * ((level * 5) + 45)))/100)+0.5))*100;
        }

        newHealth += healthGain;                  // increment Health

        if (newMana > 0)
        {
            newMana += manaGain;                  // increment Mana
        }
        if( level > 9)
        {
            // Give Talent Point
            uint32 curTalentPoints = GetUInt32Value(PLAYER_CHARACTER_POINTS1);
            SetUInt32Value(PLAYER_CHARACTER_POINTS1,curTalentPoints+1);

            if(isEven(level))
            {
                // Stuff to do when the level is even
            }
            else
            {
                // Stuff to do when the level is odd
            }
        }

        TotalHealthGain += healthGain;
        TotalManaGain += manaGain;
    }

    if(levelup)
    {
        // TODO: UNEQUIP everything and remove affects

        SetUInt32Value(PLAYER_NEXT_LEVEL_XP, nextLvlXP);
        SetUInt32Value(UNIT_FIELD_LEVEL, level);
        SetUInt32Value(UNIT_FIELD_MAXHEALTH, newHealth);
        SetUInt32Value(UNIT_FIELD_HEALTH, newHealth);
        SetUInt32Value(UNIT_FIELD_POWER1, newMana);
        SetUInt32Value(UNIT_FIELD_MAXPOWER1, newMana);

        // TODO: REEQUIP everything and add effects

        data.Initialize(SMSG_LEVELUP_INFO);

        data << uint32(level);
        data << uint32(TotalHealthGain);          // health gain
        data << uint32(TotalManaGain);            // mana gain
        data << uint32(1);
        data << uint32(2);
        data << uint32(3);

        // 6 new fields
        data << uint32(4);
        data << uint32(0);                        // Strength
        data << uint32(0);                        // Agility
        data << uint32(0);                        // Stamina
        data << uint32(0);                        // Intellect
        data << uint32(0);                        // Spirit

        WPAssert(data.size() == 48);
        GetSession()->SendPacket(&data);
    }
    // Set the update bit
    SetUInt32Value(PLAYER_XP, newXP);
}


void Player::smsg_InitialSpells()
{
    WorldPacket data;
    uint16 spellCount = m_spells.size();

    data.Initialize( SMSG_INITIAL_SPELLS );
    data << uint8(0);
    data << uint16(spellCount);                   // spell count

    std::list<struct spells>::iterator itr;
    for (itr = m_spells.begin(); itr != m_spells.end(); ++itr)
    {
        data << uint16(itr->spellId);             // spell id
        data << uint16(itr->slotId);              // slot
    }
    data << uint16(0);

    WPAssert(data.size() == 5+(4*spellCount));

    GetSession()->SendPacket(&data);

    Log::getSingleton( ).outDetail( "CHARACTER: Sent Initial Spells" );
}


bidentry* Player::GetBid(uint32 id)
{
    std::list<bidentry*>::iterator itr;
    for (itr = m_bids.begin(); itr != m_bids.end();)
    {
        if ((*itr)->AuctionID == id)
        {
            return (*itr);
        }
        else
        {
            ++itr;
        }
    }
    return NULL;

}


void Player::AddBid(bidentry *be)
{
    std::list<bidentry*>::iterator itr;
    for (itr = m_bids.begin(); itr != m_bids.end();)
    {
        if ((*itr)->AuctionID == be->AuctionID)
        {
            // std::list<bidentry*>::iterator iold = itr++;
            // bidentry* b = *iold;
            m_bids.erase(itr++);
            // delete b;
        }
        else
        {
            ++itr;
        }
    }
    m_bids.push_back(be);

}


void Player::RemoveMail(uint32 id)
{
    std::list<Mail*>::iterator itr;
    for (itr = m_mail.begin(); itr != m_mail.end();)
    {
        if ((*itr)->messageID == id)
        {
            m_mail.erase(itr++);
        }
        else
        {
            ++itr;
        }
    }
}


void Player::AddMail(Mail *m)
{
    std::list<Mail*>::iterator itr;
    for (itr = m_mail.begin(); itr != m_mail.end();)
    {
        if ((*itr)->messageID == m->messageID)
        {
            m_mail.erase(itr++);
        }
        else
        {
            ++itr;
        }
    }
    m_mail.push_back(m);
}


void Player::_SaveAuctions()
{
    std::stringstream delinvq, del;
    // TODO: use full guids
    delinvq << "DELETE FROM auctionhouse WHERE itemowner = " << GetGUIDLow();
    sDatabase.Execute( delinvq.str().c_str( ) );
    ObjectMgr::AuctionEntryMap::iterator itr;
    for (itr = objmgr.GetAuctionsBegin();itr != objmgr.GetAuctionsEnd();itr++)
    {
        AuctionEntry *Aentry = itr->second;
        if ((Aentry) && (Aentry->owner == GetGUIDLow()))
        {
            Item *it = objmgr.GetAItem(Aentry->item);
            // TODO: use full guids
            del<< "DELETE FROM auctioned_items WHERE guid = " << it->GetGUIDLow();
            sDatabase.Execute( del.str().c_str( ) );
            std::stringstream invq;
            invq <<  "INSERT INTO auctionhouse (auctioneerguid, itemguid, itemowner,buyoutprice,time,buyguid,lastbid,Id) VALUES ( " <<
                Aentry->auctioneer << ", " << Aentry->item << ", " << Aentry->owner << ", " << Aentry->buyout << ", " << Aentry->time << ", " << Aentry->bidder << ", " << Aentry->bid << ", " << Aentry->Id << " )";
            sDatabase.Execute( invq.str().c_str( ) );
            std::stringstream ss;
            ss << "INSERT INTO auctioned_items (guid, data) VALUES ("
                << it->GetGUIDLow() << ", '";     // TODO: use full guids
            for(uint16 i = 0; i < it->GetValuesCount(); i++ )
            {
                ss << it->GetUInt32Value(i) << " ";
            }
            ss << "' )";
            sDatabase.Execute( ss.str().c_str() );
        }
    }
}


void Player::_SaveMail()
{
    std::stringstream delinvq;
    // TODO: use full guids
    delinvq << "DELETE FROM mail WHERE reciever = " << GetGUIDLow();
    sDatabase.Execute( delinvq.str().c_str( ) );
    std::list<Mail*>::iterator itr;
    for (itr = m_mail.begin(); itr != m_mail.end(); itr++)
    {
        Mail *m = (*itr);
        std::stringstream invq;
        invq <<  "INSERT INTO mail (mailId,sender,reciever,subject,body,item,time,money,COD,checked) VALUES ( " <<
            m->messageID << ", " << m->sender << ", " << m->reciever << ", '" << m->subject.c_str() << "', '" << m->body.c_str() << "', " <<
            m->item << ", " << m->time << ", " << m->money << ", " << m->COD << ", " << m->checked << " )";

        sDatabase.Execute( invq.str().c_str( ) );
    }
}


void Player::_SaveBids()
{
    std::stringstream delinvq;
    // TODO: use full guids
    delinvq << "DELETE FROM bids WHERE bidder = " << GetGUIDLow();
    sDatabase.Execute( delinvq.str().c_str( ) );
    std::list<bidentry*>::iterator itr;
    for (itr = m_bids.begin(); itr != m_bids.end(); itr++)
    {
        AuctionEntry *a = objmgr.GetAuction((*itr)->AuctionID);
        if (a)
        {
            std::stringstream invq;
            invq <<  "INSERT INTO bids (bidder, Id, amt) VALUES ( " <<
                GetGUIDLow() << ", " << (*itr)->AuctionID << ", " << (*itr)->amt << " )";

            sDatabase.Execute( invq.str().c_str( ) );
        }
    }

}


void Player::_LoadMail()
{
    // Clear spell list
    m_mail.clear();

    // Mail
    std::stringstream query;
    query << "SELECT * FROM mail WHERE reciever=" << GetGUIDLow();

    QueryResult *result = sDatabase.Query( query.str().c_str() );
    if(result)
    {
        do
        {
            Field *fields = result->Fetch();
            Mail *be = new Mail;
            be->messageID = fields[0].GetUInt32();
            be->sender = fields[1].GetUInt32();
            be->reciever = fields[2].GetUInt32();
            be->subject = fields[3].GetString();
            be->body = fields[4].GetString();
            be->item = fields[5].GetUInt32();
            be->time = fields[6].GetUInt32();
            be->money = fields[7].GetUInt32();
            be->COD = fields[8].GetUInt32();
            be->checked = fields[9].GetUInt32();
            m_mail.push_back(be);
        }
        while( result->NextRow() );

        delete result;
    }

}


void Player::_LoadBids()
{
    // Clear spell list
    m_bids.clear();

    // Spells
    std::stringstream query;
    query << "SELECT Id,amt FROM bids WHERE bidder=" << GetGUIDLow();

    QueryResult *result = sDatabase.Query( query.str().c_str() );
    if(result)
    {
        do
        {
            Field *fields = result->Fetch();
            bidentry *be = new bidentry;
            be->AuctionID = fields[0].GetUInt32();
            be->amt = fields[1].GetUInt32();
            m_bids.push_back(be);
        }
        while( result->NextRow() );

        delete result;
    }

}


void Player::addSpell(uint16 spell_id, uint16 slot_id)
{
    struct spells newspell;
    newspell.spellId = spell_id;

    if (slot_id == 0xffff)
    {
        uint16 maxid = 0;
        std::list<struct spells>::iterator itr;
        for (itr = m_spells.begin(); itr != m_spells.end(); ++itr)
        {
            if (itr->slotId > maxid) maxid = itr->slotId;
        }

        slot_id = maxid + 1;
    }

    newspell.slotId = slot_id;
    m_spells.push_back(newspell);
}


Mail* Player::GetMail(uint32 id)
{
    std::list<Mail*>::iterator itr;
    for (itr = m_mail.begin(); itr != m_mail.end(); itr++)
    {
        if ((*itr)->messageID == id)
        {
            return (*itr);
        }
    }
    return NULL;
}


//===================================================================================================================
//  Set Create Player Bits -- Sets bits required for creating a player in the updateMask.
//  Note:  Doesn't set Quest or Inventory bits
//  updateMask - the updatemask to hold the set bits
//===================================================================================================================
void Player::_SetCreateBits(UpdateMask *updateMask, Player *target) const
{
    if(target == this)
    {
        Object::_SetCreateBits(updateMask, target);
    }
    else
    {
        UpdateMask mask;
        mask.SetCount(m_valuesCount);
        _SetVisibleBits(&mask, target);

        for(uint16 index = 0; index < m_valuesCount; index++)
        {
            if(GetUInt32Value(index) != 0 && mask.GetBit(index))
                updateMask->SetBit(index);
        }
    }
}


void Player::_SetUpdateBits(UpdateMask *updateMask, Player *target) const
{
    if(target == this)
    {
        Object::_SetUpdateBits(updateMask, target);
    }
    else
    {
        UpdateMask mask;
        mask.SetCount(m_valuesCount);
        _SetVisibleBits(&mask, target);

        Object::_SetUpdateBits(updateMask, target);
        *updateMask &= mask;
    }
}


void Player::_SetVisibleBits(UpdateMask *updateMask, Player *target) const
{
    updateMask->SetBit(OBJECT_FIELD_GUID);
    updateMask->SetBit(OBJECT_FIELD_TYPE);
    updateMask->SetBit(OBJECT_FIELD_SCALE_X);

    updateMask->SetBit(UNIT_FIELD_SUMMON);
    updateMask->SetBit(UNIT_FIELD_SUMMON+1);

    updateMask->SetBit(UNIT_FIELD_TARGET);
    updateMask->SetBit(UNIT_FIELD_TARGET+1);

    updateMask->SetBit(UNIT_FIELD_HEALTH);
    updateMask->SetBit(UNIT_FIELD_POWER1);
    updateMask->SetBit(UNIT_FIELD_POWER2);
    updateMask->SetBit(UNIT_FIELD_POWER3);
    updateMask->SetBit(UNIT_FIELD_POWER4);
    updateMask->SetBit(UNIT_FIELD_POWER5);

    updateMask->SetBit(UNIT_FIELD_MAXHEALTH);
    updateMask->SetBit(UNIT_FIELD_MAXPOWER1);
    updateMask->SetBit(UNIT_FIELD_MAXPOWER2);
    updateMask->SetBit(UNIT_FIELD_MAXPOWER3);
    updateMask->SetBit(UNIT_FIELD_MAXPOWER4);
    updateMask->SetBit(UNIT_FIELD_MAXPOWER5);

    updateMask->SetBit(UNIT_FIELD_LEVEL);
    updateMask->SetBit(UNIT_FIELD_FACTIONTEMPLATE);
    updateMask->SetBit(UNIT_FIELD_BYTES_0);
    updateMask->SetBit(UNIT_FIELD_FLAGS);
    for(uint16 i = UNIT_FIELD_AURA; i < UNIT_FIELD_AURASTATE; i ++)
        updateMask->SetBit(i);
    updateMask->SetBit(UNIT_FIELD_BASEATTACKTIME);
    updateMask->SetBit(UNIT_FIELD_BASEATTACKTIME+1);
    updateMask->SetBit(UNIT_FIELD_BOUNDINGRADIUS);
    updateMask->SetBit(UNIT_FIELD_COMBATREACH);
    updateMask->SetBit(UNIT_FIELD_DISPLAYID);
    updateMask->SetBit(UNIT_FIELD_NATIVEDISPLAYID);
    updateMask->SetBit(UNIT_FIELD_MOUNTDISPLAYID);
    updateMask->SetBit(UNIT_FIELD_BYTES_1);
    updateMask->SetBit(UNIT_FIELD_MOUNTDISPLAYID);
    updateMask->SetBit(UNIT_FIELD_PETNUMBER);
    updateMask->SetBit(UNIT_FIELD_PET_NAME_TIMESTAMP);
    updateMask->SetBit(UNIT_DYNAMIC_FLAGS);

    updateMask->SetBit(PLAYER_BYTES);
    updateMask->SetBit(PLAYER_BYTES_2);
    updateMask->SetBit(PLAYER_BYTES_3);
    updateMask->SetBit(PLAYER_GUILD_TIMESTAMP);

    // for(uint16 i = 0; i < EQUIPMENT_SLOT_END; i++)
    for(uint16 i = 0; i < INVENTORY_SLOT_BAG_END; i++)
    {
        // lowguid
        updateMask->SetBit((uint16)(PLAYER_FIELD_INV_SLOT_HEAD + i*2));
        // highguid
        updateMask->SetBit((uint16)(PLAYER_FIELD_INV_SLOT_HEAD + (i*2) + 1));
        /* Naked Fix */
        /* updateMask->SetBit((uint16)(PLAYER_VISIBLE_ITEM_1_0 + (i*11))); // visual items for other players */
        // visual items for other players
        updateMask->SetBit((uint16)(PLAYER_VISIBLE_ITEM_1_0 + (i*12)));
    }
}


void Player::BuildCreateUpdateBlockForPlayer( UpdateData *data, Player *target ) const
{
    // for(int i = 0; i < EQUIPMENT_SLOT_END; i++)
    for(int i = 0; i < INVENTORY_SLOT_BAG_END; i++)
    {
        if(m_items[i] == NULL)
            continue;

        m_items[i]->BuildCreateUpdateBlockForPlayer( data, target );
    }

    if(target == this)
    {
        // for(int i = EQUIPMENT_SLOT_END; i < INVENTORY_SLOT_ITEM_END; i++)
        for(int i = EQUIPMENT_SLOT_END; i < BANK_SLOT_BAG_END; i++)
        {
            if(m_items[i] == NULL)
                continue;

            m_items[i]->BuildCreateUpdateBlockForPlayer( data, target );
        }
    }

    Unit::BuildCreateUpdateBlockForPlayer( data, target );
}


void Player::DestroyForPlayer( Player *target ) const
{
    Unit::DestroyForPlayer( target );

    // for(int i = 0; i < EQUIPMENT_SLOT_END; i++)
    for(int i = 0; i < INVENTORY_SLOT_BAG_END; i++)
    {
        if(m_items[i] == NULL)
            continue;

        m_items[i]->DestroyForPlayer( target );
    }

    if(target == this)
    {
        // for(int i = EQUIPMENT_SLOT_END; i < INVENTORY_SLOT_ITEM_END; i++)
        for(int i = EQUIPMENT_SLOT_END; i < BANK_SLOT_BAG_END; i++)
        {
            if(m_items[i] == NULL)
                continue;

            m_items[i]->DestroyForPlayer( target );
        }
    }
}


void Player::SaveToDB()
{
    if (m_dismountTimer != 0)
    {
        SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID , 0);
        RemoveFlag( UNIT_FIELD_FLAGS ,0x000004 );
        RemoveFlag( UNIT_FIELD_FLAGS, 0x002000 );
    }

    _RemoveAllItemMods();
    _RemoveAllAffectMods();

    bool inworld = IsInWorld();
    if (inworld)
        RemoveFromWorld();

    std::stringstream ss;
    ss << "DELETE FROM characters WHERE guid = " << GetGUIDLow();
    sDatabase.Execute( ss.str( ).c_str( ) );

    ss.rdbuf()->str("");
    ss << "INSERT INTO characters (guid, acct, name, mapId, zoneId, positionX, positionY, positionZ, orientation, data, taximask) VALUES ("
        << GetGUIDLow() << ", "                   // TODO: use full guids
        << GetSession()->GetAccountId() << ", '"
        << m_name << "', "
        << m_mapId << ", "
        << m_zoneId << ", "
        << m_positionX << ", "
        << m_positionY << ", "
        << m_positionZ << ", "
        << m_orientation << ", '";

    uint16 i;
    for( i = 0; i < m_valuesCount; i++ )
        ss << GetUInt32Value(i) << " ";

    ss << "', '";

    for( i = 0; i < 8; i++ )
        ss << m_taximask[i] << " ";

    ss << "' )";

    sDatabase.Execute( ss.str().c_str() );

    // TODO: equip all items and apply affects

    // Mail
    _SaveMail();

    // bids
    _SaveBids();

    // Auctions
    _SaveAuctions();

    // inventory
    _SaveInventory();

    // save quest progress
    _SaveQuestStatus();

    // spells
    _SaveSpells();

    // Action bar
    _SaveActions();

    // affects
    _SaveAffects();

    _ApplyAllAffectMods();
    _ApplyAllItemMods();

    if (inworld)
        AddToWorld();
}


void Player::_SaveQuestStatus()
{
    std::stringstream ss;
    ss << "DELETE FROM queststatus WHERE playerId = " << GetGUIDLow();
    sDatabase.Execute( ss.str().c_str() );

    for( StatusMap::iterator i = mQuestStatus.begin( ); i != mQuestStatus.end( ); ++ i )
    {
        std::stringstream ss2;
        ss2 << "INSERT INTO queststatus (playerId,questId,status,questMobCount1,questMobCount2,questMobCount3,questMobCount4,"
            << "questItemCount1,questItemCount2,questItemCount3,questItemCount4) VALUES "
            << "(" << GetGUIDLow() << ", "
            << i->first << ", "
            << i->second.status << ", "
            << i->second.m_questMobCount[0] << ", "
            << i->second.m_questMobCount[1] << ", "
            << i->second.m_questMobCount[2] << ", "
            << i->second.m_questMobCount[3] << ", "
            << i->second.m_questItemCount[0] << ", "
            << i->second.m_questItemCount[1] << ", "
            << i->second.m_questItemCount[2] << ", "
            << i->second.m_questItemCount[3]
            << ")";

        sDatabase.Execute( ss2.str().c_str() );
    }
}


void Player::_SaveInventory()
{
    std::stringstream delinvq;
    // TODO: use full guids
    delinvq << "DELETE FROM inventory WHERE player_guid = " << GetGUIDLow();
    sDatabase.Execute( delinvq.str().c_str( ) );

    // for(unsigned int i = 0; i < INVENTORY_SLOT_ITEM_END; i++)
    for(unsigned int i = 0; i < BANK_SLOT_BAG_END; i++)
    {
        if (m_items[i] != 0)
        {
            m_items[i]->SaveToDB();

            std::stringstream invq;
            invq <<  "INSERT INTO inventory (player_guid, slot, item_guid) VALUES ( " <<
                GetGUIDLow() << ", " << i << ", " << m_items[i]->GetGUIDLow() << " )";

            sDatabase.Execute( invq.str().c_str( ) );
        }
    }
}


void Player::_SaveSpells()
{
    std::stringstream query;
    // TODO: use full guids
    query << "DELETE FROM char_spells WHERE charId = " << GetGUIDLow();
    sDatabase.Execute( query.str().c_str() );

    std::list<struct spells>::iterator itr;
    for (itr = m_spells.begin(); itr != m_spells.end(); ++itr)
    {
        query.rdbuf()->str("");
        query << "INSERT INTO char_spells (charId,spellId,slotId) VALUES ( "
            << GetGUIDLow() << ", " << itr->spellId << ", " << itr->slotId << " )";

        sDatabase.Execute( query.str().c_str() );
    }
}


void Player::_SaveActions()
{
    std::stringstream query;
    query << "DELETE FROM char_actions WHERE charId=" << GetGUIDLow();
    sDatabase.Execute( query.str().c_str() );

    std::list<struct actions>::iterator itr;
    for (itr = m_actions.begin(); itr != m_actions.end(); ++itr)
    {
        query.rdbuf()->str("");
        query << "INSERT INTO char_actions (charId,button,action,type,misc) VALUES ( ";
        query << GetGUIDLow() << "," << int(itr->button) << "," << int(itr->action);
        query << "," << int(itr->type) << "," << int(itr->misc) << ")" << '\0';

        sDatabase.Execute( query.str().c_str() );
    }
}


void Player::_SaveAffects()
{
}


// NOTE: 32bit guids are only for compatibility with older bases.
void Player::LoadFromDB( uint32 guid )
{
    std::stringstream ss;
    ss << "SELECT * FROM characters WHERE guid=" << guid;

    QueryResult *result = sDatabase.Query( ss.str().c_str() );
    ASSERT(result);

    Field *fields = result->Fetch();

    Object::_Create( guid, HIGHGUID_PLAYER );

    LoadValues( fields[2].GetString() );

    // TODO: check for overflow
    m_name = fields[3].GetString();

    m_positionX = fields[4].GetFloat();
    m_positionY = fields[5].GetFloat();
    m_positionZ = fields[6].GetFloat();
    m_mapId = fields[7].GetUInt32();
    m_zoneId = fields[8].GetUInt32();
    m_orientation = fields[9].GetFloat();

    ZoneIDMap.SetZoneBitOn(m_zoneId);

    if( HasFlag(PLAYER_FLAGS, 0x10) )
        m_deathState = DEAD;

    LoadTaxiMask( fields[10].GetString() );

    delete result;

/*
    m_outfitId = atoi( row[ 11 ] );
    m_guildId = atoi( row[ 17 ] );
    m_petInfoId = atoi( row[ 18 ] );
    m_petLevel = atoi( row[ 19 ] );
    m_petFamilyId = atoi( row[ 20 ] );
*/

    _LoadMail();

    _LoadInventory();

    _LoadSpells();

    _LoadActions();

    _LoadQuestStatus();

    _LoadBids();

    _LoadAffects();

    _ApplyAllAffectMods();
    _ApplyAllItemMods();
}


void Player::_LoadInventory()
{
    // Clean current inventory
    // for(uint16 i = 0; i < INVENTORY_SLOT_ITEM_END; i++)
    for(uint16 i = 0; i < BANK_SLOT_BAG_END; i++)
    {
        if(m_items[i])
        {
            delete m_items[i];

            SetUInt64Value((uint16)(PLAYER_FIELD_INV_SLOT_HEAD + i*2), 0);
            m_items[i] = 0;
        }
    }

    // Inventory
    std::stringstream invq;
    invq << "SELECT item_guid, slot FROM inventory WHERE player_guid=" << GetGUIDLow();

    QueryResult *result = sDatabase.Query( invq.str().c_str() );
    if(result)
    {
        do
        {
            Field *fields = result->Fetch();

            Item* item = new Item;
            item->LoadFromDB(fields[0].GetUInt32(),1);

            AddItemToSlot( (uint8)fields[1].GetUInt16(), item);
        }
        while( result->NextRow() );

        delete result;
    }
}


bool Player::HasSpell(uint32 spell)
{
    std::list<struct spells>::iterator itr;
    for (itr = m_spells.begin(); itr != m_spells.end(); ++itr)
    {
        if (itr->spellId == spell)
        {
            return true;
        }
    }
    return false;

}


void Player::_LoadSpells()
{
    // Clear spell list
    m_spells.clear();

    // Spells
    std::stringstream query;
    query << "SELECT spellId, slotId FROM char_spells WHERE charId=" << GetGUIDLow();

    QueryResult *result = sDatabase.Query( query.str().c_str() );
    if(result)
    {
        do
        {
            Field *fields = result->Fetch();

            addSpell(fields[0].GetUInt16(), fields[1].GetUInt16());
        }
        while( result->NextRow() );

        delete result;
    }
}


void Player::_LoadActions()
{
    // Clear Actions List
    m_actions.clear();

    // Actions
    std::stringstream query;
    query << "SELECT * FROM char_actions WHERE charId=" << GetGUIDLow() << " ORDER BY button";
    QueryResult *result = sDatabase.Query( query.str().c_str() );
    if(result)
    {
        do
        {
            Field *fields = result->Fetch();

            addAction(fields[1].GetUInt8(), fields[2].GetUInt16(), fields[3].GetUInt8(), fields[4].GetUInt8());
        }
        while( result->NextRow() );

        delete result;
    }
}


void Player::_LoadQuestStatus()
{
    // clear list
    mQuestStatus.clear();

    std::stringstream ss;
    ss << "SELECT * FROM queststatus WHERE playerId=" << GetGUIDLow();

    QueryResult *result = sDatabase.Query( ss.str().c_str() );
    if(result)
    {
        do
        {
            Field *fields = result->Fetch();

            quest_status qs;
            qs.quest_id = fields[1].GetUInt32();
            qs.status = fields[2].GetUInt32();
            qs.m_questMobCount[0] = fields[3].GetUInt32();
            qs.m_questMobCount[1] = fields[4].GetUInt32();
            qs.m_questMobCount[2] = fields[5].GetUInt32();
            qs.m_questMobCount[3] = fields[6].GetUInt32();
            qs.m_questItemCount[0] = fields[7].GetUInt32();
            qs.m_questItemCount[1] = fields[8].GetUInt32();
            qs.m_questItemCount[2] = fields[9].GetUInt32();
            qs.m_questItemCount[3] = fields[10].GetUInt32();

            loadExistingQuest(qs);
        }
        while( result->NextRow() );

        delete result;
    }
}


void Player::_LoadAffects()
{
}


void Player::DeleteFromDB()
{
    std::stringstream ss;

    ss << "DELETE FROM characters WHERE guid = " << GetGUIDLow();
    sDatabase.Execute( ss.str( ).c_str( ) );

    ss.rdbuf()->str("");
    ss << "DELETE FROM char_spells WHERE charid = " << GetGUIDLow();
    sDatabase.Execute( ss.str( ).c_str( ) );

    ss.rdbuf()->str("");
    ss << "DELETE FROM inventory WHERE player_guid = " << GetGUIDLow();
    sDatabase.Execute( ss.str( ).c_str( ) );

    // for(int i = 0; i < EQUIPMENT_SLOT_END; i++)
    for(int i = 0; i < BANK_SLOT_ITEM_END; i++)
    {
        if(m_items[i] == NULL)
            continue;

        m_items[i]->DeleteFromDB();
    }

    ss.rdbuf()->str("");
    ss << "DELETE FROM queststatus WHERE playerId = " << GetGUIDLow();
    sDatabase.Execute( ss.str( ).c_str( ) );

    ss.rdbuf()->str("");
    ss << "DELETE FROM char_actions WHERE charId = " << GetGUIDLow();
    sDatabase.Execute( ss.str( ).c_str( ) );
}


uint8 Player::FindFreeItemSlot(uint32 type)
{
    switch(type)
    {
        case INVTYPE_NON_EQUIP:
            return INVENTORY_SLOT_ITEM_END;       // ???
        case INVTYPE_HEAD:
        {
            if (!GetItemBySlot(EQUIPMENT_SLOT_HEAD))
                return EQUIPMENT_SLOT_HEAD;
            else
                return INVENTORY_SLOT_ITEM_END;
        }
        case INVTYPE_NECK:
        {
            if (!GetItemBySlot(EQUIPMENT_SLOT_NECK))
                return EQUIPMENT_SLOT_NECK;
            else
                return INVENTORY_SLOT_ITEM_END;
        }
        case INVTYPE_SHOULDERS:
        {
            if (!GetItemBySlot(EQUIPMENT_SLOT_SHOULDERS))
                return EQUIPMENT_SLOT_SHOULDERS;
            else
                return INVENTORY_SLOT_ITEM_END;
        }
        case INVTYPE_BODY:
        {
            if (!GetItemBySlot(EQUIPMENT_SLOT_BODY))
                return EQUIPMENT_SLOT_BODY;
            else
                return INVENTORY_SLOT_ITEM_END;
        }
        case INVTYPE_CHEST:
        {
            if (!GetItemBySlot(EQUIPMENT_SLOT_CHEST))
                return EQUIPMENT_SLOT_CHEST;
            else
                return INVENTORY_SLOT_ITEM_END;
        }
        case INVTYPE_ROBE:                        // ???
        {
            if (!GetItemBySlot(EQUIPMENT_SLOT_CHEST))
                return EQUIPMENT_SLOT_CHEST;
            else
                return INVENTORY_SLOT_ITEM_END;
        }
        case INVTYPE_WAIST:
        {
            if (!GetItemBySlot(EQUIPMENT_SLOT_WAIST))
                return EQUIPMENT_SLOT_WAIST;
            else
                return INVENTORY_SLOT_ITEM_END;
        }
        case INVTYPE_LEGS:
        {
            if (!GetItemBySlot(EQUIPMENT_SLOT_LEGS))
                return EQUIPMENT_SLOT_LEGS;
            else
                return INVENTORY_SLOT_ITEM_END;
        }
        case INVTYPE_FEET:
        {
            if (!GetItemBySlot(EQUIPMENT_SLOT_FEET))
                return EQUIPMENT_SLOT_FEET;
            else
                return INVENTORY_SLOT_ITEM_END;
        }
        case INVTYPE_WRISTS:
        {
            if (!GetItemBySlot(EQUIPMENT_SLOT_WRISTS))
                return EQUIPMENT_SLOT_WRISTS;
            else
                return INVENTORY_SLOT_ITEM_END;
        }
        case INVTYPE_HANDS:
        {
            if (!GetItemBySlot(EQUIPMENT_SLOT_HANDS))
                return EQUIPMENT_SLOT_HANDS;
            else
                return INVENTORY_SLOT_ITEM_END;
        }
        case INVTYPE_FINGER:
        {
            if (!GetItemBySlot(EQUIPMENT_SLOT_FINGER1))
                return EQUIPMENT_SLOT_FINGER1;
            else if (!GetItemBySlot(EQUIPMENT_SLOT_FINGER2))
                return EQUIPMENT_SLOT_FINGER2;
            else
                return INVENTORY_SLOT_ITEM_END;
        }
        case INVTYPE_TRINKET:
        {
            if (!GetItemBySlot(EQUIPMENT_SLOT_TRINKET1))
                return EQUIPMENT_SLOT_TRINKET1;
            else if (!GetItemBySlot(EQUIPMENT_SLOT_TRINKET2))
                return EQUIPMENT_SLOT_TRINKET2;
            else
                return INVENTORY_SLOT_ITEM_END;
        }
        case INVTYPE_CLOAK:
        {
            if (!GetItemBySlot(EQUIPMENT_SLOT_BACK))
                return EQUIPMENT_SLOT_BACK;
            else
                return INVENTORY_SLOT_ITEM_END;
        }
        case INVTYPE_WEAPON:
        {
            if (!GetItemBySlot(EQUIPMENT_SLOT_MAINHAND) )
                return EQUIPMENT_SLOT_MAINHAND;
            else if(!GetItemBySlot(EQUIPMENT_SLOT_OFFHAND))
                return EQUIPMENT_SLOT_OFFHAND;
            else
                return INVENTORY_SLOT_ITEM_END;
        }
        case INVTYPE_SHIELD:
        {
            if (!GetItemBySlot(EQUIPMENT_SLOT_OFFHAND))
                return EQUIPMENT_SLOT_OFFHAND;
            else
                return INVENTORY_SLOT_ITEM_END;
        }
        case INVTYPE_RANGED:
        {
            if (!GetItemBySlot(EQUIPMENT_SLOT_RANGED))
                return EQUIPMENT_SLOT_RANGED;
            else
                return INVENTORY_SLOT_ITEM_END;
        }
        case INVTYPE_2HWEAPON:
        {
            if (!GetItemBySlot(EQUIPMENT_SLOT_MAINHAND))
                return EQUIPMENT_SLOT_MAINHAND;
            else
                return INVENTORY_SLOT_ITEM_END;
        }
        case INVTYPE_TABARD:
        {
            if (!GetItemBySlot(EQUIPMENT_SLOT_TABARD))
                return EQUIPMENT_SLOT_TABARD;
            else
                return INVENTORY_SLOT_ITEM_END;
        }
        case INVTYPE_WEAPONMAINHAND:
        {
            if (!GetItemBySlot(EQUIPMENT_SLOT_MAINHAND))
                return EQUIPMENT_SLOT_MAINHAND;
            else if(!GetItemBySlot(EQUIPMENT_SLOT_OFFHAND))
                return EQUIPMENT_SLOT_OFFHAND;
            else
                return INVENTORY_SLOT_ITEM_END;
        }
        case INVTYPE_WEAPONOFFHAND:
        {
            if (!GetItemBySlot(EQUIPMENT_SLOT_OFFHAND))
                return EQUIPMENT_SLOT_OFFHAND;
            else
                return INVENTORY_SLOT_ITEM_END;
        }
        case INVTYPE_HOLDABLE:
        {
            if (!GetItemBySlot(EQUIPMENT_SLOT_MAINHAND))
                return EQUIPMENT_SLOT_MAINHAND;
            else
                return INVENTORY_SLOT_ITEM_END;
        }
        case INVTYPE_AMMO:
            return EQUIPMENT_SLOT_RANGED;         // ?
        case INVTYPE_THROWN:
            return EQUIPMENT_SLOT_RANGED;         // ?
        case INVTYPE_RANGEDRIGHT:
            return EQUIPMENT_SLOT_RANGED;         // ?
        case INVTYPE_BAG:
        {
            for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; i++)
            {
                if (!GetItemBySlot(i))
                    return i;
            }
            return INVENTORY_SLOT_ITEM_END;
        }
        default:
            ASSERT(0);
            return INVENTORY_SLOT_ITEM_END;
    }
}


uint8 Player::CanEquipItemInSlot(uint8 slot, ItemPrototype *proto)
{
    uint32 type=proto->InventoryType;

    // Check to see if we have the correct race
    if(!(proto->AllowableRace& (1<<getRace())))
        return 10;

    // Check to see if we have the correct class
    if(!(proto->AllowableClass& (1<<getClass())))
        return 10;

    // Check to see if we have the correct level.
    if(proto->RequiredLevel>GetUInt32Value(UNIT_FIELD_LEVEL))
        return 1;
    // Check to see if we have the required skill level.
    bool gotSkillz=false;
    for(int i=1;(i<384)&&(!gotSkillz);i+=2)
        // I'm guessing this is the way to handle skillz
        if((proto->RequiredSkill==GetUInt32Value(PLAYER_SKILL_INFO_1_1+i))&&
        (proto->RequiredSkillRank==GetUInt32Value(PLAYER_SKILL_INFO_1_1+i+1)))
            gotSkillz=true;

    if(!gotSkillz)
        return 2;

    // You are dead !
    if(m_deathState == DEAD)
        return 9;

    switch(slot)
    {
        case EQUIPMENT_SLOT_HEAD:
        {
            if(type == INVTYPE_HEAD)
                return 0;
            else
                return 3;
        }
        case EQUIPMENT_SLOT_NECK:
        {
            if(type == INVTYPE_NECK)
                return 0;
            else
                return 3;
        }
        case EQUIPMENT_SLOT_SHOULDERS:
        {
            if(type == INVTYPE_SHOULDERS)
                return 0;
            else
                return 3;
        }
        case EQUIPMENT_SLOT_BODY:
        {
            if(type == INVTYPE_BODY)
                return 0;
            else
                return 3;
        }
        case EQUIPMENT_SLOT_CHEST:
        {
            if(type == INVTYPE_CHEST || type == INVTYPE_ROBE)
                return 0;
            else
                return 3;
        }
        case EQUIPMENT_SLOT_WAIST:
        {
            if(type == INVTYPE_WAIST)
                return 0;
            else
                return 3;
        }
        case EQUIPMENT_SLOT_LEGS:
        {
            if(type == INVTYPE_LEGS)
                return 0;
            else
                return 3;
        }
        case EQUIPMENT_SLOT_FEET:
        {
            if(type == INVTYPE_FEET)
                return 0;
            else
                return 3;
        }
        case EQUIPMENT_SLOT_WRISTS:
        {
            if(type == INVTYPE_WRISTS)
                return 0;
            else
                return 3;
        }
        case EQUIPMENT_SLOT_HANDS:
        {
            if(type == INVTYPE_HANDS)
                return 0;
            else
                return 3;
        }
        case EQUIPMENT_SLOT_FINGER1:
        case EQUIPMENT_SLOT_FINGER2:
        {
            if(type == INVTYPE_FINGER)
                return 0;
            else
                return 3;
        }
        case EQUIPMENT_SLOT_TRINKET1:
        case EQUIPMENT_SLOT_TRINKET2:
        {
            if(type == INVTYPE_TRINKET)
                return 0;
            else
                return 3;
        }
        case EQUIPMENT_SLOT_BACK:
        {
            if(type == INVTYPE_CLOAK)
                return 0;
            else
                return 3;
        }
        case EQUIPMENT_SLOT_MAINHAND:
        {
            if(type == INVTYPE_WEAPON || type == INVTYPE_WEAPONMAINHAND || type == INVTYPE_HOLDABLE ||
                (type == INVTYPE_2HWEAPON && !GetItemBySlot(EQUIPMENT_SLOT_OFFHAND)))
                return 0;
            else
                return 12;
        }
        case EQUIPMENT_SLOT_OFFHAND:
        {
            if((type == INVTYPE_WEAPON || type == INVTYPE_SHIELD || type == INVTYPE_WEAPONOFFHAND)&& (GetItemBySlot(EQUIPMENT_SLOT_MAINHAND)))
                if(GetItemBySlot(EQUIPMENT_SLOT_MAINHAND)->GetProto()->InventoryType != INVTYPE_2HWEAPON)
                // Check for dualWielding skill
                {
                    // Not Yet
                    // if(getSkill(SKILL_DUAL_WIELDING))
                    return 0;
                    // else return 13;
                }
                else
                    return 12;
        }
        case EQUIPMENT_SLOT_RANGED:
        {
            if(type == INVTYPE_AMMO || type == INVTYPE_THROWN || type == INVTYPE_RANGEDRIGHT)
                return 0;
            else
                return 6;
        }
        case EQUIPMENT_SLOT_TABARD:
        {
            if(type == INVTYPE_TABARD)
                return 0;
            else
                return 6;
        }
        case INVENTORY_SLOT_BAG_1:
        case INVENTORY_SLOT_BAG_2:
        case INVENTORY_SLOT_BAG_3:
        case INVENTORY_SLOT_BAG_4:
        {
            if(type == INVTYPE_BAG)
                return 0;
            else
                return 29;
        }
        case INVENTORY_SLOT_ITEM_1:
        case INVENTORY_SLOT_ITEM_2:
        case INVENTORY_SLOT_ITEM_3:
        case INVENTORY_SLOT_ITEM_4:
        case INVENTORY_SLOT_ITEM_5:
        case INVENTORY_SLOT_ITEM_6:
        case INVENTORY_SLOT_ITEM_7:
        case INVENTORY_SLOT_ITEM_8:
        case INVENTORY_SLOT_ITEM_9:
        case INVENTORY_SLOT_ITEM_10:
        case INVENTORY_SLOT_ITEM_11:
        case INVENTORY_SLOT_ITEM_12:
        case INVENTORY_SLOT_ITEM_13:
        case INVENTORY_SLOT_ITEM_14:
        case INVENTORY_SLOT_ITEM_15:
        case INVENTORY_SLOT_ITEM_16:
        {
            return 0;
        }
        default:
            return 19;
    }
}


void Player::SwapItemSlots(uint8 srcslot, uint8 dstslot)
{
    Log::getSingleton().outError("SwapItemSlotsStart");
/*
    ASSERT(srcslot < INVENTORY_SLOT_ITEM_END);
    ASSERT(dstslot < INVENTORY_SLOT_ITEM_END);
*/
    ASSERT(srcslot < BANK_SLOT_BAG_END);
    ASSERT(dstslot < BANK_SLOT_BAG_END);
/*
    Item *temp;
    temp = m_items[srcslot];
    m_items[srcslot] = m_items[dstslot];
    m_items[dstslot] = temp;
*/

    Item* Item1 = RemoveItemFromSlot(srcslot);
    Item* Item2 = RemoveItemFromSlot(dstslot);
    if (Item2 != NULL) AddItemToSlot(srcslot, Item2);
    if (Item1 != NULL) AddItemToSlot(dstslot, Item1);
    Log::getSingleton().outError("SwapItemSlotsMid");
    if ( IsInWorld() )
    {

        // if ( srcslot < EQUIPMENT_SLOT_END && dstslot >= EQUIPMENT_SLOT_END )
        if ( srcslot < INVENTORY_SLOT_BAG_END && dstslot >= INVENTORY_SLOT_BAG_END )
        {
            for(Object::InRangeSet::iterator i = GetInRangeSetBegin();
                i != GetInRangeSetEnd(); i++)
            {
                if((*i)->GetTypeId() == TYPEID_PLAYER)
                    m_items[dstslot]->DestroyForPlayer( (Player*)*i );
            }
        }
        /* else if ( srcslot >= EQUIPMENT_SLOT_END && dstslot < EQUIPMENT_SLOT_END ) */
        else if ( srcslot >= INVENTORY_SLOT_BAG_END && dstslot < INVENTORY_SLOT_BAG_END )
        {
            UpdateData upd;
            WorldPacket packet;

            for(Object::InRangeSet::iterator i = GetInRangeSetBegin();
                i != GetInRangeSetEnd(); i++)
            {
                if((*i)->GetTypeId() == TYPEID_PLAYER)
                {
                    upd.Clear();
                    m_items[dstslot]->BuildCreateUpdateBlockForPlayer( &upd, (Player*)*i );
                    upd.BuildPacket( &packet );
                    GetSession()->SendPacket( &packet );
                }
            }
        }
    }

/*
     SetUInt64Value( (uint16)(PLAYER_FIELD_INV_SLOT_HEAD  + (dstslot*2)),
         m_items[dstslot] ? m_items[dstslot]->GetGUID() : 0 );
     SetUInt64Value( (uint16)(PLAYER_FIELD_INV_SLOT_HEAD  + (srcslot*2)),
         m_items[srcslot] ? m_items[srcslot]->GetGUID() : 0 );

     if (srcslot >= EQUIPMENT_SLOT_END && dstslot < EQUIPMENT_SLOT_END)
        _ApplyItemMods(m_items[dstslot], true);
*/
    Log::getSingleton().outError("SwapItemSlotsEnd");
}


uint32 Player::GetSlotByItemID(uint32 ID)
{
    for(uint32 i=INVENTORY_SLOT_ITEM_START;i<INVENTORY_SLOT_ITEM_END;i++)
    {
        if(m_items[i] != 0)
            if(m_items[i]->GetProto()->ItemId == ID)
                return i;
    }
    return 0;
}


uint32 Player::GetSlotByItemGUID(uint64 guid)
{
    for(uint32 i=0;i<INVENTORY_SLOT_ITEM_END;i++)
    {
        if(m_items[i] != 0)
            if(m_items[i]->GetGUID() == guid)
                return i;
    }
    return 0;

}


void Player::AddItemToSlot(uint8 slot, Item *item)
{
    /* ASSERT(slot < INVENTORY_SLOT_ITEM_END); */
    Log::getSingleton().outError("AddItemtoSlot");
    ASSERT(slot < BANK_SLOT_BAG_END);
    ASSERT(m_items[slot] == NULL);

    if( IsInWorld() )
    {
        UpdateData upd;
        WorldPacket packet;

        // create for ourselves
        item->BuildCreateUpdateBlockForPlayer( &upd, this );
        upd.BuildPacket( &packet );
        GetSession()->SendPacket( &packet );

        // if ( slot < EQUIPMENT_SLOT_END )
        if ( slot < INVENTORY_SLOT_BAG_END )
        {
            for(Object::InRangeSet::iterator i = GetInRangeSetBegin();
                i != GetInRangeSetEnd(); i++)
            {
                if((*i)->GetTypeId() == TYPEID_PLAYER)
                {
                    upd.Clear();
                    item->BuildCreateUpdateBlockForPlayer( &upd, (Player*)*i );
                    upd.BuildPacket( &packet );
                    GetSession()->SendPacket( &packet );
                }
            }
        }

    }

    m_items[slot] = item;
    SetUInt64Value( (uint16)(PLAYER_FIELD_INV_SLOT_HEAD  + (slot*2)), m_items[slot] ? m_items[slot]->GetGUID() : 0 );

    item->SetOwner( this );
    item->PlaceOnMap();

    if ( slot < EQUIPMENT_SLOT_END )
    {
        /* Naked Fix */
        // int VisibleBase = PLAYER_VISIBLE_ITEM_1_0 + (slot * 11);
        int VisibleBase = PLAYER_VISIBLE_ITEM_1_0 + (slot * 12);
        SetUInt32Value(VisibleBase, item->GetUInt32Value(OBJECT_FIELD_ENTRY));
        SetUInt32Value(VisibleBase + 1, item->GetUInt32Value(ITEM_FIELD_ENCHANTMENT));
        SetUInt32Value(VisibleBase + 2, item->GetUInt32Value(ITEM_FIELD_ENCHANTMENT + 3));
        SetUInt32Value(VisibleBase + 3, item->GetUInt32Value(ITEM_FIELD_ENCHANTMENT + 6));
        SetUInt32Value(VisibleBase + 4, item->GetUInt32Value(ITEM_FIELD_ENCHANTMENT + 9));
        SetUInt32Value(VisibleBase + 5, item->GetUInt32Value(ITEM_FIELD_ENCHANTMENT + 12));
        SetUInt32Value(VisibleBase + 6, item->GetUInt32Value(ITEM_FIELD_ENCHANTMENT + 15));
        SetUInt32Value(VisibleBase + 7, item->GetUInt32Value(ITEM_FIELD_ENCHANTMENT + 18));
        SetUInt32Value(VisibleBase + 8, item->GetUInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID));
        // SetUInt32Value(VisibleBase + 9, 0);
        // SetUInt32Value(VisibleBase + 10, GetGUID());
        _ApplyItemMods( item,slot, true );
    }
}


Item* Player::RemoveItemFromSlot(uint8 slot)
{
    /* ASSERT(slot < INVENTORY_SLOT_ITEM_END); */
    Log::getSingleton().outError("RemoveItemFromSlot");
    ASSERT(slot < BANK_SLOT_BAG_END);

    Log::getSingleton().outError("RemoveItemFromSlot %u", slot);
    Item *item = m_items[slot];
    if (item == NULL) return NULL;
    Log::getSingleton().outError("RemoveItemFromSlot %u",item->GetGUID());
    m_items[slot] = NULL;
    SetUInt64Value( (uint16)(PLAYER_FIELD_INV_SLOT_HEAD  + (slot*2)), 0 );
    Log::getSingleton().outError("RemoveItemFromSlotINVHEAD");
    if ( slot < EQUIPMENT_SLOT_END )
    {
        _ApplyItemMods( item,slot, false );
        // Clear all the 'visible item' fields
        /* Naked fix */
/*
        int VisibleBase = PLAYER_VISIBLE_ITEM_1_0 + (slot * 11);
        for (int i = VisibleBase; i < VisibleBase + 11; ++i)
*/
        int VisibleBase = PLAYER_VISIBLE_ITEM_1_0 + (slot * 12);
        for (int i = VisibleBase; i < VisibleBase + 12; ++i)
            SetUInt32Value(i, 0);
    }
    Log::getSingleton().outError("RemoveItemFromSlotSETOWNER");
    item->SetOwner( NULL );

    if ( IsInWorld() )
    {
        item->RemoveFromMap();

        // create for ourselves
        item->DestroyForPlayer( this );

        // if ( slot < EQUIPMENT_SLOT_END )
        if ( slot < INVENTORY_SLOT_BAG_END )
        {
            for(Object::InRangeSet::iterator i = GetInRangeSetBegin();
                i != GetInRangeSetEnd(); i++)
            {
                if((*i)->GetTypeId() == TYPEID_PLAYER)
                    item->DestroyForPlayer( (Player*)*i );
            }
        }
    }
    Log::getSingleton().outError("RemoveItemFromSlotEND");
    return item;
}


void Player::AddToWorld()
{
    Object::AddToWorld();

    // for(int i = 0; i < INVENTORY_SLOT_ITEM_END; i++)
    for(int i = 0; i < BANK_SLOT_BAG_END; i++)
    {
        if(m_items[i])
            m_items[i]->AddToWorld();
    }
}


void Player::RemoveFromWorld()
{
    // for(int i = 0; i < INVENTORY_SLOT_ITEM_END; i++)
    for(int i = 0; i < BANK_SLOT_BAG_END; i++)
    {
        if(m_items[i])
            m_items[i]->RemoveFromWorld();
    }

    Object::RemoveFromWorld();
}


// TODO: perhaps item should just have a list of mods, that will simplify code
void Player::_ApplyItemMods(Item *item, uint8 slot,bool apply)
{
    ASSERT(item);
    ItemPrototype *proto = item->GetProto();
    if (apply)
    {
        Log::getSingleton().outString("applying mods for item %u ",item->GetGUIDLow());
    }
    else
    {
        Log::getSingleton().outString("removing mods for item %u ",item->GetGUIDLow());
    }
    // FIXED: ? Was missing armor and holy resistance
   if (proto->Armor)
        SetUInt32Value(UNIT_FIELD_ARMOR, GetUInt32Value(UNIT_FIELD_ARMOR) +
        (apply ? proto->Armor : -(int32)proto->Armor));
	if (proto->HolyRes)
        SetUInt32Value(UNIT_FIELD_RESISTANCES_01, GetUInt32Value(UNIT_FIELD_RESISTANCES_01) +
        (apply ? proto->HolyRes : -(int32)proto->HolyRes));
    if (proto->FireRes)
        SetUInt32Value(UNIT_FIELD_RESISTANCES_02, GetUInt32Value(UNIT_FIELD_RESISTANCES_02) +
        (apply ? proto->FireRes : -(int32)proto->FireRes));
    if (proto->NatureRes)
        SetUInt32Value(UNIT_FIELD_RESISTANCES_03, GetUInt32Value(UNIT_FIELD_RESISTANCES_03) +
        (apply ? proto->NatureRes : -(int32)proto->NatureRes));
    if (proto->FrostRes)
        SetUInt32Value(UNIT_FIELD_RESISTANCES_04, GetUInt32Value(UNIT_FIELD_RESISTANCES_04) +
        (apply ? proto->FrostRes : -(int32)proto->FrostRes));
    if (proto->ShadowRes)
        SetUInt32Value(UNIT_FIELD_RESISTANCES_05, GetUInt32Value(UNIT_FIELD_RESISTANCES_05) +
        (apply ? proto->ShadowRes : -(int32)proto->ShadowRes));
	 if (proto->ArcaneRes)
        SetUInt32Value(UNIT_FIELD_RESISTANCES_06, GetUInt32Value(UNIT_FIELD_RESISTANCES_06) +
        (apply ? proto->ArcaneRes : -(int32)proto->ArcaneRes));

    uint8 MINDAMAGEFIELD=(slot==EQUIPMENT_SLOT_OFFHAND)?UNIT_FIELD_MINOFFHANDDAMAGE:UNIT_FIELD_MINDAMAGE;
    uint8 MAXDAMAGEFIELD=(slot==EQUIPMENT_SLOT_OFFHAND)?UNIT_FIELD_MAXOFFHANDDAMAGE:UNIT_FIELD_MAXDAMAGE;

    if (proto->DamageMin[0])
    {
        SetFloatValue(MINDAMAGEFIELD, GetFloatValue(MINDAMAGEFIELD) +
            (apply ? proto->DamageMin[0] : -proto->DamageMin[0]));
        if (apply)
        {
            Log::getSingleton().outString("adding %f mindam ",proto->DamageMin[0]);
        }
        else
        {
            Log::getSingleton().outString("removing %f mindamn ",proto->DamageMin[0]);
        }

    }
    if (proto->DamageMax[0])
    {
        SetFloatValue(MAXDAMAGEFIELD, GetFloatValue(MAXDAMAGEFIELD) +
            (apply ? proto->DamageMax[0] : -proto->DamageMax[0]));
        if (apply)
        {
            Log::getSingleton().outString("adding %f maxdam ",proto->DamageMax[0]);
        }
        else
        {
            Log::getSingleton().outString("removing %f maxdam ",proto->DamageMax[0]);
        }
    }
    if (proto->Delay)
    {
        if(slot!=EQUIPMENT_SLOT_OFFHAND)
            SetUInt32Value(UNIT_FIELD_BASEATTACKTIME, apply ? proto->Delay : 2000);
        else SetUInt32Value(UNIT_FIELD_BASEATTACKTIME + 1, apply ? proto->Delay : 2000);
    }
}


void Player::_RemoveAllItemMods()
{
    // for (int i = 0; i < EQUIPMENT_SLOT_END; i++)
    for (int i = 0; i < INVENTORY_SLOT_BAG_END; i++)
    {
        if(m_items[i])
            _ApplyItemMods(m_items[i],i, false);
    }
}


void Player::_ApplyAllItemMods()
{
    // for (int i = 0; i < EQUIPMENT_SLOT_END; i++)
    for (int i = 0; i < INVENTORY_SLOT_BAG_END; i++)
    {
        if(m_items[i])
            _ApplyItemMods(m_items[i],i, true);
    }
}


void Player::SetMovement(uint8 pType)
{
    WorldPacket data;

    switch(pType)
    {
        case MOVE_ROOT:
        {
            data.Initialize(SMSG_FORCE_MOVE_ROOT);
            data << GetGUID();
            GetSession()->SendPacket( &data );
        }break;
        case MOVE_UNROOT:
        {
            data.Initialize(SMSG_FORCE_MOVE_UNROOT);
            data << GetGUID();
            GetSession()->SendPacket( &data );
        }break;
        case MOVE_WATER_WALK:
        {
            data.Initialize(SMSG_MOVE_WATER_WALK);
            data << GetGUID();
            GetSession()->SendPacket( &data );
        }break;
        case MOVE_LAND_WALK:
        {
            data.Initialize(SMSG_MOVE_LAND_WALK);
            data << GetUInt32Value( OBJECT_FIELD_GUID );
            GetSession()->SendPacket( &data );
        }break;
        default:break;
    }
}


void Player::SetPlayerSpeed(uint8 SpeedType, float value, bool forced)
{
    WorldPacket data;

    switch(SpeedType)
    {
        case RUN:
        {
            if(forced) { data.Initialize(SMSG_FORCE_RUN_SPEED_CHANGE); }
            else { data.Initialize(MSG_MOVE_SET_RUN_SPEED); }
            data << GetGUID();
            data << float(value);
            GetSession()->SendPacket( &data );
        }break;
        case RUNBACK:
        {
            if(forced) { data.Initialize(SMSG_FORCE_RUN_BACK_SPEED_CHANGE); }
            else { data.Initialize(MSG_MOVE_SET_RUN_BACK_SPEED); }
            data << GetGUID();
            data << float(value);
            GetSession()->SendPacket( &data );
        }break;
        case SWIM:
        {
            if(forced) { data.Initialize(SMSG_FORCE_SWIM_SPEED_CHANGE); }
            else { data.Initialize(MSG_MOVE_SET_SWIM_SPEED); }
            data << GetGUID();
            data << float(value);
            GetSession()->SendPacket( &data );
        }break;
        case SWIMBACK:
        {
            data.Initialize(MSG_MOVE_SET_SWIM_BACK_SPEED);
            data << GetGUID();
            data << float(value);
            GetSession()->SendPacket( &data );
        }break;
        default:break;
    }
}


void Player::BuildPlayerRepop()
{
    WorldPacket data;
    // 1.1.1
    SetUInt32Value( UNIT_FIELD_HEALTH, 1 );

    SetMovement(MOVE_UNROOT);
    SetMovement(MOVE_WATER_WALK);

    SetPlayerSpeed(RUN, (float)8.5, true);
    SetPlayerSpeed(SWIM, (float)5.9, true);

    data.Initialize(SMSG_CORPSE_RECLAIM_DELAY );
    data << uint8(0x30) << uint8(0x75) << uint8(0x00) << uint8(0x00);
    GetSession()->SendPacket( &data );

    data.Initialize(SMSG_SPELL_START );
    data << GetGUID() << GetGUID() << uint32(8326);
    data << uint16(0) << uint32(0) << uint16(0x02) << uint32(0) << uint32(0);
    GetSession()->SendPacket( &data );

    data.Initialize(SMSG_UPDATE_AURA_DURATION);
    data << uint8(32);
    data << uint32(0);
    GetSession()->SendPacket( &data );

    data.Initialize(SMSG_CAST_RESULT);
    data << uint32(8326);
    data << uint8(0x00);
    GetSession()->SendPacket( &data );

    data.Initialize(SMSG_SPELL_GO);
    data << GetGUID() << GetGUID() << uint32(8326);
    data << uint16(01) << uint8(0) << uint8(0);
    data << uint16(0040);
    data << GetPositionX();
    data << GetPositionY();
    data << GetPositionZ();
    GetSession()->SendPacket( &data );

    data.Initialize(SMSG_SPELLLOGEXECUTE);
    data << (uint32)GetGUID() << (uint32)GetGUID();
    data << uint32(8326);
    data << uint32(1);
    data << uint32(0x24);
    data << uint32(1);
    data << GetGUID();
    GetSession()->SendPacket( &data );

    data.Initialize(SMSG_STOP_MIRROR_TIMER);
    data << uint8(0x00) << uint8(0x00) << uint8(0x00) << uint8(0x00);
    GetSession()->SendPacket( &data );

    data.Initialize(SMSG_STOP_MIRROR_TIMER);
    data << uint8(0x01) << uint8(0x00) << uint8(0x00) << uint8(0x00);
    GetSession()->SendPacket( &data );

    data.Initialize(SMSG_STOP_MIRROR_TIMER);
    data << uint8(0x02) << uint8(0x00) << uint8(0x00) << uint8(0x00);
    GetSession()->SendPacket( &data );

    SetUInt32Value(CONTAINER_FIELD_SLOT_1+29, 8326);
    SetUInt32Value(UNIT_FIELD_AURA+32, 8326);
    SetUInt32Value(UNIT_FIELD_AURALEVELS+8, 0xeeeeee00);
    SetUInt32Value(UNIT_FIELD_AURAAPPLICATIONS+8, 0xeeeeee00);
    SetUInt32Value(UNIT_FIELD_AURAFLAGS+4, 12);
    SetUInt32Value(UNIT_FIELD_AURASTATE, 2);

    SetFlag(PLAYER_FLAGS, 0x10);

    // spawn Corpse
    SpawnCorpseBody();
}


void Player::ResurrectPlayer()
{
    RemoveFlag(PLAYER_FLAGS, 0x10);
    setDeathState(ALIVE);
    if(getRace() == 4)                            // NEs to turn back from Wisp.
    {
        DeMorph();
    }

    // hiding spirit healers to living players
    for (Object::InRangeSet::iterator iter = GetInRangeSetBegin();
        iter != GetInRangeSetEnd(); iter++)
    {
        Creature *creat = objmgr.GetObject<Creature>((*iter)->GetGUID());
        if (creat && creat->GetUInt32Value(UNIT_FIELD_DISPLAYID) == 5233)
            creat->DestroyForPlayer(this);
    }
}


void Player::KillPlayer()
{
    WorldPacket data;

    SetMovement(MOVE_ROOT);

    data.Initialize(SMSG_STOP_MIRROR_TIMER);
    data << uint8(0x00) << uint8(0x00) << uint8(0x00) << uint8(0x00);
    GetSession()->SendPacket( &data );

    data.Initialize(SMSG_STOP_MIRROR_TIMER);
    data << uint8(0x01) << uint8(0x00) << uint8(0x00) << uint8(0x00);
    GetSession()->SendPacket( &data );

    data.Initialize(SMSG_STOP_MIRROR_TIMER);
    data << uint8(0x02) << uint8(0x00) << uint8(0x00) << uint8(0x00);
    GetSession()->SendPacket( &data );

    setDeathState(CORPSE);
    SetFlag( UNIT_FIELD_FLAGS, 0x08 );            //player death animation, also can be used with DYNAMIC_FLAGS
    SetFlag( UNIT_DYNAMIC_FLAGS, 0x00 );
    CreateCorpse();

    if(getRace() == 4)                            // NEs
    {
        this->SetUInt32Value(UNIT_FIELD_DISPLAYID, 10045);
    }
}


void Player::CreateCorpse()
{
    Corpse *pCorpse;
    uint32 _uf, _pb, _pb2, _cfb1, _cfb2;

    pCorpse = objmgr.GetCorpseByOwner(this);
    if(!pCorpse)
    {
        pCorpse = new Corpse();
        pCorpse->Create(objmgr.GenerateLowGuid(HIGHGUID_CORPSE), this, GetMapId(), GetPositionX(),
            GetPositionY(), GetPositionZ(), GetOrientation());

        _uf = GetUInt32Value(UNIT_FIELD_BYTES_0);
        _pb = GetUInt32Value(PLAYER_BYTES);
        _pb2 = GetUInt32Value(PLAYER_BYTES_2);

        uint8 race       = (uint8)(_uf);
        uint8 skin       = (uint8)(_pb);
        uint8 face       = (uint8)(_pb >> 8);
        uint8 hairstyle  = (uint8)(_pb >> 16);
        uint8 haircolor  = (uint8)(_pb >> 24);
        uint8 facialhair = (uint8)(_pb2);

        _cfb1 = ((0x00) | (race << 8) | (0x00 << 16) | (skin << 24));
        _cfb2 = ((face) | (hairstyle << 8) | (haircolor << 16) | (facialhair << 24));

        pCorpse->SetZoneId( GetZoneId() );
        pCorpse->SetUInt32Value( CORPSE_FIELD_BYTES_1, _cfb1 );
        pCorpse->SetUInt32Value( CORPSE_FIELD_BYTES_2, _cfb2 );
        pCorpse->SetUInt32Value( CORPSE_FIELD_FLAGS, 4 );
        pCorpse->SetUInt32Value( CORPSE_FIELD_DISPLAY_ID, GetUInt32Value(UNIT_FIELD_DISPLAYID) );

        uint32 iDisplayID;
        uint16 iIventoryType;
        uint32 _cfi;
        for (int i = 0; i < EQUIPMENT_SLOT_END; i++)
        {
            if(m_items[i])
            {
                iDisplayID = m_items[i]->GetProto()->DisplayInfoID;
                iIventoryType = (uint16)m_items[i]->GetProto()->InventoryType;

                _cfi =  (uint16(iDisplayID)) | (iIventoryType)<< 24;
                pCorpse->SetUInt32Value(CORPSE_FIELD_ITEM + i,_cfi);
            }
        }
        // save corpse in db for future use
        pCorpse->SaveToDB();
        Log::getSingleton( ).outError("AddObject at Player.cpp");
        objmgr.AddObject(pCorpse);
    }
    else                                          //Corpse already exist in world, update it
    {
        pCorpse->SetPosition(GetPositionX(), GetPositionY(), GetPositionZ(), GetOrientation());
    }
}


void Player::SpawnCorpseBody()
{
    Corpse *pCorpse;

    pCorpse = objmgr.GetCorpseByOwner(this);
    if(pCorpse && !pCorpse->IsInWorld())
        pCorpse->PlaceOnMap();
    // Deadknight:hiding every creature except spirit healers
    for (Object::InRangeSet::iterator iter = GetInRangeSetBegin();
        iter != GetInRangeSetEnd(); iter++)
    {
        Creature *creat = objmgr.GetObject<Creature>((*iter)->GetGUID());
        if (creat && creat->GetUInt32Value(UNIT_FIELD_DISPLAYID) != 5233)
            creat->DestroyForPlayer(this);
    }
    // Deadknight:hiding players except dead
    for (Object::InRangeSet::iterator iter = GetInRangeSetBegin();
        iter != GetInRangeSetEnd(); iter++)
    {
        Player *plyr = objmgr.GetObject<Player>((*iter)->GetGUID());
        if (plyr && plyr->isAlive())
        {
            if(!plyr->IsGroupMember(this))
            {
                plyr->DestroyForPlayer(this);
                this->DestroyForPlayer(plyr);
            }
        }
        if (plyr && plyr->isDead())               //Deadknight:if removed before add dead people
        {
            WorldPacket packet,packetR;
            UpdateData data,dataR;
            Player *plyrR=plyr;

            plyr->BuildCreateUpdateBlockForPlayer( &data, this );
            data.BuildPacket(&packet);
            GetSession()->SendPacket( &packet );

            BuildCreateUpdateBlockForPlayer( &dataR, plyrR );
            dataR.BuildPacket(&packetR);
            plyrR->GetSession()->SendPacket( &packetR );
        }
    }

}


void Player::SpawnCorpseBones()
{
    Corpse *pCorpse;
    pCorpse = objmgr.GetCorpseByOwner(this);
    if(pCorpse)
    {
        pCorpse->SetUInt32Value(CORPSE_FIELD_FLAGS, 5);
        // remove corpse owner association
        pCorpse->SetUInt64Value(CORPSE_FIELD_OWNER, 0);
        // remove item association
        for (int i = 0; i < EQUIPMENT_SLOT_END; i++)
        {
            if(pCorpse->GetUInt32Value(CORPSE_FIELD_ITEM + i))
                pCorpse->SetUInt32Value(CORPSE_FIELD_ITEM + i, 0);
        }
        pCorpse->DeleteFromDB();
    }
    // Deadknight:Add creatures nearby
    for (Object::InRangeSet::iterator iter = GetInRangeSetBegin();
        iter != GetInRangeSetEnd(); iter++)
    {
        Creature *creat = objmgr.GetObject<Creature>((*iter)->GetGUID());
        if (creat && creat->GetUInt32Value(UNIT_FIELD_DISPLAYID) != 5233)
        {
            WorldPacket packet;
            UpdateData data;

            creat->BuildCreateUpdateBlockForPlayer( &data, this );
            data.BuildPacket(&packet);
            GetSession()->SendPacket( &packet );
        }
    }
    // Deadknight:Add players nearby
    for (Object::InRangeSet::iterator iter = GetInRangeSetBegin();
        iter != GetInRangeSetEnd(); iter++)
    {
        Player *plyr = objmgr.GetObject<Player>((*iter)->GetGUID());
        if (plyr && plyr->isAlive())
        {
            WorldPacket packet, packetR;
            UpdateData data, dataR;
            Player *plyrR=plyr;

            plyr->BuildCreateUpdateBlockForPlayer( &data, this );
            data.BuildPacket(&packet);
            GetSession()->SendPacket( &packet );

            BuildCreateUpdateBlockForPlayer( &dataR, plyrR );
            dataR.BuildPacket(&packetR);
            plyrR->GetSession()->SendPacket( &packetR );
        }
        if(plyr && plyr->isDead())
        {
            if(!plyr->IsGroupMember(this))
            {
                plyr->DestroyForPlayer(this);
                this->DestroyForPlayer(plyr);
            }
        }
    }
}


void Player::DeathDurabilityLoss(double percent)
{
    uint32 pDurability, pNewDurability;

    for (int i = 0; i < EQUIPMENT_SLOT_END; i++)
    {
        if(m_items[i])
        {
            pDurability =  m_items[i]->GetUInt32Value(ITEM_FIELD_DURABILITY);
            if(pDurability)
            {
                pNewDurability = (uint32)(pDurability*percent);
                pNewDurability = (pDurability - pNewDurability);
                if(pNewDurability < 0) { pNewDurability = 0; }

                m_items[i]->SetUInt32Value(ITEM_FIELD_DURABILITY, pNewDurability);
            }
        }
    }
}


void Player::RepopAtGraveyard()
{
    // Make sure we dont get any random numbers, if we have no graveyard you will pop where your corpse is at (dead world).
    float closestX = 0, closestY = 0, closestZ = 0, closestO = 0;
    WorldPacket data;
    float curX, curY, curZ;
    bool first = true;

    ObjectMgr::GraveyardMap::const_iterator itr;
    for (itr = objmgr.GetGraveyardListBegin(); itr != objmgr.GetGraveyardListEnd(); itr++)
    {
        GraveyardTeleport *pGrave = itr->second;
        if(pGrave->MapId == GetMapId())
        {
            curX = pGrave->X;
            curY = pGrave->Y;
            curZ = pGrave->Z;
            if( first || pow(m_positionX-curX,2) + pow(m_positionY-curY,2) <
                pow(m_positionX-closestX,2) + pow(m_positionY-closestY,2) )
            {
                first = false;

                closestX = curX;
                closestY = curY;
                closestZ = curZ;
                closestO = pGrave->O;
            }
        }
    }

    if(closestX != 0 && closestY != 0 && closestZ != 0)
    {
        WorldPacket data;

        // Send new position to client via MSG_MOVE_TELEPORT_ACK
        BuildTeleportAckMsg(&data, closestX, closestY, closestZ, 0);
        GetSession()->SendPacket(&data);

        // Set actual position and update in-range lists
        SetPosition(closestX, closestY, closestZ, 0);

        //////////////////////////////////
        // Now send new position of this player to clients using MSG_MOVE_HEARTBEAT
        BuildHeartBeatMsg(&data);
        SendMessageToSet(&data, true);

        // SetPosition(closestX, closestY, closestZ, 0);
    }

    // check for nearby spirit healers, and send update
    for (Object::InRangeSet::iterator iter = GetInRangeSetBegin();
        iter != GetInRangeSetEnd(); iter++)
    {
        Creature *creat = objmgr.GetObject<Creature>((*iter)->GetGUID());
        if (creat && creat->GetUInt32Value(UNIT_FIELD_DISPLAYID) == 5233)
        {
            WorldPacket packet;
            UpdateData data;

            creat->BuildCreateUpdateBlockForPlayer( &data, this );
            data.BuildPacket(&packet);
            GetSession()->SendPacket( &packet );
        }
    }
}


void Player::JoinedChannel(Channel *c)
{
    m_channels.push_back(c);
}


void Player::LeftChannel(Channel *c)
{
    m_channels.remove(c);
}


void Player::CleanupChannels()
{
    list<Channel *>::iterator i;
    for(i = m_channels.begin(); i != m_channels.end(); i++)
        (*i)->Leave(this,false);
}


// skilllines
bool Player::HasSkillLine(uint32 id)
{
    std::list<struct skilllines>::iterator itr;
    for (itr = m_skilllines.begin(); itr != m_skilllines.end(); ++itr)
    {
        if (itr->lineId == id)
        {
            return true;
        }
    }
    return false;
}


void Player::AddSkillLine(uint32 id, uint16 currVal, uint16 maxVal)
{
    AddSkillLine(id, currVal, maxVal, true);
}


void Player::AddSkillLine(uint32 id, uint16 currVal, uint16 maxVal, bool sendUpdate)
{
    struct skilllines newline;
    newline.lineId = id;
    newline.currVal = currVal;
    newline.maxVal = maxVal;
    newline.posStatCurrVal = 0;
    newline.posstatMaxVal = 0;

    uint32 CurMax = 0;
    CurMax |= ((uint32)(maxVal << 16)) | currVal;

    m_skilllines.push_back(newline);

    if (sendUpdate)
    {
        uint16 LineBase = PLAYER_SKILL_INFO_1_1 + (m_skilllines.size() * 3);

        SetUInt32Value(LineBase, id);
        SetUInt32Value(LineBase + 1, CurMax);
        SetUInt32Value(LineBase + 2, 0);
    }
}


void Player::RemoveSkillLine(uint32 id)
{
}


void Player::smsg_InitialActions()
{
    Log::getSingleton( ).outString( "Initializing Action Buttons for '%u'", GetGUID() );
    WorldPacket data;
    uint16 actionCount = m_actions.size();
    uint16 button=0;

    std::list<struct actions>::iterator itr;
    data.Initialize(SMSG_ACTION_BUTTONS);
    for (itr = m_actions.begin(); itr != m_actions.end();)
    {
        if (itr->button == button)
        {
            data << uint16(itr->action);
            data << uint8(itr->type);
            data << uint8(itr->misc);
            ++itr;
        }
        else
        {
            data << uint32(0);
        }
        button++;
    }

    if (button < 120 )
    {
        for (int temp_counter=(120-button); temp_counter>0; temp_counter--)
        {
            data << uint32(0);
        }
    }
    GetSession()->SendPacket( &data );
    Log::getSingleton( ).outString( "Action Buttons for '%u' Initialized", GetGUID() );
}


void Player::addAction(const uint8 button, const uint16 action, const uint8 type, const uint8 misc)
{
    bool ButtonExists = false;
    std::list<struct actions>::iterator itr;
    for (itr = m_actions.begin(); itr != m_actions.end(); ++itr)
    {
        if (itr->button == button)
        {
            itr->button=button;
            itr->action=action;
            itr->type=type;
            itr->misc=misc;
            ButtonExists = true;
            break;
        }
    }
    if (!ButtonExists)
    {
        struct actions newaction;
        newaction.button=button;
        newaction.action=action;
        newaction.type=type;
        newaction.misc=misc;
        m_actions.push_back(newaction);
    }
    Log::getSingleton( ).outString( "Player '%u' Added Action '%u' to Button '%u'", GetGUID(), action, button );
}


void Player::removeAction(uint8 button)
{
    std::list<struct actions>::iterator itr;
    for (itr = m_actions.begin(); itr != m_actions.end(); ++itr)
    {
        if (itr->button == button)
        {
            m_actions.erase(itr);
            break;
        }
    }
    Log::getSingleton( ).outString( "Action Button '%u' Removed from Player '%u'", button, GetGUID() );
}


void Player::SetDontMove(bool dontMove)
{
    m_dontMove = dontMove;
}


//Groupcheck
bool Player::IsGroupMember(Player *plyr)
{
    if(!plyr->IsInGroup())
        return false;
    Group *grp = objmgr.GetGroupByLeader(plyr->GetGroupLeader());
    if(grp->GroupCheck(plyr->GetGUID()))
    {
        return true;
    }
    return false;
}
