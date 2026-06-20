#ifndef INVENTORY_H
#define INVENTORY_H

#include "gameplay/BackpackConfig.h"
#include "gameplay/Item.h"
#include "lighting/LightMaskTypes.h"

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
    bool DeselectGroup();
    bool ToggleGroup(int groupIndex);
    int GetSelectedGroup() const { return selectedGroup; }
    bool HasSelection() const { return selectedGroup >= 0; }

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

    bool TryRefuelWithFuel();
    bool TryTurnLightOn();
    HeldPropVisual GetHeldPropVisual() const;
    bool IsUsableLightActive() const;
    bool IsActiveLightLamp() const;
    bool IsActiveLightLighter() const;
    float GetActiveLightFuelRatio() const;
    LightMaskParams BuildLighterLightParams(const LightMaskParams& base) const;
    LightMaskParams BuildLampLightParams(const LightMaskParams& base) const;
    void TickUsingDurability(float dt);

    void WriteToSave(struct SaveGameState& state) const;
    void ReadFromSave(const struct SaveGameState& state,
                      const std::vector<ItemDef>& itemCatalog);

    bool isLightToggledOn = false;

private:
    void SyncUsingSlotMirror();
    int FindBestItemIndexInGroup(int groupIndex) const;
    int FindLighterGroupIndex() const;
    int FindLampGroupIndex() const;
    int FindFuelGroupIndex() const;
    int FindRefuelTargetIndex(int groupIndex) const;
    int FindUsableFuelIndexInGroup(int fuelGroup) const;
    bool TryRefuelGroupItem(int targetGroup, int fuelGroup, int targetItemIndex = -1);
    bool CanEmitLight(const ItemInstance* active) const;

    BackpackConfig backpackConfig = DefaultBackpackConfig();
    std::vector<std::vector<ItemInstance>> groups;
    int selectedGroup = 0;
    int activeItemIndex = -1;
    std::optional<ItemInstance> usingSlot;
    float usingDrainAccum = 0.0f;
};

#endif
