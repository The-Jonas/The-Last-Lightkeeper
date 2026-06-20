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

int Inventory::FindLighterGroupIndex() const {
    const int byId = backpackConfig.GroupIndexForId("lighter");
    if (byId >= 0) {
        return byId;
    }
    return backpackConfig.GroupIndexForItem("Flashlight");
}

int Inventory::FindLampGroupIndex() const {
    for (size_t i = 0; i < backpackConfig.groups.size(); ++i) {
        if (backpackConfig.groups[i].id == "lamp") {
            return static_cast<int>(i);
        }
    }
    return backpackConfig.GroupIndexForItem("Lamp");
}

int Inventory::FindFuelGroupIndex() const {
    for (size_t i = 0; i < backpackConfig.groups.size(); ++i) {
        const std::string& id = backpackConfig.groups[i].id;
        if (id == "fuel" || id == "oil") {
            return static_cast<int>(i);
        }
    }
    return backpackConfig.GroupIndexForItem("Fuel");
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

int Inventory::FindRefuelTargetIndex(int groupIndex) const {
    if (groupIndex < 0 || groupIndex >= static_cast<int>(groups.size())) {
        return -1;
    }
    int best = -1;
    int bestDur = 2147483647;
    for (size_t i = 0; i < groups[groupIndex].size(); ++i) {
        const ItemInstance& item = groups[groupIndex][i];
        if (!item.def.HasProperty(ItemProperty::LIGHT_SOURCE)) {
            continue;
        }
        if (item.def.maxDurability > 0 && item.durability >= item.def.maxDurability) {
            continue;
        }
        if (item.durability < bestDur) {
            bestDur = item.durability;
            best = static_cast<int>(i);
        }
    }
    return best;
}

int Inventory::FindUsableFuelIndexInGroup(int fuelGroup) const {
    if (fuelGroup < 0 || fuelGroup >= static_cast<int>(groups.size())) {
        return -1;
    }
    int best = -1;
    int bestRemaining = 2147483647;
    for (size_t i = 0; i < groups[fuelGroup].size(); ++i) {
        const int remaining = groups[fuelGroup][i].durability;
        if (remaining <= 0) {
            continue;
        }
        if (remaining < bestRemaining) {
            bestRemaining = remaining;
            best = static_cast<int>(i);
        }
    }
    return best;
}

bool Inventory::TryRefuelGroupItem(int targetGroup, int fuelGroup, int targetItemIndex) {
    if (targetGroup < 0 || fuelGroup < 0) {
        return false;
    }
    if (targetGroup >= static_cast<int>(groups.size()) || fuelGroup >= static_cast<int>(groups.size())) {
        return false;
    }
    if (groups[targetGroup].empty() || groups[fuelGroup].empty()) {
        return false;
    }

    int itemIdx = targetItemIndex;
    if (itemIdx < 0) {
        itemIdx = FindRefuelTargetIndex(targetGroup);
    }
    if (itemIdx < 0 || itemIdx >= static_cast<int>(groups[targetGroup].size())) {
        return false;
    }

    ItemInstance& target = groups[targetGroup][static_cast<size_t>(itemIdx)];
    if (!target.def.HasProperty(ItemProperty::LIGHT_SOURCE)) {
        return false;
    }
    if (target.def.maxDurability > 0 && target.durability >= target.def.maxDurability) {
        return false;
    }

    int fuelIndex = -1;
    if (selectedGroup == fuelGroup && activeItemIndex >= 0 &&
        activeItemIndex < static_cast<int>(groups[fuelGroup].size()) &&
        groups[fuelGroup][static_cast<size_t>(activeItemIndex)].durability > 0) {
        fuelIndex = activeItemIndex;
    } else {
        fuelIndex = FindUsableFuelIndexInGroup(fuelGroup);
    }
    if (fuelIndex < 0) {
        return false;
    }

    ItemInstance& fuelItem = groups[fuelGroup][static_cast<size_t>(fuelIndex)];
    if (fuelItem.durability <= 0) {
        return false;
    }

    int needed = fuelItem.durability;
    if (target.def.maxDurability > 0) {
        needed = target.def.maxDurability - target.durability;
    }
    if (needed <= 0) {
        return false;
    }

    const int transfer = std::min(needed, fuelItem.durability);
    if (transfer <= 0) {
        return false;
    }

    target.durability += transfer;
    if (target.def.maxDurability > 0 && target.durability > target.def.maxDurability) {
        target.durability = target.def.maxDurability;
    }

    fuelItem.durability -= transfer;
    if (fuelItem.durability <= 0) {
        groups[fuelGroup].erase(groups[fuelGroup].begin() + fuelIndex);
        if (selectedGroup == fuelGroup) {
            if (activeItemIndex == fuelIndex) {
                activeItemIndex = FindUsableFuelIndexInGroup(fuelGroup);
            } else if (activeItemIndex > fuelIndex) {
                activeItemIndex -= 1;
            }
            if (activeItemIndex < 0) {
                activeItemIndex = -1;
            }
        }
    } else if (selectedGroup == targetGroup) {
        activeItemIndex = FindBestItemIndexInGroup(targetGroup);
    }
    SyncUsingSlotMirror();
    return true;
}

bool Inventory::TryRefuelWithFuel() {
    const int fuelGroup = FindFuelGroupIndex();
    if (fuelGroup < 0 || groups[fuelGroup].empty()) {
        return false;
    }

    const ItemInstance* active = GetActiveItem();
    if (!active) {
        return false;
    }

    if (active->def.HasProperty(ItemProperty::LIGHT_SOURCE)) {
        return TryRefuelGroupItem(selectedGroup, fuelGroup, activeItemIndex);
    }

    if (active->def.HasProperty(ItemProperty::FUEL) && selectedGroup == fuelGroup) {
        const int lighterGroup = FindLighterGroupIndex();
        const int lampGroup = FindLampGroupIndex();
        if (TryRefuelGroupItem(lighterGroup, fuelGroup)) {
            return true;
        }
        if (TryRefuelGroupItem(lampGroup, fuelGroup)) {
            return true;
        }
    }

    return false;
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

bool Inventory::IsActiveLightLamp() const {
    if (!IsUsableLightActive()) {
        return false;
    }
    const BackpackGroupDef* group = backpackConfig.GetGroup(selectedGroup);
    return group && group->id == "lamp";
}

bool Inventory::IsActiveLightLighter() const {
    if (!IsUsableLightActive()) {
        return false;
    }
    const BackpackGroupDef* group = backpackConfig.GetGroup(selectedGroup);
    return group && group->id == "lighter";
}

float Inventory::GetActiveLightFuelRatio() const {
    const ItemInstance* active = GetActiveItem();
    if (!active || !active->def.HasProperty(ItemProperty::LIGHT_SOURCE)) {
        return 0.0f;
    }
    if (active->def.maxDurability <= 0) {
        return 1.0f;
    }
    const float ratio =
        static_cast<float>(active->durability) / static_cast<float>(active->def.maxDurability);
    return std::max(0.0f, std::min(1.0f, ratio));
}

LightMaskParams Inventory::BuildLighterLightParams(const LightMaskParams& base) const {
    LightMaskParams params = base;
    const float fuelRatio = GetActiveLightFuelRatio();
    params.falloffRadiusPx *= fuelRatio;
    params.shadowMaxLengthPx *= fuelRatio;
    params.coneLengthPx *= fuelRatio;
    return params;
}

LightMaskParams Inventory::BuildLampLightParams(const LightMaskParams& base) const {
    LightMaskParams params = base;
    constexpr float kLampVsLighterScale = 1.40f;
    const float sizeScale = GetActiveLightFuelRatio() * kLampVsLighterScale;
    params.falloffRadiusPx *= sizeScale;
    params.shadowMaxLengthPx *= sizeScale;
    params.coneLengthPx *= sizeScale;
    return params;
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
    if (name == "Lamp Fuel" || name == "Lighter Fuel" || name == "Light Fuel" || name == "Oil Gallon") {
        for (const ItemDef& def : catalog) {
            if (def.name == "Fuel") {
                return &def;
            }
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
