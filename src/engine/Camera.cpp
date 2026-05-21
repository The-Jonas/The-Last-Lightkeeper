#include "engine/Camera.h"
#include "core/Game.h"
#include "engine/GameObject.h"
#include "core/InputManager.h"
#include <algorithm>
#include <cmath>

//Definição e inicialização dos membros estáticos
Vec2 Camera::pos(0,0);
Vec2 Camera::speed(0,0);
GameObject* Camera::focus = nullptr;
GameObject* Camera::pairA = nullptr;
GameObject* Camera::pairB = nullptr;
GameObject* Camera::pairPrimary = nullptr;
float Camera::zoom = 1.0f;

void Camera::Follow(GameObject* newFocus) {             // Seta um novo foco
    focus = newFocus;
    ClearPairFollow();
}

void Camera::Unfollow() {                               // Atribui nullptr ao foco
    focus = nullptr;
}

void Camera::FollowPair(GameObject* first, GameObject* second, GameObject* primary) { // Enquadra os dois personagens
    pairA = first;
    pairB = second;
    pairPrimary = primary;
    focus = nullptr;
}

void Camera::ClearPairFollow() {
    pairA = nullptr;
    pairB = nullptr;
    pairPrimary = nullptr;
}

float Camera::GetZoom() {
    return zoom;
}

void Camera::Update(float dt) { 
    if (pairA && pairB) { // Quando a dupla está ativa, enquadra os dois personagens
        const float lookAheadMaxX = 120.0f;
        const float lookAheadMaxY = 80.0f;
        const float followSmoothing = 8.0f;
        const float zoomSmoothing = 6.0f;
        const float minZoom = 0.65f;
        const float maxZoom = 1.0f;
        const float framePaddingX = 220.0f;
        const float framePaddingY = 180.0f;

        Game& game = Game::GetInstance();
        InputManager& input = InputManager::GetInstance();
        Vec2 aCenter = pairA->box.Center();
        Vec2 bCenter = pairB->box.Center();
        Vec2 midpoint((aCenter.x + bCenter.x) * 0.5f, (aCenter.y + bCenter.y) * 0.5f);
        Vec2 center = midpoint;
        if (pairPrimary) {
            const float primaryWeight = 0.72f;
            Vec2 primaryCenter = pairPrimary->box.Center();
            center.x = (midpoint.x * (1.0f - primaryWeight)) + (primaryCenter.x * primaryWeight);
            center.y = (midpoint.y * (1.0f - primaryWeight)) + (primaryCenter.y * primaryWeight);
        }

        float winW = static_cast<float>(game.GetWindowsWidth());
        float winH = static_cast<float>(game.GetWindowsHeight());
        float distX = std::abs(aCenter.x - bCenter.x) + framePaddingX;
        float distY = std::abs(aCenter.y - bCenter.y) + framePaddingY;
        float zoomFromX = (distX > 0.0f) ? (winW / distX) : maxZoom;
        float zoomFromY = (distY > 0.0f) ? (winH / distY) : maxZoom;
        float desiredZoom = std::clamp(std::min(zoomFromX, zoomFromY), minZoom, maxZoom);

        float halfWidth = (winW / desiredZoom) / 2.0f;
        float halfHeight = (winH / desiredZoom) / 2.0f;

        float normalizedMouseX = 0.0f;
        float normalizedMouseY = 0.0f;
        if (winW > 0.0f) {
            normalizedMouseX = (input.GetMouseX() - (winW / 2.0f)) / (winW / 2.0f);
        }
        if (winH > 0.0f) {
            normalizedMouseY = (input.GetMouseY() - (winH / 2.0f)) / (winH / 2.0f);
        }

        normalizedMouseX = std::clamp(normalizedMouseX, -1.0f, 1.0f);
        normalizedMouseY = std::clamp(normalizedMouseY, -1.0f, 1.0f);

        Vec2 lookAhead(normalizedMouseX * lookAheadMaxX, normalizedMouseY * lookAheadMaxY);
        Vec2 targetPos(center.x - halfWidth + lookAhead.x, center.y - halfHeight + lookAhead.y);

        float posInterpolation = 1.0f - std::exp(-followSmoothing * dt);
        float zoomInterpolation = 1.0f - std::exp(-zoomSmoothing * dt);
        pos.x += (targetPos.x - pos.x) * posInterpolation;
        pos.y += (targetPos.y - pos.y) * posInterpolation;
        zoom += (desiredZoom - zoom) * zoomInterpolation;
    } else if (focus) {                                        // Quando so tem um personagem, centraliza no objeto
        const float lookAheadMaxX = 180.0f;
        const float lookAheadMaxY = 120.0f;
        const float followSmoothing = 10.0f;

        Game& game = Game::GetInstance();
        InputManager& input = InputManager::GetInstance();
        Vec2 focusCenter = focus ->box.Center();
        float halfWidth = (game.GetWindowsWidth() / zoom) / 2.0f;
        float halfHeight = (game.GetWindowsHeight() / zoom) / 2.0f;

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
        zoom += (1.0f - zoom) * interpolation;
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
        zoom += (1.0f - zoom) * (1.0f - std::exp(-8.0f * dt));
    }
}

