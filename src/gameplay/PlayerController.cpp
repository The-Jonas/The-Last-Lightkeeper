#include "gameplay/PlayerController.h"
#include "core/InputManager.h"
#include "core/Game.h"
#include "engine/Camera.h"
#include "engine/GameObject.h"
#include "gameplay/Character.h"
#include <cmath>

PlayerController::PlayerController(GameObject& associated) : Component(associated){}

void PlayerController::Start() {
    // Deixando vazio, pois o componente é criado no Start do State e o Character já foi iniciado
}

void PlayerController::Render() {
    // Vazio
}

void PlayerController::Update(float dt) {
    // 1. Obtém o componente Character do GameObject associado
    Character* character = associated.GetComponent<Character>();
    if (!character) {                                                   // Se não encontrar, simplesmente encerre a função
        return;
    }

    InputManager& input = InputManager::GetInstance();                  // Uma instância de InputManager para usarmos mouse e teclado
    bool isMoving = false;
    Vec2 direction(0.0f, 0.0f);

    // 2. Verifica as ações de movimento (teclas remapeáveis).
    if (input.ActionDown(GameAction::MoveUp)) {
        direction.y -= 1.0f;
        isMoving = true;
    }
    if (input.ActionDown(GameAction::MoveDown)) {
        direction.y += 1.0f;
        isMoving = true;
    }
    if (input.ActionDown(GameAction::MoveLeft)) {
        direction.x -= 1.0f;
        isMoving = true;
    }
    if (input.ActionDown(GameAction::MoveRight)) {
        direction.x += 1.0f;
        isMoving = true;
    }

    // 3. Emite o comando de movimento, se houver
    if (isMoving) {
        // O vetor precisa ser normalizado para ter o módulo constante,
        // garantindo que a velocidade linear (linearSpeed) seja aplicada corretamente
        Vec2 normalizeDirection = direction.Normalized();
        Character::Command moveCommand(Character::Command::MOVE, associated.box.Center().x + normalizeDirection.x, associated.box.Center().y + normalizeDirection.y);
        character->Issue(moveCommand);
    }

}