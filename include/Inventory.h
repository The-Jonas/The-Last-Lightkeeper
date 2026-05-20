#ifndef INVENTORY_H
#define INVENTORY_H

#include "Item.h"

#include <optional>

class Inventory {
public:
    static constexpr int kSlots = 20;
    static constexpr int kHotbarSlots = 6;
    static constexpr int kCols = 5;
    static constexpr int kRows = 4;

    bool AddItem(const ItemDef& def, int durability);
    void RemoveItem(int slot);
    void SwapSlots(int a, int b);
    int DropItem(int slot);
    bool IsFull() const;
    bool IsEmpty(int slot) const;
    const ItemInstance* GetSlot(int slot) const;
    void MoveItem(int from, int to);

    // Hand / "using" slot (separate from backpack slots 0–19).
    const ItemInstance* GetUsing() const;
    bool IsUsingEmpty() const;
    void ClearUsing();
    void SetUsing(ItemInstance item);
    void SwapUsingAndSlot(int slot);
    std::optional<ItemInstance> TakeUsing();

    bool IsUsableLightActive() const;
    void TickUsingDurability(float dt);

private:
    std::optional<ItemInstance> slots[kSlots];
    std::optional<ItemInstance> usingSlot;
    float usingDrainAccum = 0.0f;
};

#endif
