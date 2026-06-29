#ifndef ENDSTATE_H
#define ENDSTATE_H

#include "core/State.h"
#include "audio/Music.h"

class GameObject;

class EndState : public State {
public:
    EndState();
    ~EndState();

    void LoadAssets() override;
    void Update(float dt) override;
    void Render() override;

    void Start() override;
    void Pause() override;
    void Resume() override;

private:
    Music backgroundMusic;
    GameObject* gameOverTitle = nullptr;
    GameObject* continuePrompt = nullptr;
};

#endif