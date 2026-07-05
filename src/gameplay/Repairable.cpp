#include "gameplay/Repairable.h"
#include "core/InputManager.h"
#include "engine/SpriteRenderer.h"
#include "states/stage/StageState.h"
#include "core/Game.h"
#include "engine/Camera.h"
#include "gameplay/Character.h"
#include <iostream>
#include <utility>

Repairable::Repairable(GameObject& associated, std::string fixedSpritePath, std::string requiredItem, std::string soundPath, float interactionDistance, Vec2 interactionOffset) 
    : Component(associated),
      fixedSpritePath(std::move(fixedSpritePath)),
      requiredItem(std::move(requiredItem)),
      soundPath(std::move(soundPath)),
      interactionDistance(interactionDistance),
      interactionOffset(interactionOffset),
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

    Vec2 exactInteractPoint(associated.box.x + interactionOffset.x,
                            associated.box.y + interactionOffset.y);
    float distance = exactInteractPoint.Distance(bigBro->box.Center());

    if (distance <= interactionDistance && bigChar && bigChar->isElevated) {

        // Só mostra o prompt se tiver o item necessário no inventário
        if (requiredItem.empty() || stage->GetInventory().HasItem(requiredItem)) {
            stage->SetReachableRepairable(this);
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
    if (isRepaired && !isRepairing) return;

#ifdef DEBUG
    
    SDL_Renderer* renderer = Game::GetInstance().GetRenderer();

    // 1. Ponto exato no mundo
    Vec2 exactInteractPoint(associated.box.x + interactionOffset.x, associated.box.y + interactionOffset.y);

    // 2. Subtrai a câmera para desenhar no lugar certo da tela
    float renderX = exactInteractPoint.x - Camera::pos.x;
    float renderY = exactInteractPoint.y - Camera::pos.y;

    // 3. Define a cor como AMARELO
    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);

    // 4. Desenha a Cruz
    SDL_RenderDrawLine(renderer, renderX - 10, renderY, renderX + 10, renderY);
    SDL_RenderDrawLine(renderer, renderX, renderY - 10, renderX, renderY + 10);

    // 5. Desenha o Quadrado da Área
    SDL_Rect debugRect;
    debugRect.x = (int)(renderX - interactionDistance);
    debugRect.y = (int)(renderY - interactionDistance);
    debugRect.w = (int)(interactionDistance * 2);
    debugRect.h = (int)(interactionDistance * 2);
    
    SDL_RenderDrawRect(renderer, &debugRect);

#endif
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
