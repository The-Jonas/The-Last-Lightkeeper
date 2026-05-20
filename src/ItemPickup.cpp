#include "../include/ItemPickup.h"
#include "../include/GameObject.h"
#include "../include/SpriteRenderer.h"

ItemPickup::ItemPickup(GameObject& associated, const ItemDef& def, int durability)
    : Component(associated), def(def), durability(durability) {
}

Vec2 ItemPickup::GetCenter() const {
    return associated.box.Center();
}

void ItemPickup::Destroy() {
    associated.RequestDelete();
}

ItemPickup* ItemPickup::Spawn(float worldX, float worldY, const ItemDef& def, int durability,
                        std::vector<ItemPickup*>& outList) {
    GameObject* obj = new GameObject();
    obj->box.x = worldX;
    obj->box.y = worldY;
    // Mesma camada que personagens/props (z=2): profundidade vem do Y-sort em `StageState::Render`.
    obj->z = 2;
    // Com mesmo `bottom_y`, desenhar o item atrás do personagem (empate de ordenação).
    obj->sub_z = -1;    obj->AddComponent(new SpriteRenderer(*obj, def.spritePath));
    obj->box.w = 48.0f;
    obj->box.h = 48.0f;

    ItemPickup* pickup = new ItemPickup(*obj, def, durability);
    obj->AddComponent(pickup);

    outList.push_back(pickup);
    return pickup;
}
