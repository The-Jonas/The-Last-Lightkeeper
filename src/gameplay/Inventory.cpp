#include "gameplay/Inventory.h"
#include "core/SaveData.h"

#include <algorithm>
#include <cmath>

void Inventory::ClearAll() {
    stacks.clear();
    activeIndex = -1;
    oilApplyMode = false;
    oilApplySourceIndex = -1;
    oilApplyReturnActiveIndex = 0;
    primedOilSpritePath.clear();
    usingDrainAccum = 0.0f;
    isLightToggledOn = false;
}

bool Inventory::AddItem(const ItemDef& def, int durability) {
    const bool wasEmpty = stacks.empty();
    ItemStack stack;
    stack.def = def;
    stack.count = 1;
    stack.durabilities.push_back(durability);
    stacks.push_back(stack);
    if (wasEmpty) {
        activeIndex = 0;
    }
    return true;
}

void Inventory::SetActiveIndex(int index) {
    activeIndex = index;
}

void Inventory::CycleLeft() {
    activeIndex--;
    // Moving the wheel deselects whatever was lit; the item must be re-activated
    // (press F) when it returns to the center.
    isLightToggledOn = false;
}

void Inventory::CycleRight() {
    activeIndex++;
    isLightToggledOn = false;
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

int Inventory::GetVisibleSlotCount() const {
    return (GetStackCount() >= 4) ? 5 : 3;
}

int Inventory::GetRingSize() const {
    return std::max(GetStackCount(), GetVisibleSlotCount());
}

int Inventory::GetSelectedStackIndex() const {
    const int count = GetStackCount();
    if (count <= 0) return -1;
    const int ring = GetRingSize();
    // The wheel renders its center slot as ((-activeIndex) mod ring); gameplay
    // must select that exact stack. Positions beyond the item count are empty.
    const int center = ((-activeIndex) % ring + ring) % ring;
    return (center < count) ? center : -1;
}

void Inventory::SetSelectedStackIndex(int stackIndex) {
    const int count = GetStackCount();
    if (count <= 0) {
        activeIndex = 0;
        return;
    }
    stackIndex = std::max(0, std::min(stackIndex, count - 1));
    const int ring = GetRingSize();
    // Solve ((-activeIndex) mod ring) == stackIndex, picking the representative
    // nearest the current activeIndex so the wheel takes the short way around.
    const int base = ((-stackIndex) % ring + ring) % ring;
    const int k = static_cast<int>(std::lround(static_cast<double>(activeIndex - base) / ring));
    activeIndex = base + k * ring;
}

const Inventory::ItemStack* Inventory::GetActiveStack() const {
    const int idx = GetSelectedStackIndex();
    if (idx < 0) return nullptr;
    return &stacks[static_cast<size_t>(idx)];
}

Inventory::ItemStack* Inventory::GetActiveStackMutable() {
    const int idx = GetSelectedStackIndex();
    if (idx < 0) return nullptr;
    return &stacks[static_cast<size_t>(idx)];
}

const Inventory::ItemStack* Inventory::GetStack(int index) const {
    if (index < 0 || index >= static_cast<int>(stacks.size())) return nullptr;
    return &stacks[static_cast<size_t>(index)];
}

int Inventory::GetPrimedOilDurability() const {
    if (!oilApplyMode) return 0;
    if (oilApplySourceIndex < 0 || oilApplySourceIndex >= static_cast<int>(stacks.size())) return 0;
    const std::vector<int>& dur = stacks[static_cast<size_t>(oilApplySourceIndex)].durabilities;
    return dur.empty() ? 0 : dur.front();
}

bool Inventory::CanCombineOilWithActive() const {
    if (!oilApplyMode) return false;
    const int targetIdx = GetSelectedStackIndex();
    if (targetIdx < 0 || targetIdx == oilApplySourceIndex) return false;  // empty slot or the oil itself
    const ItemStack& target = stacks[static_cast<size_t>(targetIdx)];
    if (!target.def.HasProperty(ItemProperty::LIGHT_SOURCE)) return false;
    if (target.durabilities.empty()) return false;
    const int maxDur = target.def.maxDurability;
    if (maxDur > 0 && target.durabilities.front() >= maxDur) return false;  // already full
    return true;
}

void Inventory::ExitOilApplyMode() {
    oilApplyMode = false;
    oilApplySourceIndex = -1;
    primedOilSpritePath.clear();
    // Return the wheel to the slot the oil was in when the player started.
    activeIndex = oilApplyReturnActiveIndex;
}

bool Inventory::TryPrimeOil() {
    if (oilApplyMode) return false;
    const int sel = GetSelectedStackIndex();
    if (sel < 0) return false;
    const ItemStack& active = stacks[static_cast<size_t>(sel)];
    if (!active.def.HasProperty(ItemProperty::FUEL)) return false;
    if (active.durabilities.empty() || active.durabilities.front() <= 0) return false;

    // Enter apply mode. The oil stays where it is; remember its slot so we can
    // return there (showing it, or an empty slot if it gets used up).
    oilApplyMode = true;
    oilApplySourceIndex = sel;
    oilApplyReturnActiveIndex = activeIndex;
    primedOilSpritePath = active.def.spritePath;
    return true;
}

bool Inventory::TryCombineOil() {
    if (!oilApplyMode) return false;
    if (oilApplySourceIndex < 0 || oilApplySourceIndex >= static_cast<int>(stacks.size())) {
        ExitOilApplyMode();
        return false;
    }

    const int targetIdx = GetSelectedStackIndex();
    if (targetIdx < 0 || targetIdx == oilApplySourceIndex) return false;  // empty slot or the oil itself

    ItemStack& target = stacks[static_cast<size_t>(targetIdx)];
    if (!target.def.HasProperty(ItemProperty::LIGHT_SOURCE)) return false;
    if (target.durabilities.empty()) return false;

    const int maxDur = target.def.maxDurability;
    int& targetDurability = target.durabilities.front();
    if (maxDur > 0 && targetDurability >= maxDur) return false;  // already full

    ItemStack& oil = stacks[static_cast<size_t>(oilApplySourceIndex)];
    if (oil.durabilities.empty() || oil.durabilities.front() <= 0) {
        ExitOilApplyMode();
        return false;
    }

    int oilAmount = oil.durabilities.front();
    const int room = (maxDur > 0) ? (maxDur - targetDurability) : oilAmount;
    const int transfer = std::min(room, oilAmount);
    targetDurability += transfer;
    oilAmount -= transfer;

    if (oilAmount > 0) {
        oil.durabilities.front() = oilAmount;
    } else {
        // Oil unit fully spent: remove it. The oil only leaves the wheel here,
        // when its durability hits 0.
        oil.durabilities.erase(oil.durabilities.begin());
        oil.count--;
        if (oil.count <= 0) {
            stacks.erase(stacks.begin() + oilApplySourceIndex);
        }
    }

    ExitOilApplyMode();
    return true;
}

void Inventory::CancelOil() {
    if (!oilApplyMode) return;
    ExitOilApplyMode();
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
    const int sel = GetSelectedStackIndex();
    if (sel < 0) return;
    ItemStack* active = &stacks[static_cast<size_t>(sel)];
    if (active->durabilities.empty()) return;
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
                stacks.erase(stacks.begin() + sel);
                if (!stacks.empty()) {
                    SetSelectedStackIndex(sel);
                }
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
        int sel = GetSelectedStackIndex();
        stacks.erase(stacks.begin() + idx);
        if (stacks.empty()) {
            activeIndex = 0;
        } else {
            if (sel > idx) {
                sel--;  // the selected item shifted down a slot
            } else if (sel < 0) {
                sel = 0;
            }
            SetSelectedStackIndex(sel);
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
    const int selectedIndex = GetSelectedStackIndex();
    state.activeStackIndex = selectedIndex;
    // Oil is now a normal fuel stack saved with the rest of the inventory; the
    // transient apply-mode state is not persisted.
    state.primedOilDurability = 0;

    state.selectedSlot = selectedIndex;
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
        if (stacks.empty()) {
            activeIndex = -1;
        } else {
            SetSelectedStackIndex(state.activeStackIndex);
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
        if (stacks.empty()) {
            activeIndex = -1;
        } else {
            SetSelectedStackIndex(state.selectedSlot);
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
        int sel = -1;
        if (state.selectedBackpackGroup >= 0 && state.selectedBackpackGroup < static_cast<int>(state.backpackGroups.size())) {
            const auto& oldGroup = state.backpackGroups[static_cast<size_t>(state.selectedBackpackGroup)];
            if (!oldGroup.items.empty()) {
                sel = FindStackWithName(oldGroup.items.front().name);
            }
        }
        if (sel < 0) {
            sel = FindBestFlashlight();
        }
        if (sel < 0 && !stacks.empty()) {
            sel = 0;
        }
        if (sel >= 0) {
            SetSelectedStackIndex(sel);
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
        int sel = -1;
        if (state.usingItem.has_value()) {
            sel = FindStackWithName(state.usingItem->name);
        }
        if (sel < 0) {
            sel = FindBestFlashlight();
        }
        if (sel < 0 && !stacks.empty()) {
            sel = 0;
        }
        if (sel >= 0) {
            SetSelectedStackIndex(sel);
        }
        return;
    }

    if (!stacks.empty()) {
        int sel = FindBestFlashlight();
        if (sel < 0) sel = 0;
        SetSelectedStackIndex(sel);
    }
}
