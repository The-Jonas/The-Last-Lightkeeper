#include "states/EndState.h"
#include "core/GameData.h"
#include "core/Game.h"
#include "core/InputManager.h"
#include "core/Resources.h"
#include "engine/GameObject.h"
#include "ui/Text.h"
#include "states/TitleState.h"

#define INCLUDE_SDL
#define INCLUDE_SDL_MIXER
#define INCLUDE_SDL_TTF
#include "SDL_include.h"

#include "nlohmann/json.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>

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
        // Vitória / fim da fatia jogável → rola os créditos finais.
        creditsMode = true;
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

    // Na vitória, o rolo de créditos cuida do texto e do próprio prompt.
    if (!creditsMode) {
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
}

void EndState::Update(float dt) {
    InputManager& input = InputManager::GetInstance();

    if (input.QuitRequested() || input.KeyPress(ESCAPE_KEY)) {
        quitRequested = true;
    }

    if (creditsMode) {
        // ESPAÇO: se ainda rolando, pula para o fim; se já terminou, volta ao menu.
        if (input.KeyPress(SPACE_KEY)) {
            if (creditsFinished) {
                popRequested = true;
            } else {
                creditsFinished = true;   // pula o resto do rolo
            }
        }
        if (!creditsFinished) {
            constexpr float kScrollSpeed = 60.0f;   // px/s
            creditsScrollY += kScrollSpeed * dt;
        }
        UpdateArray(dt);
        return;
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

    if (creditsMode && renderer) {
        RenderCredits(renderer);
        return;
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

// ── Créditos finais ───────────────────────────────────────────────────────────
namespace {

using json = nlohmann::json;

// Uma linha do rolo antes de virar textura: texto (text vazio = só espaço) ou
// imagem (imagePath). `gap` é o espaço deixado abaixo do item.
struct CreditSpec {
    bool isImage = false;
    std::string text;
    std::string imagePath;
    int size = 26;        // tamanho da fonte (texto)
    int height = 320;     // altura alvo em px (imagem)
    float gap = 22.0f;
    SDL_Color color{210, 205, 190, 255};
};

// Conteúdo padrão embutido — usado se Recursos/data/credits.json não existir.
// Preencha os nomes reais e as artes pela pasta/JSON (veja o arquivo criado).
std::vector<CreditSpec> EmbeddedCredits() {
    const SDL_Color warm{232, 210, 170, 255};
    const SDL_Color soft{200, 200, 210, 255};
    const SDL_Color head{225, 205, 160, 255};
    std::vector<CreditSpec> c;
    c.push_back({false, "A Luz do Farol", "", 60, 0, 16.0f, warm});
    c.push_back({false, "", "", 26, 0, 26.0f, soft});
    c.push_back({false, "Universidade de Brasília — UnB", "", 32, 0, 6.0f, soft});
    c.push_back({false, "Introdução ao Desenvolvimento de Jogos", "", 26, 0, 48.0f, soft});
    c.push_back({false, "— Equipe —", "", 36, 0, 20.0f, head});
    c.push_back({false, "Programação", "", 28, 0, 4.0f, head});
    c.push_back({false, "João Victor Pereira Vieira", "", 24, 0, 26.0f, soft});
    c.push_back({false, "Arte & Design", "", 28, 0, 4.0f, head});
    c.push_back({false, "(edite Recursos/data/credits.json)", "", 24, 0, 26.0f, soft});
    c.push_back({false, "Áudio", "", 28, 0, 4.0f, head});
    c.push_back({false, "(edite Recursos/data/credits.json)", "", 24, 0, 48.0f, soft});
    c.push_back({false, "— Arte dos nossos Designers —", "", 30, 0, 24.0f, head});
    // Ex.: solte PNGs em Recursos/img/creditos/ e liste-os no JSON.
    c.push_back({true, "", "Recursos/img/creditos/designers.png", 26, 360, 40.0f, soft});
    c.push_back({false, "Obrigado por jogar", "", 46, 0, 20.0f, warm});
    return c;
}

SDL_Color ParseColor(const json& arr, SDL_Color fallback) {
    if (arr.is_array() && arr.size() >= 3) {
        return SDL_Color{
            static_cast<Uint8>(arr[0].get<int>()),
            static_cast<Uint8>(arr[1].get<int>()),
            static_cast<Uint8>(arr[2].get<int>()),
            arr.size() >= 4 ? static_cast<Uint8>(arr[3].get<int>()) : Uint8{255}};
    }
    return fallback;
}

std::vector<CreditSpec> LoadCreditSpecs() {
    std::ifstream f("Recursos/data/credits.json");
    if (!f.is_open()) {
        return EmbeddedCredits();
    }
    try {
        json j;
        f >> j;
        std::vector<CreditSpec> out;
        if (j.contains("entries") && j["entries"].is_array()) {
            for (const auto& e : j["entries"]) {
                CreditSpec s;
                if (e.contains("image")) {
                    s.isImage = true;
                    s.imagePath = e.at("image").get<std::string>();
                    s.height = e.value("height", 320);
                } else {
                    s.text = e.value("text", std::string());
                    s.size = e.value("size", 26);
                }
                s.gap = e.value("gap", 22.0f);
                if (e.contains("color")) {
                    s.color = ParseColor(e["color"], s.color);
                }
                out.push_back(std::move(s));
            }
        }
        if (!out.empty()) {
            return out;
        }
    } catch (const std::exception&) {
        // JSON quebrado → cai no padrão.
    }
    return EmbeddedCredits();
}

} // namespace

void EndState::BuildCredits(SDL_Renderer* renderer) {
    const int winW = Game::GetInstance().GetWindowsWidth();
    const Uint32 wrap = static_cast<Uint32>(winW * 0.75f);

    creditsItems.clear();
    creditsContentHeight = 0.0f;

    for (const CreditSpec& s : LoadCreditSpecs()) {
        CreditsItem item;
        item.gapAfter = s.gap;

        if (s.isImage) {
            auto tex = Resources::GetImage(s.imagePath);
            if (!tex) {
                continue;   // arte ausente → pula silenciosamente
            }
            int tw = 0, th = 0;
            SDL_QueryTexture(tex.get(), nullptr, nullptr, &tw, &th);
            const float aspect = (th > 0) ? static_cast<float>(tw) / static_cast<float>(th) : 1.0f;
            item.h = std::max(1, s.height);
            item.w = static_cast<int>(item.h * aspect);
            item.tex = tex;
        } else if (s.text.empty()) {
            item.h = s.size / 2;   // linha em branco = espaço
            item.w = 0;
        } else {
            auto font = Resources::GetFont("Recursos/font/times.ttf", s.size);
            if (!font) {
                continue;
            }
            SDL_Surface* sf = TTF_RenderUTF8_Blended_Wrapped(font.get(), s.text.c_str(), s.color, wrap);
            if (!sf) {
                continue;
            }
            item.w = sf->w;
            item.h = sf->h;
            SDL_Texture* raw = SDL_CreateTextureFromSurface(renderer, sf);
            SDL_FreeSurface(sf);
            if (!raw) {
                continue;
            }
            item.tex = std::shared_ptr<SDL_Texture>(raw, SDL_DestroyTexture);
        }

        creditsContentHeight += static_cast<float>(item.h) + item.gapAfter;
        creditsItems.push_back(std::move(item));
    }
}

void EndState::RenderCredits(SDL_Renderer* renderer) {
    if (!creditsBuilt) {
        BuildCredits(renderer);
        creditsBuilt = true;
    }

    const int winW = Game::GetInstance().GetWindowsWidth();
    const int winH = Game::GetInstance().GetWindowsHeight();

    // Fim do rolo: quando o conteúdo assentou perto do topo.
    const float endScroll = creditsContentHeight + static_cast<float>(winH) * 0.30f;
    if (creditsFinished) {
        creditsScrollY = endScroll;   // trava no fim (skip ou término natural)
    } else if (creditsScrollY >= endScroll) {
        creditsScrollY = endScroll;
        creditsFinished = true;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    float cum = 0.0f;
    for (const CreditsItem& item : creditsItems) {
        const float top = static_cast<float>(winH) + cum - creditsScrollY;
        if (item.tex && item.w > 0 && item.h > 0 && top + item.h > 0.0f && top < winH) {
            SDL_Rect dst{(winW - item.w) / 2, static_cast<int>(top), item.w, item.h};
            SDL_RenderCopy(renderer, item.tex.get(), nullptr, &dst);
        }
        cum += static_cast<float>(item.h) + item.gapAfter;
    }

    // Prompt ao terminar (pulsa suavemente).
    if (creditsFinished) {
        auto font = Resources::GetFont("Recursos/font/times.ttf", 26);
        if (font) {
            const float pulse = 0.6f + 0.4f * std::sin(static_cast<float>(SDL_GetTicks()) * 0.004f);
            SDL_Color col{200, 200, 200, 255};
            SDL_Surface* sf = TTF_RenderUTF8_Blended(font.get(), "Pressione Espaço para voltar", col);
            if (sf) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, sf);
                const int tw = sf->w, th = sf->h;
                SDL_FreeSurface(sf);
                if (t) {
                    SDL_SetTextureAlphaMod(t, static_cast<Uint8>(255.0f * pulse));
                    SDL_Rect d{(winW - tw) / 2, winH - th - 60, tw, th};
                    SDL_RenderCopy(renderer, t, nullptr, &d);
                    SDL_DestroyTexture(t);
                }
            }
        }
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}