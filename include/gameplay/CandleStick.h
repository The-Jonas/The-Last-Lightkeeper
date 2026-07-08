#ifndef CANDLESTICK_H
#define CANDLESTICK_H

#include "engine/Component.h"
#include <string>

class StageState;

class Candlestick : public Component {
public:
    // Construtor recebe o estado inicial e a direção da parede ("frente", "esquerda", "direita")
    Candlestick(GameObject& associated, bool startsLit, const std::string& direction);
    ~Candlestick();

    void Start() override;
    void Update(float dt) override;
    void Render() override;

    // Função pública caso o vento (ou um monstro) apague o castiçal no futuro
    GameObject& GetAssociated() { return associated; }
    const GameObject& GetAssociated() const { return associated; }

    void EnsureLightRegistered(StageState& stage);
    void SetLit(bool lit);
    bool IsLit() const { return isLit; }
    const std::string& GetWallDirection() const { return direction; }

    // "Trancamento": quando TODAS as janelas do andar estão abertas, o monstro
    // domina o andar — as velas são apagadas na hora e, se o jogador reacender
    // uma, ela volta a apagar depois de kLockdownRelightSeconds.
    void SetLockdown(bool on);

private:
    void UpdatePromptText();
    bool isLit;
    std::string direction;
    std::string basePath;

    bool  lockdown = false;
    float relightSuppressTimer = 0.0f;
    static constexpr float kLockdownRelightSeconds = 2.0f;
    
    // ID da luz no StageState para podermos ligar/desligar
    int myLightId; 

    bool showPrompt = false;
    GameObject* textObj = nullptr;
};

#endif