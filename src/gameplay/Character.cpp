#include "gameplay/Character.h"
#include "engine/GameObject.h"
#include "engine/SpriteRenderer.h"
#include "engine/Animation.h"
#include "engine/Animator.h"
#include "world/Collider.h"
#include "engine/Camera.h"
#include "core/Game.h"
#include "core/Resources.h"
#include "states/stage/StageState.h"
#include "gameplay/StairTrigger.h"
#include "core/InputManager.h"
#include "gameplay/Monster.h"
#include "audio/GameSfx.h"
#include <cmath>
#include <string>
#include <iostream>
#include <cstdio>

namespace {
constexpr int kFootCollisionSkinPx = 1;

/// Se ainda estiver dentro de geometria estática (polígonos/retângulos sobrepostos no mapa), empurra o jogador para fora.
void TryNudgeOutOfStaticGeometry(StageState* stage, Character* character, Collider* collider, bool isElevated) {
    if (!stage || !collider || !character) {
        return;
    }

    // Caixa QUADRADA dos pés, com uma margem extra de segurança (skin + 2px).
    auto footBoxWithMargin = [&]() {
        SDL_Rect r = character->GetFootRect();
        const int m = kFootCollisionSkinPx + 2;
        r.x += m; r.y += m; r.w -= 2 * m; r.h -= 2 * m;
        if (r.w < 1) r.w = 1;
        if (r.h < 1) r.h = 1;
        return r;
    };

    if (!stage->level.CheckCollision(footBoxWithMargin(), isElevated)) {
        return;                                                 // Não está preso de verdade. Foge da função imediatamente sem gastar a CPU!
    }

    GameObject& go = character->GetAssociated();
    const float dists[] = {4.0f, 16.0f, 32.0f};
    const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (float d : dists) {
        for (const auto& dir : dirs) {
            float mx = static_cast<float>(dir[0]);
            float my = static_cast<float>(dir[1]);
            const float len = std::sqrt(mx * mx + my * my);
            if (len > 1e-3f) {
                mx /= len;
                my /= len;
            }

            // Empurra para testar a rota de fuga
            go.box.x += mx * d;
            go.box.y += my * d;
            collider->Update(0);

            if (!stage->level.CheckCollision(footBoxWithMargin(), isElevated)) {
                return;                                         // Empurrou pra fora com sucesso! Interrompe o processo.
            }

            // Falhou, estava pior que antes. Desfaz o movimento e tenta outra direção.
            go.box.x -= mx * d;
            go.box.y -= my * d;
            collider->Update(0);
        }
    }
}
constexpr const char* kIrmaozaoIdleRoot = "Recursos/img/personagens/irmaozao_idle/";
constexpr const char* kIrmaozaoWalkRoot = "Recursos/img/personagens/irmaozao_walk/";
constexpr const char* kIrmaozaoIdleLighterRoot = "Recursos/img/personagens/irmaozao_idle_lighter/";
constexpr const char* kIrmaozaoIdleLampRoot = "Recursos/img/personagens/irmaozao_idle_lamp/";
constexpr const char* kIrmaozaoWalkLighterRoot = "Recursos/img/personagens/irmaozao_walk_lighter/";
constexpr const char* kIrmaozaoWalkLampRoot    = "Recursos/img/personagens/irmaozao_walk_lamp/";

// Para as animações do irmãozinho
constexpr const char* kIrmaozinhoIdleRoot = "Recursos/img/personagens/irmaozinho_idle/";
constexpr const char* kIrmaozinhoWalkRoot = "Recursos/img/personagens/irmaozinho_walk/";

constexpr float kIrmaozaoMovingSpeedThreshold = 5.0f;
constexpr int kIrmaozaoStripFrameCount = 6;
constexpr float kIrmaozaoStripFrameSeconds = 0.11f;
} // namespace


// Implementação do construtor da classe aninhada Command.
Character::Command::Command(CommandType type, float x, float y): type(type), pos(x, y){}

// Implementação do Character
Character* Character::player = nullptr;
Character* Character::littleBrother = nullptr;

Character::Character(GameObject& associated, std::string spritePath, bool useIrmaozaoIdleStrips)
    : Component(associated), irmaozaoIdleStrips(useIrmaozaoIdleStrips) {
    // Definições das animações (modo clássico: spritesheet)
    constexpr int PLAYER_FRAMES_PER_ROW = 1;
    constexpr int PLAYER_ROWS = 1;

    linearSpeed = 325.0f;
    speedMultiplier = 1.0f;
    acceleration = 1000.0f;
    deceleration = 1400.0f;

    currentDirection = Direction::DOWN;
    baseSpritePath = spritePath;


    stripAnimTimer = 0.0f;
    stripFrameIndex = 0;
    
    // ambos os personagens pegam o primeiro frame da animação no construtor
    const std::string first = GetAnimStripPath(currentDirection, stripFrameIndex);
    SpriteRenderer* sprite = new SpriteRenderer(associated, first, PLAYER_FRAMES_PER_ROW, PLAYER_ROWS);
    associated.AddComponent(sprite);

    if (player == nullptr) {
        player = this;
    }
    if (littleBrother == this) {
        littleBrother = nullptr;
    }

    speed = Vec2(0, 0);
    targetSpeed = Vec2(0, 0);

    if (useIrmaozaoIdleStrips) {
        if (player == nullptr) player = this; 
    } else {
        if (littleBrother == nullptr) littleBrother = this; 
    }
}

std::string Character::GetAnimStripPath(Direction dir, int frameIndex, HeldPropVisual prop, bool moving) const {
    int fi = frameIndex % kIrmaozaoStripFrameCount;
    if (fi < 0) {
        fi += kIrmaozaoStripFrameCount;
    }
    const int n = fi + 1;

    const char* root;

    if (irmaozaoIdleStrips) {
        if (prop == HeldPropVisual::Lighter) {
            root = moving ? kIrmaozaoWalkLighterRoot : kIrmaozaoIdleLighterRoot;
        } else if (prop == HeldPropVisual::Lamp) {
            root = moving ? kIrmaozaoWalkLampRoot : kIrmaozaoIdleLampRoot;
        } else {
            root = moving ? kIrmaozaoWalkRoot : kIrmaozaoIdleRoot;
        }
    } else {
        root = moving? kIrmaozinhoWalkRoot : kIrmaozinhoIdleRoot;

        if (dir == Direction::RIGHT) {
            dir = Direction::LEFT;
        }
    }

    const char* ext = ".png";

    switch (dir) {
    case Direction::UP:
        return std::string(root) + "trás/FRAME_" + std::to_string(n) + ext;
    case Direction::DOWN:
        return std::string(root) + "frente/FRAME_" + std::to_string(n) + ext;
    case Direction::LEFT:
        return std::string(root) + "esquerda/FRAME_" + std::to_string(n) + ext;
    case Direction::RIGHT:
        return std::string(root) + "direita/FRAME_" + std::to_string(n) + ext;
    }
    return std::string(root) + "frente/FRAME_1" + ext;
}

std::string Character::IrmaozaoPickLampStripPath(Direction dir, int frameIndex) const {
    int fi = frameIndex % kIrmaozaoStripFrameCount;
    if (fi < 0) {
        fi += kIrmaozaoStripFrameCount;
    }
    const int n = fi + 1;
    const std::string root = "Recursos/img/personagens/irmaozao_pick_lamp/";

    if (dir == Direction::LEFT) {
        return root + "esquerda/FRAME_" + std::to_string(n) + ".bmp";
    }

    const char* subdir = "frente";
    switch (dir) {
    case Direction::UP:
        subdir = "trás";
        break;
    case Direction::DOWN:
        subdir = "frente";
        break;
    case Direction::LEFT:
        break;
    case Direction::RIGHT:
        subdir = "direita";
        break;
    }

    return root + subdir + "/FRAME_" + std::to_string(n) + ".bmp";
}

void Character::EnsureBaselineBox() {
    if (baselineBoxW > 0.0f) {
        return;
    }
    baselineBoxW = associated.box.w;
    baselineBoxH = associated.box.h;
}

void Character::RestoreCollisionBox(float centerX, float footY) {
    EnsureBaselineBox();
    if (baselineBoxW <= 0.0f || baselineBoxH <= 0.0f) {
        return;
    }

    // Não esmaga a imagem, só centraliza o pé
    associated.box.x = centerX - (associated.box.w * 0.5f);
    associated.box.y = footY - associated.box.h;

    // Ajusta a escala do colisor com os Setters
    Collider* col = (Collider*)associated.GetComponent<Collider>();
    if (col) {
        float absoluteHitboxW = baselineBoxW * 0.5f;
        float absoluteHitboxH = baselineBoxH * 0.20f;   // altura (2x da original), inalterada

        Vec2 newScale(absoluteHitboxW / associated.box.w, absoluteHitboxH / associated.box.h);
        // Base da caixa ancorada nos pés (footY): a caixa fica na sola.
        float offsetY = (associated.box.h / 2.0f) * (1.0f - newScale.y);

        col->SetScale(newScale);
        col->SetOffset(Vec2(0, offsetY));
    }
}

std::string Character::GetShadowSpritePath() const {
    const bool moving = speed.Magnitude() > kIrmaozaoMovingSpeedThreshold;
    return GetAnimStripPath(currentDirection, stripFrameIndex, HeldPropVisual::None, moving);
}

void Character::NotifyInventoryLightChanged() {
    if (!irmaozaoIdleStrips) {
        return;
    }
    playingPickLampAnim = false;
    pickLampAnimTimer = 0.0f;
    pickLampFrameIndex = 0;
    RefreshAnimSprite();
}

void Character::PlayPickLampAnimation() {
// Animação removida
}

void Character::RefreshAnimSprite() {
    SpriteRenderer* sprite = associated.GetComponent<SpriteRenderer>();
    if (!sprite) return;

    const float centerX = associated.box.x + associated.box.w * 0.5f;
    const float footY = associated.box.y + associated.box.h;
    const bool moving = speed.Magnitude() > kIrmaozaoMovingSpeedThreshold;

    HeldPropVisual prop = HeldPropVisual::None;
    if (StageState* stage = Game::TryGetStageState()) {
        prop = stage->GetInventory().GetHeldPropVisual();
    }
    std::string path = GetAnimStripPath(currentDirection, stripFrameIndex, prop, moving);

    if (path != lastStripSpritePath) {
        Resources::ReloadImage(path);
        lastStripSpritePath = path;
    }

    sprite->Open(path);
    sprite->SetFrameCount(1, 1);
    sprite->SetFrame(0);

    if (!irmaozaoIdleStrips && currentDirection == Direction::RIGHT) {
        sprite->SetFlip(SDL_FLIP_HORIZONTAL);
    } else {
        sprite->SetFlip(SDL_FLIP_NONE);
    }

    RestoreCollisionBox(centerX, footY);
}

// Destrutor de Character
Character::~Character() {                               
    if (player == this) {                                                               // Se este era o jogador principal, define o ponteiro estático como nulo.
        player = nullptr;
    }
    if (littleBrother == this) {
        littleBrother = nullptr;
    }
}

void Character::Start() {
    PreloadAnimationFrames();   // carrega tudo de uma vez antes do gameplay


    EnsureBaselineBox();

    // Define o tamanho proporcional da HitBox (Na sola do pé)
    Vec2 scale(0.45f, 0.20f);
    // Calculo automatico do deslocamento Y
    float offsetY = (associated.box.h / 2.0f) * (1.0f - scale.y);
    // Adiciona o colisor
    associated.AddComponent(new Collider(associated, scale, Vec2(0,offsetY)));          // Cria o colisor aqui para evitar delay de posição
}

void Character::Update(float dt) {

    // Trava de segurança contra a "Espiral da Morte" do Delta Time!
    // Se o frame demorar mais que 0.033s (30 FPS), a física ignora a diferença
    // para o personagem não atravessar as paredes ou bugar os cálculos.
    if (dt > 0.033f) {
        dt = 0.033f;
    }
    
    // ==========================================
    // SE ESTIVER INTERAGINDO, CONGELA TUDO
    // ==========================================
    if (currentState == ActionState::INTERACTING) {
        speed = Vec2(0.0f, 0.0f);                       // zera a velocidade
        targetSpeed = Vec2(0.0f, 0.0f);                 // zera o alvo também

        Mix_HaltChannel(3);                             // Para o loop de passos diretamente pelo canal fixo

        interactTimer -= dt;
        if (interactTimer <= 0.0f) {
            currentState = ActionState::NORMAL;         // Libera o movimento quando o tempo acaba
        }

        // Escondido no armário: mesmo "congelado", o irmãozinho controlado ainda
        // pode usar o poder de visão (revelar o monstro) apertando UseItem (F).
        if (isHidden) {
            UpdateVisionPower(dt);
        }
        return;                                         // Sai do Update prematuramente para não processar movimento!
    }

    //Animator* animator = associated.GetComponent<Animator>();
    bool hasMoveCommand = false;

    //Processa a fila de comandos
    while (!taskQueue.empty()) {                        // Chegamos se há alguma ação na fila
        Command cmd = taskQueue.front();                // Enquanto houver checamos o tipo e
        taskQueue.pop();                                // quando finalizada tiramos da fila

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

        // Caixa de colisão QUADRADA nos pés (AABB estável) — não muda com a animação.
        SDL_Rect footBoxX = GetFootCollisionRect();
        if (stage->level.CheckCollision(footBoxX, isElevated)) {
            associated.box.x = oldX;
            speed.x = 0;
            // Update forçado para reverter o collider caso colida
            collider->Update(0);
        }

        // --- TESTE DO EIXO Y ---
        float oldY = associated.box.y;
        associated.box.y += speed.y * dt;

        collider->Update(0);

        SDL_Rect footBoxY = GetFootCollisionRect();
        // NO EIXO Y TAMBÉM:
        if (stage->level.CheckCollision(footBoxY, isElevated)) {
            associated.box.y = oldY;
            speed.y = 0;
        }

        collider->Update(0);

        TryNudgeOutOfStaticGeometry(stage, this, collider, isElevated);
        collider->Update(0);
        }

    } else {
        // Fallback caso o GameObject não tenha Collider (não deve acontecer com os irmãos)
        associated.box.x += speed.x * dt;
        associated.box.y += speed.y * dt;
    }

    // ==============================================
    // Troca de sprite / animação por direção (congelada enquanto empurra caixa)

    if (currentState != ActionState::PUSHING_BOX) {
        Direction newDirection = currentDirection;

        // Direção de OLHAR vem da INTENÇÃO de movimento (targetSpeed = teclas
        // pressionadas), não da velocidade real. Assim pressionar W vira o
        // personagem p/ CIMA mesmo quando bloqueado (sem mudar de posição).
        if (std::abs(targetSpeed.x) > 0.1f) {
            newDirection = (targetSpeed.x > 0.0f) ? Direction::RIGHT : Direction::LEFT;
        }
        else if (std::abs(targetSpeed.y) > 0.1f) {
            newDirection = (targetSpeed.y > 0.0f) ? Direction::DOWN : Direction::UP;
        }

        // REMOVIDO O IF Agora ambos rodam o loop de animação:
        const bool moving = speed.Magnitude() > kIrmaozaoMovingSpeedThreshold;

        if (moving != stripWasMoving && !playingPickLampAnim) {
            stripWasMoving = moving;
            stripFrameIndex = 0;
            stripAnimTimer = 0.0f;
            RefreshAnimSprite();
        }
        // Sem o requisito `&& moving`: vira mesmo parado/bloqueado.
        if (newDirection != currentDirection) {
            currentDirection = newDirection;
            if (!playingPickLampAnim) {
                stripFrameIndex = 0;
                stripAnimTimer = 0.0f;
            }
            RefreshAnimSprite();
        }
        

        stripAnimTimer += dt;
        while (stripAnimTimer >= kIrmaozaoStripFrameSeconds) {
            stripAnimTimer -= kIrmaozaoStripFrameSeconds;
            stripFrameIndex = (stripFrameIndex + 1) % kIrmaozaoStripFrameCount;
            RefreshAnimSprite();
        }
    }

    StageState* stage = Game::TryGetStageState();
    if (irmaozaoIdleStrips && stage && currentState != ActionState::INTERACTING) {
        const Vec2 foot = GetFootCircleCenter();
        const FootstepSurface surface = stage->level.QueryFootstepSurface(
            static_cast<int>(foot.x), static_cast<int>(foot.y), isElevated);
        GameSfx::UpdateBigBrotherFootsteps(dt, speed.Magnitude(), true, surface);
    }

    // ============================================================
    // SISTEMA DE SANIDADE (BASEADO NO CONTATO REAL COM A LUZ)
    // ============================================================

    float myLightContact = 1.0f;

    if (stage) {
        // Puxa a variável correta dependendo de qual irmão é
        if (irmaozaoIdleStrips) {
            myLightContact = stage->bigIlluminationLevel;
        } else {
            myLightContact = stage->smallIlluminationLevel;
        }
    }

    const float darknessThreshold = 0.1f;   // Abaixo de 10% de luz é escuridão
    const float sanityDrainRate = 8.0f;     // Perde 8 pontos de sanidade por segundo
    const float sanityRegenRate = 12.0f;    // Recupera 12 pontos de sanidade por segundo

    if (myLightContact < darknessThreshold) {
        // Tá escuro, drena a sanidade...
        sanity -= sanityDrainRate * dt;

        if (sanity <= 0.0f) {
            sanity = 0.0f;
            if (Game::debugMode) {
                std::cout << "[GAME OVER] O personagem foi engolido pela escuridao!" << std::endl;
                std::cout << "[GAME OVER] Sanidade atual: " << sanity << std::endl;
            }
        }
    }
    else {
        // Tá claro, recupera...
        sanity += sanityRegenRate * dt;
        if (sanity > kMaxSanity) {
            sanity = kMaxSanity;            // Trava no 100, nunca ultrapassa
        }
    }

    // Poder de visão do irmãozinho (revela o monstro).
    UpdateVisionPower(dt);
}

void Character::UpdateVisionPower(float dt) {
    if (irmaozaoIdleStrips) {   // só o irmãozinho tem o poder
        return;
    }

    if (visionPowerTimer > 0.0f) {
        visionPowerTimer -= dt;
        if (visionPowerTimer <= 0.0f) {
            visionPowerTimer = 0.0f;
            visionCooldown = kVisionCooldown;  // Começa a recarga ao acabar
        }
    }

    if (visionCooldown > 0.0f) visionCooldown -= dt;

    StageState* stageCtrl = Game::TryGetStageState();
    bool isBeingControlled = stageCtrl && stageCtrl->GetControlledCharacter() == this;
    const bool inputFrozen = stageCtrl && stageCtrl->IsPlayerInputFrozen();

    // Pode ativar quando NÃO está travado numa interação — OU quando está
    // escondido no armário (aí ele está "congelado" como INTERACTING, mas ainda
    // assim pode usar o poder de visão).
    const bool actionAllowed = (currentState != ActionState::INTERACTING) || isHidden;

    // A habilidade de visão do irmãozinho agora é no UseItem (F) — o Interact (E)
    // passou a servir para ele ENTRAR/SAIR de armários, como o irmãozão.
    if (isBeingControlled && !inputFrozen && InputManager::GetInstance().ActionPress(GameAction::UseItem) &&
        visionPowerTimer <= 0.0f && visionCooldown <= 0.0f && actionAllowed) {

        visionPowerTimer = kVisionDuration;
        if (Game::debugMode) std::cout << "[PODER] Irmãozinho ativou visão!\n";

        if (stageCtrl) {
            for (auto& go : stageCtrl->GetObjectArray()) {
                if (Monster* m = go->GetComponent<Monster>()) {
                    m->ActivateVision(kVisionDuration);
                }
            }
        }
    }
}


void Character::NotifyCollision(GameObject& other) {
}

SDL_Rect Character::GetInteractionRect(int targetHeightLevel) const {
    int reachDistance = 50;                                                 // o quão longe o braço chega
    int boxSize = 60;                                                       // o tamanho da "mão" que vai checar a colisão
    
    int centerX = associated.box.x + (associated.box.w / 2);
    int footY = associated.box.y + associated.box.h;

    // FAZENDO A MÁGICA DA ALTURA (EIXO Z FALSO)
    int zOffset = 0;
    if (targetHeightLevel == 1) {
        zOffset = 140;
    }
    else if (targetHeightLevel == 2) {
        zOffset = 260;
    }

    // Centraliza a caixa de interação no pé inicialmente
    SDL_Rect interactBox = { 
        centerX - (boxSize / 2), 
        (footY - (boxSize / 2)) - zOffset, 
        boxSize, 
        boxSize 
    };

    // Desloca a caixa para a frente baseado na direção já existente!
    switch (currentDirection) {
        case Direction::UP:
            interactBox.y -= reachDistance;
            interactBox.h += 20;
            break;
        case Direction::DOWN:
            interactBox.y += reachDistance;
            break;
        case Direction::LEFT:
            interactBox.x -= reachDistance;
            interactBox.y -= 20;
            break;
        case Direction::RIGHT:
            interactBox.x += reachDistance;
            interactBox.y -= 20;
            break;
    }

    return interactBox;
}

void Character::PositionForCoop(Character* leader) {
    if (!leader) return;

    // Lê a mente do líder: Copia a direção pra olharem pro mesmo lado
    this->currentDirection = leader-> currentDirection;

    RefreshAnimSprite();

    // POSIÇÃO:
    float distance = 45.0f; // O quão longe o Irmãozinho fica na frente (ajuste no olho)

    // Alinha os centros no eixo X perfeitamente
    this->associated.box.x = leader->associated.box.x + (leader->associated.box.w / 2.0f) - (this->associated.box.w / 2.0f);

    // Alinha a base dos pés no eixo Y perfeitamente
    float leaderFootY = leader->associated.box.y + leader->associated.box.h;
    float myFootY = this->associated.box.y + this->associated.box.h;
    this->associated.box.y += (leaderFootY - myFootY);

    // Move para a direção que estão olhando
    switch (leader->currentDirection) {
        case Direction::UP:
            this->associated.box.y -= distance;
            break;
        case Direction::DOWN:
            this->associated.box.y += distance;
            break;
        case Direction::LEFT:
            this->associated.box.x -= distance;
            break;
        case Direction::RIGHT:
            this->associated.box.x += distance;
            break;
    }
}

void Character::PreloadAnimationFrames() {
    const Direction allDirs[] = {
        Direction::UP, Direction::DOWN, Direction::LEFT, Direction::RIGHT
    };
 
    // Carrega idle e walk (moving = false / true)
    for (bool moving : {false, true}) {
        for (Direction d : allDirs) {
 
            if (irmaozaoIdleStrips) {
                // Irmãozão — precisa cobrir os 3 estados de prop também
                // (None, Lighter, Lamp) porque cada um é uma pasta diferente
                for (HeldPropVisual prop : {HeldPropVisual::None,
                                             HeldPropVisual::Lighter,
                                             HeldPropVisual::Lamp}) {
                    for (bool moving : {false, true}) {  // <- moving já estava aqui, cobre automaticamente
                        for (Direction d : allDirs) {
                            for (int f = 0; f < kIrmaozaoStripFrameCount; f++) {
                                std::string path = GetAnimStripPath(d, f, prop, moving);
                                Resources::GetImage(path);
                            }
                        }
                    }
                }
            } else {
                // Irmãozinho — sem variação de prop
                for (int f = 0; f < kIrmaozaoStripFrameCount; f++) {
                    std::string path = GetAnimStripPath(d, f, HeldPropVisual::None, moving);
                    Resources::GetImage(path);
                }
            }
        }
    }
}


void Character::Render() {
#ifdef DEBUG
    SDL_Renderer* renderer = Game::GetInstance().GetRenderer();

    // Caixa de COLISÃO dos pés (verde) — quadrado estável.
    SDL_Rect foot = GetFootRect();
    foot.x -= (int)Camera::pos.x;
    foot.y -= (int)Camera::pos.y;
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_RenderDrawRect(renderer, &foot);

    // HURTBOX do corpo (vermelho) — quadrado estável (dano do monstro).
    SDL_Rect hit = GetHitRect();
    hit.x -= (int)Camera::pos.x;
    hit.y -= (int)Camera::pos.y;
    SDL_SetRenderDrawColor(renderer, 255, 60, 60, 255);
    SDL_RenderDrawRect(renderer, &hit);
#endif
}

void Character::ClearMovement() {
    speed = Vec2(0.0f, 0.0f);
    targetSpeed = Vec2(0.0f, 0.0f);
    while (!taskQueue.empty()) {
        taskQueue.pop();
    }
}

Vec2 Character::GetCenter() {
    return associated.box.Center();
}

float Character::GetFootCircleRadius() const {
    // Em vez de usar a largura da arte atual, usamos a largura base imutável
    float baseW = (baselineBoxW > 0.0f) ? baselineBoxW : associated.box.w;
    return baseW * 0.25f;
}

Vec2 Character::GetFootCircleCenter() const {
    const float r = GetFootCircleRadius();
    return Vec2(associated.box.x + associated.box.w * 0.5f, associated.box.y + associated.box.h - r);
}

// centerX (box.x+box.w/2) e footY (box.y+box.h) são mantidos ESTÁVEIS a cada
// frame por RefreshAnimSprite/RestoreCollisionBox — então ancoramos as hitboxes
// neles e usamos SEMPRE a caixa-base (baselineBoxW/H) para o tamanho, de forma
// que as caixas não mudem de tamanho nem de lugar quando a animação troca.
SDL_Rect Character::GetFootRect() const {
    // A caixa de colisão dos pés é a do componente Collider — mantida ESTÁVEL e no
    // tamanho certo (tira fina nos pés) por RestoreCollisionBox + Collider::Update,
    // independente da animação. É a caixa que o jogo já usa como "verdade".
    if (Collider* col = associated.GetComponent<Collider>()) {
        const Rect& b = col->box;
        return SDL_Rect{ static_cast<int>(b.x), static_cast<int>(b.y),
                         static_cast<int>(b.w), static_cast<int>(b.h) };
    }
    // Fallback (sem Collider — não deve ocorrer com os irmãos): quadrado pela base.
    const float baseW = (baselineBoxW > 0.0f) ? baselineBoxW : associated.box.w;
    const float side = baseW * 0.5f;
    const float centerX = associated.box.x + associated.box.w * 0.5f;
    const float footY   = associated.box.y + associated.box.h;
    return SDL_Rect{ static_cast<int>(centerX - side * 0.5f),
                     static_cast<int>(footY - side),
                     static_cast<int>(side), static_cast<int>(side) };
}

SDL_Rect Character::GetFootCollisionRect() const {
    SDL_Rect r = GetFootRect();
    const int s = kFootCollisionSkinPx;   // margem contra erros de ponto flutuante
    r.x += s; r.y += s; r.w -= 2 * s; r.h -= 2 * s;
    if (r.w < 1) r.w = 1;
    if (r.h < 1) r.h = 1;
    return r;
}

SDL_Rect Character::GetHitRect() const {
    const float baseW = (baselineBoxW > 0.0f) ? baselineBoxW : associated.box.w;
    const float baseH = (baselineBoxH > 0.0f) ? baselineBoxH : associated.box.h;
    const float w = baseW * 0.50f;
    const float h = baseH * 0.60f;
    const float centerX = associated.box.x + associated.box.w * 0.5f;
    const float footY   = associated.box.y + associated.box.h;
    const float centerY = footY - baseH * 0.45f;   // tronco (fixo pela base, não pela arte atual)
    return SDL_Rect{ static_cast<int>(centerX - w * 0.5f),
                     static_cast<int>(centerY - h * 0.5f),
                     static_cast<int>(w), static_cast<int>(h) };
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



