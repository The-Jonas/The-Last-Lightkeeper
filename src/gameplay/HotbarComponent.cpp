#include "gameplay/HotbarComponent.h"
#include "gameplay/Item.h"
#include "engine/Camera.h"
#include "gameplay/Character.h"
#include "core/Game.h"
#include "core/InputManager.h"
#include "gameplay/ItemPickup.h"
#include "core/Resources.h"
#include "audio/Sound.h"
#include "math/Vec2.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <vector>
#include <iostream>

namespace {

void FillCircle(SDL_Renderer* r, float cx, float cy, float rad, Uint8 rr, Uint8 gg, Uint8 bb, Uint8 aa);

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
    // Círculo inscrito no quadrado do slot: preenchimento de durabilidade e overlay usam o slot inteiro (não o inner).
    const float cx = static_cast<float>(slotRect.x) + static_cast<float>(slotRect.w) * 0.5f;
    const float cy = static_cast<float>(slotRect.y) + static_cast<float>(slotRect.h) * 0.5f;
    const float rad =
        0.5f * static_cast<float>((std::min)(slotRect.w, slotRect.h));

    if (ratio > 0.001f) {
        const int fillH = static_cast<int>(static_cast<float>(slotRect.h) * ratio);
        SDL_Rect clipBand{slotRect.x, slotRect.y + slotRect.h - fillH, slotRect.w, fillH};
        SDL_RenderSetClipRect(renderer, &clipBand);
        const float s = 1.0f - ratio;
        const Uint8 cr = static_cast<Uint8>(30.0f + s * 120.0f);
        const Uint8 cg = static_cast<Uint8>(140.0f + (1.0f - s) * 90.0f);
        const Uint8 cb = static_cast<Uint8>(45.0f + s * 40.0f);
        FillCircle(renderer, cx, cy, rad, cr, cg, cb, 200);
        SDL_RenderSetClipRect(renderer, nullptr);
    }

    auto tex = Resources::GetImage(item->def.spritePath);
    if (tex) {
        SDL_Rect dst = inner;
        SDL_RenderCopy(renderer, tex.get(), nullptr, &dst);
    }

    if (ratio <= 0.001f) {
        FillCircle(renderer, cx, cy, rad, 200, 60, 60, 140);
    }
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void FillCircle(SDL_Renderer* r, float cx, float cy, float rad, Uint8 rr, Uint8 gg, Uint8 bb, Uint8 aa) {
    if (!r || rad < 1.0f) {
        return;
    }
    constexpr int kSeg = 32;
    std::vector<SDL_Vertex> verts;
    std::vector<int> ind;
    verts.reserve(static_cast<size_t>(kSeg + 2));
    ind.reserve(static_cast<size_t>(kSeg * 3));
    verts.push_back({{cx, cy}, {rr, gg, bb, aa}, {0, 0}});
    for (int i = 0; i <= kSeg; i++) {
        const float t = (static_cast<float>(i) / static_cast<float>(kSeg)) * 2.0f * static_cast<float>(M_PI);
        verts.push_back({{cx + std::cos(t) * rad, cy + std::sin(t) * rad}, {rr, gg, bb, aa}, {0, 0}});
    }
    for (int i = 0; i < kSeg; i++) {
        ind.push_back(0);
        ind.push_back(1 + i);
        ind.push_back(2 + i);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(r, nullptr, verts.data(), static_cast<int>(verts.size()), ind.data(), static_cast<int>(ind.size()));
}

void StrokeCircle(SDL_Renderer* r, float cx, float cy, float rad, Uint8 rr, Uint8 gg, Uint8 bb, Uint8 aa) {
    if (!r || rad < 1.0f) {
        return;
    }
    constexpr int kSeg = 40;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, rr, gg, bb, aa);
    for (int i = 0; i < kSeg; i++) {
        const float t0 = (static_cast<float>(i) / static_cast<float>(kSeg)) * 2.0f * static_cast<float>(M_PI);
        const float t1 = (static_cast<float>(i + 1) / static_cast<float>(kSeg)) * 2.0f * static_cast<float>(M_PI);
        const int x0 = static_cast<int>(cx + std::cos(t0) * rad);
        const int y0 = static_cast<int>(cy + std::sin(t0) * rad);
        const int x1 = static_cast<int>(cx + std::cos(t1) * rad);
        const int y1 = static_cast<int>(cy + std::sin(t1) * rad);
        SDL_RenderDrawLine(r, x0, y0, x1, y1);
    }
}

} // namespace

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
    }
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

bool HotbarComponent::PointInCircle(int mx, int my, float cx, float cy, float radius) const {
    const float dx = static_cast<float>(mx) - cx;
    const float dy = static_cast<float>(my) - cy;
    return dx * dx + dy * dy <= radius * radius;
}

SDL_Rect HotbarComponent::SlotRectAtCenter(float cx, float cy, int size) const {
    const int half = size / 2;
    return {static_cast<int>(cx) - half, static_cast<int>(cy) - half, size, size};
}

bool HotbarComponent::BlocksLightPointerUnlock(int mx, int my) const {
    if (!bigCharacter || !controlledCharacterPtr || !*controlledCharacterPtr ||
        *controlledCharacterPtr != bigCharacter) {
        return false;
    }
    if (inventoryOpen) {
        return true;
    }
    return PointInCircle(mx, my, closedUseCenterX, closedUseCenterY, kWheelHitRadius);
}

void HotbarComponent::RecalcLayout() {
    const int screenW = Game::GetInstance().GetWindowsWidth();
    const int screenH = Game::GetInstance().GetWindowsHeight();
    const int barH = HotbarComponent::kSlotSize + 12;

    closedUseCenterX = static_cast<float>(screenW) * 0.5f;
    closedUseCenterY = static_cast<float>(screenH - barH) + static_cast<float>(HotbarComponent::kSlotSize) * 0.5f;

    usingSlotRect = SlotRectAtCenter(closedUseCenterX, closedUseCenterY, HotbarComponent::kSlotSize);

    wheelCenterX = static_cast<float>(screenW) * 0.5f;
    wheelCenterY = static_cast<float>(screenH) * 0.5f;

    constexpr float kTwoPi = 2.0f * static_cast<float>(M_PI);
    for (int i = 0; i < HotbarComponent::kWheelRingSlots; i++) {
        const float ang =
            -static_cast<float>(M_PI) * 0.5f + static_cast<float>(i) * (kTwoPi / static_cast<float>(HotbarComponent::kWheelRingSlots));
        wheelRingCenterX[i] = wheelCenterX + std::cos(ang) * kWheelRadiusPx;
        wheelRingCenterY[i] = wheelCenterY + std::sin(ang) * kWheelRadiusPx;
    }

    if (inventoryOpen) {
        associated.box.x = 0.0f;
        associated.box.y = 0.0f;
        associated.box.w = static_cast<float>(screenW);
        associated.box.h = static_cast<float>(screenH);
    } else {
        const float pad = 12.0f;
        associated.box.x = closedUseCenterX - kWheelHitRadius - pad;
        associated.box.y = closedUseCenterY - kWheelHitRadius - pad;
        associated.box.w = (kWheelHitRadius + pad) * 2.0f;
        associated.box.h = (kWheelHitRadius + pad) * 2.0f;
    }
}

int HotbarComponent::HitTestInventorySlot(int mx, int my) const {
    if (inventoryOpen) {
        if (PointInCircle(mx, my, wheelCenterX, wheelCenterY, kWheelHitRadius)) {
            return kWheelCenterIndex;
        }
        for (int i = 0; i < HotbarComponent::kWheelRingSlots; i++) {
            if (PointInCircle(mx, my, wheelRingCenterX[i], wheelRingCenterY[i], kWheelHitRadius)) {
                return i;
            }
        }
        return -1;
    }
    if (PointInCircle(mx, my, closedUseCenterX, closedUseCenterY, kWheelHitRadius)) {
        return kWheelCenterIndex;
    }
    return -1;
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
    const int mx = input.GetMouseX();
    const int my = input.GetMouseY();

    RecalcLayout();

    if (input.KeyPress(SDLK_q)) {
        inventory.isLightToggledOn = !inventory.isLightToggledOn;
    }

    if (input.KeyPress(SDLK_TAB)) {
        inventoryOpen = !inventoryOpen;
    }

    if (input.KeyPress(SDLK_e)) {
        ItemPickup* closest = nullptr;
        float closestDist = 1e30f;

        // Pega a Mão projetada do personagem
        Vec2 playerCenter = bigCharacter->GetCenter();

        // Procura os Itens
        for (ItemPickup* p : itemPickups) {
            if (!p || p->GetAssociated().IsDead()) {
                continue;
            }

            // Descobre a altura DESTE item especifico
            int itemHeight = p->GetHeightLevel();

            // Pede pro personagem gerar a mão EXATAMENTE pra essa altura
            SDL_Rect reachBox = Character::player->GetInteractionRect(itemHeight);

            GameObject& itemObj = p->GetAssociated();
            SDL_Rect itemRect = {
                static_cast<int>(itemObj.box.x),
                static_cast<int>(itemObj.box.y),
                static_cast<int>(itemObj.box.w),
                static_cast<int>(itemObj.box.h)
            };
            
            if (SDL_HasIntersection(&reachBox, &itemRect)) {
                float d = playerCenter.Distance(p->GetCenter());
                if (d < closestDist) {
                    closestDist = d;
                    closest = p;
                }
            }
        }   

        if (closest) {
            int hLevel = closest->GetHeightLevel();

            // =====================================
            // NÍVEIS 0 e 1: O Irmãozão pega sozinho
            // =====================================

            if (hLevel == 0 || hLevel == 1) {
                std::cout << "PEGANDO SOZINHO: Altura lida foi " << hLevel << std::endl;
                if (!inventory.IsFull()) {
                    const ItemDef& def = *closest->GetDef();
                    inventory.AddItem(def, closest->GetDurability());
                    for (auto& p : itemPickups) {
                        if (p == closest) {
                            p = nullptr;
                            break;
                        }
                    }
                    closest->Destroy();
                    PlayRandomPickupSound();

                    // Congela o Irmãozão rapidinho pra animação de pegar (0.2 segundos)
                    Character::player->currentState = Character::ActionState::INTERACTING;
                    Character::player->interactTimer = 0.2f;
                }
                else {
                    // Inventário cheio
                    toastTimer = kToastDuration;
                }
            }

            // =====================================
            // NÍVEL 2: Precisamos do Irmãozinho
            // =====================================

            else if (hLevel == 2) {
                if (Character::littleBrother) {
                    // Checa a distância entre os dois irmãos
                    float distBrothers = Character::player->GetFootCircleCenter().Distance(Character::littleBrother->GetFootCircleCenter());

                    if (distBrothers <= 170.0f) {
                        if (!inventory.IsFull()) {

                            // Congela os dois personagens pelo tempo da animação (1.5 segundos)
                            Character::player->currentState = Character::ActionState::INTERACTING;
                            Character::player->interactTimer = 1.5f;
                            
                            Character::littleBrother->currentState = Character::ActionState::INTERACTING;
                            Character::littleBrother->interactTimer = 1.5f;

                            // Alinha o irmãozinho na frente do irmãozão (Teleporte leve)
                            Character::littleBrother->PositionForCoop(Character::player);

                            // Pega o item e bota no inventário do Irmãozão
                            const ItemDef& def = *closest->GetDef();
                            inventory.AddItem(def, closest->GetDurability());

                            for (auto& p : itemPickups) {
                                if (p == closest) {
                                    p = nullptr;
                                    break;
                                }
                            }
                            closest->Destroy();
                            PlayRandomPickupSound();
                            std::cout << "[CO-OP] Irmaos trabalharam juntos e pegaram o item no alto!" << std::endl;
                        }
                        else { 
                            toastTimer = kToastDuration;
                        }
                    }
                    else {
                        // FALHA: Irmãozinho muito longe
                        std::cout << "[FALHA] O irmaozinho esta muito longe para ajudar!" << std::endl;
                        std::cout << "Distancia entre os irmaos: " << distBrothers << std::endl;
                    }
                }
            }
        }
    }

    if (inventoryOpen && input.MousePress(SDL_BUTTON_LEFT)) {
        if (HitTestInventorySlot(mx, my) < 0) {
            inventoryOpen = false;
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
    }

    if (isDragging) {
        if (!input.IsMouseDown(SDL_BUTTON_LEFT)) {
            const int hit = HitTestInventorySlot(mx, my);
            HandleDragRelease(hit);
        }
    } else {
        if (input.MousePress(SDL_BUTTON_LEFT)) {
            const int hit = HitTestInventorySlot(mx, my);
            if (hit == kWheelCenterIndex && !inventory.IsUsingEmpty()) {
                dragFromUsing = true;
                dragSourceSlot = -1;
                isDragging = true;
                const ItemInstance* u = inventory.GetUsing();
                if (u && dragSprite) {
                    dragSprite->Open(u->def.spritePath);
                }
            } else if (hit >= 0 && hit < HotbarComponent::kWheelRingSlots && !inventory.IsEmpty(hit)) {
                dragFromUsing = false;
                dragSourceSlot = hit;
                isDragging = true;
                const ItemInstance* item = inventory.GetSlot(hit);
                if (item && dragSprite) {
                    dragSprite->Open(item->def.spritePath);
                }
            }
        }
    }
}

void HotbarComponent::HandleDragRelease(int targetWheelIndex) {

    // ============================================================
    // SISTEMA DE REABASTECIMENTO (ARRASTAR COMBUSTÍVEL PARA LUZ)
    // ============================================================

    bool isTargetingSlot = (targetWheelIndex >= 0 && targetWheelIndex < HotbarComponent::kWheelRingSlots);
    bool isTargetingUsing = (targetWheelIndex == kWheelCenterIndex);

    if (isTargetingSlot || isTargetingUsing) {
        // Pegamos os ponteiros mutáveis para podermos alterar a durabilidade
        ItemInstance* draggedItem = dragFromUsing ? inventory.GetUsingMutable() : inventory.GetSlotMutable(dragSourceSlot);
        ItemInstance* targetItem = isTargetingUsing ? inventory.GetUsingMutable() : inventory.GetSlotMutable(targetWheelIndex);
        
        if (draggedItem && targetItem && draggedItem != targetItem) {
            bool draggedIsFuel = draggedItem->def.HasProperty(ItemProperty::FUEL);
            bool targetIsLight = targetItem->def.HasProperty(ItemProperty::LIGHT_SOURCE);
            
            if (draggedIsFuel && targetIsLight) {
                // Descobre quanto de combustível esse galão dá (ex: 50.0)
                float fuelValue = 50.0f; 
                for (const auto& p : draggedItem->def.properties) {
                    if (p.first == ItemProperty::FUEL) fuelValue = p.second;
                }
                
                // Reabastece a luz alvo e limita para não passar da carga máxima (100)
                targetItem->durability += static_cast<int>(fuelValue);
                if (targetItem->durability > targetItem->def.maxDurability) {
                    targetItem->durability = targetItem->def.maxDurability;
                }
                
                // Destroi o galão de combustível do inventário
                if (dragFromUsing) inventory.ClearUsing();
                else inventory.RemoveItem(dragSourceSlot);
                
                PlayRandomPickupSound(); // No futuro por um som de glub glub
                
                // Limpa o estado do cursor e sai (para não rodar a lógica de trocar os itens de lugar)
                isDragging = false;
                dragSourceSlot = -1;
                dragFromUsing = false;
                if (dragSprite) dragSprite->Open("");
                return; 
            }
        }
    }


    // ============================================================
    // SISTEMA NORMAL DE TROCA E DROP 
    // ============================================================

    if (targetWheelIndex == kWheelCenterIndex) {
        if (!dragFromUsing) {
            inventory.SwapUsingAndSlot(dragSourceSlot);
            PlayRandomPickupSound();
        }
    } else if (targetWheelIndex >= 0 && targetWheelIndex < HotbarComponent::kWheelRingSlots) {
        if (dragFromUsing) {
            inventory.SwapUsingAndSlot(targetWheelIndex);
            PlayRandomPickupSound();
        } else if (targetWheelIndex != dragSourceSlot) {
            if (!inventory.IsEmpty(targetWheelIndex)) {
                inventory.SwapSlots(dragSourceSlot, targetWheelIndex);
            } else {
                inventory.MoveItem(dragSourceSlot, targetWheelIndex);
            }
            PlayRandomPickupSound();
        }
    } else {
        const float kPickW = static_cast<float>(kSlotSize);
        const float kPickH = static_cast<float>(kSlotSize);
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

    RecalcLayout();

    SDL_Renderer* renderer = Game::GetInstance().GetRenderer();
    if (!renderer) {
        return;
    }

    if (inventoryOpen) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 5, 8, 14, 200);
        SDL_Rect full{0, 0, Game::GetInstance().GetWindowsWidth(), Game::GetInstance().GetWindowsHeight()};
        SDL_RenderFillRect(renderer, &full);

        for (int i = 0; i < HotbarComponent::kWheelRingSlots; i++) {
            FillCircle(renderer, wheelRingCenterX[i], wheelRingCenterY[i], kWheelSlotDrawRadius, 28, 32, 42, 220);
            StrokeCircle(renderer, wheelRingCenterX[i], wheelRingCenterY[i], kWheelSlotDrawRadius, 200, 200, 220, 200);
            const SDL_Rect r = SlotRectAtCenter(wheelRingCenterX[i], wheelRingCenterY[i], kSlotSize);
            const ItemInstance* item = inventory.GetSlot(i);
            if (item) {
                RenderItemInSlot(renderer, r, item);
            }
        }

        FillCircle(renderer, wheelCenterX, wheelCenterY, kWheelSlotDrawRadius * 1.05f, 50, 42, 18, 235);
        StrokeCircle(renderer, wheelCenterX, wheelCenterY, kWheelSlotDrawRadius * 1.05f, 240, 200, 80, 245);
        {
            const SDL_Rect r = SlotRectAtCenter(wheelCenterX, wheelCenterY, kSlotSize);
            if (!inventory.IsUsingEmpty()) {
                const ItemInstance* u = inventory.GetUsing();
                RenderItemInSlot(renderer, r, u);
            }
        }
    } else {
        FillCircle(renderer, closedUseCenterX, closedUseCenterY, kWheelSlotDrawRadius, 24, 28, 36, 210);
        StrokeCircle(renderer, closedUseCenterX, closedUseCenterY, kWheelSlotDrawRadius, 220, 190, 70, 240);
        if (!inventory.IsUsingEmpty()) {
            const ItemInstance* u = inventory.GetUsing();
            RenderItemInSlot(renderer, usingSlotRect, u);
        }
    }

    auto font = Resources::GetFont("Recursos/font/TradeWinds-Regular.ttf", 14);

    for (ItemPickup* p : itemPickups) {
        if (!p || p->GetAssociated().IsDead()) continue;
                
        int itemHeight = p->GetHeightLevel();

        // Pede pro personagem gerar a mão EXATAMENTE pra essa altura
        SDL_Rect reachBox = Character::player->GetInteractionRect(itemHeight);

        GameObject& itemObj = p->GetAssociated();
        SDL_Rect itemRect = {
            static_cast<int>(itemObj.box.x),
            static_cast<int>(itemObj.box.y),
            static_cast<int>(itemObj.box.w),
            static_cast<int>(itemObj.box.h)
        };
        
        if (SDL_HasIntersection(&reachBox, &itemRect)) {
            Vec2 worldPos = p->GetCenter();
        
            float sx = (worldPos.x - Camera::pos.x) * Camera::GetZoom();
            float sy = (worldPos.y - Camera::pos.y) * Camera::GetZoom();
            const char* label = "Press E";
            if (font) {
                SDL_Color col{255, 255, 255, 220};
                SDL_Surface* s = TTF_RenderText_Blended(font.get(), label, col);
                if (s) {
                    SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                    SDL_Rect dr = {static_cast<int>(sx - s->w / 2.0f), static_cast<int>(sy - 56.0f), s->w, s->h};
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
        auto hintFont = Resources::GetFont("Recursos/font/TradeWinds-Regular.ttf", 16);
        if (hintFont) {
            const char* hint = "Centro: item em uso (atalho)   TAB: fechar   Arraste para o centro para equipar";
            SDL_Color hc{230, 225, 200, 240};
            SDL_Surface* surf = TTF_RenderText_Blended(hintFont.get(), hint, hc);
            if (surf) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                const int tw = surf->w;
                const int th = surf->h;
                SDL_FreeSurface(surf);
                if (tex) {
                    SDL_Rect dst = {(Game::GetInstance().GetWindowsWidth() - tw) / 2,
                                    static_cast<int>(wheelCenterY) - static_cast<int>(kWheelRadiusPx) - 48, tw, th};
                    SDL_RenderCopy(renderer, tex, nullptr, &dst);
                    SDL_DestroyTexture(tex);
                }
            }
        }
    }

    if (isDragging && dragSprite && dragSprite->IsOpen()) {
        const int mcx = InputManager::GetInstance().GetMouseX();
        const int mcy = InputManager::GetInstance().GetMouseY();
        dragSprite->Render(mcx - kSlotSize / 2, mcy - kSlotSize / 2, kSlotSize, kSlotSize);
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

#ifdef DEBUG
    // Desenha as Hitboxes de Interação (Níveis 0, 1 e 2) para teste
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (int h = 0; h <= 2; h++) {
        SDL_Rect rBox = bigCharacter->GetInteractionRect(h);
        
        SDL_FRect screenReachBox = {
            (rBox.x - Camera::pos.x) * Camera::GetZoom(),
            (rBox.y - Camera::pos.y) * Camera::GetZoom(),
            rBox.w * Camera::GetZoom(),
            rBox.h * Camera::GetZoom()
        };

        // Vamos colorir cada andar de uma cor para ficar fácil de entender:
        if (h == 0) {
            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 150); // Verde (Chão)
        } else if (h == 1) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 150); // Amarelo (Mesa/Peito)
        } else {
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 150); // Vermelho (Armário/Cabeça)
        }

        SDL_RenderDrawRectF(renderer, &screenReachBox);
    }
#endif
}
