#include "gameplay/HotbarComponent.h"
#include "core/CrashHandler.h"
#include "core/Telemetry.h"
#include "core/Game.h"
#include "core/InputManager.h"
#include "gameplay/Box.h"
#include "gameplay/Character.h"
#include "gameplay/ItemPickup.h"
#include "states/stage/StageState.h"
#include "states/stage/FirstLoadData.h"
#include "audio/Sound.h"
#include "audio/GameSfx.h"
#include "audio/GameVoice.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace {

// Resultado de uma tentativa de pegar item: bloqueada (bolsa cheia), pega normal,
// ou pega que ENCHEU a bolsa (dispara fala diferente).
enum class PickupOutcome { Blocked, PickedUp, PickedUpAndFilled };

// Sprite do combustível (do catálogo de itens), para o balão "preciso de combustível".
const std::string& FuelIconPath() {
    static const std::string path = [] {
        for (const ItemDef& def : LoadStageFirstLoadData().pickupCycle) {
            if (def.HasProperty(ItemProperty::FUEL)) return def.spritePath;
        }
        return std::string();
    }();
    return path;
}

constexpr float kNoFuelThoughtSeconds = 2.5f;           // s do balão quando aperta [R] sem combustível

// Nome do item em português para a origem no log ("Ao pegar: Tábua de madeira").
std::string ItemLabelPt(const std::string& name) {
    if (name == "Flashlight")       return "Isqueiro";
    if (name == "Lamp")             return "Lamparina";
    if (name == "Fuel")             return "Combustível";
    if (name == "Apple")            return "Maçã";
    if (name == "Tabua de Madeira") return "Tábua de madeira";
    return name;
}

PickupOutcome PerformPickup(Inventory& inventory, ItemPickup* closest, std::vector<ItemPickup*>& itemPickups,
                            Character* bigChar) {
    const ItemDef& def = *closest->GetDef();
    const Vec2 pickupPos = closest->GetAssociated().box.Center();
    const int pickupDurability = closest->GetDurability();
    if (!inventory.AddItem(def, closest->GetDurability())) {
        // Tentou apanhar e NAO conseguiu. Vale tanto como o sucesso: diz que o
        // jogador quis aquele item e a bolsa estava cheia.
        Telemetry::Event("item_blocked", Telemetry::Fields()
            .Str("item", def.name)
            .Pos("", pickupPos.x, pickupPos.y));
        return PickupOutcome::Blocked;   // bolsa cheia
    }

    for (auto& p : itemPickups) {
        if (p == closest) {
            p = nullptr;
            break;
        }
    }

    // Guarda a conversa antes de destruir o pickup.
    const std::vector<DialogueBox::Line> pickupLines = closest->GetDialogueLines();
    std::string pickupContext = closest->GetDialogueContext();
    if (pickupContext.empty()) pickupContext = "Ao pegar: " + ItemLabelPt(def.name);
    const int pickupTiledId = closest->GetAssociated().tiledId;

    closest->Destroy();
    GameSfx::PlayItemPickup(def.name);                 // som próprio do item (ou genérico)
    CrashHandler::Log("pegou item: %s", def.name.c_str());
    Telemetry::Event("item_pickup", Telemetry::Fields()
        .Str("item", def.name)
        .Int("durability", pickupDurability)
        .Pos("", pickupPos.x, pickupPos.y));
    if (StageState* stage = Game::TryGetStageState()) {
        stage->NotifyItemPickupCollected(closest);
        if (!pickupLines.empty()) {
            stage->PlayDialogue("item:" + std::to_string(pickupTiledId), pickupContext, pickupLines);
        }
        stage->SaveCurrentProgress();
    }
    return inventory.IsFull() ? PickupOutcome::PickedUpAndFilled : PickupOutcome::PickedUp;
}

// Dispara a fala adequada ao resultado de pegar item: bolsa cheia → "bolsa
// pesada" (sem a fala normal de pegar); bloqueado → "não consigo"; senão,
// ocasionalmente comenta. A conversa da tábua vem do Tiled (ItemSpawn: dialogue).
void VoiceForPickup(PickupOutcome outcome, const std::string& itemName, bool oilAtMax) {
    const bool woodPlank = (itemName == "Tabua de Madeira");
    switch (outcome) {
    case PickupOutcome::Blocked:
        // Óleo além do limite de combustível: "minha bolsa tá pesada".
        // Qualquer outro bloqueio: "não consigo".
        if (oilAtMax) {
            // O tutorial cuida: fala na caixa ("não consigo carregar mais…") + anel na roda.
            if (StageState* stage = Game::TryGetStageState()) stage->Hints().NotifyFuelLimitHit();
        } else {
            GameVoice::OnActionBlocked();
        }
        break;
    case PickupOutcome::PickedUpAndFilled:
        if (woodPlank) GameVoice::OnPickupWoodPlank();
        else           GameVoice::OnBagFull();
        break;
    case PickupOutcome::PickedUp:
        // Tábua de madeira → SEMPRE "Isso vai servir."; demais itens → comentário ocasional.
        if (woodPlank) GameVoice::OnPickupWoodPlank();
        else           GameVoice::OnItemPickup();
        break;
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

void HotbarComponent::TryCycleWheel() {
    InputManager& input = InputManager::GetInstance();
    bool cycled = false;
    if (input.ActionPress(GameAction::CyclePrev)) {
        inventory.CycleLeft();
        cycled = true;
    }
    if (input.ActionPress(GameAction::CycleNext)) {
        inventory.CycleRight();
        cycled = true;
    }

    // The item that just moved to the wheel's center is now the usable item, so
    // refresh the held-prop visual (lighter/lamp in hand) right away.
    if (cycled && bigCharacter) {
        bigCharacter->NotifyInventoryLightChanged();
    }
    if (cycled && inventory.GetStackCount() >= 2) {
        GameSfx::PlayItemCycle();   // vasculhando a mochila (com 1 item só não há o que trocar)
    }
}

void HotbarComponent::TryUseActiveItemOnKeyPress() {
    InputManager& input = InputManager::GetInstance();
    if (!input.ActionPress(GameAction::UseItem)) {
        return;
    }

    const Inventory::ItemStack* active = inventory.GetActiveStack();
    if (!active) return;

    // Combustível não se "usa" com [F]: recarregar é só pelo [R].
    if (active->def.HasProperty(ItemProperty::FUEL)) return;

    if (!active->def.HasProperty(ItemProperty::LIGHT_SOURCE)) return;

    // [F] no meio do "acender": desiste e fecha a tampa.
    if (igniteTimer >= 0.0f) {
        CancelLighterIgnite(true);
        return;
    }

    const bool wasOn = inventory.isLightToggledOn;
    // Decide "is this a lighter?" from the item type, NOT from the lit state:
    // IsActiveLightLighter() requires the light to already be on, so it would be
    // false at the moment we turn it ON (no turn-on sound would play).
    const bool isLighter = inventory.IsActiveItemLighter();
    if (wasOn) {
        inventory.isLightToggledOn = false;              // fechar é imediato
        if (isLighter) GameSfx::PlayLighterToggle(false);
        else           GameSfx::PlayLampToggle(false);
    } else if (isLighter) {
        if (inventory.CanTurnLightOn()) BeginLighterIgnite();   // a luz vem com a chama do som
    } else if (inventory.TryTurnLightOn()) {
        GameSfx::PlayLampToggle(true);                   // lamparina: acende na hora
    }
}

// Toca abrir+riscar e espera o instante em que a chama pega no som.
void HotbarComponent::BeginLighterIgnite() {
    igniteTimer = std::max(0.0f, GameSfx::PlayLighterIgnite());
}

// Desiste do acender em curso (trocou de item, [F] de novo, recarga…).
void HotbarComponent::CancelLighterIgnite(bool playClose) {
    if (igniteTimer < 0.0f) return;
    igniteTimer = -1.0f;
    GameSfx::CancelLighterIgnite();
    if (playClose) GameSfx::PlayLighterToggle(false);
}

// Conta até a chama; se o isqueiro saiu da mão (ou ficou sem carga), desiste.
void HotbarComponent::UpdateLighterIgnite(float dt) {
    if (igniteTimer < 0.0f) return;
    if (!inventory.IsActiveItemLighter() || inventory.IsReloading()) {
        CancelLighterIgnite(false);
        return;
    }
    igniteTimer -= dt;
    if (igniteTimer > 0.0f) return;
    igniteTimer = -1.0f;
    if (inventory.TryTurnLightOn() && bigCharacter) bigCharacter->NotifyInventoryLightChanged();
}

void HotbarComponent::TryPickupOnKeyPress() {
    InputManager& input = InputManager::GetInstance();
    StageState* stage = Game::TryGetStageState();
    if (!stage) {
        return;
    }

    if (!input.ActionPress(GameAction::Interact)) {
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
    const bool windowWins = stage->GetReachableWindow() && stage->IsWindowClosestForInteraction(stage->GetReachableWindow());
    if (boxWins || jornalWins || candleWins || windowWins) {
        return;
    }

    const int hLevel = closest->GetHeightLevel();
    if (hLevel == 0 || hLevel == 1) {
        const std::string pickedName = closest->GetDef() ? closest->GetDef()->name : std::string();
        // Antes de pegar: era óleo E a bolsa já estava no limite de combustível?
        const bool oilAtMax = closest->GetDef() &&
                              closest->GetDef()->HasProperty(ItemProperty::FUEL) &&
                              inventory.IsFuelAtMax();
        const PickupOutcome outcome = PerformPickup(inventory, closest, itemPickups, bigCharacter);
        VoiceForPickup(outcome, pickedName, oilAtMax);
        if (outcome == PickupOutcome::Blocked) {
            return;   // bolsa cheia: nada foi pego
        }
        if (Character::player) {
            Character::player->currentState = Character::ActionState::INTERACTING;
            Character::player->interactTimer = 0.3f;
        }
        if (bigCharacter) {
            bigCharacter->NotifyInventoryLightChanged();
        }
        return;
    }

    if (hLevel == 2) {
        const std::string pickedName = closest->GetDef() ? closest->GetDef()->name : std::string();
        // Antes de pegar: era óleo E a bolsa já estava no limite de combustível?
        const bool oilAtMax = closest->GetDef() &&
                              closest->GetDef()->HasProperty(ItemProperty::FUEL) &&
                              inventory.IsFuelAtMax();
        const PickupOutcome outcome = PerformPickup(inventory, closest, itemPickups, bigCharacter);
        VoiceForPickup(outcome, pickedName, oilAtMax);
        if (outcome == PickupOutcome::Blocked) {
            return;
        }
        if (Character::player) {
            Character::player->currentState = Character::ActionState::INTERACTING;
            Character::player->interactTimer = 0.4f;  
        }
        if (bigCharacter) {
            bigCharacter->NotifyInventoryLightChanged();
        }
    }
}

void HotbarComponent::Update(float dt) {
    if (!controlledCharacterPtr || !*controlledCharacterPtr || !bigCharacter) {
        return;
    }
    UpdateLighterIgnite(dt);                             // a chama pega mesmo se trocar de irmão no meio
    if (*controlledCharacterPtr != bigCharacter) {
        return;
    }
    // Congela o input do hotbar quando um overlay (menu de pausa/modal) está ativo.
    if (StageState* stage = Game::TryGetStageState(); stage && stage->IsPlayerInputFrozen()) {
        return;
    }

    // Recarregando: a roda, o [F] e pegar itens ficam travados até terminar.
    if (inventory.IsReloading()) {
        UpdateReload(dt);
        return;
    }
    if (InputManager::GetInstance().ActionPress(GameAction::Reload)) {
        StartReload();
        return;
    }

    TryCycleWheel();
    TryUseActiveItemOnKeyPress();
    TryPickupOnKeyPress();
}

// [R]: apaga a luz e começa a recarga. Sem combustível: o irmãozão "pensa" no
// combustível (balão sobre a cabeça). Com tudo cheio: "não consigo".
void HotbarComponent::StartReload() {
    const bool wasLighterOn = inventory.IsActiveLightLighter();
    const bool wasLampOn = inventory.IsActiveLightLamp();
    const bool wasIgniting = igniteTimer >= 0.0f;
    const int  target = inventory.FindReloadTarget();
    const bool lampTarget = target >= 0 && inventory.GetStack(target)->def.name == "Lamp";
    if (!inventory.BeginReload()) {
        StageState* stage = Game::TryGetStageState();
        if (inventory.GetFuelUnits() == 0 && stage) {
            stage->Hints().ShowThought(FuelIconPath(), kNoFuelThoughtSeconds);
        } else {
            GameVoice::OnActionBlocked();
        }
        return;
    }
    if (wasIgniting)       CancelLighterIgnite(true);   // estava abrindo: fecha a tampa
    else if (wasLighterOn) GameSfx::PlayLighterToggle(false);
    else if (wasLampOn)    GameSfx::PlayLampToggle(false);
    GameSfx::PlayReloadPour(lampTarget);                 // fluido escorrendo durante os 2 s
    if (bigCharacter) bigCharacter->NotifyInventoryLightChanged();
    Telemetry::Event("reload_start", Telemetry::Fields().Int("fuelUnits", inventory.GetFuelUnits()));
}

// Conta a recarga; no fim a luz volta acesa (com o som do isqueiro, se for ele).
void HotbarComponent::UpdateReload(float dt) {
    if (!inventory.TickReload(dt)) return;
    // O isqueiro reabre e risca: a luz volta só quando a chama pega (a lamparina, na hora).
    if (inventory.IsActiveLightLighter()) {
        inventory.isLightToggledOn = false;
        BeginLighterIgnite();
    } else if (inventory.IsActiveLightLamp()) {
        GameSfx::PlayLampToggle(true);
    }
    if (bigCharacter) bigCharacter->NotifyInventoryLightChanged();
    Telemetry::Event("reload_done", Telemetry::Fields().Num("charge", inventory.GetSelectedLightFuelRatio()));
}

void HotbarComponent::Render() {}


