#include "gameplay/CurtainTrigger.h"
#include "gameplay/Curtain.h"
#include "gameplay/Character.h"
#include "engine/GameObject.h"
#include "engine/Camera.h"
#include "states/stage/StageState.h"
#include "core/Game.h"
#include <iostream>

CurtainTrigger::CurtainTrigger(GameObject& associated, int curtainId,
                                CurtainTriggerAction action)
    : Component(associated),
      curtainId(curtainId),
      action(action) {}

// Acha a cortina com o mesmo curtainId no stage
static Curtain* FindCurtain(int id) {
    StageState* stage = Game::TryGetStageState();
    if (!stage) return nullptr;
    for (const auto& goPtr : stage->GetObjectArray()) {
        if (!goPtr || goPtr->IsDead()) continue;
        Curtain* c = goPtr->GetComponent<Curtain>();
        if (c && c->GetCurtainId() == id) return c;
    }
    return nullptr;
}

// Verifica se o pé do personagem está dentro do trigger
static bool FootInside(GameObject* go, const Rect& triggerBox) {
    if (!go) return false;
    Vec2 foot(go->box.Center().x, go->box.y + go->box.h);
    return triggerBox.Contains(foot);
}

void CurtainTrigger::Update(float dt) {

    StageState* stage = Game::TryGetStageState();
    if (!stage) return;

    GameObject* bigGO   = stage->GetBigCharacter();
    GameObject* smallGO = stage->GetSmallCharacter();
    const Rect& box     = associated.box;

    if (bigGO) {
        Vec2 foot(bigGO->box.Center().x, bigGO->box.y + bigGO->box.h);
    }

    Curtain* curtain = FindCurtain(curtainId);
    if (!curtain) return;

    if (action == CurtainTriggerAction::OPEN) {
        // Qualquer irmão entra -> abre
        if (FootInside(bigGO, box) || FootInside(smallGO, box)) {
            curtain->Open();
        }

    } else { // CLOSE
        // Rastreia entrada dos dois irmãos
        if (FootInside(bigGO,   box)) bigEntered   = true;
        if (FootInside(smallGO, box)) smallEntered = true;

        // Ambos entraram -> fecha
        if (bigEntered && smallEntered) {
            curtain->Close();
            bigEntered   = false;
            smallEntered = false;
        }

        // Reset se a cortina foi reaberta (para não fechar imediatamente)
        if (curtain->IsOpen() == false) {
            bigEntered   = false;
            smallEntered = false;
        }
    }
}

void CurtainTrigger::Render() {
#ifdef DEBUG
    SDL_Renderer* r = Game::GetInstance().GetRenderer();
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    SDL_Rect box = {
        static_cast<int>(associated.box.x - Camera::pos.x),
        static_cast<int>(associated.box.y - Camera::pos.y),
        static_cast<int>(associated.box.w),
        static_cast<int>(associated.box.h)
    };

    // Amarelo = trigger de abrir, Laranja = trigger de fechar
    bool isOpen = (action == CurtainTriggerAction::OPEN);
    SDL_SetRenderDrawColor(r, isOpen?255:255, isOpen?255:140, 0, 50);
    SDL_RenderFillRect(r, &box);
    SDL_SetRenderDrawColor(r, isOpen?255:255, isOpen?255:140, 0, 200);
    SDL_RenderDrawRect(r, &box);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
#endif
}