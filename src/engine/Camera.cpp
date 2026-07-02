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
float Camera::trauma = 0.0f;
float Camera::vertigo = 0.0f;
float Camera::fxTime = 0.0f;
Vec2 Camera::shakeOffset(0, 0);

namespace {
constexpr float kTraumaDecayPerSec = 1.6f;   // trauma volta a 0 em ~0.6s
constexpr float kMaxShakePx = 22.0f;          // amplitude máxima do tremor (trauma=1)
constexpr float kMaxSwayPx = 10.0f;           // amplitude máxima do balanço de vertigem
}

void Camera::AddTrauma(float amount) {
    trauma += amount;
    if (trauma > 1.0f) trauma = 1.0f;
    if (trauma < 0.0f) trauma = 0.0f;
}

void Camera::SetVertigo(float amount) {
    if (amount < 0.0f) amount = 0.0f;
    if (amount > 1.0f) amount = 1.0f;
    vertigo = amount;
}

void Camera::ResetShake() {
    trauma = 0.0f;
    vertigo = 0.0f;
    pos = pos - shakeOffset;  // remove o offset aplicado para não deixar resíduo
    shakeOffset = Vec2(0, 0);
}

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
    // Remove o offset de tremor do frame anterior para que o enquadramento
    // interpole sobre a posição-base limpa (sem acumular o tremor).
    pos = pos - shakeOffset;

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

        // Pan livre por setas — ferramenta de desenvolvedor apenas.
        if (Game::debugMode) {
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
        }
        pos += speed;                                   // Somamos a velocidade da câmera a posição
        zoom += (1.0f - zoom) * (1.0f - std::exp(-8.0f * dt));
    }

    // ── Tremor + vertigem: somados em `pos` por cima da posição-base ──
    fxTime += dt;
    trauma -= kTraumaDecayPerSec * dt;
    if (trauma < 0.0f) trauma = 0.0f;

    // Tremor de impacto: alta frequência, escala com trauma^2 (sensação seca).
    const float shakeMag = kMaxShakePx * trauma * trauma;
    const float shakeX = std::sin(fxTime * 47.0f) + 0.5f * std::sin(fxTime * 23.3f);
    const float shakeY = std::sin(fxTime * 41.0f + 1.7f) + 0.5f * std::sin(fxTime * 29.7f);

    // Vertigem: balanço lento e contínuo enquanto a sanidade está baixa.
    const float swayMag = kMaxSwayPx * vertigo;
    const float swayX = std::sin(fxTime * 1.6f);
    const float swayY = std::sin(fxTime * 1.13f + 0.9f);

    shakeOffset = Vec2(shakeX * shakeMag + swayX * swayMag,
                       shakeY * shakeMag + swayY * swayMag);
    pos += shakeOffset;
}

