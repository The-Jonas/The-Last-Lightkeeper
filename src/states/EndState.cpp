#include "states/EndState.h"
#include "core/GameData.h"
#include "core/Game.h"
#include "core/InputManager.h"
#include "engine/GameObject.h"
#include "ui/Text.h"
#include "states/TitleState.h"

#define INCLUDE_SDL_MIXER
#include "SDL_include.h"

namespace {

void LayoutCenteredText(GameObject* label, float yOffset = 0.0f) {
    if (!label) {
        return;
    }
    Game& game = Game::GetInstance();
    const float windowW = static_cast<float>(game.GetWindowsWidth());
    const float windowH = static_cast<float>(game.GetWindowsHeight());
    label->box.x = (windowW - label->box.w) * 0.5f;
    label->box.y = (windowH - label->box.h) * 0.5f + yOffset;
}

} // namespace

EndState::EndState() = default;

EndState::~EndState() = default;

void EndState::LoadAssets() {
    Mix_HaltMusic();

    std::string musicFile;
    if (GameData::playerVictory) {
        musicFile = "Recursos/audio/soundtracks/dark_harmonics_Dark_Rumble_Atmos_02_191.mp3";
    } else {
        musicFile = "Recursos/audio/soundtracks/Akane's Regret.mp3";
    }

    backgroundMusic.Open(musicFile);
    if (backgroundMusic.IsOpen()) {
        Mix_VolumeMusic(Game::MusicVolume());
        backgroundMusic.Play(-1);
    }

    if (GameData::playerVictory) {
        // Vitória "por enquanto": alcançar a escada do 2º andar. Tela ambígua.
        GameObject* titleGO = new GameObject();
        titleGO->z = 10;
        SDL_Color titleColor = {210, 205, 190, 255};
        Text* titleText = new Text(*titleGO, "Recursos/font/times.ttf", 52, Text::BLENDED,
                                   "Continua...", titleColor);
        titleGO->AddComponent(titleText);
        AddObject(titleGO);
        gameOverTitle = titleGO;

        GameObject* subGO = new GameObject();
        subGO->z = 10;
        SDL_Color subColor = {170, 175, 185, 230};
        Text* sub = new Text(*subGO, "Recursos/font/times.ttf", 24, Text::BLENDED,
                             "Vocês subiram a escada, mas o topo do farol ainda está longe.",
                             subColor);
        subGO->AddComponent(sub);
        AddObject(subGO);
        causeSubtitle = subGO;
    } else {
        GameObject* titleGO = new GameObject();
        titleGO->z = 10;
        SDL_Color titleColor = {220, 220, 220, 255};
        Text* titleText = new Text(*titleGO, "Recursos/font/times.ttf", 56, Text::BLENDED,
                                   "Fim de Jogo", titleColor);
        titleGO->AddComponent(titleText);
        AddObject(titleGO);
        gameOverTitle = titleGO;

        // Subtítulo com a causa da derrota (6.5)
        GameObject* causeGO = new GameObject();
        causeGO->z = 10;
        SDL_Color causeColor = {190, 120, 120, 230};
        const char* causeText = GameData::deathByMonster
            ? "O monstro alcancou vocês na escuridão."
            : "A escuridão consumiu a sanidade de vocês.";
        Text* cause = new Text(*causeGO, "Recursos/font/times.ttf", 24, Text::BLENDED,
                               causeText, causeColor);
        causeGO->AddComponent(cause);
        AddObject(causeGO);
        causeSubtitle = causeGO;
    }

    GameObject* textGO = new GameObject();
    textGO->z = 10;
    SDL_Color color = {180, 180, 180, 255};
    Text* text = new Text(*textGO, "Recursos/font/times.ttf", 28, Text::BLENDED,
                          "Pressione Espaço para continuar", color);
    textGO->AddComponent(text);
    AddObject(textGO);
    continuePrompt = textGO;

    LayoutCenteredText(gameOverTitle, -80.0f);
    LayoutCenteredText(causeSubtitle, -20.0f);
    LayoutCenteredText(continuePrompt, 60.0f);
}

void EndState::Update(float dt) {
    InputManager& input = InputManager::GetInstance();

    if (input.QuitRequested() || input.KeyPress(ESCAPE_KEY)) {
        quitRequested = true;
    }

    if (input.KeyPress(SPACE_KEY)) {
        popRequested = true;
    }

    LayoutCenteredText(gameOverTitle, -80.0f);
    LayoutCenteredText(continuePrompt, 60.0f);

    UpdateArray(dt);
}

void EndState::Render() {
    SDL_Renderer* renderer = Game::GetInstance().GetRenderer();
    if (renderer) {
        const int winW = Game::GetInstance().GetWindowsWidth();
        const int winH = Game::GetInstance().GetWindowsHeight();
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        const SDL_Rect fullScreen{0, 0, winW, winH};
        SDL_RenderFillRect(renderer, &fullScreen);
    }

    RenderArray();
}

void EndState::Start() {
    LoadAssets();
    StartArray();
    started = true;
}

void EndState::Pause() {}

void EndState::Resume() {}