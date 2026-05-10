#ifndef TITLESTATE_H
#define TITLESTATE_H

#include "State.h"
#include "Timer.h"
#include "Music.h"
#include "Game.h"

class GameObject;

class TitleState : public State {
public:
    TitleState();
    ~TitleState();

    void LoadAssets() override;
    void Update(float dt) override;
    void Render() override;

    void Start() override;
    void Pause() override;
    void Resume() override;
    
private:
    Music music;
    Timer fadeTimer;
    float fadeAlpha = 0.0f;
    static constexpr float kFadeDuration = 3.0f;

    GameObject* titleBackground = nullptr;
    GameObject* titleText = nullptr;
    GameObject* pressSpaceText = nullptr;

    bool draggingSlider = false;
    int sliderVolume = Game::MASTER_VOLUME_PERCENT;
    SDL_Rect sliderBar{0, 0, 0, 0};
    SDL_Rect sliderHandle{0, 0, 0, 0};
    static constexpr int kSliderW = 260;
    static constexpr int kSliderH = 12;
    static constexpr int kHandleW = 20;

    void RecalcSlider();
    void RenderSlider(SDL_Renderer* renderer);
};

#endif