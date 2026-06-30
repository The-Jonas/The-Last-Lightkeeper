#ifndef INVENTORY_H
#define INVENTORY_H

#include "gameplay/Item.h"
#include "lighting/LightMaskTypes.h"

#include <string>
#include <vector>

enum class HeldPropVisual { None, Lighter, Lamp };

class Inventory {
public:
    struct ItemStack {
        ItemDef def;
        int count;
        std::vector<int> durabilities;
    };

    static constexpr int kMaxStackSize = 5;

    bool AddItem(const ItemDef& def, int durability);
    void ClearAll();

    int GetActiveIndex() const { return activeIndex; }
    void SetActiveIndex(int index);
    void CycleLeft();
    void CycleRight();

    const ItemStack* GetActiveStack() const;
    ItemStack* GetActiveStackMutable();
    const ItemStack* GetStack(int index) const;
    int GetStackCount() const { return static_cast<int>(stacks.size()); }
    int GetVisibleStackIndex(int visibleOffset, int visibleCount) const;

    // Wheel geometry shared by the UI and gameplay so the item the player can
    // use is always the one shown at the center of the wheel. activeIndex is an
    // unbounded cycle counter (kept continuous for the wheel animation); the
    // selected stack is derived from it, never indexed directly.
    int GetVisibleSlotCount() const;
    int GetRingSize() const;
    int GetSelectedStackIndex() const;
    void SetSelectedStackIndex(int stackIndex);

    // "Oil apply" mode: pressing F on a fuel item enters a state where the
    // player cycles to a lighter/lamp and presses F again to pour the oil in.
    // The oil is NOT removed from the wheel while applying; it stays in its slot
    // and only disappears once its own durability hits 0.
    bool IsOilPrimed() const { return oilApplyMode; }
    int GetPrimedOilDurability() const;
    const std::string& GetPrimedOilSpritePath() const { return primedOilSpritePath; }
    // True when the currently centered slot is a valid oil target (a non-empty
    // lighter/lamp that still has room). Drives the faded vs. bright oil icon.
    bool CanCombineOilWithActive() const;
    bool TryPrimeOil();
    bool TryCombineOil();
    void CancelOil();

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

    bool CanAcceptItem(const ItemDef& def) const;
    bool HasItem(const std::string& name) const;
    bool TryConsumeItem(const std::string& name);
    int FindStackWithName(const std::string& name) const;
    int FindStackWithProperty(ItemProperty prop) const;

    bool isLightToggledOn = false;

private:
    std::vector<ItemStack> stacks;
    int activeIndex = -1;
    bool oilApplyMode = false;
    int oilApplySourceIndex = -1;        // stack index of the oil being applied
    int oilApplyReturnActiveIndex = 0;   // wheel position to restore on exit
    std::string primedOilSpritePath;
    float usingDrainAccum = 0.0f;

    void ExitOilApplyMode();

    int FindBestFlashlight() const;
    int FindBestLamp() const;
};

#endif
