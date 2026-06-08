#include "gameplay/Monster.h"
#include "gameplay/Character.h"
#include "engine/SpriteRenderer.h"
#include "states/stage/StageState.h"
#include "core/Game.h"
#include "math/Vec2.h"
#include <cstdlib>
#include <cmath>
#include <iostream>

Monster::Monster(GameObject& associated): Component(associated) {}

Monster::~Monster() {}

void Monster::Start() {
    // Sprite placeholder — troca quando tiver o asset final
    SpriteRenderer* sr = new SpriteRenderer(associated, "Recursos/img/personagens/monstro/monstro_placeholder.png");
    associated.AddComponent(sr);
    associated.box.y -= associated.box.h;
 
    // Começa patrulhando
    state = MonsterState::PATROL;
    PickNextPatrolPoint();
}

// ─────────────────────────────────────────────────────────────────────────────
//  UPDATE PRINCIPAL
// ─────────────────────────────────────────────────────────────────────────────
void Monster::Update(float dt) {
    stateTimer += dt;
    pathRefreshTimer += dt;

    if (damageCooldown > 0.0f) damageCooldown -= dt;
    if (visionRevealTimer > 0.0f) visionRevealTimer -= dt;
    if (hasMemory) {
        memoryDecayTimer += dt;
        if (memoryDecayTimer >= kMemoryDecayTime) {
            hasMemory = false;                          // Esqueceu - volta a patrulhar
        }
    }

    // ── Sensor de luz na própria posição ─────────────────────────────────────
    // Se entrou em área iluminada demais, foge (Exceto se estiver em HUNT)
    // nesse estado ele está tão alterado que ignora a luz por um tempo
    if (state != MonsterState::HUNT && state != MonsterState::FLEE_LIGHT && IsSelfInLight()) {
        TransitionTo(MonsterState::FLEE_LIGHT);
        return;
    }

    // ── Sensor de visão ───────────────────────────────────────────────────────
    Vec2 seenPos;
    if (state != MonsterState::HUNT && CanSeeLitBrother(seenPos)) {
        lastKnownPlayerPos = seenPos;
        hasMemory          = true;
        memoryDecayTimer   = 0.0f;
        visionRevealTimer  = 0.3f;  // Irmãozinho pode ver a silhueta por 0.3s
 
        if (state != MonsterState::CHASE)
            TransitionTo(MonsterState::CHASE);
    }

    // ── Despacha para o sub-update do estado atual ────────────────────────────
    switch (state) {
        case MonsterState::PATROL:       UpdatePatrol(dt);      break;
        case MonsterState::INVESTIGATE:  UpdateInvestigate(dt); break;
        case MonsterState::CHASE:        UpdateChase(dt);       break;
        case MonsterState::HUNT:         UpdateHunt(dt);        break;
        case MonsterState::CAMP_CLOSET:  UpdateCampCloset(dt);  break;
        case MonsterState::FLEE_LIGHT:   UpdateFleeLigth(dt);   break;
    }
}

void Monster::Render() {
    // Silhueta é desenhada pelo MonsterSilhouette (z=100, acima da escuridão)
    // Nada aqui.
}

// ─────────────────────────────────────────────────────────────────────────────
//  TRANSIÇÃO DE ESTADO
// ─────────────────────────────────────────────────────────────────────────────
void Monster::TransitionTo(MonsterState next) {
    state      = next;
    stateTimer = 0.0f;
    currentPath.clear();
    pathStep = 0;
 
    switch (next) {
        case MonsterState::PATROL:
            moveSpeed = kSpeedPatrol;
            PickNextPatrolPoint();
            break;
        case MonsterState::INVESTIGATE:
            moveSpeed = kSpeedInvestigate;
            RequestPath(lastKnownPlayerPos);
            break;
        case MonsterState::CHASE:
            moveSpeed = kSpeedChase;
            break;
        case MonsterState::HUNT:
            moveSpeed = kSpeedHunt;
            break;
        case MonsterState::CAMP_CLOSET:
            moveSpeed = kSpeedInvestigate;
            campTimer = 0.0f;
            RequestPath(closetTargetPos);
            break;
        case MonsterState::FLEE_LIGHT:
            moveSpeed = kSpeedChase;  // Foge rápido
            // Tenta se mover para um waypoint próximo que esteja no escuro
            if (!patrolPoints.empty())
                RequestPath(patrolPoints[patrolIndex]);
            break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  SUB-UPDATES
// ─────────────────────────────────────────────────────────────────────────────
void Monster::UpdatePatrol(float dt) {
    MoveAlongPath(dt, moveSpeed);
 
    if (HasReachedTarget()) {
        PickNextPatrolPoint();
        // Pequena pausa aleatória antes do próximo waypoint (0.5 a 2s)
        // Implementada zerando o path e esperando stateTimer resetar
        // Para simplificar, apenas pega o próximo ponto imediatamente
    }
 
    // Se tem memória de onde viu luz, vai investigar
    if (hasMemory) {
        TransitionTo(MonsterState::INVESTIGATE);
    }
}

void Monster::UpdateInvestigate(float dt) {
    MoveAlongPath(dt, moveSpeed);
 
    if (HasReachedTarget()) {
        // Chegou na última posição conhecida — não achou ninguém, volta a patrulhar
        hasMemory = false;
        TransitionTo(MonsterState::PATROL);
    }
 
    // Atualiza o path se a memória ainda está fresca
    if (pathRefreshTimer >= kPathRefreshInterval && hasMemory) {
        pathRefreshTimer = 0.0f;
        RequestPath(lastKnownPlayerPos);
    }
}

void Monster::UpdateChase(float dt) {
    // Perseguição: atualiza o path para a posição atual dos irmãos
    Vec2 seenPos;
    if (CanSeeLitBrother(seenPos)) {
        lastKnownPlayerPos = seenPos;
        memoryDecayTimer   = 0.0f;
 
        if (pathRefreshTimer >= kPathRefreshInterval) {
            pathRefreshTimer = 0.0f;
            RequestPath(seenPos);
        }
        MoveAlongPath(dt, moveSpeed);
    } else {
        // Perdeu a visão — vai até onde viu pela última vez
        MoveAlongPath(dt, moveSpeed);
        if (HasReachedTarget()) {
            // Chegou no último ponto e não achou — investiga por perto
            TransitionTo(MonsterState::INVESTIGATE);
        }
    }
}

void Monster::UpdateHunt(float dt) {
    // HUNT: tocou nos irmãos no escuro — persegue às cegas
    // Vai se mover para a última posição conhecida e fazer buscas em espiral
    if (Character::player) {
        Vec2 playerPos = Character::player->GetAssociated().box.Center();
        if (pathRefreshTimer >= kPathRefreshInterval) {
            pathRefreshTimer = 0.0f;
            RequestPath(playerPos);
            lastKnownPlayerPos = playerPos;
        }
    }
    MoveAlongPath(dt, moveSpeed);
 
    // Sai do HUNT após 8 segundos sem tocar — volta a investigar a última posição
    if (stateTimer >= 8.0f) {
        TransitionTo(MonsterState::INVESTIGATE);
    }
}

void Monster::UpdateCampCloset(float dt) {
    campTimer += dt;
    MoveAlongPath(dt, kSpeedInvestigate);
 
    // Depois de kCampMaxTime segundos desiste e volta a patrulhar
    if (campTimer >= kCampMaxTime) {
        TransitionTo(MonsterState::PATROL);
        return;
    }
 
    // Se os irmãos sairem do armário iluminados, entra no Chase
    Vec2 seenPos;
    if (CanSeeLitBrother(seenPos)) {
        lastKnownPlayerPos = seenPos;
        TransitionTo(MonsterState::CHASE);
    }
}

void Monster::UpdateFleeLigth(float dt) {
    MoveAlongPath(dt, moveSpeed);
 
    // Chegou num tile escuro — sai do FLEE
    if (!IsSelfInLight() || HasReachedTarget()) {
        if (hasMemory)
            TransitionTo(MonsterState::INVESTIGATE);
        else
            TransitionTo(MonsterState::PATROL);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  COLISÃO — Dano e HUNT
// ─────────────────────────────────────────────────────────────────────────────
void Monster::NotifyCollision(GameObject& other) {
    if (damageCooldown > 0.0f) return;
 
    auto ApplyDamage = [&](Character* c) {
        if (!c) return;
        c->sanity -= kSanityDamageOnTouch;
        if (c->sanity < 0.0f) c->sanity = 0.0f;
        std::cout << "[Monster] Tocou num irmao! Sanidade -= " << kSanityDamageOnTouch << "\n";
    };
 
    bool hit = false;
    if (Character::player && &other == &Character::player->GetAssociated()) {
        ApplyDamage(Character::player);
        lastKnownPlayerPos = Character::player->GetAssociated().box.Center();
        hit = true;
    }
    if (Character::littleBrother && &other == &Character::littleBrother->GetAssociated()) {
        ApplyDamage(Character::littleBrother);
        lastKnownPlayerPos = Character::littleBrother->GetAssociated().box.Center();
        hit = true;
    }
 
    if (hit) {
        damageCooldown = kDamageCooldownTime;
        hasMemory      = true;
        memoryDecayTimer = 0.0f;
 
        // Toque no escuro → HUNT (mais agressivo)
        if (state != MonsterState::HUNT)
            TransitionTo(MonsterState::HUNT);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  NOTIFICAÇÃO DO ARMÁRIO
// ─────────────────────────────────────────────────────────────────────────────
void Monster::NotifyClosetOccupied(Vec2 closetWorldPos) {
    // Só vai de campana se já estava CHASE ou INVESTIGATE
    if (state == MonsterState::CHASE || state == MonsterState::INVESTIGATE || state == MonsterState::PATROL) {
        closetTargetPos = closetWorldPos;
        TransitionTo(MonsterState::CAMP_CLOSET);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  SENSORES
// ─────────────────────────────────────────────────────────────────────────────
bool Monster::CanSeeLitBrother(Vec2& outPos) const {
    StageState* stage = Game::TryGetStageState();
    if (!stage) return false;
 
    auto Check = [&](Character* c, float illumLevel) -> bool {
        if (!c) return false;
        if (illumLevel < kIlluminationThreshold) return false;  // Não está iluminado
 
        Vec2 charPos = c->GetAssociated().box.Center();
        Vec2 myPos   = associated.box.Center();
        float dist   = myPos.Distance(charPos);
        if (dist > kSightRadius) return false;
 
        // Está iluminado e dentro do raio — monstro enxerga
        outPos = charPos;
        return true;
    };
 
    if (Check(Character::player,       stage->bigIlluminationLevel))   return true;
    if (Check(Character::littleBrother, stage->smallIlluminationLevel)) return true;
    return false;
}
 
bool Monster::IsPosInLight(Vec2 worldPos) const {
    StageState* stage = Game::TryGetStageState();
    if (!stage) return false;
 
    // Checa se a posição está próxima de qualquer irmão iluminado
    auto NearLit = [&](Character* c, float illumLevel) -> bool {
        if (!c || illumLevel < kIlluminationThreshold) return false;
        return worldPos.Distance(c->GetAssociated().box.Center()) < 160.0f;
    };
 
    if (NearLit(Character::player,        stage->bigIlluminationLevel))   return true;
    if (NearLit(Character::littleBrother, stage->smallIlluminationLevel)) return true;
    return false;
}
 
bool Monster::IsSelfInLight() const {
    return IsPosInLight(associated.box.Center());
}

// ─────────────────────────────────────────────────────────────────────────────
//  PATHFINDING
// ─────────────────────────────────────────────────────────────────────────────
void Monster::RequestPath(Vec2 destination) {
    StageState* stage = Game::TryGetStageState();
    if (!stage) return;
 
    targetPos   = destination;
    currentPath = stage->FindPathWorld(associated.box.Center(), destination, &associated);
    pathStep    = 0;
}
 
void Monster::MoveAlongPath(float dt, float speed) {
    if (currentPath.empty() || pathStep >= static_cast<int>(currentPath.size())) return;
 
    Vec2 next      = currentPath[static_cast<size_t>(pathStep)];
    Vec2 myCenter  = associated.box.Center();
    Vec2 dir       = next - myCenter;
    float dist     = std::sqrt(dir.x * dir.x + dir.y * dir.y);
 
    if (dist < 6.0f) {
        pathStep++;
        return;
    }
 
    dir.x /= dist;
    dir.y /= dist;
 
    associated.box.x += dir.x * speed * dt;
    associated.box.y += dir.y * speed * dt;
}
 
bool Monster::HasReachedTarget() const {
    if (currentPath.empty()) return true;
    return pathStep >= static_cast<int>(currentPath.size());
}

// ─────────────────────────────────────────────────────────────────────────────
//  PATRULHA
// ─────────────────────────────────────────────────────────────────────────────
void Monster::AddPatrolPoint(Vec2 worldPos) {
    patrolPoints.push_back(worldPos);
}
 
void Monster::PickNextPatrolPoint() {
    if (patrolPoints.empty()) return;
 
    if (patrolRandom) {
        // Escolhe aleatoriamente, evitando repetir o mesmo ponto
        int next = patrolIndex;
        if (patrolPoints.size() > 1) {
            while (next == patrolIndex)
                next = rand() % static_cast<int>(patrolPoints.size());
        }
        patrolIndex = next;
    } else {
        patrolIndex = (patrolIndex + 1) % static_cast<int>(patrolPoints.size());
    }
 
    RequestPath(patrolPoints[static_cast<size_t>(patrolIndex)]);
}