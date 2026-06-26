#include "gameplay/Inventory.h"
#include "core/SaveData.h"

#include <algorithm>

void Inventory::ClearAll() {
    slots.clear();
    slots.resize(kTotalSlots);
    selectedSlot = -1;
    usingSlot.reset();
    usingDrainAccum = 0.0f;
    isLightToggledOn = false;
}

bool Inventory::AddItem(const ItemDef& def, int durability) {
    for (int i = 0; i < kTotalSlots; ++i) {
        if (!slots[static_cast<size_t>(i)].has_value()) {
            slots[static_cast<size_t>(i)] = ItemInstance{def, durability};
            return true;
        }
    }
    return false;
}

int Inventory::TotalItemCount() const {
    int total = 0;
    for (const auto& slot : slots) {
        if (slot.has_value()) {
            total++;
        }
    }
    return total;
}

bool Inventory::IsInventoryFull() const {
    return TotalItemCount() >= kTotalSlots;
}

bool Inventory::CanAcceptItem(const ItemDef& def) const {
    (void)def;
    return !IsInventoryFull();
}

bool Inventory::SelectSlot(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= kTotalSlots) {
        return false;
    }
    if (!slots[static_cast<size_t>(slotIndex)].has_value()) {
        return false;
    }
    if (slotIndex == selectedSlot) {
        return false;
    }
    selectedSlot = slotIndex;
    isLightToggledOn = false;
    SyncUsingSlotMirror();
    return true;
}

void Inventory::DeselectSlot() {
    selectedSlot = -1;
    isLightToggledOn = false;
    usingSlot.reset();
}

void Inventory::MoveItem(int fromSlot, int toSlot) {
    if (fromSlot < 0 || fromSlot >= kTotalSlots || toSlot < 0 || toSlot >= kTotalSlots) {
        return;
    }
    std::swap(slots[static_cast<size_t>(fromSlot)], slots[static_cast<size_t>(toSlot)]);
    if (selectedSlot == fromSlot) {
        selectedSlot = toSlot;
    } else if (selectedSlot == toSlot) {
        selectedSlot = fromSlot;
    }
    SyncUsingSlotMirror();
}

bool Inventory::TryCombineOil(int oilSlot, int lightSlot) {
    if (oilSlot < 0 || oilSlot >= kTotalSlots || lightSlot < 0 || lightSlot >= kTotalSlots) {
        return false;
    }
    auto& oilOpt = slots[static_cast<size_t>(oilSlot)];
    auto& lightOpt = slots[static_cast<size_t>(lightSlot)];
    if (!oilOpt.has_value() || !lightOpt.has_value()) {
        return false;
    }
    ItemInstance& oil = *oilOpt;
    ItemInstance& light = *lightOpt;
    if (!oil.def.HasProperty(ItemProperty::FUEL)) {
        return false;
    }
    if (!light.def.HasProperty(ItemProperty::LIGHT_SOURCE)) {
        return false;
    }
    if (oil.durability <= 0) {
        return false;
    }
    if (light.def.maxDurability > 0 && light.durability >= light.def.maxDurability) {
        return false;
    }

    int needed = oil.durability;
    if (light.def.maxDurability > 0) {
        needed = light.def.maxDurability - light.durability;
    }
    if (needed <= 0) {
        return false;
    }

    const int transfer = std::min(needed, oil.durability);
    if (transfer <= 0) {
        return false;
    }

    light.durability += transfer;
    if (light.def.maxDurability > 0 && light.durability > light.def.maxDurability) {
        light.durability = light.def.maxDurability;
    }

    oil.durability -= transfer;
    if (oil.durability <= 0) {
        oilOpt.reset();
    }

    SyncUsingSlotMirror();
    return true;
}

void Inventory::RemoveFromSlot(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= kTotalSlots) {
        return;
    }
    slots[static_cast<size_t>(slotIndex)].reset();
    if (selectedSlot == slotIndex) {
        selectedSlot = -1;
        isLightToggledOn = false;
    }
    SyncUsingSlotMirror();
}

const ItemInstance* Inventory::GetSelectedItem() const {
    if (selectedSlot < 0 || selectedSlot >= kTotalSlots) {
        return nullptr;
    }
    const auto& slot = slots[static_cast<size_t>(selectedSlot)];
    return slot.has_value() ? &*slot : nullptr;
}

ItemInstance* Inventory::GetSelectedItemMutable() {
    if (selectedSlot < 0 || selectedSlot >= kTotalSlots) {
        return nullptr;
    }
    auto& slot = slots[static_cast<size_t>(selectedSlot)];
    return slot.has_value() ? &*slot : nullptr;
}

const ItemInstance* Inventory::GetItem(int slotIndex) const {
    if (slotIndex < 0 || slotIndex >= kTotalSlots) {
        return nullptr;
    }
    const auto& slot = slots[static_cast<size_t>(slotIndex)];
    return slot.has_value() ? &*slot : nullptr;
}

const std::optional<ItemInstance>& Inventory::GetSlot(int slotIndex) const {
    static const std::optional<ItemInstance> empty;
    if (slotIndex < 0 || slotIndex >= kTotalSlots) {
        return empty;
    }
    return slots[static_cast<size_t>(slotIndex)];
}

bool Inventory::IsLightSlot(int slotIndex) const {
    const ItemInstance* item = GetItem(slotIndex);
    return item && item->def.HasProperty(ItemProperty::LIGHT_SOURCE);
}

int Inventory::FindBestFlashlight() const {
    int best = -1;
    int bestDur = -1;
    for (int i = 0; i < kTotalSlots; ++i) {
        const ItemInstance* item = GetItem(i);
        if (!item || !item->def.HasProperty(ItemProperty::LIGHT_SOURCE)) {
            continue;
        }
        const std::string& name = item->def.name;
        if (name != "Flashlight" && name != "Broken Flashlight") {
            continue;
        }
        if (item->durability > bestDur) {
            bestDur = item->durability;
            best = i;
        }
    }
    return best;
}

int Inventory::FindBestLamp() const {
    int best = -1;
    int bestDur = -1;
    for (int i = 0; i < kTotalSlots; ++i) {
        const ItemInstance* item = GetItem(i);
        if (!item || !item->def.HasProperty(ItemProperty::LIGHT_SOURCE)) {
            continue;
        }
        if (item->def.name != "Lamp") {
            continue;
        }
        if (item->durability > bestDur) {
            bestDur = item->durability;
            best = i;
        }
    }
    return best;
}

int Inventory::FindFirstFlashlight() const {
    for (int i = 0; i < kTotalSlots; ++i) {
        const ItemInstance* item = GetItem(i);
        if (item && item->def.HasProperty(ItemProperty::LIGHT_SOURCE)) {
            const std::string& name = item->def.name;
            if (name == "Flashlight" || name == "Broken Flashlight") {
                return i;
            }
        }
    }
    return -1;
}

int Inventory::FindFirstLamp() const {
    for (int i = 0; i < kTotalSlots; ++i) {
        const ItemInstance* item = GetItem(i);
        if (item && item->def.HasProperty(ItemProperty::LIGHT_SOURCE) && item->def.name == "Lamp") {
            return i;
        }
    }
    return -1;
}

int Inventory::FindSlotWithFuel() const {
    for (int i = 0; i < kTotalSlots; ++i) {
        const ItemInstance* item = GetItem(i);
        if (item && item->def.HasProperty(ItemProperty::FUEL) && item->durability > 0) {
            return i;
        }
    }
    return -1;
}

int Inventory::FindSlotWithName(const std::string& name) const {
    for (int i = 0; i < kTotalSlots; ++i) {
        const ItemInstance* item = GetItem(i);
        if (item && item->def.name == name) {
            return i;
        }
    }
    return -1;
}

bool Inventory::HasItem(const std::string& name) const {
    return FindSlotWithName(name) >= 0;
}

void Inventory::SyncUsingSlotMirror() {
    usingSlot.reset();
    const ItemInstance* selected = GetSelectedItem();
    if (selected) {
        usingSlot = *selected;
    }
}

bool Inventory::TryTurnLightOn() {
    const ItemInstance* selected = GetSelectedItem();
    if (!selected || !selected->def.HasProperty(ItemProperty::LIGHT_SOURCE)) {
        return false;
    }
    if (selected->durability > 0) {
        isLightToggledOn = true;
        return true;
    }
    return false;
}

HeldPropVisual Inventory::GetHeldPropVisual() const {
    const ItemInstance* selected = GetSelectedItem();
    if (!selected || !selected->def.HasProperty(ItemProperty::LIGHT_SOURCE)) {
        return HeldPropVisual::None;
    }
    const std::string& name = selected->def.name;
    if (name == "Flashlight" || name == "Broken Flashlight") {
        return HeldPropVisual::Lighter;
    }
    if (name == "Lamp") {
        return HeldPropVisual::Lamp;
    }
    return HeldPropVisual::None;
}

bool Inventory::IsUsableLightActive() const {
    const ItemInstance* selected = GetSelectedItem();
    if (!selected || !isLightToggledOn) {
        return false;
    }
    return selected->durability > 0 && selected->def.HasProperty(ItemProperty::LIGHT_SOURCE);
}

bool Inventory::IsActiveLightLamp() const {
    if (!IsUsableLightActive()) {
        return false;
    }
    const ItemInstance* selected = GetSelectedItem();
    return selected && selected->def.name == "Lamp";
}

bool Inventory::IsActiveLightLighter() const {
    if (!IsUsableLightActive()) {
        return false;
    }
    const ItemInstance* selected = GetSelectedItem();
    return selected && (selected->def.name == "Flashlight" || selected->def.name == "Broken Flashlight");
}

float Inventory::GetSelectedLightFuelRatio() const {
    const ItemInstance* selected = GetSelectedItem();
    if (!selected || !selected->def.HasProperty(ItemProperty::LIGHT_SOURCE)) {
        return 0.0f;
    }
    if (selected->def.maxDurability <= 0) {
        return 1.0f;
    }
    const float ratio =
        static_cast<float>(selected->durability) / static_cast<float>(selected->def.maxDurability);
    return std::max(0.0f, std::min(1.0f, ratio));
}

LightMaskParams Inventory::BuildLighterLightParams(const LightMaskParams& base) const {
    LightMaskParams params = base;
    const float fuelRatio = GetSelectedLightFuelRatio();
    params.falloffRadiusPx *= fuelRatio;
    params.shadowMaxLengthPx *= fuelRatio;
    params.coneLengthPx *= fuelRatio;
    return params;
}

LightMaskParams Inventory::BuildLampLightParams(const LightMaskParams& base) const {
    LightMaskParams params = base;
    constexpr float kLampVsLighterScale = 1.40f;
    const float sizeScale = GetSelectedLightFuelRatio() * kLampVsLighterScale;
    params.falloffRadiusPx *= sizeScale;
    params.shadowMaxLengthPx *= sizeScale;
    params.coneLengthPx *= sizeScale;
    return params;
}

void Inventory::TickUsingDurability(float dt) {
    if (!IsUsableLightActive()) {
        return;
    }
    ItemInstance* selected = GetSelectedItemMutable();
    if (!selected) {
        return;
    }
    if (selected->def.maxDurability <= 0 || selected->durability <= 0) {
        return;
    }
    const float drainInterval = (selected->def.name == "Lamp") ? 2.0f : 1.0f;
    usingDrainAccum += dt;
    while (usingDrainAccum >= drainInterval && selected->durability > 0) {
        selected->durability -= 1;
        usingDrainAccum -= drainInterval;
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

bool IsFlashlightName(const std::string& name) {
    return name == "Flashlight" || name == "Broken Flashlight";
}

bool IsLampName(const std::string& name) {
    return name == "Lamp";
}

bool IsFuelName(const std::string& name) {
    return name == "Fuel" || name == "Lamp Fuel" || name == "Lighter Fuel" ||
           name == "Light Fuel" || name == "Oil Gallon";
}

const ItemDef* FindFlashlightDef(const std::vector<ItemDef>& catalog) {
    for (const ItemDef& def : catalog) {
        if (IsFlashlightName(def.name)) {
            return &def;
        }
    }
    return nullptr;
}

const ItemDef* FindLampDef(const std::vector<ItemDef>& catalog) {
    for (const ItemDef& def : catalog) {
        if (IsLampName(def.name)) {
            return &def;
        }
    }
    return nullptr;
}

} // namespace

void Inventory::WriteToSave(SaveGameState& state) const {
    state.lightOn = isLightToggledOn;
    state.selectedSlot = selectedSlot;

    state.inventorySlots.clear();
    state.inventorySlots.resize(kTotalSlots);
    for (size_t i = 0; i < slots.size(); ++i) {
        if (slots[i].has_value()) {
            state.inventorySlots[i] = SavedItemSlot{slots[i]->def.name, slots[i]->durability};
        } else {
            state.inventorySlots[i] = std::nullopt;
        }
    }

    state.selectedBackpackGroup = -1;
    state.backpackGroups.clear();

    state.usingItem.reset();
    if (const ItemInstance* selected = GetSelectedItem()) {
        state.usingItem = SavedItemSlot{selected->def.name, selected->durability};
    }
}

void Inventory::ReadFromSave(const SaveGameState& state, const std::vector<ItemDef>& itemCatalog) {
    ClearAll();

    isLightToggledOn = state.lightOn;

    if (!state.inventorySlots.empty()) {
        for (size_t i = 0; i < state.inventorySlots.size() && i < static_cast<size_t>(kTotalSlots); ++i) {
            if (!state.inventorySlots[i].has_value()) {
                continue;
            }
            const ItemDef* def = FindItemDefByName(state.inventorySlots[i]->name, itemCatalog);
            if (def) {
                slots[i] = ItemInstance{*def, state.inventorySlots[i]->durability};
            }
        }
        selectedSlot = state.selectedSlot;
        if (selectedSlot < 0 || selectedSlot >= kTotalSlots || !slots[static_cast<size_t>(selectedSlot)].has_value()) {
            selectedSlot = -1;
        }
    } else if (!state.backpackGroups.empty()) {
        int slotIdx = 0;
        for (const SavedBackpackGroup& group : state.backpackGroups) {
            for (const SavedItemSlot& saved : group.items) {
                if (slotIdx >= kTotalSlots) {
                    break;
                }
                const ItemDef* def = FindItemDefByName(saved.name, itemCatalog);
                if (def) {
                    slots[static_cast<size_t>(slotIdx)] = ItemInstance{*def, saved.durability};
                    slotIdx++;
                }
            }
            if (slotIdx >= kTotalSlots) {
                break;
            }
        }
        selectedSlot = state.selectedBackpackGroup;
        if (state.selectedBackpackGroup >= 0 && state.selectedBackpackGroup < static_cast<int>(state.backpackGroups.size())) {
            const int oldGroupIndex = state.selectedBackpackGroup;
            const auto& oldGroup = state.backpackGroups[static_cast<size_t>(oldGroupIndex)];
            if (!oldGroup.items.empty()) {
                const ItemDef* selDef = FindItemDefByName(oldGroup.items.front().name, itemCatalog);
                if (selDef) {
                    if (IsFlashlightName(oldGroup.items.front().name)) {
                        selectedSlot = FindFirstFlashlight();
                    } else if (IsLampName(oldGroup.items.front().name)) {
                        selectedSlot = FindFirstLamp();
                    } else {
                        selectedSlot = FindSlotWithFuel();
                    }
                }
            }
        }
        if (selectedSlot < 0 || selectedSlot >= kTotalSlots) {
            selectedSlot = FindFirstFlashlight();
        }
    } else {
        for (size_t i = 0; i < state.slots.size() && i < static_cast<size_t>(kTotalSlots); ++i) {
            if (!state.slots[i].has_value()) {
                continue;
            }
            const ItemDef* def = FindItemDefByName(state.slots[i]->name, itemCatalog);
            if (def) {
                slots[i] = ItemInstance{*def, state.slots[i]->durability};
            }
        }
        if (state.usingItem.has_value()) {
            const ItemDef* def = FindItemDefByName(state.usingItem->name, itemCatalog);
            if (def) {
                for (int i = 0; i < kTotalSlots; ++i) {
                    if (slots[i].has_value() && slots[i]->def.name == def->name &&
                        slots[i]->durability == state.usingItem->durability) {
                        selectedSlot = i;
                        break;
                    }
                }
                if (selectedSlot < 0) {
                    if (IsFlashlightName(state.usingItem->name)) {
                        selectedSlot = FindFirstFlashlight();
                    } else if (IsLampName(state.usingItem->name)) {
                        selectedSlot = FindFirstLamp();
                    }
                }
            }
        }
    }

    if (selectedSlot < 0) {
        selectedSlot = FindFirstFlashlight();
    }
    SyncUsingSlotMirror();
}
