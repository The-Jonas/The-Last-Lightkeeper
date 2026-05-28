#ifndef ITEM_PICKUP_H
#define ITEM_PICKUP_H

#include "engine/Component.h"
#include "gameplay/Item.h"
#include "math/Vec2.h"

#include <vector>

class ItemPickup : public Component {
public:
    ItemPickup(GameObject& associated, const ItemDef& def, int durability);

    const ItemDef* GetDef() const { return &def; }
    int GetDurability() const { return durability; }
    Vec2 GetCenter() const;
    GameObject& GetAssociated() { return associated; }
    void Destroy();

    static ItemPickup* Spawn(float worldX, float worldY, const ItemDef& def, int durability,
                      std::vector<ItemPickup*>& outList);

    void SetHeightLevel(int heightlevel);
    int GetHeightLevel() const {return HeightLevel;}

private:
    ItemDef def;
    int durability;
    int HeightLevel;
};

#endif
