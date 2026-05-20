#include "../include/TitleState.h"
#include "../include/LoadingState.h"
#include "../include/Game.h"
#include "../include/GameObject.h"
#include "../include/SpriteRenderer.h"
#include "../include/InputManager.h"
#include "../include/Camera.h"
#include "../include/Text.h"
#include "../include/Resources.h"
#include <algorithm>

namespace {

void LayoutTitleScreen(GameObject* background, GameObject* title,
                       GameObject* pressSpace) {
    if (!background || !pressSpace) {
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

    const float textW = pressSpace->box.w;
    const float textH = pressSpace->box.h;
    pressSpace->box.x = Camera::pos.x + (windowW - textW) * 0.5f;
    pressSpace->box.y = Camera::pos.y + windowH - std::max(120.0f, textH + 56.0f);
}
}
#include "../include/CutsceneState.h"

TitleState::TitleState() : State() {
    sliderVolume = Game::masterVolumePercent;
}

TitleState::~TitleState() {
}

void TitleState::LoadAssets() {
    GameObject* titleGo = new GameObject();
    SpriteRenderer* titleSprite = new SpriteRenderer(*titleGo, "Recursos/img/main_title.png");
    titleGo->AddComponent(titleSprite);
    titleSprite->SetCameraFollower(true);
    titleGo->box.x = Camera::pos.x;
    titleGo->box.y = Camera::pos.y;
    AddObject(titleGo);
    titleBackground = titleGo;

    GameObject* nameGO = new GameObject();
    nameGO->z = 10;
    SDL_Color titleColor = {255, 255, 255, 0};
    Text* nameText = new Text(*nameGO, "Recursos/font/alcotton.ttf", 48, Text::BLENDED, "The Last LightKeeper", titleColor);
    nameGO->AddComponent(nameText);
    AddObject(nameGO);
    titleText = nameGO;

    GameObject* textGO = new GameObject();
    textGO->z = 10;
    SDL_Color white = {255, 255, 255, 0};
    Text* pressText = new Text(*textGO, "Recursos/font/TradeWinds-Regular.ttf", 40, Text::BLENDED, "Press Space to continue", white);
    textGO->AddComponent(pressText);
    AddObject(textGO);
    pressSpaceText = textGO;

    LayoutTitleScreen(titleBackground, titleText, pressSpaceText);
}

void TitleState::Update(float dt) {
    InputManager& input = InputManager::GetInstance();

    if (input.QuitRequested() || input.KeyPress(ESCAPE_KEY)) {
        quitRequested = true;
    }

    if (input.KeyPress(SPACE_KEY)) {
        Game::GetInstance().Push(new LoadingState());
        // Game::GetInstance().Push(new CutsceneState(
        // "Recursos/video/video_cutscene.mpg",
        // "Recursos/audio/audio_cutscene.wav",
        // new StageState()
        // ));                 
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

    if (pressSpaceText) {
        Text* text = pressSpaceText->GetComponent<Text>();
        if (text) {
            text->SetColor({255, 255, 255, a});
        }
    }

    LayoutTitleScreen(titleBackground, titleText, pressSpaceText);
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
