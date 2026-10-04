// ─────────────────────────────────────────────────────────────────────────────
//  StageState::BuildHintContext — o único lugar que traduz o estado do andar
//  para o HintContext que os roteiros de tutorial leem.
// ─────────────────────────────────────────────────────────────────────────────
#include "states/stage/StageState.h"
#include "audio/GameVoice.h"
#include "core/InputManager.h"
#include "engine/GameObject.h"
#include "gameplay/Character.h"
#include "gameplay/Item.h"
#include "gameplay/Monster.h"

namespace {

// Topo da caixa (cabeça), meio (mãos) e base (pés) de um personagem, no mundo.
void BodyPoints(const GameObject* go, Vec2& head, Vec2* hands = nullptr, Vec2* feet = nullptr) {
    if (!go) return;
    const float cx = go->box.x + go->box.w * 0.5f;
    head = Vec2(cx, go->box.y);
    if (hands) *hands = Vec2(cx, go->box.y + go->box.h * 0.45f);
    if (feet)  *feet  = Vec2(cx, go->box.y + go->box.h);
}

bool IsLighter(const ItemDef& def) {
    return def.HasProperty(ItemProperty::LIGHT_SOURCE) && def.name != "Lamp";
}

}  // namespace

HintContext StageState::BuildHintContext() const {
    HintContext c;
    c.level          = currentLevelIndex;
    std::string subtitle;
    c.dialogueActive = dialogueBox.isActive() || GameVoice::GetActiveSubtitle(subtitle);
    c.overlayOpen    = IsPlayerInputFrozen() || sceneTransitionActive;
    c.controllingBig = controlledCharacter == bigCharacter;

    if (Monster* m = FindMonster()) {
        const Monster::MonsterState s = m->GetState();
        c.monsterHunting = s == Monster::MonsterState::CHASE || s == Monster::MonsterState::HUNT;
    }

    // Irmãos.
    c.partyReady    = IsPartyReady();
    c.partyTogether = partyMode == PartyMode::TOGETHER;
    BodyPoints(bigCharacterObject, c.bigHead, &c.bigHands, &c.bigFeet);
    BodyPoints(smallCharacterObject, c.smallHead);
    BodyPoints(companionCharacterObject, c.companionHead);
    if (bigCharacterObject && smallCharacterObject) {
        c.brothersDistance = bigCharacterObject->box.Center().Distance(smallCharacterObject->box.Center());
    }
    InputManager& input = InputManager::GetInstance();
    c.moveInput = input.ActionDown(GameAction::MoveUp) || input.ActionDown(GameAction::MoveDown) ||
                  input.ActionDown(GameAction::MoveLeft) || input.ActionDown(GameAction::MoveRight);
    c.useItemPressed = input.ActionPress(GameAction::UseItem);
    c.itemCount      = inventory.GetStackCount();
    c.cyclePressed   = input.ActionPress(GameAction::CyclePrev) || input.ActionPress(GameAction::CycleNext);

    // Luz e combustível: uma passada pela bolsa.
    c.lightOn       = inventory.IsUsableLightActive();
    c.lighterInHand = inventory.IsActiveItemLighter();
    c.canReload     = inventory.CanReload();
    c.reloading     = inventory.IsReloading();
    if (const Inventory::ItemStack* target = inventory.GetStack(inventory.FindReloadTarget())) {
        c.reloadTargetIcon = target->def.spritePath;
    }
    if (const Inventory::ItemStack* held = inventory.GetActiveStack()) {
        if (held->def.HasProperty(ItemProperty::LIGHT_SOURCE)) c.heldLightCharge = inventory.GetSelectedLightFuelRatio();
    }
    for (int i = 0; i < inventory.GetStackCount(); ++i) {
        const Inventory::ItemStack* st = inventory.GetStack(i);
        if (!st) continue;
        if (IsLighter(st->def)) {
            c.hasLighter  = true;
            c.lighterIcon = st->def.spritePath;
            const int charge = st->durabilities.empty() ? 0 : st->durabilities.front();
            c.lighterCharge = st->def.maxDurability > 0
                                  ? static_cast<float>(charge) / static_cast<float>(st->def.maxDurability)
                                  : 0.0f;
        } else if (st->def.HasProperty(ItemProperty::FUEL)) {
            c.fuelInBag += st->count;
            c.fuelIcon = st->def.spritePath;
        }
    }

    // Documentos e escada.
    c.documentsCollected = static_cast<int>(collectedDocuments.size());
    c.folderOpen         = documentFolderOpen;
    c.atHoleWithoutPlank = repairableInReachNoItem && c.controllingBig;
    c.atHoleNotHolding   = !repairableHeldItemNeeded.empty() && c.controllingBig;
    c.atHoleReady        = reachableRepairable != nullptr && c.controllingBig;
    for (int i = 0; i < inventory.GetStackCount(); ++i) {
        const Inventory::ItemStack* st = inventory.GetStack(i);
        if (st && st->def.name == repairableHeldItemNeeded) c.holeItemIcon = st->def.spritePath;
    }
    return c;
}
