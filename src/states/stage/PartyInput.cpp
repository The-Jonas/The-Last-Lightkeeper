#include "states/stage/StageState.h"
#include "states/stage/InternalHelpers.h"
#include "core/Game.h"
#include "engine/GameObject.h"
#include "engine/SpriteRenderer.h"
#include "math/Rect.h"
#include "world/TileSet.h"
#include "world/TileMap.h"
#include "core/InputManager.h"
#include "engine/Camera.h"
#include "engine/Component.h"
#include "gameplay/Character.h"
#include "world/Collider.h"
#include "world/Collision.h"
#include "core/GameData.h"
#include "states/EndState.h"
#include "ui/Text.h"
#include "lighting/TopDownLightShadows.h"
#include "lighting/LightShadowProfile.h"
#include "gameplay/Item.h"
#include "gameplay/ItemPickup.h"
#include "gameplay/HotbarComponent.h"
#include "gameplay/Box.h"
#include "ui/FadeEffect.h"
#include "gameplay/Repairable.h"
#include "gameplay/StairTrigger.h"
#include "audio/GameVoice.h"
#include "core/Resources.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <array>
#include <queue>
#include <limits>
#include <unordered_map>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace stage_internal;
bool StageState::IsPartyReady() const { // Verifica se a dupla está pronta (ambos os personagens estão presentes)
    return controlledCharacter && controlledCharacterObject && companionCharacter && companionCharacterObject;
}
void StageState::SwapControlledCharacter() {
    if (!IsPartyReady()) {
        return;
    }

    std::swap(controlledCharacter, companionCharacter);
    std::swap(controlledCharacterObject, companionCharacterObject);

    TriggerControlIndicator();   // mostra/anima o indicador no novo personagem
}

void StageState::HandlePartyInput() {
    InputManager& input = InputManager::GetInstance();

    if (input.ActionPress(GameAction::SwapBrother)) {
        SwapControlledCharacter();
    }

    if (input.ActionPress(GameAction::ToggleMode)) {
        if (partyMode == PartyMode::TOGETHER) {
            partyMode = PartyMode::INDEPENDENT;
            GameVoice::OnAskToStay();   // mandou ficar parado → irmãozinho protesta
        } else {
            partyMode = PartyMode::TOGETHER;
            // Chamou para segui-lo: "Aqui", mas só quando o irmão está longe.
            if (bigCharacterObject && smallCharacterObject) {
                const float dist = bigCharacterObject->box.Center().Distance(
                    smallCharacterObject->box.Center());
                if (dist > 350.0f) {
                    GameVoice::OnCallToFollow();
                }
            }
        }
    }
}

void StageState::IssueMovementFromInput(Character* character, GameObject* object) {
    if (!character || !object) {
        return;
    }

    // The companion follow logic scales the follower's speed (catch-up up to
    // 1.55x, arrival easing down to 0.45x). When control swaps onto a character
    // that was just following, that leftover multiplier would make it move too
    // fast/slow. The player-controlled character always moves at base speed.
    character->SetSpeedMultiplier(1.0f);

    InputManager& input = InputManager::GetInstance();
    Vec2 direction(0.0f, 0.0f);

    if (input.ActionDown(GameAction::MoveUp)) {
        direction.y -= 1.0f;
    }
    if (input.ActionDown(GameAction::MoveDown)) {
        direction.y += 1.0f;
    }
    if (input.ActionDown(GameAction::MoveLeft)) {
        direction.x -= 1.0f;
    }
    if (input.ActionDown(GameAction::MoveRight)) {
        direction.x += 1.0f;
    }

    if (direction.Magnitude() > 0.0f) {
        Vec2 normalized = direction.Normalized();
        Character::Command moveCommand(Character::Command::MOVE, object->box.Center().x + normalized.x, object->box.Center().y + normalized.y);
        character->Issue(moveCommand);
    }
}

void StageState::IssueFollowCommand(Character* follower, GameObject* followerObject, GameObject* leaderObject, bool allowCatchup) {
    companionFollowPathWorld.clear();
 
    if (!follower || !followerObject || !leaderObject) {
        return;
    }
 
    const float preferredDistance = 68.0f;
    const float overlapDistance = 40.0f;
    const float followStartDistance = 82.0f;
    const float catchupDistance = 420.0f;
 
    Vec2 leaderCenter = leaderObject->box.Center();
    Vec2 followerCenter = followerObject->box.Center();
    Vec2 toLeader = leaderCenter - followerCenter;
    float distance = toLeader.Magnitude();
 
    follower->SetSpeedMultiplier(1.0f);
    if (distance < overlapDistance) {
        if (distance > 0.01f) {
            Vec2 pushDir = (followerCenter - leaderCenter).Normalized();
            Character::Command moveAway(Character::Command::MOVE, followerCenter.x + pushDir.x, followerCenter.y + pushDir.y);
            follower->Issue(moveAway);
        }
        return;
    }
 
    if (distance < followStartDistance) {
        return;
    }
 
    Vec2 dir = toLeader.Normalized();
    Vec2 targetPos = leaderCenter - (dir * preferredDistance);

    // 1.1 — easing de chegada: corre quando longe (catch-up), desacelera ao se
    // aproximar (em vez de manter velocidade cheia e ultrapassar/oscilar).
    const float arrivalEaseDist = 210.0f;
    float speedMul = 1.0f;
    if (allowCatchup && distance > catchupDistance) {
        speedMul = 1.55f;
    } else if (distance < arrivalEaseDist) {
        float t = (distance - followStartDistance) / (arrivalEaseDist - followStartDistance);
        t = std::clamp(t, 0.0f, 1.0f);
        speedMul = 0.45f + 0.55f * t;   // ~0.45 perto do deadband → 1.0 longe
    }
    follower->SetSpeedMultiplier(speedMul);

    Vec2 followTarget = targetPos;

    const Character* leaderChar = leaderObject->GetComponent<Character>();
    const bool elevationMismatch = leaderChar && follower->isElevated != leaderChar->isElevated;

    if (HasNavigationGrid() && !elevationMismatch && !HasWalkableLine(followerCenter, targetPos, followerObject)) {

        // ── THROTTLE: só recalcula o A* a cada kCompanionPathRefreshInterval,
        // reaproveitando o último caminho calculado nos frames intermediários.
        if (companionPathRefreshTimer <= 0.0f) {
            companionPathRefreshTimer = kCompanionPathRefreshInterval;
            cachedCompanionPath = FindPathWorld(followerCenter, targetPos, followerObject);
            companionPathIndex = (cachedCompanionPath.size() >= 2) ? 1 : 0;  // pula [0]=posição atual
        }

        companionFollowPathWorld = cachedCompanionPath;
        if (!companionFollowPathWorld.empty()) {
            if ((companionFollowPathWorld.front() - followerCenter).Magnitude() > 3.0f) {
                companionFollowPathWorld.insert(companionFollowPathWorld.begin(), followerCenter);
            }
        } else {
            companionFollowPathWorld = {followerCenter, targetPos};
        }

        if (!cachedCompanionPath.empty()) {
            // 1.1 — avança pelos waypoints conforme o seguidor os alcança, fluindo
            // ao longo da rota em vez de mirar sempre o mesmo path[1] por 0.35s.
            const int last = static_cast<int>(cachedCompanionPath.size()) - 1;
            if (companionPathIndex > last) {
                companionPathIndex = last;
            }
            const float kWaypointReachDist = 26.0f;
            while (companionPathIndex < last &&
                   followerCenter.Distance(cachedCompanionPath[static_cast<size_t>(companionPathIndex)]) < kWaypointReachDist) {
                ++companionPathIndex;
            }
            followTarget = cachedCompanionPath[static_cast<size_t>(companionPathIndex)];
        }
    } else {
        // Linha livre — não precisa de A*, zera o throttle pro próximo bloqueio.
        companionPathRefreshTimer = 0.0f;
        cachedCompanionPath.clear();
        companionPathIndex = 0;
        companionFollowPathWorld = {followerCenter, targetPos};
    }

    Character::Command followCommand(Character::Command::MOVE, followTarget.x, followTarget.y);
    follower->Issue(followCommand);
}

void StageState::UpdateCompanionBehavior() {
    if (!IsPartyReady()) {
        companionFollowPathWorld.clear();
        return;
    }

    if (partyMode == PartyMode::TOGETHER) {
        IssueFollowCommand(companionCharacter, companionCharacterObject, controlledCharacterObject, true);
        return;
    }

    companionFollowPathWorld.clear();
    companionCharacter->SetSpeedMultiplier(1.0f);
}

void StageState::EnforceMaxDistance() {
    if (!IsPartyReady()) {
        return;
    }
    if (partyMode != PartyMode::TOGETHER) {
        return;
    }
    // No hard snap/teleport when party members are far apart.
    // Companion catch-up is handled smoothly by IssueFollowCommand().
}

void StageState::RefreshCameraTargets() {
    if (controlledCharacterObject) {
        Camera::Follow(controlledCharacterObject);
    }
}

void StageState::UpdateControlledCharacterVisuals() {
    if (!bigCharacterObject || !smallCharacterObject || !controlledCharacterObject) {
        return;
    }

    SpriteRenderer* bigSprite = bigCharacterObject->GetComponent<SpriteRenderer>();
    SpriteRenderer* smallSprite = smallCharacterObject->GetComponent<SpriteRenderer>();
    if (!bigSprite || !smallSprite) {
        return;
    }

    if (controlledCharacterObject == bigCharacterObject) {
        bigSprite->SetTint(255, 255, 255, 255);
        smallSprite->SetTint(120, 170, 230, 215);
    } else {
        bigSprite->SetTint(190, 190, 190, 220);
        smallSprite->SetTint(170, 230, 255, 255);
    }
}

void StageState::UpdateHudInstructions() {
    const float startX = 16.0f;
    const float startY = 12.0f;
    const float lineGap = 22.0f;

    if (hudLine1) {
        hudLine1->box.x = Camera::pos.x + startX;
        hudLine1->box.y = Camera::pos.y + startY;
    }

    if (hudLine2) {
        hudLine2->box.x = Camera::pos.x + startX;
        hudLine2->box.y = Camera::pos.y + startY + lineGap;
    }

    if (hudLine3) {
        hudLine3->box.x = Camera::pos.x + startX;
        hudLine3->box.y = Camera::pos.y + startY + lineGap * 2.0f;
    }

    if (hudFps) {
        hudFps->box.x = Camera::pos.x + startX;
        hudFps->box.y = Camera::pos.y + startY + lineGap * 3.0f;
    }
}
