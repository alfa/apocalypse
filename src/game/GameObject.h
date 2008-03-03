/* 
 * Copyright (C) 2005-2008 MaNGOS <http://www.mangosproject.org/>
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

#ifndef MANGOSSERVER_GAMEOBJECT_H
#define MANGOSSERVER_GAMEOBJECT_H

#include "Common.h"
#include "Object.h"
#include "LootMgr.h"
#include "Database/DatabaseEnv.h"

// GCC have alternative #pragma pack(N) syntax and old gcc version not support pack(push,N), also any gcc version not support it at some platform
#if defined( __GNUC__ )
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

// from `gameobject_template`
struct GameObjectInfo
{
    uint32  id;
    uint32  type;
    uint32  displayId;
    char   *name;
    uint32  faction;
    uint32  flags;
    float   size;
    uint32  data0;
    uint32  data1;
    uint32  data2;
    uint32  data3;
    uint32  data4;
    uint32  data5;
    uint32  data6;
    uint32  data7;
    uint32  data8;
    uint32  data9;
    uint32  data10;
    uint32  data11;
    uint32  data12;
    uint32  data13;
    uint32  data14;
    uint32  data15;
    uint32  data16;
    uint32  data17;
    uint32  data18;
    uint32  data19;
    uint32  data20;
    uint32  data21;
    uint32  data22;
    uint32  data23;
    char   *ScriptName;
};

struct GameObjectLocale
{
    std::vector<std::string> Name;
};

// from `ganeobject`
struct GameObjectData
{
    uint32 id;                                              // entry in gamobject_template
    uint32 mapid;
    float posX;
    float posY;
    float posZ;
    float orientation;
    float rotation0;
    float rotation1;
    float rotation2;
    float rotation3;
    int32  spawntimesecs;
    uint32 animprogress;
    uint32 go_state;
};

// GCC have alternative #pragma pack() syntax and old gcc version not support pack(pop), also any gcc version not support it at some platform
#if defined( __GNUC__ )
#pragma pack()
#else
#pragma pack(pop)
#endif

// For containers: [GO_NOT_READY] -> GO_CLOSED -> GO_OPEN -> GO_LOOTED -> GO_CLOSED -> ...
// For bobber:     GO_NOT_READY   -> GO_CLOSED -> GO_OPEN -> GO_LOOTED -> <deleted>
enum LootState
{
    GO_NOT_READY = 0,
    GO_CLOSED,
    GO_OPEN,
    GO_LOOTED
};

class Unit;

// 5 sec for bobber catch
#define FISHING_BOBBER_READY_TIME 5

class MANGOS_DLL_SPEC GameObject : public WorldObject
{
    public:
        explicit GameObject( WorldObject *instantiator );
        ~GameObject();

        void AddToWorld();
        void RemoveFromWorld();

        bool Create(uint32 guidlow, uint32 name_id, uint32 mapid, float x, float y, float z, float ang, float rotation0, float rotation1, float rotation2, float rotation3, uint32 animprogress, uint32 go_state);
        void Update(uint32 p_time);
        GameObjectInfo const* GetGOInfo() const;

        bool IsTransport() const;

        void SetOwnerGUID(uint64 owner)
        {
            m_spawnedByDefault = false;                     // all object with owner is de-spawned after delay
            SetUInt64Value(OBJECT_FIELD_CREATED_BY, owner);
        }
        uint64 GetOwnerGUID() const { return GetUInt64Value(OBJECT_FIELD_CREATED_BY); }
        Unit* GetOwner() const;

        uint32 GetDBTableGUIDLow() const { return m_DBTableGuid; }

        void Say(const char* text, const uint32 language, const uint64 TargetGuid) { MonsterSay(text,language,TargetGuid); }
        void Yell(const char* text, const uint32 language, const uint64 TargetGuid) { MonsterYell(text,language,TargetGuid); }
        void TextEmote(const char* text, const uint64 TargetGuid) { MonsterTextEmote(text,TargetGuid); }
        void Whisper(const uint64 receiver, const char* text) { MonsterWhisper(receiver,text); }

        void SaveToDB();
        bool LoadFromDB(uint32 guid, uint32 InstanceId);
        void DeleteFromDB();
        void SetLootState(LootState s) { m_lootState = s; }
        static uint32 GetLootId(GameObjectInfo const* info);
        uint32 GetLootId() const { return GetLootId(GetGOInfo()); }
        void SetRespawnTime(int32 respawn)
        {
            m_respawnTime = respawn > 0 ? time(NULL) + respawn : 0;
            m_respawnDelayTime = respawn > 0 ? respawn : 0;
        }
        void Respawn();
        bool isSpawned() const
        {
            return m_respawnDelayTime == 0 ||
                m_respawnTime > 0 && !m_spawnedByDefault ||
                m_respawnTime == 0 && m_spawnedByDefault;
        }
        bool isSpawnedByDefault() const { return m_spawnedByDefault; }
        void Refresh();
        void Delete();
        void SetSpellId(uint32 id) { m_spellId = id;}
        uint32 GetSpellId() const { return m_spellId;}
        void getFishLoot(Loot *loot);
        uint32 GetGoType() const { return GetUInt32Value(GAMEOBJECT_TYPE_ID); }

        LootState getLootState() const { return m_lootState; }

        void AddToSkillupList(uint32 PlayerGuidLow) { m_SkillupList.push_back(PlayerGuidLow); }
        bool IsInSkillupList(uint32 PlayerGuidLow) const
        {
            for (std::list<uint32>::const_iterator i = m_SkillupList.begin(); i != m_SkillupList.end(); ++i)
                if (*i == PlayerGuidLow) return true;
            return false;
        }
        void ClearSkillupList() { m_SkillupList.clear(); }

        void AddUniqueUse(Player* player);
        void AddUse() { ++m_usetimes; }
        
        uint32 GetUseCount() const { return m_usetimes; }
        uint32 GetUniqueUseCount() const { return m_unique_users.size(); }

        void SaveRespawnTime();

        Loot        loot;

        bool hasQuest(uint32 quest_id) const;
        bool hasInvolvedQuest(uint32 quest_id) const;
        bool ActivateToQuest(Player *pTarget) const;

        bool isVisibleForInState(Player const* u, bool inVisibleList) const;

        GridReference<GameObject> &GetGridRef() { return m_gridRef; }
    protected:
        uint32      m_charges;                              // Spell charges for GAMEOBJECT_TYPE_SPELLCASTER (22)
        uint32      m_spellId;
        time_t      m_respawnTime;                          // (secs) time of next respawn (or despawn if GO have owner()),
        uint32      m_respawnDelayTime;                     // (secs) if 0 then current GO state no dependent from timer
        uint32      m_flags;
        LootState   m_lootState;
        bool        m_spawnedByDefault;
        std::list<uint32> m_SkillupList;

        std::set<uint32> m_unique_users;
        uint32 m_usetimes;

        uint32 m_DBTableGuid;
    private:
        GridReference<GameObject> m_gridRef;
};
#endif
