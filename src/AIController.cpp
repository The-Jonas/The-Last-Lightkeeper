#include "../include/AIController.h"
#include "../include/GameObject.h"
#include "../include/Character.h"
#include "../include/Gun.h"
#include "../include/Game.h"
#include "../include/SpriteRenderer.h"

#include <iostream>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// INICIALIZAÇÃO DO ESTÁTICO
int AIController::npcCounter = 0;

AIController::AIController(GameObject& associated) : Component(associated) {
    state = RESTING;                                                                    // Começa descansando 
    restTimer.Restart();
    
    // Incrementa o contador global de inimigos (usando o do Zombie)
    // para que a Wave não acabe enquanto este NPC estiver vivo.
    npcCounter++; 
}

AIController::~AIController() {
    npcCounter--;                                                                       // Decrementa ao morrer para liberar a Wave
}

void AIController::Update(float dt) {
    if (Character::player == nullptr) return;

    // Obtém o corpo (Character) que este cérebro controla
    Character* myCharacter = associated.GetComponent<Character>();
    if (!myCharacter) return; // Se não tem corpo, não faz nada

    if (state == RESTING) {
        restTimer.Update(dt);
        if (restTimer.Get() > 1.5f) {
            state = MOVING;
        }
    }
    else if (state == MOVING) {
        Vec2 playerPos = Character::player->GetCenter();
        Vec2 myPos = associated.box.Center();
        Vec2 direction = (playerPos - myPos);
        float distance = direction.Magnitude();

        // Se chegou perto
        if (distance < 300.0f) { 
            
            // AÇÃO: ATIRAR
            // Em vez de pegar a Gun direto, mandamos o comando pro Character
            // O Character sabe onde está a arma e como atirar.
            Character::Command shootCmd(Character::Command::SHOOT, playerPos.x, playerPos.y);
            myCharacter->Issue(shootCmd);

            restTimer.Restart();
            state = RESTING;
        } 
        else {
            // AÇÃO: MOVER
            // Calculamos a posição futura desejada
            // O Character::Update vai ler isso, calcular a velocidade e mover a box com colisão.
            
            // Nota: O Command::MOVE do Character espera uma "Target Position", não um vetor direção.
            // Vamos mandar ele ir para a posição atual do player.
            Character::Command moveCmd(Character::Command::MOVE, playerPos.x, playerPos.y);
            myCharacter->Issue(moveCmd);
        }
    }
}

void AIController::Render() {
    // Nada aqui
}