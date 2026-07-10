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
    // Roda VERTICAL: 0° = slot ATIVO no vértice DIREITO do arco; offsets negativos
    // sobem, positivos descem. O arco abre para a ESQUERDA (centro à esquerda do
    // ativo), então os vizinhos ficam acima-esquerda e abaixo-esquerda.
    float angleDeg = shiftedOffset * angleStep;
    float angleRad = angleDeg * static_cast<float>(M_PI) / 180.0f;

    const float arcRadius = kArcRadius * Game::UiScale();
    const float activeX = GetAnchorX();
    const float activeY = GetAnchorY();
    const float ccx = activeX - arcRadius;   // centro do arco à esquerda do ativo
    const float ccy = activeY;

    outX = ccx + arcRadius * std::cos(angleRad);
    outY = ccy + arcRadius * std::sin(angleRad);
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
    scale *= Game::UiScale();   // escala p/ a resolução (além da perspectiva do slot)
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

    const float iconSize = kIconSize * scale * 2.25f;   // +50% e +50% (só a imagem do item)
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

namespace {
// Rótulo amigável (PT) para as fontes de luz no modal de reabastecimento.
std::string RefuelDisplayName(const std::string& internalName) {
    if (internalName == "Flashlight" || internalName == "Broken Flashlight") return "Isqueiro";
    if (internalName == "Lamp") return "Lamparina";
    return internalName;
}

// Desenha um texto centrado horizontalmente em cx, com o topo em topY. Devolve
// a altura desenhada (0 se falhar).
float DrawCenteredText(SDL_Renderer* r, TTF_Font* font, const std::string& text,
                       float cx, float topY, SDL_Color color) {
    if (!font) return 0.0f;
    SDL_Surface* sf = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!sf) return 0.0f;
    SDL_Texture* t = SDL_CreateTextureFromSurface(r, sf);
    const float w = static_cast<float>(sf->w);
    const float h = static_cast<float>(sf->h);
    SDL_FreeSurface(sf);
    if (!t) return 0.0f;
    const SDL_FRect dst{cx - w * 0.5f, topY, w, h};
    SDL_RenderCopyF(r, t, nullptr, &dst);
    SDL_DestroyTexture(t);
    return h;
}
}  // namespace

// Modal central: "Escolha qual item quer abastecer". Lista as fontes de luz que
// ainda cabem óleo; a selecionada fica destacada. Navega com A/D ou setas e
// confirma com [F].
void InventoryWheel::DrawRefuelSelector(SDL_Renderer* renderer) {
    if (!inventory.IsOilPrimed()) return;
    const std::vector<int> targets = inventory.GetRefuelTargetIndices();
    if (targets.empty()) return;

    // Espaço LÓGICO (resolução escolhida) — NÃO o tamanho físico da tela — e
    // escala de UI para caber/ficar consistente em qualquer resolução.
    const int winW = Game::GetInstance().GetWindowsWidth();
    const int winH = Game::GetInstance().GetWindowsHeight();
    const float u = Game::UiScale();
    const float cx = winW * 0.5f;
    const float cy = winH * 0.5f;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Fundo escurecido em tela cheia.
    const SDL_FRect full{0.0f, 0.0f, static_cast<float>(winW), static_cast<float>(winH)};
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 175);
    SDL_RenderFillRectF(renderer, &full);

    auto scaledFont = [&](int base, int floorPx) {
        return Resources::GetFont("Recursos/font/times.ttf",
                                  std::max(floorPx, static_cast<int>(std::lround(base * u))));
    };

    // Título.
    auto titleFont = scaledFont(30, 14);
    DrawCenteredText(renderer, titleFont ? titleFont.get() : nullptr,
                     "Escolha qual item quer abastecer",
                     cx, cy - 150.0f * u, SDL_Color{240, 228, 195, 255});

    // Linha de candidatos.
    const int n = static_cast<int>(targets.size());
    const int sel = inventory.GetRefuelSelection();
    const float slot = 118.0f * u;
    const float gap  = 46.0f * u;
    const float totalW = n * slot + (n - 1) * gap;
    const float startCX = cx - totalW * 0.5f + slot * 0.5f;
    const float pulse = 1.0f + std::sin(bobTimer * 4.0f) * 0.04f;

    auto nameFont = scaledFont(18, 10);

    for (int i = 0; i < n; ++i) {
        const bool selected = (i == sel);
        const float scx = startCX + i * (slot + gap);
        const float radius = (slot * 0.5f) * (selected ? pulse : 0.9f);

        // Halo suave na seleção.
        if (selected) {
            FillCircle(renderer, scx, cy, radius * 1.28f, 235, 200, 110, 45);
            FillCircle(renderer, scx, cy, radius * 1.14f, 235, 200, 110, 60);
        }
        FillCircle(renderer, scx, cy, radius, 26, 26, 33, selected ? 235 : 170);
        if (selected) {
            StrokeCircle(renderer, scx, cy, radius, 230, 195, 60, 255);
            StrokeCircle(renderer, scx, cy, radius - 2.0f, 230, 195, 60, 150);
        } else {
            StrokeCircle(renderer, scx, cy, radius, 70, 70, 85, 220);
        }

        const Inventory::ItemStack* stack = inventory.GetRefuelTargetStack(i);
        if (!stack) continue;

        // Ícone.
        auto tex = Resources::GetImage(stack->def.spritePath);
        if (tex) {
            const float iconSz = radius * 1.25f;
            const SDL_FRect dst = FitIconRect(tex.get(), scx - iconSz * 0.5f,
                                              cy - iconSz * 0.5f, iconSz, iconSz);
            SDL_SetTextureAlphaMod(tex.get(), selected ? 255 : 150);
            SDL_SetTextureColorMod(tex.get(), 255, 255, 255);
            SDL_RenderCopyExF(renderer, tex.get(), nullptr, &dst, 0.0, nullptr, SDL_FLIP_NONE);
        }

        // Anel de durabilidade atual.
        if (stack->def.maxDurability > 0 && !stack->durabilities.empty()) {
            float ratio = static_cast<float>(stack->durabilities.front()) /
                          static_cast<float>(stack->def.maxDurability);
            ratio = std::max(0.0f, std::min(1.0f, ratio));
            Uint8 gR, gG, gB;
            if (ratio > 0.6f)       { gR = 70;  gG = 180; gB = 80; }
            else if (ratio > 0.25f) { gR = 210; gG = 170; gB = 50; }
            else                    { gR = 200; gG = 55;  gB = 55; }
            DrawDurabilityRing(renderer, scx, cy, radius + 9.0f * u, 6.0f * u, ratio,
                               gR, gG, gB, selected ? 255 : 160);
        }

        // Nome do item abaixo do slot.
        DrawCenteredText(renderer, nameFont ? nameFont.get() : nullptr,
                         RefuelDisplayName(stack->def.name), scx, cy + radius + 16.0f * u,
                         selected ? SDL_Color{245, 232, 200, 255}
                                  : SDL_Color{160, 160, 170, 220});
    }

    // Dicas de navegação/confirmação.
    InputManager& im = InputManager::GetInstance();
    auto keyName = [](int code, const char* fb) {
        const char* k = SDL_GetKeyName(code);
        return (k && k[0] != '\0') ? std::string(k) : std::string(fb);
    };
    const std::string lf = keyName(im.GetBinding(GameAction::MoveLeft), "A");
    const std::string rt = keyName(im.GetBinding(GameAction::MoveRight), "D");
    const std::string useK = keyName(im.GetBinding(GameAction::UseItem), "F");

    auto hintFont = scaledFont(17, 10);
    DrawCenteredText(renderer, hintFont ? hintFont.get() : nullptr,
                     "[" + lf + "] / [" + rt + "] escolher     [" + useK + "] confirmar     [ESC] cancelar",
                     cx, cy + 140.0f * u, SDL_Color{205, 200, 185, 235});

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
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
    const float u = Game::UiScale();
    const int half = visibleCount / 2;
    const float edgeSlotHalf = kSlotSize * GetSlotScale(half, visibleCount) * 0.5f * u;
    const float slotGap = 10.0f * u;

    InputManager& im = InputManager::GetInstance();
    auto keyName = [](int code, const char* fallback) {
        const char* k = SDL_GetKeyName(code);
        return (k && k[0] != '\0') ? std::string(k) : std::string(fallback);
    };
    const std::string prevLabel = keyName(im.GetBinding(GameAction::CyclePrev), "Left");
    const std::string nextLabel = keyName(im.GetBinding(GameAction::CycleNext), "Right");

    // Imagem da tecla-seta (mesma p/ os dois; a da ESQUERDA é espelhada) + rótulo
    // pequeno logo ABAIXO da imagem.
    auto keyTex = Resources::GetImage("Recursos/img/hud/key_arrow.png");
    auto font   = Resources::GetFont("Recursos/font/times.ttf",
                                     std::max(13, static_cast<int>(std::lround(20.0f * u))));

    // dir = -1 (acima do slot de cima) / +1 (abaixo do de baixo).
    // leftArrow=true → espelha a imagem (seta apontando p/ a esquerda).
    auto drawKeyHint = [&](float slotCX, float slotCY, float dir, bool leftArrow, const std::string& label) {
        const float imgSize = 47.0f * u;   // 30→39 (+30%) →47 (+20%)
        const float textGap = 1.0f * u;

        SDL_Texture* txt = nullptr; int tw = 0, th = 0;
        if (font) {
            SDL_Color col{235, 225, 195, 255};
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
        // Modal central "Escolha qual item quer abastecer" — substitui as dicas
        // da roda enquanto o reabastecimento está ativo.
        DrawRefuelSelector(renderer);
    } else {
        DrawUseHint(renderer, activeSlotX, activeSlotY);
        DrawCycleKeyHints(renderer);
    }
}

// "[F] Usar" logo ao lado do slot ativo quando o item tem uso
// (isqueiro/lâmpada = LIGHT_SOURCE, ou óleo = FUEL).
void InventoryWheel::DrawUseHint(SDL_Renderer* renderer, float activeX, float activeY) {
    const Inventory::ItemStack* active = inventory.GetActiveStack();
    if (!active) return;
    const bool usable = active->def.HasProperty(ItemProperty::LIGHT_SOURCE) ||
                        active->def.HasProperty(ItemProperty::FUEL);
    if (!usable) return;

    // Mesmo tamanho/estilo das dicas de seta (DrawCycleKeyHints): ícone da tecla
    // (~30px) + rótulo pequeno logo abaixo. Aqui o ícone é a tecla [F] e o texto
    // é "Usar".
    const float u = Game::UiScale();
    auto keyTex = Resources::GetImage("Recursos/img/hud/key_f.png");
    auto font = Resources::GetFont("Recursos/font/times.ttf",
                                   std::max(13, static_cast<int>(std::lround(20.0f * u))));
    const float imgSize = 47.0f * u;   // 30→39 (+30%) →47 (+20%)
    const float textGap = 1.0f * u;

    SDL_Texture* txt = nullptr; int tw = 0, th = 0;
    if (font) {
        SDL_Color col{235, 225, 195, 255};
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
