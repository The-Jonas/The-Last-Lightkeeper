#include "gameplay/DialogueTrigger.h"
#include "engine/GameObject.h"
#include "states/stage/StageState.h"
#include "core/Game.h"

DialogueTrigger::DialogueTrigger(GameObject& associated, std::vector<DialogueBox::Line> lines, bool once)
    : Component(associated), lines(std::move(lines)), once(once) {}

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

    GameObject* bigGO   = stage->GetBigCharacter();
    GameObject* smallGO = stage->GetSmallCharacter();
    if (!FootInsideTrigger(bigGO, associated.box) && !FootInsideTrigger(smallGO, associated.box)) {
        return;
    }

    for (const DialogueBox::Line& l : lines) {
        stage->QueueDialogue(l.speaker, l.listener, l.emotion, l.listenerEmotion, l.text);
    }
    fired = true;
}   