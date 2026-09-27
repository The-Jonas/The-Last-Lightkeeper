#include "states/BrightnessCalibrationState.h"
#include "states/TitleState.h"
#include "core/Game.h"
#include "core/InputManager.h"
#include "core/Resources.h"
#include "engine/Camera.h"
#include "nlohmann/json.hpp"

#define INCLUDE_SDL
#define INCLUDE_SDL_TTF
#include "SDL_include.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>

namespace {

const char* kSilhouettePath = "Recursos/img/ui/calibracao_silhueta.png";
const char* kFontPath       = "Recursos/font/times.ttf";

// Desenha um texto centrado em cx com o topo em y.
void DrawCenteredText(SDL_Renderer* r, TTF_Font* font, const std::string& text,
                      float cx, float y, SDL_Color color) {
    if (!font || text.empty()) return;
    SDL_Surface* s = TTF_RenderUTF8_Blended(font, text.c_str(), SDL_Color{color.r, color.g, color.b, 255});
    if (!s) return;
    if (SDL_Texture* t = SDL_CreateTextureFromSurface(r, s)) {
        SDL_SetTextureAlphaMod(t, color.a);
        const SDL_FRect d{cx - s->w * 0.5f, y, static_cast<float>(s->w), static_cast<float>(s->h)};
        SDL_RenderCopyF(r, t, nullptr, &d);
        SDL_DestroyTexture(t);
    }
    SDL_FreeSurface(s);
}

}  // namespace

BrightnessCalibrationState::BrightnessCalibrationState(bool firstLaunch)
    : firstLaunch(firstLaunch) {}
// Tela de calibração; firstLaunch decide se ao sair vai para o título ou volta.

void BrightnessCalibrationState::LoadAssets() {
    Resources::GetImage(kSilhouettePath);   // pré-carrega a silhueta
}
// Pré-carrega a silhueta do isqueiro.

void BrightnessCalibrationState::LoadTuning() {
    std::ifstream f("config/calibration.json");
    if (!f.is_open()) return;   // sem arquivo: valores padrão do header
    try {
        nlohmann::json j;
        f >> j;
        if (j.contains("background") && j["background"].is_number_integer()) {
            backgroundGray = std::max(0, std::min(255, j["background"].get<int>()));
        }
        if (j.contains("symbols") && j["symbols"].is_array() && j["symbols"].size() == 3) {
            for (int i = 0; i < 3; ++i) {
                if (j["symbols"][i].is_number_integer()) {
                    symbolGray[i] = std::max(0, std::min(255, j["symbols"][i].get<int>()));
                }
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "config/calibration.json ignorado (parse): " << ex.what() << std::endl;
    }
}
// Lê os cinzas do fundo e dos três símbolos de config/calibration.json.

void BrightnessCalibrationState::Start() {
    Camera::ResetView();   // estado de interface: desenha a 1:1
    LoadTuning();
    LoadAssets();
    startBrightness = Game::brightnessPercent;
    StartArray();
    started = true;
}
// Prepara a tela: câmera neutra, valores do JSON e brilho inicial para desfazer.

void BrightnessCalibrationState::Pause() {}
void BrightnessCalibrationState::Resume() {}

int BrightnessCalibrationState::ApplyGamma(int gray) {
    const float black = Game::BrightnessBlackPoint();
    const float g = std::max(0.3f, std::min(3.0f, Game::BrightnessGamma()));
    float v = std::max(0, std::min(255, gray)) / 255.0f;
    v = std::max(0.0f, std::min(1.0f, (v - black) / (1.0f - black)));
    v = std::pow(v, 1.0f / g);
    return static_cast<int>(std::lround(v * 255.0f));
}
// Mesma curva do shader da cena: sobe o ponto de preto (brilho < 100) e depois
// aplica a gama (brilho > 100).

void BrightnessCalibrationState::Step(int delta) {
    Game::SetBrightness(Game::brightnessPercent + delta);   // SetBrightness já limita a 50..150
}
// Aumenta/diminui o brilho em `delta` pontos.

void BrightnessCalibrationState::Finish(bool keep) {
    if (!keep) {
        Game::SetBrightness(startBrightness);   // ESC pelas Configurações: desfaz
    } else {
        Game::brightnessCalibrated = true;
        Game::SaveSettings();
    }
    popRequested = true;
    if (firstLaunch) {
        Game::GetInstance().Push(new TitleState());
    }
}
// Sai da tela: grava (keep) ou restaura o brilho; no 1º arranque segue ao título.

void BrightnessCalibrationState::Update(float dt) {
    InputManager& input = InputManager::GetInstance();
    if (input.QuitRequested()) {
        quitRequested = true;
        return;
    }

    // Toque: um passo. Segurando: repete depois de um pequeno atraso.
    const bool leftPress  = input.ActionPress(GameAction::MoveLeft)  || input.KeyPress(SDLK_LEFT);
    const bool rightPress = input.ActionPress(GameAction::MoveRight) || input.KeyPress(SDLK_RIGHT);
    const bool leftDown   = input.ActionDown(GameAction::MoveLeft)   || input.IsKeyDown(SDLK_LEFT);
    const bool rightDown  = input.ActionDown(GameAction::MoveRight)  || input.IsKeyDown(SDLK_RIGHT);

    if (leftPress)  { Step(-kStep); holdTimer = -kHoldDelay; }
    if (rightPress) { Step(+kStep); holdTimer = -kHoldDelay; }
    if (leftDown != rightDown) {
        holdTimer += dt;
        while (holdTimer >= kHoldRepeat) {
            holdTimer -= kHoldRepeat;
            Step(leftDown ? -kStep : +kStep);
        }
    } else {
        holdTimer = 0.0f;
    }

    if (input.KeyPress(SDLK_RETURN) || input.KeyPress(SPACE_KEY) ||
        input.ActionPress(GameAction::Interact)) {
        Finish(true);
        return;
    }
    if (input.KeyPress(ESCAPE_KEY)) {
        // 1º arranque: ESC aceita o valor atual (não fica preso aqui).
        // Pelas Configurações: desfaz o que mexeu.
        Finish(firstLaunch);
    }
}
// A/D ou setas ajustam (segurando repete), Enter/Espaço/E confirmam, ESC sai.

void BrightnessCalibrationState::Render() {
    SDL_Renderer* renderer = Game::GetInstance().GetRenderer();
    if (!renderer) return;
    const float winW = static_cast<float>(Game::GetInstance().GetWindowsWidth());
    const float winH = static_cast<float>(Game::GetInstance().GetWindowsHeight());
    const float u = Game::UiScale();

    // Fundo = o escuro do jogo, já com a gama atual (como a cena ficaria).
    const Uint8 bg = static_cast<Uint8>(backgroundGray);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, bg, bg, bg, 255);
    const SDL_Rect full{0, 0, static_cast<int>(winW), static_cast<int>(winH)};
    SDL_RenderFillRect(renderer, &full);

    // Três silhuetas: a mesma imagem branca multiplicada pelo cinza (com gama).
    if (auto tex = Resources::GetImage(kSilhouettePath)) {
        int tw = 1, th = 1;
        SDL_QueryTexture(tex.get(), nullptr, nullptr, &tw, &th);
        const float h = 220.0f * u;
        const float w = h * static_cast<float>(tw) / static_cast<float>(std::max(1, th));
        const float xs[3] = {0.30f, 0.50f, 0.70f};
        SDL_SetTextureBlendMode(tex.get(), SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(tex.get(), 255);
        for (int i = 0; i < 3; ++i) {
            const Uint8 g = static_cast<Uint8>(ApplyGamma(symbolGray[i]));
            SDL_SetTextureColorMod(tex.get(), g, g, g);
            const SDL_FRect dst{winW * xs[i] - w * 0.5f, winH * 0.42f - h * 0.5f, w, h};
            SDL_RenderCopyF(renderer, tex.get(), nullptr, &dst);
        }
        SDL_SetTextureColorMod(tex.get(), 255, 255, 255);   // textura compartilhada
    }

    auto titleFont = Resources::GetFont(kFontPath, std::max(20, static_cast<int>(std::lround(46.0f * u))));
    auto textFont  = Resources::GetFont(kFontPath, std::max(16, static_cast<int>(std::lround(30.0f * u))));
    auto smallFont = Resources::GetFont(kFontPath, std::max(13, static_cast<int>(std::lround(24.0f * u))));
    const SDL_Color textCol{200, 195, 175, 255};
    const SDL_Color dimCol {140, 135, 120, 255};

    DrawCenteredText(renderer, titleFont.get(), "Ajuste de brilho", winW * 0.5f, winH * 0.10f, textCol);
    DrawCenteredText(renderer, textFont.get(),
                     "Ajuste até que o isqueiro da ESQUERDA fique quase invisível.",
                     winW * 0.5f, winH * 0.64f, textCol);

    // Barra do brilho (50..150).
    const float barW = 420.0f * u, barH = 6.0f * u;
    const float barX = winW * 0.5f - barW * 0.5f, barY = winH * 0.74f;
    const float t = (Game::brightnessPercent - 50) / 100.0f;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 90, 85, 75, 255);
    const SDL_FRect track{barX, barY, barW, barH};
    SDL_RenderFillRectF(renderer, &track);
    SDL_SetRenderDrawColor(renderer, 235, 215, 160, 255);
    const SDL_FRect fill{barX, barY, barW * t, barH};
    SDL_RenderFillRectF(renderer, &fill);
    const SDL_FRect knob{barX + barW * t - 5.0f * u, barY - 7.0f * u, 10.0f * u, barH + 14.0f * u};
    SDL_RenderFillRectF(renderer, &knob);
    DrawCenteredText(renderer, smallFont.get(), "Brilho: " + std::to_string(Game::brightnessPercent),
                     winW * 0.5f, barY + 22.0f * u, dimCol);

    const std::string escText = firstLaunch ? "ESC — pular" : "ESC — cancelar";
    DrawCenteredText(renderer, smallFont.get(),
                     "A / D — ajustar    Enter — confirmar    " + escText,
                     winW * 0.5f, winH * 0.90f, dimCol);
}
// Fundo no escuro do jogo, as três silhuetas (20/32/56 com gama), instrução,
// barra do brilho atual e dicas de teclas.