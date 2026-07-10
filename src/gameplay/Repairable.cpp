#include "gameplay/Repairable.h"
#include "core/InputManager.h"
#include "engine/SpriteRenderer.h"
#include "states/stage/StageState.h"
#include "core/Game.h"
#include "engine/Camera.h"
#include "gameplay/Character.h"
#include <iostream>
#include <utility>

Repairable::Repairable(GameObject& associated, std::string fixedSpritePath, std::string requiredItem, std::string soundPath)
    : Component(associated),
      fixedSpritePath(std::move(fixedSpritePath)),
      requiredItem(std::move(requiredItem)),
      soundPath(std::move(soundPath)),
      isRepaired(false) {
}

Repairable::~Repairable() {
}

void Repairable::ApplyRepairedState() {
    if (isRepaired) {
        return;
    }
    SpriteRenderer* sprite = associated.GetComponent<SpriteRenderer>();
    if (sprite) {
        sprite->Open(fixedSpritePath);
    }
    isRepaired = true;
    StageState* stage = Game::TryGetStageState();
    if (stage) {
        stage->level.escadaConsertada = true;
    }
}

void Repairable::Update(float dt) {
    // Processa a animação de fade PRIMEIRO, antes de qualquer return
    if (isRepairing) {
        repairTimer += dt;

        // Trava os personagens durante a animação
        StageState* stage = Game::TryGetStageState();
        if (stage) {
            if (Character* big = stage->GetBigCharacterComponent())
                big->currentState = Character::ActionState::INTERACTING;
            if (Character* small = stage->GetSmallCharacterComponent())
                small->currentState = Character::ActionState::INTERACTING;
        }

        // Aplica o conserto no meio do fade
        if (repairTimer >= kRepairFadeIn && !isRepaired) {
            SpriteRenderer* sprite = associated.GetComponent<SpriteRenderer>();
            if (sprite) sprite->Open(fixedSpritePath);
            isRepaired = true;
            if (stage) {
                stage->level.escadaConsertada = true;
                stage->SaveCurrentProgress();
            }
        }

        // Terminou — libera os personagens
        if (repairTimer >= kRepairTotal) {
            isRepairing = false;
            if (stage) {
                if (Character* big = stage->GetBigCharacterComponent())
                    big->currentState = Character::ActionState::NORMAL;
                if (Character* small = stage->GetSmallCharacterComponent())
                    small->currentState = Character::ActionState::NORMAL;
            }
        }
        return;
    }

    // Já reparado e sem animação — não faz nada
    if (isRepaired) return;

    StageState* stage = Game::TryGetStageState();
    if (!stage) return;

    GameObject* bigBro = stage->GetBigCharacter();
    if (!bigBro) return;

    Character* bigChar = bigBro->GetComponent<Character>();
    if (!bigChar) return;

    // O gatilho agora é uma FORMA de colisão do mapa ("repairable_trigger"):
    // basta o irmãozão (caixa dos pés) encostar nela.
    const SDL_Rect footBox = bigChar->GetFootCollisionRect();
    const bool onTrigger = stage->level.CheckRepairableTrigger(footBox);

    if (onTrigger && bigChar->isElevated) {

        // Só mostra o prompt se tiver o item necessário no inventário; caso
        // contrário, sinaliza o aviso "preciso de uma tábua para consertar".
        if (requiredItem.empty() || stage->GetInventory().HasItem(requiredItem)) {
            stage->SetReachableRepairable(this);
        } else {
            stage->SetRepairableInReachNoItem(true);
        }
        
        if (!stage->IsPlayerInputFrozen() &&
            InputManager::GetInstance().ActionPress(GameAction::Interact)) {

            if (!requiredItem.empty()) {
                if (!stage->GetInventory().TryConsumeItem(requiredItem)) return;
            }

            isRepairing = true;
            repairTimer = 0.0f;
            GameSfx::PlayRepair();
        }
    }
}

void Repairable::Render() {
    // O gatilho é uma forma do mapa ("repairable_trigger") e é desenhado pelo
    // overlay de colisão do LevelManager (verde). Nada a desenhar aqui.
}

float Repairable::GetRepairOverlayAlpha() const {
    if (!isRepairing) return 0.0f;
    float alpha = 0.0f;
    if (repairTimer < kRepairFadeIn)
        alpha = repairTimer / kRepairFadeIn;
    else if (repairTimer < kRepairFadeIn + kRepairHold)
        alpha = 1.0f;
    else
        alpha = 1.0f - (repairTimer - kRepairFadeIn - kRepairHold) / kRepairFadeOut;
    return alpha;
}
