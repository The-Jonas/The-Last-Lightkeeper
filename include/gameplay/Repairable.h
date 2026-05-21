#ifndef REPAIRABLE_H
#define REPAIRABLE_H

#include "engine/Component.h"
#include "engine/GameObject.h"
#include "math/Vec2.h"
#include <string>

class Repairable : public Component {
public:
    // O construtor recebe o caminho do arquivo que representa o objeto consertado e a distância que o jogador precisa estar para interagir
    Repairable(GameObject& associated, std::string fixedSpritePath, std::string requiredItem, std::string soundPath, float interactionDistance = 130.0f, Vec2 interactionOffset = Vec2(0, 0));
    ~Repairable();

    void Update(float dt) override;
    void Render() override;

private:
    std::string fixedSpritePath;
    std::string requiredItem;
    std::string soundPath;
    float interactionDistance;
    Vec2 interactionOffset;
    bool isRepaired;
};

#endif