#include "gameplay/Inventory.h"
#include "core/SaveData.h"

#include <algorithm>

void Inventory::ApplyBackpackConfig(const BackpackConfig& config) {
    backpackConfig = config;
    groups.resize(backpackConfig.groups.size());
}

void Inventory::ClearAll() {
    for (auto& group : groups) {
        group.clear();
    }
    selectedGroup = -1;
    activeItemIndex = -1;
    usingSlot.reset();
    usingDrainAccum = 0.0f;
    isLightToggledOn = false;
}

int Inventory::FindLampGroupIndex() const {
    for (size_t i = 0; i < backpackConfig.groups.size(); ++i) {
        if (backpackConfig.groups[i].id == "lamp") {
            return static_cast<int>(i);
        }
    }
    return backpackConfig.GroupIndexForItem("Lamp");
}

int Inventory::FindOilGroupIndex() const {
    for (size_t i = 0; i < backpackConfig.groups.size(); ++i) {
        if (backpackConfig.groups[i].id == "oil") {
            return static_cast<int>(i);
        }
    }
    return backpackConfig.GroupIndexForItem("Oil Gallon");
}

int Inventory::FindBestItemIndexInGroup(int groupIndex) const {
    if (groupIndex < 0 || groupIndex >= static_cast<int>(groups.size()) || groups[groupIndex].empty()) {
        return -1;
    }
    int best = 0;
    int bestDur = groups[groupIndex][0].durability;
    for (size_t i = 1; i < groups[groupIndex].size(); ++i) {
        if (groups[groupIndex][i].durability > bestDur) {
            bestDur = groups[groupIndex][i].durability;
            best = static_cast<int>(i);
        }
    }
    return best;
}

void Inventory::SyncUsingSlotMirror() {
    usingSlot.reset();
    const ItemInstance* active = GetActiveItem();
    if (active) {
        usingSlot = *active;
    }
}

bool Inventory::SelectGroup(int groupIndex) {
    if (groupIndex < 0 || groupIndex >= static_cast<int>(groups.size())) {
        return false;
    }
    if (groups[groupIndex].empty()) {
        return false;
    }
    if (groupIndex == selectedGroup) {
        return false;
    }
    selectedGroup = groupIndex;
    activeItemIndex = FindBestItemIndexInGroup(groupIndex);
    isLightToggledOn = false;
    SyncUsingSlotMirror();
    return true;
}

bool Inventory::DeselectGroup() {
    if (selectedGroup < 0) {
        return false;
    }
    selectedGroup = -1;
    activeItemIndex = -1;
    isLightToggledOn = false;
    usingSlot.reset();
    return true;
}

bool Inventory::ToggleGroup(int groupIndex) {
    if (groupIndex < 0 || groupIndex >= static_cast<int>(groups.size())) {
        return false;
    }
    if (groups[groupIndex].empty()) {
        return false;
    }
    if (groupIndex == selectedGroup) {
        return DeselectGroup();
    }
    return SelectGroup(groupIndex);
}

bool Inventory::AddItemToGroup(int groupIndex, const ItemDef& def, int durability) {
    if (groupIndex < 0 || groupIndex >= static_cast<int>(groups.size())) {
        return false;
    }
    if (IsGroupFull(groupIndex)) {
        return false;
    }
    groups[groupIndex].push_back(ItemInstance{def, durability});
    if (selectedGroup == groupIndex) {
        activeItemIndex = FindBestItemIndexInGroup(groupIndex);
        SyncUsingSlotMirror();
    }
    return true;
}

bool Inventory::AddItem(const ItemDef& def, int durability) {
    const int groupIndex = backpackConfig.GroupIndexForItem(def.name);
    if (groupIndex < 0) {
        return false;
    }
    return AddItemToGroup(groupIndex, def, durability);
}

int Inventory::CountInGroup(int groupIndex) const {
    if (groupIndex < 0 || groupIndex >= static_cast<int>(groups.size())) {
        return 0;
    }
    return static_cast<int>(groups[groupIndex].size());
}

bool Inventory::IsGroupFull(int groupIndex) const {
    return CountInGroup(groupIndex) >= backpackConfig.MaxItemsInGroup(groupIndex);
}

int Inventory::TotalItemCount() const {
    int total = 0;
    for (const auto& group : groups) {
        total += static_cast<int>(group.size());
    }
    return total;
}

bool Inventory::IsInventoryFull() const {
    return TotalItemCount() >= backpackConfig.TotalCapacity();
}

bool Inventory::CanAcceptItem(const ItemDef& def) const {
    const int groupIndex = backpackConfig.GroupIndexForItem(def.name);
    if (groupIndex < 0) {
        return false;
    }
    return !IsGroupFull(groupIndex);
}

const ItemInstance* Inventory::GetActiveItem() const {
    if (selectedGroup < 0 || selectedGroup >= static_cast<int>(groups.size())) {
        return nullptr;
    }
    if (activeItemIndex < 0 || activeItemIndex >= static_cast<int>(groups[selectedGroup].size())) {
        return nullptr;
    }
    return &groups[selectedGroup][static_cast<size_t>(activeItemIndex)];
}

const ItemInstance* Inventory::GetItemInGroup(int groupIndex, int itemIndex) const {
    if (groupIndex < 0 || groupIndex >= static_cast<int>(groups.size())) {
        return nullptr;
    }
    if (itemIndex < 0 || itemIndex >= static_cast<int>(groups[groupIndex].size())) {
        return nullptr;
    }
    return &groups[groupIndex][static_cast<size_t>(itemIndex)];
}

ItemInstance* Inventory::GetActiveItemMutable() {
    if (selectedGroup < 0 || selectedGroup >= static_cast<int>(groups.size())) {
        return nullptr;
    }
    if (activeItemIndex < 0 || activeItemIndex >= static_cast<int>(groups[selectedGroup].size())) {
        return nullptr;
    }
    return &groups[selectedGroup][static_cast<size_t>(activeItemIndex)];
}

bool Inventory::TryRefuelLampFromAnyOil() {
    const int lampGroup = FindLampGroupIndex();
    if (lampGroup < 0 || groups[lampGroup].empty()) {
        return false;
    }

    ItemInstance& lamp = groups[lampGroup][0];
    if (lamp.durability >= lamp.def.maxDurability) {
        return false;
    }

    const int oilGroup = FindOilGroupIndex();
    if (oilGroup < 0 || groups[oilGroup].empty()) {
        return false;
    }

    const int oilIndex = FindBestItemIndexInGroup(oilGroup);
    if (oilIndex < 0) {
        return false;
    }

    float fuelValue = 100.0f;
    for (const auto& p : groups[oilGroup][static_cast<size_t>(oilIndex)].def.properties) {
        if (p.first == ItemProperty::FUEL) {
            fuelValue = p.second;
        }
    }

    lamp.durability += static_cast<int>(fuelValue);
    if (lamp.durability > lamp.def.maxDurability) {
        lamp.durability = lamp.def.maxDurability;
    }

    groups[oilGroup].erase(groups[oilGroup].begin() + oilIndex);
    if (selectedGroup == oilGroup) {
        activeItemIndex = FindBestItemIndexInGroup(oilGroup);
    } else if (selectedGroup == lampGroup) {
        activeItemIndex = FindBestItemIndexInGroup(lampGroup);
    }
    SyncUsingSlotMirror();
    return true;
}

bool Inventory::CanEmitLight(const ItemInstance* active) const {
    return active && active->durability > 0 && active->def.HasProperty(ItemProperty::LIGHT_SOURCE);
}

bool Inventory::TryTurnLightOn() {
    const ItemInstance* active = GetActiveItem();
    if (!active || !active->def.HasProperty(ItemProperty::LIGHT_SOURCE)) {
        return false;
    }
    if (CanEmitLight(active)) {
        isLightToggledOn = true;
        return true;
    }
    return false;
}

bool Inventory::TryRefuelLampFromOil() {
    const ItemInstance* active = GetActiveItem();
    if (!active || !active->def.HasProperty(ItemProperty::FUEL)) {
        return false;
    }

    const int oilGroup = FindOilGroupIndex();
    if (oilGroup < 0 || oilGroup != selectedGroup) {
        return false;
    }

    return TryRefuelLampFromAnyOil();
}

HeldPropVisual Inventory::GetHeldPropVisual() const {
    if (selectedGroup < 0) {
        return HeldPropVisual::None;
    }
    const ItemInstance* active = GetActiveItem();
    if (!active || !active->def.HasProperty(ItemProperty::LIGHT_SOURCE)) {
        return HeldPropVisual::None;
    }
    const BackpackGroupDef* group = backpackConfig.GetGroup(selectedGroup);
    if (!group) {
        return HeldPropVisual::None;
    }
    if (group->id == "lighter") {
        return HeldPropVisual::Lighter;
    }
    if (group->id == "lamp") {
        return HeldPropVisual::Lamp;
    }
    return HeldPropVisual::None;
}

bool Inventory::IsUsableLightActive() const {
    const ItemInstance* active = GetActiveItem();
    if (!active || !isLightToggledOn) {
        return false;
    }
    return active->durability > 0 && active->def.HasProperty(ItemProperty::LIGHT_SOURCE);
}

void Inventory::TickUsingDurability(float dt) {
    if (!IsUsableLightActive()) {
        return;
    }
    ItemInstance* active = GetActiveItemMutable();
    if (!active) {
        return;
    }
    if (active->def.maxDurability <= 0 || active->durability <= 0) {
        return;
    }
    usingDrainAccum += dt;
    while (usingDrainAccum >= 1.0f && active->durability > 0) {
        active->durability -= 1;
        usingDrainAccum -= 1.0f;
    }
    SyncUsingSlotMirror();
}

namespace {

const ItemDef* FindItemDefByName(const std::string& name, const std::vector<ItemDef>& catalog) {
    for (const ItemDef& def : catalog) {
        if (def.name == name) {
            return &def;
        }
    }
    return nullptr;
}

} // namespace

void Inventory::WriteToSave(SaveGameState& state) const {
    state.lightOn = isLightToggledOn;
    state.selectedBackpackGroup = selectedGroup;
    state.backpackGroups.clear();
    state.backpackGroups.resize(backpackConfig.groups.size());

    for (size_t gi = 0; gi < groups.size(); ++gi) {
        SavedBackpackGroup saved;
        if (gi < backpackConfig.groups.size()) {
            saved.groupId = backpackConfig.groups[gi].id;
        }
        for (const ItemInstance& item : groups[gi]) {
            saved.items.push_back(SavedItemSlot{item.def.name, item.durability});
        }
        state.backpackGroups[gi] = std::move(saved);
    }

    state.usingItem.reset();
    if (const ItemInstance* active = GetActiveItem()) {
        state.usingItem = SavedItemSlot{active->def.name, active->durability};
    }
}

void Inventory::ReadFromSave(const SaveGameState& state, const std::vector<ItemDef>& itemCatalog) {
    ClearAll();
    isLightToggledOn = state.lightOn;
    selectedGroup = state.selectedBackpackGroup;

    if (!state.backpackGroups.empty()) {
        for (size_t gi = 0; gi < state.backpackGroups.size() && gi < groups.size(); ++gi) {
            for (const SavedItemSlot& saved : state.backpackGroups[gi].items) {
                const ItemDef* def = FindItemDefByName(saved.name, itemCatalog);
                if (def && !IsGroupFull(static_cast<int>(gi))) {
                    groups[gi].push_back(ItemInstance{*def, saved.durability});
                }
            }
        }
    } else {
        for (size_t i = 0; i < state.slots.size(); ++i) {
            if (!state.slots[i].has_value()) {
                continue;
            }
            const ItemDef* def = FindItemDefByName(state.slots[i]->name, itemCatalog);
            if (!def) {
                continue;
            }
            AddItem(*def, state.slots[i]->durability);
        }
        if (state.usingItem.has_value()) {
            const ItemDef* def = FindItemDefByName(state.usingItem->name, itemCatalog);
            if (def) {
                const int gi = backpackConfig.GroupIndexForItem(def->name);
                if (gi >= 0) {
                    SelectGroup(gi);
                }
            }
        }
    }

    if (selectedGroup < 0 || selectedGroup >= static_cast<int>(groups.size())) {
        selectedGroup = -1;
    }
    activeItemIndex = FindBestItemIndexInGroup(selectedGroup);
    SyncUsingSlotMirror();
}
