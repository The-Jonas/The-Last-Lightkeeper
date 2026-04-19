#include "../include/Collider.h"
#include "../include/GameObject.h"
#include "../include/Camera.h"
#include "../include/Game.h"

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
#ifdef DEBUG
    // Pega o centro atual da box calculada
    Vec2 center(box.Center());
    SDL_Point points[5]; // Array de pontos para desenhar as 4 linhas (o 5º ponto fecha o quadrado voltando ao 1º)

    // Converte ângulo para radianos
    float angleRad = associated.angleDeg * (M_PI / 180.0); 

    // A lógica abaixo calcula a posição de cada um dos 4 vértices do retângulo rotacionado:
    // (Ponto Original - Centro) -> Rotaciona -> Soma Centro -> Subtrai Câmera

    // Ponto 1: Canto Superior Esquerdo
    Vec2 point = (Vec2(box.x, box.y) - center).Rotated(angleRad) + center - Camera::pos;
    points[0] = {(int)point.x, (int)point.y};
    points[4] = {(int)point.x, (int)point.y}; // O último ponto é igual ao primeiro para fechar o loop

    // Ponto 2: Canto Superior Direito
    point = (Vec2(box.x + box.w, box.y) - center).Rotated(angleRad) + center - Camera::pos;
    points[1] = {(int)point.x, (int)point.y};
    
    // Ponto 3: Canto Inferior Direito
    point = (Vec2(box.x + box.w, box.y + box.h) - center).Rotated(angleRad) + center - Camera::pos;
    points[2] = {(int)point.x, (int)point.y};
    
    // Ponto 4: Canto Inferior Esquerdo
    point = (Vec2(box.x, box.y + box.h) - center).Rotated(angleRad) + center - Camera::pos;
    points[3] = {(int)point.x, (int)point.y};

    // Configura a cor da linha para Vermelho
    SDL_SetRenderDrawColor(Game::GetInstance().GetRenderer(), 255, 0, 0, SDL_ALPHA_OPAQUE);
    
    // Desenha as linhas conectando os pontos
    SDL_RenderDrawLines(Game::GetInstance().GetRenderer(), points, 5);
#endif // DEBUG
}

void Collider::SetScale(Vec2 scale) {
    this->scale = scale;
}

void Collider::SetOffset(Vec2 offset) {
    this->offset = offset;
}

