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

    bool IsRepaired() const { return isRepaired; }
    void ApplyRepairedState();

    float GetRepairOverlayAlpha() const;

private:
    std::string fixedSpritePath;
    std::string requiredItem;
    std::string soundPath;
    float interactionDistance;
    Vec2 interactionOffset;
    bool isRepaired;

    float repairTimer    = 0.0f;
    bool  isRepairing    = false;
    static constexpr float kRepairFadeIn  = 0.4f; // tempo para escurecer
    static constexpr float kRepairHold    = 2.4f; // tempo parado no preto (som toca aqui)
    static constexpr float kRepairFadeOut = 0.4f; // tempo para clarear
    static constexpr float kRepairTotal   = kRepairFadeIn + kRepairHold + kRepairFadeOut;
};

#endif