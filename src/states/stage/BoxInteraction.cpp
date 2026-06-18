#include "states/stage/StageState.h"
#include "core/InputManager.h"
#include "engine/GameObject.h"
#include "gameplay/Box.h"
#include "gameplay/Character.h"
#include "gameplay/ItemPickup.h"
#include "gameplay/Jornal.h"
#include "gameplay/Candlestick.h"
#include "ui/InteractionOutline.h"
#include "world/Collider.h"
#include "audio/GameSfx.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {

constexpr float kCandleReachPadX = 110.0f;
constexpr float kCandleReachPadUp = 280.0f;
constexpr float kCandleReachPadDown = 320.0f;

bool IsCandleWithinRelaxedReach(Character& character, const Candlestick& candle) {
    const GameObject& candleObj = candle.GetAssociated();
    const Vec2 footCenter = character.GetFootCircleCenter();
    const Vec2 bodyCenter = character.GetAssociated().box.Center();

    Rect zone = candleObj.box;
    zone.x -= kCandleReachPadX;
    zone.w += kCandleReachPadX * 2.0f;
    zone.y -= kCandleReachPadUp;
    zone.h += kCandleReachPadUp + kCandleReachPadDown;

    return zone.Contains(footCenter) || zone.Contains(bodyCenter);
}

} // namespace

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

bool StageState::ShouldSkipPickupSpawn(int tiledId) const {
    if (tiledId < 0) {
        return false;
    }
    return skippedPickupSpawnIds.count(tiledId) > 0;
}

Candlestick* StageState::FindClosestReachableCandle() const {
    if (!bigCharacter || controlledCharacter != bigCharacter) {
        return nullptr;
    }

    Candlestick* closest = nullptr;
    float closestDist = 1e30f;
    const Vec2 playerCenter = bigCharacter->GetCenter();

    for (const auto& goPtr : objectArray) {
        GameObject* go = goPtr.get();
        if (!go || go->IsDead()) {
            continue;
        }

        Candlestick* candle = go->GetComponent<Candlestick>();
        if (!candle) {
            continue;
        }

        if (!IsCandleWithinRelaxedReach(*bigCharacter, *candle)) {
            continue;
        }

        const GameObject& candleObj = candle->GetAssociated();
        const Vec2 candleCenter(candleObj.box.x + candleObj.box.w * 0.5f,
                                candleObj.box.y + candleObj.box.h * 0.5f);
        const float d = playerCenter.Distance(candleCenter);
        if (d < closestDist) {
            closestDist = d;
            closest = candle;
        }
    }

    return closest;
}

bool StageState::IsCandleClosestForInteraction(Candlestick* candle) const {
    if (!candle) {
        return false;
    }

    const float candleDist = GetInteractableDistance(candle->GetAssociated());

    if (reachableJornal) {
        const float dJornal = GetInteractableDistance(reachableJornal->GetAssociated());
        if (dJornal < candleDist) {
            return false;
        }
    }

    ItemPickup* item = FindClosestReachableItem();
    if (item && !item->GetAssociated().IsDead()) {
        const float dItem = GetInteractableDistance(item->GetAssociated());
        if (dItem < candleDist) {
            return false;
        }
    }

    if (reachablePushBox && reachablePushBox->IsPushable()) {
        const float dBox = GetInteractableDistance(reachablePushBox->GetAssociated());
        if (dBox < candleDist) {
            return false;
        }
    }

    return true;
}

void StageState::TryInteractCandleOnKeyPress() {
    InputManager& input = InputManager::GetInstance();
    if (!input.KeyPress(SDLK_e) || !reachableCandle) {
        return;
    }

    if (!IsCandleClosestForInteraction(reachableCandle)) {
        return;
    }

    if (reachableCandle->IsLit()) {
        reachableCandle->SetLit(false);
        SaveCurrentProgress();
        return;
    }

    if (inventory.IsUsableLightActive()) {
        reachableCandle->SetLit(true);
        SaveCurrentProgress();
    }
}

bool StageState::IsPickupBlocked(ItemPickup* pickup) const {
    if (!pickup || !pickup->GetDef()) {
        return true;
    }
    return !inventory.CanAcceptItem(*pickup->GetDef());
}

void StageState::MergeSkippedPickupIds(const std::vector<int>& removed, const std::vector<int>& missed) {
    skippedPickupSpawnIds.clear();
    for (int id : removed) {
        skippedPickupSpawnIds.insert(id);
    }
    for (int id : missed) {
        skippedPickupSpawnIds.insert(id);
    }
}

void StageState::MarkMissedUniquePickupsOnLevelLeave() {
    std::unordered_set<int> aliveIds;
    for (const auto& goPtr : objectArray) {
        GameObject* go = goPtr.get();
        if (!go || go->IsDead()) {
            continue;
        }
        ItemPickup* pickup = go->GetComponent<ItemPickup>();
        if (pickup && go->tiledId >= 0) {
            aliveIds.insert(go->tiledId);
        }
    }

    for (const EntitySpawn& spawn : level.entitySpawns) {
        if (spawn.type != "ItemSpawn" || spawn.tiledId < 0) {
            continue;
        }
        if (!spawn.properties.count("unique")) {
            continue;
        }
        if (!spawn.properties.at("unique").get<bool>()) {
            continue;
        }
        if (aliveIds.count(spawn.tiledId) > 0) {
            const int id = spawn.tiledId;
            if (std::find(missedUniquePickupIdsAccum.begin(), missedUniquePickupIdsAccum.end(), id) ==
                missedUniquePickupIdsAccum.end()) {
                missedUniquePickupIdsAccum.push_back(id);
            }
        }
    }
}

void StageState::UpdateBoxInteraction() {
    reachableJornal = FindClosestReachableJornal();
    reachablePushBox = FindClosestReachablePushBox();
    reachablePickup = FindClosestReachableItem();
    reachableCandle = FindClosestReachableCandle();

    GameSfx::UpdateCandleProximity(reachableCandle != nullptr);

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
        boxPushSoundStreak = 0;
        return;
    }

    if (activePushBox->TryMoveBy(dx, dy)) {
        boxPushSoundStreak++;
        GameSfx::PlayBoxPushSound(boxPushSoundStreak >= 4);
        if (boxPushSoundStreak >= 4) {
            boxPushSoundStreak = 0;
        }
    } else {
        boxPushSoundStreak = 0;
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

    Candlestick* candle = go.GetComponent<Candlestick>();
    if (candle && candle == reachableCandle) {
        DrawSpriteInteractionGlow(go, 255, 255, 255, 1.14f);
        return;
    }

    if (!reachablePickup || reachablePickup->GetAssociated().IsDead()) {
        return;
    }
    if (&reachablePickup->GetAssociated() == &go) {
        if (IsPickupBlocked(reachablePickup)) {
            DrawSpriteInteractionGlow(go, 255, 64, 64, 1.14f);
        } else {
            DrawSpriteInteractionGlow(go, 255, 255, 255, 1.14f);
        }
    }
}
