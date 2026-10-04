#include "gameplay/StairTrigger.h"
#include "states/stage/StageState.h"
#include "core/Game.h"
#include "gameplay/Character.h"

StairTrigger::StairTrigger(GameObject& associated, float anchorY) : Component(associated), anchorY(anchorY) {}

StairTrigger::~StairTrigger() {}

// Pé dentro do tapete: subindo (vy < -5) entra na escada e anota a base;
// descendo (vy > 5) sai dela. Parado ou andando de lado não muda nada.
void StairTrigger::ApplyCrossing(const Rect& zone, float zoneAnchorY, const Vec2& foot, float velocityY,
                                 bool& isElevated, float& stairAnchorY) {
    if (!zone.Contains(foot)) return;
    if (velocityY < -5.0f && !isElevated) {
        isElevated = true;
        stairAnchorY = zoneAnchorY;
    } else if (velocityY > 5.0f && isElevated) {
        isElevated = false;
    }
}

// Aplica a catraca aos dois irmãos (o monstro cuida de si no Monster::Update).
void StairTrigger::Update(float /*dt*/) {
    StageState* stage = Game::TryGetStageState();
    if (!stage) return;

    for (GameObject* player : {stage->GetBigCharacter(), stage->GetSmallCharacter()}) {
        Character* c = player ? player->GetComponent<Character>() : nullptr;
        if (!c) continue;
        const Vec2 foot(player->box.Center().x, player->box.y + player->box.h);
        ApplyCrossing(associated.box, anchorY, foot, c->GetSpeed().y, c->isElevated, c->stairAnchorY);
    }
}

void StairTrigger::Render() {}