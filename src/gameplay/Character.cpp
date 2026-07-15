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
constexpr float kCollisionWidthBoost = 1.15f; // Aumento horizontal (+15%) na hitbox dos pés

void TryNudgeOutOfStaticGeometry(StageState* stage, Character* character, Collider* collider, bool isElevated) {
    if (!stage || !collider || !character) return;

    auto footBoxWithMargin = [&]() {
        SDL_Rect r = character->GetFootRect();
        const int m = kFootCollisionSkinPx + 2;
        r.x += m; r.y += m; r.w -= 2 * m; r.h -= 2 * m;
        if (r.w < 1) r.w = 1;
        if (r.h < 1) r.h = 1;
        return r;
    };

    if (!stage->level.CheckCollision(footBoxWithMargin(), isElevated)) return;

    GameObject& go = character->GetAssociated();
    const float dists[] = {4.0f, 16.0f, 32.0f};
    const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    
    for (float d : dists) {
        for (const auto& dir : dirs) {
            float mx = static_cast<float>(dir[0]);
            float my = static_cast<float>(dir[1]);
            const float len = std::sqrt(mx * mx + my * my);
            if (len > 1e-3f) { mx /= len; my /= len; }

            go.box.x += mx * d;
            go.box.y += my * d;
            collider->Update(0);

            if (!stage->level.CheckCollision(footBoxWithMargin(), isElevated)) return;

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

constexpr const char* kIrmaozinhoIdleRoot = "Recursos/img/personagens/irmaozinho_idle/";
constexpr const char* kIrmaozinhoWalkRoot = "Recursos/img/personagens/irmaozinho_walk/";

constexpr float kIrmaozaoMovingSpeedThreshold = 5.0f;
constexpr int kIrmaozaoStripFrameCount = 6;
constexpr float kIrmaozaoStripFrameSeconds = 0.11f;
} // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  Construtores e Setup Inicial
// ─────────────────────────────────────────────────────────────────────────────
Character::Command::Command(CommandType type, float x, float y): type(type), pos(x, y) {}

Character* Character::player = nullptr;
Character* Character::littleBrother = nullptr;

Character::Character(GameObject& associated, std::string spritePath, bool useIrmaozaoIdleStrips)
    : Component(associated), irmaozaoIdleStrips(useIrmaozaoIdleStrips) {
    
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
    
    const std::string first = GetAnimStripPath(currentDirection, stripFrameIndex);
    SpriteRenderer* sprite = new SpriteRenderer(associated, first, PLAYER_FRAMES_PER_ROW, PLAYER_ROWS);
    associated.AddComponent(sprite);

    if (player == nullptr) player = this;
    if (littleBrother == this) littleBrother = nullptr;

    speed = Vec2(0, 0);
    targetSpeed = Vec2(0, 0);

    if (useIrmaozaoIdleStrips) {
        if (player == nullptr) player = this; 
    } else {
        if (littleBrother == nullptr) littleBrother = this; 
    }
}

Character::~Character() {                               
    if (player == this) player = nullptr;
    if (littleBrother == this) littleBrother = nullptr;
}

void Character::Start() {
    PreloadAnimationFrames(); 
    EnsureBaselineBox();

    // Adiciona o colisor na base (sola dos pés)
    Vec2 scale(0.45f, 0.20f);
    float offsetY = (associated.box.h / 2.0f) * (1.0f - scale.y);
    associated.AddComponent(new Collider(associated, scale, Vec2(0, offsetY)));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Gerenciamento de Animação e Sprites
// ─────────────────────────────────────────────────────────────────────────────
std::string Character::GetAnimStripPath(Direction dir, int frameIndex, HeldPropVisual prop, bool moving) const {
    int fi = frameIndex % kIrmaozaoStripFrameCount;
    if (fi < 0) fi += kIrmaozaoStripFrameCount;
    const int n = fi + 1;

    const char* root;
    if (irmaozaoIdleStrips) {
        if (prop == HeldPropVisual::Lighter) root = moving ? kIrmaozaoWalkLighterRoot : kIrmaozaoIdleLighterRoot;
        else if (prop == HeldPropVisual::Lamp) root = moving ? kIrmaozaoWalkLampRoot : kIrmaozaoIdleLampRoot;
        else root = moving ? kIrmaozaoWalkRoot : kIrmaozaoIdleRoot;
    } else {
        root = moving ? kIrmaozinhoWalkRoot : kIrmaozinhoIdleRoot;
        if (dir == Direction::RIGHT) dir = Direction::LEFT;
    }

    const char* ext = ".png";
    switch (dir) {
        case Direction::UP:    return std::string(root) + "trás/FRAME_" + std::to_string(n) + ext;
        case Direction::DOWN:  return std::string(root) + "frente/FRAME_" + std::to_string(n) + ext;
        case Direction::LEFT:  return std::string(root) + "esquerda/FRAME_" + std::to_string(n) + ext;
        case Direction::RIGHT: return std::string(root) + "direita/FRAME_" + std::to_string(n) + ext;
    }
    return std::string(root) + "frente/FRAME_1" + ext;
}

std::string Character::IrmaozaoPickLampStripPath(Direction dir, int frameIndex) const {
    int fi = frameIndex % kIrmaozaoStripFrameCount;
    if (fi < 0) fi += kIrmaozaoStripFrameCount;
    const int n = fi + 1;
    const std::string root = "Recursos/img/personagens/irmaozao_pick_lamp/";

    if (dir == Direction::LEFT) return root + "esquerda/FRAME_" + std::to_string(n) + ".bmp";

    const char* subdir = "frente";
    switch (dir) {
        case Direction::UP:    subdir = "trás"; break;
        case Direction::DOWN:  subdir = "frente"; break;
        case Direction::LEFT:  break;
        case Direction::RIGHT: subdir = "direita"; break;
    }
    return root + subdir + "/FRAME_" + std::to_string(n) + ".bmp";
}

void Character::EnsureBaselineBox() {
    if (baselineBoxW > 0.0f) return;
    baselineBoxW = associated.box.w;
    baselineBoxH = associated.box.h;
}

void Character::RestoreCollisionBox(float centerX, float footY) {
    EnsureBaselineBox();
    if (baselineBoxW <= 0.0f || baselineBoxH <= 0.0f) return;

    associated.box.x = centerX - (associated.box.w * 0.5f);
    associated.box.y = footY - associated.box.h;

    if (Collider* col = (Collider*)associated.GetComponent<Collider>()) {
        float absoluteHitboxW = baselineBoxW * 0.5f * kCollisionWidthBoost;
        float absoluteHitboxH = baselineBoxH * 0.20f; 

        Vec2 newScale(absoluteHitboxW / associated.box.w, absoluteHitboxH / associated.box.h);
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
    if (!irmaozaoIdleStrips) return;
    playingPickLampAnim = false;
    pickLampAnimTimer = 0.0f;
    pickLampFrameIndex = 0;
    RefreshAnimSprite();
}

void Character::PlayPickLampAnimation() {}

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
    sprite->SetFlip((!irmaozaoIdleStrips && currentDirection == Direction::RIGHT) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);

    RestoreCollisionBox(centerX, footY);
}

void Character::PreloadAnimationFrames() {
    const Direction allDirs[] = { Direction::UP, Direction::DOWN, Direction::LEFT, Direction::RIGHT };
    float maxW = 0.0f, maxH = 0.0f;

    auto consider = [&](const std::string& path) {
        auto tex = Resources::GetImage(path);
        if (!tex) return;
        int w = 0, h = 0;
        if (SDL_QueryTexture(tex.get(), nullptr, nullptr, &w, &h) == 0) {
            if (w > maxW) maxW = static_cast<float>(w);
            if (h > maxH) maxH = static_cast<float>(h);
        }
    };

    for (bool moving : {false, true}) {
        for (Direction d : allDirs) {
            if (irmaozaoIdleStrips) {
                for (HeldPropVisual prop : {HeldPropVisual::None, HeldPropVisual::Lighter, HeldPropVisual::Lamp}) {
                    for (int f = 0; f < kIrmaozaoStripFrameCount; f++) {
                        consider(GetAnimStripPath(d, f, prop, moving));
                    }
                }
            } else {
                for (int f = 0; f < kIrmaozaoStripFrameCount; f++) {
                    consider(GetAnimStripPath(d, f, HeldPropVisual::None, moving));
                }
            }
        }
    }

    if (maxW > 0.0f && maxH > 0.0f) {
        baselineBoxW = maxW;
        baselineBoxH = maxH;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Update Principal
// ─────────────────────────────────────────────────────────────────────────────
void Character::Update(float dt) {
    if (dt > 0.033f) dt = 0.033f; // Cap em 30 FPS para estabilidade física
    
    // ── Interação ─────────────────────────────────────────────────────────────
    if (currentState == ActionState::INTERACTING) {
        speed = Vec2(0.0f, 0.0f);
        targetSpeed = Vec2(0.0f, 0.0f);
        Mix_HaltChannel(3);

        interactTimer -= dt;
        if (interactTimer <= 0.0f) currentState = ActionState::NORMAL;

        if (isHidden) UpdateVisionPower(dt);
        return; 
    }

    // ── Fila de Movimento ─────────────────────────────────────────────────────
    bool hasMoveCommand = false;
    while (!taskQueue.empty()) {
        Command cmd = taskQueue.front();
        taskQueue.pop();

        if (cmd.type == Command::MOVE) {
            Vec2 direction = (cmd.pos - associated.box.Center()).Normalized();
            targetSpeed = direction * (linearSpeed * speedMultiplier);
            hasMoveCommand = true;
        } 
    }

    if (!hasMoveCommand) targetSpeed = Vec2(0, 0);

    auto approach = [](float current, float target, float maxDelta) {
        float delta = target - current;
        if (std::fabs(delta) <= maxDelta) return target;
        return current + ((delta > 0.0f) ? maxDelta : -maxDelta);
    };

    float changeRate = hasMoveCommand ? acceleration : deceleration;
    float maxDelta = changeRate * dt;
    speed.x = approach(speed.x, targetSpeed.x, maxDelta);
    speed.y = approach(speed.y, targetSpeed.y, maxDelta);

    // ── Colisão Slide ─────────────────────────────────────────────────────────
    Collider* collider = associated.GetComponent<Collider>();

    if (collider != nullptr) {
        StageState* stage = Game::TryGetStageState();
        if (!stage) {
            associated.box.x += speed.x * dt;
            associated.box.y += speed.y * dt;
            collider->Update(0);
        } else {
            // Eixo X
            float oldX = associated.box.x;
            associated.box.x += speed.x * dt;
            collider->Update(0);

            if (stage->level.CheckCollision(GetFootCollisionRect(), isElevated)) {
                associated.box.x = oldX;
                speed.x = 0;
                collider->Update(0);
            }

            // Eixo Y
            float oldY = associated.box.y;
            associated.box.y += speed.y * dt;
            collider->Update(0);

            if (stage->level.CheckCollision(GetFootCollisionRect(), isElevated)) {
                associated.box.y = oldY;
                speed.y = 0;
            }

            collider->Update(0);
            TryNudgeOutOfStaticGeometry(stage, this, collider, isElevated);
            collider->Update(0);
        }
    } else {
        associated.box.x += speed.x * dt;
        associated.box.y += speed.y * dt;
    }

    // ── Animação e Direção ────────────────────────────────────────────────────
    if (currentState != ActionState::PUSHING_BOX) {
        Direction newDirection = currentDirection;

        if (std::abs(targetSpeed.x) > 0.1f) newDirection = (targetSpeed.x > 0.0f) ? Direction::RIGHT : Direction::LEFT;
        else if (std::abs(targetSpeed.y) > 0.1f) newDirection = (targetSpeed.y > 0.0f) ? Direction::DOWN : Direction::UP;

        const bool moving = speed.Magnitude() > kIrmaozaoMovingSpeedThreshold;

        if (moving != stripWasMoving && !playingPickLampAnim) {
            stripWasMoving = moving;
            stripFrameIndex = 0;
            stripAnimTimer = 0.0f;
            RefreshAnimSprite();
        }

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

    // ── Efeitos Sonoros ───────────────────────────────────────────────────────
    StageState* stage = Game::TryGetStageState();
    if (irmaozaoIdleStrips && stage && currentState != ActionState::INTERACTING) {
        const Vec2 foot = GetFootCircleCenter();
        const FootstepSurface surface = stage->level.QueryFootstepSurface(static_cast<int>(foot.x), static_cast<int>(foot.y), isElevated);
        GameSfx::UpdateBigBrotherFootsteps(dt, speed.Magnitude(), true, surface);
    }

    // ── Sistema de Sanidade ───────────────────────────────────────────────────
    float myLightContact = 1.0f;
    if (stage) {
        myLightContact = irmaozaoIdleStrips ? stage->bigIlluminationLevel : stage->smallIlluminationLevel;
    }

    const float darknessThreshold = 0.1f; 
    const float sanityDrainRate = 8.0f; 
    const float sanityRegenRate = 12.0f; 

    if (myLightContact < darknessThreshold) {
        sanity -= sanityDrainRate * dt;
        if (sanity <= 0.0f) sanity = 0.0f;
    } else {
        sanity += sanityRegenRate * dt;
        if (sanity > kMaxSanity) sanity = kMaxSanity; 
    }

    UpdateVisionPower(dt);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Poder de Visão e Interação
// ─────────────────────────────────────────────────────────────────────────────
void Character::UpdateVisionPower(float dt) {
    if (irmaozaoIdleStrips) return; 

    if (visionPowerTimer > 0.0f) {
        visionPowerTimer -= dt;
        if (visionPowerTimer <= 0.0f) {
            visionPowerTimer = 0.0f;
            visionCooldown = kVisionCooldown;
        }
    }

    if (visionCooldown > 0.0f) visionCooldown -= dt;

    StageState* stageCtrl = Game::TryGetStageState();
    bool isBeingControlled = stageCtrl && stageCtrl->GetControlledCharacter() == this;
    const bool inputFrozen = stageCtrl && stageCtrl->IsPlayerInputFrozen();
    const bool actionAllowed = (currentState != ActionState::INTERACTING) || isHidden;

    if (isBeingControlled && !inputFrozen && InputManager::GetInstance().ActionPress(GameAction::UseItem) &&
        visionPowerTimer <= 0.0f && visionCooldown <= 0.0f && actionAllowed) {

        visionPowerTimer = kVisionDuration;

        if (stageCtrl) {
            for (auto& go : stageCtrl->GetObjectArray()) {
                if (Monster* m = go->GetComponent<Monster>()) {
                    m->ActivateVision(kVisionDuration);
                }
            }
        }
    }
}

void Character::NotifyCollision(GameObject& other) {}

SDL_Rect Character::GetInteractionRect(int targetHeightLevel) const {
    int reachDistance = 50; 
    int boxSize = 60; 
    int centerX = associated.box.x + (associated.box.w / 2);
    int footY = associated.box.y + associated.box.h;

    int zOffset = 0;
    if (targetHeightLevel == 1) zOffset = 140;
    else if (targetHeightLevel == 2) zOffset = 260;

    SDL_Rect interactBox = { centerX - (boxSize / 2), (footY - (boxSize / 2)) - zOffset, boxSize, boxSize };

    switch (currentDirection) {
        case Direction::UP:
            interactBox.y -= reachDistance;
            interactBox.h += 20;
            break;
        case Direction::DOWN: interactBox.y += reachDistance; break;
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

    this->currentDirection = leader->currentDirection;
    RefreshAnimSprite();

    float distance = 45.0f;
    this->associated.box.x = leader->associated.box.x + (leader->associated.box.w / 2.0f) - (this->associated.box.w / 2.0f);

    float leaderFootY = leader->associated.box.y + leader->associated.box.h;
    float myFootY = this->associated.box.y + this->associated.box.h;
    this->associated.box.y += (leaderFootY - myFootY);

    switch (leader->currentDirection) {
        case Direction::UP:    this->associated.box.y -= distance; break;
        case Direction::DOWN:  this->associated.box.y += distance; break;
        case Direction::LEFT:  this->associated.box.x -= distance; break;
        case Direction::RIGHT: this->associated.box.x += distance; break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Utilitários Geométricos (Hitboxes e Debug)
// ─────────────────────────────────────────────────────────────────────────────
void Character::Render() {
#ifdef DEBUG
    SDL_Renderer* renderer = Game::GetInstance().GetRenderer();

    SDL_Rect foot = GetFootRect();
    foot.x -= (int)Camera::pos.x;
    foot.y -= (int)Camera::pos.y;
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_RenderDrawRect(renderer, &foot);

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
    while (!taskQueue.empty()) taskQueue.pop();
}

Vec2 Character::GetCenter() {
    return associated.box.Center();
}

float Character::GetFootCircleRadius() const {
    float baseW = (baselineBoxW > 0.0f) ? baselineBoxW : associated.box.w;
    return baseW * 0.25f;
}

Vec2 Character::GetFootCircleCenter() const {
    const float r = GetFootCircleRadius();
    return Vec2(associated.box.x + associated.box.w * 0.5f, associated.box.y + associated.box.h - r);
}

SDL_Rect Character::GetFootRect() const {
    if (Collider* col = associated.GetComponent<Collider>()) {
        const Rect& b = col->box;
        return SDL_Rect{ static_cast<int>(b.x), static_cast<int>(b.y), static_cast<int>(b.w), static_cast<int>(b.h) };
    }
    const float baseW = (baselineBoxW > 0.0f) ? baselineBoxW : associated.box.w;
    const float side = baseW * 0.5f;
    const float centerX = associated.box.x + associated.box.w * 0.5f;
    const float footY   = associated.box.y + associated.box.h;
    return SDL_Rect{ static_cast<int>(centerX - side * 0.5f), static_cast<int>(footY - side), static_cast<int>(side), static_cast<int>(side) };
}

SDL_Rect Character::GetFootCollisionRect() const {
    SDL_Rect r = GetFootRect();
    const int s = kFootCollisionSkinPx; 
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
    const float centerY = footY - baseH * 0.45f; 
    return SDL_Rect{ static_cast<int>(centerX - w * 0.5f), static_cast<int>(centerY - h * 0.5f), static_cast<int>(w), static_cast<int>(h) };
}

void Character::Issue(Command task) {
    taskQueue.push(task);
}

void Character::SetSpeedMultiplier(float multiplier) {
    if (multiplier < 0.1f) {
        speedMultiplier = 0.1f;
        return;
    }
    speedMultiplier = multiplier;
}

void Character::SetBaseSpeed(float speed) {
    if (speed > 0.0f) linearSpeed = speed;
}