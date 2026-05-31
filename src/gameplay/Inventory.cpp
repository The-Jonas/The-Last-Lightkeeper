#include "gameplay/Inventory.h"
#include "core/SaveData.h"

bool Inventory::AddItem(const ItemDef& def, int durability) {
    for (int i = 0; i < kSlots; i++) {
        if (!slots[i].has_value()) {
            slots[i] = ItemInstance{def, durability};
            return true;
        }
    }
    return false;
}

void Inventory::RemoveItem(int slot) {
    if (slot >= 0 && slot < kSlots) {
        slots[slot].reset();
    }
}

void Inventory::SwapSlots(int a, int b) {
    if (a < 0 || a >= kSlots || b < 0 || b >= kSlots) {
        return;
    }
    std::swap(slots[a], slots[b]);
}

int Inventory::DropItem(int slot) {
    if (slot < 0 || slot >= kSlots || !slots[slot].has_value()) {
        return -1;
    }
    int dur = slots[slot]->durability;
    slots[slot].reset();
    return dur;
}

bool Inventory::IsFull() const {
    for (int i = 0; i < kSlots; i++) {
        if (!slots[i].has_value()) {
            return false;
        }
    }
    return true;
}

bool Inventory::IsEmpty(int slot) const {
    if (slot < 0 || slot >= kSlots) {
        return true;
    }
    return !slots[slot].has_value();
}

const ItemInstance* Inventory::GetSlot(int slot) const {
    if (slot < 0 || slot >= kSlots || !slots[slot].has_value()) {
        return nullptr;
    }
    return &slots[slot].value();
}

void Inventory::MoveItem(int from, int to) {
    if (from < 0 || from >= kSlots || to < 0 || to >= kSlots) return;
    if (from == to) return;
    if (!slots[from].has_value()) return;
    slots[to] = std::move(slots[from]);
    slots[from].reset();
}

const ItemInstance* Inventory::GetUsing() const {
    if (!usingSlot.has_value()) {
        return nullptr;
    }
    return &usingSlot.value();
}

bool Inventory::IsUsingEmpty() const {
    return !usingSlot.has_value();
}

void Inventory::ClearUsing() {
    usingSlot.reset();
    usingDrainAccum = 0.0f;
}

void Inventory::SetUsing(ItemInstance item) {
    usingSlot = std::move(item);
    usingDrainAccum = 0.0f;
}

void Inventory::SwapUsingAndSlot(int slot) {
    if (slot < 0 || slot >= kSlots) {
        return;
    }
    std::swap(usingSlot, slots[slot]);
    usingDrainAccum = 0.0f;
}

std::optional<ItemInstance> Inventory::TakeUsing() {
    if (!usingSlot.has_value()) {
        return std::nullopt;
    }
    ItemInstance out = std::move(usingSlot.value());
    usingSlot.reset();
    usingDrainAccum = 0.0f;
    return out;
}

bool Inventory::IsUsableLightActive() const {
    if (!usingSlot.has_value()) {
        return false;
    }

    if (!isLightToggledOn) {
        return false;
    }
    
    const ItemInstance& u = usingSlot.value();
    return u.durability > 0 && u.def.HasProperty(ItemProperty::LIGHT_SOURCE);
}

void Inventory::TickUsingDurability(float dt) {
    if (!usingSlot.has_value()) {
        return;
    }
    ItemInstance& u = usingSlot.value();
    if (u.def.maxDurability <= 0 || u.durability <= 0) {
        return;
    }
    usingDrainAccum += dt;
    while (usingDrainAccum >= 1.0f && u.durability > 0) {
        u.durability -= 1;
        usingDrainAccum -= 1.0f;
    }
}

ItemInstance* Inventory::GetSlotMutable(int slot) {
    if (slot < 0 || slot >= kSlots || !slots[slot].has_value()) return nullptr;
    return &slots[slot].value();
}

ItemInstance* Inventory::GetUsingMutable() {
    if (!usingSlot.has_value()) return nullptr;
    return &usingSlot.value();
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
    state.usingItem.reset();
    if (usingSlot.has_value()) {
        state.usingItem = SavedItemSlot{usingSlot->def.name, usingSlot->durability};
    }
    state.slots.clear();
    state.slots.resize(kSlots);
    for (int i = 0; i < kSlots; ++i) {
        if (slots[i].has_value()) {
            state.slots[i] = SavedItemSlot{slots[i]->def.name, slots[i]->durability};
        }
    }
}

void Inventory::ReadFromSave(const SaveGameState& state, const std::vector<ItemDef>& itemCatalog) {
    isLightToggledOn = state.lightOn;
    usingSlot.reset();
    usingDrainAccum = 0.0f;
    for (int i = 0; i < kSlots; ++i) {
        slots[i].reset();
    }

    if (state.usingItem.has_value()) {
        const ItemDef* def = FindItemDefByName(state.usingItem->name, itemCatalog);
        if (def) {
            usingSlot = ItemInstance{*def, state.usingItem->durability};
        }
    }

    for (int i = 0; i < kSlots && i < static_cast<int>(state.slots.size()); ++i) {
        if (!state.slots[i].has_value()) {
            continue;
        }
        const ItemDef* def = FindItemDefByName(state.slots[i]->name, itemCatalog);
        if (def) {
            slots[i] = ItemInstance{*def, state.slots[i]->durability};
        }
    }
}
