#include "gameplay/Inventory.h"
#include "core/SaveData.h"

#include <algorithm>

void Inventory::ClearAll() {
    stacks.clear();
    activeIndex = -1;
    primedOilDurability = 0;
    primedOilSpritePath.clear();
    usingDrainAccum = 0.0f;
    isLightToggledOn = false;
}

bool Inventory::AddItem(const ItemDef& def, int durability) {
    ItemStack stack;
    stack.def = def;
    stack.count = 1;
    stack.durabilities.push_back(durability);
    stacks.push_back(stack);
    if (activeIndex < 0) {
        activeIndex = 0;
    }
    return true;
}

void Inventory::SetActiveIndex(int index) {
    activeIndex = index;
}

void Inventory::CycleLeft() {
    activeIndex--;
}

void Inventory::CycleRight() {
    activeIndex++;
}

int Inventory::GetVisibleStackIndex(int visibleOffset, int visibleCount) const {
    if (stacks.empty()) return -1;
    int half = visibleCount / 2;
    int idx = activeIndex - half + visibleOffset;
    int n = static_cast<int>(stacks.size());
    while (idx < 0) idx += n;
    idx = idx % n;
    return idx;
}

const Inventory::ItemStack* Inventory::GetActiveStack() const {
    if (activeIndex < 0 || activeIndex >= static_cast<int>(stacks.size())) return nullptr;
    return &stacks[static_cast<size_t>(activeIndex)];
}

Inventory::ItemStack* Inventory::GetActiveStackMutable() {
    if (activeIndex < 0 || activeIndex >= static_cast<int>(stacks.size())) return nullptr;
    return &stacks[static_cast<size_t>(activeIndex)];
}

const Inventory::ItemStack* Inventory::GetStack(int index) const {
    if (index < 0 || index >= static_cast<int>(stacks.size())) return nullptr;
    return &stacks[static_cast<size_t>(index)];
}

bool Inventory::TryPrimeOil() {
    if (IsOilPrimed()) return false;
    ItemStack* active = GetActiveStackMutable();
    if (!active) return false;
    if (!active->def.HasProperty(ItemProperty::FUEL)) return false;
    if (active->durabilities.empty()) return false;

    int durability = active->durabilities.front();
    if (durability <= 0) return false;

    primedOilDurability = durability;
    primedOilSpritePath = active->def.spritePath;

    active->durabilities.erase(active->durabilities.begin());
    active->count--;

    if (active->count <= 0) {
        stacks.erase(stacks.begin() + activeIndex);
    }

    return true;
}

bool Inventory::TryCombineOil() {
    if (!IsOilPrimed()) return false;

    ItemStack* active = GetActiveStackMutable();
    if (!active) return false;
    if (!active->def.HasProperty(ItemProperty::LIGHT_SOURCE)) return false;
    if (active->durabilities.empty()) return false;

    int& targetDurability = active->durabilities.front();
    int maxDur = active->def.maxDurability;

    if (maxDur > 0 && targetDurability >= maxDur) return false;

    targetDurability += primedOilDurability;
    if (maxDur > 0 && targetDurability > maxDur) {
        int overflow = targetDurability - maxDur;
        primedOilDurability = overflow;
        targetDurability = maxDur;
    } else {
        primedOilDurability = 0;
        primedOilSpritePath.clear();
    }
    return true;
}

void Inventory::CancelOil() {
    primedOilDurability = 0;
    primedOilSpritePath.clear();
}

bool Inventory::TryTurnLightOn() {
    const ItemStack* active = GetActiveStack();
    if (!active || !active->def.HasProperty(ItemProperty::LIGHT_SOURCE)) return false;
    if (!active->durabilities.empty() && active->durabilities.front() > 0) {
        isLightToggledOn = true;
        return true;
    }
    return false;
}

HeldPropVisual Inventory::GetHeldPropVisual() const {
    const ItemStack* active = GetActiveStack();
    if (!active || !active->def.HasProperty(ItemProperty::LIGHT_SOURCE)) {
        return HeldPropVisual::None;
    }
    const std::string& name = active->def.name;
    if (name == "Flashlight" || name == "Broken Flashlight") {
        return HeldPropVisual::Lighter;
    }
    if (name == "Lamp") {
        return HeldPropVisual::Lamp;
    }
    return HeldPropVisual::None;
}

bool Inventory::IsUsableLightActive() const {
    const ItemStack* active = GetActiveStack();
    if (!active || !isLightToggledOn) return false;
    if (active->durabilities.empty() || active->durabilities.front() <= 0) return false;
    return active->def.HasProperty(ItemProperty::LIGHT_SOURCE);
}

bool Inventory::IsActiveLightLamp() const {
    if (!IsUsableLightActive()) return false;
    const ItemStack* active = GetActiveStack();
    return active && active->def.name == "Lamp";
}

bool Inventory::IsActiveLightLighter() const {
    if (!IsUsableLightActive()) return false;
    const ItemStack* active = GetActiveStack();
    return active && (active->def.name == "Flashlight" || active->def.name == "Broken Flashlight");
}

float Inventory::GetSelectedLightFuelRatio() const {
    const ItemStack* active = GetActiveStack();
    if (!active || !active->def.HasProperty(ItemProperty::LIGHT_SOURCE)) return 0.0f;
    if (active->def.maxDurability <= 0) return 1.0f;
    if (active->durabilities.empty()) return 0.0f;
    float ratio = static_cast<float>(active->durabilities.front()) / static_cast<float>(active->def.maxDurability);
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
    if (!IsUsableLightActive()) return;
    ItemStack* active = GetActiveStackMutable();
    if (!active || active->durabilities.empty()) return;
    if (active->def.maxDurability <= 0 || active->durabilities.front() <= 0) return;

    const float drainInterval = (active->def.name == "Lamp") ? 2.0f : 1.0f;
    usingDrainAccum += dt;
    while (usingDrainAccum >= drainInterval && !active->durabilities.empty() && active->durabilities.front() > 0) {
        active->durabilities.front() -= 1;
        usingDrainAccum -= drainInterval;
        if (active->durabilities.front() <= 0) {
            active->durabilities.erase(active->durabilities.begin());
            active->count--;
            if (active->count <= 0) {
                stacks.erase(stacks.begin() + activeIndex);
                isLightToggledOn = false;
                break;
            }
        }
    }
}

bool Inventory::HasItem(const std::string& name) const {
    return FindStackWithName(name) >= 0;
}

bool Inventory::CanAcceptItem(const ItemDef&) const {
    return true;
}

bool Inventory::TryConsumeItem(const std::string& name) {
    int idx = FindStackWithName(name);
    if (idx < 0) return false;
    ItemStack& stack = stacks[static_cast<size_t>(idx)];
    if (stack.count <= 0) return false;

    stack.count--;
    if (!stack.durabilities.empty()) {
        stack.durabilities.erase(stack.durabilities.begin());
    }

    if (stack.count <= 0) {
        stacks.erase(stacks.begin() + idx);
        if (activeIndex >= static_cast<int>(stacks.size())) {
            activeIndex = stacks.empty() ? -1 : static_cast<int>(stacks.size()) - 1;
        } else if (activeIndex > idx) {
            activeIndex--;
        }
    }

    return true;
}

int Inventory::FindStackWithName(const std::string& name) const {
    for (size_t i = 0; i < stacks.size(); ++i) {
        if (stacks[i].def.name == name) return static_cast<int>(i);
    }
    return -1;
}

int Inventory::FindStackWithProperty(ItemProperty prop) const {
    for (size_t i = 0; i < stacks.size(); ++i) {
        if (stacks[i].def.HasProperty(prop)) return static_cast<int>(i);
    }
    return -1;
}

int Inventory::FindBestFlashlight() const {
    int best = -1;
    int bestDur = -1;
    for (size_t i = 0; i < stacks.size(); ++i) {
        if (!stacks[i].def.HasProperty(ItemProperty::LIGHT_SOURCE)) continue;
        const std::string& name = stacks[i].def.name;
        if (name != "Flashlight" && name != "Broken Flashlight") continue;
        int dur = stacks[i].durabilities.empty() ? 0 : stacks[i].durabilities.front();
        if (dur > bestDur) { bestDur = dur; best = static_cast<int>(i); }
    }
    return best;
}

int Inventory::FindBestLamp() const {
    int best = -1;
    int bestDur = -1;
    for (size_t i = 0; i < stacks.size(); ++i) {
        if (!stacks[i].def.HasProperty(ItemProperty::LIGHT_SOURCE)) continue;
        if (stacks[i].def.name != "Lamp") continue;
        int dur = stacks[i].durabilities.empty() ? 0 : stacks[i].durabilities.front();
        if (dur > bestDur) { bestDur = dur; best = static_cast<int>(i); }
    }
    return best;
}

namespace {

const ItemDef* FindItemDefByName(const std::string& name, const std::vector<ItemDef>& catalog) {
    for (const ItemDef& def : catalog) {
        if (def.name == name) return &def;
    }
    if (name == "Lamp Fuel" || name == "Lighter Fuel" || name == "Light Fuel" || name == "Oil Gallon") {
        for (const ItemDef& def : catalog) {
            if (def.name == "Fuel") return &def;
        }
    }
    return nullptr;
}

} // namespace

void Inventory::WriteToSave(SaveGameState& state) const {
    state.lightOn = isLightToggledOn;

    state.inventoryStacks.clear();
    for (const ItemStack& stack : stacks) {
        SavedInventoryStack saved;
        saved.name = stack.def.name;
        saved.count = stack.count;
        saved.durabilities = stack.durabilities;
        state.inventoryStacks.push_back(saved);
    }
    state.activeStackIndex = activeIndex;
    state.primedOilDurability = primedOilDurability;

    state.selectedSlot = activeIndex;
    state.inventorySlots.clear();
    state.selectedBackpackGroup = -1;
    state.backpackGroups.clear();
    state.usingItem.reset();
    state.slots.clear();
}

void Inventory::ReadFromSave(const SaveGameState& state, const std::vector<ItemDef>& itemCatalog) {
    ClearAll();

    isLightToggledOn = state.lightOn;

    if (!state.inventoryStacks.empty()) {
        for (const SavedInventoryStack& saved : state.inventoryStacks) {
            const ItemDef* def = FindItemDefByName(saved.name, itemCatalog);
            if (!def) continue;
            ItemStack stack;
            stack.def = *def;
            stack.count = saved.count;
            stack.durabilities = saved.durabilities;
            stacks.push_back(stack);
        }
        activeIndex = state.activeStackIndex;
        if (activeIndex < 0 || activeIndex >= static_cast<int>(stacks.size())) {
            activeIndex = stacks.empty() ? -1 : 0;
        }
        primedOilDurability = state.primedOilDurability;
        if (primedOilDurability > 0) {
            const ItemDef* oilDef = FindItemDefByName("Fuel", itemCatalog);
            if (oilDef) {
                primedOilSpritePath = oilDef->spritePath;
            }
        }
        return;
    }

    if (!state.inventorySlots.empty()) {
        for (size_t i = 0; i < state.inventorySlots.size() && i < 25; ++i) {
            if (!state.inventorySlots[i].has_value()) continue;
            const ItemDef* def = FindItemDefByName(state.inventorySlots[i]->name, itemCatalog);
            if (!def) continue;
            AddItem(*def, state.inventorySlots[i]->durability);
        }
        activeIndex = state.selectedSlot;
        if (activeIndex < 0 || activeIndex >= static_cast<int>(stacks.size())) {
            activeIndex = stacks.empty() ? -1 : 0;
        }
        return;
    }

    if (!state.backpackGroups.empty()) {
        for (const SavedBackpackGroup& group : state.backpackGroups) {
            for (const SavedItemSlot& saved : group.items) {
                const ItemDef* def = FindItemDefByName(saved.name, itemCatalog);
                if (def) AddItem(*def, saved.durability);
            }
        }
        if (state.selectedBackpackGroup >= 0 && state.selectedBackpackGroup < static_cast<int>(state.backpackGroups.size())) {
            const auto& oldGroup = state.backpackGroups[static_cast<size_t>(state.selectedBackpackGroup)];
            if (!oldGroup.items.empty()) {
                activeIndex = FindStackWithName(oldGroup.items.front().name);
            }
        }
        if (activeIndex < 0) {
            activeIndex = FindBestFlashlight();
        }
        if (activeIndex < 0 && !stacks.empty()) {
            activeIndex = 0;
        }
        return;
    }

    if (!state.slots.empty()) {
        for (size_t i = 0; i < state.slots.size() && i < 25; ++i) {
            if (!state.slots[i].has_value()) continue;
            const ItemDef* def = FindItemDefByName(state.slots[i]->name, itemCatalog);
            if (!def) continue;
            AddItem(*def, state.slots[i]->durability);
        }
        if (state.usingItem.has_value()) {
            activeIndex = FindStackWithName(state.usingItem->name);
        }
        if (activeIndex < 0) {
            activeIndex = FindBestFlashlight();
        }
        if (activeIndex < 0 && !stacks.empty()) {
            activeIndex = 0;
        }
        return;
    }

    if (!stacks.empty()) {
        activeIndex = FindBestFlashlight();
        if (activeIndex < 0) activeIndex = 0;
    }
}
