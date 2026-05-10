#include "../include/HotbarComponent.h"
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
}

HotbarComponent::HotbarComponent(GameObject& associated, Inventory& inventory,
                                 Character* bigChar, Character** controlledChar,
                                 std::vector<ItemPickup*>& pickups,
                                 std::function<void(GameObject*)> addObjFn)
    : Component(associated), inventory(inventory), bigCharacter(bigChar),
      controlledCharacterPtr(controlledChar), itemPickups(pickups),
      addObjectToState(addObjFn),
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

void HotbarComponent::Update(float dt) {
    if (!controlledCharacterPtr || *controlledCharacterPtr != bigCharacter) {
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
        float closestDist = kPickupRange;
        Vec2 bigCenter = bigCharacter->GetCenter();

        for (ItemPickup* p : itemPickups) {
            if (!p || p->GetAssociated().IsDead()) continue;
            Vec2 pCenter = p->GetCenter();
            float d = bigCenter.Distance(pCenter);
            if (d < closestDist) {
                closestDist = d;
                closest = p;
            }
        }

        if (closest) {
            if (!inventory.IsFull()) {
                const ItemDef& def = *closest->GetDef();
                inventory.AddItem(def, def.maxDurability);
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
            int targetSlot = -1;
            if (inventoryOpen) {
                targetSlot = HitTestInvSlot(mx, my);
                if (targetSlot < 0) {
                    targetSlot = HitTestHotbarSlot(mx, my);
                }
            } else {
                targetSlot = HitTestHotbarSlot(mx, my);
            }
            HandleDragRelease(targetSlot);
        }
    } else {
        if (input.MousePress(SDL_BUTTON_LEFT)) {
            int slot = -1;
            if (inventoryOpen) {
                slot = HitTestInvSlot(mx, my);
                if (slot < 0) slot = HitTestHotbarSlot(mx, my);
            } else {
                slot = HitTestHotbarSlot(mx, my);
            }
            if (slot >= 0 && !inventory.IsEmpty(slot)) {
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

void HotbarComponent::HandleDragRelease(int targetSlot) {
    if (targetSlot >= 0 && targetSlot != dragSourceSlot) {
        if (!inventory.IsEmpty(targetSlot)) {
            inventory.SwapSlots(dragSourceSlot, targetSlot);
        } else {
            inventory.MoveItem(dragSourceSlot, targetSlot);
        }
        PlayRandomPickupSound();
    } else if (targetSlot < 0) {
        bool outsideBar = true;
        if (!inventoryOpen) {
            SDL_Rect bar;
            bar.x = static_cast<int>(associated.box.x);
            bar.y = static_cast<int>(associated.box.y);
            bar.w = static_cast<int>(associated.box.w);
            bar.h = static_cast<int>(associated.box.h);
            SDL_Point pt{InputManager::GetInstance().GetMouseX(), InputManager::GetInstance().GetMouseY()};
            outsideBar = !SDL_PointInRect(&pt, &bar);
        }
        if (outsideBar) {
            const ItemInstance* item = inventory.GetSlot(dragSourceSlot);
            if (item) {
                Vec2 dropPos = bigCharacter->GetCenter();
                ItemPickup* dropped = ItemPickup::Spawn(dropPos.x, dropPos.y, item->def, item->durability, itemPickups);
                if (dropped && addObjectToState) {
                    addObjectToState(&dropped->GetAssociated());
                }
                inventory.DropItem(dragSourceSlot);
                PlayRandomPickupSound();
            }
        }
    }
    isDragging = false;
    dragSourceSlot = -1;
    if (dragSprite) {
        dragSprite->Open("");
    }
}

void HotbarComponent::Render() {
    if (!controlledCharacterPtr || *controlledCharacterPtr != bigCharacter) {
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
            auto tex = Resources::GetImage(item->def.spritePath);
            if (tex) {
                int pad = 4;
                SDL_Rect dst = {r.x + pad, r.y + pad, r.w - pad * 2, r.h - pad * 2};
                SDL_RenderCopy(renderer, tex.get(), nullptr, &dst);
            }
        }
    }

    auto font = Resources::GetFont("Recursos/font/TradeWinds-Regular.ttf", 14);
    Vec2 bigCenter = bigCharacter->GetCenter();
    for (ItemPickup* p : itemPickups) {
        if (!p || p->GetAssociated().IsDead()) continue;
        float d = bigCenter.Distance(p->GetCenter());
        if (d <= kPickupRange) {
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
            auto tex = Resources::GetImage(item->def.spritePath);
            if (tex) {
                int pad = 4;
                SDL_Rect dst = {r.x + pad, r.y + pad, r.w - pad * 2, r.h - pad * 2};
                SDL_RenderCopy(renderer, tex.get(), nullptr, &dst);
            }
        }
    }
}

void HotbarComponent::RecalcSlotRects() {
    int screenW = Game::GetInstance().GetWindowsWidth();
    int screenH = Game::GetInstance().GetWindowsHeight();
    int totalW = Inventory::kHotbarSlots * kSlotSize + (Inventory::kHotbarSlots - 1) * kSlotGap;
    int barH = kSlotSize + 12;
    int startX = (screenW - totalW) / 2;
    int startY = screenH - barH;

    associated.box.x = static_cast<float>(startX - 4);
    associated.box.y = static_cast<float>(startY - 6);
    associated.box.w = static_cast<float>(totalW + 8);
    associated.box.h = static_cast<float>(barH + 2);

    for (int i = 0; i < Inventory::kHotbarSlots; i++) {
        hotbarRects[i].x = startX + i * (kSlotSize + kSlotGap);
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
