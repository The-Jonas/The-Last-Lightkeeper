#ifndef HOTBAR_COMPONENT_H
#define HOTBAR_COMPONENT_H

#define INCLUDE_SDL
#include "SDL_include.h"

#include "Component.h"
#include "Inventory.h"
#include "Item.h"
#include "Sprite.h"

#include <functional>
#include <string>
#include <vector>

class Character;
class ItemPickup;

class HotbarComponent : public Component {
public:
    HotbarComponent(GameObject& associated, Inventory& inventory,
                    Character* bigChar, Character** controlledChar,
                    std::vector<ItemPickup*>& pickups,
                    std::function<void(GameObject*)> addObjFn);

    /// Screen-space: hotbar bar or open inventory panel (used so UI clicks don't unlock the preview light).
    bool BlocksLightPointerUnlock(int screenX, int screenY) const;

    void Start() override;
    void Update(float dt) override;
    void Render() override;

private:
    Inventory& inventory;
    Character* bigCharacter;
    Character** controlledCharacterPtr;
    std::vector<ItemPickup*>& itemPickups;
    std::function<void(GameObject*)> addObjectToState;

    Sprite* slotSprites[Inventory::kSlots];
    SDL_Rect hotbarRects[Inventory::kHotbarSlots];
    SDL_Rect invRects[Inventory::kSlots];
    SDL_Rect invBgRect;

    bool inventoryOpen = false;
    int dragSourceSlot;
    bool isDragging;
    Sprite* dragSprite;

    float toastTimer;
    static constexpr float kToastDuration = 2.0f;

    static constexpr int kSlotSize = 48;
    static constexpr int kSlotGap = 4;
    static constexpr float kPickupRange = 48.0f;

    void RecalcSlotRects();
    void RecalcInvRect();
    int HitTestHotbarSlot(int mouseX, int mouseY) const;
    int HitTestInvSlot(int mouseX, int mouseY) const;
    void RenderInvPopup(SDL_Renderer* renderer);
    void HandleDragRelease(int targetSlot);
    void UpdateSlotSprites();
    void ClearSlotSprites();
};

#endif
