#include "../include/PlayerController.h"
#include "../include/InputManager.h"
#include "../include/Game.h"
#include "../include/Camera.h"
#include "../include/GameObject.h"
#include "../include/Character.h"
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

    // 2. Verifica as teclas W, A, S, D para movimento.
    if (input.IsKeyDown(SDLK_w)) {                                      // Exemplo: SDLK_w é o keycode para a tecla 'w'
        direction.y -= 1.0f;
        isMoving = true;
    }
    if (input.IsKeyDown(SDLK_s)) {
        direction.y += 1.0f;
        isMoving = true;
    }
    if (input.IsKeyDown(SDLK_a)) {
        direction.x -= 1.0f;
        isMoving = true;
    }
    if (input.IsKeyDown(SDLK_d)) {
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