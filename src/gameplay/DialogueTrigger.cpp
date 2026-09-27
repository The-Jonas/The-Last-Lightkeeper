#include "gameplay/DialogueTrigger.h"
#include "engine/GameObject.h"
#include "states/stage/StageState.h"
#include "core/Game.h"

DialogueTrigger::DialogueTrigger(GameObject& associated, std::vector<DialogueBox::Line> lines, bool once,
                                 std::string context)
    : Component(associated), lines(std::move(lines)), once(once), context(std::move(context)) {}

// Mesmo teste de "pé dentro do retângulo" que o CurtainTrigger já usa.
static bool FootInsideTrigger(GameObject* go, const Rect& box) {
    if (!go) return false;
    Vec2 foot(go->box.Center().x, go->box.y + go->box.h);
    return box.Contains(foot);
}

void DialogueTrigger::Update(float dt) {
    if (once && fired) return;

    StageState* stage = Game::TryGetStageState();
    if (!stage) return;

    // Save carregado: a conversa deste gatilho já aconteceu, não repete.
    const std::string key = "trigger:" + std::to_string(associated.tiledId);
    if (once && stage->IsDialogueLogged(key)) {
        fired = true;
        return;
    }

    GameObject* bigGO   = stage->GetBigCharacter();
    GameObject* smallGO = stage->GetSmallCharacter();
    if (!FootInsideTrigger(bigGO, associated.box) && !FootInsideTrigger(smallGO, associated.box)) {
        return;
    }

    stage->PlayDialogue(key, context, lines);
    fired = true;
}