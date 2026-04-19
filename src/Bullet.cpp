#include "../include/Bullet.h"
#include "../include/GameObject.h"
#include "../include/SpriteRenderer.h"
#include "../include/Rect.h"
#include "../include/Collider.h"
#include "../include/Character.h"
#include "../include/Zombie.h"
#include <cmath>                                        

Bullet::Bullet (GameObject& associated, float angle, float speed, int damage, float maxDistance, bool targetsPlayer) : Component(associated), targetsPlayer(targetsPlayer), damage(damage) {
    distanceLeft = maxDistance;

    //Inicializa Component, depois cria e adiciona a SpriteRenderer
    SpriteRenderer* sprite = new SpriteRenderer(associated, "Recursos/img/Bullet.png", 1, 1);
    associated.AddComponent(sprite);

    //Define a rotação (em graus), o angle recebido está em radianos
    associated.angleDeg = angle * 180.0 / M_PI;

    //Define a escala (muda o tamanho das bullets), por exemplo 0.5f para metade e 2.0f para o dobro
    sprite->SetScale(1.0f, 1.0f);

    //Calcula o vetor velocidade (Vec2) a partir do ângulo e do módulo de velocidade
    this->speed = Vec2::FromAngle(angle) * speed;
    //A posição da Bullet é implementada em "Gun::Shoot"
}

void Bullet::Start() {
    associated.AddComponent(new Collider(associated));                  // Cria o colisor aqui para evitar delay de posição
}

void Bullet::Update(float dt) {
    // Para cada Update, a Bullet deve se mover (speed * dt)
    Vec2 movement = speed * dt;
    associated.box.x += movement.x;
    associated.box.y += movement.y;

    distanceLeft -= movement.Magnitude();                                   // Subtrair essa mesma distância da distância remanescente

    if (distanceLeft <= 0.0f) {                                             // Caso essa distância seja menor ou igual a zero, solicitar deleção
        associated.RequestDelete();
    }
}

void Bullet::NotifyCollision(GameObject& other) {
    // "O comportamento depende de com quem ele está colidindo"
    // Se uma Bullet colide com Characters ou Zombies, ela some. Se colide com outra Bullet, não. 

    // Ignora colisão com outras balas
    if (other.GetComponent<Bullet>()) {
        return;
    }

    // Verifica se atingiu um Zumbi
    if (other.GetComponent<Zombie>()) {
        // Se a bala foi do player (não-NPC), ela se destrói
        if (!targetsPlayer) {
            associated.RequestDelete();
        }
        return;
    }

    // Verifica se atingiu um Character
    Character* character = other.GetComponent<Character>();
    if (character) {
        // Se a bala era de um NPC (mirando no player) E atingiu o player
        if (targetsPlayer && character == Character::player) {
            associated.RequestDelete(); 
        }
        // Se a bala era do Player E atingiu um NPC (não-player)
        else if (!targetsPlayer && character != Character::player) {
            associated.RequestDelete(); 
        }
        // (Se a bala do Player atingir o Player, ela não se destrói - friendly fire evitado)
    }
}

void Bullet::Render() {
    // Não faz nada
}

int Bullet::GetDamage() {                                                   // Retorna o dano que essa Bullet vai causar na colisão.
    return damage;
}

