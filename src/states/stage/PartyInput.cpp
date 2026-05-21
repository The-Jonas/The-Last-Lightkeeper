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
}

void StageState::HandlePartyInput() {
    InputManager& input = InputManager::GetInstance();

    if (input.KeyPress(SDLK_LCTRL) || input.KeyPress(SDLK_RCTRL)) {
        SwapControlledCharacter();
    }

    if (input.KeyPress(TOGGLE_MODE_KEY)) {
        if (partyMode == PartyMode::TOGETHER) {
            partyMode = PartyMode::INDEPENDENT;
        } else {
            partyMode = PartyMode::TOGETHER;
        }
    }
}

void StageState::IssueMovementFromInput(Character* character, GameObject* object) {
    if (!character || !object) {
        return;
    }

    InputManager& input = InputManager::GetInstance();
    Vec2 direction(0.0f, 0.0f);

    if (input.IsKeyDown(SDLK_w)) {
        direction.y -= 1.0f;
    }
    if (input.IsKeyDown(SDLK_s)) {
        direction.y += 1.0f;
    }
    if (input.IsKeyDown(SDLK_a)) {
        direction.x -= 1.0f;
    }
    if (input.IsKeyDown(SDLK_d)) {
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
    if (allowCatchup && distance > catchupDistance) {
        follower->SetSpeedMultiplier(1.55f);
    }
    Vec2 followTarget = targetPos;
    if (HasNavigationGrid() && !HasWalkableLine(followerCenter, targetPos, followerObject)) {
        const std::vector<Vec2> path = FindPathWorld(followerCenter, targetPos, followerObject);
        companionFollowPathWorld = path;
        if (!companionFollowPathWorld.empty()) {
            if ((companionFollowPathWorld.front() - followerCenter).Magnitude() > 3.0f) {
                companionFollowPathWorld.insert(companionFollowPathWorld.begin(), followerCenter);
            }
        } else {
            // A* falhou: ainda mostramos o desejo de ir em linha reta até o alvo (mesmo que o movimento use só `targetPos`).
            companionFollowPathWorld = {followerCenter, targetPos};
        }
        if (path.size() >= 2) {
            followTarget = path[1];
        } else if (!path.empty()) {
            followTarget = path.front();
        }
    } else {
        // Linha de visada livre (ou sem grade): mostramos o segmento direto até o ponto-alvo atrás do líder.
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
