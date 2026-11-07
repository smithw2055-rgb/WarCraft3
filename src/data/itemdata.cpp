#include "itemdata.h"
#include "data/slk_merge.h"
#include "map/object_mods.h"
#include "resource/mpq_storm.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace {

CItemClass ParseItemClass(const std::string& classification) {
    const std::string value = ToLowerCopy(TrimCopy(classification));
    if (value == "permanent") return CItemClass::PERMANENT;
    if (value == "charged") return CItemClass::CHARGED;
    if (value == "powerup") return CItemClass::POWERUP;
    if (value == "artifact") return CItemClass::ARTIFACT;
    if (value == "purchasable") return CItemClass::PURCHASABLE;
    if (value == "campaign") return CItemClass::CAMPAIGN;
    if (value == "miscellaneous") return CItemClass::MISCELLANEOUS;
    if (value == "unknown") return CItemClass::UNKNOWN;
    if (value == "any") return CItemClass::ANY;
    return CItemClass::UNKNOWN;
}

} // namespace

CItemData CItemData::LoadFromMpq(const MpqManager& mpq) {
    return LoadFromMpq(mpq, std::string());
}

CItemData CItemData::LoadFromMpq(const MpqManager& mpq, const std::string& mapPath) {
    CItemData data;

    SlkTable itemSlk;
    {
        std::vector<uint8_t> buf;
        if (mpq.ReadFile("Units\\ItemData.slk", buf)) {
            itemSlk.Load(buf.data(), buf.size());
        }
    }

    TxtTable skinTbl;
    {
        // Generic + per-race/campaign overlays (ref StandardObjectData item
        // func/strings); missing files are skipped by ReadFile.
        const char* itemFiles[] = {
            "Units\\ItemFunc.txt",
            "Units\\ItemStrings.txt",
            "Units\\ItemSkin.txt",
            "Units\\CommonItemFunc.txt",
            "Units\\CommonItemStrings.txt",
            "Units\\HumanItemFunc.txt",
            "Units\\HumanItemStrings.txt",
            "Units\\OrcItemFunc.txt",
            "Units\\OrcItemStrings.txt",
            "Units\\NightElfItemFunc.txt",
            "Units\\NightElfItemStrings.txt",
            "Units\\UndeadItemFunc.txt",
            "Units\\UndeadItemStrings.txt",
            "Units\\NeutralItemFunc.txt",
            "Units\\NeutralItemStrings.txt",
            "Units\\CampaignItemFunc.txt",
            "Units\\CampaignItemStrings.txt",
        };
        for (const char* f : itemFiles) {
            std::vector<uint8_t> buf;
            if (mpq.ReadFile(f, buf)) {
                skinTbl.Load(buf.data(), buf.size());
            }
        }
    }

    ApplyMapItemObjectMods(mpq, mapPath, itemSlk, skinTbl);

    const int nameColumn = FindFirstColumn(itemSlk, {"Name"});
    const int fileColumn = FindFirstColumn(itemSlk, {"file"});
    const int artColumn = FindFirstColumn(itemSlk, {"Art"});
    const int abilityListColumn = FindFirstColumn(itemSlk, {"abilList"});
    const int goldColumn = FindFirstColumn(itemSlk, {"goldcost", "goldCost"});
    const int lumberColumn = FindFirstColumn(itemSlk, {"lumbercost", "lumberCost"});
    const int maxLifeColumn = FindFirstColumn(itemSlk, {"HP"});
    const int levelColumn = FindFirstColumn(itemSlk, {"Level", "level"});
    const int classColumn = FindFirstColumn(itemSlk, {"class"});
    const int pickRandomColumn = FindFirstColumn(itemSlk, {"pickRandom"});
    const int chargesColumn = FindFirstColumn(itemSlk, {"uses", "charges"});
    const int perishableColumn = FindFirstColumn(itemSlk, {"perishable"});
    const int powerupColumn = FindFirstColumn(itemSlk, {"powerup"});
    const int useOnlyColumn = FindFirstColumn(itemSlk, {"usable", "useOnly"});
    const int ignoreCooldownColumn = FindFirstColumn(itemSlk, {"ignoreCD"});
    const int droppableColumn = FindFirstColumn(itemSlk, {"droppable"});
    const int pawnableColumn = FindFirstColumn(itemSlk, {"pawnable"});
    const int dropOnDeathColumn = FindFirstColumn(itemSlk, {"drop"});
    const int sellableColumn = FindFirstColumn(itemSlk, {"sellable"});
    const int stockColumn = FindFirstColumn(itemSlk, {"stock"});
    const int stockRegenColumn = FindFirstColumn(itemSlk, {"stockRegen"});
    const int stockMaxColumn = FindFirstColumn(itemSlk, {"stockMax"});
    const int cooldownColumn = FindFirstColumn(itemSlk, {"cooldownID", "coolID"});

    const SlkAccessor itemData(itemSlk, {"itemID", "unitID", "ID"});
    std::set<std::string> ids;
    CollectDestructableIds(itemData, skinTbl, ids);

    for (const std::string& sid : ids) {
        if (sid.size() != 4) continue;
        const auto rowIt = itemData.rowsById.find(sid);
        const int row = (rowIt != itemData.rowsById.end()) ? rowIt->second : -1;
        War3ID id = War3ID::FromString(sid);

        auto item = std::make_unique<CItemType>();
        item->id = id;
        item->name = GetMergedString(itemSlk, row, nameColumn, skinTbl, sid, "Name", sid);
        item->legacyName = skinTbl.GetString(sid, "LegacyName");
        if (item->legacyName.empty()) item->legacyName = item->name;
        item->modelPath = GetMergedString(itemSlk, row, fileColumn, skinTbl, sid, "file");
        item->iconPath = GetMergedString(itemSlk, row, artColumn, skinTbl, sid, "Art");
        ParseWar3IdList(GetMergedString(itemSlk, row, abilityListColumn, skinTbl, sid, "abilList"),
                        item->abilityList);
        item->goldCost = GetMergedInt(itemSlk, row, goldColumn, skinTbl, sid, "goldCost", 0);
        item->lumberCost = GetMergedInt(itemSlk, row, lumberColumn, skinTbl, sid, "lumberCost", 0);
        item->maxLife = GetMergedInt(itemSlk, row, maxLifeColumn, skinTbl, sid, "HP", 50);
        item->level = GetMergedInt(itemSlk, row, levelColumn, skinTbl, sid, "Level", 0);
        item->itemClass = ParseItemClass(GetMergedString(itemSlk, row, classColumn, skinTbl, sid, "class"));
        item->includeAsRandomChoice =
            GetMergedInt(itemSlk, row, pickRandomColumn, skinTbl, sid, "pickRandom", 0) != 0;
        item->charges = GetMergedInt(itemSlk, row, chargesColumn, skinTbl, sid, "charges", 0);
        item->perishable = GetMergedInt(itemSlk, row, perishableColumn, skinTbl, sid, "perishable", 0) != 0;
        item->powerup = GetMergedInt(itemSlk, row, powerupColumn, skinTbl, sid, "powerup", 0) != 0;
        item->useOnly = GetMergedInt(itemSlk, row, useOnlyColumn, skinTbl, sid, "useOnly", 0) != 0;
        item->ignoreCooldown = GetMergedInt(itemSlk, row, ignoreCooldownColumn, skinTbl, sid, "ignoreCD", 0) != 0;
        item->droppable = GetMergedInt(itemSlk, row, droppableColumn, skinTbl, sid, "droppable", 0) != 0;
        item->pawnable = GetMergedInt(itemSlk, row, pawnableColumn, skinTbl, sid, "pawnable", 0) != 0;
        item->dropOnDeath = GetMergedInt(itemSlk, row, dropOnDeathColumn, skinTbl, sid, "drop", 0) != 0;
        item->sellable = GetMergedInt(itemSlk, row, sellableColumn, skinTbl, sid, "sellable", 0) != 0;
        item->stockable = GetMergedInt(itemSlk, row, stockColumn, skinTbl, sid, "stock", 0) != 0;
        item->stockRegen = GetMergedInt(itemSlk, row, stockRegenColumn, skinTbl, sid, "stockRegen", 0);
        item->stockMax = GetMergedInt(itemSlk, row, stockMaxColumn, skinTbl, sid, "stockMax", 0);
        item->cooldownID = GetMergedInt(itemSlk, row, cooldownColumn, skinTbl, sid, "coolID", 0);

        if (item->includeAsRandomChoice) {
            auto& choices = data.levelToRandomChoices_[item->level];
            choices.unclassifiedItems.push_back(id);
            choices.classificationToItems[(int)item->itemClass].push_back(id);
        }

        data.items_[id] = std::move(item);
    }

    std::cout << "CItemData: loaded " << data.items_.size() << " items" << std::endl;
    return data;
}

bool CItemData::HasItem(War3ID id) const {
    return items_.find(id) != items_.end();
}

const CItemType* CItemData::GetItem(War3ID id) const {
    auto it = items_.find(id);
    return (it != items_.end()) ? it->second.get() : nullptr;
}

War3ID CItemData::ChooseRandomItem(int level, std::function<float()> rand01) const {
    auto it = levelToRandomChoices_.find(level);
    if (it == levelToRandomChoices_.end() || it->second.unclassifiedItems.empty()) {
        return War3ID();
    }
    const auto& items = it->second.unclassifiedItems;
    const size_t index = (size_t)(rand01() * (float)items.size()) % items.size();
    return items[index];
}

War3ID CItemData::ChooseRandomItem(CItemClass itemClass, int level, std::function<float()> rand01) const {
    auto it = levelToRandomChoices_.find(level);
    if (it == levelToRandomChoices_.end()) {
        std::cerr << "chooseRandomItem: no item set: " << level << std::endl;
        return War3ID();
    }
    auto classIt = it->second.classificationToItems.find((int)itemClass);
    if (classIt == it->second.classificationToItems.end() || classIt->second.empty()) {
        std::cerr << "chooseRandomItem: no items of class: " << (int)itemClass
                  << " " << level << std::endl;
        return War3ID();
    }
    const auto& items = classIt->second;
    const size_t index = (size_t)(rand01() * (float)items.size()) % items.size();
    return items[index];
}
