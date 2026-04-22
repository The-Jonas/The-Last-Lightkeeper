#include "../include/Camera.h"
#include "../include/Game.h"
#include "../include/GameObject.h"
#include "../include/InputManager.h"
#include <algorithm>
#include <cmath>

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
        const float lookAheadMaxX = 180.0f;
        const float lookAheadMaxY = 120.0f;
        const float followSmoothing = 10.0f;

        Game& game = Game::GetInstance();
        InputManager& input = InputManager::GetInstance();
        Vec2 focusCenter = focus ->box.Center();
        float halfWidth = game.GetWindowsWidth() / 2.0f;
        float halfHeight = game.GetWindowsHeight() / 2.0f;

        float normalizedMouseX = 0.0f;
        float normalizedMouseY = 0.0f;
        if (halfWidth > 0.0f) {
            normalizedMouseX = (input.GetMouseX() - halfWidth) / halfWidth;
        }
        if (halfHeight > 0.0f) {
            normalizedMouseY = (input.GetMouseY() - halfHeight) / halfHeight;
        }

        normalizedMouseX = std::clamp(normalizedMouseX, -1.0f, 1.0f);
        normalizedMouseY = std::clamp(normalizedMouseY, -1.0f, 1.0f);

        Vec2 lookAhead(normalizedMouseX * lookAheadMaxX, normalizedMouseY * lookAheadMaxY);
        Vec2 targetPos(focusCenter.x - halfWidth + lookAhead.x, focusCenter.y - halfHeight + lookAhead.y);
        float interpolation = 1.0f - std::exp(-followSmoothing * dt);
        pos.x += (targetPos.x - pos.x) * interpolation;
        pos.y += (targetPos.y - pos.y) * interpolation;
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

