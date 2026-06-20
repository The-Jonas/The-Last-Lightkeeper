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
#include <cstdio>

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
    InitSliders();
}

TitleState::~TitleState() {
}

void TitleState::InitSliders() {
    sliders[static_cast<int>(VolumeSliderKind::Master)] = {
        VolumeSliderKind::Master, "Master", Game::masterVolumePercent};
    sliders[static_cast<int>(VolumeSliderKind::Ambient)] = {
        VolumeSliderKind::Ambient, "Ambiente", Game::ambientVolumePercent};
    sliders[static_cast<int>(VolumeSliderKind::Thunder)] = {
        VolumeSliderKind::Thunder, "Trovao", Game::thunderVolumePercent};
}

void TitleState::ApplySliderValue(VolumeSliderKind kind, int percent) {
    switch (kind) {
    case VolumeSliderKind::Master:
        Game::SetMasterVolume(percent);
        break;
    case VolumeSliderKind::Ambient:
        Game::SetAmbientVolume(percent);
        break;
    case VolumeSliderKind::Thunder:
        Game::SetThunderVolume(percent);
        break;
    case VolumeSliderKind::Count:
        break;
    }
}

TitleState::VolumeSliderUi* TitleState::FindSliderAtPoint(int mx, int my) {
    SDL_Point pt{mx, my};
    for (VolumeSliderUi& slider : sliders) {
        if (SDL_PointInRect(&pt, &slider.bar) || SDL_PointInRect(&pt, &slider.handle)) {
            return &slider;
        }
    }
    return nullptr;
}

void TitleState::RecalcSliders() {
    const int winH = Game::GetInstance().GetWindowsHeight();
    const int baseY = winH - 36;

    for (int i = 0; i < static_cast<int>(VolumeSliderKind::Count); ++i) {
        VolumeSliderUi& slider = sliders[i];
        slider.bar.x = 40;
        slider.bar.y = baseY - i * kSliderRowH;
        slider.bar.w = kSliderW;
        slider.bar.h = kSliderH;

        const int frac = (slider.value * kSliderW) / 100;
        slider.handle.x = slider.bar.x + frac - kHandleW / 2;
        slider.handle.y = slider.bar.y - 4;
        slider.handle.w = kHandleW;
        slider.handle.h = kSliderH + 8;
    }
}

void TitleState::HandleSliderInput(int mx, int my, InputManager& input) {
    if (input.MousePress(SDL_BUTTON_LEFT)) {
        if (VolumeSliderUi* hit = FindSliderAtPoint(mx, my)) {
            hit->dragging = true;
        }
    }

    for (VolumeSliderUi& slider : sliders) {
        if (!slider.dragging) {
            continue;
        }
        if (input.IsMouseDown(SDL_BUTTON_LEFT)) {
            float frac = static_cast<float>(mx - slider.bar.x) / static_cast<float>(kSliderW);
            frac = std::max(0.0f, std::min(1.0f, frac));
            slider.value = static_cast<int>(frac * 100.0f);
            ApplySliderValue(slider.kind, slider.value);
        } else {
            slider.dragging = false;
        }
    }
}

void TitleState::RenderSliders(SDL_Renderer* renderer) {
    if (!renderer) {
        return;
    }

    auto font = Resources::GetFont("Recursos/font/TradeWinds-Regular.ttf", 14);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (const VolumeSliderUi& slider : sliders) {
        SDL_SetRenderDrawColor(renderer, 60, 60, 60, 200);
        SDL_RenderFillRect(renderer, &slider.bar);

        SDL_SetRenderDrawColor(renderer, 120, 120, 120, 255);
        SDL_RenderDrawRect(renderer, &slider.bar);

        SDL_SetRenderDrawColor(renderer, 200, 180, 100, 220);
        SDL_RenderFillRect(renderer, &slider.handle);

        if (!font) {
            continue;
        }

        char labelBuf[64];
        std::snprintf(labelBuf, sizeof(labelBuf), "%s: %d", slider.label, slider.value);
        SDL_Color labelColor{180, 180, 180, 220};
        SDL_Surface* labelSurface = TTF_RenderText_Blended(font.get(), labelBuf, labelColor);
        if (!labelSurface) {
            continue;
        }
        SDL_Texture* labelTexture = SDL_CreateTextureFromSurface(renderer, labelSurface);
        SDL_Rect labelDst{slider.bar.x, slider.bar.y - 22, labelSurface->w, labelSurface->h};
        SDL_FreeSurface(labelSurface);
        if (labelTexture) {
            SDL_RenderCopy(renderer, labelTexture, nullptr, &labelDst);
            SDL_DestroyTexture(labelTexture);
        }
    }
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

    const int mx = input.GetMouseX();
    const int my = input.GetMouseY();
    RecalcSliders();
    HandleSliderInput(mx, my, input);

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
    RenderSliders(Game::GetInstance().GetRenderer());
}

void TitleState::Start() {
    LoadAssets();
    StartArray();
    music.Open("Recursos/audio/soundtracks/Ambientacao.mp3");
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
    InitSliders();
}
