#include "../include/Gun.h"
#include "../include/GameObject.h"
#include "../include/SpriteRenderer.h"
#include "../include/Animation.h"
#include "../include/Animator.h"
#include "../include/InputManager.h"
#include "../include/State.h"
#include "../include/Game.h"
#include "../include/Camera.h"
#include "../include/Bullet.h"
#include "../include/Character.h"
#include <cmath>

//Construtor da Gun
Gun::Gun(GameObject& associated, std::weak_ptr<GameObject> character) : Component(associated),
shotSound("Recursos/audio/Range.wav"), reloadSound("Recursos/audio/PumpAction.mp3") {

    //Inicialização das variáveis
    this->character = character;
    angle = 0.0f;
    cooldownState = 0;

    //Adiciona componentes visuais e de animação
    SpriteRenderer* sprite = new SpriteRenderer(associated, "Recursos/img/Gun.png", 3, 2);
    associated.AddComponent(sprite);
    Animator* animator = new Animator(associated);
    associated.AddComponent(animator);

    //Adiciona animações
    animator->AddAnimation("idle", Animation(0, 0, 0.0f));
    animator->AddAnimation("reloading", Animation(1, 5, 0.3f));
    animator->SetAnimation("idle");
     
}

void Gun::Update(float dt) {
    //Verifica se o Character ainda existe. Se não, a Gun se destrói
    std::shared_ptr<GameObject> charSharedPtr = character.lock();
    if (!charSharedPtr) {
        associated.RequestDelete();
        return;
    }

    //Obtém o centro desejado (O centro de Character)
    Vec2 characterCenter = charSharedPtr->box.Center();

    Character* ownerChar = charSharedPtr->GetComponent<Character>();

    // Verifica se o dono dessa arma é o jogador principal
    if (ownerChar == Character::player) {
        // -- COMPORTAMENTO DO JOGADOR --

        //Obtém a posição do mouse (em coordenadas do mundo) para mirar
        float mouseX = InputManager::GetInstance().GetMouseX() + Camera::pos.x;
        float mouseY = InputManager::GetInstance().GetMouseY() + Camera::pos.y;
        Vec2 mousePos(mouseX, mouseY);

        // Calcula o ângulo entre a Gun e o mouse (em radianos)
        angle = (mousePos - characterCenter).Angle();
        // Aplica a rotação (em graus) ao GameObject da Gun
        associated.angleDeg = angle * 180.0 / M_PI;
    } 
    else {
        // -- COMPORTAMENTO DO NPC --

        // Se o jogador existir, mira nele!
        if (Character::player != nullptr) { 
            Vec2 playerPos = Character::player->GetCenter();
            angle  = (playerPos - characterCenter).Angle();
            associated.angleDeg = angle * 180.0 / M_PI;

        }
        else {
            // Se o jogador morreu, a arma volta a seguir o corpo do NPC
            associated.angleDeg = charSharedPtr->angleDeg;
            angle = associated.angleDeg * (M_PI / 180.0);
        }
        
    }

    //Calcula as novas coordenadas x e y da Gun para que seu centro coincida com o centro de Character
    associated.box.x = characterCenter.x - associated.box.w / 2;
    associated.box.y = characterCenter.y - associated.box.h / 2;

    // Aplica o espelhamento vertical (flip)
    SpriteRenderer* sprite = associated.GetComponent<SpriteRenderer>();
    if (sprite) {
        // Se o ângulo (em radianos) está "para trás" (entre 90 e 270 graus)
        if (angle > (M_PI / 2.0) || angle < -(M_PI / 2.0)) {
            
            sprite->SetFlip(SDL_FLIP_VERTICAL);                             // Usa SetFlip(flip) para definir o flip vertical
        } else {
            sprite->SetFlip(SDL_FLIP_NONE);
        }
    }


    //Reposiciona a arma frente ao corpo (opcional)
    float distanceFromBody = 40.0f; //Ajustar isso mais tarde caso eu precise
    Vec2 offset = Vec2::FromAngle(angle) * distanceFromBody;
    Vec2 finalGunCenter = characterCenter + offset;                             // Recalcula x e y baseados no centro do personagem + offset
    associated.box.x = finalGunCenter.x - associated.box.w /2;
    associated.box.y = finalGunCenter.y - associated.box.h /2;

    //Gerencia a máquina de estados do cooldown
    cdTimer.Update(dt);
    Animator* animator = associated.GetComponent<Animator>();

    if (cooldownState == 1 && cdTimer.Get() > 0.2f) {                           // Tempo entre atirar e recarregar
        cooldownState = 2;
        cdTimer.Restart();
        reloadSound.Play(1);
        if (animator) animator->SetAnimation("reloading");

    } else if (cooldownState == 2 && cdTimer.Get() > 0.6f)  {                   // Tempo de recarga
        cooldownState = 3;
        cdTimer.Restart();
        if (animator) animator->SetAnimation("idle");

    } else if (cooldownState == 3 && cdTimer.Get() > 0.2f) {                    // Tempo após recarregar
        cooldownState = 0;                                                      // Pronta pra atirar novamente
    }
}

void Gun::Render() {
}

void Gun::Shoot(Vec2 target) {
    // Calcula o ângulo para o alvo
    if (cooldownState == 0) {                  

        // Inicia o cooldown
        shotSound.Play(1);
        cooldownState = 1;
        cdTimer.Restart();

        // Define número de projéteis e dispersão
        int numProjectiles = 3;
        float spread = 15.0f;

        float baseAngle = (target - associated.box.Center()).Angle();           // Calcula o ângulo principal

        for (int i = 0; i < numProjectiles; i++) {
            // Calculamos o offset do ângulo: -15, 0, +15 (3 balas)
            float angleOffset = (i - (numProjectiles / 2)) * (spread * M_PI / 180.0f);
            float finalAngle = baseAngle + angleOffset;

            // Criação da bala
            GameObject* bulletGo = new GameObject();                                // Cria um novo GameObject para a Bullet
            bulletGo->z = 2;                                                        // Z = 2 (Camada de Gameplay)
            bulletGo->sub_z = 1;                                                    // Garante que a bala vai aparecer por cima do NPC e Player
            // Calcule a posição inicial central da bala (startPos)
            float bulletOffset = associated.box.w / 2;
            Vec2 startPos = associated.box.Center() + Vec2::FromAngle(angle) * bulletOffset; 

            // Lógica de FRIENDLY FIRE
            std::shared_ptr<GameObject> charSharedPtr = character.lock();
            Character* ownerCharacter = nullptr;

            int bulletDamage = 0;

            if (charSharedPtr) {
            ownerCharacter = charSharedPtr->GetComponent<Character>();

                if (ownerCharacter) {
                    bulletDamage = ownerCharacter->GetDamage();
                }
                else {
                    bulletDamage = 10;              // Valor de segurança caso não ache o dono
                }
            }
            // Se a arma NÃO pertence ao Player (ex: é de outro NPC), a bala mira no Player
            bool targetsPlayer = (ownerCharacter != Character::player);

            // Constantes arbitrátrias para bullet
            float bulletSpeed = 500.0f;                                             // 500 pixels/segundo
            float maxDistance = 360.0f;                                             // 360 pixels

            // 1. Cria o componente Bullet
            Bullet* bulletComp = new Bullet(*bulletGo, finalAngle, bulletSpeed, bulletDamage, maxDistance, targetsPlayer);
            bulletGo->AddComponent(bulletComp);

            // 2. Define a posição da Bullet
            bulletGo->box.x = startPos.x - (bulletGo->box.w / 2.0f);
            bulletGo->box.y = startPos.y - (bulletGo->box.h / 2.0f);

            Game::GetInstance().GetCurrentState().AddObject(bulletGo);                     // 3. Adiciona o GameObject da Bullet ao State
        }
    }
}