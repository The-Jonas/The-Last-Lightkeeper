#ifndef REPAIRABLE_H
#define REPAIRABLE_H

#include "engine/Component.h"
#include "engine/GameObject.h"
#include "math/Vec2.h"
#include <string>

class Repairable : public Component {
public:
    // O construtor recebe o caminho do arquivo que representa o objeto consertado.
    // O GATILHO de interação é uma forma "repairable_trigger" do mapa (Tiled) —
    // basta o irmãozão encostar nela (ver LevelManager::CheckRepairableTrigger).
    Repairable(GameObject& associated, std::string fixedSpritePath, std::string requiredItem, std::string soundPath);
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
    bool isRepaired;

    float repairTimer    = 0.0f;
    bool  isRepairing    = false;
    static constexpr float kRepairFadeIn  = 0.4f; // tempo para escurecer
    static constexpr float kRepairHold    = 2.4f; // tempo parado no preto (som toca aqui)
    static constexpr float kRepairFadeOut = 0.4f; // tempo para clarear
    static constexpr float kRepairTotal   = kRepairFadeIn + kRepairHold + kRepairFadeOut;
};

#endif