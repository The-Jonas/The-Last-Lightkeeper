#include "ui/InventoryWheel.h"
#include "core/Game.h"
#include "core/Resources.h"
#include "core/InputManager.h"

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

// #7 SDL não tem círculo nativo. Preenche um disco por varredura de linhas
// horizontais (uma por linha de pixel), respeitando o alpha.
void FillCircle(SDL_Renderer* r, float cx, float cy, float radius,
                Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca) {
    if (radius <= 0.0f) return;
    SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
    const int rad = static_cast<int>(radius);
    for (int dy = -rad; dy <= rad; ++dy) {
        const float dxf = std::sqrt(std::max(0.0f, radius * radius - static_cast<float>(dy) * dy));
        const SDL_FRect line{cx - dxf, cy + static_cast<float>(dy), dxf * 2.0f, 1.0f};
        SDL_RenderFillRectF(r, &line);
    }
}

// Contorno de círculo (borda do slot).
void StrokeCircle(SDL_Renderer* r, float cx, float cy, float radius,
                  Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca) {
    if (radius <= 0.0f) return;
    SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
    const int segments = std::max(28, static_cast<int>(radius * 3.0f));
    float px = cx + radius, py = cy;
    for (int i = 1; i <= segments; ++i) {
        const float t = 2.0f * static_cast<float>(M_PI) * static_cast<float>(i) / segments;
        const float nx = cx + radius * std::cos(t);
        const float ny = cy + radius * std::sin(t);
        SDL_RenderDrawLineF(r, px, py, nx, ny);
        px = nx; py = ny;
    }
}

// Anel de durabilidade "bonito e consistente": um TRACK escuro completo (sempre
// círculo inteiro) + um arco colorido proporcional, começando no topo e drenando
// no sentido horário, com PONTAS ARREDONDADAS. Linhas radiais densas p/ suavidade.
void DrawDurabilityRing(SDL_Renderer* ren, float cx, float cy, float rOuter, float thickness,
                        float ratio, Uint8 fr, Uint8 fg, Uint8 fb, Uint8 alpha) {
    ratio = std::max(0.0f, std::min(1.0f, ratio));
    const float rInner = std::max(0.0f, rOuter - thickness);
    const float rMid   = (rOuter + rInner) * 0.5f;
    const float twoPi  = 2.0f * static_cast<float>(M_PI);
    const int   segments = std::max(200, static_cast<int>(rOuter * 9.0f));
    const float start  = -static_cast<float>(M_PI) * 0.5f;   // topo (12h)
    const int   filled = static_cast<int>(std::ceil(segments * ratio));
    const Uint8 trackA = static_cast<Uint8>(std::min(255.0f, alpha * 0.45f));

    // 1) Track completo (fundo) — desenhado inteiro para consistência visual.
    SDL_SetRenderDrawColor(ren, 52, 52, 60, trackA);
    for (int i = 0; i < segments; ++i) {
        const float t = start + twoPi * (static_cast<float>(i) + 0.5f) / segments;
        const float cs = std::cos(t), sn = std::sin(t);
        SDL_RenderDrawLineF(ren, cx + rInner * cs, cy + rInner * sn, cx + rOuter * cs, cy + rOuter * sn);
    }

    // 2) Arco preenchido (colorido) por cima.
    SDL_SetRenderDrawColor(ren, fr, fg, fb, alpha);
    for (int i = 0; i < filled; ++i) {
        const float t = start + twoPi * (static_cast<float>(i) + 0.5f) / segments;
        const float cs = std::cos(t), sn = std::sin(t);
        SDL_RenderDrawLineF(ren, cx + rInner * cs, cy + rInner * sn, cx + rOuter * cs, cy + rOuter * sn);
    }

    // 3) Pontas ARREDONDADAS do arco (início no topo + fim proporcional). Quando
    // CHEIO (ratio≈1) o anel já é um círculo completo — não desenha as pontas,
    // senão o cap do início e o do fim se sobrepõem num "calombo" no topo.
    if (filled > 0 && ratio < 0.999f) {
        const float capR = thickness * 0.5f;
        FillCircle(ren, cx + rMid * std::cos(start), cy + rMid * std::sin(start), capR, fr, fg, fb, alpha);
        const float endT = start + twoPi * ratio;
        FillCircle(ren, cx + rMid * std::cos(endT), cy + rMid * std::sin(endT), capR, fr, fg, fb, alpha);
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

void InventoryWheel::Update(float dt) {
    int currentActive = inventory.GetActiveIndex();

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
    return static_cast<float>(Game::GetInstance().GetWindowsWidth()) * kAnchorXFraction + kSlotSize * 0.5f;
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
    // Roda VERTICAL: 0° = slot ATIVO no vértice DIREITO do arco; offsets negativos
    // sobem, positivos descem. O arco abre para a ESQUERDA (centro à esquerda do
    // ativo), então os vizinhos ficam acima-esquerda e abaixo-esquerda.
    float angleDeg = shiftedOffset * angleStep;
    float angleRad = angleDeg * static_cast<float>(M_PI) / 180.0f;

    const float activeX = GetAnchorX();
    const float activeY = GetAnchorY();
    const float ccx = activeX - kArcRadius;   // centro do arco à esquerda do ativo
    const float ccy = activeY;

    outX = ccx + kArcRadius * std::cos(angleRad);
    outY = ccy + kArcRadius * std::sin(angleRad);
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
    const float scaledSize = kSlotSize * scale;
    const float halfSize = scaledSize * 0.5f;
    const float slotX = x - halfSize;
    const float slotY = y - halfSize;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    const float slotRadius = halfSize;

    // Brilho suave atrás do slot ATIVO (halo quente) — dá destaque sem poluir.
    if (isActive) {
        const Uint8 glowA = static_cast<Uint8>(std::min(255.0f, alpha * 0.16f));
        FillCircle(renderer, x, y, slotRadius * 1.35f, 235, 200, 110, glowA);
        FillCircle(renderer, x, y, slotRadius * 1.18f, 235, 200, 110, glowA);
    }

    // Fundo do slot: disco circular escuro.
    FillCircle(renderer, x, y, slotRadius, 26, 26, 33,
               static_cast<Uint8>(std::min(255.0f, alpha * 0.88f)));

    Uint8 borderR, borderG, borderB;
    if (isActive && inventory.IsOilPrimed()) {
        borderR = 230; borderG = 190; borderB = 40;
    } else if (isActive) {
        borderR = 210; borderG = 190; borderB = 80;
    } else {
        borderR = 60; borderG = 60; borderB = 75;
    }
    Uint8 borderA = static_cast<Uint8>(std::min(255.0f, alpha));
    StrokeCircle(renderer, x, y, slotRadius, borderR, borderG, borderB, borderA);

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

    bool showGauge = stack->def.HasProperty(ItemProperty::LIGHT_SOURCE) && stack->def.maxDurability > 0;
    bool isFuel = stack->def.HasProperty(ItemProperty::FUEL);

    if (showGauge || isFuel) {
        int durability = stack->durabilities.empty() ? 0 : stack->durabilities.front();
        float ratio = 0.0f;
        if (stack->def.maxDurability > 0) {
            ratio = static_cast<float>(durability) / static_cast<float>(stack->def.maxDurability);
        } else if (isFuel && durability > 0) {
            ratio = 1.0f;
        }
        ratio = std::max(0.0f, std::min(1.0f, ratio));

        // #7 Durabilidade agora é um ANEL radial ao redor do slot (verde/âmbar/
        // vermelho), drenando no sentido horário a partir do topo.
        Uint8 gR, gG, gB;
        if (ratio > 0.6f) { gR = 70; gG = 180; gB = 80; }
        else if (ratio > 0.25f) { gR = 210; gG = 170; gB = 50; }
        else { gR = 200; gG = 55; gB = 55; }

        const float ringOuter = slotRadius + kRingThickness * scale + 2.0f * scale;
        DrawDurabilityRing(renderer, x, y, ringOuter, kRingThickness * scale, ratio,
                           gR, gG, gB, static_cast<Uint8>(std::min(255.0f, alpha * 0.95f)));
    }
}

void InventoryWheel::DrawPrimedOil(SDL_Renderer* renderer, float activeX, float activeY) {
    if (!inventory.IsOilPrimed()) return;

    // When the centered slot can't take the oil (empty slot or a non-light /
    // already-full item), keep the floating icon faded so the player sees it.
    // Quando PODE recarregar, o ícone fica bem claro/quente para mostrar que
    // está prestes a interagir com o combustível (antes ficava escuro demais
    // contra a escuridão do cenário).
    const bool canUse = inventory.CanCombineOilWithActive();
    const Uint8 iconAlpha = canUse ? 255 : 120;

    float bobOffset = std::sin(bobTimer * 3.0f) * 7.0f;

    // Roda vertical: o óleo flutuante fica à DIREITA do slot ativo (lado aberto),
    // centrado verticalmente, com um leve balanço.
    const float iconSize = kIconSize * 0.6f;
    const float iconCX = activeX + kSlotSize * 0.5f + 16.0f + iconSize * 0.5f;
    const float iconX  = iconCX - iconSize * 0.5f;
    const float iconY  = activeY - iconSize * 0.5f + bobOffset;

    auto tex = Resources::GetImage(inventory.GetPrimedOilSpritePath());
    if (tex) {
        const SDL_FRect dst = FitIconRect(tex.get(), iconX, iconY, iconSize, iconSize);
        SDL_SetTextureAlphaMod(tex.get(), iconAlpha);
        if (canUse) {
            SDL_SetTextureColorMod(tex.get(), 255, 250, 235);
        } else {
            SDL_SetTextureColorMod(tex.get(), 175, 175, 185);
        }
        SDL_RenderCopyExF(renderer, tex.get(), nullptr, &dst, 0.0, nullptr, SDL_FLIP_NONE);
    }

    auto font = Resources::GetFont("Recursos/font/times.ttf", 10);
    if (font) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d", inventory.GetPrimedOilDurability());
        SDL_Color textColor = canUse ? SDL_Color{255, 215, 40, 230}
                                     : SDL_Color{170, 170, 170, 90};
        SDL_Surface* surface = TTF_RenderUTF8_Blended(font.get(), buf, textColor);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            const float textW = static_cast<float>(surface->w);
            const float textH = static_cast<float>(surface->h);
            SDL_FreeSurface(surface);
            if (texture) {
                const SDL_FRect textDst{iconCX - textW * 0.5f, iconY + iconSize + 2.0f, textW, textH};
                SDL_RenderCopyF(renderer, texture, nullptr, &textDst);
                SDL_DestroyTexture(texture);
            }
        }
    }
}

void InventoryWheel::DrawCycleKeyHints(SDL_Renderer* renderer) {
    // As teclas ficam SEMPRE visíveis junto da roda (mesmo com um item só).
    const int count = inventory.GetStackCount();
    const int visibleCount = (count >= 4) ? 5 : 3;

    // Roda VERTICAL: i=0 é o slot de CIMA; i=visibleCount-1 é o de BAIXO
    // (ver GetSlotScreenPos). CyclePrev (↑) traz o item de cima; CycleNext (↓) o de baixo.
    float tx, ty, bx, by;
    GetSlotScreenPos(0, visibleCount, 0.0f, tx, ty);                 // slot do topo
    GetSlotScreenPos(visibleCount - 1, visibleCount, 0.0f, bx, by);  // slot de baixo

    // Meia-altura do slot das pontas (para colar as caixas acima/abaixo dele).
    const int half = visibleCount / 2;
    const float edgeSlotHalf = kSlotSize * GetSlotScale(half, visibleCount) * 0.5f;
    const float slotGap = 10.0f;

    InputManager& im = InputManager::GetInstance();
    auto keyName = [](int code, const char* fallback) {
        const char* k = SDL_GetKeyName(code);
        return (k && k[0] != '\0') ? std::string(k) : std::string(fallback);
    };
    const std::string upKey   = keyName(im.GetBinding(GameAction::CyclePrev), "Left");
    const std::string downKey = keyName(im.GetBinding(GameAction::CycleNext), "Right");

    auto font = Resources::GetFont("Recursos/font/times.ttf", 16);
    if (!font) return;

    // dir = -1 (acima) / +1 (abaixo): posiciona a caixa colada ao slot da ponta.
    auto drawBadge = [&](float slotCX, float slotCY, float dir, const std::string& text) {
        SDL_Color col{245, 232, 200, 255};
        SDL_Surface* sf = TTF_RenderUTF8_Blended(font.get(), text.c_str(), col);
        if (!sf) return;
        SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, sf);
        const int tw = sf->w;
        const int th = sf->h;
        SDL_FreeSurface(sf);
        if (!t) return;

        const float padX = 7.0f;
        const float padY = 4.0f;
        const float bw = std::max(static_cast<float>(tw), 13.0f) + padX * 2.0f;
        const float bh = static_cast<float>(th) + padY * 2.0f;
        const float edge = slotCY + dir * edgeSlotHalf;
        const float cy = edge + dir * (slotGap + bh * 0.5f);
        const SDL_FRect bg{slotCX - bw * 0.5f, cy - bh * 0.5f, bw, bh};

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 18, 18, 24, 210);
        SDL_RenderFillRectF(renderer, &bg);
        SDL_SetRenderDrawColor(renderer, 200, 180, 110, 230);
        SDL_RenderDrawRectF(renderer, &bg);

        const SDL_FRect dst{slotCX - tw * 0.5f, cy - th * 0.5f, static_cast<float>(tw), static_cast<float>(th)};
        SDL_RenderCopyF(renderer, t, nullptr, &dst);
        SDL_DestroyTexture(t);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    };

    drawBadge(tx, ty, -1.0f, upKey);    // ↑ acima do slot de cima
    drawBadge(bx, by, +1.0f, downKey);  // ↓ abaixo do slot de baixo
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
    // <=3 items -> 3 slots (empty slots fill the gaps).
    // >=4 items -> 5 slots (4 items still leave one empty slot).
    int visibleCount = (count >= 4) ? 5 : 3;
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

    if (inventory.IsOilPrimed()) {
        DrawPrimedOil(renderer, activeSlotX, activeSlotY);
    }

    DrawCycleKeyHints(renderer);
}
