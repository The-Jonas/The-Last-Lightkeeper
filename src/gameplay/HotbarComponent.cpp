#include "gameplay/HotbarComponent.h"
#include "core/Game.h"
#include "core/InputManager.h"
#include "gameplay/Box.h"
#include "gameplay/Character.h"
#include "gameplay/ItemPickup.h"
#include "states/stage/StageState.h"
#include "ui/InventoryGrid.h"
#include "audio/Sound.h"
#include "audio/GameSfx.h"

#include <cstdlib>
#include <iostream>

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
    const int idx = rand() % kPickupSoundCount;
    gPickupSounds[idx].Play();
}

void PerformPickup(Inventory& inventory, ItemPickup* closest, std::vector<ItemPickup*>& itemPickups,
                   Character* bigChar) {
    const ItemDef& def = *closest->GetDef();
    if (!inventory.CanAcceptItem(def)) {
        return;
    }
    if (!inventory.AddItem(def, closest->GetDurability())) {
        return;
    }
    for (auto& p : itemPickups) {
        if (p == closest) {
            p = nullptr;
            break;
        }
    }
    closest->Destroy();
    PlayRandomPickupSound();
    if (StageState* stage = Game::TryGetStageState()) {
        stage->NotifyItemPickupCollected(closest);
        stage->SaveCurrentProgress();
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
      addObjectToState(addObjFn), clampPickupTopLeft(std::move(clampFn)) {
    (void)addObjectToState;
    (void)clampPickupTopLeft;
}

void HotbarComponent::Start() {}

float HotbarComponent::GetPickupReachRadius() const {
    if (!bigCharacter) {
        return 0.0f;
    }
    return bigCharacter->GetFootCircleRadius() + kPickupPromptFootRadiusExtra;
}

ItemPickup* HotbarComponent::FindClosestReachablePickup() const {
    if (!bigCharacter || !Character::player) {
        return nullptr;
    }

    ItemPickup* closest = nullptr;
    float closestDist = 1e30f;
    const Vec2 playerCenter = bigCharacter->GetCenter();

    for (ItemPickup* p : itemPickups) {
        if (!p || p->GetAssociated().IsDead()) {
            continue;
        }

        const int itemHeight = p->GetHeightLevel();
        const SDL_Rect reachBox = Character::player->GetInteractionRect(itemHeight);
        const GameObject& itemObj = p->GetAssociated();
        const SDL_Rect itemRect = {
            static_cast<int>(itemObj.box.x),
            static_cast<int>(itemObj.box.y),
            static_cast<int>(itemObj.box.w),
            static_cast<int>(itemObj.box.h),
        };

        if (!SDL_HasIntersection(&reachBox, &itemRect)) {
            continue;
        }

        const float d = playerCenter.Distance(p->GetCenter());
        if (d < closestDist) {
            closestDist = d;
            closest = p;
        }
    }

    return closest;
}

void HotbarComponent::TryInventoryToggle() {
    InputManager& input = InputManager::GetInstance();
    if (!input.KeyPress(SDLK_i)) return;
    if (!inventoryGrid) return;
    inventoryGrid->Toggle();
}

void HotbarComponent::TryCloseGridOnEKey() {
    InputManager& input = InputManager::GetInstance();
    if (!input.KeyPress(SDLK_e)) return;
    if (!inventoryGrid || !inventoryGrid->IsOpen()) return;
    inventoryGrid->Close();
}

void HotbarComponent::TrySelectSlotOnKeyPress() {
    InputManager& input = InputManager::GetInstance();

    if (input.KeyPress(SDLK_1)) {
        const int slot = inventory.FindBestFlashlight();
        if (slot >= 0) {
            const bool wasSelected = inventory.GetSelectedSlot() == slot;
            if (!inventory.SelectSlot(slot)) return;
            if (!bigCharacter) return;
            if (!wasSelected) {
                bigCharacter->NotifyInventoryLightChanged();
            }
        }
    }

    if (input.KeyPress(SDLK_2)) {
        const int slot = inventory.FindBestLamp();
        if (slot >= 0) {
            const bool wasSelected = inventory.GetSelectedSlot() == slot;
            if (!inventory.SelectSlot(slot)) return;
            if (!bigCharacter) return;
            if (wasSelected) {
                bigCharacter->PlayPickLampAnimation();
            } else {
                bigCharacter->NotifyInventoryLightChanged();
            }
        }
    }
}

void HotbarComponent::TryUseActiveItemOnKeyPress() {
    InputManager& input = InputManager::GetInstance();
    if (!input.KeyPress(SDLK_f)) {
        return;
    }

    const ItemInstance* active = inventory.GetSelectedItem();
    if (!active) {
        return;
    }

    if (!active->def.HasProperty(ItemProperty::LIGHT_SOURCE)) {
        return;
    }

    const bool wasOn = inventory.isLightToggledOn;
    const bool isLighter = inventory.IsActiveLightLighter();
    if (wasOn) {
        inventory.isLightToggledOn = false;
        if (isLighter) {
            GameSfx::PlayLighterToggle(false);
        }
    } else if (inventory.TryTurnLightOn()) {
        if (isLighter) {
            GameSfx::PlayLighterToggle(true);
        }
    }
}

void HotbarComponent::TryPickupOnKeyPress() {
    InputManager& input = InputManager::GetInstance();
    StageState* stage = Game::TryGetStageState();
    if (!stage) {
        return;
    }

    if (!input.KeyPress(SDLK_e)) {
        return;
    }

    if (inventoryGrid && inventoryGrid->IsOpen()) {
        return;
    }

    ItemPickup* closest = stage->GetReachablePickup();
    if (!closest) {
        return;
    }

    Box* pushBox = stage->GetReachablePushBox();
    const bool boxWins = pushBox && stage->IsPushBoxCloserThanItem(closest, pushBox);
    const bool jornalWins =
        stage->GetReachableJornal() &&
        stage->IsJornalCloserThanItemAndBox(stage->GetReachableJornal(), closest, pushBox);
    const bool candleWins =
        stage->GetReachableCandle() && stage->IsCandleClosestForInteraction(stage->GetReachableCandle());
    if (boxWins || jornalWins || candleWins) {
        return;
    }

    const int hLevel = closest->GetHeightLevel();
    if (hLevel == 0 || hLevel == 1) {
        if (!inventory.CanAcceptItem(*closest->GetDef())) {
            return;
        }
        PerformPickup(inventory, closest, itemPickups, bigCharacter);
        if (Character::player) {
            Character::player->currentState = Character::ActionState::INTERACTING;
            Character::player->interactTimer = 0.2f;
        }
        if (bigCharacter) {
            bigCharacter->NotifyInventoryLightChanged();
        }
        return;
    }

    if (hLevel == 2 && Character::littleBrother) {
        const float distBrothers = Character::player->GetFootCircleCenter().Distance(
            Character::littleBrother->GetFootCircleCenter());
        if (distBrothers > 170.0f || !inventory.CanAcceptItem(*closest->GetDef())) {
            return;
        }

        Character::player->currentState = Character::ActionState::INTERACTING;
        Character::player->interactTimer = 1.5f;
        Character::littleBrother->currentState = Character::ActionState::INTERACTING;
        Character::littleBrother->interactTimer = 1.5f;
        Character::littleBrother->PositionForCoop(Character::player);
        PerformPickup(inventory, closest, itemPickups, bigCharacter);
    }
}

void HotbarComponent::Update(float dt) {
    (void)dt;
    if (!controlledCharacterPtr || !*controlledCharacterPtr || !bigCharacter) {
        return;
    }
    if (*controlledCharacterPtr != bigCharacter) {
        return;
    }

    TryInventoryToggle();
    TryCloseGridOnEKey();
    TrySelectSlotOnKeyPress();
    TryUseActiveItemOnKeyPress();
    TryPickupOnKeyPress();
}

void HotbarComponent::Render() {}
