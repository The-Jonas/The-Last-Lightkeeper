#ifndef INVENTORY_H
#define INVENTORY_H

#include "gameplay/BackpackConfig.h"
#include "gameplay/Item.h"

#include <optional>
#include <vector>

enum class HeldPropVisual { None, Lighter, Lamp };

class Inventory {
public:
    void ApplyBackpackConfig(const BackpackConfig& config);
    const BackpackConfig& GetBackpackConfig() const { return backpackConfig; }

    bool AddItemToGroup(int groupIndex, const ItemDef& def, int durability);
    bool AddItem(const ItemDef& def, int durability);
    void ClearAll();
    bool SelectGroup(int groupIndex);
    int GetSelectedGroup() const { return selectedGroup; }

    int CountInGroup(int groupIndex) const;
    bool IsGroupFull(int groupIndex) const;
    bool IsInventoryFull() const;
    int TotalItemCount() const;
    bool CanAcceptItem(const ItemDef& def) const;

    const ItemInstance* GetActiveItem() const;
    ItemInstance* GetActiveItemMutable();
    const ItemInstance* GetItemInGroup(int groupIndex, int itemIndex) const;
    const ItemInstance* GetUsing() const { return GetActiveItem(); }
    ItemInstance* GetUsingMutable() { return GetActiveItemMutable(); }
    bool IsUsingEmpty() const { return GetActiveItem() == nullptr; }

    bool TryRefuelLampFromOil();
    bool TryTurnLightOn();
    HeldPropVisual GetHeldPropVisual() const;
    bool IsUsableLightActive() const;
    void TickUsingDurability(float dt);

    void WriteToSave(struct SaveGameState& state) const;
    void ReadFromSave(const struct SaveGameState& state,
                      const std::vector<ItemDef>& itemCatalog);

    bool isLightToggledOn = false;

private:
    void SyncUsingSlotMirror();
    int FindBestItemIndexInGroup(int groupIndex) const;
    int FindLampGroupIndex() const;
    int FindOilGroupIndex() const;
    bool TryRefuelLampFromAnyOil();
    bool CanEmitLight(const ItemInstance* active) const;

    BackpackConfig backpackConfig = DefaultBackpackConfig();
    std::vector<std::vector<ItemInstance>> groups;
    int selectedGroup = 0;
    int activeItemIndex = -1;
    std::optional<ItemInstance> usingSlot;
    float usingDrainAccum = 0.0f;
};

#endif
