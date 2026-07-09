#include "states/EndState.h"
#include "core/GameData.h"
#include "audio/GameSfx.h"
#include "audio/GameVoice.h"
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

EndState::EndState(bool creditsOnly) : creditsOnly(creditsOnly) {}

EndState::~EndState() = default;

void EndState::LoadAssets() {
    Mix_HaltMusic();

    // Créditos = vitória OU "Créditos" no menu principal. Nos créditos toca a
    // música dedicada; na derrota, a música de fim de jogo.
    const bool credits = GameData::playerVictory || creditsOnly;

    std::string musicFile = credits
        ? "Recursos/audio/soundtracks/ES_Zone of Deception - DEX 1200.mp3"
        : "Recursos/audio/soundtracks/Akane's_Regret.mp3";

    backgroundMusic.Open(musicFile);
    if (backgroundMusic.IsOpen()) {
        Mix_VolumeMusic(Game::MusicVolume());
        backgroundMusic.Play(-1);
    }

    if (credits) {
        // Rola os créditos finais. Abertos pelo menu: pula a intro de "vitória".
        creditsMode = true;
        if (creditsOnly) {
            introDone = true;
        }
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

    if (input.QuitRequested()) {   // só o X da janela fecha o jogo
        quitRequested = true;
    }

    if (creditsMode) {
        // Fase 1 — intro de escape/agradecimento (tela negra). Espaço/Esc a pulam.
        if (!introDone) {
            introTimer += dt;
            if (input.KeyPress(SPACE_KEY) || input.KeyPress(ESCAPE_KEY) || introTimer >= kIntroDuration) {
                introDone = true;
            }
            UpdateArray(dt);
            return;
        }

        // Aberto pelo menu: ESC volta ao menu a qualquer momento (botão "voltar").
        if (creditsOnly && input.KeyPress(ESCAPE_KEY)) {
            ReturnToMainMenu();
            return;
        }

        // Fase 2 — rolo de créditos/pós-créditos.
        if (!creditsFinished) {
            // Segurar ESPAÇO (e ESC na vitória) ACELERA o rolo (não pula ao fim).
            const bool fast = input.IsKeyDown(SPACE_KEY) || (!creditsOnly && input.IsKeyDown(ESCAPE_KEY));
            constexpr float kScrollSpeed = 130.0f;        // px/s (acompanha a fonte grande)
            constexpr float kFastScrollSpeed = 800.0f;    // rolagem rápida ao segurar
            creditsScrollY += (fast ? kFastScrollSpeed : kScrollSpeed) * dt;
        } else if (input.KeyPress(SPACE_KEY) || input.KeyPress(ESCAPE_KEY)) {
            ReturnToMainMenu();   // rolou tudo: ESPAÇO/ESC voltam ao menu
        }
        UpdateArray(dt);
        return;
    }

    // Tela de fim de jogo (morte): ESPAÇO e ESC voltam ao MENU (não fecham o jogo;
    // só o X da janela fecha, tratado acima).
    if (input.KeyPress(SPACE_KEY) || input.KeyPress(ESCAPE_KEY)) {
        ReturnToMainMenu();
    }

    LayoutCenteredText(gameOverTitle, -80.0f);
    LayoutCenteredText(continuePrompt, 60.0f);

    UpdateArray(dt);
}

// Volta ao menu principal em vez de fechar o app: sai deste EndState e empilha
// um TitleState novo. Um TitleState recém-criado (quitRequested=false) fica no
// topo, então o loop do jogo nunca encerra — funciona mesmo que não haja um
// TitleState válido embaixo.
void EndState::ReturnToMainMenu() {
    popRequested = true;
    // Aberto pelo menu: o TitleState já está embaixo — só volta a ele (sem
    // duplicar). Vitória/derrota: empilha um TitleState novo (a pilha pode estar
    // vazia embaixo, senão o jogo fecharia).
    if (!creditsOnly) {
        Game::GetInstance().Push(new TitleState());
    }
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
        if (!introDone) {
            RenderVictoryIntro(renderer);
        } else {
            RenderCredits(renderer);
        }
        return;
    }

    RenderArray();
}

void EndState::Start() {
    // Transição nível→fim/menu: silencia TODO o áudio de gameplay (rádio, ondas,
    // vento, monstro...) para nada sobreviver à morte/vitória. A música do EndState
    // (Mix_Music) é iniciada por LoadAssets e não é afetada.
    GameSfx::HardStopAll();
    GameVoice::StopAll();
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
    // Cores são IGNORADAS (o texto é forçado a branco puro em BuildCredits).
    // Regra de tamanho: função (rótulo) menor; nomes maiores (80).
    const SDL_Color w{255, 255, 255, 255};
    constexpr int kName = 80;   // nomes
    constexpr int kRole = 48;   // funções/rótulos (menor que os nomes)
    std::vector<CreditSpec> c;
    c.push_back({false, "The Last Lightkeeper", "", 100, 0, 60.0f, w});
    c.push_back({true, "", "Recursos/img/creditos/logo_unb.png", 26, 260, 30.0f, w});
    c.push_back({false, "Universidade de Brasília — UnB", "", 64, 0, 10.0f, w});
    c.push_back({false, "Introdução ao Desenvolvimento de Jogos", "", kRole, 0, 90.0f, w});

    c.push_back({false, "Professores", "", kRole, 0, 16.0f, w});
    c.push_back({false, "Alberto José Miranda Vaz", "", kName, 0, 8.0f, w});
    c.push_back({false, "Carla Denise Castanho", "", kName, 0, 8.0f, w});
    c.push_back({false, "Gabriel Lyra", "", kName, 0, 8.0f, w});
    c.push_back({false, "Mariana “Blue” Lima", "", kName, 0, 8.0f, w});
    c.push_back({false, "Fernanda dos Santos", "", kName, 0, 8.0f, w});
    c.push_back({false, "Miguel Eduardo Gutierrez “Meduag”", "", kName, 0, 100.0f, w});

    c.push_back({false, "— Equipe —", "", 72, 0, 60.0f, w});
    c.push_back({false, "Game Designers", "", kRole, 0, 16.0f, w});
    c.push_back({false, "Luana Duque", "", kName, 0, 8.0f, w});
    c.push_back({false, "Haru Braga Vasconcelos", "", kName, 0, 8.0f, w});
    c.push_back({false, "Flávio", "", kName, 0, 8.0f, w});
    c.push_back({false, "Bryan", "", kName, 0, 80.0f, w});
    c.push_back({false, "Roteiro e Design de Narrativa", "", kRole, 0, 16.0f, w});
    c.push_back({false, "Luana Duque", "", kName, 0, 80.0f, w});
    c.push_back({false, "Diretor de Arte", "", kRole, 0, 16.0f, w});
    c.push_back({false, "Haru Braga Vasconcelos", "", kName, 0, 80.0f, w});
    c.push_back({false, "Animadores", "", kRole, 0, 16.0f, w});
    c.push_back({false, "Bryan Gomes Silva", "", kName, 0, 8.0f, w});
    c.push_back({false, "Haru Braga Vasconcelos", "", kName, 0, 8.0f, w});
    c.push_back({false, "Luana Duque", "", kName, 0, 80.0f, w});
    c.push_back({false, "Design de Som e Direção de Dublagem", "", kRole, 0, 16.0f, w});
    c.push_back({false, "Luana Duque", "", kName, 0, 80.0f, w});
    c.push_back({false, "Elenco de Dublagem", "", kRole, 0, 16.0f, w});
    c.push_back({false, "Henrique do Nascimento Vieira", "", kName, 0, 8.0f, w});
    c.push_back({false, "Haru Braga Vasconcelos", "", kName, 0, 80.0f, w});

    c.push_back({false, "Obrigado por jogar", "", 88, 0, 60.0f, w});
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

void EndState::RenderVictoryIntro(SDL_Renderer* renderer) {
    const int winW = Game::GetInstance().GetWindowsWidth();
    const int winH = Game::GetInstance().GetWindowsHeight();

    // Envelope de alpha: aparece, segura, e some antes de dar lugar aos créditos.
    const float t = std::min(1.0f, introTimer / kIntroDuration);
    float alpha = 1.0f;
    if (t < 0.18f) {
        alpha = t / 0.18f;                    // fade-in
    } else if (t > 0.85f) {
        alpha = std::max(0.0f, (1.0f - t) / 0.15f);   // fade-out
    }
    const Uint8 a = static_cast<Uint8>(std::min(1.0f, alpha) * 255.0f);

    struct Line {
        const char* text;
        int size;
        SDL_Color color;
        float yOffset;
    };
    const SDL_Color warm{232, 210, 170, a};
    const SDL_Color soft{200, 200, 210, a};
    const Line lines[] = {
        {"Vocês escaparam.", 60, warm, -90.0f},
        {"A luz do farol vos guiou para fora da escuridão.", 28, soft, 0.0f},
        {"Obrigado por jogar.", 40, warm, 90.0f},
    };

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (const Line& ln : lines) {
        auto font = Resources::GetFont("Recursos/font/times.ttf", ln.size);
        if (!font) {
            continue;
        }
        SDL_Color col = ln.color;
        SDL_Surface* sf = TTF_RenderUTF8_Blended(font.get(), ln.text, col);
        if (!sf) {
            continue;
        }
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, sf);
        const int tw = sf->w, th = sf->h;
        SDL_FreeSurface(sf);
        if (!tex) {
            continue;
        }
        SDL_SetTextureAlphaMod(tex, a);
        SDL_Rect dst{(winW - tw) / 2,
                     static_cast<int>((winH - th) * 0.5f + ln.yOffset),
                     tw, th};
        SDL_RenderCopy(renderer, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

void EndState::BuildCredits(SDL_Renderer* renderer) {
    const int winW = Game::GetInstance().GetWindowsWidth();
    const Uint32 wrap = static_cast<Uint32>(winW * 0.85f);
    const SDL_Color kWhite{255, 255, 255, 255};

    creditsItems.clear();
    creditsContentHeight = 0.0f;

    // Rasteriza um texto para a textura de um item (usado por nomes e pelos avisos
    // de imagem ausente). Retorna false se a fonte/superfície falhar.
    auto rasterizeText = [&](CreditsItem& item, const std::string& text, int size,
                             SDL_Color color) -> bool {
        auto font = Resources::GetFont("Recursos/font/times.ttf", size);
        if (!font) {
            return false;
        }
        SDL_Surface* sf = TTF_RenderUTF8_Blended_Wrapped(font.get(), text.c_str(), color, wrap);
        if (!sf) {
            return false;
        }
        item.w = sf->w;
        item.h = sf->h;
        SDL_Texture* raw = SDL_CreateTextureFromSurface(renderer, sf);
        SDL_FreeSurface(sf);
        if (!raw) {
            return false;
        }
        item.tex = std::shared_ptr<SDL_Texture>(raw, SDL_DestroyTexture);
        return true;
    };

    for (const CreditSpec& s : LoadCreditSpecs()) {
        CreditsItem item;
        item.gapAfter = s.gap;

        if (s.isImage) {
            item.centered = true;   // artes ficam centralizadas
            auto tex = Resources::GetImage(s.imagePath);
            if (tex) {
                int tw = 0, th = 0;
                SDL_QueryTexture(tex.get(), nullptr, nullptr, &tw, &th);
                const float aspect = (th > 0) ? static_cast<float>(tw) / static_cast<float>(th) : 1.0f;
                item.h = std::max(1, s.height);
                item.w = static_cast<int>(item.h * aspect);
                item.tex = tex;
            } else {
                // Arte ausente: não some — vira um aviso visível (centralizado).
                const std::string base = s.imagePath.substr(s.imagePath.find_last_of("/\\") + 1);
                if (!rasterizeText(item, "[imagem ausente: " + base + "]", 40, kWhite)) {
                    continue;
                }
            }
        } else if (s.text.empty()) {
            item.h = s.size / 2;   // linha em branco = espaço
            item.w = 0;
        } else {
            item.centered = true;   // tudo centralizado
            // Cor dos textos: SEMPRE branco puro (ignora a cor do JSON).
            if (!rasterizeText(item, s.text, s.size, kWhite)) {
                continue;
            }
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

    // Tudo centralizado (texto e imagens).
    float cum = 0.0f;
    for (const CreditsItem& item : creditsItems) {
        const float top = static_cast<float>(winH) + cum - creditsScrollY;
        if (item.tex && item.w > 0 && item.h > 0 && top + item.h > 0.0f && top < winH) {
            const int x = (winW - item.w) / 2;
            SDL_Rect dst{x, static_cast<int>(top), item.w, item.h};
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
            SDL_Surface* sf = TTF_RenderUTF8_Blended(font.get(), "Pressione Espaço ou Esc para voltar", col);
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