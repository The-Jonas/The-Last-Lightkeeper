#ifndef WINDOW_H
#define WINDOW_H

#include "engine/Component.h"
#include <string>

class GameObject;
class Text;

class Window : public Component {
public:
    enum class WindowState { CLOSED, OPENING, OPEN, CLOSING };

    // windRadius define a distância em pixels que o vento alcança
    Window(GameObject& associated, std::string windowType, bool startsOpen, float windRadius = 300.0f);
    ~Window();

    void Start() override;
    void Update(float dt) override;
    void Render() override;

    void Toggle(); // Chamado pelo StageState/Monstro para interagir
    WindowState GetState() const { return state; }
    float GetWindRadius() const { return windRadius; }

    GameObject& GetAssociated() { return associated; }
    const GameObject& GetAssociated() const { return associated; }

private:
    WindowState state;
    std::string basePath;
    std::string windowType; // Guarda qual é a posição/tipo visual dessa janela
    
    float windRadius;
    float windTimer;
    constexpr static float kWindInterval = 5.0f; // Tempo em segundos pro vento apagar as velas

    float animTimer;
    constexpr static float kAnimDuration = 0.5f; // Tempo da animação de abrir/fechar

    GameObject* textObj;
    bool showPrompt = false;

    void UpdateVisuals();
    void UpdatePromptText();
    void BlowOutNearbyCandles();
};

#endif