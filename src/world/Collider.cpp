#include "world/Collider.h"
#include "engine/GameObject.h"
#include "engine/Camera.h"
#include "core/Game.h"

#include <cmath>

// Garante que M_PI esteja definido

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Collider::Collider(GameObject& associated, Vec2 scale, Vec2 offset)
    : Component(associated), scale(scale), offset(offset){
    // box é inicializado pelo Update
}

void Collider::Update(float dt){
    box = associated.box;                       // Seta a box como uma cópia da box de associated

    // Multiplica a altura e a largura pela escala
    box.w *= scale.x;
    box.h *= scale.y;

    Vec2 center = associated.box.Center();      // Calcula o centro da box de associated

    // Adiciona o offset rotacionado pelo ângulo de associated
    Vec2 rotatedOffset = offset.Rotated(associated.angleDeg * (M_PI / 180.0));
    center = center + rotatedOffset;

    // Define a nova posição da box (canto superior esquerdo)
    box.x = center.x - (box.w / 2.0f);
    box.y = center.y - (box.h / 2.0f);
}

void Collider::Render(){
    // A visualização de colisores agora é feita SÓ pela tecla [B]
    // (StageState::RenderGameplayCollisionDebug), com cores por tipo e legenda.
    // Antes este Render desenhava um quadrado VERMELHO em TODO collider sempre que
    // a build era DEBUG (não respeitava o [B]) — eram os "quadrados aleatórios".
}

void Collider::SetScale(Vec2 scale) {
    this->scale = scale;
}

void Collider::SetOffset(Vec2 offset) {
    this->offset = offset;
}

