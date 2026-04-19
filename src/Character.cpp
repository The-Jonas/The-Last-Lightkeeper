#include "../include/Character.h"
#include "../include/GameObject.h"
#include "../include/SpriteRenderer.h"
#include "../include/Animation.h"
#include "../include/Animator.h"
#include "../include/Gun.h"
#include "../include/State.h"
#include "../include/Game.h"
#include "../include/Collider.h"
#include "../include/Camera.h"
#include "../include/Bullet.h"
#include "../include/Zombie.h"

// Implementação do construtor da classe aninhada Command.
Character::Command::Command(CommandType type, float x, float y): type(type), pos(x, y){}

// Implementação do Character
Character* Character::player = nullptr;

Character::Character(GameObject& associated, std::string spritePath, CharacterRole role)
: Component(associated), hitSound("Recursos/audio/Hit1.wav"), deathSound("Recursos/audio/Dead.wav"), role(role) {
    linearSpeed = 200.0f;
    facingLeft = false;                                                                 // Começa olhando pra direita
    
    // -- BALANCEAMENTO DE VIDA E DANO --
    if (role == PLAYER) {
        hp = 200;
        damage = 20;
    }
    else {  // Pro caso de ser um NPC
        hp = 60;
        damage = 25;
    }

    SpriteRenderer* sprite = new SpriteRenderer(associated, spritePath, 3, 4);          // Usa o path fornecido
    associated.AddComponent(sprite);

    Animator* animator = new Animator(associated);
    associated.AddComponent(animator);

    //Adicionando as animações para o character
    animator->AddAnimation("idle", Animation(6, 9, 0.4f));
    animator->AddAnimation("idle-flip", Animation(6, 9, 0.4f, SDL_FLIP_HORIZONTAL));
    animator->AddAnimation("walking", Animation(0, 5, 0.3f));
    animator->AddAnimation("dead", Animation(10, 11, 0.5f));
    animator->AddAnimation("walking-flip", Animation(0, 5, 0.3f, SDL_FLIP_HORIZONTAL));
    animator->SetAnimation("idle");

    if (role == PLAYER) {
        if (player == nullptr) {                                                            
            player = this;
        }
    } 

    speed = Vec2(0, 0);                                                                 // Inicializa a velocidade como zero.
}

// Destrutor de Character
Character::~Character() {                               
    if (role == PLAYER && player == this) {                                             // Se este era o jogador principal, define o ponteiro estático como nulo.
        player = nullptr;
    }
}

void Character::Start() {
    // 1. Cria um novo GameObject para a Gun.
    GameObject* gunGo = new GameObject();
    gunGo->z = 2;                                                                       // Coloca a Arma na camada de Gameplay (mesma do Jogador)
    gunGo->owner = &associated;                                                         // O dono da Gun é este Character
    gunGo->sub_z = 1;                                                                   // A Gun fica na sub-camada 1 (acima do Character 0)
    
    // 2. Obtém uma referência weak_ptr ao GameObject do Character (usando a função do State).
    std::weak_ptr<GameObject> charWeakPtr = Game::GetInstance().GetCurrentState().GetObjectPtr(&associated);

    // 3. Cria o componente Gun, passando a referência ao Character.
    Gun* gunComponent = new Gun(*gunGo, charWeakPtr);
    gunGo->AddComponent(gunComponent);

    // 4. Adiciona o GameObject da Gun ao State e guarda a weak_ptr
    gun = Game::GetInstance().GetCurrentState().AddObject(gunGo);

    associated.AddComponent(new Collider(associated, Vec2(0.6f, 0.8f), Vec2(0,5)));                      // Cria o colisor aqui para evitar delay de posição
}

void Character::Update(float dt) {
    deathTimer.Update(dt);                          // Atualiza o timer da morte
    damageCooldownTimer.Update(dt);                 // Atualiza o timer do cooldown

    Animator* animator = associated.GetComponent<Animator>();
    //Lógica de morte e remoção (Se HP <= 0)
    if (hp <= 0) {
        if (animator) animator->SetAnimation("dead");
        //Verificar se o tempo passou para remover o objeto
        if (deathTimer.Get() > 2.0f) {
            associated.RequestDelete();
        }
        return;
    }

    //Processa a fila de comandos
    while (!taskQueue.empty()) {                    // Chegamos se há alguma ação na fila
        Command cmd = taskQueue.front();            // Enquanto houver checamos o tipo e
        taskQueue.pop();                            // quando finalizada tiramos da fila

        if (cmd.type == Command::MOVE) {
            //Calcula a direção normalizada
            Vec2 targePos = cmd.pos;
            Vec2 direction = (targePos - associated.box.Center()).Normalized();
            //Define a velocidade baseada na direção e linearSpeed
            speed = direction * linearSpeed;
        } else if (cmd.type == Command::SHOOT) {
            //Tenta obter o shared_ptr da Gun a partir do weak_ptr
            std::shared_ptr<GameObject> gunSharedPtr = gun.lock();
            if (gunSharedPtr) {
                Gun* gunComp = gunSharedPtr->GetComponent<Gun>();
                if (gunComp) {
                    gunComp->Shoot(cmd.pos);        // Chamando o método Shoot da Gun
                }
            } 
        }
    }

    //Atualiza a posição do GameObject com base na velocidade e dt
    associated.box.x += speed.x * dt;
    associated.box.y += speed.y * dt;

    // -- LIMITAÇÃO DE MAPA (TILEMAP)
    // O mapa é um retângulo 1280 x 1536 e começamos em 640 x 512

    float mapMinX = 640.0f;
    float mapMinY = 512.0f;
    float mapWidth = 1280.0f;
    float mapHeight = 1536.0f;
    float mapMaxX = mapMinX + mapWidth;
    float mapMaxY = mapMinY + mapHeight;

    // Vou fazer uma gambiarra pra compensar o erro da largura e altura do char (PULO DO GATO)

    float offsetLeft = 40.0f;                                               // Ajuste da Esquerda
    float offsetRight = 50.0f;                                              // Ajuste da Direita (Aumente aqui para liberar a direita!)
    float offsetTop = 60.0f;                                                // Ajuste de Cima
    float offsetBottom = 5.0f;                                              // Ajuste de Baixo

    // 3. Limites Finais
    mapMinX = mapMinX - offsetLeft;
    mapMaxX = mapMaxX + offsetRight;
    mapMinY = mapMinY - offsetTop;
    mapMaxY = mapMaxY + offsetBottom;

    // Verifica e corrige X
    if (associated.box.x < mapMinX) {
        associated.box.x = mapMinX;
    }
    else if (associated.box.x > mapMaxX - associated.box.w) {               // Subtrai largura do char para não sair pela direita
        associated.box.x = mapMaxX - associated.box.w;
    }

    // Verifica e corrige Y
    if (associated.box.y < mapMinY) {
        associated.box.y = mapMinY;
    } 
    else if (associated.box.y > mapMaxY - associated.box.h) {               // Subtrai altura do char para não sair por baixo
        associated.box.y = mapMaxY - associated.box.h;
    }


    //Atualiza a animação com base no movimento
    if (animator) {
        if (speed.Magnitude() > 0.01f) {                                    // Se estiver se movendo

            // Atualiza a animação APENAS se houver movimento horizontal
            if (speed.x < 0) {                                              // Se a velocidade X é negativa (indo para esquerda)
                facingLeft = true;
            } else if (speed.x > 0) {                                       // Se speed.x > 0 (direita)
                facingLeft = false;
            }

            // Escolhe a animação de andar baseada na direção atual
            if (facingLeft) {
                animator->SetAnimation("walking-flip");
            } else {
                animator->SetAnimation("walking");
            }
        
        // Se estiver parado (escolhe a animação com base na última direção registrada)
        } else {
            if(facingLeft) {
                animator->SetAnimation("idle-flip");
            } else {
                animator->SetAnimation("idle");
            }
                
        }
    }

    speed = Vec2(0,0);                                      // Reseta a velocidade se não houver o comando MOVE (para parar)
    
}

void Character::Damage(int damage) {

    if (hp <= 0) return;                                    // Se HP já é 0 ou menos, não faz nada

    hp -= damage;
    hitSound.Play(1);                                       // Toca o som de dano
    
    if (hp <= 0) {
        // Morte IMEDIATA
        deathSound.Play(1);
        deathTimer.Restart();                               // Começa o timer para remover o corpo

        Collider* collider = associated.GetComponent<Collider>();
        if (collider) {
            associated.RemoveComponent(collider);
        }

        // Correção do CRASH (CameraFallower)
        if (role == PLAYER) {
            Camera::Unfollow();
        }

        // Deleta a arma imediatamente ao morrer
        std::shared_ptr<GameObject> gunSharedPtr = gun.lock();
        if (gunSharedPtr) {
            gunSharedPtr->RequestDelete();
        }
    }
}

void Character::NotifyCollision(GameObject& other) {
    if (other.GetComponent<Zombie>()) {                     // Colisão com o Zumbi
        if (damageCooldownTimer.Get() > 1.0f) {             // Invencibilidade por 1 segundo
            Damage(10);                                     // Dano dado por zumbi
            damageCooldownTimer.Restart();
        }
    }

    Bullet* bullet = other.GetComponent<Bullet>();          // Colisão com bala
    if (bullet) {
        // "Se true, tome dano apenas se o ponteiro for igual a player"
        if (bullet->targetsPlayer && role == PLAYER) {
            Damage(bullet->GetDamage());
        }
        // "E caso false, apenas se for diferente"
        else if (!bullet->targetsPlayer && role == NPC) {
            Damage(bullet->GetDamage());
        }
    }
}

void Character::Render() {                                  // Renderização vazia, delegada aos componentes
}

Vec2 Character::GetCenter() {
    return associated.box.Center();
}

int Character::GetDamage() {                                // Retorna o dano pra bullet saber o dano da arma
    return damage;
}

void Character::Issue(Command task) {                       // Adiciona comando na fila
    taskQueue.push(task);
}



