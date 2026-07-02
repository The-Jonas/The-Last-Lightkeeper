#ifndef INVENTORY_WHEEL_H
#define INVENTORY_WHEEL_H

#define INCLUDE_SDL
#include "SDL_include.h"
#include "engine/Component.h"
#include "gameplay/Inventory.h"

class InventoryWheel : public Component {
public:
    InventoryWheel(GameObject& associated, Inventory& inventory);

    void Start() override;
    void Update(float dt) override;
    void Render() override;

private:
    Inventory& inventory;
    float displayActiveIndex = 0.0f;
    int lastKnownActive = -1;
    float bobTimer = 0.0f;

    static constexpr float kSlotSize = 64.0f;
    static constexpr float kIconSize = 48.0f;
    static constexpr float kArcRadius = 110.0f;
    static constexpr float kAnchorXFraction = 0.12f;
    static constexpr float kAnchorYOffset = -100.0f;
    static constexpr float kSlideSpeed = 10.0f;
    static constexpr float kGaugeHeight = 4.0f;

    float GetAnchorX() const;
    float GetAnchorY() const;
    void GetSlotScreenPos(int visibleOffset, int visibleCount, float scrollOffset,
                          float& outX, float& outY) const;
    float GetSlotAlpha(int distanceFromCenter, int visibleCount) const;
    float GetSlotScale(int distanceFromCenter, int visibleCount) const;
    void DrawSlot(SDL_Renderer* renderer, int stackIndex, float x, float y,
                  float alpha, float scale, bool isActive) const;
    void DrawPrimedOil(SDL_Renderer* renderer, float activeX, float activeY);
};

#endif
