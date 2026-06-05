#ifndef HOTBAR_COMPONENT_H
#define HOTBAR_COMPONENT_H

#include "engine/Component.h"
#include "gameplay/Inventory.h"
#include "math/Vec2.h"

#include <functional>
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

    static constexpr float kPickupPromptFootRadiusExtra = 18.0f;

    float GetPickupReachRadius() const;
    ItemPickup* FindClosestReachablePickup() const;
    void TryPickupOnKeyPress();
    void TrySelectGroupOnKeyPress();
    void TryUseActiveItemOnKeyPress();
};

#endif
