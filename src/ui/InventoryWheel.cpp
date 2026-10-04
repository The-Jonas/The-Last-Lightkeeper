#include "ui/InventoryWheel.h"
#include "core/Game.h"
#include "core/Resources.h"
#include "core/InputManager.h"
#include "ui/HorrorFx.h"
#include "ui/KeyGlyphs.h"

#define INCLUDE_SDL_TTF
#include "SDL_include.h"
#include "states/stage/StageState.h"

#include <algorithm>
#include <cstdio>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
// Encaixa o ícone dentro de uma caixa quadrada preservando a proporção original
// da textura (evita a distorção de "amassado" quando o PNG não é quadrado).
SDL_FRect FitIconRect(SDL_Texture* tex, float boxX, float boxY, float boxW, float boxH) {
    SDL_FRect dst{boxX, boxY, boxW, boxH};
    int tw = 0, th = 0;
    if (tex && SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th) == 0 && tw > 0 && th > 0) {
        const float ar = static_cast<float>(tw) / static_cast<float>(th);
        float w = boxW;
        float h = boxH;
        if (ar >= 1.0f) {
            h = boxW / ar;
        } else {
            w = boxH * ar;
        }
        dst.x = boxX + (boxW - w) * 0.5f;
        dst.y = boxY + (boxH - h) * 0.5f;
        dst.w = w;
        dst.h = h;
    }
    return dst;
}

// Disco "cheio de líquido": preenche o círculo de baixo até `ratio` da altura,
// uma linha de pixel por vez, com uma linha mais clara na superfície.
void DrawFuelFill(SDL_Renderer* r, float cx, float cy, float radius, float ratio,
                  SDL_Color body, SDL_Color surface) {
    if (radius <= 0.0f || ratio <= 0.0f) return;
    const float levelY = cy + radius - 2.0f * radius * std::min(1.0f, ratio);
    const int rad = static_cast<int>(radius);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    bool first = true;
    for (int dy = -rad; dy <= rad; ++dy) {
        const float y = cy + static_cast<float>(dy);
        if (y < levelY) continue;
        const float dxf = std::sqrt(std::max(0.0f, radius * radius - static_cast<float>(dy) * dy));
        const SDL_Color& c = first ? surface : body;
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
        const SDL_FRect line{cx - dxf, y, dxf * 2.0f, first ? 2.0f : 1.0f};
        SDL_RenderFillRectF(r, &line);
        first = false;
    }
}

} // namespace

InventoryWheel::InventoryWheel(GameObject& associated, Inventory& inventory)
    : Component(associated), inventory(inventory) {
}

void InventoryWheel::Start() {
    lastKnownActive = inventory.GetActiveIndex();
    displayActiveIndex = static_cast<float>(lastKnownActive >= 0 ? lastKnownActive : 0);
}

// Atualiza a roda: fade das dicas de tecla e o deslizar suave até o item ativo.
void InventoryWheel::Update(float dt) {
    int currentActive = inventory.GetActiveIndex();

    // As teclas da roda só aparecem quando uma dica do tutorial pede (ex.: girar
    // até o combustível). Fade suave nos dois sentidos.
    StageState* stage = Game::TryGetStageState();
    const float hintTarget = (stage && stage->Hints().WantsHudKeys(HudSlot::Wheel)) ? 1.0f : 0.0f;
    const float hintStep = dt / kKeyHintFadeDuration;
    if (keyHintAlpha < hintTarget) keyHintAlpha = std::min(hintTarget, keyHintAlpha + hintStep);
    else                           keyHintAlpha = std::max(hintTarget, keyHintAlpha - hintStep);
    
    // Tecla [Tab] da pasta: enquanto houver documento novo — menos quando a dica
    // da pasta já desenha "[Tab] Documentos" ao lado do slot.
    const float folderTarget = (stage && stage->HasUnreadDocuments() &&
                                !stage->Hints().HasGlyphAt(HudSlot::Folder)) ? 1.0f : 0.0f;
    if (folderHintAlpha < folderTarget) folderHintAlpha = std::min(folderTarget, folderHintAlpha + hintStep);
    else                                folderHintAlpha = std::max(folderTarget, folderHintAlpha - hintStep);

    float target = static_cast<float>(currentActive);
    float diff = target - displayActiveIndex;
    float step = kSlideSpeed * dt;

    if (std::abs(diff) > step) {
        displayActiveIndex += (diff > 0 ? step : -step);
    } else {
        displayActiveIndex = target;
    }

    lastKnownActive = currentActive;
    bobTimer += dt;
}

float InventoryWheel::GetAnchorX() const {
    return static_cast<float>(Game::GetInstance().GetWindowsWidth()) * kAnchorXFraction
           + kSlotSize * 0.5f * Game::UiScale();
}

float InventoryWheel::GetAnchorY() const {
    return static_cast<float>(Game::GetInstance().GetWindowsHeight()) * kAnchorYFraction;
}

void InventoryWheel::GetSlotScreenPos(int visibleOffset, int visibleCount, float scrollOffset,
                                       float& outX, float& outY) const {
    int half = visibleCount / 2;
    float baseOffset = static_cast<float>(visibleOffset - half);
    float shiftedOffset = baseOffset + scrollOffset;

    float angleStep = (visibleCount <= 3) ? 46.0f : 34.0f;
    float angleDeg = shiftedOffset * angleStep;
    float angleRad = angleDeg * static_cast<float>(M_PI) / 180.0f;

    const float arcRadius = kArcRadius * Game::UiScale();
    const float activeX = GetAnchorX();
    const float activeY = GetAnchorY();
    const float ccx = activeX - arcRadius;
    const float ccy = activeY;

    outX = ccx + arcRadius * std::cos(angleRad);
    outY = ccy + arcRadius * std::sin(angleRad);

    // Ajuste fino manual só do slot de cima/baixo — cresce suavemente conforme
    // ele se afasta do centro (shiftedOffset vai de 0 no ativo até ±1 na posição
    // de descanso), assim não quebra a transição deslizante que já está boa.
    const float u = Game::UiScale();
    if (shiftedOffset < 0.0f) {
        float t = std::min(1.0f, -shiftedOffset);
        outX += kTopSlotOffsetX * t * u;
        outY += kTopSlotOffsetY * t * u;
    } else if (shiftedOffset > 0.0f) {
        float t = std::min(1.0f, shiftedOffset);
        outX += kBottomSlotOffsetX * t * u;
        outY += kBottomSlotOffsetY * t * u;
    }
}

float InventoryWheel::GetSlotAlpha(int distanceFromCenter, int) const {
    if (distanceFromCenter == 0) return 255.0f;
    if (distanceFromCenter == 1) return 130.0f;
    return 50.0f;
}

float InventoryWheel::GetSlotScale(int distanceFromCenter, int) const {
    if (distanceFromCenter == 0) return 1.0f;
    if (distanceFromCenter == 1) return 0.75f;
    return 0.55f;
}

void InventoryWheel::DrawSlot(SDL_Renderer* renderer, int stackIndex, float x, float y,
                               float alpha, float scale, bool isActive) const {
    scale *= Game::UiScale();
    const float scaledSize = kSlotSize * scale;
    const float halfSize = scaledSize * 0.5f;
    const float slotX = x - halfSize;
    const float slotY = y - halfSize;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    
    
    // Combustível: o fundo do slot "enche" de baixo para cima na proporção do que
    // tem no frasco, ATRÁS da moldura (que cobre a borda). Luzes não têm: a carga
    // delas só aparece (vaga) na chama da HUD.
    if (const Inventory::ItemStack* fuel = (stackIndex >= 0) ? inventory.GetStack(stackIndex) : nullptr) {
        if (fuel->def.HasProperty(ItemProperty::FUEL) && fuel->def.maxDurability > 0 && !fuel->durabilities.empty()) {
            const float ratio = std::max(0.0f, std::min(1.0f, static_cast<float>(fuel->durabilities.front()) /
                                                              static_cast<float>(fuel->def.maxDurability)));
            const bool low = ratio < kChargeLowRatio;
            const float a = alpha * (low ? 0.75f + 0.25f * std::sin(bobTimer * 6.0f) : 1.0f);
            const SDL_Color body    = low ? SDL_Color{150, 30, 20, static_cast<Uint8>(a * kFuelFillAlpha)}
                                          : SDL_Color{190, 125, 45, static_cast<Uint8>(a * kFuelFillAlpha)};
            const SDL_Color surface = low ? SDL_Color{220, 70, 50, static_cast<Uint8>(a * kFuelSurfaceAlpha)}
                                          : SDL_Color{245, 190, 100, static_cast<Uint8>(a * kFuelSurfaceAlpha)};
            DrawFuelFill(renderer, x, y, scaledSize * kFuelFillFrac, ratio, body, surface);
        }
    }

    auto frameTex = Resources::GetImage(isActive
    ? "Recursos/img/ui/hud_items/bola_maior.png"
    : "Recursos/img/ui/hud_items/bola_menor.png");
    if (frameTex) {
        const SDL_FRect frameDst{slotX, slotY, scaledSize, scaledSize};
        SDL_SetTextureAlphaMod(frameTex.get(), static_cast<Uint8>(std::min(255.0f, alpha)));
        SDL_RenderCopyF(renderer, frameTex.get(), nullptr, &frameDst);
    }

    if (stackIndex < 0) return;

    const Inventory::ItemStack* stack = inventory.GetStack(stackIndex);
    if (!stack) return;

    const float iconSize = kIconSize * scale;
    const float iconX = slotX + (scaledSize - iconSize) * 0.5f;
    const float iconY = slotY + (scaledSize - iconSize) * 0.5f - 4.0f * scale;

    auto tex = Resources::GetImage(stack->def.spritePath);
    if (tex) {
        const SDL_FRect dst = FitIconRect(tex.get(), iconX, iconY, iconSize, iconSize);
        SDL_SetTextureAlphaMod(tex.get(), static_cast<Uint8>(std::min(255.0f, alpha)));
        SDL_SetTextureColorMod(tex.get(), 255, 255, 255);
        SDL_RenderCopyExF(renderer, tex.get(), nullptr, &dst, 0.0, nullptr, SDL_FLIP_NONE);
    }
}

// Desenha o slot fixo da pasta de documentos ao lado da roda: moldura, ícone,
// ponto vermelho quando há documento novo e a tecla [Tab] (com fade).
void InventoryWheel::DrawDocumentFolderSlot(SDL_Renderer* renderer) {
    StageState* stage = Game::TryGetStageState();
    if (!stage) return;

    const float u    = Game::UiScale();
    const float size = kFolderSlotSize * u;
    const float winH = static_cast<float>(Game::GetInstance().GetWindowsHeight());
    const float cx   = kFolderMarginX * u + size * 0.5f;
    const float cy   = winH - kFolderMarginY * u - size * 0.5f;

    stage->Hints().ReportHudRect(HudSlot::Folder, SDL_FRect{cx - size * 0.5f, cy - size * 0.5f, size, size},
                                 "Recursos/img/ui/hud_items/bola_maior.png");
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Moldura redonda — mesma arte do slot ATIVO da roda.
    if (auto frame = Resources::GetImage("Recursos/img/ui/hud_items/bola_maior.png")) {
        const SDL_FRect dst{cx - size * 0.5f, cy - size * 0.5f, size, size};
        SDL_SetTextureAlphaMod(frame.get(), 255);
        SDL_RenderCopyF(renderer, frame.get(), nullptr, &dst);
    }

    // Ícone da pasta.
    if (auto icon = Resources::GetImage("Recursos/img/items/pasta_documentos.png")) {
        const float iconSize = size * 1.0f;
        const SDL_FRect dst = FitIconRect(icon.get(), cx - iconSize * 0.5f, cy - iconSize * 0.5f,
                                          iconSize, iconSize);
        SDL_SetTextureAlphaMod(icon.get(), 255);
        SDL_SetTextureColorMod(icon.get(), 255, 255, 255);
        SDL_RenderCopyF(renderer, icon.get(), nullptr, &dst);
    }

    // Ponto vermelho pulsando quando há documento não lido.
    if (stage->HasUnreadDocuments()) {
        const float pulse = 0.7f + 0.3f * std::sin(bobTimer * 4.0f);
        const float r   = 8.0f * u;
        const float dcx = cx + size * 0.32f;
        const float dcy = cy - size * 0.32f;
        SDL_SetRenderDrawColor(renderer, 200, 40, 30, static_cast<Uint8>(255.0f * pulse));
        for (int dy = static_cast<int>(-r); dy <= static_cast<int>(r); ++dy) {
            const float half = std::sqrt(std::max(0.0f, r * r - dy * dy));
            const SDL_FRect row{dcx - half, dcy + dy, half * 2.0f, 1.0f};
            SDL_RenderFillRectF(renderer, &row);
        }
    }

    // Tecla [Tab] EM CIMA do slot (arte via KeyGlyphs).
    if (folderHintAlpha > 0.01f) {
        auto font = Resources::GetFont("Recursos/font/times.ttf",
                                       std::max(13, static_cast<int>(std::lround(20.0f * u))));
        if (font) {
            const std::string text = "[Tab]";
            int tw = 0, th = 0;
            KeyGlyphs::Measure(font.get(), text, KeyGlyphs::kDefaultKeyScale, tw, th);
            KeyGlyphs::Draw(renderer, font.get(), text,
                            static_cast<int>(cx - tw * 0.5f),
                            static_cast<int>(cy - size * 0.5f - 6.0f * u - th),
                            SDL_Color{235, 225, 195, 255},
                            static_cast<Uint8>(255.0f * folderHintAlpha),
                            KeyGlyphs::kDefaultKeyScale);
        }
    }
}

void InventoryWheel::DrawCycleKeyHints(SDL_Renderer* renderer, float fadeAlpha) {
    // As teclas ficam SEMPRE visíveis junto da roda (mesmo com um item só).
    const int visibleCount = inventory.GetVisibleSlotCount();

    // Roda VERTICAL: i=0 é o slot de CIMA; i=visibleCount-1 é o de BAIXO
    // (ver GetSlotScreenPos). CyclePrev (↑) traz o item de cima; CycleNext (↓) o de baixo.
    float tx, ty, bx, by;
    GetSlotScreenPos(0, visibleCount, 0.0f, tx, ty);                 // slot do topo
    GetSlotScreenPos(visibleCount - 1, visibleCount, 0.0f, bx, by);  // slot de baixo

    // Meia-altura do slot das pontas (para colar as caixas acima/abaixo dele).
    const float u = Game::UiScale();
    const int half = visibleCount / 2;
    const float edgeSlotHalf = kSlotSize * GetSlotScale(half, visibleCount) * 0.5f * u;
    const float slotGap = 10.0f * u;

    // Rótulo da ação (não o nome da tecla): a seta já mostra qual é.
    const std::string prevLabel = "Anterior";
    const std::string nextLabel = "Próximo";

    // Imagem da tecla-seta (mesma p/ os dois; a da ESQUERDA é espelhada) + rótulo
    // pequeno logo ABAIXO da imagem.
    auto keyTex = Resources::GetImage("Recursos/img/hud/key_arrow.png");
    auto font   = Resources::GetFont("Recursos/font/times.ttf",
                                     std::max(13, static_cast<int>(std::lround(20.0f * u))));
    const Uint8 a = static_cast<Uint8>(255.0f * fadeAlpha);

    // dir = -1 (acima do slot de cima) / +1 (abaixo do de baixo).
    // leftArrow=true → espelha a imagem (seta apontando p/ a esquerda).
    auto drawKeyHint = [&](float slotCX, float slotCY, float dir, bool leftArrow, const std::string& label) {
        // Tremem como as dicas do tutorial (cada seta no seu ritmo), para chamar o olho.
        const float seed = leftArrow ? 0.21f : 0.63f;
        slotCX += HorrorFx::Jitter(bobTimer, seed) * 1.5f * u;
        slotCY += HorrorFx::Jitter(bobTimer, seed + 0.4f) * 1.5f * u;
        const float imgSize = 47.0f * u * 1.5f;   // 30→39 (+30%) →47 (+20%)
        const float textGap = 1.0f * u * 1.5f;

        SDL_Texture* txt = nullptr; int tw = 0, th = 0;
        if (font) {
            SDL_Color col{235, 225, 195, a};
            if (SDL_Surface* sf = TTF_RenderUTF8_Blended(font.get(), label.c_str(), col)) {
                txt = SDL_CreateTextureFromSurface(renderer, sf);
                tw = sf->w; th = sf->h;
                SDL_FreeSurface(sf);
            }
        }
        const float totalH = imgSize + (txt ? textGap + th : 0.0f);
        const float edge = slotCY + dir * edgeSlotHalf;
        const float top  = (dir < 0.0f) ? (edge - slotGap - totalH) : (edge + slotGap);

        if (keyTex) {
            SDL_SetTextureAlphaMod(keyTex.get(), a);
            const SDL_FRect imgDst{ slotCX - imgSize * 0.5f, top, imgSize, imgSize };
            SDL_RenderCopyExF(renderer, keyTex.get(), nullptr, &imgDst, 0.0, nullptr,
                              leftArrow ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
        }
        if (txt) {
            const SDL_FRect td{ slotCX - tw * 0.5f, top + imgSize + textGap,
                                static_cast<float>(tw), static_cast<float>(th) };
            SDL_RenderCopyF(renderer, txt, nullptr, &td);
            SDL_DestroyTexture(txt);
        }
    };

    drawKeyHint(tx, ty, -1.0f, true,  prevLabel);   // topo (item anterior) = seta ESQUERDA
    drawKeyHint(bx, by, +1.0f, false, nextLabel);   // baixo (próximo item) = seta DIREITA
}

void InventoryWheel::Render() {

    if (StageState* stage = Game::TryGetStageState()) {
        if (stage->GetControlledCharacter() != stage->GetBigCharacterComponent()) {
            return;
        }
    }

    SDL_Renderer* renderer = Game::GetInstance().GetRenderer();
    if (!renderer) return;

    int count = inventory.GetStackCount();
    // Quantos slots a roda mostra — decidido em `Inventory::GetVisibleSlotCount`,
    // que e a MESMA funcao que o `GetRingSize` usa para saber qual e o item na
    // mao. Recalcular aqui por fora foi o que fez os dois lados discordarem.
    int visibleCount = inventory.GetVisibleSlotCount();
    // The circular ring spans every logical position: when there are fewer
    // items than slots the ring matches the slot count (so empty slots appear);
    // once there are more items than slots the extra ones live off-screen and
    // are revealed by cycling.
    int ringSize = std::max(count, visibleCount);

    float scrollOffset = displayActiveIndex - std::round(displayActiveIndex);

    float activeSlotX = 0.0f;
    float activeSlotY = 0.0f;

    for (int i = 0; i < visibleCount; ++i) {
        int half = visibleCount / 2;
        int offsetFromCenter = i - half;

        float slotX, slotY;
        GetSlotScreenPos(i, visibleCount, scrollOffset, slotX, slotY);

        if (offsetFromCenter == 0) {
            activeSlotX = slotX;
            activeSlotY = slotY;
        }

        int stackIndex = -1;
        if (count > 0) {
            int center = visibleCount / 2;
            int idx = i - static_cast<int>(std::round(displayActiveIndex)) - center;
            idx = ((idx % ringSize) + ringSize) % ringSize;
            if (idx < count) {
                stackIndex = idx;
            }
        }

        int distFromCenter = std::abs(offsetFromCenter);
        float alpha = GetSlotAlpha(distFromCenter, visibleCount);
        float scale = GetSlotScale(distFromCenter, visibleCount);
        bool isActive = (offsetFromCenter == 0);
        DrawSlot(renderer, stackIndex, slotX, slotY, alpha, scale, isActive);
    }

    if (StageState* stage = Game::TryGetStageState()) {
        // A aura do tutorial usa o slot do item na mão (silhueta da moldura).
        const float half = kSlotSize * 0.5f * Game::UiScale();
        stage->Hints().ReportHudRect(HudSlot::Wheel,
                                     SDL_FRect{activeSlotX - half, activeSlotY - half, 2 * half, 2 * half},
                                     "Recursos/img/ui/hud_items/bola_maior.png");
    }
    DrawDocumentFolderSlot(renderer);

    if (keyHintAlpha > 0.01f) {
        DrawUseHint(renderer, activeSlotX, activeSlotY, keyHintAlpha);
        DrawCycleKeyHints(renderer, keyHintAlpha);
    }
}

// "[F] Usar" logo ao lado do slot ativo quando o item tem uso
// (isqueiro/lâmpada = LIGHT_SOURCE, ou óleo = FUEL).
void InventoryWheel::DrawUseHint(SDL_Renderer* renderer, float activeX, float activeY, float fadeAlpha) {
    const Inventory::ItemStack* active = inventory.GetActiveStack();
    if (!active) return;
    // Só luz se "usa" com [F]; combustível vai pela recarga ([R]).
    if (!active->def.HasProperty(ItemProperty::LIGHT_SOURCE)) return;

    const Uint8 a = static_cast<Uint8>(255.0f * fadeAlpha);
    const float u = Game::UiScale();
    auto keyTex = Resources::GetImage("Recursos/img/hud/key_f.png");
    auto font = Resources::GetFont("Recursos/font/times.ttf",
                                   std::max(13, static_cast<int>(std::lround(20.0f * u))));
    const float imgSize = 47.0f * u * 1.5f;   // 30→39 (+30%) →47 (+20%)
    const float textGap = 1.0f * u * 1.5f;

    SDL_Texture* txt = nullptr; int tw = 0, th = 0;
    if (font) {
        SDL_Color col{235, 225, 195, a};
        if (SDL_Surface* s = TTF_RenderUTF8_Blended(font.get(), "Usar", col)) {
            txt = SDL_CreateTextureFromSurface(renderer, s);
            tw = s->w; th = s->h;
            SDL_FreeSurface(s);
        }
    }

    const float totalH = imgSize + (txt ? textGap + th : 0.0f);
    // À direita do slot ativo (lado aberto da roda), bloco centralizado na vertical.
    const float gap = 40.0f * u;
    const float cx = activeX + kSlotSize * 0.5f * u + gap + imgSize * 0.5f;
    const float top = activeY - totalH * 0.5f;

    if (keyTex) {
        SDL_SetTextureAlphaMod(keyTex.get(), a);
        const SDL_FRect imgDst{ cx - imgSize * 0.5f, top, imgSize, imgSize };
        SDL_RenderCopyExF(renderer, keyTex.get(), nullptr, &imgDst, 0.0, nullptr, SDL_FLIP_NONE);
    }
    if (txt) {
        const SDL_FRect td{ cx - tw * 0.5f, top + imgSize + textGap,
                            static_cast<float>(tw), static_cast<float>(th) };
        SDL_RenderCopyF(renderer, txt, nullptr, &td);
        SDL_DestroyTexture(txt);
    }
}


