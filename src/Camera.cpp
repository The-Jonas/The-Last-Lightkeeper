#include "../include/Camera.h"
#include "../include/Game.h"
#include "../include/GameObject.h"
#include "../include/InputManager.h"

//Definição e inicialização dos membros estáticos
Vec2 Camera::pos(0,0);
Vec2 Camera::speed(0,0);
GameObject* Camera::focus = nullptr;

void Camera::Follow(GameObject* newFocus) {             // Seta um novo foco
    focus = newFocus;
}

void Camera::Unfollow() {                               // Atribui nullptr ao foco
    focus = nullptr;
}

void Camera::Update(float dt) { 
    if (focus) {                                        // Se a câmera tem um foco, centralize-a no objeto
        Vec2 focusCenter = focus ->box.Center();
        pos.x = focusCenter.x - (Game::GetInstance().GetWindowsWidth()) / 2;                 // O movimento independe do dt
        pos.y = focusCenter.y - (Game::GetInstance().GetWindowsHeight()) / 2;                // Utilizamos os get para pegar altura e largura da janela do jogo
        
    } else  {
        // Se não tem foco, a câmera responde ao input
        speed = Vec2(0, 0);
        float cameraSpeed = 400.0f;                     // Velocidade da camera em pixels por segundo

        if (InputManager::GetInstance().IsKeyDown(LEFT_ARROW_KEY)) {
            speed.x -= cameraSpeed * dt;
        }

        if (InputManager::GetInstance().IsKeyDown(RIGHT_ARROW_KEY)) {
            speed.x += cameraSpeed * dt;
        }

        if (InputManager::GetInstance().IsKeyDown(UP_ARROW_KEY)) {
            speed.y -= cameraSpeed * dt;
        }

        if (InputManager::GetInstance().IsKeyDown(DOWN_ARROW_KEY)) {
            speed.y += cameraSpeed * dt;
        }
        pos += speed;                                   // Somamos a velocidade da câmera a posição
    }
}

