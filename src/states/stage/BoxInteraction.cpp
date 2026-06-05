#include "states/stage/StageState.h"
#include "core/InputManager.h"
#include "engine/GameObject.h"
#include "gameplay/Box.h"
#include "gameplay/Character.h"
#include "gameplay/ItemPickup.h"
#include "gameplay/Jornal.h"
#include "ui/InteractionOutline.h"
#include "world/Collider.h"

#include <cmath>

float StageState::GetInteractableDistance(const GameObject& obj) const {
    if (!bigCharacter) {
        return 1e30f;
    }
    const Vec2 objCenter(obj.box.x + obj.box.w * 0.5f, obj.box.y + obj.box.h * 0.5f);
    return bigCharacter->GetCenter().Distance(objCenter);
}

bool StageState::IsPushBoxCloserThanItem(ItemPickup* item, Box* box) const {
    if (!box) {
        return false;
    }
    if (!item || item->GetAssociated().IsDead()) {
        return true;
    }
    return GetInteractableDistance(box->GetAssociated()) < GetInteractableDistance(item->GetAssociated());
}

ItemPickup* StageState::FindClosestReachableItem() const {
    if (!bigCharacter || !Character::player || controlledCharacter != bigCharacter) {
        return nullptr;
    }

    ItemPickup* closest = nullptr;
    float closestDist = 1e30f;
    const Vec2 playerCenter = bigCharacter->GetCenter();

    for (ItemPickup* pickup : itemPickups) {
        if (!pickup || pickup->GetAssociated().IsDead()) {
            continue;
        }

        const int itemHeight = pickup->GetHeightLevel();
        const SDL_Rect reachBox = Character::player->GetInteractionRect(itemHeight);
        const GameObject& itemObj = pickup->GetAssociated();
        const SDL_Rect itemRect = {
            static_cast<int>(itemObj.box.x),
            static_cast<int>(itemObj.box.y),
            static_cast<int>(itemObj.box.w),
            static_cast<int>(itemObj.box.h),
        };

        if (!SDL_HasIntersection(&reachBox, &itemRect)) {
            continue;
        }

        const float d = playerCenter.Distance(pickup->GetCenter());
        if (d < closestDist) {
            closestDist = d;
            closest = pickup;
        }
    }

    return closest;
}

Box* StageState::FindClosestReachablePushBox() const {
    if (!bigCharacter || !Character::player || controlledCharacter != bigCharacter) {
        return nullptr;
    }

    Box* closest = nullptr;
    float closestDist = 1e30f;
    const Vec2 playerCenter = bigCharacter->GetCenter();
    const SDL_Rect reachBox = Character::player->GetInteractionRect(0);

    for (const auto& goPtr : objectArray) {
        GameObject* go = goPtr.get();
        if (!go || go->IsDead()) {
            continue;
        }

        Box* box = go->GetComponent<Box>();
        if (!box || !box->IsPushable()) {
            continue;
        }

        const GameObject& boxObj = box->GetAssociated();
        const SDL_Rect boxRect = {
            static_cast<int>(boxObj.box.x),
            static_cast<int>(boxObj.box.y),
            static_cast<int>(boxObj.box.w),
            static_cast<int>(boxObj.box.h),
        };

        if (!SDL_HasIntersection(&reachBox, &boxRect)) {
            continue;
        }

        const Vec2 boxCenter(boxObj.box.x + boxObj.box.w * 0.5f, boxObj.box.y + boxObj.box.h * 0.5f);
        const float d = playerCenter.Distance(boxCenter);
        if (d < closestDist) {
            closestDist = d;
            closest = box;
        }
    }

    return closest;
}

void StageState::UpdateBoxInteraction() {
    reachableJornal = FindClosestReachableJornal();
    reachablePushBox = FindClosestReachablePushBox();

    InputManager& input = InputManager::GetInstance();
    const bool eHeld = input.IsKeyDown(SDLK_e);
    ItemPickup* reachableItem = FindClosestReachableItem();

    if (eHeld) {
        if (!activePushBox && reachablePushBox && IsPushBoxCloserThanItem(reachableItem, reachablePushBox)) {
            activePushBox = reachablePushBox;
            if (bigCharacterObject) {
                const GameObject& boxObj = reachablePushBox->GetAssociated();
                pushBoxOffset.x = boxObj.box.x - bigCharacterObject->box.x;
                pushBoxOffset.y = boxObj.box.y - bigCharacterObject->box.y;
            }
            if (bigCharacter) {
                bigCharacter->currentState = Character::ActionState::PUSHING_BOX;
            }
        }
    } else if (activePushBox) {
        activePushBox = nullptr;
        if (bigCharacter && bigCharacter->currentState == Character::ActionState::PUSHING_BOX) {
            bigCharacter->currentState = Character::ActionState::NORMAL;
        }
    }

    Box::SetActivePushTarget(activePushBox);

    if (wasPushingLastFrame && input.KeyRelease(SDLK_e)) {
        SaveCurrentProgress();
    }
    wasPushingLastFrame = activePushBox != nullptr;
}

void StageState::ApplyCoupledPushMovement(const Vec2& prevPlayerPos) {
    if (!activePushBox || !bigCharacterObject || !bigCharacter) {
        return;
    }
    if (bigCharacter->currentState != Character::ActionState::PUSHING_BOX) {
        return;
    }

    GameObject& boxObj = activePushBox->GetAssociated();
    const float targetBoxX = bigCharacterObject->box.x + pushBoxOffset.x;
    const float targetBoxY = bigCharacterObject->box.y + pushBoxOffset.y;
    const float dx = targetBoxX - boxObj.box.x;
    const float dy = targetBoxY - boxObj.box.y;
    if (dx == 0.0f && dy == 0.0f) {
        return;
    }

    if (!activePushBox->TryMoveBy(dx, dy)) {
        bigCharacterObject->box.x = prevPlayerPos.x;
        bigCharacterObject->box.y = prevPlayerPos.y;
        if (Collider* playerCol = bigCharacterObject->GetComponent<Collider>()) {
            playerCol->Update(0);
        }
        bigCharacter->ClearMovement();
    }
}

void StageState::RenderInteractionGlowIfNeeded(GameObject& go) {
    if (controlledCharacter != bigCharacter) {
        return;
    }

    Box* box = go.GetComponent<Box>();
    if (box && box->IsPushable()) {
        if (box == activePushBox) {
            DrawSpriteInteractionGlow(go, 255, 220, 0);
        } else if (box == reachablePushBox) {
            DrawSpriteInteractionGlow(go, 255, 255, 255);
        }
        return;
    }

    Jornal* jornal = go.GetComponent<Jornal>();
    if (jornal && jornal == reachableJornal) {
        DrawSpriteInteractionGlow(go, 255, 255, 255, 1.14f);
        return;
    }

    ItemPickup* reachableItem = FindClosestReachableItem();
    if (!reachableItem || reachableItem->GetAssociated().IsDead()) {
        return;
    }
    if (&reachableItem->GetAssociated() == &go) {
        DrawSpriteInteractionGlow(go, 255, 255, 255, 1.14f);
    }
}
