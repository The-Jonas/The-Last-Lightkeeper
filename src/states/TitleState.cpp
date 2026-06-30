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

    GameObject* newGameGO = new GameObject();
    newGameGO->z = 10;
    SDL_Color menuColor = {0, 0, 0, 0};
    Text* newGameText = new Text(*newGameGO, "Recursos/font/times.ttf", 40, Text::BLENDED,
                                  "New Game", menuColor);
    newGameGO->AddComponent(newGameText);
    AddObject(newGameGO);
    newGameMenuText = newGameGO;

    GameObject* continueGO = new GameObject();
    continueGO->z = 10;
    Text* continueText = new Text(*continueGO, "Recursos/font/times.ttf", 40, Text::BLENDED,
                                   "Continue", menuColor);
    continueGO->AddComponent(continueText);
    AddObject(continueGO);
    continueMenuText = continueGO;

    hasContinueSave = SaveManager::HasSave();
    menuSelection = 0;

    LayoutTitleScreen(titleBackground, titleText);
    LayoutMenuOptions();
}

void TitleState::LayoutMenuOptions() {
    hasContinueSave = SaveManager::HasSave();
    if (!hasContinueSave && menuSelection == 1) {
        menuSelection = 0;
    }

    constexpr float kNewGameY = 440.0f;
    constexpr float kContinueY = 500.0f;

    LayoutMenuOption(newGameMenuText, kNewGameY);
    LayoutMenuOption(continueMenuText, kContinueY);
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
    if (menuSelection == 1 && hasContinueSave) {
        StartContinue();
    } else {
        StartNewGame();
    }
}

void TitleState::Update(float dt) {
    InputManager& input = InputManager::GetInstance();

    hasContinueSave = SaveManager::HasSave();
    if (!hasContinueSave && menuSelection == 1) {
        menuSelection = 0;
    }

    if (input.QuitRequested() || input.KeyPress(ESCAPE_KEY)) {
        quitRequested = true;
    }

    if (input.KeyPress(SDLK_w) || input.KeyPress(SDLK_UP)) {
        menuSelection = 0;
    }
    if ((input.KeyPress(SDLK_s) || input.KeyPress(SDLK_DOWN)) && hasContinueSave) {
        menuSelection = 1;
    }

    if (input.KeyPress(SDLK_f) || input.KeyPress(SPACE_KEY) || input.KeyPress(SDLK_RETURN)) {
        ActivateMenuSelection();
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
            float s = (std::sin(pulseTimer * 2.0f) + 1.0f) * 0.5f;
            Uint8 r = static_cast<Uint8>(200.0f + s * 55.0f);
            Uint8 g = static_cast<Uint8>(160.0f + s * 55.0f);
            Uint8 b = static_cast<Uint8>(40.0f + s * 40.0f);
            text->SetColor({r, g, b, 255});
        } else {
            text->SetColor({130, 130, 130, 255});
        }
    };

    applyMenuColor(newGameMenuText, menuSelection == 0);
    if (hasContinueSave) {
        applyMenuColor(continueMenuText, menuSelection == 1);
    } else if (continueMenuText) {
        Text* text = continueMenuText->GetComponent<Text>();
        if (text) {
            text->SetColor({100, 100, 100, static_cast<Uint8>(t < 1.0f ? a : 120)});
        }
    }

    LayoutTitleScreen(titleBackground, titleText);
    LayoutMenuOptions();
    UpdateArray(dt);
}

void TitleState::RenderSelectionIndicator(SDL_Renderer* renderer) {
    if (!renderer) {
        return;
    }
    // Only show the marker once the menu has fully faded in (matches the
    // moment the selected option starts pulsing in golden tones).
    if (fadeAlpha < 250.0f) {
        return;
    }

    GameObject* selected = (menuSelection == 1 && hasContinueSave) ? continueMenuText
                                                                   : newGameMenuText;
    if (!selected) {
        return;
    }

    const float screenX = selected->box.x - Camera::pos.x;
    const float screenY = selected->box.y - Camera::pos.y;
    const float cy = screenY + selected->box.h * 0.5f;

    // Pulse the marker in sync with the selected option's golden pulse.
    const float s = (std::sin(pulseTimer * 2.0f) + 1.0f) * 0.5f;
    const Uint8 r = static_cast<Uint8>(200.0f + s * 55.0f);
    const Uint8 g = static_cast<Uint8>(160.0f + s * 55.0f);
    const Uint8 b = static_cast<Uint8>(40.0f + s * 40.0f);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);

    constexpr float kTriH = 22.0f;
    constexpr float kTriW = 16.0f;
    constexpr float kGap = 14.0f;

    // Right-pointing triangle to the left of the option (tip toward the text).
    const float leftTipX = screenX - kGap;
    const float leftBaseX = leftTipX - kTriW;
    for (float x = leftBaseX; x <= leftTipX; x += 1.0f) {
        const float t = (x - leftBaseX) / kTriW;
        const float halfH = (kTriH * 0.5f) * (1.0f - t);
        SDL_RenderDrawLineF(renderer, x, cy - halfH, x, cy + halfH);
    }

    // Left-pointing triangle to the right of the option (tip toward the text).
    const float rightTipX = screenX + selected->box.w + kGap;
    const float rightBaseX = rightTipX + kTriW;
    for (float x = rightTipX; x <= rightBaseX; x += 1.0f) {
        const float t = (x - rightTipX) / kTriW;
        const float halfH = (kTriH * 0.5f) * t;
        SDL_RenderDrawLineF(renderer, x, cy - halfH, x, cy + halfH);
    }
}

void TitleState::Render() {
    RenderArray();
    RenderSelectionIndicator(Game::GetInstance().GetRenderer());
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
        menuSelection = 0;
    }
    InitSliders();
}
