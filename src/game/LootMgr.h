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

#ifndef MANGOS_LOOTMGR_H
#define MANGOS_LOOTMGR_H

#include <map>
#include <string>
#include <vector>
#include "Common.h"
#include "Policies/Singleton.h"

struct LootItem
{
  LootItem(uint32 item = 0, float c = 0) : itemid(item), chance(c) {}
 
    uint32 itemid;
    float chance;
};






struct SequenceGen
{
    SequenceGen(short start_idx = -1) : i_current(start_idx) {}
    short operator()(void) { return ++i_current; }
    short i_current;
};




class LootMgr 
{

public:
  typedef std::vector<LootItem> LootList;
  typedef HM_NAMESPACE::hash_map<uint32, LootList* > LootTable;  

    
  LootMgr();
  ~LootMgr();
  void LoadLootTables(void);
  inline const LootList& getCreaturesLootList(int id) const      
  {
    LootTable::const_iterator iter = i_creaturesLoot.find(id);
    return (iter == i_creaturesLoot.end() ? si_noLoot : *iter->second);
  }

  inline const LootList& getGameObjectsLootList(int id) const      
  {
    LootTable::const_iterator iter = i_gameObjectsLoot.find(id);
    return (iter == i_gameObjectsLoot.end() ? si_noLoot : *iter->second);
  }

private:

  void _populateLootTemplate(const char *, LootTable &);
  LootTable i_creaturesLoot;
  LootTable i_gameObjectsLoot;
  static LootList si_noLoot;
};

#define LootManager MaNGOS::Singleton<LootMgr>::Instance()

#endif
