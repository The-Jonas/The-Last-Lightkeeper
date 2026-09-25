#include "gameplay/HotbarComponent.h"
#include "core/CrashHandler.h"
#include "core/Telemetry.h"
#include "core/Game.h"
#include "core/InputManager.h"
#include "gameplay/Box.h"
#include "gameplay/Character.h"
#include "gameplay/ItemPickup.h"
#include "states/stage/StageState.h"
#include "audio/Sound.h"
#include "audio/GameSfx.h"
#include "audio/GameVoice.h"

#include <cstdlib>
#include <iostream>

namespace {

// Resultado de uma tentativa de pegar item: bloqueada (bolsa cheia), pega normal,
// ou pega que ENCHEU a bolsa (dispara fala diferente).
enum class PickupOutcome { Blocked, PickedUp, PickedUpAndFilled };

const char* kPickupSounds[] = {
    "Recursos/audio/pickup/pickup_1.mp3",
    "Recursos/audio/pickup/pickup_2.mp3",
    "Recursos/audio/pickup/pickup_3.mp3",
    "Recursos/audio/pickup/pickup_4.mp3",
    "Recursos/audio/pickup/pickup_5.mp3",
    "Recursos/audio/pickup/pickup_6.mp3",
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
    // Pegar item é um VFX → passa pelo barramento de efeitos (master × VFX).
    const int ch = gPickupSounds[idx].GetChannel();
    if (ch >= 0) {
        Mix_Volume(ch, GameSfx::CurrentSfxVolume());
    }
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
    closest->Destroy();
    PlayRandomPickupSound();
    CrashHandler::Log("pegou item: %s", def.name.c_str());
    Telemetry::Event("item_pickup", Telemetry::Fields()
        .Str("item", def.name)
        .Int("durability", pickupDurability)
        .Pos("", pickupPos.x, pickupPos.y));
    if (StageState* stage = Game::TryGetStageState()) {
        stage->NotifyItemPickupCollected(closest);
        stage->SaveCurrentProgress();
    }
    return inventory.IsFull() ? PickupOutcome::PickedUpAndFilled : PickupOutcome::PickedUp;
}

// Dispara a fala adequada ao resultado de pegar item: bolsa cheia → "bolsa
// pesada" (sem a fala normal de pegar); bloqueado → "não consigo"; senão,
// ocasionalmente comenta.
void VoiceForPickup(PickupOutcome outcome, const std::string& itemName, bool oilAtMax) {
    const bool woodPlank = (itemName == "Tabua de Madeira");
    switch (outcome) {
    case PickupOutcome::Blocked:
        // Óleo além do limite de combustível: "minha bolsa tá pesada" (a bolsa já
        // está cheia de óleo). Qualquer outro bloqueio: "não consigo".
        if (oilAtMax) GameVoice::OnBagFull();
        else          GameVoice::OnActionBlocked();
        break;
    case PickupOutcome::PickedUpAndFilled:
        if (woodPlank) {
            GameVoice::OnPickupWoodPlank();
            if (StageState* stage = Game::TryGetStageState()) {
                stage->QueueDialogue(DialogueBox::Speaker::LittleBrother, DialogueBox::Speaker::BigBrother,
                                     DialogueBox::Emotion::Normal, DialogueBox::Emotion::Doubt, "Isso pode ajudar a consertar a escada!");
            }
        }
        else GameVoice::OnBagFull();
        break;
    case PickupOutcome::PickedUp:
        // Tábua de madeira → SEMPRE "Isso vai servir."; demais itens → comentário ocasional.
        if (woodPlank) {
            GameVoice::OnPickupWoodPlank();
            if (StageState* stage = Game::TryGetStageState()) {
                stage->QueueDialogue(DialogueBox::Speaker::BigBrother, DialogueBox::Speaker::None,
                                     DialogueBox::Emotion::Normal, DialogueBox::Emotion::Normal, "Isso pode ajudar a consertar a escada!");
            }
        }
        else GameVoice::OnItemPickup();
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

    // Modo de reabastecimento: as setas / A / D navegam entre as fontes de luz
    // do modal central (e NÃO giram a roda). Confirmar é com [F] (TryUse...).
    if (inventory.IsOilPrimed()) {
        if (input.ActionPress(GameAction::CyclePrev) || input.ActionPress(GameAction::MoveLeft)) {
            inventory.RefuelSelectionPrev();
        }
        if (input.ActionPress(GameAction::CycleNext) || input.ActionPress(GameAction::MoveRight)) {
            inventory.RefuelSelectionNext();
        }
        return;
    }

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
}

void HotbarComponent::TryUseActiveItemOnKeyPress() {
    InputManager& input = InputManager::GetInstance();
    if (!input.ActionPress(GameAction::UseItem)) {
        return;
    }

    if (inventory.IsOilPrimed()) {
        // In oil-apply mode, F pours the oil into the centered lighter/lamp. On
        // an empty or invalid slot it does nothing (the oil is kept). ESC, which
        // exits this mode, is handled in the stage update.
        if (inventory.TryCombineOil()) {
            if (bigCharacter) {
                bigCharacter->NotifyInventoryLightChanged();
            }
        }
        return;
    }

    const Inventory::ItemStack* active = inventory.GetActiveStack();
    if (!active) return;

    if (active->def.HasProperty(ItemProperty::FUEL)) {
        // Sem nenhuma fonte de luz com espaço (tudo cheio), avisa em vez de ignorar o F.
        if (!inventory.TryPrimeOil()) {
            GameVoice::OnActionBlocked();
        }
        return;
    }

    if (!active->def.HasProperty(ItemProperty::LIGHT_SOURCE)) return;

    const bool wasOn = inventory.isLightToggledOn;
    // Decide "is this a lighter?" from the item type, NOT from the lit state:
    // IsActiveLightLighter() requires the light to already be on, so it would be
    // false at the moment we turn it ON (no turn-on sound would play).
    const bool isLighter = inventory.IsActiveItemLighter();
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
    (void)dt;
    if (!controlledCharacterPtr || !*controlledCharacterPtr || !bigCharacter) {
        return;
    }
    if (*controlledCharacterPtr != bigCharacter) {
        return;
    }
    // Congela o input do hotbar quando um overlay (menu de pausa/modal) está ativo.
    if (StageState* stage = Game::TryGetStageState(); stage && stage->IsPlayerInputFrozen()) {
        return;
    }

    TryCycleWheel();
    TryUseActiveItemOnKeyPress();
    TryPickupOnKeyPress();
}

void HotbarComponent::Render() {}
