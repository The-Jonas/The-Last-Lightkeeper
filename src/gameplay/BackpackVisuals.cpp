#include "gameplay/BackpackVisuals.h"
#include "core/Game.h"
#include "core/Resources.h"
#include "gameplay/Inventory.h"

#define INCLUDE_SDL_TTF
#include "SDL_include.h"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace {

constexpr float kHudMargin = 18.0f;
constexpr float kIconSize = 46.0f;
constexpr float kSlotGap = 10.0f;
constexpr float kStackOffsetX = 5.0f;
constexpr float kStackOffsetY = -5.0f;
constexpr int kMaxStackLayers = 4;

const char* const kPriorityGroupIds[] = {"lighter", "lamp", "fuel"};

void DrawItemIcon(SDL_Renderer* renderer, const ItemInstance& item, float x, float y, float size, Uint8 alpha) {
    auto tex = Resources::GetImage(item.def.spritePath);
    if (!tex) {
        return;
    }
    const SDL_FRect dst{x, y, size, size};
    SDL_SetTextureAlphaMod(tex.get(), alpha);
    SDL_SetTextureColorMod(tex.get(), 255, 255, 255);
    SDL_RenderCopyExF(renderer, tex.get(), nullptr, &dst, 0.0, nullptr, SDL_FLIP_NONE);
}

void DrawCountLabel(SDL_Renderer* renderer, int count, float anchorX, float anchorY) {
    if (count <= 1) {
        return;
    }

    char buf[8];
    std::snprintf(buf, sizeof(buf), "x%d", count);
    auto font = Resources::GetFont("Recursos/font/TradeWinds-Regular.ttf", 14);
    if (!font) {
        return;
    }

    SDL_Color color{240, 230, 200, 235};
    SDL_Surface* surface = TTF_RenderText_Blended(font.get(), buf, color);
    if (!surface) {
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    const float textW = static_cast<float>(surface->w);
    const float textH = static_cast<float>(surface->h);
    const float textX = anchorX - textW;
    const float textY = anchorY - textH;
    SDL_FreeSurface(surface);

    if (texture) {
        const SDL_FRect textDst{textX, textY, textW, textH};
        SDL_RenderCopyF(renderer, texture, nullptr, &textDst);
        SDL_DestroyTexture(texture);
    }
}

std::vector<int> BuildDisplayOrder(const BackpackConfig& cfg, const Inventory& inventory) {
    std::vector<int> order;
    order.reserve(cfg.groups.size());

    for (const char* groupId : kPriorityGroupIds) {
        const int groupIndex = cfg.GroupIndexForId(groupId);
        if (groupIndex >= 0 && inventory.CountInGroup(groupIndex) > 0) {
            order.push_back(groupIndex);
        }
    }

    for (int gi = 0; gi < static_cast<int>(cfg.groups.size()); ++gi) {
        if (inventory.CountInGroup(gi) <= 0) {
            continue;
        }
        const BackpackGroupDef* group = cfg.GetGroup(gi);
        if (!group) {
            continue;
        }
        const bool priority = std::any_of(std::begin(kPriorityGroupIds), std::end(kPriorityGroupIds),
                                          [&](const char* id) { return group->id == id; });
        if (!priority) {
            order.push_back(gi);
        }
    }

    return order;
}

} // namespace

BackpackVisuals::BackpackVisuals(GameObject& associated, Inventory& inventory, Character* bigChar)
    : Component(associated), inventory(inventory), bigCharacter(bigChar) {
    (void)bigCharacter;
}

void BackpackVisuals::Start() {}

void BackpackVisuals::Update(float dt) {
    (void)dt;
}

void BackpackVisuals::Render() {
    SDL_Renderer* renderer = Game::GetInstance().GetRenderer();
    if (!renderer) {
        return;
    }

    const int winW = Game::GetInstance().GetWindowsWidth();
    const int winH = Game::GetInstance().GetWindowsHeight();
    const BackpackConfig& cfg = inventory.GetBackpackConfig();
    const int selectedGroup = inventory.GetSelectedGroup();

    const std::vector<int> displayOrder = BuildDisplayOrder(cfg, inventory);
    if (displayOrder.empty()) {
        return;
    }

    float cursorX = static_cast<float>(winW) - kHudMargin;
    const float baseY = static_cast<float>(winH) - kHudMargin - kIconSize;

    for (int slot = static_cast<int>(displayOrder.size()) - 1; slot >= 0; --slot) {
        const int groupIndex = displayOrder[static_cast<size_t>(slot)];
        const int count = inventory.CountInGroup(groupIndex);
        const ItemInstance* frontItem = inventory.GetItemInGroup(groupIndex, 0);
        if (!frontItem || count <= 0) {
            continue;
        }

        cursorX -= kIconSize;
        const float slotX = cursorX;
        const bool selected = groupIndex == selectedGroup;
        const float iconSize = selected ? kIconSize * 1.08f : kIconSize;
        const float sizeOffset = (iconSize - kIconSize) * 0.5f;
        const float drawBaseX = slotX - sizeOffset;
        const float drawBaseY = baseY - sizeOffset;

        const int layers = std::min(count, kMaxStackLayers);
        for (int layer = layers - 1; layer >= 0; --layer) {
            const int itemIndex = std::min(layer, count - 1);
            const ItemInstance* item = inventory.GetItemInGroup(groupIndex, itemIndex);
            if (!item) {
                continue;
            }

            const float drawX = drawBaseX + static_cast<float>(layer) * kStackOffsetX;
            const float drawY = drawBaseY + static_cast<float>(layer) * kStackOffsetY;
            const Uint8 alpha = static_cast<Uint8>(layer == layers - 1 ? 255 : 210);
            DrawItemIcon(renderer, *item, drawX, drawY, iconSize, alpha);
        }

        DrawCountLabel(renderer, count, slotX + kIconSize, baseY + kIconSize);
        cursorX -= kSlotGap;
    }
}
