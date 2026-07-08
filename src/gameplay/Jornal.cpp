#include "gameplay/Jornal.h"
#include "engine/GameObject.h"
#include "engine/SpriteRenderer.h"

Jornal::Jornal(GameObject& associated, std::string imagePath, int heightLevel,
               std::string soundPath, float zoomFactor)
    : Component(associated),
      imagePath(std::move(imagePath)),
      soundPath(std::move(soundPath)),
      zoomFactor(zoomFactor),
      heightLevel(heightLevel) {}

Vec2 Jornal::GetCenter() const {
    return associated.box.Center();
}

void Jornal::Start() {}

void Jornal::Update(float dt) {
    (void)dt;
}

void Jornal::Render() {}

Jornal* Jornal::Spawn(float worldX, float worldY,
                      const std::string& spritePath,
                      const std::string& imagePath,
                      int height,
                      std::vector<Jornal*>& outList,
                      const std::string& soundPath,
                      float zoomFactor) {
    GameObject* obj = new GameObject();
    obj->box.x = worldX;
    obj->box.y = worldY;
    obj->z     = 2;
    obj->sub_z = -1;
    obj->AddComponent(new SpriteRenderer(*obj, spritePath));
    Jornal* jornal = new Jornal(*obj, imagePath, height, soundPath, zoomFactor);
    obj->AddComponent(jornal);
    outList.push_back(jornal);
    return jornal;
}
