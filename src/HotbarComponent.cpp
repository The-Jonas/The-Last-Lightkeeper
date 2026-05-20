#include "../include/HotbarComponent.h"
#include "../include/Item.h"
#include "../include/Camera.h"
#include "../include/Character.h"
#include "../include/Game.h"
#include "../include/InputManager.h"
#include "../include/ItemPickup.h"
#include "../include/Resources.h"
#include "../include/Sound.h"
#include "../include/Vec2.h"

#include <cmath>
#include <cstdlib>
#include <functional>
#include <algorithm>

namespace {
const char* kPickupSounds[] = {
    "Recursos/audio/pickup/zapsplat_foley_luggage_backpack_rucksack_grab_hard_001.mp3",
    "Recursos/audio/pickup/zapsplat_foley_luggage_backpack_rucksack_grab_hard_002.mp3",
    "Recursos/audio/pickup/zapsplat_foley_luggage_backpack_rucksack_grab_hard_003.mp3",
    "Recursos/audio/pickup/zapsplat_foley_luggage_backpack_rucksack_grab_hard_004.mp3",
    "Recursos/audio/pickup/zapsplat_foley_luggage_backpack_rucksack_grab_hard_005.mp3",
    "Recursos/audio/pickup/zapsplat_foley_luggage_backpack_rucksack_grab_hard_007.mp3",
};
constexpr int kPickupSoundCount = 6;

Sound gPickupSounds[kPickupSoundCount];
bool gPickupSoundsLoaded = false;

void PlayRandomPickupSound() {
    if (!gPickupSoundsLoaded) {
        for (int i = 0; i < kPickupSoundCount; i++) {
            gPickupSounds[i].Open(kPickupSounds[i]);
        }
        gPickupSoundsLoaded = true;
    }
    int idx = rand() % kPickupSoundCount;
    gPickupSounds[idx].Play();
}

bool ShowDurabilityOverlay(const ItemDef& def) {
    return def.HasProperty(ItemProperty::LIGHT_SOURCE);
}

float DurabilityRatio(const ItemInstance& item) {
    if (item.def.maxDurability <= 0) {
        return item.durability > 0 ? 1.0f : 0.0f;
    }
    const float t = static_cast<float>(item.durability) / static_cast<float>(item.def.maxDurability);
    return std::max(0.0f, std::min(1.0f, t));
}

void RenderItemInSlot(SDL_Renderer* renderer, const SDL_Rect& slotRect, const ItemInstance* item) {
    if (!renderer || !item) {
        return;
    }
    constexpr int kPad = 4;
    SDL_Rect inner{slotRect.x + kPad, slotRect.y + kPad, slotRect.w - kPad * 2, slotRect.h - kPad * 2};

    if (!ShowDurabilityOverlay(item->def)) {
        auto texOnly = Resources::GetImage(item->def.spritePath);
        if (texOnly) {
            SDL_RenderCopy(renderer, texOnly.get(), nullptr, &inner);
        }
        return;
    }

    const float ratio = DurabilityRatio(*item);
    const int fillH = static_cast<int>(static_cast<float>(inner.h) * ratio);

    if (ratio > 0.001f) {
        SDL_Rect fill{inner.x, inner.y + inner.h - fillH, inner.w, fillH};
        const float s = 1.0f - ratio;
        const Uint8 cr = static_cast<Uint8>(30.0f + s * 120.0f);
        const Uint8 cg = static_cast<Uint8>(140.0f + (1.0f - s) * 90.0f);
        const Uint8 cb = static_cast<Uint8>(45.0f + s * 40.0f);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, cr, cg, cb, 200);
        SDL_RenderFillRect(renderer, &fill);
    }

    auto tex = Resources::GetImage(item->def.spritePath);
    if (tex) {
        SDL_Rect dst = inner;
        SDL_RenderCopy(renderer, tex.get(), nullptr, &dst);
    }

    if (ratio <= 0.001f) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 200, 60, 60, 140);
        SDL_RenderFillRect(renderer, &inner);
    }
}
}

HotbarComponent::HotbarComponent(GameObject& associated, Inventory& inventory,
                                 Character* bigChar, Character** controlledChar,
                                 std::vector<ItemPickup*>& pickups,
                                 std::function<void(GameObject*)> addObjFn,
                                 std::function<Vec2(Vec2, float, float)> clampFn)
    : Component(associated), inventory(inventory), bigCharacter(bigChar),
      controlledCharacterPtr(controlledChar), itemPickups(pickups),
      addObjectToState(addObjFn), clampPickupTopLeft(std::move(clampFn)),
      inventoryOpen(false), dragSourceSlot(-1), isDragging(false),
      dragSprite(nullptr), toastTimer(0.0f) {
    for (int i = 0; i < Inventory::kSlots; i++) {
        slotSprites[i] = nullptr;
        invRects[i] = {0, 0, 0, 0};
    }
    for (int i = 0; i < Inventory::kHotbarSlots; i++) {
        hotbarRects[i] = {0, 0, 0, 0};
    }
    invBgRect = {0, 0, 0, 0};
}

void HotbarComponent::Start() {
    dragSprite = new Sprite();
    dragSprite->SetCameraFollower(true);
    dragSprite->SetTint(255, 255, 255, 180);
}

float HotbarComponent::GetPickupReachRadius() const {
    if (!bigCharacter) {
        return 0.0f;
    }
    return bigCharacter->GetFootCircleRadius() + kPickupPromptFootRadiusExtra;
}

bool HotbarComponent::BlocksLightPointerUnlock(int mx, int my) const {
    if (!bigCharacter || !controlledCharacterPtr || !*controlledCharacterPtr ||
        *controlledCharacterPtr != bigCharacter) {
        return false;
    }
    SDL_Point pt{mx, my};
    const int screenW = Game::GetInstance().GetWindowsWidth();
    const int screenH = Game::GetInstance().GetWindowsHeight();
    const int hotbarStripW = Inventory::kHotbarSlots * kSlotSize + (Inventory::kHotbarSlots - 1) * kSlotGap;
    const int totalW = kSlotSize + kUsingHotbarGap + hotbarStripW;
    const int barH = kSlotSize + 12;
    const int startX = (screenW - totalW) / 2;
    const int startY = screenH - barH;
    SDL_Rect bar{startX - 4, startY - 6, totalW + 8, barH + 2};
    if (SDL_PointInRect(&pt, &bar) == SDL_TRUE) {
        return true;
    }
    if (inventoryOpen) {
        const int fullTotalW = Inventory::kCols * kSlotSize + (Inventory::kCols - 1) * kSlotGap;
        const int fullTotalH = Inventory::kRows * kSlotSize + (Inventory::kRows - 1) * kSlotGap;
        const int invStartX = (screenW - fullTotalW) / 2;
        const int invStartY = (screenH - fullTotalH) / 2 - 30;
        SDL_Rect invBg{invStartX - 12, invStartY - 12, fullTotalW + 24, fullTotalH + 24};
        if (SDL_PointInRect(&pt, &invBg) == SDL_TRUE) {
            return true;
        }
    }
    return false;
}

void HotbarComponent::Update(float dt) {
    if (!controlledCharacterPtr || !*controlledCharacterPtr || !bigCharacter) {
        return;
    }
    if (*controlledCharacterPtr != bigCharacter) {
        if (inventoryOpen) {
            inventoryOpen = false;
        }
        if (isDragging) {
            isDragging = false;
            dragSourceSlot = -1;
            dragFromUsing = false;
            if (dragSprite) {
                dragSprite->Open("");
            }
        }
        return;
    }

    if (toastTimer > 0.0f) {
        toastTimer -= dt;
    }

    InputManager& input = InputManager::GetInstance();
    int mx = input.GetMouseX();
    int my = input.GetMouseY();

    if (input.KeyPress(SDLK_i)) {
        inventoryOpen = !inventoryOpen;
    }

    if (input.KeyPress(SDLK_e)) {
        ItemPickup* closest = nullptr;
        float closestDist = 1e30f;
        const Vec2 footCenter = bigCharacter->GetFootCircleCenter();
        const float reachR = GetPickupReachRadius();

        for (ItemPickup* p : itemPickups) {
            if (!p || p->GetAssociated().IsDead()) continue;
            Vec2 pCenter = p->GetCenter();
            float d = footCenter.Distance(pCenter);
            if (d <= reachR && d < closestDist) {
                closestDist = d;
                closest = p;
            }
        }

        if (closest) {
            if (!inventory.IsFull()) {
                const ItemDef& def = *closest->GetDef();
                inventory.AddItem(def, closest->GetDurability());
                for (auto& p : itemPickups) {
                    if (p == closest) { p = nullptr; break; }
                }
                closest->Destroy();
                PlayRandomPickupSound();
            } else {
                toastTimer = kToastDuration;
            }
        }
    }

    RecalcSlotRects();
    RecalcInvRect();

    if (isDragging) {
        if (!input.IsMouseDown(SDL_BUTTON_LEFT)) {
            const bool targetUsing = HitTestUsingSlot(mx, my);
            int targetSlot = -1;
            if (!targetUsing) {
                if (inventoryOpen) {
                    targetSlot = HitTestInvSlot(mx, my);
                    if (targetSlot < 0) {
                        targetSlot = HitTestHotbarSlot(mx, my);
                    }
                } else {
                    targetSlot = HitTestHotbarSlot(mx, my);
                }
            }
            HandleDragRelease(targetSlot, targetUsing);
        }
    } else {
        if (input.MousePress(SDL_BUTTON_LEFT)) {
            if (HitTestUsingSlot(mx, my) && !inventory.IsUsingEmpty()) {
                dragFromUsing = true;
                dragSourceSlot = -1;
                isDragging = true;
                const ItemInstance* u = inventory.GetUsing();
                if (u && dragSprite) {
                    dragSprite->Open(u->def.spritePath);
                }
            } else {
                int slot = -1;
                if (inventoryOpen) {
                    slot = HitTestInvSlot(mx, my);
                    if (slot < 0) slot = HitTestHotbarSlot(mx, my);
                } else {
                    slot = HitTestHotbarSlot(mx, my);
                }
                if (slot >= 0 && !inventory.IsEmpty(slot)) {
                    dragFromUsing = false;
                    dragSourceSlot = slot;
                    isDragging = true;
                    const ItemInstance* item = inventory.GetSlot(slot);
                    if (item && dragSprite) {
                        dragSprite->Open(item->def.spritePath);
                    }
                }
            }
        }
    }
}

void HotbarComponent::HandleDragRelease(int targetSlot, bool targetUsing) {
    const int mx = InputManager::GetInstance().GetMouseX();
    const int my = InputManager::GetInstance().GetMouseY();
    SDL_Point pt{mx, my};

    if (targetUsing) {
        if (!dragFromUsing) {
            inventory.SwapUsingAndSlot(dragSourceSlot);
            PlayRandomPickupSound();
        }
    } else if (targetSlot >= 0) {
        if (dragFromUsing) {
            inventory.SwapUsingAndSlot(targetSlot);
            PlayRandomPickupSound();
        } else if (targetSlot != dragSourceSlot) {
            if (!inventory.IsEmpty(targetSlot)) {
                inventory.SwapSlots(dragSourceSlot, targetSlot);
            } else {
                inventory.MoveItem(dragSourceSlot, targetSlot);
            }
            PlayRandomPickupSound();
        }
    } else {
        SDL_Rect barBounds;
        barBounds.x = static_cast<int>(associated.box.x);
        barBounds.y = static_cast<int>(associated.box.y);
        barBounds.w = static_cast<int>(associated.box.w);
        barBounds.h = static_cast<int>(associated.box.h);
        const bool inBar = SDL_PointInRect(&pt, &barBounds) == SDL_TRUE;
        const bool inInv = inventoryOpen && (SDL_PointInRect(&pt, &invBgRect) == SDL_TRUE);
        if (!inBar && !inInv) {
            constexpr float kPickW = 48.0f;
            constexpr float kPickH = 48.0f;
            if (dragFromUsing) {
                std::optional<ItemInstance> taken = inventory.TakeUsing();
                if (taken.has_value() && bigCharacter) {
                    Vec2 tl = bigCharacter->GetCenter();
                    tl.x -= kPickW * 0.5f;
                    tl.y -= kPickH * 0.5f;
                    if (clampPickupTopLeft) {
                        tl = clampPickupTopLeft(tl, kPickW, kPickH);
                    }
                    ItemInstance item = std::move(taken.value());
                    ItemPickup* dropped =
                        ItemPickup::Spawn(tl.x, tl.y, item.def, item.durability, itemPickups);
                    if (dropped && addObjectToState) {
                        addObjectToState(&dropped->GetAssociated());
                    }
                    PlayRandomPickupSound();
                }
            } else {
                const ItemInstance* item = inventory.GetSlot(dragSourceSlot);
                if (item && bigCharacter) {
                    Vec2 tl = bigCharacter->GetCenter();
                    tl.x -= kPickW * 0.5f;
                    tl.y -= kPickH * 0.5f;
                    if (clampPickupTopLeft) {
                        tl = clampPickupTopLeft(tl, kPickW, kPickH);
                    }
                    ItemPickup* dropped =
                        ItemPickup::Spawn(tl.x, tl.y, item->def, item->durability, itemPickups);
                    if (dropped && addObjectToState) {
                        addObjectToState(&dropped->GetAssociated());
                    }
                    inventory.DropItem(dragSourceSlot);
                    PlayRandomPickupSound();
                }
            }
        }
    }

    isDragging = false;
    dragSourceSlot = -1;
    dragFromUsing = false;
    if (dragSprite) {
        dragSprite->Open("");
    }
}

void HotbarComponent::Render() {
    if (!controlledCharacterPtr || !*controlledCharacterPtr || !bigCharacter ||
        *controlledCharacterPtr != bigCharacter) {
        return;
    }

    RecalcSlotRects();
    RecalcInvRect();

    SDL_Renderer* renderer = Game::GetInstance().GetRenderer();
    if (!renderer) return;

    SDL_Rect barRect;
    barRect.x = static_cast<int>(associated.box.x);
    barRect.y = static_cast<int>(associated.box.y);
    barRect.w = static_cast<int>(associated.box.w);
    barRect.h = static_cast<int>(associated.box.h);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 180);
    SDL_RenderFillRect(renderer, &barRect);

    SDL_SetRenderDrawColor(renderer, 220, 180, 40, 230);
    SDL_RenderDrawRect(renderer, &usingSlotRect);

    if (!inventory.IsUsingEmpty()) {
        const ItemInstance* u = inventory.GetUsing();
        RenderItemInSlot(renderer, usingSlotRect, u);
    }

    for (int i = 0; i < Inventory::kHotbarSlots; i++) {
        const SDL_Rect& r = hotbarRects[i];
        if (inventory.IsEmpty(i)) {
            SDL_SetRenderDrawColor(renderer, 40, 40, 40, 200);
        } else {
            SDL_SetRenderDrawColor(renderer, 80, 80, 80, 200);
        }
        SDL_RenderDrawRect(renderer, &r);

        const ItemInstance* item = inventory.GetSlot(i);
        if (item) {
            RenderItemInSlot(renderer, r, item);
        }
    }

    auto font = Resources::GetFont("Recursos/font/TradeWinds-Regular.ttf", 14);
    const Vec2 footCenter = bigCharacter->GetFootCircleCenter();
    const float promptR = GetPickupReachRadius();
    for (ItemPickup* p : itemPickups) {
        if (!p || p->GetAssociated().IsDead()) continue;
        float d = footCenter.Distance(p->GetCenter());
        if (d <= promptR) {
            Vec2 worldPos = p->GetCenter();
            float sx = (worldPos.x - Camera::pos.x) * Camera::GetZoom();
            float sy = (worldPos.y - Camera::pos.y) * Camera::GetZoom();
            const char* label = "Press E";
            if (font) {
                SDL_Color col{255, 255, 255, 220};
                SDL_Surface* s = TTF_RenderText_Blended(font.get(), label, col);
                if (s) {
                    SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                    SDL_Rect dr = {static_cast<int>(sx - s->w / 2.0f),
                                   static_cast<int>(sy - 56.0f), s->w, s->h};
                    SDL_FreeSurface(s);
                    if (t) {
                        SDL_RenderCopy(renderer, t, nullptr, &dr);
                        SDL_DestroyTexture(t);
                    }
                }
            }
        }
    }

    if (inventoryOpen) {
        RenderInvPopup(renderer);
    }

    if (isDragging && dragSprite && dragSprite->IsOpen()) {
        int mx = InputManager::GetInstance().GetMouseX();
        int my = InputManager::GetInstance().GetMouseY();
        dragSprite->Render(mx - kSlotSize / 2, my - kSlotSize / 2, kSlotSize, kSlotSize);
    }

    if (toastTimer > 0.0f) {
        SDL_Color color{255, 80, 80, 255};
        const char* msg = "Inventory full!";
        auto ttfFont = Resources::GetFont("Recursos/font/TradeWinds-Regular.ttf", 18);
        if (ttfFont) {
            SDL_Surface* surf = TTF_RenderText_Blended(ttfFont.get(), msg, color);
            if (surf) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                int tw = surf->w;
                int th = surf->h;
                SDL_FreeSurface(surf);
                if (tex) {
                    SDL_Rect dst = {(Game::GetInstance().GetWindowsWidth() - tw) / 2,
                                    Game::GetInstance().GetWindowsHeight() - 110, tw, th};
                    SDL_RenderCopy(renderer, tex, nullptr, &dst);
                    SDL_DestroyTexture(tex);
                }
            }
        }
    }
}

void HotbarComponent::RenderInvPopup(SDL_Renderer* renderer) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 10, 10, 15, 220);
    SDL_RenderFillRect(renderer, &invBgRect);

    for (int i = 0; i < Inventory::kSlots; i++) {
        const SDL_Rect& r = invRects[i];
        if (i < Inventory::kHotbarSlots) {
            SDL_SetRenderDrawColor(renderer, 220, 180, 40, 200);
        } else if (inventory.IsEmpty(i)) {
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 200);
        } else {
            SDL_SetRenderDrawColor(renderer, 80, 80, 80, 200);
        }
        SDL_RenderDrawRect(renderer, &r);

        const ItemInstance* item = inventory.GetSlot(i);
        if (item) {
            RenderItemInSlot(renderer, r, item);
        }
    }
}

void HotbarComponent::RecalcSlotRects() {
    int screenW = Game::GetInstance().GetWindowsWidth();
    int screenH = Game::GetInstance().GetWindowsHeight();
    const int hotbarStripW = Inventory::kHotbarSlots * kSlotSize + (Inventory::kHotbarSlots - 1) * kSlotGap;
    const int totalBarW = kSlotSize + kUsingHotbarGap + hotbarStripW;
    int barH = kSlotSize + 12;
    int startX = (screenW - totalBarW) / 2;
    int startY = screenH - barH;

    usingSlotRect.x = startX;
    usingSlotRect.y = startY + 6;
    usingSlotRect.w = kSlotSize;
    usingSlotRect.h = kSlotSize;

    associated.box.x = static_cast<float>(startX - 4);
    associated.box.y = static_cast<float>(startY - 6);
    associated.box.w = static_cast<float>(totalBarW + 8);
    associated.box.h = static_cast<float>(barH + 2);

    const int hotbarStartX = startX + kSlotSize + kUsingHotbarGap;
    for (int i = 0; i < Inventory::kHotbarSlots; i++) {
        hotbarRects[i].x = hotbarStartX + i * (kSlotSize + kSlotGap);
        hotbarRects[i].y = startY + 6;
        hotbarRects[i].w = kSlotSize;
        hotbarRects[i].h = kSlotSize;
    }

    int fullTotalW = Inventory::kCols * kSlotSize + (Inventory::kCols - 1) * kSlotGap;
    int fullTotalH = Inventory::kRows * kSlotSize + (Inventory::kRows - 1) * kSlotGap;
    int invStartX = (screenW - fullTotalW) / 2;
    int invStartY = (screenH - fullTotalH) / 2 - 30;

    invBgRect.x = invStartX - 12;
    invBgRect.y = invStartY - 12;
    invBgRect.w = fullTotalW + 24;
    invBgRect.h = fullTotalH + 24;

    for (int r = 0; r < Inventory::kRows; r++) {
        for (int c = 0; c < Inventory::kCols; c++) {
            int idx = r * Inventory::kCols + c;
            invRects[idx].x = invStartX + c * (kSlotSize + kSlotGap);
            invRects[idx].y = invStartY + r * (kSlotSize + kSlotGap);
            invRects[idx].w = kSlotSize;
            invRects[idx].h = kSlotSize;
        }
    }
}

void HotbarComponent::RecalcInvRect() {
    RecalcSlotRects();
}

int HotbarComponent::HitTestHotbarSlot(int mouseX, int mouseY) const {
    SDL_Point pt{mouseX, mouseY};
    for (int i = 0; i < Inventory::kHotbarSlots; i++) {
        if (SDL_PointInRect(&pt, &hotbarRects[i])) {
            return i;
        }
    }
    return -1;
}

bool HotbarComponent::HitTestUsingSlot(int mouseX, int mouseY) const {
    SDL_Point pt{mouseX, mouseY};
    return SDL_PointInRect(&pt, &usingSlotRect) == SDL_TRUE;
}

int HotbarComponent::HitTestInvSlot(int mouseX, int mouseY) const {
    SDL_Point pt{mouseX, mouseY};
    for (int i = 0; i < Inventory::kSlots; i++) {
        if (SDL_PointInRect(&pt, &invRects[i])) {
            return i;
        }
    }
    return -1;
}

void HotbarComponent::UpdateSlotSprites() {
    (void)slotSprites;
}

void HotbarComponent::ClearSlotSprites() {
}
