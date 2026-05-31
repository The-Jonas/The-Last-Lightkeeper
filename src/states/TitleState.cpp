#include "states/TitleState.h"
#include "states/LoadingState.h"
#include "core/SaveManager.h"
#include "states/stage/StageState.h"
#include "core/Game.h"
#include "engine/GameObject.h"
#include "engine/SpriteRenderer.h"
#include "core/InputManager.h"
#include "engine/Camera.h"
#include "ui/Text.h"
#include "core/Resources.h"
#include <algorithm>

namespace {

void LayoutTitleScreen(GameObject* background, GameObject* title) {
    if (!background) {
        return;
    }
    Game& game = Game::GetInstance();
    const float windowW = static_cast<float>(game.GetWindowsWidth());
    const float windowH = static_cast<float>(game.GetWindowsHeight());

    background->box.x = Camera::pos.x;
    background->box.y = Camera::pos.y;
    background->box.w = windowW;
    background->box.h = windowH;

    if (title) {
        const float tw = title->box.w;
        title->box.x = Camera::pos.x + (windowW - tw) * 0.5f;
        title->box.y = Camera::pos.y + 120.0f;
    }
}

void LayoutMenuOption(GameObject* option, float centerY) {
    if (!option) {
        return;
    }
    const float windowW = static_cast<float>(Game::GetInstance().GetWindowsWidth());
    option->box.x = Camera::pos.x + (windowW - option->box.w) * 0.5f;
    option->box.y = Camera::pos.y + centerY;
}

} // namespace
#include "states/CutsceneState.h"

TitleState::TitleState() : State() {
    sliderVolume = Game::masterVolumePercent;
}

TitleState::~TitleState() {
}

void TitleState::LoadAssets() {
    GameObject* titleGo = new GameObject();
    SpriteRenderer* titleSprite = new SpriteRenderer(*titleGo, "Recursos/img/Title.png");
    titleGo->AddComponent(titleSprite);
    titleSprite->SetCameraFollower(true);
    titleGo->box.x = Camera::pos.x;
    titleGo->box.y = Camera::pos.y;
    AddObject(titleGo);
    titleBackground = titleGo;

    //GameObject* nameGO = new GameObject();
    //nameGO->z = 10;
    //SDL_Color titleColor = {255, 255, 255, 0};
    //Text* nameText = new Text(*nameGO, "Recursos/font/alcotton.ttf", 48, Text::BLENDED, "The Last LightKeeper", titleColor);
    //nameGO->AddComponent(nameText);
    //AddObject(nameGO);
    //titleText = nameGO;

    GameObject* continueGO = new GameObject();
    continueGO->z = 10;
    SDL_Color menuColor = {0, 0, 0, 0};
    Text* continueText = new Text(*continueGO, "Recursos/font/times.ttf", 40, Text::BLENDED,
                                  "1 Continue", menuColor);
    continueGO->AddComponent(continueText);
    AddObject(continueGO);
    continueMenuText = continueGO;

    GameObject* newGameGO = new GameObject();
    newGameGO->z = 10;
    Text* newGameText = new Text(*newGameGO, "Recursos/font/times.ttf", 40, Text::BLENDED,
                                 "2 New Game", menuColor);
    newGameGO->AddComponent(newGameText);
    AddObject(newGameGO);
    newGameMenuText = newGameGO;

    hasContinueSave = SaveManager::HasSave();
    menuSelection = hasContinueSave ? 0 : 1;

    LayoutTitleScreen(titleBackground, titleText);
    LayoutMenuOptions();
}

void TitleState::LayoutMenuOptions() {
    hasContinueSave = SaveManager::HasSave();
    if (!hasContinueSave && menuSelection == 0) {
        menuSelection = 1;
    }

    constexpr float kNewGameY = 500.0f;
    constexpr float kContinueY = 440.0f;

    LayoutMenuOption(continueMenuText, kContinueY);
    LayoutMenuOption(newGameMenuText, kNewGameY);
}

void TitleState::StartNewGame() {
    SaveManager::DeleteSave();
    Game::GetInstance().Push(new LoadingState(StageState::LoadMode::NewGame));
}

void TitleState::StartContinue() {
    if (!SaveManager::HasSave()) {
        return;
    }
    Game::GetInstance().Push(new LoadingState(StageState::LoadMode::Continue));
}

void TitleState::ActivateMenuSelection() {
    if (menuSelection == 0 && hasContinueSave) {
        StartContinue();
    } else {
        StartNewGame();
    }
}

void TitleState::Update(float dt) {
    InputManager& input = InputManager::GetInstance();

    hasContinueSave = SaveManager::HasSave();
    if (!hasContinueSave && menuSelection == 0) {
        menuSelection = 1;
    }

    if (input.QuitRequested() || input.KeyPress(ESCAPE_KEY)) {
        quitRequested = true;
    }

    if (input.KeyPress(SDLK_UP) || input.KeyPress(SDLK_w)) {
        if (hasContinueSave) {
            menuSelection = 0;
        }
    }
    if (input.KeyPress(SDLK_DOWN) || input.KeyPress(SDLK_s)) {
        menuSelection = 1;
    }

    if (input.KeyPress(SPACE_KEY) || input.KeyPress(SDLK_RETURN)) {
        ActivateMenuSelection();
    } else if (hasContinueSave && (input.KeyPress(SDLK_1) || input.KeyPress(SDLK_c))) {
        StartContinue();
    } else if (input.KeyPress(SDLK_2) || input.KeyPress(SDLK_n)) {
        StartNewGame();
    }

    fadeTimer.Update(dt);
    float t = std::min(1.0f, fadeTimer.Get() / kFadeDuration);
    fadeAlpha = t * 255.0f;
    Uint8 a = static_cast<Uint8>(fadeAlpha);

    int mx = input.GetMouseX();
    int my = input.GetMouseY();
    RecalcSlider();

    if (input.MousePress(SDL_BUTTON_LEFT)) {
        SDL_Point pt{mx, my};
        if (SDL_PointInRect(&pt, &sliderBar) || SDL_PointInRect(&pt, &sliderHandle)) {
            draggingSlider = true;
        }
    }
    if (draggingSlider) {
        if (input.IsMouseDown(SDL_BUTTON_LEFT)) {
            float frac = static_cast<float>(mx - sliderBar.x) / static_cast<float>(kSliderW);
            if (frac < 0.0f) frac = 0.0f;
            if (frac > 1.0f) frac = 1.0f;
            sliderVolume = static_cast<int>(frac * 100.0f);
            Game::SetMasterVolume(sliderVolume);
        } else {
            draggingSlider = false;
        }
    }

    if (titleText) {
        Text* text = titleText->GetComponent<Text>();
        if (text) {
            text->SetColor({255, 255, 255, a});
        }
    }

    auto applyMenuColor = [&](GameObject* option, bool selected) {
        if (!option) {
            return;
        }
        Text* text = option->GetComponent<Text>();
        if (!text) {
            return;
        }
        if (t < 1.0f) {
            text->SetColor({0, 0, 0, a});
            return;
        }
        if (selected) {
            pulseTimer += dt;
            float s = (std::sin(pulseTimer * 1.5f) + 1.0f) * 0.5f;
            Uint8 pulseAlpha = static_cast<Uint8>(40.0f + s * 215.0f);
            text->SetColor({0, 0, 0, pulseAlpha});
        } else {
            text->SetColor({0, 0, 0, 120});
        }
    };

    applyMenuColor(newGameMenuText, menuSelection == 1);
    if (hasContinueSave) {
        applyMenuColor(continueMenuText, menuSelection == 0);
    } else if (continueMenuText) {
        Text* text = continueMenuText->GetComponent<Text>();
        if (text) {
            const Uint8 alpha = t < 1.0f ? a : static_cast<Uint8>(90);
            text->SetColor({100, 100, 100, alpha});
        }
    }

    LayoutTitleScreen(titleBackground, titleText);
    LayoutMenuOptions();
    UpdateArray(dt);
}

void TitleState::Render() {
    RenderArray();
    RenderSlider(Game::GetInstance().GetRenderer());
}

void TitleState::Start() {
    LoadAssets();
    StartArray();
    music.Open("Recursos/audio/soundtracks/Virtutes Instrumenti.mp3");
    music.Play();
    started = true;
}

void TitleState::Pause() {
}

void TitleState::Resume() {
    Camera::pos = Vec2(0, 0);
    hasContinueSave = SaveManager::HasSave();
    if (!hasContinueSave) {
        menuSelection = 1;
    }
}

void TitleState::RecalcSlider() {
    int winH = Game::GetInstance().GetWindowsHeight();
    sliderBar.x = 40;
    sliderBar.y = winH - 70;
    sliderBar.w = kSliderW;
    sliderBar.h = kSliderH;
    int frac = (sliderVolume * kSliderW) / 100;
    sliderHandle.x = sliderBar.x + frac - kHandleW / 2;
    sliderHandle.y = sliderBar.y - 4;
    sliderHandle.w = kHandleW;
    sliderHandle.h = kSliderH + 8;
}

void TitleState::RenderSlider(SDL_Renderer* renderer) {
    if (!renderer) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(renderer, 60, 60, 60, 200);
    SDL_RenderFillRect(renderer, &sliderBar);

    SDL_SetRenderDrawColor(renderer, 120, 120, 120, 255);
    SDL_RenderDrawRect(renderer, &sliderBar);

    SDL_SetRenderDrawColor(renderer, 200, 180, 100, 220);
    SDL_RenderFillRect(renderer, &sliderHandle);

    const char* label = "Volume";
    auto font = Resources::GetFont("Recursos/font/TradeWinds-Regular.ttf", 14);
    if (font) {
        SDL_Color c{180, 180, 180, 200};
        SDL_Surface* s = TTF_RenderText_Blended(font.get(), label, c);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            SDL_Rect d = {sliderBar.x, sliderBar.y - 22, s->w, s->h};
            SDL_FreeSurface(s);
            if (t) {
                SDL_RenderCopy(renderer, t, nullptr, &d);
                SDL_DestroyTexture(t);
            }
        }
    }
}
