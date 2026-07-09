#include "ui/VideoSettings.h"

#include "core/Game.h"
#include "core/InputManager.h"
#include "core/Resources.h"

#define INCLUDE_SDL_TTF
#include "SDL_include.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {

bool gOpen = false;
bool gDropdownOpen = false;
int  gDropScroll = 0;        // primeiro item visível quando o dropdown rola
bool gPromptOpen = false;    // modal "reiniciar agora/depois"
int  gPromptSel = 0;         // 0 = agora, 1 = depois
int  gFocus = 0;             // 0=resolucao 1=aplicar 2=voltar

constexpr int kMaxVisibleItems = 9;   // itens visíveis antes de rolar
constexpr int kFocusCount = 3;

// Retângulos para hit-test (recalculados no Render, usados no HandleInput).
SDL_Rect gResField{};
SDL_Rect gApplyBtn{}, gBackBtn{};
std::vector<SDL_Rect> gDropItemRects;
SDL_Rect gDropBox{};
SDL_Rect gPromptNow{}, gPromptLater{};

SDL_Texture* MakeText(SDL_Renderer* r, TTF_Font* font, const std::string& s, SDL_Color c,
                      int& outW, int& outH) {
    outW = outH = 0;
    if (!font || s.empty()) return nullptr;
    SDL_Surface* sf = TTF_RenderUTF8_Blended(font, s.c_str(), c);
    if (!sf) return nullptr;
    SDL_Texture* t = SDL_CreateTextureFromSurface(r, sf);
    outW = sf->w; outH = sf->h;
    SDL_FreeSurface(sf);
    return t;
}

// Desenha texto; alinhamento: 0=esquerda(x), 1=centro(x=centro), 2=direita(x=direita).
void DrawText(SDL_Renderer* r, TTF_Font* font, const std::string& s, int x, int y,
              SDL_Color c, int align = 0) {
    int w = 0, h = 0;
    SDL_Texture* t = MakeText(r, font, s, c, w, h);
    if (!t) return;
    int dx = x;
    if (align == 1) dx = x - w / 2;
    else if (align == 2) dx = x - w;
    const SDL_Rect d{dx, y, w, h};
    SDL_RenderCopy(r, t, nullptr, &d);
    SDL_DestroyTexture(t);
}

void FillRect(SDL_Renderer* r, const SDL_Rect& rc, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
    SDL_RenderFillRect(r, &rc);
}
void StrokeRect(SDL_Renderer* r, const SDL_Rect& rc, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
    SDL_RenderDrawRect(r, &rc);
}

void EnsureScrollVisible(int idx, int count) {
    const int visible = std::min(kMaxVisibleItems, count);
    if (idx < gDropScroll) gDropScroll = idx;
    else if (idx >= gDropScroll + visible) gDropScroll = idx - visible + 1;
    const int maxScroll = std::max(0, count - visible);
    gDropScroll = std::max(0, std::min(gDropScroll, maxScroll));
}

void DoRestartPrompt() {
    // Só faz sentido perguntar se houve mudança de vídeo.
    if (Game::VideoSettingsDirty()) {
        Game::ApplyVideoSettings();   // comita + salva antes de reiniciar
        gPromptOpen = true;
        gPromptSel = 0;
    }
}

}  // namespace

namespace VideoSettings {

void Open() {
    gOpen = true;
    gDropdownOpen = false;
    gPromptOpen = false;
    gFocus = 0;
    gDropScroll = 0;
    EnsureScrollVisible(Game::resolutionIndex, Game::ResolutionCount());
}

void Close() {
    // Descarta pré-visualizações não aplicadas ao sair.
    Game::RevertVideoSettings();
    gOpen = false;
    gDropdownOpen = false;
    gPromptOpen = false;
}

bool IsOpen() { return gOpen; }

void HandleInput(InputManager& input) {
    if (!gOpen) return;
    const int resCount = Game::ResolutionCount();
    SDL_Point mp{input.GetMouseX(), input.GetMouseY()};
    const bool click = input.MousePress(SDL_BUTTON_LEFT);

    // ── Modal de reinício tem prioridade ─────────────────────────────────────
    if (gPromptOpen) {
        if (input.KeyPress(SDLK_LEFT) || input.KeyPress(SDLK_a)) gPromptSel = 0;
        if (input.KeyPress(SDLK_RIGHT) || input.KeyPress(SDLK_d)) gPromptSel = 1;
        for (int i = 0; i < 2; ++i) {
            const SDL_Rect& rc = (i == 0) ? gPromptNow : gPromptLater;
            if (SDL_PointInRect(&mp, &rc)) gPromptSel = i;
        }
        const bool confirm = input.KeyPress(SDLK_RETURN) || input.KeyPress(SDLK_SPACE) ||
                             input.KeyPress(SDLK_f) ||
                             (click && (SDL_PointInRect(&mp, &gPromptNow) || SDL_PointInRect(&mp, &gPromptLater)));
        if (confirm) {
            const bool now = (click && SDL_PointInRect(&mp, &gPromptNow)) ||
                             (!click && gPromptSel == 0);
            if (now) {
                Game::RestartApplication();   // relança e encerra
            } else {
                gPromptOpen = false;          // reinicia depois: fecha o aviso
            }
        }
        if (input.KeyPress(SDLK_ESCAPE)) gPromptOpen = false;
        return;
    }

    // ── Dropdown de resolução aberto ─────────────────────────────────────────
    if (gDropdownOpen) {
        const int wheel = input.GetMouseWheel();
        if (wheel != 0) {
            gDropScroll -= wheel;
            const int visible = std::min(kMaxVisibleItems, resCount);
            gDropScroll = std::max(0, std::min(gDropScroll, std::max(0, resCount - visible)));
        }
        if (input.KeyPress(SDLK_UP) || input.KeyPress(SDLK_w)) {
            Game::SetResolutionIndex(std::max(0, Game::resolutionIndex - 1));
            EnsureScrollVisible(Game::resolutionIndex, resCount);
        }
        if (input.KeyPress(SDLK_DOWN) || input.KeyPress(SDLK_s)) {
            Game::SetResolutionIndex(std::min(resCount - 1, Game::resolutionIndex + 1));
            EnsureScrollVisible(Game::resolutionIndex, resCount);
        }
        if (input.KeyPress(SDLK_RETURN) || input.KeyPress(SDLK_SPACE) || input.KeyPress(SDLK_f)) {
            gDropdownOpen = false;
            return;
        }
        if (input.KeyPress(SDLK_ESCAPE)) { gDropdownOpen = false; return; }
        // Hover + clique nos itens.
        for (size_t i = 0; i < gDropItemRects.size(); ++i) {
            if (SDL_PointInRect(&mp, &gDropItemRects[i])) {
                const int idx = gDropScroll + static_cast<int>(i);
                if (click) {
                    Game::SetResolutionIndex(idx);
                    gDropdownOpen = false;
                    return;
                }
            }
        }
        // Clique fora do dropdown fecha-o.
        if (click && !SDL_PointInRect(&mp, &gDropBox) && !SDL_PointInRect(&mp, &gResField)) {
            gDropdownOpen = false;
        }
        return;
    }

    // ── Navegação normal do painel ───────────────────────────────────────────
    if (input.KeyPress(SDLK_UP) || input.KeyPress(SDLK_w)) gFocus = (gFocus + kFocusCount - 1) % kFocusCount;
    if (input.KeyPress(SDLK_DOWN) || input.KeyPress(SDLK_s)) gFocus = (gFocus + 1) % kFocusCount;

    // Hover do mouse define o foco.
    if (SDL_PointInRect(&mp, &gResField)) gFocus = 0;
    else if (SDL_PointInRect(&mp, &gApplyBtn)) gFocus = 1;
    else if (SDL_PointInRect(&mp, &gBackBtn)) gFocus = 2;

    // Ativação por teclado (Enter/Espaço/F).
    const bool activate = input.KeyPress(SDLK_RETURN) || input.KeyPress(SDLK_SPACE) || input.KeyPress(SDLK_f);
    if (activate) {
        if (gFocus == 0) { gDropdownOpen = true; EnsureScrollVisible(Game::resolutionIndex, resCount); }
        else if (gFocus == 1) DoRestartPrompt();
        else { Close(); return; }
    }

    // Cliques do mouse.
    if (click) {
        if (SDL_PointInRect(&mp, &gResField)) { gDropdownOpen = true; EnsureScrollVisible(Game::resolutionIndex, resCount); }
        else if (SDL_PointInRect(&mp, &gApplyBtn)) DoRestartPrompt();
        else if (SDL_PointInRect(&mp, &gBackBtn)) { Close(); return; }
    }

    if (input.KeyPress(SDLK_ESCAPE)) { Close(); return; }
}

void Render(SDL_Renderer* renderer) {
    if (!gOpen || !renderer) return;

    const int winW = Game::GetInstance().GetWindowsWidth();
    const int winH = Game::GetInstance().GetWindowsHeight();
    const int resCount = Game::ResolutionCount();

    // Mouse no ESPAÇO LÓGICO (o InputManager já converte) — para o realce de hover.
    SDL_Point lm{InputManager::GetInstance().GetMouseX(), InputManager::GetInstance().GetMouseY()};

    auto titleFont = Resources::GetFont("Recursos/font/times.ttf", 30);
    auto font = Resources::GetFont("Recursos/font/times.ttf", 20);
    auto small = Resources::GetFont("Recursos/font/times.ttf", 16);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    FillRect(renderer, {0, 0, winW, winH}, 0, 0, 0, 180);   // fundo escurecido

    const int panelW = 560;
    const int panelH = 280;
    const int px = (winW - panelW) / 2;
    const int py = (winH - panelH) / 2;
    FillRect(renderer, {px, py, panelW, panelH}, 28, 28, 34, 245);
    StrokeRect(renderer, {px, py, panelW, panelH}, 180, 160, 100, 255);

    DrawText(renderer, titleFont ? titleFont.get() : nullptr, "Video",
             px + panelW / 2, py + 22, SDL_Color{225, 205, 140, 255}, 1);

    const SDL_Color labelCol{205, 205, 205, 255};
    const SDL_Color selCol{245, 232, 200, 255};
    const int rowX = px + 34;
    const int valX = px + panelW - 34;   // valores alinhados à direita

    // ── Linha: Resolução (campo de dropdown) ─────────────────────────────────
    const int resY = py + 100;
    DrawText(renderer, font ? font.get() : nullptr, "Resolucao", rowX + 6, resY,
             gFocus == 0 ? selCol : labelCol);
    gResField = {valX - 300, resY - 6, 300, 34};
    FillRect(renderer, gResField, 44, 44, 52, 255);
    StrokeRect(renderer, gResField, gFocus == 0 ? 220 : 90, gFocus == 0 ? 190 : 90, gFocus == 0 ? 90 : 105, 255);
    DrawText(renderer, font ? font.get() : nullptr, Game::CurrentResolutionLabel(),
             gResField.x + 12, resY, selCol);
    DrawText(renderer, font ? font.get() : nullptr, gDropdownOpen ? "^" : "v",
             gResField.x + gResField.w - 20, resY, selCol);

    // ── Botões Aplicar / Voltar ──────────────────────────────────────────────
    const int btnY = py + panelH - 62;
    const int btnW = 200, btnH = 44;
    gApplyBtn = {px + panelW / 2 - btnW - 12, btnY, btnW, btnH};
    gBackBtn = {px + panelW / 2 + 12, btnY, btnW, btnH};
    const bool dirty = Game::VideoSettingsDirty();
    FillRect(renderer, gApplyBtn, gFocus == 1 ? 70 : 45, gFocus == 1 ? 90 : 60, gFocus == 1 ? 55 : 40, 255);
    StrokeRect(renderer, gApplyBtn, dirty ? 210 : 120, dirty ? 190 : 120, dirty ? 90 : 120, 255);
    DrawText(renderer, font ? font.get() : nullptr, "Aplicar", gApplyBtn.x + gApplyBtn.w / 2,
             btnY + 8, dirty ? selCol : SDL_Color{150, 150, 150, 255}, 1);
    FillRect(renderer, gBackBtn, gFocus == 2 ? 70 : 45, gFocus == 2 ? 60 : 45, gFocus == 2 ? 40 : 50, 255);
    StrokeRect(renderer, gBackBtn, 150, 140, 110, 255);
    DrawText(renderer, font ? font.get() : nullptr, "Voltar", gBackBtn.x + gBackBtn.w / 2,
             btnY + 8, gFocus == 2 ? selCol : labelCol, 1);

    DrawText(renderer, small ? small.get() : nullptr,
             "A resolucao aplica ao reiniciar o jogo.",
             px + panelW / 2, py + 150, SDL_Color{200, 175, 110, 220}, 1);

    // ── Dropdown expandido (por cima de tudo) ────────────────────────────────
    gDropItemRects.clear();
    if (gDropdownOpen) {
        const int itemH = 30;
        const int visible = std::min(kMaxVisibleItems, resCount);
        gDropBox = {gResField.x, gResField.y + gResField.h + 2, gResField.w, visible * itemH + 4};
        FillRect(renderer, gDropBox, 22, 22, 28, 252);
        StrokeRect(renderer, gDropBox, 200, 175, 100, 255);
        for (int i = 0; i < visible; ++i) {
            const int idx = gDropScroll + i;
            if (idx >= resCount) break;
            SDL_Rect item{gDropBox.x + 2, gDropBox.y + 2 + i * itemH, gDropBox.w - 4, itemH};
            gDropItemRects.push_back(item);
            const bool isSel = (idx == Game::resolutionIndex);
            const bool hover = SDL_PointInRect(&lm, &item);
            if (isSel) FillRect(renderer, item, 45, 70, 95, 255);        // azul (selecionado)
            else if (hover) FillRect(renderer, item, 55, 55, 64, 255);
            DrawText(renderer, small ? small.get() : nullptr, Game::ResolutionLabelAt(idx),
                     item.x + 12, item.y + 5, isSel ? SDL_Color{235, 240, 255, 255} : labelCol);
        }
        // Indicador de rolagem.
        if (resCount > visible) {
            DrawText(renderer, small ? small.get() : nullptr,
                     std::to_string(Game::resolutionIndex + 1) + "/" + std::to_string(resCount),
                     gDropBox.x + gDropBox.w - 8, gDropBox.y + gDropBox.h - 22,
                     SDL_Color{150, 150, 160, 200}, 2);
        }
    }

    // ── Modal "reiniciar agora/depois" (por cima do dropdown) ────────────────
    if (gPromptOpen) {
        FillRect(renderer, {0, 0, winW, winH}, 0, 0, 0, 150);
        const int mw = 520, mh = 190;
        const int mx = (winW - mw) / 2, my = (winH - mh) / 2;
        FillRect(renderer, {mx, my, mw, mh}, 32, 32, 40, 250);
        StrokeRect(renderer, {mx, my, mw, mh}, 200, 175, 100, 255);
        DrawText(renderer, font ? font.get() : nullptr,
                 "As mudancas de video serao aplicadas.", mx + mw / 2, my + 30, selCol, 1);
        DrawText(renderer, small ? small.get() : nullptr,
                 "E preciso reiniciar o jogo para valerem.", mx + mw / 2, my + 62,
                 SDL_Color{200, 200, 200, 230}, 1);
        const int pbW = 200, pbH = 46;
        gPromptNow = {mx + mw / 2 - pbW - 10, my + mh - 64, pbW, pbH};
        gPromptLater = {mx + mw / 2 + 10, my + mh - 64, pbW, pbH};
        FillRect(renderer, gPromptNow, gPromptSel == 0 ? 70 : 45, gPromptSel == 0 ? 90 : 60, gPromptSel == 0 ? 55 : 40, 255);
        StrokeRect(renderer, gPromptNow, 200, 180, 100, 255);
        DrawText(renderer, font ? font.get() : nullptr, "Reiniciar agora",
                 gPromptNow.x + gPromptNow.w / 2, my + mh - 54, selCol, 1);
        FillRect(renderer, gPromptLater, gPromptSel == 1 ? 70 : 45, gPromptSel == 1 ? 60 : 45, gPromptSel == 1 ? 40 : 50, 255);
        StrokeRect(renderer, gPromptLater, 150, 140, 110, 255);
        DrawText(renderer, font ? font.get() : nullptr, "Reiniciar depois",
                 gPromptLater.x + gPromptLater.w / 2, my + mh - 54, selCol, 1);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

}  // namespace VideoSettings
