#ifndef LOADINGSTATE_H
#define LOADINGSTATE_H

#include "State.h"

class GameObject;

/// Tela escura com "Loading..." entre o menu e o primeiro nível; carrega `StageState` antes do push.
class LoadingState : public State {
public:
    LoadingState();
    ~LoadingState();

    void LoadAssets() override;
    void Update(float dt) override;
    void Render() override;

    void Start() override;
    void Pause() override;
    void Resume() override;

private:
    void LayoutLoadingText();

    GameObject* loadingLabel = nullptr;
    /// Garante um frame de UI antes do carregamento pesado bloquear o Update.
    bool waitingFirstPaint = true;
    bool transitionDone = false;
};

#endif
