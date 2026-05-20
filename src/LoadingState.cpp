#include "../include/LoadingState.h"
#include "../include/Game.h"
#include "../include/GameObject.h"
#include "../include/InputManager.h"
#include "../include/Camera.h"
#include "../include/Text.h"
#include "../include/StageState.h"

#define INCLUDE_SDL_MIXER
#include "../include/SDL_include.h"

namespace {

void LayoutCenteredLabel(GameObject* label) {
    if (!label) {
        return;
    }
    Game& game = Game::GetInstance();
    const float windowW = static_cast<float>(game.GetWindowsWidth());
    const float windowH = static_cast<float>(game.GetWindowsHeight());
    label->box.x = Camera::pos.x + (windowW - label->box.w) * 0.5f;
    label->box.y = Camera::pos.y + (windowH - label->box.h) * 0.5f;
}
}

LoadingState::LoadingState() = default;

LoadingState::~LoadingState() = default;

void LoadingState::LoadAssets() {
    GameObject* textGO = new GameObject();
    textGO->z = 10;
    SDL_Color white = {220, 220, 220, 255};
    Text* t = new Text(*textGO, "Recursos/font/TradeWinds-Regular.ttf", 40, Text::BLENDED, "Loading...", white);
    textGO->AddComponent(t);
    AddObject(textGO);
    loadingLabel = textGO;
    LayoutCenteredLabel(loadingLabel);
}

void LoadingState::Start() {
    Mix_HaltMusic();
    LoadAssets();
    StartArray();
    started = true;
}

void LoadingState::Update(float /*dt*/) {
    InputManager& input = InputManager::GetInstance();

    if (input.QuitRequested() || input.KeyPress(ESCAPE_KEY)) {
        quitRequested = true;
        return;
    }

    if (transitionDone) {
        return;
    }

    if (waitingFirstPaint) {
        waitingFirstPaint = false;
        LayoutLoadingText();
        return;
    }

    const int masterVol = (MIX_MAX_VOLUME * Game::masterVolumePercent) / 100;
    Mix_Volume(-1, 0);

    StageState* stage = new StageState();
    stage->LoadAssets();

    Mix_Volume(-1, masterVol);

    Game::GetInstance().Push(stage);
    popRequested = true;
    transitionDone = true;
}

void LoadingState::LayoutLoadingText() {
    LayoutCenteredLabel(loadingLabel);
}

void LoadingState::Render() {
    SDL_Renderer* renderer = Game::GetInstance().GetRenderer();
    if (!renderer) {
        return;
    }
    const int winW = Game::GetInstance().GetWindowsWidth();
    const int winH = Game::GetInstance().GetWindowsHeight();
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 10, 10, 14, 255);
    const SDL_Rect full{0, 0, winW, winH};
    SDL_RenderFillRect(renderer, &full);

    LayoutLoadingText();
    RenderArray();
}

void LoadingState::Pause() {}

void LoadingState::Resume() {}
