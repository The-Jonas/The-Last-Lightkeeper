#include "states/LoadingState.h"
#include "core/Game.h"
#include "core/SaveManager.h"
#include "core/SaveData.h"
#include "audio/GameSfx.h"
#include "audio/GameVoice.h"
#include "engine/GameObject.h"
#include "core/InputManager.h"
#include "engine/Camera.h"
#include "ui/Text.h"
#include "states/stage/StageState.h"
#include "states/CutsceneState.h"

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
}

LoadingState::LoadingState(StageState::LoadMode mode) : loadMode(mode) {}

LoadingState::~LoadingState() = default;

void LoadingState::LoadAssets() {
    GameObject* textGO = new GameObject();
    textGO->z = 10;
    SDL_Color white = {220, 220, 220, 255};
    Text* t = new Text(*textGO, "Recursos/font/times.ttf", 40, Text::BLENDED, "Carregando...", white);
    textGO->AddComponent(t);
    AddObject(textGO);
    loadingLabel = textGO;
    LayoutCenteredLabel(loadingLabel);
}

void LoadingState::Start() {
    GameSfx::NotifyLoadingBegin();
    GameVoice::NotifyLoadingBegin();
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

    StageState* stage = new StageState(loadMode);

    if (loadMode == StageState::LoadMode::Continue) {
        SaveFile saveFile;
        if (SaveManager::Load(saveFile)) {
            stage->SetInitialLevelIndex(saveFile.levelIndex);
        }
    }

    stage->LoadAssets();

    if (loadMode == StageState::LoadMode::Continue) {
        SaveFile saveFile;
        if (SaveManager::Load(saveFile)) {
            stage->ApplySaveState(saveFile.current);
        }
    }

    Mix_Volume(-1, masterVol);

    if (loadMode == StageState::LoadMode::NewGame) {
        // Jogo novo: com o stage já totalmente carregado, toca a cutscene de
        // abertura em cima. Quando ela termina (ou o jogador a pula), o stage
        // pré-carregado é empilhado e o jogo começa instantaneamente.
        const std::string videoPath = "Recursos/video/CUTSCENE_THE_LAST_LIGHTKEEPER.mpg";
        const std::string audioPath = "Recursos/video/CUTSCENE_THE_LAST_LIGHTKEEPER.mp3";
        Game::GetInstance().Push(new CutsceneState(videoPath, audioPath, stage));
    } else {
        Game::GetInstance().Push(stage);
    }
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
