#ifndef LEVEL_TRANSITION_LOADING_STATE_H
#define LEVEL_TRANSITION_LOADING_STATE_H

#include "core/State.h"

class GameObject;
class StageState;

/// Brief loading overlay between levels; always visible for at least kMinDuration seconds.
class LevelTransitionLoadingState : public State {
public:
    LevelTransitionLoadingState(StageState* stage, int targetLevelIndex);
    ~LevelTransitionLoadingState();

    void LoadAssets() override;
    void Update(float dt) override;
    void Render() override;

    void Start() override;
    void Pause() override;
    void Resume() override;

private:
    void LayoutLoadingText();

    StageState* stage = nullptr;
    int targetLevelIndex = 0;
    GameObject* loadingLabel = nullptr;
    bool waitingFirstPaint = true;
    bool loadDone = false;
    float elapsed = 0.0f;
    static constexpr float kMinDuration = 1.2f;
};

#endif
