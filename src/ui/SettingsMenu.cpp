#include "ui/SettingsMenu.h"
#include "ui/KeyGlyphs.h"
#include "core/Game.h"
#include "core/Resources.h"
#include "states/BrightnessCalibrationState.h"

#define INCLUDE_SDL_TTF
#include "SDL_include.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <string>

namespace {

const char* kFontPath = "Recursos/font/Broadsheet_0.ttf";

// ── Paleta (a mesma da pasta de documentos) ─────────────────────────────────
const SDL_Color kGold  {235, 215, 160, 255};   // ativo: título, seleção, valores, sliders
const SDL_Color kText  {200, 195, 175, 255};   // rótulos não selecionados
const SDL_Color kDim   {150, 140, 125, 255};   // seções, notas, dicas
const SDL_Color kTrack { 70,  66,  58, 255};   // trilho do slider
const SDL_Color kWarn  {220, 120, 100, 255};   // tecla recusada

const char* kRowLabels[] = {
    "Volume geral", "Música e ambiente", "Efeitos", "Vozes",
    "Brilho", "Calibrar brilho", "Modo de tela", "Resolução", "VSync", "Limite de FPS",
    "Reduzir flashes", "Controles",
    "Voltar",
};

// Teclas aceitas no remapeamento (mesma lista das telas antigas).
const int kAllowedKeys[] = {
    SDLK_a, SDLK_b, SDLK_c, SDLK_d, SDLK_e, SDLK_f, SDLK_g, SDLK_h, SDLK_i, SDLK_j,
    SDLK_k, SDLK_l, SDLK_m, SDLK_n, SDLK_o, SDLK_p, SDLK_q, SDLK_r, SDLK_s, SDLK_t,
    SDLK_u, SDLK_v, SDLK_w, SDLK_x, SDLK_y, SDLK_z,
    SDLK_0, SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5, SDLK_6, SDLK_7, SDLK_8, SDLK_9,
    SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT,
    SDLK_RETURN, SDLK_SPACE, SDLK_BACKSPACE, SDLK_TAB,
    SDLK_LSHIFT, SDLK_RSHIFT, SDLK_LCTRL, SDLK_RCTRL, SDLK_LALT, SDLK_RALT,
};

// ── Layout da página principal (espaço lógico 1920x1080) ────────────────────
constexpr int kPanelW   = 980;
constexpr int kRowH     = 46;
constexpr int kRowGap   = 4;
constexpr int kHeaderH  = 40;   // altura de um cabeçalho de seção
constexpr int kTitleH   = 96;   // título + linha dourada
constexpr int kFooterH  = 116;  // nota + dicas de teclas (teclas grandes)
constexpr int kLabelX   = 44;   // rótulo, a partir da borda do painel
constexpr int kValueX   = 470;  // início da coluna de valores
constexpr int kValueW   = 420;  // largura da coluna de valores (setas nas pontas)
constexpr float kHintKeyScale = 2.6f;   // tamanho das teclas nas dicas do rodapé
constexpr int kHintBottomGap = 52;   // distância do meio da linha de dicas até a borda de baixo do painel
constexpr int kNoteOffsetY = -11;   // posição da nota do rodapé em relação ao fim da lista (negativo = sobe)

// ── Layout da página Controles: lista única, tecla escrita à direita ────────
constexpr int kCtrlPanelW = 820;
constexpr int kCtrlRowH   = 50;
constexpr int kCtrlGap    = 4;
constexpr int kCtrlKeyX   = 520;    // início da coluna da tecla (a partir da borda do painel)
constexpr int kCtrlKeyW   = 220;    // largura da "etiqueta" da tecla

// Cabeçalho de seção antes da linha `row` (nullptr = nenhum).
const char* SectionBefore(int row) {
    switch (row) {
    case 0:  return "ÁUDIO";
    case 4:  return "IMAGEM";
    case 10: return "JOGO";
    default: return nullptr;
    }
}

// Texto com o canto superior-esquerdo em (x, y); devolve a largura desenhada.
int DrawText(SDL_Renderer* r, TTF_Font* font, const std::string& text, int x, int y, SDL_Color c) {
    if (!font || text.empty()) return 0;
    SDL_Surface* s = TTF_RenderUTF8_Blended(font, text.c_str(), SDL_Color{c.r, c.g, c.b, 255});
    if (!s) return 0;
    const int w = s->w;
    if (SDL_Texture* t = SDL_CreateTextureFromSurface(r, s)) {
        SDL_SetTextureAlphaMod(t, c.a);
        const SDL_Rect d{x, y, s->w, s->h};
        SDL_RenderCopy(r, t, nullptr, &d);
        SDL_DestroyTexture(t);
    }
    SDL_FreeSurface(s);
    return w;
}

// Texto centrado horizontalmente em cx, com o topo em y.
void DrawTextCentered(SDL_Renderer* r, TTF_Font* font, const std::string& text, int cx, int y, SDL_Color c) {
    if (!font || text.empty()) return;
    int w = 0, h = 0;
    TTF_SizeUTF8(font, text.c_str(), &w, &h);
    DrawText(r, font, text, cx - w / 2, y, c);
}

// Desenha a linha de dicas com as teclas, centrada em cx e com o MEIO em centerY
// (teclas altas, como o Enter, crescem para cima e para baixo por igual).
void DrawKeyHint(SDL_Renderer* r, TTF_Font* font, const std::string& text, int cx, int centerY) {
    if (!font) return;
    int w = 0, h = 0;
    KeyGlyphs::Measure(font, text, kHintKeyScale, w, h);
    KeyGlyphs::Draw(r, font, text, cx - w / 2, centerY - h / 2, kDim, 255, kHintKeyScale);
}

// Triângulo cheio com borda suave (~1,5 px), via SDL_RenderGeometry — o mesmo
// desenho das setas da pasta de documentos. Pontas p0 (bico), p1, p2 (base).
void DrawTriangle(SDL_Renderer* r, SDL_FPoint p0, SDL_FPoint p1, SDL_FPoint p2, SDL_Color c) {
    const SDL_FPoint p[3] = {p0, p1, p2};
    const SDL_FPoint center{(p0.x + p1.x + p2.x) / 3.0f, (p0.y + p1.y + p2.y) / 3.0f};
    const float push = 3.0f;
    SDL_Vertex v[6];
    for (int i = 0; i < 3; ++i) {
        const float dx = p[i].x - center.x, dy = p[i].y - center.y;
        const float len = std::max(0.001f, std::sqrt(dx * dx + dy * dy));
        v[i]     = SDL_Vertex{p[i], c, {0.0f, 0.0f}};
        v[i + 3] = SDL_Vertex{{p[i].x + dx / len * push, p[i].y + dy / len * push},
                              SDL_Color{c.r, c.g, c.b, 0}, {0.0f, 0.0f}};
    }
    const int idx[21] = {0, 1, 2,  0, 1, 4,  0, 4, 3,  1, 2, 5,  1, 5, 4,  2, 0, 3,  2, 3, 5};
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(r, nullptr, v, 6, idx, 21);
}

// Seta horizontal centrada em (cx, cy); left = aponta para a esquerda.
void DrawSideArrow(SDL_Renderer* r, float cx, float cy, bool left, SDL_Color c) {
    const float halfH = 8.0f, w = 10.0f;
    const float tipX  = left ? cx - w * 0.5f : cx + w * 0.5f;
    const float baseX = left ? cx + w * 0.5f : cx - w * 0.5f;
    DrawTriangle(r, SDL_FPoint{tipX, cy}, SDL_FPoint{baseX, cy - halfH}, SDL_FPoint{baseX, cy + halfH}, c);
}

// Escurece a tela inteira e desenha o painel (fundo neutro + borda dourada).
SDL_Rect DrawPanel(SDL_Renderer* r, int panelW, int panelH) {
    const int winW = Game::GetInstance().GetWindowsWidth();
    const int winH = Game::GetInstance().GetWindowsHeight();
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 190);
    const SDL_Rect full{0, 0, winW, winH};
    SDL_RenderFillRect(r, &full);

    const SDL_Rect panel{(winW - panelW) / 2, (winH - panelH) / 2, panelW, panelH};
    SDL_SetRenderDrawColor(r, 22, 20, 18, 235);
    SDL_RenderFillRect(r, &panel);
    SDL_SetRenderDrawColor(r, kGold.r, kGold.g, kGold.b, 110);
    SDL_RenderDrawRect(r, &panel);
    return panel;
}

// Título centrado + traço dourado curto logo abaixo.
void DrawTitle(SDL_Renderer* r, const SDL_Rect& panel, const char* title) {
    auto font = Resources::GetFont(kFontPath, 38);
    DrawTextCentered(r, font.get(), title, panel.x + panel.w / 2, panel.y + 26, kGold);
    SDL_SetRenderDrawColor(r, kGold.r, kGold.g, kGold.b, 140);
    const SDL_Rect line{panel.x + panel.w / 2 - 60, panel.y + 76, 120, 2};
    SDL_RenderFillRect(r, &line);
}

// Destaque da linha selecionada: fundo dourado translúcido + barra de acento.
void DrawSelection(SDL_Renderer* r, const SDL_Rect& row) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, kGold.r, kGold.g, kGold.b, 30);
    SDL_RenderFillRect(r, &row);
    SDL_SetRenderDrawColor(r, kGold.r, kGold.g, kGold.b, 255);
    const SDL_Rect accent{row.x, row.y, 4, row.h};
    SDL_RenderFillRect(r, &accent);
}

}  // namespace

// ── Abrir / fechar ──────────────────────────────────────────────────────────

void SettingsMenu::Open() {
    open = true;
    page = Page::Main;
    selection = 0;
    awaitingRebind = false;
    dragging = false;
    holdTimer = 0.0f;
}
// Abre o menu na página principal, com a primeira linha selecionada.

void SettingsMenu::Close() {
    open = false;
    page = Page::Main;
    awaitingRebind = false;
    dragging = false;
    Game::SaveSettings();
}
// Fecha o menu e grava tudo em config/settings.json.

// ── Valores dos sliders ─────────────────────────────────────────────────────

void SettingsMenu::SliderRange(int row, int& lo, int& hi, int& step) {
    lo   = (row == kBrightness) ? 50 : 0;
    hi   = (row == kBrightness) ? 150 : 100;
    step = (row == kBrightness) ? 2 : 5;
}
// Faixa e passo de cada slider: volumes 0..100 (5 em 5), brilho 50..150 (2 em 2).

int SettingsMenu::SliderValue(int row) {
    switch (row) {
    case kMaster:     return Game::masterVolumePercent;
    case kAmbient:    return Game::ambientVolumePercent;
    case kSfx:        return Game::sfxVolumePercent;
    case kVoice:      return Game::voiceVolumePercent;
    case kBrightness: return Game::brightnessPercent;
    default:          return 0;
    }
}
// Valor atual do slider da linha.

void SettingsMenu::SetSliderValue(int row, int v) {
    int lo, hi, step;
    SliderRange(row, lo, hi, step);
    v = std::max(lo, std::min(hi, v));
    switch (row) {
    case kMaster:     Game::SetMasterVolume(v); break;
    case kAmbient:    Game::SetAmbientVolume(v); break;
    case kSfx:        Game::SetSfxVolume(v); break;
    case kVoice:      Game::SetVoiceVolume(v); break;
    case kBrightness: Game::SetBrightness(v); break;
    default: break;
    }
}
// Aplica um valor ao slider (limitado à faixa da linha).

// ── Ações ───────────────────────────────────────────────────────────────────

void SettingsMenu::Adjust(int row, int dir) {
    if (IsSlider(row)) {
        int lo, hi, step;
        SliderRange(row, lo, hi, step);
        SetSliderValue(row, SliderValue(row) + dir * step);
        return;
    }
    switch (row) {
    case kDisplayMode: {
        const int n = Game::DisplayModeCount();
        Game::ApplyDisplayMode(((Game::displayMode + dir) % n + n) % n);   // aplica na hora
        break;
    }
    case kResolution:
        Game::CycleResolution(dir);
        break;
    case kVSync:
        Game::SetVSync(!Game::vsync);                             // aplica na hora
        break;
    case kFpsCap:
        Game::CycleFpsCap(dir);
        break;
    case kReduceFlash:
        Game::reduceFlashing = !Game::reduceFlashing;
        break;
    default:
        break;
    }
}
// A/D numa linha: move o slider ou troca a opção (modo de tela e VSync aplicam na hora).

void SettingsMenu::Activate(int row) {
    // Resolução diferente da que está em uso: Enter grava e reinicia o jogo.
    if (row == kResolution && Game::resolutionIndex != Game::appliedResolutionIndex) {
        Game::SaveSettings();
        Game::RestartApplication();   // relança o executável e encerra este
        return;
    }
    if (IsOption(row)) {
        Adjust(row, +1);   // Enter/clique numa opção avança para a próxima
        return;
    }
    switch (row) {
    case kCalibrate:
        Game::GetInstance().Push(new BrightnessCalibrationState(false));
        break;
    case kControls:
        page = Page::Controls;
        controlsSelection = 0;
        break;
    case kBack:
        Close();
        break;
    default:
        break;             // sliders: Enter não faz nada
    }
}
// Enter/clique: abre calibração/controles, volta, ou avança a opção da linha.

// ── Entrada ─────────────────────────────────────────────────────────────────

void SettingsMenu::Update(float dt) {
    if (!open) return;
    if (page == Page::Main) UpdateMain(dt);
    else                    UpdateControls(dt);
}
// Encaminha a entrada para a página atual.

void SettingsMenu::UpdateMain(float dt) {
    InputManager& in = InputManager::GetInstance();

    if (in.KeyPress(SDLK_ESCAPE)) {
        Close();
        return;
    }

    const bool up   = in.KeyPress(SDLK_UP)   || in.ActionPress(GameAction::MoveUp);
    const bool down = in.KeyPress(SDLK_DOWN) || in.ActionPress(GameAction::MoveDown);
    if (up)   selection = (selection + kRowCount - 1) % kRowCount;
    if (down) selection = (selection + 1) % kRowCount;

    // A/D e setas: um passo por toque; nos sliders, segurar repete.
    const bool lp = in.KeyPress(SDLK_LEFT)  || in.ActionPress(GameAction::MoveLeft);
    const bool rp = in.KeyPress(SDLK_RIGHT) || in.ActionPress(GameAction::MoveRight);
    const bool ld = in.IsKeyDown(SDLK_LEFT)  || in.ActionDown(GameAction::MoveLeft);
    const bool rd = in.IsKeyDown(SDLK_RIGHT) || in.ActionDown(GameAction::MoveRight);
    if (lp) { Adjust(selection, -1); holdTimer = -kHoldDelay; }
    if (rp) { Adjust(selection, +1); holdTimer = -kHoldDelay; }
    if (IsSlider(selection) && ld != rd) {
        holdTimer += dt;
        while (holdTimer >= kHoldRepeat) {
            holdTimer -= kHoldRepeat;
            Adjust(selection, ld ? -1 : +1);
        }
    } else if (!ld && !rd) {
        holdTimer = 0.0f;
    }

    // Mouse: hover seleciona; clique nas setas troca a opção, no slider arrasta,
    // no resto da linha ativa.
    const SDL_Point mp{in.GetMouseX(), in.GetMouseY()};
    for (int i = 0; i < kRowCount; ++i) {
        if (SDL_PointInRect(&mp, &rowRects[i])) selection = i;
    }
    if (in.MousePress(SDL_BUTTON_LEFT)) {
        if (IsOption(selection) && SDL_PointInRect(&mp, &leftArrowRects[selection])) {
            Adjust(selection, -1);
        } else if (IsOption(selection) && SDL_PointInRect(&mp, &rightArrowRects[selection])) {
            Adjust(selection, +1);
        } else if (IsSlider(selection) && SDL_PointInRect(&mp, &sliderRects[selection])) {
            dragging = true;
        } else if (SDL_PointInRect(&mp, &rowRects[selection])) {
            Activate(selection);
            if (!open) return;
        }
    }
    if (dragging) {
        if (in.IsMouseDown(SDL_BUTTON_LEFT) && IsSlider(selection)) {
            const SDL_Rect& sr = sliderRects[selection];
            int lo, hi, step;
            SliderRange(selection, lo, hi, step);
            float frac = (sr.w > 0) ? static_cast<float>(mp.x - sr.x) / static_cast<float>(sr.w) : 0.0f;
            frac = std::max(0.0f, std::min(1.0f, frac));
            SetSliderValue(selection, lo + static_cast<int>(frac * (hi - lo) + 0.5f));
        } else {
            dragging = false;
        }
    }

    if (in.KeyPress(SDLK_RETURN) || in.KeyPress(SDLK_SPACE) || in.ActionPress(GameAction::Interact)) {
        Activate(selection);
    }
}
// Página principal: navegar, ajustar (com repetição), mouse e Enter/ESC.

void SettingsMenu::UpdateControls(float dt) {
    InputManager& in = InputManager::GetInstance();
    if (rebindInvalidTimer > 0.0f) rebindInvalidTimer -= dt;

    // Esperando a nova tecla para a ação selecionada.
    if (awaitingRebind) {
        if (in.KeyPress(SDLK_ESCAPE)) {
            awaitingRebind = false;
            return;
        }
        const int key = in.PollAnyKeyPressed();
        if (key != 0) {
            const bool allowed = std::find(std::begin(kAllowedKeys), std::end(kAllowedKeys), key)
                                 != std::end(kAllowedKeys);
            if (allowed) {
                in.SetBinding(rebindAction, key);
                awaitingRebind = false;
                rebindInvalidTimer = 0.0f;
            } else {
                rebindInvalidTimer = 2.0f;
            }
        }
        return;
    }

    if (in.KeyPress(SDLK_ESCAPE)) {   // volta para a página principal
        page = Page::Main;
        Game::SaveSettings();
        return;
    }

    // Aqui só as setas e W/S cruas: as ações podem estar a ser remapeadas.
    if (in.KeyPress(SDLK_UP) || in.KeyPress(SDLK_w)) {
        controlsSelection = (controlsSelection + kControlsRowCount - 1) % kControlsRowCount;
    }
    if (in.KeyPress(SDLK_DOWN) || in.KeyPress(SDLK_s)) {
        controlsSelection = (controlsSelection + 1) % kControlsRowCount;
    }
    const SDL_Point mp{in.GetMouseX(), in.GetMouseY()};
    for (int i = 0; i < kControlsRowCount; ++i) {
        if (SDL_PointInRect(&mp, &controlsRowRects[i])) controlsSelection = i;
    }

    bool activate = in.KeyPress(SDLK_RETURN) || in.KeyPress(SDLK_SPACE);
    if (in.MousePress(SDL_BUTTON_LEFT) && SDL_PointInRect(&mp, &controlsRowRects[controlsSelection])) {
        activate = true;
    }
    if (!activate) return;

    if (controlsSelection < InputManager::ActionCount) {
        awaitingRebind = true;
        rebindAction = static_cast<GameAction>(controlsSelection);
    } else if (controlsSelection == InputManager::ActionCount) {
        in.ResetBindingsToDefault();
    } else {
        page = Page::Main;
        Game::SaveSettings();
    }
}
// Página Controles: escolher ação, capturar tecla (ESC cancela), restaurar padrão.

// ── Desenho ─────────────────────────────────────────────────────────────────

void SettingsMenu::Render(SDL_Renderer* r) {
    if (!open || !r) return;
    if (page == Page::Main) RenderMain(r);
    else                    RenderControls(r);
}
// Desenha a página atual.

void SettingsMenu::RenderMain(SDL_Renderer* r) {
    // Altura total: título + seções + linhas + um respiro antes do "Voltar" + rodapé.
    int contentH = 0;
    for (int i = 0; i < kRowCount; ++i) {
        if (SectionBefore(i)) contentH += kHeaderH;
        if (i == kBack) contentH += 16;
        contentH += kRowH + kRowGap;
    }
    const SDL_Rect panel = DrawPanel(r, kPanelW, kTitleH + contentH + kFooterH);
    DrawTitle(r, panel, "Configurações");

    auto font    = Resources::GetFont(kFontPath, 26);
    auto section = Resources::GetFont(kFontPath, 19);
    auto small   = Resources::GetFont(kFontPath, 20);
    auto hint    = Resources::GetFont(kFontPath, 22);

    const int valueX = panel.x + kValueX;
    int y = panel.y + kTitleH;

    for (int i = 0; i < kRowCount; ++i) {
        // Cabeçalho de seção: nome em versalete apagado + traço até a borda.
        if (const char* sec = SectionBefore(i)) {
            const int sy = y + 12;
            const int tw = DrawText(r, section.get(), sec, panel.x + kLabelX, sy, kDim);
            SDL_SetRenderDrawColor(r, kDim.r, kDim.g, kDim.b, 70);
            const SDL_Rect rule{panel.x + kLabelX + tw + 14, sy + 11, panel.w - 2 * kLabelX - tw - 14, 1};
            SDL_RenderFillRect(r, &rule);
            y += kHeaderH;
        }
        if (i == kBack) y += 16;

        rowRects[i] = SDL_Rect{panel.x + 24, y, panel.w - 48, kRowH};
        const bool sel = (i == selection);
        const SDL_Color col = sel ? kGold : kText;
        const int cy = y + kRowH / 2;
        const int textY = y + (kRowH - 30) / 2;

        if (sel) DrawSelection(r, rowRects[i]);
        DrawText(r, font.get(), kRowLabels[i], panel.x + kLabelX, textY, col);

        if (IsSlider(i)) {
            // Trilho fino, preenchimento dourado, puxador e o número à direita.
            const int barW = kValueW - 60, barH = 4;
            sliderRects[i] = SDL_Rect{valueX, y + 6, barW, kRowH - 12};
            int lo, hi, step;
            SliderRange(i, lo, hi, step);
            const int val = SliderValue(i);
            const float frac = (hi > lo) ? static_cast<float>(val - lo) / static_cast<float>(hi - lo) : 0.0f;
            const int fillW = static_cast<int>(barW * frac);

            SDL_SetRenderDrawColor(r, kTrack.r, kTrack.g, kTrack.b, 255);
            const SDL_Rect track{valueX, cy - barH / 2, barW, barH};
            SDL_RenderFillRect(r, &track);
            const SDL_Color fillCol = sel ? kGold : kDim;
            SDL_SetRenderDrawColor(r, fillCol.r, fillCol.g, fillCol.b, 255);
            const SDL_Rect fill{valueX, cy - barH / 2, fillW, barH};
            SDL_RenderFillRect(r, &fill);
            const SDL_Rect knob{valueX + fillW - 5, cy - 9, 10, 18};
            SDL_RenderFillRect(r, &knob);

            DrawText(r, font.get(), std::to_string(val), valueX + barW + 22, textY, col);
        } else if (IsOption(i)) {
            // ◀ valor ▶ — setas desenhadas nas pontas da coluna, valor centrado.
            std::string value;
            if (i == kDisplayMode)       value = Game::CurrentDisplayModeLabel();
            else if (i == kResolution)   value = Game::CurrentResolutionLabel();
            else if (i == kVSync)        value = Game::vsync ? "Ligado" : "Desligado";
            else if (i == kFpsCap)       value = Game::FpsCapLabel();
            else                         value = Game::reduceFlashing ? "Ligado" : "Desligado";

            const SDL_Color arrowCol = sel ? kGold : SDL_Color{kDim.r, kDim.g, kDim.b, 160};
            const float lx = static_cast<float>(valueX + 8);
            const float rx = static_cast<float>(valueX + kValueW - 8);
            DrawSideArrow(r, lx, static_cast<float>(cy), true,  arrowCol);
            DrawSideArrow(r, rx, static_cast<float>(cy), false, arrowCol);
            leftArrowRects[i]  = SDL_Rect{valueX - 6, y, 30, kRowH};
            rightArrowRects[i] = SDL_Rect{valueX + kValueW - 24, y, 30, kRowH};
            DrawTextCentered(r, font.get(), value, valueX + kValueW / 2, textY, col);
        } else if (i == kCalibrate || i == kControls) {
            // Botão que abre outra tela: uma seta só, apontando para a direita.
            const SDL_Color arrowCol = sel ? kGold : SDL_Color{kDim.r, kDim.g, kDim.b, 160};
            DrawSideArrow(r, static_cast<float>(valueX + kValueW - 8), static_cast<float>(cy), false, arrowCol);
            DrawText(r, font.get(), "Abrir", valueX + kValueW - 80, textY, sel ? kGold : kDim);
        }

        y += kRowH + kRowGap;
    }

    // Rodapé: nota da linha selecionada + dicas com a arte das teclas.
    const int cx = panel.x + panel.w / 2;
    const char* note = nullptr;
    if (selection == kDisplayMode)      note = "O modo de tela é aplicado na hora.";
    else if (selection == kResolution) {
        note = (Game::resolutionIndex != Game::appliedResolutionIndex)
             ? "Enter — aplicar agora (o jogo reinicia; progresso não salvo é perdido)."
             : "Mude com A/D; para aplicar, o jogo precisa reiniciar.";
    }
    else if (selection == kVSync)       note = "Sincroniza com o monitor: evita cortes na imagem e poupa a máquina.";
    else if (selection == kFpsCap)      note = "Teto de quadros por segundo. Útil com o VSync desligado.";
    else if (selection == kCalibrate)   note = "Abre a tela de ajuste com os isqueiros.";
    if (note) DrawTextCentered(r, small.get(), note, cx, y + kNoteOffsetY, kDim);

    DrawKeyHint(r, hint.get(), "[W] [S] Navegar    [A] [D] Ajustar    [Return] Selecionar    [Escape] Voltar",
                cx, panel.y + panel.h - kHintBottomGap);
}
// Página principal: título, seções (Áudio/Imagem/Jogo), sliders dourados,
// opções com setas desenhadas, botões "Abrir", nota da linha e dicas com teclas.

void SettingsMenu::RenderControls(SDL_Renderer* r) {
    const int actions = InputManager::ActionCount;
    const int panelH = kTitleH + kControlsRowCount * (kCtrlRowH + kCtrlGap) + 20 + kFooterH;
    const SDL_Rect panel = DrawPanel(r, kCtrlPanelW, panelH);
    DrawTitle(r, panel, "Controles");

    auto font  = Resources::GetFont(kFontPath, 26);
    auto small = Resources::GetFont(kFontPath, 20);
    auto hint  = Resources::GetFont(kFontPath, 22);
    InputManager& in = InputManager::GetInstance();

    int y = panel.y + kTitleH;
    for (int i = 0; i < kControlsRowCount; ++i) {
        if (i == actions) y += 20;   // respiro antes de Restaurar/Voltar

        controlsRowRects[i] = SDL_Rect{panel.x + 24, y, panel.w - 48, kCtrlRowH};
        const bool sel = (i == controlsSelection);
        const SDL_Color col = sel ? kGold : kText;
        const int textY = y + (kCtrlRowH - 30) / 2;
        if (sel) DrawSelection(r, controlsRowRects[i]);

        if (i < actions) {
            const GameAction a = static_cast<GameAction>(i);
            DrawText(r, font.get(), InputManager::ActionLabel(a), panel.x + kLabelX, textY, col);

            // Etiqueta da tecla: caixinha com borda e o nome ESCRITO (legível em
            // qualquer resolução, sem depender da arte da tecla).
            const SDL_Rect chip{panel.x + kCtrlKeyX, y + 7, kCtrlKeyW, kCtrlRowH - 14};
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, 0, 0, 0, 90);
            SDL_RenderFillRect(r, &chip);
            SDL_SetRenderDrawColor(r, col.r, col.g, col.b, sel ? 200 : 90);
            SDL_RenderDrawRect(r, &chip);

            std::string value;
            if (awaitingRebind && sel) {
                value = "Pressione uma tecla";
            } else {
                const char* name = SDL_GetKeyName(in.GetBinding(a));
                value = (name && name[0]) ? name : "?";
            }
            int tw = 0, th = 0;
            TTF_SizeUTF8(small.get(), value.c_str(), &tw, &th);
            DrawText(r, small.get(), value, chip.x + (chip.w - tw) / 2, chip.y + (chip.h - th) / 2, col);
        } else {
            const char* label = (i == actions) ? "Restaurar padrão" : "Voltar";
            DrawText(r, font.get(), label, panel.x + kLabelX, textY, col);
        }
        y += kCtrlRowH + kCtrlGap;
    }

    const int cx = panel.x + panel.w / 2;
    if (rebindInvalidTimer > 0.0f) {
        DrawTextCentered(r, small.get(), "Essa tecla não pode ser usada.", cx, y + kNoteOffsetY, kWarn);
    }
    const std::string keys = awaitingRebind ? "Pressione a nova tecla    [Escape] Cancelar"
                                            : "[W] [S] Navegar    [Return] Trocar Tecla    [Escape] Voltar";
    DrawKeyHint(r, hint.get(), keys, cx, panel.y + panel.h - kHintBottomGap);
}
// Página Controles: lista única (ação à esquerda, tecla escrita numa etiqueta à
// direita), Restaurar padrão e Voltar, e dicas no rodapé.