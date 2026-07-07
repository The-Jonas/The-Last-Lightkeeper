#include "gameplay/Monster.h"
#include "gameplay/Character.h"
#include "engine/SpriteRenderer.h"
#include "states/stage/StageState.h"
#include "gameplay/Window.h"
#include "engine/Camera.h"
#include "core/Game.h"
#include "math/Vec2.h"
#include <cstdlib>
#include <cmath>
#include <iostream>

Monster::Monster(GameObject& associated): Component(associated) {}

Monster::~Monster() {}

void Monster::Start() {
    SpriteRenderer* sr = new SpriteRenderer(associated, "Recursos/img/personagens/monstro/monstro_placeholder.png");
    associated.AddComponent(sr);
    associated.box.y -= associated.box.h;
    TransitionTo(MonsterState::PATROL);
}

// ─────────────────────────────────────────────────────────────────────────────
//  UPDATE PRINCIPAL
// ─────────────────────────────────────────────────────────────────────────────
void Monster::Update(float dt) {
    stateTimer       += dt;
    pathRefreshTimer += dt;
    windowRadarTimer += dt;  // timer separado para radar de janelas

    // DEBUG: imprime estado atual a cada ~1s
    static float dbgTimer = 0.0f;
    dbgTimer += dt;
    if (dbgTimer >= 1.0f) {
        dbgTimer = 0.0f;
        const char* stateName = "?";
        switch (state) {
            case MonsterState::PATROL:          stateName = "PATROL";          break;
            case MonsterState::INVESTIGATE:     stateName = "INVESTIGATE";     break;
            case MonsterState::CHASE:           stateName = "CHASE";           break;
            case MonsterState::HUNT:            stateName = "HUNT";            break;
            case MonsterState::CAMP_CLOSET:     stateName = "CAMP_CLOSET";     break;
            case MonsterState::FLEE_LIGHT:      stateName = "FLEE_LIGHT";      break;
            case MonsterState::LURK:            stateName = "LURK";            break;
            case MonsterState::SABOTAGE_WINDOW: stateName = "SABOTAGE_WINDOW"; break;
        }
        std::cout << "[MONSTER] state=" << stateName
                  << " hasMemory=" << hasMemory
                  << " memoryDecay=" << memoryDecayTimer
                  << " pathSize=" << currentPath.size()
                  << " pathStep=" << pathStep
                  << " stateTimer=" << stateTimer
                  << " windowRadar=" << windowRadarTimer
                  << " pos=(" << associated.box.Center().x << "," << associated.box.Center().y << ")"
                  << "\n";
        if (state == MonsterState::SABOTAGE_WINDOW && targetWindow) {
            Vec2 winPos = targetWindow->GetAssociated().box.Center();
            std::cout << "  -> janela em (" << winPos.x << "," << winPos.y << ")"
                      << " dist=" << associated.box.Center().Distance(winPos)
                      << "\n";
        }
        if (state == MonsterState::PATROL) {
            std::cout << "  -> patrolPoints=" << patrolPoints.size()
                      << " patrolIndex=" << patrolIndex << "\n";
        }
    }

    if (damageCooldown    > 0.0f) damageCooldown    -= dt;
    if (visionRevealTimer > 0.0f) visionRevealTimer -= dt;

    if (hasMemory) {
        memoryDecayTimer += dt;
        if (memoryDecayTimer >= kMemoryDecayTime) {
            hasMemory = false;
        }
    }

    // ── 1. Sensor de luz — prioridade máxima ─────────────────────────────────
    if (state != MonsterState::FLEE_LIGHT && IsSelfInLight()) {
        TransitionTo(MonsterState::FLEE_LIGHT);
        return;
    }

    // ── 2. Sensor de visão ────────────────────────────────────────────────────
    // Bloqueia em HUNT e FLEE_LIGHT (fugindo não enxerga para atacar)
    Vec2 seenPos;
    if (state != MonsterState::HUNT &&
        state != MonsterState::FLEE_LIGHT &&
        CanSeeLitBrother(seenPos)) {
        lastKnownPlayerPos = seenPos;
        hasMemory          = true;
        memoryDecayTimer   = 0.0f;
        if (state != MonsterState::CHASE)
            TransitionTo(MonsterState::CHASE);
    }

    // ── 3. Radar de janelas — timer PRÓPRIO, não interfere com pathfinding ───
    // Só atua quando livre (sem memória dos irmãos) e em estados ociosos
    if (!hasMemory &&
        (state == MonsterState::PATROL || state == MonsterState::INVESTIGATE) &&
        windowRadarTimer >= kWindowRadarInterval) {
        windowRadarTimer = 0.0f;
        Window* nearbyWin = FindNearbyClosedWindow();
        if (nearbyWin) {
            targetWindow = nearbyWin;
            TransitionTo(MonsterState::SABOTAGE_WINDOW);
        }
    }

    

    // ── 4. Despacha para o sub-update ─────────────────────────────────────────
    switch (state) {
        case MonsterState::PATROL:          UpdatePatrol(dt);         break;
        case MonsterState::INVESTIGATE:     UpdateInvestigate(dt);    break;
        case MonsterState::CHASE:           UpdateChase(dt);          break;
        case MonsterState::HUNT:            UpdateHunt(dt);           break;
        case MonsterState::CAMP_CLOSET:     UpdateCampCloset(dt);     break;
        case MonsterState::FLEE_LIGHT:      UpdateFleeLigth(dt);      break;
        case MonsterState::LURK:            UpdateLurk(dt);           break;
        case MonsterState::SABOTAGE_WINDOW: UpdateSabotageWindow(dt); break;
    }

    CheckDamageCollision();
}

void Monster::Render() {
    // Silhueta é desenhada pelo MonsterSilhouette (z=100, acima da escuridão)

#ifdef DEBUG
    SDL_Renderer* dbgR = Game::GetInstance().GetRenderer();
    SDL_SetRenderDrawBlendMode(dbgR, SDL_BLENDMODE_BLEND);
 
    // Caixa de dano — vermelho
    SDL_Rect dmgBox = {
        static_cast<int>(associated.box.x + associated.box.w * 0.1f) - static_cast<int>(Camera::pos.x),
        static_cast<int>(associated.box.y + associated.box.h * 0.1f) - static_cast<int>(Camera::pos.y),
        static_cast<int>(associated.box.w * 0.85f),
        static_cast<int>(associated.box.h * 0.85f)
    };
    SDL_SetRenderDrawColor(dbgR, 255, 0, 0, 100);
    SDL_RenderFillRect(dbgR, &dmgBox);
 
    // Box física — amarelo
    SDL_Rect physBox = {
        static_cast<int>(associated.box.x) - static_cast<int>(Camera::pos.x),
        static_cast<int>(associated.box.y) - static_cast<int>(Camera::pos.y),
        static_cast<int>(associated.box.w),
        static_cast<int>(associated.box.h)
    };
    SDL_SetRenderDrawColor(dbgR, 255, 255, 0, 255);
    SDL_RenderDrawRect(dbgR, &physBox);
 
    // Estado atual — texto via círculo colorido no centro
    {
        Uint8 r = 255, g = 255, b = 255;
        switch (state) {
            case MonsterState::PATROL:          r=100; g=255; b=100; break; // verde
            case MonsterState::INVESTIGATE:     r=255; g=200; b=0;   break; // amarelo
            case MonsterState::CHASE:           r=255; g=100; b=0;   break; // laranja
            case MonsterState::HUNT:            r=255; g=0;   b=0;   break; // vermelho
            case MonsterState::FLEE_LIGHT:      r=0;   g=100; b=255; break; // azul
            case MonsterState::LURK:            r=150; g=0;   b=255; break; // roxo
            case MonsterState::SABOTAGE_WINDOW: r=0;   g=255; b=255; break; // ciano
            case MonsterState::CAMP_CLOSET:     r=255; g=0;   b=150; break; // rosa
        }
        SDL_Rect stateIndicator = {
            static_cast<int>(associated.box.Center().x) - 5 - static_cast<int>(Camera::pos.x),
            static_cast<int>(associated.box.Center().y) - 5 - static_cast<int>(Camera::pos.y),
            10, 10
        };
        SDL_SetRenderDrawColor(dbgR, r, g, b, 255);
        SDL_RenderFillRect(dbgR, &stateIndicator);
    }
 
    // Raio de interação da janela — círculo CIANO
    if (state == MonsterState::SABOTAGE_WINDOW && targetWindow) {
        Vec2 winPos   = targetWindow->GetAssociated().box.Center();
        float winX    = winPos.x - Camera::pos.x;
        float winY    = winPos.y - Camera::pos.y;
        constexpr float kInteractRadius = 540.0f; // Atualizado para 540!
 
        // Desenha o círculo de interação
        SDL_SetRenderDrawColor(dbgR, 0, 255, 255, 180);
        constexpr int kSegs = 32;
        for (int i = 0; i < kSegs; i++) {
            float a0 = (static_cast<float>(i)     / kSegs) * 2.0f * 3.14159265f;
            float a1 = (static_cast<float>(i + 1) / kSegs) * 2.0f * 3.14159265f;
            SDL_RenderDrawLine(dbgR,
                static_cast<int>(winX + std::cos(a0) * kInteractRadius),
                static_cast<int>(winY + std::sin(a0) * kInteractRadius),
                static_cast<int>(winX + std::cos(a1) * kInteractRadius),
                static_cast<int>(winY + std::sin(a1) * kInteractRadius));
        }
 
        // Linha do monstro até a janela
        SDL_SetRenderDrawColor(dbgR, 0, 255, 255, 120);
        SDL_RenderDrawLine(dbgR,
            static_cast<int>(associated.box.Center().x - Camera::pos.x),
            static_cast<int>(associated.box.Center().y - Camera::pos.y),
            static_cast<int>(winX),
            static_cast<int>(winY));
 
        // Ponto exato de navegação escolhido (Quadrado Amarelo)
        Vec2 safeTarget = winPos;
        StageState* stage = Game::TryGetStageState();
        bool foundFloor = false;
        
        if (stage) {
            float bestDistToMonster = 1e9f;
            Vec2 myPos = associated.box.Center();
            constexpr float carpetBorder = 440.0f; // O raio da borda do tapete!
            
            for (int i = 0; i < 8; i++) {
                float angle = i * (3.14159265f / 4.0f);
                Vec2 candidate = winPos + Vec2(std::cos(angle) * carpetBorder, std::sin(angle) * carpetBorder);
                if (stage->IsWorldPosNavigableFor(candidate, &associated, kNavFootRadius)) {
                    float distToMonster = myPos.Distance(candidate);
                    if (distToMonster < bestDistToMonster) {
                        bestDistToMonster = distToMonster;
                        safeTarget = candidate;
                        foundFloor = true;
                    }
                }
            }
        }
        
        // Só desenha o alvo se ele achar uma beirada válida
        if (foundFloor) {
            SDL_SetRenderDrawColor(dbgR, 255, 255, 0, 255);
            int targetX = static_cast<int>(safeTarget.x - Camera::pos.x);
            int targetY = static_cast<int>(safeTarget.y - Camera::pos.y);
            SDL_Rect dot = { targetX - 5, targetY - 5, 10, 10 };
            SDL_RenderFillRect(dbgR, &dot);
        }
    }
 
    // Path atual — linha verde tracejada
    if (!currentPath.empty()) {
        SDL_SetRenderDrawColor(dbgR, 0, 255, 0, 200);
        Vec2 prev = associated.box.Center();
        for (size_t i = static_cast<size_t>(pathStep); i < currentPath.size(); i++) {
            SDL_RenderDrawLine(dbgR,
                static_cast<int>(prev.x - Camera::pos.x),
                static_cast<int>(prev.y - Camera::pos.y),
                static_cast<int>(currentPath[i].x - Camera::pos.x),
                static_cast<int>(currentPath[i].y - Camera::pos.y));
            prev = currentPath[i];
        }
    }
 
    SDL_SetRenderDrawBlendMode(dbgR, SDL_BLENDMODE_NONE);
#endif
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

        case MonsterState::LURK:
            moveSpeed = kSpeedPatrol;
            lurkTimer = 0.0f;
            break;

        case MonsterState::FLEE_LIGHT: {
            moveSpeed = kSpeedFlee;  // foge mais rápido do que persegue

            StageState* stageFlee = Game::TryGetStageState();
            Vec2  myPos           = associated.box.Center();
            Vec2  fleeDir(0.0f, 0.0f);
            float closestThreatDist = 1e9f;

            if (stageFlee) {
                for (const auto& light : stageFlee->GetLights()) {
                    if (!light.enabled) continue;
                    float d = myPos.Distance(light.worldPos);
                    if (d < closestThreatDist) {
                        closestThreatDist = d;
                        fleeDir = myPos - light.worldPos;
                    }
                }
                Vec2  torchPos;
                float torchRadius;
                if (stageFlee->GetActiveTorchWorldPos(torchPos, torchRadius)) {
                    float d = myPos.Distance(torchPos);
                    if (d < closestThreatDist) {
                        closestThreatDist = d;
                        fleeDir = myPos - torchPos;
                    }
                }
            }

            if (fleeDir.Magnitude() > 0.001f) {
                fleeDir = fleeDir.Normalized();
                constexpr float kSafeFleeDistance = 280.0f;
                RequestPath(myPos + fleeDir * kSafeFleeDistance);
            }
            break;
        }

        case MonsterState::SABOTAGE_WINDOW:
            moveSpeed = kSpeedInvestigate;
            pathRefreshTimer = kPathRefreshInterval; // Força o A* a calcular a rota IMEDIATAMENTE!
            break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  COLISÃO DE DANO
// ─────────────────────────────────────────────────────────────────────────────
void Monster::CheckDamageCollision() {
    if (damageCooldown > 0.0f) return;

    SDL_Rect dmgBox = {
        static_cast<int>(associated.box.x + associated.box.w * 0.1f),
        static_cast<int>(associated.box.y + associated.box.h * 0.1f),
        static_cast<int>(associated.box.w * 0.85f),
        static_cast<int>(associated.box.h * 0.85f)
    };

    auto CheckPlayerHit = [&](Character* c) {
        if (!c) return false;
        SDL_Rect pBox = {
            static_cast<int>(c->GetAssociated().box.x),
            static_cast<int>(c->GetAssociated().box.y),
            static_cast<int>(c->GetAssociated().box.w),
            static_cast<int>(c->GetAssociated().box.h)
        };
        return SDL_HasIntersection(&dmgBox, &pBox) == SDL_TRUE;
    };

    bool hit = false;

    if (CheckPlayerHit(Character::player)) {
        Character::player->sanity -= kSanityDamageOnTouch;
        if (Character::player->sanity < 0.0f) Character::player->sanity = 0.0f;
        lastKnownPlayerPos = Character::player->GetAssociated().box.Center();
        hit = true;
        if (Game::debugMode) std::cout << "[Monster] Dano no Irmaozao! Sanidade -= " << kSanityDamageOnTouch << "\n";
    } else if (CheckPlayerHit(Character::littleBrother)) {
        Character::littleBrother->sanity -= kSanityDamageOnTouch;
        if (Character::littleBrother->sanity < 0.0f) Character::littleBrother->sanity = 0.0f;
        lastKnownPlayerPos = Character::littleBrother->GetAssociated().box.Center();
        hit = true;
        if (Game::debugMode) std::cout << "[Monster] Dano no Irmaozinho! Sanidade -= " << kSanityDamageOnTouch << "\n";
    }

    if (hit) {
        damageCooldown   = kDamageCooldownTime;
        hasMemory        = true;
        memoryDecayTimer = 0.0f;
        if (StageState* stage = Game::TryGetStageState()) {
            stage->TriggerMonsterHitFeedback();
        }
        if (state != MonsterState::HUNT)
            TransitionTo(MonsterState::HUNT);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  SUB-UPDATES
// ─────────────────────────────────────────────────────────────────────────────
void Monster::UpdatePatrol(float dt) {
    MoveAlongPath(dt, moveSpeed);
    
    if (HasReachedTarget() || HasNoPath()) {
        if (stateTimer >= 0.5f) {
            PickNextPatrolPoint();
            stateTimer = 0.0f;
        }
    } else {
        stateTimer = 0.0f;
        // Se a rota falhar fisicamente, ele recalcula o caminho a cada 2 segundos pra não ficar travado
        if (pathRefreshTimer >= 2.0f && !patrolPoints.empty()) {
            pathRefreshTimer = 0.0f;
            RequestPath(patrolPoints[patrolIndex]);
        }
    }
}

void Monster::UpdateInvestigate(float dt) {
    if (!hasMemory) {
        TransitionTo(MonsterState::PATROL);
        return;
    }

    MoveAlongPath(dt, moveSpeed);

    if (HasReachedTarget()) {
        // Chegou na última posição conhecida sem encontrar ninguém
        hasMemory = false;
        TransitionTo(MonsterState::PATROL);
        return;
    }

    // se ficou mais de 5s sem conseguir chegar (path vazio ou bloqueado),
    // desiste — evita ficar preso investigando um ponto inalcançável
    if (HasNoPath() && stateTimer >= 5.0f) {
        hasMemory = false;
        TransitionTo(MonsterState::PATROL);
        return;
    }

    // Atualiza o path periodicamente (se falhou, tenta de novo)
    if (pathRefreshTimer >= kPathRefreshInterval && hasMemory) {
        pathRefreshTimer = 0.0f;
        RequestPath(lastKnownPlayerPos);
    }
}

void Monster::UpdateChase(float dt) {
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
        if (HasReachedTarget() || HasNoPath()) {
            TransitionTo(MonsterState::INVESTIGATE);
        }
    }
}

void Monster::UpdateHunt(float dt) {
    // Persegue às cegas — sabe que estão por ali
    if (Character::player) {
        Vec2 playerPos = Character::player->GetAssociated().box.Center();
        if (pathRefreshTimer >= kPathRefreshInterval) {
            pathRefreshTimer   = 0.0f;
            RequestPath(playerPos);
            lastKnownPlayerPos = playerPos;
        }
    }
    MoveAlongPath(dt, moveSpeed);

    // Sai do HUNT após 8s sem tocar — investiga a última posição
    if (stateTimer >= 8.0f) {
        TransitionTo(MonsterState::INVESTIGATE);
    }
}

void Monster::UpdateCampCloset(float dt) {
    campTimer += dt;
    MoveAlongPath(dt, kSpeedInvestigate);

    if (campTimer >= kCampMaxTime) {
        TransitionTo(MonsterState::PATROL);
        return;
    }

    Vec2 seenPos;
    if (CanSeeLitBrother(seenPos)) {
        lastKnownPlayerPos = seenPos;
        TransitionTo(MonsterState::CHASE);
    }
}

void Monster::UpdateFleeLigth(float dt) {
    if (!currentPath.empty()) {
        MoveAlongPath(dt, moveSpeed);
    }

    // 1. Saída da fuga com "Cool-down" de pânico
    if (!IsSelfInLight()) {
        // Recuo curto (0.8s) — volta a rondar/atacar rápido, mais agressivo.
        if (stateTimer >= 0.8f) {
            if (hasMemory) {
                TransitionTo(MonsterState::LURK);
            } else {
                TransitionTo(MonsterState::PATROL);
            }
            return;
        }
    } else {
        // Se a luz encostar nele durante a fuga, o pânico reseta!
        stateTimer = 0.0f; 
    }

    // 2. Recálculo se bateu na parede ou a rota acabou (mas ainda está em pânico)
    if ((HasReachedTarget() || HasNoPath()) && pathRefreshTimer >= kPathRefreshInterval) {
        pathRefreshTimer = 0.0f;
        StageState* stageFlee = Game::TryGetStageState();
        Vec2 myPos = associated.box.Center();
        Vec2 fleeDir(0.0f, 0.0f);
        float closestDist = 1e9f;

        if (stageFlee) {
            for (const auto& light : stageFlee->GetLights()) {
                if (!light.enabled) continue;
                float d = myPos.Distance(light.worldPos);
                if (d < closestDist) { closestDist = d; fleeDir = myPos - light.worldPos; }
            }
            Vec2 torchPos; float torchRadius;
            if (stageFlee->GetActiveTorchWorldPos(torchPos, torchRadius)) {
                float d = myPos.Distance(torchPos);
                if (d < closestDist) { closestDist = d; fleeDir = myPos - torchPos; }
            }
        }

        if (fleeDir.Magnitude() > 0.001f) {
            fleeDir = fleeDir.Normalized();
            
            // ANTI-TRAVAMENTO: Se bateu na parede, tenta escorregar pela lateral
            if (HasNoPath()) {
                float angle = std::atan2(fleeDir.y, fleeDir.x) + 0.6f; 
                fleeDir = Vec2(std::cos(angle), std::sin(angle));
            }
            constexpr float kSafeFleeDistance = 280.0f;
            RequestPath(myPos + fleeDir * kSafeFleeDistance);
        }
    }
}

void Monster::NotifyCollision(GameObject& other) {
    // Colisão física tratada pelo LevelManager.
    // Dano é checado em CheckDamageCollision() dentro do Update().
}

// ─────────────────────────────────────────────────────────────────────────────
//  NOTIFICAÇÃO DO ARMÁRIO
// ─────────────────────────────────────────────────────────────────────────────
void Monster::NotifyClosetOccupied(Vec2 closetWorldPos) {
    if (state == MonsterState::CHASE ||
        state == MonsterState::INVESTIGATE ||
        state == MonsterState::PATROL) {
        closetTargetPos = closetWorldPos;
        TransitionTo(MonsterState::CAMP_CLOSET);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  SENSORES
// ─────────────────────────────────────────────────────────────────────────────
static float DistanceFromRectToPoint(const Rect& rect, const Vec2& point) {
    float closestX = std::max(rect.x, std::min(point.x, rect.x + rect.w));
    float closestY = std::max(rect.y, std::min(point.y, rect.y + rect.h));
    float dx = point.x - closestX;
    float dy = point.y - closestY;
    return std::sqrt(dx * dx + dy * dy);
}

bool Monster::CanSeeLitBrother(Vec2& outPos) const {
    StageState* stage = Game::TryGetStageState();
    if (!stage) return false;

    auto Check = [&](Character* c, float illumLevel) -> bool {
        if (!c) return false;
        if (illumLevel < kIlluminationThreshold) return false;

        Vec2  charPos = c->GetAssociated().box.Center();
        Vec2  myPos   = associated.box.Center();
        float dist    = myPos.Distance(charPos);
        if (dist > kSightRadius) return false;

        // Linha de visão: não enxerga através de paredes
        if (!stage->HasWalkableLine(myPos, charPos, &associated, kSightLosRadius)) {
            return false;
        }

        outPos = charPos;
        return true;
    };

    if (Check(Character::player,        stage->bigIlluminationLevel))   return true;
    if (Check(Character::littleBrother, stage->smallIlluminationLevel)) return true;
    return false;
}

bool Monster::IsWorldPosInAnyLight(Vec2 worldPos, float extraRadius) const {
    StageState* stage = Game::TryGetStageState();
    if (!stage) return false;

    for (const auto& light : stage->GetLights()) {
        if (!light.enabled) continue;
        const float radius = light.params.falloffRadiusPx;
        if (radius <= 0.0f) continue;
        if (worldPos.Distance(light.worldPos) < (radius + extraRadius)) return true;
    }

    Vec2  torchPos;
    float torchRadius = 0.0f;
    if (stage->GetActiveTorchWorldPos(torchPos, torchRadius)) {
        if (worldPos.Distance(torchPos) < (torchRadius + extraRadius)) return true;
    }

    return false;
}

bool Monster::IsSelfInLight() const {
    StageState* stage = Game::TryGetStageState();
    if (!stage) return false;

    // 90% do sprite — ignora bordas transparentes
    Rect myBox = associated.box;
    myBox.x += myBox.w * 0.05f;
    myBox.y += myBox.h * 0.05f;
    myBox.w *= 0.90f;
    myBox.h *= 0.90f;

    // Só o núcleo brilhante afugenta: fora dele (anel escuro) o monstro avança e
    // consegue dar mordidas rápidas antes de recuar — deixa a luz menos "quebrada".
    for (const auto& light : stage->GetLights()) {
        if (!light.enabled) continue;
        const float radius = light.params.falloffRadiusPx * kFleeLightCoreFraction;
        if (radius <= 0.0f) continue;
        if (DistanceFromRectToPoint(myBox, light.worldPos) < radius) return true;
    }

    Vec2  torchPos;
    float torchRadius = 0.0f;
    if (stage->GetActiveTorchWorldPos(torchPos, torchRadius)) {
        if (DistanceFromRectToPoint(myBox, torchPos) < torchRadius * kFleeLightCoreFraction) return true;
    }

    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  PATHFINDING
// ─────────────────────────────────────────────────────────────────────────────
void Monster::RequestPath(Vec2 destination) {
    StageState* stage = Game::TryGetStageState();
    if (!stage) return;

    targetPos = destination;

    if (!stage->IsWorldPosNavigableFor(destination, &associated, kNavFootRadius)) {
        currentPath.clear();
        pathStep = 0;
        return;
    }

    currentPath = stage->FindPathWorld(associated.box.Center(), destination, &associated, 4096, kNavFootRadius);
    pathStep    = 0;

    if (currentPath.empty()) {
        float dist = associated.box.Center().Distance(destination);
        if (dist < 150.0f) {
            // Perto o suficiente — move diretamente sem A*
            currentPath.push_back(destination);
        }
    } else {
        // A MÁGICA ANTI-TREMEDEIRA AQUI:
        // Se o caminho tem mais de um ponto e estamos a menos de 64px (1 tile) do ponto inicial,
        // o monstro ignora o "passo pra trás" e já mira no próximo ponto da rota!
        if (currentPath.size() > 1) {
            float distToFirst = associated.box.Center().Distance(currentPath[0]);
            if (distToFirst <= 64.0f) {
                pathStep = 1;
            }
        }
    }
}

void Monster::MoveAlongPath(float dt, float speed) {
    if (currentPath.empty() || pathStep >= static_cast<int>(currentPath.size())) return;

    Vec2  next     = currentPath[static_cast<size_t>(pathStep)];
    Vec2  myCenter = associated.box.Center();
    Vec2  dir      = next - myCenter;
    float dist     = std::sqrt(dir.x * dir.x + dir.y * dir.y);

    if (dist < 12.0f) {
        pathStep++;
        return;
    }

    dir.x /= dist;
    dir.y /= dist;

    associated.box.x += dir.x * speed * dt;
    associated.box.y += dir.y * speed * dt;
}

bool Monster::HasReachedTarget() const {
    if (currentPath.empty()) return false;
    return pathStep >= static_cast<int>(currentPath.size());
}

bool Monster::HasNoPath() const {
    return currentPath.empty();
}

// ─────────────────────────────────────────────────────────────────────────────
//  PATRULHA
// ─────────────────────────────────────────────────────────────────────────────
void Monster::AddPatrolPoint(Vec2 worldPos) {
    patrolPoints.push_back(worldPos);
}

void Monster::PickNextPatrolPoint() {
    if (patrolPoints.empty()) {
        std::cout << "[PATROL] Sem waypoints!\n";
        return;
    }

    if (patrolPoints.size() == 1) {
        patrolIndex = 0;
        RequestPath(patrolPoints[0]);
        return;
    }

    if (patrolRandom) {
        int next = patrolIndex;
        while (next == patrolIndex)
            next = rand() % static_cast<int>(patrolPoints.size());
        patrolIndex = next;
    } else {
        patrolIndex = (patrolIndex + 1) % static_cast<int>(patrolPoints.size());
    }

    std::cout << "[PATROL] Indo para waypoint " << patrolIndex
              << " (" << patrolPoints[patrolIndex].x << ","
              << patrolPoints[patrolIndex].y << ")\n";
    RequestPath(patrolPoints[static_cast<size_t>(patrolIndex)]);
}

// ─────────────────────────────────────────────────────────────────────────────
//  JANELAS (SABOTAGEM)
// ─────────────────────────────────────────────────────────────────────────────
Window* Monster::FindNearbyClosedWindow() {
    StageState* stage = Game::TryGetStageState();
    if (!stage) return nullptr;

    Window* bestWin = nullptr;
    float closestDist = 1500.0f; // Radar bem longo
    Vec2 myPos = associated.box.Center();

    for (const auto& goPtr : stage->GetObjectArray()) {
        GameObject* go = goPtr.get();
        if (!go || go->IsDead()) continue;

        Window* win = go->GetComponent<Window>();
        if (!win || win->GetState() != Window::WindowState::CLOSED) continue;

        Vec2 winPos = go->box.Center();
        
        // Se tem luz na janela, ele ignora
        if (IsWorldPosInAnyLight(winPos)) continue;

        float dist = myPos.Distance(winPos);
        if (dist < closestDist) {
            closestDist = dist;
            bestWin = win;
        }
    }
    return bestWin;
}

void Monster::UpdateSabotageWindow(float dt) {
    // 1. Condições de cancelamento simples
    if (!targetWindow ||
        targetWindow->GetState() != Window::WindowState::CLOSED ||
        IsWorldPosInAnyLight(targetWindow->GetAssociated().box.Center())) {
        
        targetWindow = nullptr;
        TransitionTo(MonsterState::PATROL);
        return;
    }

    // 2. TIMEOUT ABSOLUTO: Se demorar demais, pune o radar e desiste!
    if (stateTimer >= 10.0f) {
        if (Game::debugMode) std::cout << "[MONSTER] Preso em obstaculo. Desistindo da janela por 15s!\n";
        windowRadarTimer = -15.0f; 
        targetWindow = nullptr;
        TransitionTo(MonsterState::PATROL);
        return;
    }

    Vec2 winPos = targetWindow->GetAssociated().box.Center();
    Vec2 myPos = associated.box.Center();
    float dist = myPos.Distance(winPos);

    // 3. O RAIO DE INTERAÇÃO (540 pixels)
    if (dist <= 480.0f) {
        if (Game::debugMode) std::cout << "[MONSTER] Abriu a janela a " << dist << "px de distancia!\n";
        targetWindow->Toggle();
        targetWindow = nullptr;
        TransitionTo(MonsterState::PATROL);
        return;
    }

    // 4. ATUALIZAÇÃO DA ROTA (Testando apenas a Borda do Tapete)
    if (pathRefreshTimer >= kPathRefreshInterval) {
        pathRefreshTimer = 0.0f;
        
        Vec2 safeTarget; 
        StageState* stage = Game::TryGetStageState();
        bool foundFloor = false;

        if (stage) {
            float bestDistToMonster = 1e9f;
            // Busca EXATAMENTE na borda do tapete (500px, pra dar 40px de margem de folga pro acionamento)
            constexpr float carpetBorder = 440.0f; 

            // Testa APENAS os 8 pontos da borda extrema
            for (int i = 0; i < 8; i++) {
                float angle = i * (3.14159265f / 4.0f);
                Vec2 candidate = winPos + Vec2(std::cos(angle) * carpetBorder, std::sin(angle) * carpetBorder);
                
                // O monstro pode pisar nesse ponto da borda?
                if (stage->IsWorldPosNavigableFor(candidate, &associated, kNavFootRadius)) {
                    // É o ponto da borda mais perto do monstro? (Garante que ele não mire através da parede)
                    float distToMonster = myPos.Distance(candidate);
                    if (distToMonster < bestDistToMonster) {
                        bestDistToMonster = distToMonster;
                        safeTarget = candidate;
                        foundFloor = true;
                    }
                }
            }
        }
        
        // Se achou um ponto válido na borda, pede o caminho pra lá! Se não, pune o radar.
        if (foundFloor) {
            RequestPath(safeTarget);
        } else {
            if (Game::debugMode) std::cout << "[MONSTER] Borda do tapete totalmente bloqueada. Desistindo!\n";
            windowRadarTimer = -15.0f; 
            targetWindow = nullptr;
            TransitionTo(MonsterState::PATROL);
            return;
        }
    }

    // 5. MOVIMENTAÇÃO TOTALMENTE LIMPA
    if (!HasNoPath()) {
        MoveAlongPath(dt, moveSpeed);
    } else if (dist > 540.0f && stateTimer >= 2.0f) {
        // Se o A* falhar por completo mesmo pro alvo da borda, pune o radar e sai
        if (Game::debugMode) std::cout << "[MONSTER] Caminho impossível. Desistindo da janela por 15s!\n";
        windowRadarTimer = -15.0f; 
        targetWindow = nullptr;
        TransitionTo(MonsterState::PATROL);
    }
}

void Monster::UpdateLurk(float dt) {
    lurkTimer += dt;

    if (lurkTimer >= kLurkMaxTime) {
        hasMemory = false;
        TransitionTo(MonsterState::PATROL);
        return;
    }

    // Orbita ao redor da última posição conhecida
    if (pathRefreshTimer >= kLurkRepositionInterval) {
        pathRefreshTimer = 0.0f;

        constexpr float kLurkOrbitRadius = 220.0f;
        float angle = static_cast<float>(rand() % 360) * (3.14159265f / 180.0f);
        Vec2  orbitOffset(std::cos(angle) * kLurkOrbitRadius, std::sin(angle) * kLurkOrbitRadius);
        Vec2  lurkTarget = lastKnownPlayerPos + orbitOffset;

        if (!IsWorldPosInAnyLight(lurkTarget)) {
            RequestPath(lurkTarget);
        }
    }

    MoveAlongPath(dt, moveSpeed);

    // Oportunidade: viu os irmãos iluminados de novo — persegue
    Vec2 seenPos;
    if (CanSeeLitBrother(seenPos)) {
        lastKnownPlayerPos = seenPos;
        TransitionTo(MonsterState::CHASE);
        return;
    }

    // Memória expirou — desiste e patrulha
    if (!hasMemory) {
        TransitionTo(MonsterState::PATROL);
    }
}