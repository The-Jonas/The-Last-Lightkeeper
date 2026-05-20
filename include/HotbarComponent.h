#ifndef HOTBAR_COMPONENT_H
#define HOTBAR_COMPONENT_H

#define INCLUDE_SDL
#include "SDL_include.h"

#include "Component.h"
#include "Inventory.h"
#include "Item.h"
#include "Sprite.h"
#include "Vec2.h"

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
                    std::function<void(GameObject*)> addObjFn,
                    std::function<Vec2(Vec2 topLeft, float itemW, float itemH)> clampPickupTopLeft = {});

    /// Screen-space: use slot / wheel (so UI clicks don't unlock the preview light).
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
    std::function<Vec2(Vec2, float, float)> clampPickupTopLeft;

    Sprite* slotSprites[Inventory::kSlots];
    /// Bottom-center use slot when wheel is closed; not used for layout when wheel is open (see wheel positions).
    SDL_Rect usingSlotRect;

    bool inventoryOpen = false;
    int dragSourceSlot;
    bool isDragging;
    Sprite* dragSprite;

    float toastTimer;
    static constexpr float kToastDuration = 2.0f;

    /// Ícone dentro do círculo (menor = mais slots na roda).
    static constexpr int kSlotSize = 34;
    static constexpr int kSlotGap = 4;
    /// Anel extra em torno do círculo dos pés: mesmo alcance para o prompt e para pegar (E).
    static constexpr float kPickupPromptFootRadiusExtra = 18.0f;

    /// 6 slots no anel (inventário 0..5) + centro = uso (índice 6 no hit test).
    static constexpr int kWheelRingSlots = 6;
    static constexpr int kWheelCenterIndex = 6;

    static constexpr float kWheelRadiusPx = 118.0f;
    static constexpr float kWheelSlotDrawRadius = 28.0f;
    static constexpr float kWheelHitRadius = 30.0f;

    float wheelRingCenterX[kWheelRingSlots];
    float wheelRingCenterY[kWheelRingSlots];
    float wheelCenterX = 0.0f;
    float wheelCenterY = 0.0f;
    float closedUseCenterX = 0.0f;
    float closedUseCenterY = 0.0f;

    bool dragFromUsing = false;

    float GetPickupReachRadius() const;

    void RecalcLayout();
    /// Fechado: só slot de uso (retorna kWheelCenterIndex). Aberto: 0..5 anel, 6 centro, -1 fora.
    int HitTestInventorySlot(int mouseX, int mouseY) const;
    bool PointInCircle(int mx, int my, float cx, float cy, float radius) const;

    void HandleDragRelease(int targetWheelIndex);

    SDL_Rect SlotRectAtCenter(float cx, float cy, int size) const;
};

#endif
