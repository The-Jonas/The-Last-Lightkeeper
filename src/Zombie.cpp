#include "../include/Zombie.h"
#include "../include/GameObject.h"
#include "../include/SpriteRenderer.h"
#include "../include/Animation.h"
#include "../include/Animator.h"
#include "../include/InputManager.h"
#include "../include/Vec2.h"
#include "../include/Timer.h"
#include "../include/Camera.h"
#include "../include/Collider.h"
#include "../include/Bullet.h"
#include "../include/Character.h"
#include <iostream>

int Zombie::zombieCount = 0;

Zombie::Zombie(GameObject& associated) : Component(associated), deathSound("Recursos/audio/Dead.wav"), damageSound("Recursos/audio/Hit0.wav") {
    hitpoints = 100;                                                            // Setar vida inicial para 100 de HP
    zombieIsDead = false;                                                       // Flag para saber se o zumbi morreu, iniciada como false obviamente
    hit = false;                                                                // Inicia como false quando o zombie nasce

    SpriteRenderer* sprite = new SpriteRenderer(associated, "Recursos/img/Enemy.png", 3, 2);  // O zombie tem 3x2 frames
    associated.AddComponent(sprite);                                            // Cria e adiciona um SpriteRenderer ao GameObject

    Animator* animator = new Animator(associated);                              // Cria e adiciona o componente Animator para gerenciar as animações.
    associated.AddComponent(animator);

    // Adiciona as animações "walking", "dead" e "hit" ao Animator.
    animator->AddAnimation("walking", Animation(0, 3, 0.3f));                   // "walking" vai do frame 0 ao 3, com tempo de menos de 1 segundo.
    animator->AddAnimation("walking-flip", Animation(0, 3, 0.3f, SDL_FLIP_HORIZONTAL));
    animator->AddAnimation("dead", Animation(5,5,0));                           // "dead" se mantém no frame 5 (tempo 0).
    animator->AddAnimation("hit", Animation(4,4,0));
    animator->AddAnimation("hit-flip", Animation(4,4,0, SDL_FLIP_HORIZONTAL));

    animator->SetAnimation("walking");                                          // Seta pra começar com o inimigo (zumbi) fazendo animação de andar
    zombieCount++;
}

Zombie::~Zombie() {
    zombieCount--;                                                              // Decrementa ao morrer
}

void Zombie::Start() {
    associated.AddComponent(new Collider(associated, Vec2(0.8f, 0.9f)));        // Cria o colisor aqui para evitar delay de posição
}

void Zombie::Damage(int damage) {

    hitpoints -= damage;                                                        // Função para tomar dano (diminuir quantidade de HP)
    Animator* animator = associated.GetComponent<Animator>();
    
    if ((hitpoints <= 0) && !zombieIsDead){                                     // Se vida igual a 0 (Se precisar no futuro eu faço uma flag isDead pra permitir <= 0)
        zombieIsDead = true;                                                    // Troca flag para true, para sabermos que o zumbi de fato morreu               
        if (animator) {                                                   
            animator->SetAnimation("dead");                                     // Faz animação de morte, que basicamente é só o frame 5
        }
        deathSound.Play(1);                                                     // Toca o som de morte 1 vez 

        // Remover colisor ao morrer (Polimento)
        Collider* collider = associated.GetComponent<Collider>();
        if (collider) {
            associated.RemoveComponent(collider);
        }

        deathTimer.Restart();                                                   // Inicia o timer de morte
        
                     
    } else if (hitpoints > 0) {
        // Se o zumbi ainda estiver vivo, aplica o hit
        damageSound.Play(1);                                                    // Toca o som de dano 1 vez toda vez que o zumbi não tiver morrido
        if (animator) {
            // Se o jogador existir, usamos a posição dele. Se não, assumimos um padrão.
            bool hitFromLeft = false;
            
            if (Character::player != nullptr) {
                hitFromLeft = (Character::player->GetCenter().x < associated.box.Center().x);
            }

            if (hitFromLeft) {
                animator->SetAnimation("hit-flip");
            } else {
                animator->SetAnimation("hit");
            }
            
            hit = true;
            hitTimer.Restart();
        }
    }    
}

void Zombie::Update(float dt) {

    hitTimer.Update(dt);                                                        // Atualiza o timer de hit
    deathTimer.Update(dt);

    // Lógica de movimento básica (IA)
    if (!zombieIsDead && Character::player != nullptr) {
        // Calcula o vetor até a diração do player
        Vec2 playerPos = Character::player->GetCenter();
        Vec2 myPos = associated.box.Center();

        Vec2 direction = (playerPos - myPos).Normalized();                      // (Destino - Origem).Normalized()
        float speed = 85.0f;                                                   // Velocidade constante (90 pixels/s porque zumbi é lento)

        // Move o zumbi
        associated.box.x += direction.x * speed * dt;
        associated.box.y += direction.y * speed * dt;

        // Animação para a esquerda (Polimento)
        if (!hit) {
            Animator* animator = associated.GetComponent<Animator>();
            if (animator) {
                // Se a direção X for negativa, espelha
                if (direction.x < 0) {
                    animator->SetAnimation("walking-flip");
                } else {
                    animator->SetAnimation("walking");
                }
            }
        }
        
        // Rotação para olhar pro player
        //associated.angleDeg = direction.Angle() * 180/M_PI;
    }

    //Lógica para o hit
    if (hit && !zombieIsDead && hitTimer.Get() > 0.2f) {                        // se hit for true, Zombie não estiver morto e já tiver passado tempo                                                                                                                                                      
        hit = false;
    }

    //Lógica para remoção após a morte
    if (zombieIsDead && deathTimer.Get() > 5.0f) {                              // Se zumbi tá morto por 5 segundos, remove o corpo dele
        associated.RequestDelete();
    }
    
    /*Lógica do input
    if (InputManager::GetInstance().MousePress(LEFT_MOUSE_BUTTON)){             // Verifica se o botão esquerdo do mouse foi pressionado
        //Obtém as coordenadas do mouse
        int mouseX = InputManager::GetInstance().GetMouseX() + Camera::pos.x;
        int mouseY = InputManager::GetInstance().GetMouseY() + Camera::pos.y;

        Vec2 mousePoint(mouseX, mouseY);                                        // Cria um objeto Vec2 com as coordenadas do Mouse

        if(associated.box.Contains(mousePoint)) {                               // Verifica se a box do GameObject contém o ponto do mouse
            Damage(10);                                                         // Se o clique estiver dentro da box, aplica o dano
        }
    }
    */
}

void Zombie::NotifyCollision(GameObject& other) {
    // Verifica se "other" é uma bala
    Bullet* bullet = other.GetComponent<Bullet>();

    if (bullet) {
        if (!bullet->targetsPlayer) {                                           // Verifica se a bala foi atirada pelo jogador
            Damage(bullet->GetDamage());                                        // Aplica o dano da bala
        }
    }
}


void Zombie::Render() {
    // Deixar vazio por enquanto
}