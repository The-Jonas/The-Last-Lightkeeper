#include "../include/Character.h"
#include "../include/GameObject.h"
#include "../include/SpriteRenderer.h"
#include "../include/Animation.h"
#include "../include/Animator.h"
#include "../include/Collider.h"
#include "../include/Camera.h"
#include <cmath>


// Implementação do construtor da classe aninhada Command.
Character::Command::Command(CommandType type, float x, float y): type(type), pos(x, y){}

// Implementação do Character
Character* Character::player = nullptr;

Character::Character(GameObject& associated, std::string spritePath) : Component(associated){
    // Definições das animações
    constexpr int PLAYER_FRAMES_PER_ROW = 3;
    constexpr int PLAYER_ROWS = 4;
    constexpr int RUN_START = 0;
    constexpr int RUN_END = 5;
    constexpr int IDLE_START = 6;
    constexpr int IDLE_END = 9;
    constexpr int DEATH_START = 10;
    constexpr int DEATH_END = 11;

    // Definições de velocidade e aceleração
    linearSpeed = 200.0f;
    speedMultiplier = 1.0f;
    acceleration = 1000.0f;
    deceleration = 1400.0f;
    facingLeft = false;                                                                 // Começa olhando pra direita

    SpriteRenderer* sprite = new SpriteRenderer(associated, spritePath, PLAYER_FRAMES_PER_ROW, PLAYER_ROWS);          // Usa o path fornecido
    sprite->SetFrameCount(PLAYER_FRAMES_PER_ROW, PLAYER_ROWS);
    associated.AddComponent(sprite);

    Animator* animator = new Animator(associated);
    associated.AddComponent(animator);

    //Adicionando as animações para o character
    animator->AddAnimation("idle", Animation(IDLE_START, IDLE_END, 0.4f));
    animator->AddAnimation("idle-flip", Animation(IDLE_START, IDLE_END, 0.4f, SDL_FLIP_HORIZONTAL));
    animator->AddAnimation("walking", Animation(RUN_START, RUN_END, 0.3f));
    animator->AddAnimation("dead", Animation(DEATH_START, DEATH_END, 0.5f));
    animator->AddAnimation("walking-flip", Animation(RUN_START, RUN_END, 0.3f, SDL_FLIP_HORIZONTAL));
    animator->SetAnimation("idle");

    if (player == nullptr) {                                                            
        player = this;
    } 

    speed = Vec2(0, 0);                                                                 // Inicializa a velocidade como zero.
    targetSpeed = Vec2(0, 0);                                                           // Começa sem alvo de movimento.
}

// Destrutor de Character
Character::~Character() {                               
    if (player == this) {                                                               // Se este era o jogador principal, define o ponteiro estático como nulo.
        player = nullptr;
    }
}

void Character::Start() {
    associated.AddComponent(new Collider(associated, Vec2(0.6f, 0.8f), Vec2(0,5)));                      // Cria o colisor aqui para evitar delay de posição
}

void Character::Update(float dt) {
    Animator* animator = associated.GetComponent<Animator>();
    bool hasMoveCommand = false;

    //Processa a fila de comandos
    while (!taskQueue.empty()) {                    // Chegamos se há alguma ação na fila
        Command cmd = taskQueue.front();            // Enquanto houver checamos o tipo e
        taskQueue.pop();                            // quando finalizada tiramos da fila

        if (cmd.type == Command::MOVE) {
            //Calcula a direção normalizada
            Vec2 targePos = cmd.pos;
            Vec2 direction = (targePos - associated.box.Center()).Normalized();
            //Define velocidade-alvo para suavização de movimento
            targetSpeed = direction * (linearSpeed * speedMultiplier);
            hasMoveCommand = true;
        } 
    }

    if (!hasMoveCommand) {
        targetSpeed = Vec2(0, 0);
    }

    auto approach = [](float current, float target, float maxDelta) {
        float delta = target - current;
        if (std::fabs(delta) <= maxDelta) {
            return target;
        }
        return current + ((delta > 0.0f) ? maxDelta : -maxDelta);
    };

    float changeRate = hasMoveCommand ? acceleration : deceleration;
    float maxDelta = changeRate * dt;
    speed.x = approach(speed.x, targetSpeed.x, maxDelta);
    speed.y = approach(speed.y, targetSpeed.y, maxDelta);

    //Atualiza a posição do GameObject com base na velocidade e dt
    associated.box.x += speed.x * dt;
    associated.box.y += speed.y * dt;

    // -- LIMITAÇÃO DE MAPA (TILEMAP)

    float mapMinX = 640.0f - 40.0f;
    float mapMinY = 512.0f - 60.0f;
    float mapMaxX = 1920.0f + 50.0f; // 640 + 1280
    float mapMaxY = 2048.0f + 5.0f;  // 512 + 1536

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
        if (speed.Magnitude() > 5.0f) {                                     // Se estiver se movendo

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

}


void Character::NotifyCollision(GameObject& other) {
    // Vazio por enquanto
}

void Character::Render() {                                  // Renderização vazia, delegada aos componentes
}

Vec2 Character::GetCenter() {
    return associated.box.Center();
}

void Character::Issue(Command task) {                       // Adiciona comando na fila
    taskQueue.push(task);
}

void Character::SetSpeedMultiplier(float multiplier) { // Ajusta o multiplicador de velocidade
    if (multiplier < 0.1f) {
        speedMultiplier = 0.1f;
        return;
    }
    speedMultiplier = multiplier;
}

void Character::SetBaseSpeed(float speed) { // Ajusta a velocidade base
    if (speed > 0.0f) {
        linearSpeed = speed;
    }
}



