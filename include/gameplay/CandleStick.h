#ifndef CANDLESTICK_H
#define CANDLESTICK_H

#include "engine/Component.h"
#include <string>

class Candlestick : public Component {
public:
    // Construtor recebe o estado inicial e a direção da parede ("frente", "esquerda", "direita")
    Candlestick(GameObject& associated, bool startsLit, const std::string& direction);
    ~Candlestick();

    void Start() override;
    void Update(float dt) override;
    void Render() override;

    // Função pública caso o vento (ou um monstro) apague o castiçal no futuro
    void SetLit(bool lit);

private:
    bool isLit;
    std::string direction;
    std::string basePath;
    
    // ID da luz no StageState para podermos ligar/desligar
    int myLightId; 

    // Sistema de Feedback Visual 
    bool showPrompt;
    GameObject* textObj;
};

#endif