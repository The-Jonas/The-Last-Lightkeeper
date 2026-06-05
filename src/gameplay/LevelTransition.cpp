#include "gameplay/LevelTransition.h"
#include "states/stage/StageState.h"
#include "core/Game.h"

namespace {
bool RectsOverlap(const Rect& a, const Rect& b) {
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}
}  // namespace

LevelTransition::LevelTransition(GameObject& associated, int targetLevelIndex)
    : Component(associated), targetLevelIndex(targetLevelIndex) {
}

void LevelTransition::Update(float dt) {
    if (cooldownTimer > 0.0f) {
        cooldownTimer -= dt;
        return;
    }

    StageState* stage = Game::TryGetStageState();
    if (!stage) {
        return;
    }

    GameObject* bigBro = stage->GetBigCharacter();
    if (!bigBro) {
        return;
    }

    if (!RectsOverlap(bigBro->box, associated.box)) {
        return;
    }

    cooldownTimer = 1.0f;
    stage->BeginLevelTransition(targetLevelIndex);
}

void LevelTransition::Render() {
}
