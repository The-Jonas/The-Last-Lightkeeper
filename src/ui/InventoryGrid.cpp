#include "ui/InventoryGrid.h"
#include "core/Game.h"
#include "core/InputManager.h"
#include "core/Resources.h"
#include "gameplay/Inventory.h"
#include "gameplay/Character.h"
#include "gameplay/ItemPickup.h"
#include "engine/GameObject.h"

#define INCLUDE_SDL_TTF
#include "SDL_include.h"

#include <cstdio>
#include <cmath>

InventoryGrid::InventoryGrid(GameObject& associated, Inventory& inventory,
                             Character* bigChar,
                             std::vector<ItemPickup*>* pickups,
                             std::function<void(GameObject*)> addObjFn)
    : Component(associated), inventory(inventory),
      bigCharacter(bigChar), itemPickups(pickups),
      addObjectToState(std::move(addObjFn)) {
}

void InventoryGrid::Start() {}

void InventoryGrid::Toggle() {
    if (isOpen) {
        Close();
    } else {
        Open();
    }
}

void InventoryGrid::Open() {
    isOpen = true;
    dragSourceSlot = -1;
    isDragging = false;
}

void InventoryGrid::Close() {
    isOpen = false;
    dragSourceSlot = -1;
    isDragging = false;
}

void InventoryGrid::CancelDrag() {
    dragSourceSlot = -1;
    isDragging = false;
}

void InventoryGrid::PerformDrop(int targetSlot) {
    if (targetSlot < 0) {
        const ItemInstance* item = inventory.GetItem(dragSourceSlot);
        if (item && bigCharacter && itemPickups && addObjectToState) {
            const Vec2 pos = bigCharacter->GetCenter();
            ItemPickup* pickup = ItemPickup::Spawn(pos.x, pos.y, item->def, item->durability, *itemPickups);
            if (pickup) {
                addObjectToState(&pickup->GetAssociated());
            }
        }
        inventory.RemoveFromSlot(dragSourceSlot);
        CancelDrag();
        return;
    }

    if (targetSlot == dragSourceSlot) {
        CancelDrag();
        return;
    }

    const ItemInstance* draggedItem = inventory.GetItem(dragSourceSlot);
    const ItemInstance* targetItem = inventory.GetItem(targetSlot);

    if (draggedItem && draggedItem->def.HasProperty(ItemProperty::FUEL)) {
        if (targetItem && targetItem->def.HasProperty(ItemProperty::LIGHT_SOURCE)) {
            inventory.TryCombineOil(dragSourceSlot, targetSlot);
            CancelDrag();
            return;
        }
    }

    if (targetItem && targetItem->def.HasProperty(ItemProperty::FUEL)) {
        if (draggedItem && draggedItem->def.HasProperty(ItemProperty::LIGHT_SOURCE)) {
            inventory.TryCombineOil(targetSlot, dragSourceSlot);
            CancelDrag();
            return;
        }
    }

    inventory.MoveItem(dragSourceSlot, targetSlot);
    CancelDrag();
}

void InventoryGrid::HandleMouseDown() {
    InputManager& input = InputManager::GetInstance();
    if (!input.MousePress(SDL_BUTTON_LEFT)) return;

    const int mx = input.GetMouseX();
    const int my = input.GetMouseY();
    const int clickedSlot = GetSlotAtPosition(mx, my);

    if (clickedSlot < 0) return;

    const ItemInstance* item = inventory.GetItem(clickedSlot);
    if (!item) return;

    dragSourceSlot = clickedSlot;
    isDragging = true;
}

void InventoryGrid::HandleMouseUp() {
    InputManager& input = InputManager::GetInstance();
    if (!input.MouseRelease(SDL_BUTTON_LEFT)) return;

    if (!isDragging) return;

    const int mx = input.GetMouseX();
    const int my = input.GetMouseY();
    const int targetSlot = GetSlotAtPosition(mx, my);

    PerformDrop(targetSlot);
}

void InventoryGrid::Update(float dt) {
    (void)dt;
    if (!isOpen) return;

    if (isDragging) {
        HandleMouseUp();
    } else {
        HandleMouseDown();
    }
}

float InventoryGrid::GetGridLeft() const {
    const float winW = static_cast<float>(Game::GetInstance().GetWindowsWidth());
    const float gridW = kGridCols * kSlotSize + (kGridCols - 1) * kSlotGap + kGridBorder * 2;
    return (winW - gridW) / 2.0f;
}

float InventoryGrid::GetGridTop() const {
    const float winH = static_cast<float>(Game::GetInstance().GetWindowsHeight());
    const float gridH = kGridRows * kSlotSize + (kGridRows - 1) * kSlotGap + kGridBorder * 2;
    return (winH - gridH) / 2.0f + kTitleHeight / 2.0f;
}

int InventoryGrid::GetSlotAtPosition(int mouseX, int mouseY) const {
    const float gridLeft = GetGridLeft() + kGridBorder;
    const float gridTop = GetGridTop() + kGridBorder;

    const float localX = static_cast<float>(mouseX) - gridLeft;
    const float localY = static_cast<float>(mouseY) - gridTop;

    if (localX < 0 || localY < 0) return -1;

    const float step = kSlotSize + kSlotGap;
    const int col = static_cast<int>(localX / step);
    const int row = static_cast<int>(localY / step);

    if (col >= kGridCols || row >= kGridRows) return -1;

    const float innerX = localX - col * step;
    const float innerY = localY - row * step;

    if (innerX >= kSlotSize || innerY >= kSlotSize) return -1;

    return row * kGridCols + col;
}

void InventoryGrid::DrawBackdrop(SDL_Renderer* renderer) {
    const int winW = Game::GetInstance().GetWindowsWidth();
    const int winH = Game::GetInstance().GetWindowsHeight();

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    const SDL_Rect backdrop{0, 0, winW, winH};
    SDL_RenderFillRect(renderer, &backdrop);
}

void InventoryGrid::Render() {
    if (!isOpen) return;

    SDL_Renderer* renderer = Game::GetInstance().GetRenderer();
    if (!renderer) return;

    DrawBackdrop(renderer);

    const float gridLeft = GetGridLeft();
    const float gridTop = GetGridTop();
    const float gridW = kGridCols * kSlotSize + (kGridCols - 1) * kSlotGap + kGridBorder * 2;
    const float gridH = kGridRows * kSlotSize + (kGridRows - 1) * kSlotGap + kGridBorder * 2;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 20, 20, 25, 230);
    const SDL_FRect gridRect{gridLeft, gridTop, gridW, gridH};
    SDL_RenderFillRectF(renderer, &gridRect);

    SDL_SetRenderDrawColor(renderer, 100, 100, 110, 250);
    SDL_RenderDrawRectF(renderer, &gridRect);

    int slotIdx = 0;
    for (int row = 0; row < kGridRows; ++row) {
        for (int col = 0; col < kGridCols; ++col) {
            const float x = gridLeft + kGridBorder + col * (kSlotSize + kSlotGap);
            const float y = gridTop + kGridBorder + row * (kSlotSize + kSlotGap);
            const bool dimmed = isDragging && slotIdx == dragSourceSlot;
            DrawSlot(renderer, slotIdx, x, y, dimmed);
            slotIdx++;
        }
    }

    auto font = Resources::GetFont("Recursos/font/TradeWinds-Regular.ttf", 20);
    if (font) {
        SDL_Color titleColor{220, 210, 180, 240};
        SDL_Surface* surface = TTF_RenderText_Blended(font.get(), "INVENTARIO", titleColor);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            const float textW = static_cast<float>(surface->w);
            const float textH = static_cast<float>(surface->h);
            const float textX = gridLeft + (gridW - textW) / 2.0f;
            const float textY = gridTop - textH - 8.0f;
            SDL_FreeSurface(surface);

            if (texture) {
                const SDL_FRect textDst{textX, textY, textW, textH};
                SDL_RenderCopyF(renderer, texture, nullptr, &textDst);
                SDL_DestroyTexture(texture);
            }
        }
    }

    if (isDragging) {
        DrawDraggedItem(renderer);
    }
}

void InventoryGrid::DrawSlot(SDL_Renderer* renderer, int slotIndex, float x, float y, bool dimmed) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    Uint8 bgR = 35, bgG = 35, bgB = 40, bgA = 200;
    if (dimmed) {
        bgR = 25; bgG = 25; bgB = 28; bgA = 100;
    }
    SDL_SetRenderDrawColor(renderer, bgR, bgG, bgB, bgA);
    const SDL_FRect bgRect{x, y, kSlotSize, kSlotSize};
    SDL_RenderFillRectF(renderer, &bgRect);

    Uint8 borderR = 60, borderG = 60, borderB = 70, borderA = 240;
    if (dimmed) {
        borderR = 40; borderG = 40; borderB = 50; borderA = 140;
    }
    SDL_SetRenderDrawColor(renderer, borderR, borderG, borderB, borderA);
    SDL_RenderDrawRectF(renderer, &bgRect);

    if (dimmed) return;

    const ItemInstance* item = inventory.GetItem(slotIndex);
    if (!item) return;

    const float iconX = x + (kSlotSize - kIconSize) / 2.0f;
    const float iconY = y + (kSlotSize - kIconSize) / 2.0f - 4.0f;

    auto tex = Resources::GetImage(item->def.spritePath);
    if (tex) {
        const SDL_FRect dst{iconX, iconY, kIconSize, kIconSize};
        SDL_SetTextureAlphaMod(tex.get(), 255);
        SDL_SetTextureColorMod(tex.get(), 255, 255, 255);
        SDL_RenderCopyExF(renderer, tex.get(), nullptr, &dst, 0.0, nullptr, SDL_FLIP_NONE);
    }

    const bool isLight = item->def.HasProperty(ItemProperty::LIGHT_SOURCE);
    const bool isFuel = item->def.HasProperty(ItemProperty::FUEL);

    if (isLight && item->def.maxDurability > 0) {
        const float ratio = static_cast<float>(item->durability) /
                            static_cast<float>(item->def.maxDurability);
        const float clampedRatio = std::max(0.0f, std::min(1.0f, ratio));

        const float gaugeY = y + kSlotSize - kGaugeHeight - 2.0f;
        const float gaugeInsetX = 3.0f;
        const float gaugeW = (kSlotSize - gaugeInsetX * 2.0f) * clampedRatio;

        Uint8 gR, gG, gB;
        if (clampedRatio > 0.6f) {
            gR = 70; gG = 180; gB = 80;
        } else if (clampedRatio > 0.25f) {
            gR = 210; gG = 170; gB = 50;
        } else {
            gR = 200; gG = 55; gB = 55;
        }

        SDL_SetRenderDrawColor(renderer, gR, gG, gB, 230);
        const SDL_FRect gaugeRect{x + gaugeInsetX, gaugeY, gaugeW, kGaugeHeight};
        SDL_RenderFillRectF(renderer, &gaugeRect);

        SDL_SetRenderDrawColor(renderer, 40, 40, 45, 230);
        const SDL_FRect gaugeBgRect{x + gaugeInsetX, gaugeY,
                                     kSlotSize - gaugeInsetX * 2.0f, kGaugeHeight};
        SDL_RenderDrawRectF(renderer, &gaugeBgRect);
    }

    if (!isFuel && !isLight) return;

    char durText[16];
    std::snprintf(durText, sizeof(durText), "%d", item->durability);

    auto font = Resources::GetFont("Recursos/font/TradeWinds-Regular.ttf", 12);
    if (!font) return;

    SDL_Color textColor{230, 225, 205, 240};
    SDL_Surface* surface = TTF_RenderText_Blended(font.get(), durText, textColor);
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    const float textW = static_cast<float>(surface->w);
    const float textH = static_cast<float>(surface->h);
    SDL_FreeSurface(surface);

    if (texture) {
        const float textX = x + kSlotSize - textW - 2.0f;
        const float textY = y + kSlotSize - textH - 2.0f;
        const SDL_FRect textDst{textX, textY, textW, textH};
        SDL_SetTextureAlphaMod(texture, 230);
        SDL_RenderCopyF(renderer, texture, nullptr, &textDst);
        SDL_DestroyTexture(texture);
    }
}

void InventoryGrid::DrawDraggedItem(SDL_Renderer* renderer) {
    if (dragSourceSlot < 0) return;

    const ItemInstance* item = inventory.GetItem(dragSourceSlot);
    if (!item) return;

    InputManager& input = InputManager::GetInstance();
    const float mx = static_cast<float>(input.GetMouseX());
    const float my = static_cast<float>(input.GetMouseY());

    const float iconSize = 56.0f;
    const float cx = mx - iconSize * 0.5f;
    const float cy = my - iconSize * 0.5f;

    auto tex = Resources::GetImage(item->def.spritePath);
    if (tex) {
        const SDL_FRect dst{cx, cy, iconSize, iconSize};
        SDL_SetTextureAlphaMod(tex.get(), 200);
        SDL_SetTextureColorMod(tex.get(), 255, 255, 255);
        SDL_RenderCopyExF(renderer, tex.get(), nullptr, &dst, 0.0, nullptr, SDL_FLIP_NONE);
    }
}
