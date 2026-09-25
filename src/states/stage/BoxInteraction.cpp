#include "states/stage/StageState.h"
#include "core/Telemetry.h"
#include "core/InputManager.h"
#include "engine/GameObject.h"
#include "gameplay/Box.h"
#include "gameplay/Character.h"
#include "gameplay/Monster.h"
#include "gameplay/ItemPickup.h"
#include "gameplay/Jornal.h"
#include "gameplay/Candlestick.h"
#include "gameplay/Window.h"
#include "ui/InteractionOutline.h"
#include "world/Collider.h"
#include "audio/GameSfx.h"
#include "audio/GameVoice.h"
#include "gameplay/Character.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <algorithm>
#include <cmath>
#include <string>
#include <iostream>

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

namespace {
// Alcance de pegar item (pixels de mundo). 
constexpr float kPickupReachRadius  = 110.0f;               // até onde alcança à frente
constexpr float kPickupTouchRadius  = 45.0f;                // encostado: vale até atrás
constexpr float kPickupHalfAngleDeg = 70.0f;                // metade da abertura da "frente"

// Vetor unitário da direção para onde o personagem olha (y cresce para baixo).
Vec2 FacingVector(Character::Direction d) {
    switch (d) {
        case Character::Direction::UP:    return Vec2( 0.0f, -1.0f);
        case Character::Direction::DOWN:  return Vec2( 0.0f,  1.0f);
        case Character::Direction::LEFT:  return Vec2(-1.0f,  0.0f);
        case Character::Direction::RIGHT:
        default:                          return Vec2( 1.0f,  0.0f);
    }
}
} // namespace

// Item que o irmãozão consegue pegar agora: dentro de um círculo em volta do pé,
// e À FRENTE dele (num cone na direção para onde olha). Item praticamente
// encostado vale em qualquer direção. Itens em cima de móveis são projetados
// para o chão (a base do móvel) antes de medir, como antes.
ItemPickup* StageState::FindClosestReachableItem() const {
    if (!bigCharacter || !Character::player || controlledCharacter != bigCharacter) {
        return nullptr;
    }

    const Rect& pbox = Character::player->GetAssociated().box;
    const Vec2 foot{ pbox.x + pbox.w * 0.5f, pbox.y + pbox.h };
    const Vec2 facing = FacingVector(Character::player->GetFacingDirection());
    const float cosHalf = std::cos(kPickupHalfAngleDeg * 3.14159265f / 180.0f);

    ItemPickup* closest = nullptr;
    float closestDist = 1e30f;

    for (ItemPickup* pickup : itemPickups) {
        if (!pickup || pickup->GetAssociated().IsDead()) continue;

        // Retângulo do item "descido" até o chão: um item na prateleira fica
        // desenhado mais acima, então soma-se a altura para compará-lo com o pé.
        const int h = pickup->GetHeightLevel();
        const float zOff = (h == 1) ? 140.0f : (h == 2 ? 260.0f : 0.0f);
        const Rect& ib = pickup->GetAssociated().box;
        const float rx = ib.x, ry = ib.y + zOff;

        // Ponto do retângulo mais próximo do pé (clamp). Sprites grandes ou
        // itens em cima de móveis ficam alcançáveis pela borda, como na regra antiga.
        const Vec2 nearest{ std::max(rx, std::min(foot.x, rx + ib.w)),
                            std::max(ry, std::min(foot.y, ry + ib.h)) };
        const Vec2 toItem{ nearest.x - foot.x, nearest.y - foot.y };
        const float dist = std::sqrt(toItem.x * toItem.x + toItem.y * toItem.y);

        if (dist > kPickupReachRadius) continue;

        // Encostado: vale em qualquer direção. Mais longe: só se estiver no cone da frente.
        if (dist > kPickupTouchRadius) {
            const float dot = (toItem.x * facing.x + toItem.y * facing.y) / dist;
            if (dot < cosHalf) continue;
        }

        if (dist < closestDist) {
            closestDist = dist;
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
    {
        const Vec2 c = reachableRadio->GetAssociated().box.Center();
        Telemetry::Event("radio_used", Telemetry::Fields().Int("level", currentLevelIndex).Pos("", c.x, c.y));
    }
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

// Qual objeto o [E] vai usar AGORA: o mais perto entre os que estão ao alcance.
// Usa a mesma régua (GetInteractableDistance) que as regras de desempate do E,
// então o contorno e o texto do rodapé sempre apontam para o mesmo alvo.
GameObject* StageState::GetInteractionFocus() const {
    if (controlledCharacter != bigCharacter) return nullptr;
    if (activePushBox) return &activePushBox->GetAssociated();   // agarrado: é sempre ele

    GameObject* best = nullptr;
    float bestDist = 1e30f;
    auto consider = [&](GameObject* go) {
        if (!go || go->IsDead()) return;
        const float d = GetInteractableDistance(*go);
        if (d < bestDist) { bestDist = d; best = go; }
    };

    if (reachablePushBox && reachablePushBox->IsPushable()) consider(&reachablePushBox->GetAssociated());
    if (reachableJornal) consider(&reachableJornal->GetAssociated());
    if (reachableCandle) consider(&reachableCandle->GetAssociated());
    if (reachableRadio)  consider(&reachableRadio->GetAssociated());
    if (reachableWindow && reachableWindow->GetState() == Window::WindowState::OPEN) {
        consider(&reachableWindow->GetAssociated());
    }
    if (reachablePickup && IsPickupStillTracked(reachablePickup)) {
        consider(&reachablePickup->GetAssociated());
    }
    return best;
}

void StageState::TryInteractCandleOnKeyPress() {
    InputManager& input = InputManager::GetInstance();
    if (!input.ActionPress(GameAction::Interact) || !reachableCandle) {
        return;
    }

    if (!IsCandleClosestForInteraction(reachableCandle)) {
        return;
    }

    const Vec2 candleCenter = reachableCandle->GetAssociated().box.Center();
    if (reachableCandle->IsLit()) {
        reachableCandle->SetLit(false);
        Telemetry::Event("candle_blown", Telemetry::Fields()
            .Int("level", currentLevelIndex).Pos("", candleCenter.x, candleCenter.y));
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
        Telemetry::Event("candle_lit", Telemetry::Fields()
            .Int("level", currentLevelIndex).Pos("", candleCenter.x, candleCenter.y));
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
        
        // Raio de distância para conseguir interagir com a janela (ajuste se precisar).
        // 260 = 200 +30% → dá pra interagir de mais longe.
        if (d < 260.0f && d < closestDist) {
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
    {
        const Vec2 c = reachableWindow->GetAssociated().box.Center();
        Telemetry::Event("window_closed", Telemetry::Fields().Int("level", currentLevelIndex).Pos("", c.x, c.y));
    }
    
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
    const bool ePressed = input.ActionPress(GameAction::Interact);
    const bool boxPushBlocked = IsHoldingLamp(inventory);
    ItemPickup* reachableItem = FindClosestReachableItem();

    // Apertou interagir tentando mover a caixa, mas está com a lâmpada (overlay
    // vermelho) → não dá pra empurrar: "não consigo".
    if (ePressed && boxPushBlocked && reachablePushBox &&
        IsPushBoxCloserThanItem(reachableItem, reachablePushBox)) {
        GameVoice::OnActionBlocked();
    }

    // Pegou a lâmpada enquanto empurrava → solta a caixa automaticamente.
    if (boxPushBlocked && activePushBox) {
        DetachActivePushBox();
    }

    // EMPURRAR É UM TOGGLE (não é mais segurar-para-empurrar): aperta E para
    // GRUDAR na caixa/barril, aperta E de novo para SOLTAR. Só o irmãozão e só
    // quando a caixa é o interagível mais próximo. As demais interações com E
    // (pegar item, vela, janela, rádio, jornal) continuam iguais.
    if (ePressed && !boxPushBlocked) {
        if (activePushBox) {
            // Já grudado → solta (soltar sempre é permitido, independente do que
            // mais esteja por perto).
            DetachActivePushBox();
        } else if (reachablePushBox &&
                   IsPushBoxCloserThanItem(reachableItem, reachablePushBox)) {
            // Gruda na caixa/barril (toggle ligado) — vale para móvel e imóvel.
            activePushBox = reachablePushBox;
            if (bigCharacterObject) {
                const GameObject& boxObj = reachablePushBox->GetAssociated();
                pushBoxOffset.x = boxObj.box.x - bigCharacterObject->box.x;
                pushBoxOffset.y = boxObj.box.y - bigCharacterObject->box.y;
            }
            if (bigCharacter) {
                bigCharacter->currentState = Character::ActionState::PUSHING_BOX;
                // O personagem pega a lerdeza da caixa (fica mais lento).
                bigCharacter->SetSpeedMultiplier(activePushBox->GetWeightMultiplier());
            }
        }
    }

    Box::SetActivePushTarget(activePushBox);

    // Salva o progresso ao SOLTAR (transição empurrando → parado), capturando a
    // nova posição da caixa.
    if (wasPushingLastFrame && !activePushBox) {
        SaveCurrentProgress();
    }
    wasPushingLastFrame = activePushBox != nullptr;

    // Som de arrasto (CAIXA_MADEIRALONGO): SÓ enquanto grudado numa caixa/barril E
    // o irmãozão está de fato SE MOVENDO. Parado (ou barril imóvel, que zera a
    // velocidade ao ser bloqueado) → silêncio.
    constexpr float kBoxDragMovingSpeed = 5.0f;
    const bool bigBroMoving = activePushBox && bigCharacter &&
                              bigCharacter->GetSpeed().Magnitude() > kBoxDragMovingSpeed;
    if (bigBroMoving) {
        GameSfx::MaintainBoxPushLoop();
    } else {
        GameSfx::PauseBoxPushLoop();
    }
}

void StageState::DetachActivePushBox() {
    if (!activePushBox) {
        return;
    }
    GameSfx::NotifyBoxPushEnd();
    activePushBox = nullptr;
    if (bigCharacter) {
        if (bigCharacter->currentState == Character::ActionState::PUSHING_BOX) {
            bigCharacter->currentState = Character::ActionState::NORMAL;
        }
        bigCharacter->SetSpeedMultiplier(1.0f);   // devolve a velocidade normal
    }
    Box::SetActivePushTarget(nullptr);
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
        // #2 O barulho de arrastar a caixa/barril chama o monstro para investigar.
        // NotifyNoise é auto-throttled (noiseCooldownTimer), então chamar por frame é seguro.
        const Vec2 noisePos = boxObj.box.Center();
        for (auto& go : GetObjectArray()) {
            if (Monster* m = go->GetComponent<Monster>()) {
                m->NotifyNoise(noisePos);
                break;
            }
        }
    } else {
        // A caixa/barril não se moveu. Barril IMÓVEL (peso 0) agora é SILENCIOSO:
        // nada acontece ao tentar movê-lo, para o jogador não conseguir distingui-lo
        // de um barril normal (sem o antigo "Ah, isso pesa...").
        bigCharacterObject->box.x = prevPlayerPos.x;
        bigCharacterObject->box.y = prevPlayerPos.y;
        if (Collider* playerCol = bigCharacterObject->GetComponent<Collider>()) {
            playerCol->Update(0);
        }
        bigCharacter->ClearMovement();
    }
}

bool StageState::RenderInteractionGlowIfNeeded(GameObject& go) {
    if (controlledCharacter != bigCharacter) {
        return false;
    }

    if (&go != GetInteractionFocus()) {
        return false;
    }

    Box* box = go.GetComponent<Box>();
    if (box && box->IsPushable()) {
        if (box == activePushBox) {
            DrawSpriteInteractionGlow(go, 255, 220, 0);
            return true;
        } else if (box == reachablePushBox) {
            if (IsHoldingLamp(inventory)) {
                DrawSpriteInteractionGlow(go, 255, 64, 64, 1.14f);
            } else {
                DrawSpriteInteractionGlow(go, 255, 255, 255);
            }
            return true;
        }
        return false;
    }

    Jornal* jornal = go.GetComponent<Jornal>();
    if (jornal && jornal == reachableJornal) {
        DrawSpriteInteractionGlow(go, 255, 255, 255, 1.14f);
        return true;
    }

    Candlestick* candle = go.GetComponent<Candlestick>();
    if (candle && candle == reachableCandle) {
        DrawSpriteInteractionGlow(go, 255, 255, 255, 1.14f);
        return true;
    }

    RadioAsset* radio = go.GetComponent<RadioAsset>();
    if (radio && radio == reachableRadio) {
        DrawSpriteInteractionGlow(go, 255, 255, 255, 1.14f);
        return true;
    }

    if (!reachablePickup || !IsPickupStillTracked(reachablePickup)) {
        return false;
    }
    if (reachablePickup->GetAssociated().IsDead()) {
        return false;
    }
    if (&reachablePickup->GetAssociated() == &go) {
        if (IsPickupBlocked(reachablePickup)) {
            DrawSpriteInteractionGlow(go, 255, 64, 64, 1.14f);
        } else {
            DrawSpriteInteractionGlow(go, 255, 255, 255, 1.14f);
        }
        return true;
    }
    return false;
}
