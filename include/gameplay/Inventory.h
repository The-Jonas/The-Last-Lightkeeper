#ifndef INVENTORY_H
#define INVENTORY_H

#include "gameplay/Item.h"
#include "lighting/LightMaskTypes.h"

#include <optional>
#include <vector>

enum class HeldPropVisual { None, Lighter, Lamp };

class Inventory {
public:
    static constexpr int kGridWidth = 5;
    static constexpr int kGridHeight = 5;
    static constexpr int kTotalSlots = kGridWidth * kGridHeight;

    bool AddItem(const ItemDef& def, int durability);
    void ClearAll();
    bool SelectSlot(int slotIndex);
    void DeselectSlot();
    int GetSelectedSlot() const { return selectedSlot; }
    bool HasSelection() const { return selectedSlot >= 0; }

    int TotalItemCount() const;
    bool IsInventoryFull() const;
    bool CanAcceptItem(const ItemDef& def) const;

    void MoveItem(int fromSlot, int toSlot);
    bool TryCombineOil(int oilSlot, int lightSlot);
    void RemoveFromSlot(int slotIndex);

    const ItemInstance* GetSelectedItem() const;
    ItemInstance* GetSelectedItemMutable();
    const ItemInstance* GetItem(int slotIndex) const;
    const std::optional<ItemInstance>& GetSlot(int slotIndex) const;

    int FindBestFlashlight() const;
    int FindBestLamp() const;
    int FindFirstFlashlight() const;
    int FindFirstLamp() const;
    int FindSlotWithFuel() const;
    int FindSlotWithName(const std::string& name) const;
    bool HasItem(const std::string& name) const;

    bool TryTurnLightOn();
    HeldPropVisual GetHeldPropVisual() const;
    bool IsUsableLightActive() const;
    bool IsActiveLightLamp() const;
    bool IsActiveLightLighter() const;
    float GetSelectedLightFuelRatio() const;
    LightMaskParams BuildLighterLightParams(const LightMaskParams& base) const;
    LightMaskParams BuildLampLightParams(const LightMaskParams& base) const;
    void TickUsingDurability(float dt);

    void WriteToSave(struct SaveGameState& state) const;
    void ReadFromSave(const struct SaveGameState& state,
                      const std::vector<ItemDef>& itemCatalog);

    bool isLightToggledOn = false;

private:
    void SyncUsingSlotMirror();
    bool IsLightSlot(int slotIndex) const;

    std::vector<std::optional<ItemInstance>> slots;
    int selectedSlot = -1;
    std::optional<ItemInstance> usingSlot;
    float usingDrainAccum = 0.0f;
};

#endif
