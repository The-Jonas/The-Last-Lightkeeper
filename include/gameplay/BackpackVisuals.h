#ifndef BACKPACK_VISUALS_H
#define BACKPACK_VISUALS_H

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
    Inventory& inventory;
    Character* bigCharacter;
};

#endif
