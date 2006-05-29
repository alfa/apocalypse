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

struct ItemSetEntry
{
    uint32    spells[8];
    uint32    items_to_triggerspell[8];
    uint32    required_skill_id;
    uint32    required_skill_value;
};
struct TalentEntry
{
    uint32    TalentID;
    uint32    TalentTree;
    uint32    Row;
    uint32    Col;
    uint32    RankID[5];
    uint32    DependsOn;
    uint32    DependsOnRank;
};
struct emoteentry
{
    uint32    Id;
    uint32      textid;
};
struct SkillLineAbility
{
    uint32    id;
    uint32    miscid;
    uint32    spellid;
    uint32    unkown1;
    uint32    unkown2;
    uint32    unkown3;
    uint32    unkown4;
    uint32    req_skill_value;                              //for trade skill.not for training.
    uint32    forward_spellid;
    uint32    unkown5;
    uint32    max_value;
    uint32    min_value;
};
struct SpellEntry
{
    uint32    Id;
    uint32    School;
    uint32    Category;
    uint32    Attributes;
    uint32    AttributesEx;
    uint32    Targets;
    uint32    TargetCreatureType;
    uint32    RequiresSpellFocus;
    uint32    CasterAuraState;
    uint32    CastingTimeIndex;
    uint32    RecoveryTime;
    uint32    CategoryRecoveryTime;
    uint32    InterruptFlags;
    uint32    AuraInterruptFlags;
    uint32    ChannelInterruptFlags;
    uint32    procFlags;
    uint32    procChance;
    uint32    procCharges;
    uint32    maxLevel;
    uint32    baseLevel;
    uint32    spellLevel;
    uint32    DurationIndex;
    uint32    powerType;
    uint32    manaCost;
    uint32    manaCostPerlevel;
    uint32    manaPerSecond;
    uint32    manaPerSecondPerLevel;
    uint32    rangeIndex;
    float     speed;
    uint32    modalNextSpell;
    uint32    Totem[2];
    uint32    Reagent[8];
    uint32    ReagentCount[8];
    uint32    EquippedItemClass;
    uint32    EquippedItemSubClass;
    uint32    Effect[3];
    int32     EffectDieSides[3];
    uint32    EffectBaseDice[3];
    float     EffectDicePerLevel[3];
    float     EffectRealPointsPerLevel[3];
    int32     EffectBasePoints[3];
    uint32    EffectImplicitTargetA[3];
    uint32    EffectImplicitTargetB[3];
    uint32    EffectRadiusIndex[3];
    uint32    EffectApplyAuraName[3];
    uint32    EffectAmplitude[3];
    uint32    EffectMultipleValue[3];
    uint32    EffectChainTarget[3];
    uint32    EffectItemType[3];
    uint32    EffectMiscValue[3];
    uint32    EffectTriggerSpell[3];
    float     EffectPointsPerComboPoint[3];
    uint32    SpellVisual;
    uint32    SpellIconID;
    uint32    activeIconID;
    uint32      spellPriority;
    uint32    Rank;
    uint32    RankAlt1;
    uint32    RankAlt2;
    uint32    RankAlt3;
    uint32    RankAlt4;
    uint32    RankAlt5;
    uint32    RankAlt6;
    uint32    RankAlt7;
    uint32    RankFlags;
    uint32    ManaCostPercentage;
    uint32    StartRecoveryCategory;
    uint32    StartRecoveryTime;
    uint32      field156;
    uint32    SpellFamilyName;
};
struct SpellCastTime
{
    uint32    ID;
    uint32    CastTime;
};
struct SpellRadius
{
    uint32    ID;
    float     Radius;
    float     Radius2;
};
struct SpellRange
{
    uint32    ID;
    float     minRange;
    float     maxRange;
};
struct SpellDuration
{
    uint32    ID;
    int32     Duration[3];
};
struct AreaTableEntry
{
    uint32    ID;
    uint32    zone;
    uint32    exploreFlag;
    uint32    area_level;
};
struct FactionEntry
{
    uint32      ID;
    int32       reputationListID;
    uint32      something1;
    uint32      something2;
    uint32      something3;
    uint32      something4;
    uint32      something5;
    uint32      something6;
    uint32      something7;
    uint32      something8;
    uint32      something9;
    uint32      faction;
};
struct FactionTemplateEntry
{
    uint32      ID;
    uint32      faction;
    uint32      fightsupport;
    uint32      friendly;
    uint32      hostile;
};
struct ItemDisplayTemplateEntry
{
    uint32      ID;
    uint32      seed;
    uint32      randomPropertyID;
};
struct SpellItemEnchantment
{
    uint32      ID;
    uint32      display_type;
    uint32      unkown1;
    uint32      value1;
    uint32      value2;
    uint32      spellid;
    uint32      unkown2;
    uint32      description;
    uint32      aura_id;
    uint32      unkown4;
};
