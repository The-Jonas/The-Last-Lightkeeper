#include "gameplay/Monster.h"
#include "gameplay/Character.h"
#include "engine/SpriteRenderer.h"
#include "states/stage/StageState.h"
#include "gameplay/Window.h"
#include "engine/Camera.h"
#include "core/Game.h"
#include "core/Resources.h"
#include "math/Vec2.h"

#define INCLUDE_SDL_TTF
#include "SDL_include.h"

#include "nlohmann/json.hpp"
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <string>
#include <fstream>

namespace {
const char* kMonsterFramePaths[] = {
    "Recursos/img/personagens/monstro/monstro_f1.png",
    "Recursos/img/personagens/monstro/monstro_f2.png",
    "Recursos/img/personagens/monstro/monstro_f3.png",
    "Recursos/img/personagens/monstro/monstro_f4.png",
    "Recursos/img/personagens/monstro/monstro_f5.png",
};
}

Monster::Monster(GameObject& associated): Component(associated) {}

Monster::~Monster() {
    GameSfx::StopMonsterFootsteps();
}

void Monster::LoadTuning() {
    std::ifstream f("Recursos/data/monster.json");
    if (!f.is_open()) return;
    
    try {
        nlohmann::json j;
        f >> j;
        auto rd = [&](const char* key, float& out) {
            if (j.contains(key) && j[key].is_number()) out = j[key].get<float>();
        };
        rd("speed_patrol", kSpeedPatrol);
        rd("speed_investigate", kSpeedInvestigate);
        rd("speed_chase", kSpeedChase);
        rd("speed_hunt", kSpeedHunt);
        rd("speed_flee", kSpeedFlee);
        rd("sight_radius", kSightRadius);
        rd("illumination_threshold", kIlluminationThreshold);
        rd("memory_decay_time", kMemoryDecayTime);
        rd("camp_max_time", kCampMaxTime);
        rd("window_radar_interval", kWindowRadarInterval);
        rd("window_radar_range", kWindowRadarRange);
        rd("noise_hear_radius", kNoiseHearRadius);
        rd("noise_cooldown", kNoiseCooldown);
        rd("sabotage_delay", kSabotageDelay);
        rd("post_sabotage_idle", kPostSabotageIdle);
        rd("flee_light_avoid_time", kFleeLightAvoidTime);
        rd("flee_light_avoid_radius", kFleeLightAvoidRadius);
        rd("flee_light_radius_fraction", kFleeLightRadiusFraction);
        rd("chase_grace_duration", kChaseGraceDuration);
        rd("first_chase_grace_duration", kFirstChaseGraceDuration);
        rd("chase_ignore_light_chance", kChaseIgnoreLightChance);
        rd("hunt_ignore_light_chance", kHuntIgnoreLightChance);
        rd("flee_distance", kFleeDistance);
        rd("sanity_damage_dark", kSanityDamageDark);
        rd("sanity_damage_lit", kSanityDamageLit);
        rd("damage_cooldown_time", kDamageCooldownTime);
    } catch (const std::exception&) {
        // Fallback silencioso
    }
}

void Monster::Start() {
    LoadTuning();
    
    for (const char* p : kMonsterFramePaths) {
        Resources::GetImage(p);
    }
    
    SpriteRenderer* sr = new SpriteRenderer(associated, kMonsterFramePaths[0]);
    associated.AddComponent(sr);
    associated.box.y -= associated.box.h;
    TransitionTo(MonsterState::PATROL);
}

void Monster::ApplyAnimFrame() {
    SpriteRenderer* sr = associated.GetComponent<SpriteRenderer>();
    if (!sr) return;
    sr->Open(kMonsterFramePaths[animFrame]);
    sr->SetFrameCount(1, 1);
    sr->SetFrame(0);
}

// ─────────────────────────────────────────────────────────────────────────────
//  UPDATE PRINCIPAL
// ─────────────────────────────────────────────────────────────────────────────
void Monster::Update(float dt) {
    stateTimer += dt;
    pathRefreshTimer += dt;
    windowRadarTimer += dt;
    
    if (postSabotageIdleTimer > 0.0f) postSabotageIdleTimer -= dt;
    if (spotSoundCooldown > 0.0f) spotSoundCooldown -= dt;
    if (huntScreamTimer > 0.0f) huntScreamTimer -= dt;
    if (noiseCooldownTimer > 0.0f) noiseCooldownTimer -= dt;
    if (damageCooldown > 0.0f) damageCooldown -= dt;
    if (visionRevealTimer > 0.0f) visionRevealTimer -= dt;
    if (fleeLightAvoidTimer > 0.0f) fleeLightAvoidTimer -= dt;

    // Debugging Output
    static float dbgTimer = 0.0f;
    dbgTimer += dt;
    if (Game::debugMode && dbgTimer >= 1.0f) {
        dbgTimer = 0.0f;
        const char* stateName = "?";
        switch (state) {
            case MonsterState::PATROL:          stateName = "PATROL";          break;
            case MonsterState::INVESTIGATE:     stateName = "INVESTIGATE";     break;
            case MonsterState::CHASE:           stateName = "CHASE";           break;
            case MonsterState::HUNT:            stateName = "HUNT";            break;
            case MonsterState::FLEE_LIGHT:      stateName = "FLEE_LIGHT";      break;
            case MonsterState::SABOTAGE_WINDOW: stateName = "SABOTAGE_WINDOW"; break;
            case MonsterState::UNSTUCK:         stateName = "UNSTUCK";         break;
        }
        std::cout << "[MONSTER] state=" << stateName
                  << " hasMemory=" << hasMemory
                  << " pos=(" << associated.box.Center().x << "," << associated.box.Center().y << ")\n";
    }

    if (hasMemory) {
        memoryDecayTimer += dt;
        if (memoryDecayTimer >= kMemoryDecayTime) {
            hasMemory = false;
        }
    }

    // 1. Recuperação de colisão (UNSTUCK)
    if (state == MonsterState::UNSTUCK) {
        UpdateUnstuck(dt);
        return;
    }

    // 2. Sensor de Luz
    if (state != MonsterState::FLEE_LIGHT) {
        const bool inLight = IsSelfInLight();
        const bool inGrace = chaseGraceTimer > 0.0f;

        if (!inLight || inGrace) {
            lightDecisionMade = false;
            lightIgnored = false;
        } else if (!lightDecisionMade) {
            lightDecisionMade = true;
            float ignoreChance = 0.0f;
            if (state == MonsterState::CHASE) ignoreChance = kChaseIgnoreLightChance;
            else if (state == MonsterState::HUNT) ignoreChance = kHuntIgnoreLightChance;
            lightIgnored = (static_cast<float>(rand() % 1000) / 1000.0f) < ignoreChance;
        }

        if (inLight && !inGrace && !lightIgnored) {
            TransitionTo(MonsterState::FLEE_LIGHT);
            return;
        }
    }

    if (chaseGraceTimer > 0.0f) chaseGraceTimer -= dt;

    // 3. Sensor de Visão
    Vec2 seenPos;
    if (state != MonsterState::HUNT && state != MonsterState::FLEE_LIGHT && CanSeeLitBrother(seenPos)) {
        lastKnownPlayerPos = seenPos;
        hasMemory = true;
        memoryDecayTimer = 0.0f;
        if (state != MonsterState::CHASE) TransitionTo(MonsterState::CHASE);
    }

    // 4. Modo Estratégico (Acampamento no armário)
    if (AnyBrotherHidden()) {
        if (!strategicMode) {
            campTimer += dt;
            if (campTimer >= kCampMaxTime) {
                strategicMode = true;
                windowRadarTimer = kStrategicRadarInterval;
            }
        }
    } else {
        campTimer = 0.0f;
    }

    // 5. Radar de Janelas
    if ((state == MonsterState::PATROL || state == MonsterState::INVESTIGATE) && !hasMemory && postSabotageIdleTimer <= 0.0f) {
        const float radarInterval = strategicMode ? kStrategicRadarInterval : kWindowRadarInterval;
        if (windowRadarTimer >= radarInterval) {
            windowRadarTimer = 0.0f;
            Window* nearbyWin = FindNearbyClosedWindow();
            if (nearbyWin) {
                targetWindow = nearbyWin;
                TransitionTo(MonsterState::SABOTAGE_WINDOW);
            }
        }
    }

    // 6. Sub-Updates
    switch (state) {
        case MonsterState::PATROL:          UpdatePatrol(dt);          break;
        case MonsterState::INVESTIGATE:     UpdateInvestigate(dt);     break;
        case MonsterState::CHASE:           UpdateChase(dt);           break;
        case MonsterState::HUNT:            UpdateHunt(dt);            break;
        case MonsterState::FLEE_LIGHT:      UpdateFleeLigth(dt);       break;
        case MonsterState::SABOTAGE_WINDOW: UpdateSabotageWindow(dt);  break;
        case MonsterState::UNSTUCK:         break; 
    }

    // 7. Controle de aprisionamento (Stuck)
    const bool movingState = (state == MonsterState::PATROL || state == MonsterState::INVESTIGATE || state == MonsterState::CHASE || state == MonsterState::HUNT);
    if (movingState) {
        if (associated.box.Center().Distance(stuckRefPos) > kStuckMoveEpsilon) {
            stuckRefPos = associated.box.Center();
            stuckTimer = 0.0f;
        } else {
            stuckTimer += dt;
            if (stuckTimer >= kStuckTime) {
                TransitionTo(MonsterState::UNSTUCK);
            }
        }
    } else {
        stuckRefPos = associated.box.Center();
        stuckTimer = 0.0f;
    }

    // 8. Animação e Som de Passos
    Vec2 myPos = associated.box.Center();
    Vec2 plPos = Character::player ? Character::player->GetAssociated().box.Center() : myPos;
    
    float realSpeed = (!currentPath.empty() && pathStep < static_cast<int>(currentPath.size())) ? moveSpeed : 0.0f;
    const bool fleeing = (state == MonsterState::FLEE_LIGHT);

    if (realSpeed > 0.0f) {
        const float pxPerFrame = fleeing ? kAnimPxPerFrame * 0.6f : kAnimPxPerFrame;
        const float stepsMaxDist = 1900.0f + moveSpeed * 4.0f;
        animDistAccum += realSpeed * dt;
        bool changed = false;
        
        while (animDistAccum >= pxPerFrame) {
            animDistAccum -= pxPerFrame;
            animFrame = (animFrame + 1) % kAnimFrameCount;
            changed = true;
            GameSfx::PlayMonsterStep(animFrame, myPos.x, myPos.y, plPos.x, plPos.y, stepsMaxDist);
        }
        if (changed) ApplyAnimFrame();
    }

    GameSfx::UpdateMonsterFootsteps(dt, realSpeed, myPos.x, myPos.y, plPos.x, plPos.y, fleeing);

    const float dx = myPos.x - lastCenterX;
    lastCenterX = myPos.x;
    if (dx < -0.5f) facingLeft = true;
    else if (dx > 0.5f) facingLeft = false;
    
    if (SpriteRenderer* sr = associated.GetComponent<SpriteRenderer>()) {
        sr->SetFlip(facingLeft ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    }

    CheckDamageCollision();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RENDERIZAÇÃO E DEBUG
// ─────────────────────────────────────────────────────────────────────────────
void Monster::Render() {
#ifdef DEBUG
    StageState* dbgStage = Game::TryGetStageState();
    if (!dbgStage || !dbgStage->IsPhysicsDebugOn()) return;

    SDL_Renderer* dbgR = Game::GetInstance().GetRenderer();
    SDL_SetRenderDrawBlendMode(dbgR, SDL_BLENDMODE_BLEND);

    // Hitbox de dano
    SDL_Rect dmgBox = {
        static_cast<int>(associated.box.x + associated.box.w * kDamageBoxInset) - static_cast<int>(Camera::pos.x),
        static_cast<int>(associated.box.y + associated.box.h * kDamageBoxInset) - static_cast<int>(Camera::pos.y),
        static_cast<int>(associated.box.w * kDamageBoxScale),
        static_cast<int>(associated.box.h * kDamageBoxScale)
    };
    SDL_SetRenderDrawColor(dbgR, 255, 0, 0, 100);
    SDL_RenderFillRect(dbgR, &dmgBox);

    // Hitbox de navegação (círculo verde)
    const Vec2 c = associated.box.Center();
    float px = c.x + kNavFootRadius - Camera::pos.x, py = c.y - Camera::pos.y;
    SDL_SetRenderDrawColor(dbgR, 0, 255, 120, 230);
    
    for (int i = 1; i <= 40; ++i) {
        float a = (static_cast<float>(i) / 40) * 2.0f * 3.14159265f;
        float nx = c.x - Camera::pos.x + std::cos(a) * kNavFootRadius;
        float ny = c.y - Camera::pos.y + std::sin(a) * kNavFootRadius;
        SDL_RenderDrawLineF(dbgR, px, py, nx, ny);
        px = nx; py = ny;
    }
 
    // Rótulo de status 
    const char* stName = "?";
    switch (state) {
        case MonsterState::PATROL:          stName = "PATROL"; break;
        case MonsterState::INVESTIGATE:     stName = "INVESTIGATE"; break;
        case MonsterState::CHASE:           stName = "CHASE"; break;
        case MonsterState::HUNT:            stName = "HUNT"; break;
        case MonsterState::FLEE_LIGHT:      stName = "FLEE_LIGHT"; break;
        case MonsterState::SABOTAGE_WINDOW: stName = "SABOTAGE_WINDOW"; break;
        case MonsterState::UNSTUCK:         stName = "UNSTUCK"; break;
    }
    
    std::string label = std::string("STATE: ") + stName;
    if (strategicMode) label += " [STRAT]";
    auto font = Resources::GetFont("Recursos/font/times.ttf", 18);
    
    if (font) {
        SDL_Surface* sf = TTF_RenderUTF8_Blended(font.get(), label.c_str(), {255, 255, 255, 255});
        if (sf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(dbgR, sf);
            SDL_Rect dst{ static_cast<int>(c.x - Camera::pos.x) - sf->w / 2, static_cast<int>(associated.box.y - Camera::pos.y) - sf->h - 8, sf->w, sf->h };
            SDL_RenderCopy(dbgR, tex, nullptr, &dst);
            SDL_FreeSurface(sf);
            SDL_DestroyTexture(tex);
        }
    }

    // Path debug
    if (!currentPath.empty()) {
        SDL_SetRenderDrawColor(dbgR, 0, 255, 0, 200);
        Vec2 prev = associated.box.Center();
        for (size_t i = static_cast<size_t>(pathStep); i < currentPath.size(); i++) {
            SDL_RenderDrawLine(dbgR,
                static_cast<int>(prev.x - Camera::pos.x), static_cast<int>(prev.y - Camera::pos.y),
                static_cast<int>(currentPath[i].x - Camera::pos.x), static_cast<int>(currentPath[i].y - Camera::pos.y));
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
    bool isActualTransition = (state != next);
 
    state = next;
    stateTimer = 0.0f;
    currentPath.clear();
    pathStep = 0;
    stuckTimer = 0.0f;
    stuckRefPos = associated.box.Center();

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
            if (!firstChaseGraceGiven) {
                firstChaseGraceGiven = true;
                chaseGraceTimer = kFirstChaseGraceDuration;
            } else {
                chaseGraceTimer = kChaseGraceDuration;
            }
            chaseNoSightTimer = 0.0f;
            if (isActualTransition && spotSoundCooldown <= 0.0f) {
                GameSfx::PlayMonsterSpot();
                spotSoundCooldown = kSpotSoundCooldown;
            }
            break;

        case MonsterState::HUNT:
            moveSpeed = kSpeedHunt;
            chaseGraceTimer = kChaseGraceDuration;
            if (isActualTransition) {
                GameSfx::PlayMonsterScream();
                huntScreamTimer = kHuntScreamInterval;
            }
            break;

        case MonsterState::UNSTUCK:
            moveSpeed = kSpeedUnstuck;
            GameSfx::StopMonsterFootsteps();
            break;

        case MonsterState::FLEE_LIGHT: {
            moveSpeed = kSpeedFlee * 2.0f;
            GameSfx::StopMonsterFootsteps();

            StageState* stageFlee = Game::TryGetStageState();
            Vec2 myPos = associated.box.Center();
            Vec2 fleeDir(0.0f, 0.0f);
            float closestThreatDist = 1e9f;
            Vec2 threatPos = myPos;

            if (stageFlee) {
                for (const auto& light : stageFlee->GetLights()) {
                    if (!light.enabled) continue;
                    float d = myPos.Distance(light.worldPos);
                    if (d < closestThreatDist) {
                        closestThreatDist = d;
                        fleeDir = myPos - light.worldPos;
                        threatPos = light.worldPos;
                    }
                }
                Vec2 torchPos;
                float torchRadius;
                if (stageFlee->GetActiveTorchWorldPos(torchPos, torchRadius)) {
                    float d = myPos.Distance(torchPos);
                    if (d < closestThreatDist) {
                        closestThreatDist = d;
                        fleeDir = myPos - torchPos;
                        threatPos = torchPos;
                    }
                }
            }

            if (closestThreatDist < 1e8f) {
                fleeLightPos = threatPos;
                fleeLightAvoidTimer = kFleeLightAvoidTime;
            }

            if (fleeDir.Magnitude() > 0.001f) {
                fleeDir = fleeDir.Normalized();
                RequestPath(myPos + fleeDir * kFleeDistance);
            }
            break;
        }

        case MonsterState::SABOTAGE_WINDOW:
            moveSpeed = kSpeedInvestigate;
            pathRefreshTimer = kPathRefreshInterval;
            break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  COLISÃO E SENSORES
// ─────────────────────────────────────────────────────────────────────────────
void Monster::CheckDamageCollision() {
    if (state == MonsterState::UNSTUCK || damageCooldown > 0.0f) return;
    
    StageState* s = Game::TryGetStageState();
    if (s && s->IsMonsterBlindDebug()) return;

    SDL_Rect dmgBox = {
        static_cast<int>(associated.box.x + associated.box.w * kDamageBoxInset),
        static_cast<int>(associated.box.y + associated.box.h * kDamageBoxInset),
        static_cast<int>(associated.box.w * kDamageBoxScale),
        static_cast<int>(associated.box.h * kDamageBoxScale)
    };

    auto CheckPlayerHit = [&](Character* c) -> bool {
        if (!c || c->isHidden || c->currentState == Character::ActionState::INTERACTING) return false;
        SDL_Rect hb = c->GetHitRect();
        return SDL_HasIntersection(&hb, &dmgBox) == SDL_TRUE;
    };

    bool hit = false;
    if (CheckPlayerHit(Character::player)) {
        bool lit = s && s->bigIlluminationLevel >= kIlluminationThreshold;
        Character::player->sanity -= lit ? kSanityDamageLit : kSanityDamageDark;
        if (Character::player->sanity < 0.0f) Character::player->sanity = 0.0f;
        lastKnownPlayerPos = Character::player->GetAssociated().box.Center();
        hit = true;
    }
    else if (CheckPlayerHit(Character::littleBrother)) {
        bool lit = s && s->smallIlluminationLevel >= kIlluminationThreshold;
        Character::littleBrother->sanity -= lit ? kSanityDamageLit : kSanityDamageDark;
        if (Character::littleBrother->sanity < 0.0f) Character::littleBrother->sanity = 0.0f;
        lastKnownPlayerPos = Character::littleBrother->GetAssociated().box.Center();
        hit = true;
    }

    if (hit) {
        damageCooldown = kDamageCooldownTime;
        hasMemory = true;
        memoryDecayTimer = 0.0f;
        if (s) s->TriggerMonsterHitFeedback();
        if (state != MonsterState::HUNT) TransitionTo(MonsterState::HUNT);
    }
}

void Monster::NotifyCollision(GameObject& other) {}

void Monster::NotifyNoise(Vec2 noiseWorldPos) {
    if (noiseCooldownTimer > 0.0f || (state != MonsterState::PATROL && state != MonsterState::INVESTIGATE)) return;
    if (associated.box.Center().Distance(noiseWorldPos) > kNoiseHearRadius) return;

    noiseCooldownTimer = kNoiseCooldown;
    lastKnownPlayerPos = noiseWorldPos;
    hasMemory = true;
    memoryDecayTimer = 0.0f;
    
    if (state != MonsterState::INVESTIGATE) TransitionTo(MonsterState::INVESTIGATE);
    else RequestPath(noiseWorldPos);
}

bool Monster::AnyBrotherHidden() {
    return (Character::player && Character::player->isHidden) ||
           (Character::littleBrother && Character::littleBrother->isHidden);
}

static float DistanceFromRectToPoint(const Rect& rect, const Vec2& point) {
    float closestX = std::max(rect.x, std::min(point.x, rect.x + rect.w));
    float closestY = std::max(rect.y, std::min(point.y, rect.y + rect.h));
    return std::sqrt(std::pow(point.x - closestX, 2) + std::pow(point.y - closestY, 2));
}

bool Monster::CanSeeLitBrother(Vec2& outPos) const {
    StageState* stage = Game::TryGetStageState();
    if (!stage || stage->IsMonsterBlindDebug()) return false;

    auto Check = [&](Character* c, float illumLevel) -> bool {
        if (!c || c->isHidden || illumLevel < kIlluminationThreshold) return false;
        Vec2 charPos = c->GetAssociated().box.Center();
        Vec2 myPos = associated.box.Center();
        if (myPos.Distance(charPos) > kSightRadius) return false;
        if (!stage->HasWalkableLine(myPos, charPos, &associated, kSightLosRadius)) return false;
        outPos = charPos;
        return true;
    };

    if (Check(Character::player, stage->bigIlluminationLevel)) return true;
    if (Check(Character::littleBrother, stage->smallIlluminationLevel)) return true;
    return false;
}

bool Monster::IsWorldPosInAnyLight(Vec2 worldPos, float extraRadius) const {
    StageState* stage = Game::TryGetStageState();
    if (!stage) return false;

    for (const auto& light : stage->GetLights()) {
        if (!light.enabled || light.params.falloffRadiusPx <= 0.0f) continue;
        if (worldPos.Distance(light.worldPos) < (light.params.falloffRadiusPx + extraRadius)) return true;
    }

    Vec2 torchPos;
    float torchRadius = 0.0f;
    if (stage->GetActiveTorchWorldPos(torchPos, torchRadius)) {
        if (worldPos.Distance(torchPos) < (torchRadius + extraRadius)) return true;
    }
    return false;
}

bool Monster::IsSelfInLight() const {
    StageState* stage = Game::TryGetStageState();
    if (!stage) return false;

    Rect myBox = associated.box;
    myBox.x += myBox.w * 0.05f;
    myBox.y += myBox.h * 0.05f;
    myBox.w *= 0.90f;
    myBox.h *= 0.90f;

    for (const auto& light : stage->GetLights()) {
        if (!light.enabled || light.params.falloffRadiusPx <= 0.0f) continue;
        if (DistanceFromRectToPoint(myBox, light.worldPos) < light.params.falloffRadiusPx * kFleeLightRadiusFraction) return true;
    }

    Vec2 torchPos;
    float torchRadius = 0.0f;
    if (stage->GetActiveTorchWorldPos(torchPos, torchRadius)) {
        if (DistanceFromRectToPoint(myBox, torchPos) < torchRadius * kFleeLightRadiusFraction) return true;
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
    pathStep = 0;

    if (currentPath.empty()) {
        if (associated.box.Center().Distance(destination) < 150.0f) {
            currentPath.push_back(destination);
        }
    } else if (currentPath.size() > 1 && associated.box.Center().Distance(currentPath[0]) <= 64.0f) {
        // Evita trepidação ignorando o primeiro nó se já estivermos sobre ele
        pathStep = 1;
    }
}

void Monster::MoveAlongPath(float dt, float speed) {
    if (currentPath.empty() || pathStep >= static_cast<int>(currentPath.size())) return;

    Vec2 next = currentPath[static_cast<size_t>(pathStep)];
    Vec2 dir = next - associated.box.Center();
    float dist = std::sqrt(dir.x * dir.x + dir.y * dir.y);

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
    return !currentPath.empty() && pathStep >= static_cast<int>(currentPath.size());
}

bool Monster::HasNoPath() const {
    return currentPath.empty();
}

// ─────────────────────────────────────────────────────────────────────────────
//  SUB-UPDATES E AÇÕES
// ─────────────────────────────────────────────────────────────────────────────
void Monster::AddPatrolPoint(Vec2 worldPos) {
    patrolPoints.push_back(worldPos);
}

void Monster::PickNextPatrolPoint() {
    if (patrolPoints.empty()) return;
    if (patrolPoints.size() == 1) {
        patrolIndex = 0;
        RequestPath(patrolPoints[0]);
        return;
    }

    const int count = static_cast<int>(patrolPoints.size());

    if (fleeLightAvoidTimer > 0.0f) {
        int best = -1;
        float bestDist = -1.0f;
        for (int tries = 0; tries < count * 2; ++tries) {
            const int cand = rand() % count;
            if (cand == patrolIndex) continue;
            if (patrolPoints[cand].Distance(fleeLightPos) > kFleeLightAvoidRadius) {
                patrolIndex = cand;
                RequestPath(patrolPoints[static_cast<size_t>(patrolIndex)]);
                return;
            }
        }
        for (int i = 0; i < count; ++i) {
            if (i == patrolIndex) continue;
            const float d = patrolPoints[i].Distance(fleeLightPos);
            if (d > bestDist) { bestDist = d; best = i; }
        }
        if (best >= 0) {
            patrolIndex = best;
            RequestPath(patrolPoints[static_cast<size_t>(patrolIndex)]);
            return;
        }
    }

    if (patrolRandom) {
        int next = patrolIndex;
        while (next == patrolIndex) next = rand() % count;
        patrolIndex = next;
    } else {
        patrolIndex = (patrolIndex + 1) % count;
    }

    RequestPath(patrolPoints[static_cast<size_t>(patrolIndex)]);
}

void Monster::UpdatePatrol(float dt) {
    if (postSabotageIdleTimer > 0.0f) {
        currentPath.clear();
        stateTimer = 0.0f;
        return;
    }

    MoveAlongPath(dt, moveSpeed);

    if (HasReachedTarget() || HasNoPath()) {
        if (stateTimer >= 0.5f) {
            PickNextPatrolPoint();
            stateTimer = 0.0f;
        }
    } else {
        stateTimer = 0.0f;
    }
}

void Monster::UpdateInvestigate(float dt) {
    if (!hasMemory) {
        TransitionTo(MonsterState::PATROL);
        return;
    }

    MoveAlongPath(dt, moveSpeed);

    if (HasReachedTarget() || (HasNoPath() && stateTimer >= 5.0f)) {
        hasMemory = false;
        TransitionTo(MonsterState::PATROL);
        return;
    }

    if (pathRefreshTimer >= kPathRefreshInterval && hasMemory) {
        pathRefreshTimer = 0.0f;
        RequestPath(lastKnownPlayerPos);
    }
}

void Monster::UpdateChase(float dt) {
    Vec2 seenPos;
    if (CanSeeLitBrother(seenPos)) {
        lastKnownPlayerPos = seenPos;
        memoryDecayTimer = 0.0f;
        chaseNoSightTimer = 0.0f;

        if (pathRefreshTimer >= kPathRefreshInterval) {
            pathRefreshTimer = 0.0f;
            RequestPath(seenPos);
        }
        MoveAlongPath(dt, moveSpeed);
    } else {
        chaseNoSightTimer += dt;
        MoveAlongPath(dt, moveSpeed);
        if (chaseNoSightTimer >= 2.0f || HasReachedTarget() || HasNoPath()) {
            TransitionTo(MonsterState::INVESTIGATE);
        }
    }
}

void Monster::UpdateHunt(float dt) {
    if (AnyBrotherHidden()) {
        TransitionTo(MonsterState::INVESTIGATE);
        return;
    }

    if (huntScreamTimer <= 0.0f) {
        GameSfx::PlayMonsterScream();
        huntScreamTimer = kHuntScreamInterval;
    }

    if (Character::player) {
        Vec2 playerPos = Character::player->GetAssociated().box.Center();
        if (pathRefreshTimer >= kPathRefreshInterval) {
            pathRefreshTimer = 0.0f;
            RequestPath(playerPos);
            lastKnownPlayerPos = playerPos;
        }
    }
    MoveAlongPath(dt, moveSpeed);

    if (stateTimer >= 8.0f) TransitionTo(MonsterState::INVESTIGATE);
}

void Monster::UpdateUnstuck(float dt) {
    StageState* stage = Game::TryGetStageState();
    Vec2 myPos = associated.box.Center();
    Vec2 away(1.0f, 0.0f);
    
    if (Character::player) {
        away = myPos - Character::player->GetAssociated().box.Center();
    }
    
    float mag = std::sqrt(away.x * away.x + away.y * away.y);
    if (mag < 0.01f) { away = Vec2(1.0f, 0.0f); mag = 1.0f; }
    away.x /= mag; away.y /= mag;

    associated.box.x += away.x * kSpeedUnstuck * dt;
    associated.box.y += away.y * kSpeedUnstuck * dt;
    myPos = associated.box.Center();

    const bool navigableNow = stage && stage->IsWorldPosNavigableFor(myPos, &associated, kNavFootRadius);

    if ((navigableNow && stateTimer >= kUnstuckMinTime) || stateTimer >= kUnstuckMaxTime) {
        if (!navigableNow && !patrolPoints.empty()) {
            int best = 0;
            float bestD = 1e18f;
            for (size_t i = 0; i < patrolPoints.size(); ++i) {
                float d = myPos.Distance(patrolPoints[i]);
                if (d < bestD) { bestD = d; best = static_cast<int>(i); }
            }
            const Vec2 p = patrolPoints[static_cast<size_t>(best)];
            associated.box.x = p.x - associated.box.w / 2.0f;
            associated.box.y = p.y - associated.box.h / 2.0f;
        }
        damageCooldown = kDamageCooldownTime;
        TransitionTo(MonsterState::PATROL);
    }
}

void Monster::UpdateFleeLigth(float dt) {
    if (!currentPath.empty()) MoveAlongPath(dt, moveSpeed);

    if (stateTimer >= 4.0f && hasMemory) {
        TransitionTo(MonsterState::CHASE);
        return;
    }

    if (!IsSelfInLight()) {
        if (stateTimer >= 1.5f) TransitionTo(MonsterState::PATROL);
        return;
    }

    stateTimer = 0.0f;

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
            bool found = false;
            constexpr int kNumAngles = 8;

            for (int i = 0; i < kNumAngles && !found; i++) {
                float angleOffset = (static_cast<float>(i) / kNumAngles) * 2.0f * 3.14159265f;
                float angle = std::atan2(fleeDir.y, fleeDir.x) + angleOffset;
                Vec2 candidate = myPos + Vec2(std::cos(angle), std::sin(angle)) * kFleeDistance;

                if (!IsWorldPosInAnyLight(candidate)) {
                    RequestPath(candidate);
                    found = true;
                }
            }
            if (!found) {
                float angle = std::atan2(fleeDir.y, fleeDir.x);
                RequestPath(myPos + Vec2(std::cos(angle), std::sin(angle)) * kFleeDistance);
            }
        }
    }
}

Window* Monster::FindNearbyClosedWindow() {
    StageState* stage = Game::TryGetStageState();
    if (!stage) return nullptr;

    Window* bestWin = nullptr;
    float closestDist = kWindowRadarRange;
    Vec2 myPos = associated.box.Center();

    for (const auto& goPtr : stage->GetObjectArray()) {
        GameObject* go = goPtr.get();
        if (!go || go->IsDead()) continue;

        Window* win = go->GetComponent<Window>();
        if (!win || win->GetState() != Window::WindowState::CLOSED) continue;

        Vec2 winPos = go->box.Center();
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
    if (!targetWindow || targetWindow->GetState() != Window::WindowState::CLOSED || IsWorldPosInAnyLight(targetWindow->GetAssociated().box.Center())) {
        targetWindow = nullptr;
        TransitionTo(MonsterState::PATROL);
        return;
    }

    if (stateTimer >= 26.0f) {
        windowRadarTimer = strategicMode ? -4.0f : -15.0f; 
        targetWindow = nullptr;
        TransitionTo(MonsterState::PATROL);
        return;
    }

    Vec2 winPos = targetWindow->GetAssociated().box.Center();
    Vec2 myPos = associated.box.Center();
    float dist = myPos.Distance(winPos);

    if (dist <= 480.0f) {
        targetWindow->Toggle();
        targetWindow = nullptr;
        windowRadarTimer = -kSabotageDelay;
        postSabotageIdleTimer = kPostSabotageIdle;
        TransitionTo(MonsterState::PATROL);
        return;
    }

    if (pathRefreshTimer >= kPathRefreshInterval) {
        pathRefreshTimer = 0.0f;
        Vec2 safeTarget; 
        StageState* stage = Game::TryGetStageState();
        bool foundFloor = false;

        if (stage) {
            float bestDistToMonster = 1e9f;
            constexpr float carpetBorder = 440.0f; 

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
        
        if (foundFloor) {
            RequestPath(safeTarget);
        } else {
            windowRadarTimer = strategicMode ? -4.0f : -15.0f; 
            targetWindow = nullptr;
            TransitionTo(MonsterState::PATROL);
            return;
        }
    }

    if (!HasNoPath()) {
        MoveAlongPath(dt, moveSpeed);
    } else if (dist > 540.0f && stateTimer >= 2.0f) {
        windowRadarTimer = strategicMode ? -4.0f : -15.0f; 
        targetWindow = nullptr;
        TransitionTo(MonsterState::PATROL);
    }
}