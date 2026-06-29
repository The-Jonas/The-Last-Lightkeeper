#ifndef LOADINGSTATE_H
#define LOADINGSTATE_H

#include "core/State.h"
#include "states/stage/StageState.h"

class GameObject;

/// Tela escura com "Loading..." entre o menu e o primeiro nível; carrega `StageState` antes do push.
class LoadingState : public State {
public:
    explicit LoadingState(StageState::LoadMode mode = StageState::LoadMode::NewGame);
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
    StageState::LoadMode loadMode = StageState::LoadMode::NewGame;
};

#endif
