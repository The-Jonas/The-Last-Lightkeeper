#include "ui/InventoryWheel.h"
#include "core/Game.h"
#include "core/Resources.h"

#define INCLUDE_SDL_TTF
#include "SDL_include.h"
#include "states/stage/StageState.h"

#include <algorithm>
#include <cstdio>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
    return static_cast<float>(Game::GetInstance().GetWindowsHeight()) + kAnchorYOffset;
}

void InventoryWheel::GetSlotScreenPos(int visibleOffset, int visibleCount, float scrollOffset,
                                       float& outX, float& outY) const {
    int half = visibleCount / 2;
    float baseOffset = static_cast<float>(visibleOffset - half);
    float shiftedOffset = baseOffset + scrollOffset;

    float angleStep = (visibleCount <= 3) ? 50.0f : 37.0f;
    float angleDeg = 90.0f + shiftedOffset * angleStep;
    float angleRad = angleDeg * static_cast<float>(M_PI) / 180.0f;

    float cx = GetAnchorX();
    float cy = GetAnchorY() - kArcRadius;

    outX = cx + kArcRadius * std::cos(angleRad);
    outY = cy + kArcRadius * std::sin(angleRad);
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

    Uint8 bgR = 28, bgG = 28, bgB = 35, bgA = static_cast<Uint8>(std::min(255.0f, alpha * 0.85f));
    SDL_SetRenderDrawColor(renderer, bgR, bgG, bgB, bgA);
    const SDL_FRect bgRect{slotX, slotY, scaledSize, scaledSize};
    SDL_RenderFillRectF(renderer, &bgRect);

    Uint8 borderR, borderG, borderB;
    if (isActive && inventory.IsOilPrimed()) {
        borderR = 230; borderG = 190; borderB = 40;
    } else if (isActive) {
        borderR = 210; borderG = 190; borderB = 80;
    } else {
        borderR = 60; borderG = 60; borderB = 75;
    }
    Uint8 borderA = static_cast<Uint8>(std::min(255.0f, alpha));
    SDL_SetRenderDrawColor(renderer, borderR, borderG, borderB, borderA);
    SDL_RenderDrawRectF(renderer, &bgRect);

    if (stackIndex < 0) return;

    const Inventory::ItemStack* stack = inventory.GetStack(stackIndex);
    if (!stack) return;

    const float iconSize = kIconSize * scale;
    const float iconX = slotX + (scaledSize - iconSize) * 0.5f;
    const float iconY = slotY + (scaledSize - iconSize) * 0.5f - 4.0f * scale;

    auto tex = Resources::GetImage(stack->def.spritePath);
    if (tex) {
        const SDL_FRect dst{iconX, iconY, iconSize, iconSize};
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

        const float gaugeY = slotY + scaledSize - kGaugeHeight * scale - 2.0f * scale;
        const float gaugeInsetX = 2.0f * scale;
        const float gaugeW = (scaledSize - gaugeInsetX * 2.0f) * ratio;

        Uint8 gR, gG, gB;
        if (ratio > 0.6f) { gR = 70; gG = 180; gB = 80; }
        else if (ratio > 0.25f) { gR = 210; gG = 170; gB = 50; }
        else { gR = 200; gG = 55; gB = 55; }

        SDL_SetRenderDrawColor(renderer, gR, gG, gB, static_cast<Uint8>(std::min(255.0f, alpha * 0.9f)));
        const SDL_FRect gaugeRect{slotX + gaugeInsetX, gaugeY, gaugeW, kGaugeHeight * scale};
        SDL_RenderFillRectF(renderer, &gaugeRect);

        SDL_SetRenderDrawColor(renderer, 40, 40, 45, static_cast<Uint8>(std::min(255.0f, alpha * 0.5f)));
        const SDL_FRect gaugeBg{slotX + gaugeInsetX, gaugeY, scaledSize - gaugeInsetX * 2.0f, kGaugeHeight * scale};
        SDL_RenderDrawRectF(renderer, &gaugeBg);
    }
}

void InventoryWheel::DrawPrimedOil(SDL_Renderer* renderer, float activeX, float activeY) {
    if (!inventory.IsOilPrimed()) return;

    // When the centered slot can't take the oil (empty slot or a non-light /
    // already-full item), keep the floating icon faded so the player sees it.
    const bool canUse = inventory.CanCombineOilWithActive();
    const Uint8 iconAlpha = canUse ? 240 : 70;

    float bobOffset = std::sin(bobTimer * 3.0f) * 7.0f;

    const float iconSize = kIconSize * 0.85f;
    const float iconX = activeX - iconSize * 0.5f;
    const float iconY = activeY - kSlotSize * 0.5f - iconSize - 18.0f + bobOffset;

    auto tex = Resources::GetImage(inventory.GetPrimedOilSpritePath());
    if (tex) {
        const SDL_FRect dst{iconX, iconY, iconSize, iconSize};
        SDL_SetTextureAlphaMod(tex.get(), iconAlpha);
        if (canUse) {
            SDL_SetTextureColorMod(tex.get(), 255, 255, 220);
        } else {
            SDL_SetTextureColorMod(tex.get(), 150, 150, 160);
        }
        SDL_RenderCopyExF(renderer, tex.get(), nullptr, &dst, 0.0, nullptr, SDL_FLIP_NONE);
    }

    auto font = Resources::GetFont("Recursos/font/times.ttf", 10);
    if (font) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d", inventory.GetPrimedOilDurability());
        SDL_Color textColor = canUse ? SDL_Color{255, 215, 40, 230}
                                     : SDL_Color{170, 170, 170, 90};
        SDL_Surface* surface = TTF_RenderText_Blended(font.get(), buf, textColor);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            const float textW = static_cast<float>(surface->w);
            const float textH = static_cast<float>(surface->h);
            SDL_FreeSurface(surface);
            if (texture) {
                const SDL_FRect textDst{activeX - textW * 0.5f, iconY + iconSize + 2.0f, textW, textH};
                SDL_RenderCopyF(renderer, texture, nullptr, &textDst);
                SDL_DestroyTexture(texture);
            }
        }
    }
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
}
