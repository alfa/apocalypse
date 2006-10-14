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

#ifndef MANGOS_LOOTMGR_H
#define MANGOS_LOOTMGR_H

#include <list>
#include <vector>
#include "Common.h"
#include "Policies/Singleton.h"
#include "ItemPrototype.h"
#include "ByteBuffer.h"
#include "Util.h"

#define ROLL_PASS  0
#define ROLL_NEED  1
#define ROLL_GREED 2

enum LootMethod
{
    FREE_FOR_ALL      = 0,
    ROUND_ROBIN       = 1,
    MASTER_LOOT       = 2,
    GROUP_LOOT        = 3,
    NEED_BEFORE_GREED = 4
};

using std::vector;
using std::list;

class Player;

struct LootItem
{
    uint32  itemid;
    uint32  displayid;
    float   chance;
    float   questchance;
    uint8   count;
    bool    is_looted;
    

    LootItem()
        : itemid(0), displayid(0), chance(0), questchance(0), is_looted(true), count(1) {}

    LootItem(uint32 _itemid, uint32 _displayid, float _chance, float _questchance, uint8 _count = 0)
        : itemid(_itemid), displayid(_displayid), chance(_chance), questchance(_questchance), count(_count), is_looted(false) {}

    static bool looted(LootItem &itm) { return itm.is_looted; }
    static bool not_looted(LootItem &itm) { return !itm.is_looted; }
    static bool lootable(LootItem &itm, Player* player);
};

struct Loot
{
    std::set<Player*> PlayersLooting;
    vector<LootItem> items;
    uint32 gold;
    bool released;

    Loot(uint32 _gold = 0) : gold(_gold), released(false) {}

    ~Loot()
    {
        items.clear();
        PlayersLooting.clear();
    }

    bool empty() const { return items.empty() && gold == 0; }
    void clear() { items.clear(); gold = 0; }
    void NotifyItemRemoved(uint8 lootIndex);
    void NotifyMoneyRemoved();
    void AddLooter(Player *player) { PlayersLooting.insert(player); }
    void RemoveLooter(Player *player) { PlayersLooting.erase(player); }
    void remove(uint8 lootSlot);
    void remove(const LootItem & item);
};

typedef list<LootItem> LootItemList;
typedef HM_NAMESPACE::hash_map<uint32, LootItemList > LootStore;

extern LootStore LootTemplates_Creature;
extern LootStore LootTemplates_Fishing;
extern LootStore LootTemplates_Gameobject;
extern LootStore LootTemplates_Pickpocketing;

void FillLoot(Player* player,Loot *loot, uint32 loot_id, LootStore& store);
void FillSkinLoot(Loot *Skinloot,uint32 itemid);
void LoadLootTables();

inline ByteBuffer& operator<<(ByteBuffer& b, LootItem& li)
{
    b << uint32(li.itemid);
    b << uint32(li.count);                                  // nr of items of this type
    b << uint32(li.displayid);
    b << uint64(0) << uint8(0);
    return b;
}

inline ByteBuffer& operator<<(ByteBuffer& b, Loot& l)
{
    b << l.gold;

    uint8 unlootedCount = 0;
    for (uint8 i = 0; i < l.items.size(); i++)
        if (!l.items[i].is_looted) unlootedCount++;
    b << unlootedCount;

    for (uint8 i = 0; i < l.items.size(); i++)
        if (!l.items[i].is_looted)
            b << uint8(i) << l.items[i];

    return b;
}
#endif
