#ifndef TITLESTATE_H
#define TITLESTATE_H

#include "core/State.h"
#include "core/Timer.h"
#include "audio/Music.h"
#include "core/Game.h"

class GameObject;
class InputManager;

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
    enum class VolumeSliderKind { Master, Ambient, Thunder, Count };

    struct VolumeSliderUi {
        VolumeSliderKind kind;
        const char* label;
        int value = 0;
        bool dragging = false;
        SDL_Rect bar{0, 0, 0, 0};
        SDL_Rect handle{0, 0, 0, 0};
    };

    Music music;
    Timer fadeTimer;
    float fadeAlpha = 0.0f;
    static constexpr float kFadeDuration = 3.0f;
    float pulseTimer = 0.0f;

    GameObject* titleBackground = nullptr;
    GameObject* titleText = nullptr;
    GameObject* continueMenuText = nullptr;
    GameObject* newGameMenuText = nullptr;

    bool hasContinueSave = false;
    int menuSelection = 1;

    void LayoutMenuOptions();
    void StartNewGame();
    void StartContinue();
    void ActivateMenuSelection();

    VolumeSliderUi sliders[static_cast<int>(VolumeSliderKind::Count)];

    static constexpr int kSliderW = 260;
    static constexpr int kSliderH = 12;
    static constexpr int kHandleW = 20;
    static constexpr int kSliderRowH = 44;

    void InitSliders();
    void RecalcSliders();
    void HandleSliderInput(int mx, int my, InputManager& input);
    void ApplySliderValue(VolumeSliderKind kind, int percent);
    void RenderSliders(SDL_Renderer* renderer);
    VolumeSliderUi* FindSliderAtPoint(int mx, int my);
};

#endif
