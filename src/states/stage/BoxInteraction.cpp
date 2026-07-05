#include "states/stage/StageState.h"
#include "core/InputManager.h"
#include "engine/GameObject.h"
#include "gameplay/Box.h"
#include "gameplay/Character.h"
#include "gameplay/ItemPickup.h"
#include "gameplay/Jornal.h"
#include "gameplay/Candlestick.h"
#include "gameplay/Window.h"
#include "ui/InteractionOutline.h"
#include "world/Collider.h"
#include "audio/GameSfx.h"
#include "audio/GameVoice.h"
#include "gameplay/Character.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {


constexpr float kCandleMaxDistX       = 120.0f;  // faixa lateral
constexpr float kCandleMaxDistFront   = 180.0f;  // distância máxima na frente
constexpr float kCandleBehindTolerance = 40.0f;  // tolerância pra não ser injusto

bool IsHoldingLamp(const Inventory& inventory) {
    return inventory.GetHeldPropVisual() == HeldPropVisual::Lamp;
}

bool IsCandleWithinRelaxedReach(Character& character, const Candlestick& candle) {
    const GameObject& candleObj = candle.GetAssociated();
    const Vec2 foot = character.GetFootCircleCenter();

    float candleFootX = candleObj.box.Center().x;
    float candleFootY = candleObj.box.y + candleObj.box.h;

    float dx = std::abs(foot.x - candleFootX);
    float dy = foot.y - candleFootY;

    // Alcance frontal — maior quando olhando para cima (de frente pro objeto)
    float maxFront = kCandleMaxDistFront;
    if (character.GetCurrentDirection() == Character::Direction::UP) {
        maxFront = 250.0f;
    }

    return dx <= kCandleMaxDistX &&
           dy >= -kCandleBehindTolerance &&
           dy <= maxFront;
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

bool StageState::IsPickupStillTracked(ItemPickup* pickup) const {
    if (!pickup) {
        return false;
    }
    for (ItemPickup* tracked : itemPickups) {
        if (tracked == pickup && !tracked->GetAssociated().IsDead()) {
            return true;
        }
    }
    return false;
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

RadioAsset* StageState::FindClosestReachableRadio() const {
    // Qualquer irmão pode interagir (não restrito ao irmãozão como a vela)
    Character* c = controlledCharacter;
    if (!c) return nullptr;
 
    RadioAsset* closest = nullptr;
    float closestDist = 1e30f;
    const Vec2 playerCenter = c->GetCenter();
 
    for (const auto& goPtr : objectArray) {
        GameObject* go = goPtr.get();
        if (!go || go->IsDead()) continue;
 
        RadioAsset* radio = go->GetComponent<RadioAsset>();
        if (!radio) continue;
 
        const Vec2 radioCenter = go->box.Center();
        const float d = playerCenter.Distance(radioCenter);
 
        constexpr float kRadioReach = 120.0f;
        if (d < kRadioReach && d < closestDist) {
            closestDist = d;
            closest = radio;
        }
    }
    return closest;
}

void StageState::TryInteractRadioOnKeyPress() {
    InputManager& input = InputManager::GetInstance();
    if (!input.ActionPress(GameAction::Interact) || !reachableRadio) return;
    reachableRadio->Toggle();
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


bool StageState::IsPlayerNearLitCandle() const {
    if (!controlledCharacter) {
        return false;
    }

    for (const auto& goPtr : objectArray) {
        GameObject* go = goPtr.get();
        if (!go || go->IsDead()) {
            continue;
        }

        Candlestick* candle = go->GetComponent<Candlestick>();
        if (!candle || !candle->IsLit()) {
            continue;
        }

        if (IsCandleWithinRelaxedReach(*controlledCharacter, *candle)) {
            return true;
        }
    }

    return false;
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
    if (!input.ActionPress(GameAction::Interact) || !reachableCandle) {
        return;
    }

    if (!IsCandleClosestForInteraction(reachableCandle)) {
        return;
    }

    if (reachableCandle->IsLit()) {
        reachableCandle->SetLit(false);
        GameSfx::PlayCandleBlow();   
        SaveCurrentProgress();
        return;
    }

    const bool wasLightActive = inventory.IsUsableLightActive();
    if (inventory.TryActivateBestLight()) {
        if (!wasLightActive && inventory.IsActiveLightLighter()) {
            GameSfx::PlayLighterToggle(true);
        }
        if (bigCharacter) {
            bigCharacter->NotifyInventoryLightChanged();
        }
        reachableCandle->SetLit(true);
        GameSfx::PlayCandleLightUp();   
        SaveCurrentProgress();
    }
}

Window* StageState::FindClosestReachableWindow() const {
    if (!bigCharacter || controlledCharacter != bigCharacter) {
        return nullptr;
    }

    Window* closest = nullptr;
    float closestDist = 1e30f;
    const Vec2 playerCenter = bigCharacter->GetCenter();

    for (const auto& goPtr : objectArray) {
        GameObject* go = goPtr.get();
        if (!go || go->IsDead()) continue;

        Window* window = go->GetComponent<Window>();
        if (!window) continue;

        const float d = playerCenter.Distance(go->box.Center());
        
        // Raio de distância para conseguir interagir com a janela (ajuste se precisar)
        if (d < 200.0f && d < closestDist) {
            closestDist = d;
            closest = window;
        }
    }
    return closest;
}

bool StageState::IsWindowClosestForInteraction(Window* window) const {
    if (!window) return false;

    const float windowDist = GetInteractableDistance(window->GetAssociated());

    if (reachableJornal && GetInteractableDistance(reachableJornal->GetAssociated()) < windowDist) return false;
    if (reachableCandle && GetInteractableDistance(reachableCandle->GetAssociated()) < windowDist) return false;
    if (reachablePushBox && reachablePushBox->IsPushable() && GetInteractableDistance(reachablePushBox->GetAssociated()) < windowDist) return false;
    
    ItemPickup* item = FindClosestReachableItem();
    if (item && !item->GetAssociated().IsDead() && GetInteractableDistance(item->GetAssociated()) < windowDist) return false;

    return true;
}

void StageState::TryInteractWindowOnKeyPress() {
    InputManager& input = InputManager::GetInstance();
    if (!input.ActionPress(GameAction::Interact) || !reachableWindow) {
        return;
    }

    // Jogador só pode FECHAR janelas, não abrir
    // (abrir é responsabilidade do monstro — prejudica os irmãos)
    if (reachableWindow->GetState() != Window::WindowState::OPEN) {
        return;
    }

    // Chama a função da janela para abrir/fechar
    reachableWindow->Toggle();
    
    // Salva o jogo toda vez que interage com a janela (igual a vela)
    SaveCurrentProgress(); 
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

void StageState::NotifyItemPickupCollected(ItemPickup* pickup) {
    if (reachablePickup == pickup) {
        reachablePickup = nullptr;
    }
}

void StageState::UpdateBoxInteraction() {
    reachableJornal = FindClosestReachableJornal();
    reachablePushBox = FindClosestReachablePushBox();
    reachablePickup = FindClosestReachableItem();
    reachableCandle = FindClosestReachableCandle();
    reachableWindow = FindClosestReachableWindow();
    reachableRadio = FindClosestReachableRadio();

    GameSfx::UpdateCandleProximity(IsPlayerNearLitCandle());

    InputManager& input = InputManager::GetInstance();
    const bool eHeld = input.ActionDown(GameAction::Interact);
    const bool ePressed = input.ActionPress(GameAction::Interact);
    const bool boxPushBlocked = IsHoldingLamp(inventory);
    ItemPickup* reachableItem = FindClosestReachableItem();

    // Apertou interagir tentando mover a caixa, mas está com a lâmpada (overlay
    // vermelho) → não dá pra empurrar: "não consigo".
    if (ePressed && boxPushBlocked && reachablePushBox &&
        IsPushBoxCloserThanItem(reachableItem, reachablePushBox)) {
        GameVoice::OnActionBlocked();
    }

    if (boxPushBlocked && activePushBox) {
        GameSfx::NotifyBoxPushEnd();
        activePushBox = nullptr;
        if (bigCharacter && bigCharacter->currentState == Character::ActionState::PUSHING_BOX) {
            bigCharacter->currentState = Character::ActionState::NORMAL;
        }
    }

    if (eHeld) {
        if (!boxPushBlocked && !activePushBox && reachablePushBox &&
            IsPushBoxCloserThanItem(reachableItem, reachablePushBox)) {
            activePushBox = reachablePushBox;
            GameVoice::OnDragObject();   // às vezes comenta o peso ("isso pesa")
            if (bigCharacterObject) {
                const GameObject& boxObj = reachablePushBox->GetAssociated();
                pushBoxOffset.x = boxObj.box.x - bigCharacterObject->box.x;
                pushBoxOffset.y = boxObj.box.y - bigCharacterObject->box.y;
            }
            if (bigCharacter) {
                bigCharacter->currentState = Character::ActionState::PUSHING_BOX;
            }
            // O personagem pega a lerdeza da caixa pra ele (em outras palavras ele fica mais lento)
            bigCharacter->SetSpeedMultiplier(activePushBox->GetWeightMultiplier());
        }
    } else if (activePushBox) {
        GameSfx::NotifyBoxPushEnd();
        activePushBox = nullptr;
        if (bigCharacter && bigCharacter->currentState == Character::ActionState::PUSHING_BOX) {
            bigCharacter->currentState = Character::ActionState::NORMAL;

            // --- DEVOLVE A VELOCIDADE NORMAL AO SOLTAR ---
            bigCharacter->SetSpeedMultiplier(1.0f);
        }
        
    }

    Box::SetActivePushTarget(activePushBox);

    if (wasPushingLastFrame && input.KeyRelease(SDLK_e)) {
        SaveCurrentProgress();
    }
    wasPushingLastFrame = activePushBox != nullptr;
    if (activePushBox) {
        GameSfx::MaintainBoxPushLoop();
    }
}

void StageState::ApplyCoupledPushMovement(const Vec2& prevPlayerPos) {
    if (!activePushBox || !bigCharacterObject || !bigCharacter) {
        return;
    }
    if (IsHoldingLamp(inventory)) {
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

    if (activePushBox->TryMoveBy(dx, dy)) {
        GameSfx::NotifyBoxSlide();
    } else {
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
            if (IsHoldingLamp(inventory)) {
                DrawSpriteInteractionGlow(go, 255, 64, 64, 1.14f);
            } else {
                DrawSpriteInteractionGlow(go, 255, 255, 255);
            }
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

    RadioAsset* radio = go.GetComponent<RadioAsset>();
    if (radio && radio == reachableRadio) {
        DrawSpriteInteractionGlow(go, 255, 255, 255, 1.14f);
        return;
    }

    if (!reachablePickup || !IsPickupStillTracked(reachablePickup)) {
        return;
    }
    if (reachablePickup->GetAssociated().IsDead()) {
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
