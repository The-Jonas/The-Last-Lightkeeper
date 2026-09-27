#include "states/TitleState.h"
#include "states/LoadingState.h"
#include "states/EndState.h"
#include "states/stage/StageState.h"
#include "audio/GameSfx.h"
#include "audio/GameVoice.h"
#include "core/Game.h"
#include "core/InputManager.h"
#include "core/Resources.h"
#include "core/SaveManager.h"
#include "core/Telemetry.h"
#include "engine/Camera.h"
#include "engine/GameObject.h"
#include "engine/SpriteRenderer.h"
#include "ui/Text.h"
#include "nlohmann/json.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>

namespace {

constexpr float kPi        = 3.14159265f;
constexpr float kDegToRad  = kPi / 180.0f;
constexpr float kScreenW   = 1920.0f;
constexpr float kScreenH   = 1080.0f;

const char* kLayoutPath = "Recursos/img/menu/menu_config.json";
const char* kWallPath   = "Recursos/img/menu/parede.png";
const char* kLogoPath   = "Recursos/img/menu/LOGO_BRANCA_1.png";
const char* kMenuFont   = "Recursos/font/Broadsheet_0.ttf";
const char* kMusicPath  = "Recursos/audio/soundtracks/ES_Make up Your Mind - Hanna Lindgren.mp3";

const char* kMenuLabels[] = {"Continuar", "Novo Jogo", "Configurações", "Créditos", "Sair"};
const char* kTelemetryLabels[] = {"continuar", "novo_jogo", "opcoes", "creditos", "sair"};

// Deixa a textura com filtragem linear (a arte do menu é grande e é reduzida).
void SetLinear(const char* path) {
    if (auto t = Resources::GetImage(path)) {
        SDL_SetTextureScaleMode(t.get(), SDL_ScaleModeLinear);
    }
}

// Cria um GameObject com sprite que segue a câmera (interface desenhada a 1:1).
GameObject* MakeSprite(const char* path, int z, float x, float y, float w, float h) {
    auto* go = new GameObject();
    go->z = z;
    auto* sr = new SpriteRenderer(*go, path);
    sr->SetCameraFollower(true);
    go->AddComponent(sr);
    go->box = {x, y, w, h};
    return go;
}

}  // namespace

// ═════════════════════════════════════════════════════════════════════════════
//  Ciclo de vida
// ═════════════════════════════════════════════════════════════════════════════

TitleState::TitleState() : State() {}

// Libera o que não está no objectArray: textos do menu e alvos do desfoque.
TitleState::~TitleState() {
    for (GameObject*& t : menuTexts) { delete t; t = nullptr; }
    DestroyBlurTargets();
}

// Chegada ao menu (boot ou fase→menu): limpa sons da fase, monta a tela e toca a música.
void TitleState::Start() {
    Camera::ResetView();   // a interface é desenhada a 1:1; o zoom da fase encolheria tudo
    GameSfx::HardStopAll();
    GameVoice::StopAll();
    LoadAssets();
    StartArray();
    music.Open(kMusicPath);
    music.Play();
    started = true;
}

void TitleState::Pause() {}

// Volta de um estado empilhado (créditos, loading cancelado): câmera neutra e fade de novo.
void TitleState::Resume() {
    Camera::ResetView();
    Camera::pos = Vec2(0, 0);
    hasContinueSave = SaveManager::HasSave();
    menuSelection = hasContinueSave ? kContinue : kNewGame;
    fadeTimer = Timer();
    fadeAlpha = 0.0f;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Preparação
// ═════════════════════════════════════════════════════════════════════════════

// Monta a tela e já aponta o braço para a opção inicial (sem giro na entrada).
void TitleState::LoadAssets() {
    LoadLayout();
    PreloadArt();
    CreateObjects();

    hasContinueSave = SaveManager::HasSave();
    menuSelection = hasContinueSave ? kContinue : kNewGame;

    charFrameIndex = 0;
    charAnimTimer = 0.0f;
    fadeTimer = Timer();
    fadeAlpha = 0.0f;
    pulseTimer = 0.0f;

    LayoutAll();
    armAngle = ArmAngleTo(menuSelection);
}

// Lê menu_config.json; chaves ausentes mantêm o padrão do header.
void TitleState::LoadLayout() {
    std::ifstream f(kLayoutPath);
    if (!f.is_open()) { std::printf("[TitleState] %s nao encontrado\n", kLayoutPath); return; }
    try {
        nlohmann::json j;
        f >> j;
        auto getf = [](const nlohmann::json& o, const char* k, float& v) {
            if (o.contains(k)) v = o[k].get<float>();
        };
        if (j.contains("character")) {
            const auto& c = j["character"];
            getf(c, "centerX", layout.charCX);        getf(c, "bottomY", layout.charBottomY);
            getf(c, "width", layout.charW);           getf(c, "height", layout.charH);
            getf(c, "shoulderDX", layout.shoulderDX); getf(c, "shoulderDY", layout.shoulderDY);
        }
        if (j.contains("logo")) {
            const auto& l = j["logo"];
            getf(l, "x", layout.logoX);
            getf(l, "y", layout.logoY);
            if (l.contains("width")) {
                layout.logoW = l["width"].get<float>();
                layout.logoH = layout.logoW * (809.0f / 2444.0f);
            }
        }
        if (j.contains("menu")) {
            const auto& m = j["menu"];
            getf(m, "x", layout.menuX);
            getf(m, "startY", layout.menuStartY);
            getf(m, "spacing", layout.menuSpacing);
            if (m.contains("fontSize")) layout.menuFontSize = m["fontSize"].get<int>();
        }
        if (j.contains("arm")) {
            const auto& a = j["arm"];
            getf(a, "width", layout.armW);               getf(a, "height", layout.armH);
            getf(a, "lanternAlong", layout.lanternAlong); getf(a, "lanternPerp", layout.lanternPerp);
        }
        if (j.contains("darkness")) layout.darknessAlpha = static_cast<Uint8>(j["darkness"].get<int>());
    } catch (...) {
        std::printf("[TitleState] Erro ao parsear %s\n", kLayoutPath);
    }
}

// Carrega parede, logo, braço e todos os quadros do corpo com filtragem linear.
void TitleState::PreloadArt() {
    SetLinear(kWallPath);
    SetLinear(kLogoPath);
    SetLinear(ArmPath());
    for (int i = 1; i <= kCharFrames; ++i) {
        char path[192];
        BodyFramePath(i, path, sizeof(path));
        SetLinear(path);
    }
}

// Parede, logo e corpo vão para o objectArray; os textos ficam com este estado.
void TitleState::CreateObjects() {
    bg = MakeSprite(kWallPath, 0, 0.0f, 0.0f, kScreenW, kScreenH);
    AddObject(bg);

    logoGO = MakeSprite(kLogoPath, 5, layout.logoX, layout.logoY, layout.logoW, layout.logoH);
    AddObject(logoGO);

    char bodyPath[192];
    BodyFramePath(1, bodyPath, sizeof(bodyPath));
    charBodyGO = MakeSprite(bodyPath, 10, 0.0f, 0.0f, layout.charW, layout.charH);
    AddObject(charBodyGO);

    for (int i = 0; i < kOptionCount; ++i) {
        auto* go = new GameObject();
        go->AddComponent(new Text(*go, kMenuFont, layout.menuFontSize, Text::BLENDED,
                                  kMenuLabels[i], SDL_Color{220, 200, 150, 0}));
        menuTexts[i] = go;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Update
// ═════════════════════════════════════════════════════════════════════════════

// Com as configurações abertas, só elas recebem input; o fundo continua animando.
void TitleState::Update(float dt) {
    InputManager& input = InputManager::GetInstance();
    SDL_ShowCursor(settingsMenu.IsOpen() ? SDL_ENABLE : SDL_DISABLE);

    fadeTimer.Update(dt);
    pulseTimer += dt;

    if (settingsMenu.IsOpen()) {
        settingsMenu.Update(dt);
        UpdateCharAnim(dt);
        UpdateArray(dt);
        return;
    }

    if (input.QuitRequested() || input.KeyPress(ESCAPE_KEY)) {
        Telemetry::SetEndReason(input.QuitRequested() ? "window_close" : "escape_menu");
        quitRequested = true;
    }
    if (input.KeyPress(SDLK_w) || input.KeyPress(SDLK_UP))   MoveSelection(-1);
    if (input.KeyPress(SDLK_s) || input.KeyPress(SDLK_DOWN)) MoveSelection(+1);
    if (input.KeyPress(SDLK_f) || input.KeyPress(SPACE_KEY) || input.KeyPress(SDLK_RETURN)) {
        ActivateSelection();
    }

    fadeAlpha = std::min(1.0f, fadeTimer.Get() / kFadeDuration) * 255.0f;
    UpdateMenuColors();
    LayoutAll();
    UpdateCharAnim(dt);
    UpdateArm(dt);
    UpdateArray(dt);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Menu
// ═════════════════════════════════════════════════════════════════════════════

bool TitleState::IsOptionEnabled(int option) const {
    return option != kContinue || hasContinueSave;
}

// Anda uma opção para cima (-1) ou para baixo (+1), com volta, pulando as desativadas.
void TitleState::MoveSelection(int dir) {
    int next = (menuSelection + dir + kOptionCount) % kOptionCount;
    while (!IsOptionEnabled(next) && next != menuSelection) {
        next = (next + dir + kOptionCount) % kOptionCount;
    }
    menuSelection = next;
}

// Registra a escolha na telemetria e executa a opção.
void TitleState::ActivateSelection() {
    const int idx = (menuSelection >= 0 && menuSelection < kOptionCount) ? menuSelection : 0;
    Telemetry::Event("menu", Telemetry::Fields().Str("choice", kTelemetryLabels[idx])
                                                .Bool("hasSave", hasContinueSave));
    switch (menuSelection) {
    case kContinue: if (hasContinueSave) StartContinue(); break;
    case kNewGame:  StartNewGame(); break;
    case kSettings: settingsMenu.Open(); break;
    case kCredits:  Game::GetInstance().Push(new EndState(/*creditsOnly=*/true)); break;
    case kQuit:     Telemetry::SetEndReason("quit_menu"); quitRequested = true; break;
    default: break;
    }
}

// Apaga o save antigo e carrega a fase do início.
void TitleState::StartNewGame() {
    SaveManager::DeleteSave();
    Game::GetInstance().Push(new LoadingState(StageState::LoadMode::NewGame));
}

// Carrega a fase a partir do save (se ainda existir).
void TitleState::StartContinue() {
    if (!SaveManager::HasSave()) return;
    Game::GetInstance().Push(new LoadingState(StageState::LoadMode::Continue));
}

// ═════════════════════════════════════════════════════════════════════════════
//  Layout e animação
// ═════════════════════════════════════════════════════════════════════════════

// Posiciona parede, logo, corpo e textos; calcula o ombro e o centro de cada opção.
// Sem save, "Continuar" não ocupa espaço e as outras sobem uma posição.
void TitleState::LayoutAll() {
    const float camX = Camera::pos.x, camY = Camera::pos.y;
    if (bg)     bg->box = {camX, camY, kScreenW, kScreenH};
    if (logoGO) logoGO->box = {camX + layout.logoX, camY + layout.logoY, layout.logoW, layout.logoH};
    if (charBodyGO) {
        charBodyGO->box = {camX + layout.charCX - layout.charW * 0.5f,
                           camY + layout.charBottomY - layout.charH,
                           layout.charW, layout.charH};
    }

    shoulderX = layout.charCX + layout.shoulderDX;
    shoulderY = layout.charBottomY + layout.shoulderDY;

    for (int i = 0; i < kOptionCount; ++i) {
        if (!menuTexts[i]) continue;
        const int slot = hasContinueSave ? i : i - 1;
        if (slot < 0) continue;
        const float y = layout.menuStartY + slot * layout.menuSpacing;
        menuTexts[i]->box.x = camX + layout.menuX;
        menuTexts[i]->box.y = camY + y;
        optionPositions[i] = {layout.menuX + menuTexts[i]->box.w * 0.5f,
                              y + menuTexts[i]->box.h * 0.5f};
    }
}

// Ângulo (graus) do braço para a lanterna mirar no centro da opção.
float TitleState::ArmAngleTo(int option) const {
    const OptionPos& op = optionPositions[option];
    return std::atan2(op.cy - shoulderY, op.cx - shoulderX) / kDegToRad + 180.0f + kArmAngleOffset;
}

// Gira o braço pelo caminho mais curto até a opção selecionada.
void TitleState::UpdateArm(float dt) {
    float diff = ArmAngleTo(menuSelection) - armAngle;
    while (diff > 180.0f)  diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;
    armAngle += diff * std::min(1.0f, kArmTurnSpeed * dt);
}

// Avança a respiração do irmãozão (troca a textura mantendo a caixa).
void TitleState::UpdateCharAnim(float dt) {
    charAnimTimer += dt;
    if (charAnimTimer < kCharFrameSeconds) return;
    charAnimTimer -= kCharFrameSeconds;
    charFrameIndex = (charFrameIndex + 1) % kCharFrames;

    if (!charBodyGO) return;
    auto* sr = charBodyGO->GetComponent<SpriteRenderer>();
    if (!sr) return;
    char path[192];
    BodyFramePath(charFrameIndex + 1, path, sizeof(path));
    const Rect saved = charBodyGO->box;
    sr->Open(path);
    charBodyGO->box = saved;
}

// Cores das opções com o fade: desativada apagada, selecionada dourada pulsando, resto cinza.
void TitleState::UpdateMenuColors() {
    const Uint8 a = static_cast<Uint8>(fadeAlpha);
    const float pulse = (std::sin(pulseTimer * 2.0f) + 1.0f) * 0.5f;
    for (int i = 0; i < kOptionCount; ++i) {
        if (!menuTexts[i]) continue;
        Text* txt = menuTexts[i]->GetComponent<Text>();
        if (!txt) continue;
        if (!IsOptionEnabled(i)) {
            txt->SetColor({80, 80, 80, static_cast<Uint8>(a / 3)});
        } else if (i == menuSelection) {
            txt->SetColor({static_cast<Uint8>(200.0f + pulse * 55.0f),
                           static_cast<Uint8>(160.0f + pulse * 55.0f),
                           static_cast<Uint8>(40.0f + pulse * 40.0f), a});
        } else {
            txt->SetColor({130, 130, 130, a});
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Desenho
// ═════════════════════════════════════════════════════════════════════════════

// Ordem: parede e logo → película escura → cone de luz → opções → irmãozão → configurações.
void TitleState::Render() {
    SDL_Renderer* r = Game::GetInstance().GetRenderer();
    if (bg)     bg->Render();
    if (logoGO) logoGO->Render();
    RenderDarkness(r);
    RenderLanternCone(r);
    RenderMenuTexts();
    RenderBlurredCharacter(r);
    settingsMenu.Render(r);
}

// Escurece parede e logo por igual.
void TitleState::RenderDarkness(SDL_Renderer* r) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, layout.darknessAlpha);
    const SDL_Rect full{0, 0, Game::GetInstance().GetWindowsWidth(), Game::GetInstance().GetWindowsHeight()};
    SDL_RenderFillRect(r, &full);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

// Feixe aditivo da lente até a opção (trapézio que abre e some) + círculo de luz nela.
void TitleState::RenderLanternCone(SDL_Renderer* r) {
    if (fadeAlpha < 10.0f || !r || settingsMenu.IsOpen()) return;

    constexpr float kLensRadius = 35.0f;    // meia largura do feixe na lente
    constexpr float kSpotRadius = 130.0f;   // raio do círculo na opção
    constexpr int   kSpotSegs   = 24;

    const float rad = (armAngle + 180.0f) * kDegToRad;
    const float ax = std::cos(rad), ay = std::sin(rad);
    const float lx = shoulderX + ax * (layout.armW * layout.lanternAlong) - ay * layout.lanternPerp;
    const float ly = shoulderY + ay * (layout.armW * layout.lanternAlong) + ax * layout.lanternPerp;
    const OptionPos& op = optionPositions[menuSelection];

    const Uint8 a = static_cast<Uint8>(fadeAlpha * 0.35f);
    const SDL_Color lens{255, 230, 140, a};
    const SDL_Color edge{255, 220, 100, 0};

    const float beam = std::atan2(op.cy - ly, op.cx - lx) + kPi * 0.5f;
    const float px = std::cos(beam), py = std::sin(beam);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_ADD);

    SDL_Vertex v[6];
    v[0] = {{lx + px * kLensRadius, ly + py * kLensRadius}, lens, {0, 0}};
    v[1] = {{lx - px * kLensRadius, ly - py * kLensRadius}, lens, {0, 0}};
    v[2] = {{op.cx + px * kSpotRadius, op.cy + py * kSpotRadius}, edge, {0, 0}};
    v[3] = v[1];
    v[4] = {{op.cx - px * kSpotRadius, op.cy - py * kSpotRadius}, edge, {0, 0}};
    v[5] = v[2];
    SDL_RenderGeometry(r, nullptr, v, 6, nullptr, 0);

    SDL_Vertex sv[3];
    sv[0] = {{op.cx, op.cy}, {255, 230, 140, static_cast<Uint8>(a * 0.8f)}, {0, 0}};
    for (int i = 0; i < kSpotSegs; ++i) {
        const float a0 = (static_cast<float>(i) / kSpotSegs) * 2.0f * kPi;
        const float a1 = (static_cast<float>(i + 1) / kSpotSegs) * 2.0f * kPi;
        sv[1] = {{op.cx + std::cos(a0) * kSpotRadius, op.cy + std::sin(a0) * kSpotRadius}, edge, {0, 0}};
        sv[2] = {{op.cx + std::cos(a1) * kSpotRadius, op.cy + std::sin(a1) * kSpotRadius}, edge, {0, 0}};
        SDL_RenderGeometry(r, nullptr, sv, 3, nullptr, 0);
    }

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

// Desenha as opções; sem save, "Continuar" não aparece.
void TitleState::RenderMenuTexts() {
    for (int i = 0; i < kOptionCount; ++i) {
        if (!menuTexts[i] || !IsOptionEnabled(i)) continue;
        menuTexts[i]->Render();
    }
}

// Braço com pivô na ponta direita (o ombro), girado em armAngle.
void TitleState::RenderArm(SDL_Renderer* r) {
    auto tex = Resources::GetImage(ArmPath());
    if (!tex || !r) return;
    const SDL_Point pivot{static_cast<int>(layout.armW), static_cast<int>(layout.armH * 0.5f)};
    const SDL_Rect dst{static_cast<int>(shoulderX + Camera::pos.x - layout.armW),
                       static_cast<int>(shoulderY + Camera::pos.y - layout.armH * 0.5f),
                       static_cast<int>(layout.armW), static_cast<int>(layout.armH)};
    SDL_SetTextureAlphaMod(tex.get(), 255);
    SDL_RenderCopyEx(r, tex.get(), nullptr, &dst, static_cast<double>(armAngle), &pivot, SDL_FLIP_NONE);
}

// Braço atrás, corpo na frente.
void TitleState::RenderCharacter(SDL_Renderer* r) {
    RenderArm(r);
    if (charBodyGO) charBodyGO->Render();
}

// Desfoque por pirâmide: desenha o irmãozão num alvo de tela cheia, reduz ÷2 a
// cada nível (média 2×2 linear), sobe de volta ×2 por nível e amplia na tela.
// Sem alvos disponíveis, desenha sem desfoque.
void TitleState::RenderBlurredCharacter(SDL_Renderer* r) {
    const int winW = Game::GetInstance().GetWindowsWidth();
    const int winH = Game::GetInstance().GetWindowsHeight();
    if (!EnsureBlurTargets(r, winW, winH)) {
        RenderCharacter(r);
        return;
    }

    SDL_Texture* prev = SDL_GetRenderTarget(r);
    auto clearTarget = [r](SDL_Texture* t) {
        SDL_SetRenderTarget(r, t);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 0);
        SDL_RenderClear(r);
    };

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    clearTarget(charBlurFull);
    RenderCharacter(r);

    SDL_Texture* src = charBlurFull;
    for (int i = 0; i < kBlurLevels; ++i) {
        clearTarget(charBlurChain[i]);
        SDL_RenderCopy(r, src, nullptr, nullptr);
        src = charBlurChain[i];
    }
    for (int i = kBlurLevels - 2; i >= 0; --i) {
        clearTarget(charBlurChain[i]);
        SDL_RenderCopy(r, charBlurChain[i + 1], nullptr, nullptr);
    }

    SDL_SetRenderTarget(r, prev);
    const SDL_Rect full{0, 0, winW, winH};
    SDL_SetTextureAlphaMod(charBlurChain[0], 255);
    SDL_RenderCopy(r, charBlurChain[0], nullptr, &full);
}

// Cria (uma vez) o alvo de tela cheia e os níveis ÷2..÷16; true se todos existem.
bool TitleState::EnsureBlurTargets(SDL_Renderer* r, int w, int h) {
    if (!charBlurFull) {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
        auto make = [r](int tw, int th) {
            SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                               std::max(1, tw), std::max(1, th));
            if (t) {
                SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
                SDL_SetTextureScaleMode(t, SDL_ScaleModeLinear);
            }
            return t;
        };
        charBlurFull = make(w, h);
        for (int i = 0; i < kBlurLevels; ++i) {
            const int div = 1 << (i + 1);
            charBlurChain[i] = make(w / div, h / div);
        }
    }
    if (!charBlurFull) return false;
    for (SDL_Texture* t : charBlurChain) {
        if (!t) return false;
    }
    return true;
}

void TitleState::DestroyBlurTargets() {
    if (charBlurFull) { SDL_DestroyTexture(charBlurFull); charBlurFull = nullptr; }
    for (SDL_Texture*& t : charBlurChain) {
        if (t) { SDL_DestroyTexture(t); t = nullptr; }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Caminhos da arte
// ═════════════════════════════════════════════════════════════════════════════

const char* TitleState::ArmPath() {
    return kUseLuanaArt ? "Recursos/img/menu/luana/braco_irmaozao.png"
                        : "Recursos/img/menu/braco_irmaozao.png";
}

// Monta o caminho do quadro `frame1based` (1..kCharFrames) do corpo.
void TitleState::BodyFramePath(int frame1based, char* out, std::size_t n) {
    const char* fmt = kUseLuanaArt
        ? "Recursos/img/menu/luana/irmao_costas/Costas_irmaozao_%04d.png"
        : "Recursos/img/menu/irmao_costas/Costas_irmaozao_%04d.png";
    std::snprintf(out, n, fmt, frame1based);
}