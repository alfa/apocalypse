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

#ifndef DBCSTORES_H
#define DBCSTORES_H

#include "Common.h"
//#include "DataStore.h"
#include "dbcfile.h"
#include "DBCStructure.h"

enum SpellSpecific
{
    SPELL_NORMAL = 0,
    SPELL_SEAL = 1,
    SPELL_BLESSING = 2,
    SPELL_AURA = 3,
    SPELL_STING = 4,
    SPELL_CURSE = 5,
    SPELL_ASPECT = 6
};

enum SpellFamilyNames
{
    SPELLFAMILY_GENERIC = 0,
    SPELLFAMILY_MAGE = 3,
    SPELLFAMILY_WARRIOR = 4,
    SPELLFAMILY_WARLOCK = 5,
    SPELLFAMILY_PRIEST = 6,
    SPELLFAMILY_DRUID = 7,
    SPELLFAMILY_ROGUE = 8,
    SPELLFAMILY_HUNTER = 9,
    SPELLFAMILY_PALADIN = 10,
    SPELLFAMILY_SHAMAN = 11,
    SPELLFAMILY_POTION = 13
};

float GetRadius(SpellRadiusEntry *radius);
uint32 GetCastTime(SpellCastTimesEntry *time);
float GetMinRange(SpellRangeEntry *range);
float GetMaxRange(SpellRangeEntry *range);
int32 GetDuration(SpellEntry *spellInfo);
int32 GetMaxDuration(SpellEntry *spellInfo);
char* GetPetName(uint32 petfamily);
uint32 FindSpellRank(uint32 spellId);
bool canStackSpellRank(SpellEntry *spellInfo);
bool IsRankSpellDueToSpell(SpellEntry *spellInfo_1,uint32 spellId_2);
bool IsNoStackSpellDueToSpell(uint32 spellId_1, uint32 spellId_2);
bool IsNoStackAuraDueToAura(uint32 spellId_1, uint32 effIndex_1, uint32 spellId_2, uint32 effIndex_2);
int32 CompareAuraRanks(uint32 spellId_1, uint32 effIndex_1, uint32 spellId_2, uint32 effIndex_2);
SpellSpecific GetSpellSpecific(uint32 spellId);
bool IsSpellSingleEffectPerCaster(uint32 spellId);
bool IsPassiveSpell(uint32 spellId);
bool IsPositiveSpell(uint32 spellId);
bool IsPositiveEffect(uint32 spellId, uint32 effIndex);
bool IsSingleTarget(uint32 spellId);
AreaTableEntry* GetAreaEntryByAreaID(uint32 area_id);
AreaTableEntry* GetAreaEntryByAreaFlag(uint32 area_flag);

template<class T>
class DBCStorage
{
    public:
        DBCStorage(const char *f){data = NULL;fmt=f;}
        ~DBCStorage(){if(data) delete [] data;};

        inline
            T* LookupEntry(uint32 id)
        {
            return (id>nCount)?NULL:data[id];

        }
        inline
            unsigned int GetNumRows()
        {
            return nCount;
        }

        bool Load(char const* fn)
        {

            dbc = new DBCFile;
            // Check if load was sucessful, only then continue
            bool res = dbc->Load(fn);
            if (res)
            {
                data=(T **) dbc->AutoProduceData(fmt,&nCount);
            }
            delete dbc;

            // error in dbc file at loading
            if(!data)
                res = false;

            return res;
        }

        void Clear() {  delete[] ((char*)data); data = NULL; }

        T** data;
        uint32 nCount;
        const char * fmt;

    private:
        DBCFile * dbc;
};

//extern DBCStorage <AreaTableEntry>            sAreaStore; -- accessed using 2 functions
extern DBCStorage <CreatureFamilyEntry>       sCreatureFamilyStore;
extern DBCStorage <SpellCastTimesEntry>       sCastTimesStore;
extern DBCStorage <EmotesTextEntry>           sEmotesTextStore;
extern DBCStorage <FactionEntry>              sFactionStore;
extern DBCStorage <FactionTemplateEntry>      sFactionTemplateStore;
extern DBCStorage <ItemDisplayInfoEntry>      sItemDisplayInfoStore;
extern DBCStorage <ItemRandomPropertiesEntry> sItemRandomPropertiesStore;
extern DBCStorage <ItemSetEntry>              sItemSetStore;
extern DBCStorage <LockEntry>                 sLockStore;
extern DBCStorage <SkillLineEntry>            sSkillLineStore;
extern DBCStorage <SkillLineAbilityEntry>     sSkillLineAbilityStore;
extern DBCStorage <SpellDurationEntry>        sSpellDurationStore;
//extern DBCStorage <SpellFocusObjectEntry>     sSpellFocusObjectStore;
extern DBCStorage <SpellItemEnchantmentEntry> sSpellItemEnchantmentStore;
extern DBCStorage <SpellRadiusEntry>          sSpellRadiusStore;
extern DBCStorage <SpellRangeEntry>           sSpellRangeStore;
extern DBCStorage <SpellEntry>                sSpellStore;
extern DBCStorage <TalentEntry>               sTalentStore;
extern DBCStorage <TaxiNodesEntry>            sTaxiNodesStore;
extern TaxiPathSetBySource                    sTaxiPathSetBySource;
extern TaxiPathNodesByPath                    sTaxiPathNodesByPath;
extern DBCStorage <WorldSafeLocsEntry>        sWorldSafeLocsStore;

void LoadDBCStores(std::string dataPath);
#endif
