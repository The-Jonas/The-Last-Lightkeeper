#include "../include/Character.h"
#include "../include/GameObject.h"
#include "../include/SpriteRenderer.h"
#include "../include/Animation.h"
#include "../include/Animator.h"
#include "../include/Collider.h"
#include "../include/Camera.h"
#include "../include/Game.h"
#include "../include/StageState.h"
#include "../include/StairTrigger.h"
#include <cmath>


// Implementação do construtor da classe aninhada Command.
Character::Command::Command(CommandType type, float x, float y): type(type), pos(x, y){}

// Implementação do Character
Character* Character::player = nullptr;

Character::Character(GameObject& associated, std::string spritePath) : Component(associated){
    // Definições das animações
    constexpr int PLAYER_FRAMES_PER_ROW = 1;
    constexpr int PLAYER_ROWS = 1;
    constexpr int RUN_START = 0;
    constexpr int RUN_END = 5;
    constexpr int IDLE_START = 6;
    constexpr int IDLE_END = 9;
    constexpr int DEATH_START = 10;
    constexpr int DEATH_END = 11;

    // Definições de velocidade e aceleração (+25% vs older default)
    linearSpeed = 250.0f;
    speedMultiplier = 1.0f;
    acceleration = 1000.0f;
    deceleration = 1400.0f;

    // Inicializa a direção e salva o nome base
    currentDirection = Direction::DOWN;
    baseSpritePath = spritePath;

    SpriteRenderer* sprite = new SpriteRenderer(associated, baseSpritePath + " frente.png", PLAYER_FRAMES_PER_ROW, PLAYER_ROWS);          // Usa o path fornecido
    //sprite->SetScale(0.6f, 0.6f);
    associated.AddComponent(sprite);

    //Animator* animator = new Animator(associated);
    //associated.AddComponent(animator);

    //Adicionando as animações para o character
    //animator->AddAnimation("idle", Animation(IDLE_START, IDLE_END, 0.4f));
    //animator->AddAnimation("idle-flip", Animation(IDLE_START, IDLE_END, 0.4f, SDL_FLIP_HORIZONTAL));
    //animator->AddAnimation("walking", Animation(RUN_START, RUN_END, 0.3f));
    //animator->AddAnimation("dead", Animation(DEATH_START, DEATH_END, 0.5f));
    //animator->AddAnimation("walking-flip", Animation(RUN_START, RUN_END, 0.3f, SDL_FLIP_HORIZONTAL));
    //animator->SetAnimation("idle");

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
    associated.AddComponent(new Collider(associated, Vec2(0.45f, 0.1f), Vec2(0,80)));                      // Cria o colisor aqui para evitar delay de posição
}

void Character::Update(float dt) {
    //Animator* animator = associated.GetComponent<Animator>();
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
    // associated.box.x += speed.x * dt;
    // associated.box.y += speed.y * dt;

    // REFATORANDO COM A LÓGICA DE MOVIMENTAÇÃO COM SLIDE COLLISION

    Collider* collider = associated.GetComponent<Collider>();

    if (collider != nullptr){

        StageState* stage = (StageState*)&Game::GetInstance().GetCurrentState();

        // --- TESTE DO EIXO X ---
        float oldX = associated.box.x;
        associated.box.x += speed.x * dt;

        // Atualiza a posição do collider manualmente para teste futuro
        collider->Update(0);

        // CRIAMOS UM CÍRCULO DINÂMICO USANDO AS MEDIDAS DO COLLIDER
        Circle playerCircleX;
        // O raio é a metade da largura. Se a colisão ficar muito "gorda", você pode multiplicar por 0.8 aqui!
        playerCircleX.radius = (int)(collider->box.w * 0.35f); 
        playerCircleX.center.x = (int)(collider->box.x + (collider->box.w / 2));
        playerCircleX.center.y = (int)(collider->box.y + collider->box.h - playerCircleX.radius);

        // Passamos o isElevated direto para a engine resolver
        if (stage->level.CheckCollision(playerCircleX, isElevated)) {
            associated.box.x = oldX;
            speed.x = 0;
        }

        // --- TESTE DO EIXO Y ---
        float oldY = associated.box.y;
        associated.box.y += speed.y * dt;

        collider->Update(0);

        Circle playerCircleY;
        playerCircleY.radius = (int)(collider->box.w * 0.35f);
        playerCircleY.center.x = (int)(collider->box.x + (collider->box.w / 2));
        playerCircleY.center.y = (int)(collider->box.y + collider->box.h - playerCircleY.radius);

        // NO EIXO Y TAMBÉM:
        if (stage->level.CheckCollision(playerCircleY, isElevated)) {
            associated.box.y = oldY; 
            speed.y = 0;
        }

        collider->Update(0);
    
    } else {
        // Fallback caso o GameObject não tenha Collider (não deve acontecer com os irmãos)
        associated.box.x += speed.x * dt;
        associated.box.y += speed.y * dt;
    }

    // ==============================================
    // NOVA LÓGICA DE TROCA DE SPRITE (SEM ANIMAÇÃO)

    Direction newDirection = currentDirection;

    // Descobre para onde está tentando andar (Prioriza o eixo X se estiver andando na diagonal)
    if (std::abs(speed.x) > std::abs(speed.y)) {
        if (speed.x > 0.1f) newDirection = Direction::RIGHT;
        else if (speed.x < -0.1f) newDirection = Direction::LEFT;
    } else {
        if (speed.y > 0.1f) newDirection = Direction::DOWN; // Y cresce para baixo na tela
        else if (speed.y < -0.1f) newDirection = Direction::UP;
    }

    // Se o personagem mudou de direção, nós abrimos a imagem correspondente
    if (newDirection != currentDirection && speed.Magnitude() > 5.0f) {
        currentDirection = newDirection;
        SpriteRenderer* sprite = associated.GetComponent<SpriteRenderer>();
        
        if (sprite) {
            if (currentDirection == Direction::UP) {
                sprite->Open(baseSpritePath + " trás.png");
            } 
            else if (currentDirection == Direction::DOWN) {
                sprite->Open(baseSpritePath + " frente.png");
            } 
            else if (currentDirection == Direction::LEFT) {
                sprite->Open(baseSpritePath + " esquerda.png");
            } 
            else if (currentDirection == Direction::RIGHT) {
                sprite->Open(baseSpritePath + " direita.png");
            }
            
            // Força o sprite a resetar os frames já que não é mais um spritesheet
            sprite->SetFrameCount(1, 1);
        }
    }
}


void Character::NotifyCollision(GameObject& other) {
}


void Character::Render() { 
#ifdef DEBUG
    Collider* collider = associated.GetComponent<Collider>();
    if (collider) {
        SDL_Renderer* renderer = Game::GetInstance().GetRenderer();
        
        // O X continua centralizado
        int cx = (int)(collider->box.x + (collider->box.w / 2) - Camera::pos.x);
        
        // Em vez de usar o centro da box, usamos a base da box menos o raio,
        // exatamente como fizemos no Update() 
        int r = (int)(collider->box.w * 0.35f); 
        int cy = (int)(collider->box.y + collider->box.h - r - Camera::pos.y);

        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255); // Cor Verde
        
        // Desenha o círculo linha por linha...
        const int kSeg = 36;
        for (int i = 0; i < kSeg; i++) {
            float a0 = ((float)i / kSeg) * 2.0f * M_PI;
            float a1 = ((float)(i + 1) / kSeg) * 2.0f * M_PI;
            
            SDL_RenderDrawLine(renderer, 
                cx + (int)(cos(a0) * r), cy + (int)(sin(a0) * r), 
                cx + (int)(cos(a1) * r), cy + (int)(sin(a1) * r));
        }
    }
#endif
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



