#include "gameplay/BackpackVisuals.h"
#include "core/Game.h"
#include "core/Resources.h"
#include "gameplay/Inventory.h"

#define INCLUDE_SDL_TTF
#include "SDL_include.h"

#include <cstdio>
#include <cmath>

BackpackVisuals::BackpackVisuals(GameObject& associated, Inventory& inventory, Character* bigChar)
    : Component(associated), inventory(inventory), bigCharacter(bigChar) {
    (void)bigCharacter;
}

void BackpackVisuals::Start() {}

void BackpackVisuals::Update(float dt) {
    (void)dt;
}

void BackpackVisuals::DrawQuickSlot(SDL_Renderer* renderer, int slotIndex, float x, float y,
                                      bool isSelected, const char* label) const {
    const ItemInstance* item = inventory.GetItem(slotIndex);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    Uint8 bgR = 25, bgG = 28, bgB = 35, bgA = 200;
    float borderWidth = 1.5f;
    Uint8 borderR = 70, borderG = 70, borderB = 80, borderA = 220;

    if (isSelected) {
        bgR = 35; bgG = 38; bgB = 50; bgA = 230;
        borderWidth = 2.5f;
        borderR = 210; borderG = 190; borderB = 80; borderA = 255;
    }

    SDL_SetRenderDrawColor(renderer, bgR, bgG, bgB, bgA);
    const SDL_FRect bgRect{x, y, kSlotSize, kSlotSize};
    SDL_RenderFillRectF(renderer, &bgRect);

    SDL_SetRenderDrawColor(renderer, borderR, borderG, borderB, borderA);
    for (float bw = 0; bw < borderWidth; bw += 1.0f) {
        const SDL_FRect borderRect{x + bw, y + bw,
                                    kSlotSize - bw * 2.0f, kSlotSize - bw * 2.0f};
        SDL_RenderDrawRectF(renderer, &borderRect);
    }

    if (!item) {
        const SDL_FRect innerRect{x + kIconPadding, y + kIconPadding,
                                   kSlotSize - kIconPadding * 2.0f,
                                   kSlotSize - kIconPadding * 2.0f};
        SDL_SetRenderDrawColor(renderer, 45, 45, 55, 120);
        SDL_RenderDrawRectF(renderer, &innerRect);
    } else {
        const float iconSize = kSlotSize - kIconPadding * 2.0f;
        const float iconX = x + kIconPadding;
        const float iconY = y + kIconPadding;

        auto tex = Resources::GetImage(item->def.spritePath);
        if (tex) {
            const SDL_FRect dst{iconX, iconY, iconSize, iconSize};
            SDL_SetTextureAlphaMod(tex.get(), 255);
            SDL_SetTextureColorMod(tex.get(), 255, 255, 255);
            SDL_RenderCopyExF(renderer, tex.get(), nullptr, &dst, 0.0, nullptr, SDL_FLIP_NONE);
        }
    }

    const float gaugeY = y + kSlotSize + 2.0f;

    SDL_SetRenderDrawColor(renderer, 35, 35, 45, 230);
    const SDL_FRect gaugeBg{x, gaugeY, kSlotSize, kGaugeHeight};
    SDL_RenderFillRectF(renderer, &gaugeBg);

    if (item && item->def.HasProperty(ItemProperty::LIGHT_SOURCE) && item->def.maxDurability > 0) {
        const float ratio = static_cast<float>(item->durability) /
                            static_cast<float>(item->def.maxDurability);
        const float clampedRatio = std::max(0.0f, std::min(1.0f, ratio));

        Uint8 gR, gG, gB;
        if (clampedRatio > 0.6f) {
            gR = 80; gG = 190; gB = 90;
        } else if (clampedRatio > 0.25f) {
            gR = 220; gG = 180; gB = 60;
        } else {
            gR = 210; gG = 65; gB = 65;
        }

        SDL_SetRenderDrawColor(renderer, gR, gG, gB, 240);
        const SDL_FRect gaugeRect{x, gaugeY, kSlotSize * clampedRatio, kGaugeHeight};
        SDL_RenderFillRectF(renderer, &gaugeRect);
    }

    SDL_SetRenderDrawColor(renderer, 80, 80, 90, 200);
    const SDL_FRect gaugeOutline{x, gaugeY, kSlotSize, kGaugeHeight};
    SDL_RenderDrawRectF(renderer, &gaugeOutline);

    auto font = Resources::GetFont("Recursos/font/TradeWinds-Regular.ttf", 11);
    if (font && item && item->def.HasProperty(ItemProperty::LIGHT_SOURCE)) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d/%d", item->durability, item->def.maxDurability);
        SDL_Color textColor{210, 200, 175, 240};
        SDL_Surface* surface = TTF_RenderText_Blended(font.get(), buf, textColor);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            const float textW = static_cast<float>(surface->w);
            const float textH = static_cast<float>(surface->h);
            SDL_FreeSurface(surface);
            if (texture) {
                const float textX = x + (kSlotSize - textW) / 2.0f;
                const float textY = gaugeY - textH - 1.0f;
                const SDL_FRect textDst{textX, textY, textW, textH};
                SDL_SetTextureAlphaMod(texture, 230);
                SDL_RenderCopyF(renderer, texture, nullptr, &textDst);
                SDL_DestroyTexture(texture);
            }
        }
    }

    auto labelFont = Resources::GetFont("Recursos/font/TradeWinds-Regular.ttf", 12);
    if (labelFont) {
        SDL_Color labelColor{180, 170, 150, 220};
        if (isSelected) {
            labelColor = {220, 200, 120, 240};
        }
        SDL_Surface* surface = TTF_RenderText_Blended(labelFont.get(), label, labelColor);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            const float textW = static_cast<float>(surface->w);
            const float textH = static_cast<float>(surface->h);
            SDL_FreeSurface(surface);
            if (texture) {
                const float textX = x + (kSlotSize - textW) / 2.0f;
                const float textY = y + kSlotSize + kGaugeHeight + 4.0f;
                const SDL_FRect textDst{textX, textY, textW, textH};
                SDL_SetTextureAlphaMod(texture, 230);
                SDL_RenderCopyF(renderer, texture, nullptr, &textDst);
                SDL_DestroyTexture(texture);
            }
        }
    }
}

void BackpackVisuals::Render() {
    SDL_Renderer* renderer = Game::GetInstance().GetRenderer();
    if (!renderer) return;

    const float winW = static_cast<float>(Game::GetInstance().GetWindowsWidth());
    const float winH = static_cast<float>(Game::GetInstance().GetWindowsHeight());

    const int flashlightSlot = inventory.FindBestFlashlight();
    const int lampSlot = inventory.FindBestLamp();
    const int selectedSlot = inventory.GetSelectedSlot();

    const float startX = winW - kHudMargin - kSlotSize * 2.0f - kSlotGap;
    const float startY = winH - kHudMargin - (kSlotSize + 6.0f + kGaugeHeight + 2.0f + 14.0f);

    const bool selFlashlight = (flashlightSlot >= 0 && flashlightSlot == selectedSlot);
    DrawQuickSlot(renderer, flashlightSlot, startX, startY, selFlashlight, "1 - Lant.");

    const float lampX = startX + kSlotSize + kSlotGap;
    const bool selLamp = (lampSlot >= 0 && lampSlot == selectedSlot);
    DrawQuickSlot(renderer, lampSlot, lampX, startY, selLamp, "2 - Lamp.");
}
