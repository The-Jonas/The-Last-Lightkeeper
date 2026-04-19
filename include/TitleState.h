#ifndef TITLESTATE_H
#define TITLESTATE_H

#include "State.h"
#include "Timer.h"

class TitleState : public State {
public:
    TitleState();
    ~TitleState();

    void LoadAssets() override;
    void Update(float dt) override;
    void Render();

    void Start() override;
    void Pause() override;
    void Resume() override;
    
private:
    Timer textBlinkTimer;                   // Timer para piscar o texto
};

#endif