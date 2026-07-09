#include "states/LevelTransitionLoadingState.h"
#include "core/Game.h"
#include "core/InputManager.h"
#include "audio/GameSfx.h"
#include "audio/GameVoice.h"
#include "engine/Camera.h"
#include "states/stage/StageState.h"
#include "ui/Text.h"

#define INCLUDE_SDL_MIXER
#include "SDL_include.h"

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

} // namespace

LevelTransitionLoadingState::LevelTransitionLoadingState(StageState* stage, int targetLevelIndex)
    : stage(stage), targetLevelIndex(targetLevelIndex) {}

LevelTransitionLoadingState::~LevelTransitionLoadingState() = default;

void LevelTransitionLoadingState::LoadAssets() {
    GameObject* textGO = new GameObject();
    textGO->z = 10;
    SDL_Color white = {220, 220, 220, 255};
    Text* t = new Text(*textGO, "Recursos/font/times.ttf", 40, Text::BLENDED, "Carregando...", white);
    textGO->AddComponent(t);
    AddObject(textGO);
    loadingLabel = textGO;
    LayoutCenteredLabel(loadingLabel);
}

void LevelTransitionLoadingState::Start() {
    GameSfx::NotifyLoadingBegin();
    GameVoice::NotifyLoadingBegin();
    LoadAssets();
    StartArray();
    started = true;
}

void LevelTransitionLoadingState::Update(float dt) {
    InputManager& input = InputManager::GetInstance();

    if (input.QuitRequested() || input.KeyPress(ESCAPE_KEY)) {
        quitRequested = true;
        return;
    }

    if (waitingFirstPaint) {
        waitingFirstPaint = false;
        LayoutLoadingText();
        return;
    }

    if (!loadDone && stage) {
        const int masterVol = (MIX_MAX_VOLUME * Game::masterVolumePercent) / 100;
        Mix_Volume(-1, 0);
        stage->TransitionToLevel(targetLevelIndex);
        Mix_Volume(-1, masterVol);
        loadDone = true;
    }

    elapsed += dt;
    if (loadDone && elapsed >= kMinDuration) {
        popRequested = true;
    }
}

void LevelTransitionLoadingState::LayoutLoadingText() {
    LayoutCenteredLabel(loadingLabel);
}

void LevelTransitionLoadingState::Render() {
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

void LevelTransitionLoadingState::Pause() {}

void LevelTransitionLoadingState::Resume() {}
