#include "gameplay/StairTrigger.h"
#include "states/stage/StageState.h"
#include "core/Game.h"
#include "gameplay/Character.h"
#include <iostream>

StairTrigger::StairTrigger(GameObject& associated, float anchorY) : Component(associated), anchorY(anchorY) {
}

StairTrigger::~StairTrigger() {}

void StairTrigger::Update(float dt) {
    StageState* stage = Game::TryGetStageState();
    if (!stage) {
        return;
    }

    GameObject* bigBro = stage->GetBigCharacter();
    GameObject* littleBro = stage->GetSmallCharacter();

    // Função lambda interna para checar os dois irmãos sem repetir código
    auto checkTrigger = [&](GameObject* player) {
        if (!player) return;
        
        Character* character = player->GetComponent<Character>();
        if (!character) return;

        // Pega exatamente o "pé" do personagem (centro no X, limite de baixo no Y)
        Vec2 footPos(player->box.Center().x, player->box.y + player->box.h);

        // Se o pé pisou no tapete
        if (associated.box.Contains(footPos)) {
            
            // Catraca de Entrada: Indo para cima e está no chão
            if (character->GetSpeed().y < -5.0f && !character->isElevated) {
                character->isElevated = true;

                // ====================================================
                // O personagem anota qual é a base dessa escada!
                // ====================================================
                character->stairAnchorY = this->anchorY;

                std::cout << "Subiu na escada!" << std::endl; // Debug  
            }
            // Catraca de Saída: Indo para baixo e está na escada
            else if (character->GetSpeed().y > 5.0f && character->isElevated) {
                character->isElevated = false;
                std::cout << "Desceu da escada!" << std::endl; // Debug
            }
        }
    };

    checkTrigger(bigBro);
    checkTrigger(littleBro);
}

void StairTrigger::Render() {
}