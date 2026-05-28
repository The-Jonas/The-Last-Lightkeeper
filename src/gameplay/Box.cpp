#include "gameplay/Box.h"
#include "engine/GameObject.h"
#include "engine/SpriteRenderer.h"
#include "world/Collider.h"
#include "world/Collision.h"
#include "gameplay/Character.h"
#include "core/Game.h"
#include "states/stage/StageState.h"
#include <algorithm>
#include <vector>

// LISTA GLOBAL PARA AS CAIXAS SE ENXERGAREM NA FÍSICA PREDITIVA
static std::vector<Box*> globalBoxList;

Box::Box(GameObject& associated, bool isStatic) : Component(associated), isStatic(isStatic) {
    // Carrega a arte da caixa
    SpriteRenderer* sprite = new SpriteRenderer(associated, "Recursos/img/objetos/Caixa.png", 1, 1);

    associated.AddComponent(sprite);

    // Entra na lista ao nascer
    globalBoxList.push_back(this);
}

Box::~Box() {
    // Sai da lista ao ser destruida
    globalBoxList.erase(std::remove(globalBoxList.begin(), globalBoxList.end(), this), globalBoxList.end());
}

void Box::Start() {
    // Escala da base de madeira (ex: 85% da largura da imagem, 15% da altura)
    Vec2 scale(0.85f, 0.40f);

    // Cálculo automático para colar a hitbox no chão da caixa
    float offsetY = (associated.box.h / 2.0f) * (1.0f - scale.y);

    associated.AddComponent(new Collider(associated, scale, Vec2(0, offsetY)));
}

void Box::Update(float dt) {
    // A caixa não tem pensamento próprio, então aqui fica vazio
}

void Box::Render() {}

void Box::NotifyCollision(GameObject& other) {

    Collider* myCol = associated.GetComponent<Collider>();
    Collider* otherCol = other.GetComponent<Collider>();
    if (!myCol || !otherCol) return;

    // Checagem entre caixas
    Box* otherBox = other.GetComponent<Box>();
    if (otherBox) {
        // Se ambas forem estáticas, não faz nada
        if (isStatic && otherBox->isStatic) return;

        // para evitar que as duas resolvam a colisão ao mesmo tempo 
        // (causando o dobro de empurrão), deixamos apenas a dinâmica fazer o cálculo.
        if (isStatic) return;
    }

    // Matemática de Separação (AABB)
    // Calcula o quanto um objeto entrou no outro em cada lado
    float overLeft = (otherCol->box.x + otherCol->box.w) - myCol->box.x;
    float overRight = (myCol->box.x + myCol->box.w) - otherCol->box.x;
    float overTop = (otherCol->box.y + otherCol->box.h) - myCol->box.y;
    float overBottom = (myCol->box.y + myCol->box.h) - otherCol->box.y;

    // Se deu negativo em algum lado, não é uma colisão real
    if (overLeft <= 0 || overRight <= 0 || overTop <= 0 || overBottom <= 0) return;

    // Encontra a menor sobreposição para saber exatamente de qual lado foi a batida
    float minOverlap = std::min({overLeft, overRight, overTop, overBottom});

    Vec2 pushVector(0, 0);
    if (minOverlap == overLeft) pushVector.x = overLeft;
    else if (minOverlap == overRight) pushVector.x = -overRight;
    else if (minOverlap == overTop) pushVector.y = overTop;
    else if (minOverlap == overBottom) pushVector.y = -overBottom;

    // =====================
    // RESOLUÇÃO DA FÍSICA =
    // =====================

    if (isStatic) {
        // SOU UMA CAIXA ESTÁTICA: Sou igual a uma parede. Bloqueio quem bater em mim.
        other.box.x -= pushVector.x;
        other.box.y -= pushVector.y;

    } else {
        // SOU UMA CAIXA DINÂMICA:

        if (otherBox) {
            // Bati em outra caixa (estática ou dinâmica). Fujo para não entrar nela!
            associated.box.x += pushVector.x;
            associated.box.y += pushVector.y;

            // Se a outra também for dinâmica, ela cede metade do espaço (física de bilhar)
            if (!otherBox -> isStatic) {
                associated.box.x -= pushVector.x * 0.5f;
                associated.box.y -= pushVector.y * 0.5f;
                other.box.x -= pushVector.x * 0.5f;
                other.box.y -= pushVector.y * 0.5f;
            }

        }else {
            // Bati no Jogador! Ele quer me empurrar.
            float oldX = associated.box.x;
            float oldY = associated.box.y;

            associated.box.x += pushVector.x;
            associated.box.y += pushVector.y;

            // FÍSICA PREDITIVA (Enxergar antes de teleportar)
            // Atualizo minha hitbox no novo local e pergunto pro LevelManager se tá livre
            myCol->Update(0);
            SDL_Rect hitbox = { (int)myCol->box.x, (int)myCol->box.y, (int)myCol->box.w, (int)myCol->box.h };
            StageState* stage = Game::TryGetStageState();
            if (!stage) {
                associated.box.x = oldX;
                associated.box.y = oldY;
                other.box.x -= pushVector.x;
                other.box.y -= pushVector.y;
                return;
            }

            bool hitWall = stage->level.CheckCollision(hitbox);
            bool hitOtherBox = false;
            
            // Verifica se no novo local eu bato em outra caixa
            for (Box* b : globalBoxList) {
                if (b == this) continue; // Não checa contra si mesmo
            
                Collider* bCol = b->associated.GetComponent<Collider>();
                if (bCol) {
                    // Checagem precisa usando a função de colisão da própria engine
                    if (Collision::IsColliding(myCol->box, bCol->box, associated.angleDeg, b->associated.angleDeg)) {
                        hitOtherBox = true;
                        break;
                    }    
                }
            }

            // Se eu for bater na parede do mapa OU em outra caixa
            if (hitWall || hitOtherBox) {
            // Cancelo o movimento da caixa
            associated.box.x = oldX;
            associated.box.y = oldY;
            
            // Cancelo o movimento do jogador (Barreira absoluta!)
            other.box.x -= pushVector.x;
            other.box.y -= pushVector.y;
            }
        }
    }
}