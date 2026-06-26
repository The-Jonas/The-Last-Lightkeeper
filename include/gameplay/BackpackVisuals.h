#ifndef BACKPACK_VISUALS_H
#define BACKPACK_VISUALS_H

#define INCLUDE_SDL
#include "SDL_include.h"
#include "engine/Component.h"

class Character;
class Inventory;

class BackpackVisuals : public Component {
public:
    BackpackVisuals(GameObject& associated, Inventory& inventory, Character* bigChar);

    void Start() override;
    void Update(float dt) override;
    void Render() override;

private:
    void DrawQuickSlot(SDL_Renderer* renderer, int slotIndex, float x, float y,
                       bool isSelected, const char* label) const;

    Inventory& inventory;
    Character* bigCharacter;

    static constexpr float kHudMargin = 18.0f;
    static constexpr float kSlotSize = 56.0f;
    static constexpr float kSlotGap = 14.0f;
    static constexpr float kGaugeHeight = 4.0f;
    static constexpr float kIconPadding = 6.0f;
};

#endif
