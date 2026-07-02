#include "gameplay/Jornal.h"
#include "engine/GameObject.h"
#include "engine/SpriteRenderer.h"

Jornal::Jornal(GameObject& associated, std::string imagePath, int heightLevel)
    : Component(associated), imagePath(std::move(imagePath)), heightLevel(heightLevel) {}

Vec2 Jornal::GetCenter() const {
    return associated.box.Center();
}

void Jornal::Start() {}

void Jornal::Update(float dt) {
    (void)dt;
}

void Jornal::Render() {}

Jornal* Jornal::Spawn(float worldX, float worldY, const std::string& path, int height,
                       std::vector<Jornal*>& outList) {
    GameObject* obj = new GameObject();
    obj->box.x = worldX;
    obj->box.y = worldY;
    obj->z = 2;
    obj->sub_z = -1;

    // O SpriteRenderer já define box.w/h com o tamanho nativo da imagem. O
    // SpawnFactory ajusta depois para a dimensão do objeto no Tiled (ApplyTiledBox).
    obj->AddComponent(new SpriteRenderer(*obj, path));
    Jornal* jornal = new Jornal(*obj, path, height);
    obj->AddComponent(jornal);

    outList.push_back(jornal);
    return jornal;
}
