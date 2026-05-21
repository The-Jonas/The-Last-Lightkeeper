#include "gameplay/Character.h"
#include "engine/GameObject.h"
#include "engine/SpriteRenderer.h"
#include "engine/Animation.h"
#include "engine/Animator.h"
#include "world/Collider.h"
#include "engine/Camera.h"
#include "core/Game.h"
#include "states/stage/StageState.h"
#include "gameplay/StairTrigger.h"
#include <cmath>
#include <string>

namespace {
constexpr int kFootCollisionSkinPx = 1;

Circle FootCircleForCollision(const Rect& box, int skinPx) {
    Circle c{};
    c.radius = std::max(1, static_cast<int>(box.w * 0.25f) - skinPx);
    c.center.x = static_cast<int>(box.x + box.w * 0.5f);
    c.center.y = static_cast<int>(box.y + box.h - c.radius);
    return c;
}

/// Se ainda estiver dentro de geometria estática (polígonos/retângulos sobrepostos no mapa), empurra o jogador para fora.
void TryNudgeOutOfStaticGeometry(StageState* stage, GameObject& go, Collider* collider, bool isElevated) {
    if (!stage || !collider) {
        return;
    }
    if (!stage->level.CheckCollision(FootCircleForCollision(go.box, 0), isElevated)) {
        return;
    }
    const float dists[] = {2.0f, 4.0f, 6.0f, 10.0f, 14.0f};
    const int dirs[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    for (float d : dists) {
        for (const auto& dir : dirs) {
            float mx = static_cast<float>(dir[0]);
            float my = static_cast<float>(dir[1]);
            const float len = std::sqrt(mx * mx + my * my);
            if (len > 1e-3f) {
                mx /= len;
                my /= len;
            }
            go.box.x += mx * d;
            go.box.y += my * d;
            collider->Update(0);
            if (!stage->level.CheckCollision(FootCircleForCollision(go.box, 0), isElevated)) {
                return;
            }
            go.box.x -= mx * d;
            go.box.y -= my * d;
            collider->Update(0);
        }
    }
}
constexpr const char* kIrmaozaoIdleRoot = "Recursos/img/personagens/irmaozao_idle/";
constexpr int kIrmaozaoStripFrameCount = 6;
constexpr float kIrmaozaoStripFrameSeconds = 0.11f;
} // namespace


// Implementação do construtor da classe aninhada Command.
Character::Command::Command(CommandType type, float x, float y): type(type), pos(x, y){}

// Implementação do Character
Character* Character::player = nullptr;

Character::Character(GameObject& associated, std::string spritePath, bool useIrmaozaoIdleStrips)
    : Component(associated),
      irmaozaoIdleStrips(useIrmaozaoIdleStrips) {
    // Definições das animações (modo clássico: spritesheet)
    constexpr int PLAYER_FRAMES_PER_ROW = 1;
    constexpr int PLAYER_ROWS = 1;

    linearSpeed = 250.0f;
    speedMultiplier = 1.0f;
    acceleration = 1000.0f;
    deceleration = 1400.0f;

    currentDirection = Direction::DOWN;
    baseSpritePath = spritePath;

    if (irmaozaoIdleStrips) {
        stripAnimTimer = 0.0f;
        stripFrameIndex = 0;
        const std::string first = IrmaozaoIdleStripPath(currentDirection, stripFrameIndex);
        SpriteRenderer* sprite = new SpriteRenderer(associated, first, PLAYER_FRAMES_PER_ROW, PLAYER_ROWS);
        associated.AddComponent(sprite);
    } else {
        SpriteRenderer* sprite =
            new SpriteRenderer(associated, baseSpritePath + " frente.png", PLAYER_FRAMES_PER_ROW, PLAYER_ROWS);
        associated.AddComponent(sprite);
    }

    if (player == nullptr) {
        player = this;
    }

    speed = Vec2(0, 0);
    targetSpeed = Vec2(0, 0);
}

std::string Character::IrmaozaoIdleStripPath(Direction dir, int frameIndex) const {
    int fi = frameIndex % kIrmaozaoStripFrameCount;
    if (fi < 0) {
        fi += kIrmaozaoStripFrameCount;
    }
    const int n = fi + 1;
    switch (dir) {
    case Direction::UP:
        return std::string(kIrmaozaoIdleRoot) + "Idle trás/FRAME_" + std::to_string(n) + ".png";
    case Direction::DOWN:
        return std::string(kIrmaozaoIdleRoot) + "Idle frente/FRAME_" + std::to_string(n) + ".png";
    case Direction::LEFT:
        return std::string(kIrmaozaoIdleRoot) + "Idle L/FRAME_" + std::to_string(n) + ".png";
    case Direction::RIGHT:
        return std::string(kIrmaozaoIdleRoot) + "Idle R/frame " + std::to_string(n) + ".png";
    }
    return std::string(kIrmaozaoIdleRoot) + "Idle frente/FRAME_1.png";
}

void Character::RefreshIrmaozaoStripSprite() {
    SpriteRenderer* sprite = associated.GetComponent<SpriteRenderer>();
    if (!sprite) {
        return;
    }
    const Vec2 center = associated.box.Center();
    const std::string path = IrmaozaoIdleStripPath(currentDirection, stripFrameIndex);
    sprite->Open(path);
    sprite->SetFrameCount(1, 1);
    sprite->SetFrame(0);
    associated.box.x = center.x - associated.box.w * 0.5f;
    associated.box.y = center.y - associated.box.h * 0.5f;
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

        StageState* stage = Game::TryGetStageState();
        if (!stage) {
            associated.box.x += speed.x * dt;
            associated.box.y += speed.y * dt;
            collider->Update(0);
        } else {

        // --- TESTE DO EIXO X ---
        float oldX = associated.box.x;
        associated.box.x += speed.x * dt;

        // Atualiza a posição do collider manualmente para teste futuro
        collider->Update(0);

        Circle playerCircleX = FootCircleForCollision(associated.box, kFootCollisionSkinPx);

        if (stage->level.CheckCollision(playerCircleX, isElevated)) {
            associated.box.x = oldX;
            speed.x = 0;
            // Update forçado para reverter o collider caso colida
            collider->Update(0); 
        }

        // --- TESTE DO EIXO Y ---
        float oldY = associated.box.y;
        associated.box.y += speed.y * dt;

        collider->Update(0);

        Circle playerCircleY = FootCircleForCollision(associated.box, kFootCollisionSkinPx);

        // NO EIXO Y TAMBÉM:
        if (stage->level.CheckCollision(playerCircleY, isElevated)) {
            associated.box.y = oldY; 
            speed.y = 0;
        }

        collider->Update(0);

        TryNudgeOutOfStaticGeometry(stage, associated, collider, isElevated);
        collider->Update(0);
        }

    } else {
        // Fallback caso o GameObject não tenha Collider (não deve acontecer com os irmãos)
        associated.box.x += speed.x * dt;
        associated.box.y += speed.y * dt;
    }

    // ==============================================
    // Troca de sprite / animação por direção

    Direction newDirection = currentDirection;

    if (std::abs(speed.x) > std::abs(speed.y)) {
        if (speed.x > 0.1f) {
            newDirection = Direction::RIGHT;
        } else if (speed.x < -0.1f) {
            newDirection = Direction::LEFT;
        }
    } else {
        if (speed.y > 0.1f) {
            newDirection = Direction::DOWN; // Y cresce para baixo na tela
        } else if (speed.y < -0.1f) {
            newDirection = Direction::UP;
        }
    }

    if (irmaozaoIdleStrips) {
        if (newDirection != currentDirection && speed.Magnitude() > 5.0f) {
            currentDirection = newDirection;
            stripFrameIndex = 0;
            stripAnimTimer = 0.0f;
            RefreshIrmaozaoStripSprite();
        }
        stripAnimTimer += dt;
        while (stripAnimTimer >= kIrmaozaoStripFrameSeconds) {
            stripAnimTimer -= kIrmaozaoStripFrameSeconds;
            stripFrameIndex = (stripFrameIndex + 1) % kIrmaozaoStripFrameCount;
            RefreshIrmaozaoStripSprite();
        }
    } else if (newDirection != currentDirection && speed.Magnitude() > 5.0f) {
        currentDirection = newDirection;
        SpriteRenderer* sprite = associated.GetComponent<SpriteRenderer>();

        if (sprite) {
            if (currentDirection == Direction::UP) {
                sprite->Open(baseSpritePath + " trás.png");
            } else if (currentDirection == Direction::DOWN) {
                sprite->Open(baseSpritePath + " frente.png");
            } else if (currentDirection == Direction::LEFT) {
                sprite->Open(baseSpritePath + " esquerda.png");
            } else if (currentDirection == Direction::RIGHT) {
                sprite->Open(baseSpritePath + " direita.png");
            }

            sprite->SetFrameCount(1, 1);
        }
    }
}


void Character::NotifyCollision(GameObject& other) {
}


void Character::Render() { 
#ifdef DEBUG
    // Agora desenhamos usando a Fonte da Verdade (associated.box)
    SDL_Renderer* renderer = Game::GetInstance().GetRenderer();
    
    // O X continua centralizado na imagem real
    int cx = (int)(associated.box.x + (associated.box.w / 2) - Camera::pos.x);
    
    // O raio e o Y também baseados na imagem real (idêntico ao Update)
    int r = (int)(associated.box.w * 0.25f); 
    int cy = (int)(associated.box.y + associated.box.h - r - Camera::pos.y);

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
#endif
}

Vec2 Character::GetCenter() {
    return associated.box.Center();
}

float Character::GetFootCircleRadius() const {
    return associated.box.w * 0.25f;
}

Vec2 Character::GetFootCircleCenter() const {
    const float r = GetFootCircleRadius();
    return Vec2(associated.box.x + associated.box.w * 0.5f, associated.box.y + associated.box.h - r);
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



