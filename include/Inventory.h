#ifndef INVENTORY_H
#define INVENTORY_H

#include "Item.h"

#include <optional>

class Inventory {
public:
    static constexpr int kSlots = 20;
    static constexpr int kHotbarSlots = 5;
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

private:
    std::optional<ItemInstance> slots[kSlots];
};

#endif
