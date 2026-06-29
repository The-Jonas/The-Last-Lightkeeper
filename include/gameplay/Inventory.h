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

    bool IsOilPrimed() const { return primedOilDurability > 0; }
    int GetPrimedOilDurability() const { return primedOilDurability; }
    const std::string& GetPrimedOilSpritePath() const { return primedOilSpritePath; }
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
    int primedOilDurability = 0;
    std::string primedOilSpritePath;
    float usingDrainAccum = 0.0f;

    int FindBestFlashlight() const;
    int FindBestLamp() const;
};

#endif
