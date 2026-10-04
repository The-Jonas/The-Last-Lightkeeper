#include "gameplay/Monster.h"
#include "audio/GameSfx.h"
#include "core/Game.h"
#include "core/Resources.h"
#include "core/Telemetry.h"
#include "engine/Camera.h"
#include "engine/SpriteRenderer.h"
#include "gameplay/Character.h"
#include "gameplay/StairTrigger.h"
#include "gameplay/Window.h"
#include "states/stage/StageState.h"

#define INCLUDE_SDL_TTF
#include "SDL_include.h"

#include "nlohmann/json.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace {

constexpr float kPi = 3.14159265f;
const char* kTuningPath = "config/monster.json";

const char* kMonsterFramePaths[] = {
    "Recursos/img/personagens/monstro/monstro_f1.png",
    "Recursos/img/personagens/monstro/monstro_f2.png",
    "Recursos/img/personagens/monstro/monstro_f3.png",
    "Recursos/img/personagens/monstro/monstro_f4.png",
    "Recursos/img/personagens/monstro/monstro_f5.png",
};

// Desconta dt de um timer que para em zero.
void TickDown(float& timer, float dt) {
    if (timer > 0.0f) timer -= dt;
}

// Menor distância de um ponto a um retângulo (0 se estiver dentro).
float DistanceFromRectToPoint(const Rect& rect, const Vec2& point) {
    const float cx = std::max(rect.x, std::min(point.x, rect.x + rect.w));
    const float cy = std::max(rect.y, std::min(point.y, rect.y + rect.h));
    return std::hypot(point.x - cx, point.y - cy);
}

// Sorteio 0..1 com três casas (o mesmo do código antigo).
float Random01() {
    return static_cast<float>(std::rand() % 1000) / 1000.0f;
}

}  // namespace

// ═════════════════════════════════════════════════════════════════════════════
//  Ciclo de vida
// ═════════════════════════════════════════════════════════════════════════════

Monster::Monster(GameObject& associated) : Component(associated) {}

// Cala os passos, grava a última oscilação pendente e sai do modo "intenso" da telemetria.
Monster::~Monster() {
    GameSfx::StopMonsterFootsteps();
    FlushStateFlap();
    Telemetry::SetIntense(false);
}

// Carrega os números, os quadros e o sprite; põe a caixa com os pés no ponto de spawn.
void Monster::Start() {
    LoadTuning();
    for (const char* p : kMonsterFramePaths) {
        Resources::GetImage(p);
    }
    associated.AddComponent(new SpriteRenderer(associated, kMonsterFramePaths[0]));
    associated.box.y -= associated.box.h;
    TransitionTo(MonsterState::PATROL);
}

// Lê config/monster.json por cima dos padrões de Tuning. Chave ausente mantém o padrão.
void Monster::LoadTuning() {
    std::ifstream f(kTuningPath);
    if (!f.is_open()) return;

    struct Entry { const char* key; float Tuning::* field; };
    static const Entry kEntries[] = {
        {"speed_patrol",               &Tuning::speedPatrol},
        {"speed_investigate",          &Tuning::speedInvestigate},
        {"speed_chase",                &Tuning::speedChase},
        {"speed_hunt",                 &Tuning::speedHunt},
        {"speed_flee",                 &Tuning::speedFlee},
        {"sight_radius",               &Tuning::sightRadius},
        {"illumination_threshold",     &Tuning::illuminationThreshold},
        {"memory_decay_time",          &Tuning::memoryDecayTime},
        {"camp_max_time",              &Tuning::campMaxTime},
        {"strategic_radar_interval",   &Tuning::strategicRadarInterval},
        {"strategic_sabotage_rest",    &Tuning::strategicSabotageRest},
        {"bored_time",                 &Tuning::boredTime},
        {"bored_radius",               &Tuning::boredRadius},
        {"bored_avoid_time",           &Tuning::boredAvoidTime},
        {"window_radar_interval",      &Tuning::windowRadarInterval},
        {"window_radar_range",         &Tuning::windowRadarRange},
        {"sabotage_delay",             &Tuning::sabotageDelay},
        {"post_sabotage_idle",         &Tuning::postSabotageIdle},
        {"noise_hear_radius",          &Tuning::noiseHearRadius},
        {"noise_cooldown",             &Tuning::noiseCooldown},
        {"flee_distance",              &Tuning::fleeDistance},
        {"flee_light_avoid_time",      &Tuning::fleeLightAvoidTime},
        {"flee_light_avoid_radius",    &Tuning::fleeLightAvoidRadius},
        {"flee_light_radius_fraction", &Tuning::fleeLightRadiusFraction},
        {"chase_grace_duration",       &Tuning::chaseGraceDuration},
        {"first_chase_grace_duration", &Tuning::firstChaseGraceDuration},
        {"chase_ignore_light_chance",  &Tuning::chaseIgnoreLightChance},
        {"hunt_ignore_light_chance",   &Tuning::huntIgnoreLightChance},
        {"sanity_damage_dark",         &Tuning::sanityDamageDark},
        {"sanity_damage_lit",          &Tuning::sanityDamageLit},
        {"damage_cooldown_time",       &Tuning::damageCooldownTime},
        {"vocal_cooldown",             &Tuning::vocalCooldown},
    };

    try {
        nlohmann::json j;
        f >> j;
        for (const Entry& e : kEntries) {
            if (j.contains(e.key) && j[e.key].is_number()) {
                tuning.*(e.field) = j[e.key].get<float>();
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << kTuningPath << " ignorado (parse): " << ex.what() << std::endl;
    }
}

// Troca o sprite para o quadro atual da caminhada.
void Monster::ApplyAnimFrame() {
    SpriteRenderer* sr = associated.GetComponent<SpriteRenderer>();
    if (!sr) return;
    sr->Open(kMonsterFramePaths[animFrame]);
    sr->SetFrameCount(1, 1);
    sr->SetFrame(0);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Update
// ═════════════════════════════════════════════════════════════════════════════

// Ordem: timers → preso? → luz → visão → cerco/tédio → janelas → estado atual →
// detecção de preso → passos/eco → dano.
void Monster::Update(float dt) {
    TickTimers(dt);
    UpdateStairState(dt);
    LogDebugState(dt);

    if (state == MonsterState::UNSTUCK) {
        UpdateUnstuck(dt);
        return;
    }
    if (UpdateLightSensor()) {
        return;
    }
    TickDown(chaseGraceTimer, dt);

    const bool sawBrother = UpdateSightSensor();
    UpdateCampMode(dt, sawBrother);
    UpdateBoredom(dt, sawBrother);
    UpdateWindowRadar();

    switch (state) {
        case MonsterState::PATROL:          UpdatePatrol(dt);         break;
        case MonsterState::INVESTIGATE:     UpdateInvestigate(dt);    break;
        case MonsterState::CHASE:           UpdateChase(dt);          break;
        case MonsterState::HUNT:            UpdateHunt(dt);           break;
        case MonsterState::FLEE_LIGHT:      UpdateFleeLight(dt);      break;
        case MonsterState::SABOTAGE_WINDOW: UpdateSabotageWindow(dt); break;
        case MonsterState::UNSTUCK:         break;
    }

    UpdateStuckDetection(dt);
    UpdateFootstepsAndEcho(dt);
    CheckDamageCollision();
}

// Avança os timers do frame e esquece a última posição quando a memória vence.
void Monster::TickTimers(float dt) {
    stateTimer       += dt;
    pathRefreshTimer += dt;
    windowRadarTimer += dt;

    TickDown(postSabotageIdleTimer, dt);
    TickDown(vocalCooldown, dt);
    TickDown(noiseCooldownTimer, dt);
    TickDown(damageCooldown, dt);
    TickDown(visionRevealTimer, dt);
    TickDown(echoRevealTimer, dt);
    TickDown(fleeLightAvoidTimer, dt);

    if (hasMemory) {
        memoryDecayTimer += dt;
        if (memoryDecayTimer >= tuning.memoryDecayTime) hasMemory = false;
    }
}

// Debug: estado, memória e posição no console uma vez por segundo.
void Monster::LogDebugState(float dt) {
    if (!Game::debugMode) return;
    debugLogTimer += dt;
    if (debugLogTimer < 1.0f) return;
    debugLogTimer = 0.0f;
    const Vec2 c = associated.box.Center();
    std::cout << "[MONSTER] state=" << StateName(state) << " hasMemory=" << hasMemory
              << " elevated=" << isElevated << " pos=(" << c.x << "," << c.y << ")\n";
}

// Na luz (fora da carência de perseguição): foge — a menos que, perseguindo ou
// caçando, tenha sorteado ignorar ESTA luz (sorteio uma vez por entrada na luz).
bool Monster::UpdateLightSensor() {
    if (state == MonsterState::FLEE_LIGHT) return false;

    const bool inLight = IsSelfInLight();
    const bool inGrace = chaseGraceTimer > 0.0f;
    if (!inLight || inGrace) {
        lightDecisionMade = false;
        lightIgnored = false;
        return false;
    }
    if (!lightDecisionMade) {
        lightDecisionMade = true;
        float chance = 0.0f;
        if (state == MonsterState::CHASE)     chance = tuning.chaseIgnoreLightChance;
        else if (state == MonsterState::HUNT) chance = tuning.huntIgnoreLightChance;
        lightIgnored = Random01() < chance;
    }
    if (lightIgnored) return false;

    TransitionTo(MonsterState::FLEE_LIGHT);
    return true;
}

// Viu um irmão iluminado (fora da caçada e da fuga): memoriza e persegue.
bool Monster::UpdateSightSensor() {
    if (state == MonsterState::HUNT || state == MonsterState::FLEE_LIGHT) return false;
    Vec2 seen;
    bool seenElevated = false;
    if (!CanSeeLitBrother(seen, seenElevated)) return false;

    lastKnownPlayerPos = seen;
    lastKnownElevated = seenElevated;
    hasMemory = true;
    memoryDecayTimer = 0.0f;
    if (state != MonsterState::CHASE) TransitionTo(MonsterState::CHASE);
    return true;
}

// Irmão escondido por campMaxTime → modo cerco (abre janelas mais rápido, a
// primeira já). Ver alguém fora do esconderijo encerra o cerco.
void Monster::UpdateCampMode(float dt, bool sawBrother) {
    if (sawBrother && strategicMode) {
        strategicMode = false;
        windowRadarTimer = 0.0f;
    }
    if (!AnyBrotherHidden()) {
        campTimer = 0.0f;
        return;
    }
    if (strategicMode) return;
    campTimer += dt;
    if (campTimer >= tuning.campMaxTime) {
        strategicMode = true;
        campTimer = 0.0f;
        windowRadarTimer = tuning.strategicRadarInterval;
    }
}

// Rondando o mesmo lugar tempo demais sem ver ninguém (ex.: na porta de um
// armário): esquece o jogador e patrulha longe dali por um tempo.
void Monster::UpdateBoredom(float dt, bool sawBrother) {
    const Vec2 pos = associated.box.Center();
    // Não conta vendo alguém, abrindo janela, saindo de parede ou caçando (têm limite próprio).
    const bool exempt = sawBrother || state == MonsterState::SABOTAGE_WINDOW ||
                        state == MonsterState::UNSTUCK || state == MonsterState::HUNT;
    if (exempt || pos.Distance(boredAnchor) > tuning.boredRadius) {
        boredAnchor = pos;
        boredTimer = 0.0f;
        return;
    }

    boredTimer += dt;
    if (boredTimer < tuning.boredTime) return;

    boredTimer = 0.0f;
    hasMemory = false;
    memoryDecayTimer = 0.0f;
    campTimer = 0.0f;
    fleeLightPos = boredAnchor;
    fleeLightAvoidTimer = tuning.boredAvoidTime;
    TransitionTo(MonsterState::PATROL);
}

// Patrulhando/investigando sem memória e sem descanso pendente: de tempos em
// tempos procura uma janela fechada no escuro para abrir.
void Monster::UpdateWindowRadar() {
    const bool idle = state == MonsterState::PATROL || state == MonsterState::INVESTIGATE;
    if (!idle || hasMemory || isElevated || postSabotageIdleTimer > 0.0f) return;

    const float interval = strategicMode ? tuning.strategicRadarInterval : tuning.windowRadarInterval;
    if (windowRadarTimer < interval) return;
    windowRadarTimer = 0.0f;
    if (Window* win = FindNearbyClosedWindow()) {
        targetWindow = win;
        TransitionTo(MonsterState::SABOTAGE_WINDOW);
    }
}

// Em estado de movimento, parado no mesmo lugar por kStuckTime → UNSTUCK.
void Monster::UpdateStuckDetection(float dt) {
    const bool moving = state == MonsterState::PATROL || state == MonsterState::INVESTIGATE ||
                        state == MonsterState::CHASE || state == MonsterState::HUNT;
    const Vec2 c = associated.box.Center();
    if (!moving || c.Distance(stuckRefPos) > kStuckMoveEpsilon) {
        stuckRefPos = c;
        stuckTimer = 0.0f;
        return;
    }
    stuckTimer += dt;
    if (stuckTimer >= kStuckTime) TransitionTo(MonsterState::UNSTUCK);
}

// Anda a animação pela distância percorrida (um passo sonoro por quadro), solta
// o eco visível com cadência própria, atualiza o loop de passos e vira o sprite.
void Monster::UpdateFootstepsAndEcho(float dt) {
    const Vec2 myPos = associated.box.Center();
    const Vec2 plPos = Character::player ? Character::player->GetAssociated().box.Center() : myPos;
    const float speed = (!currentPath.empty() && pathStep < static_cast<int>(currentPath.size())) ? moveSpeed : 0.0f;
    const bool fleeing = (state == MonsterState::FLEE_LIGHT);

    if (speed > 0.0f) {
        const float pxPerFrame = fleeing ? kAnimPxPerFrame * 0.6f : kAnimPxPerFrame;
        const float stepsMaxDist = 1900.0f + moveSpeed * 4.0f;   // alcance do som da passada

        animDistAccum += speed * dt;
        bool changed = false;
        while (animDistAccum >= pxPerFrame) {
            animDistAccum -= pxPerFrame;
            animFrame = (animFrame + 1) % kAnimFrameCount;
            changed = true;
            GameSfx::PlayMonsterStep(animFrame, myPos.x, myPos.y, plPos.x, plPos.y, stepsMaxDist);
        }
        if (changed) ApplyAnimFrame();

        // Eco: uma onda a cada kEchoStepPx andados (mais espaçado correndo).
        // Colado aos irmãos não há onda — ali ele já se ouve e se sente.
        echoDistAccum += speed * dt;
        const bool running = state == MonsterState::CHASE || state == MonsterState::HUNT || fleeing;
        if (echoDistAccum >= (running ? kEchoStepFarPx : kEchoStepPx)) {
            echoDistAccum = 0.0f;
            StageState* stage = Game::TryGetStageState();
            float nearest = 1e30f;
            if (Character::player)        nearest = std::min(nearest, myPos.Distance(Character::player->GetAssociated().box.Center()));
            if (Character::littleBrother) nearest = std::min(nearest, myPos.Distance(Character::littleBrother->GetAssociated().box.Center()));
            if (stage && nearest < 1e29f && nearest >= kEchoMinDistancePx) {
                const Vec2 origin(associated.box.x + associated.box.w * 0.5f,
                                  associated.box.y + associated.box.h - kEchoLiftPx);
                float loudness = 1.0f - std::min(1.0f, nearest / std::max(1.0f, stepsMaxDist));
                stage->SpawnMonsterEcho(origin, loudness * loudness);   // mesma curva do som
            }
        }
    } else {
        echoDistAccum = 0.0f;
    }

    GameSfx::UpdateMonsterFootsteps(dt, speed, myPos.x, myPos.y, plPos.x, plPos.y, fleeing);

    const float dx = myPos.x - lastCenterX;
    lastCenterX = myPos.x;
    if (dx < -0.5f)     facingLeft = true;
    else if (dx > 0.5f) facingLeft = false;
    if (SpriteRenderer* sr = associated.GetComponent<SpriteRenderer>()) {
        sr->SetFlip(facingLeft ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Estados
// ═════════════════════════════════════════════════════════════════════════════

const char* Monster::StateName(MonsterState s) {
    switch (s) {
        case MonsterState::PATROL:          return "PATROL";
        case MonsterState::INVESTIGATE:     return "INVESTIGATE";
        case MonsterState::CHASE:           return "CHASE";
        case MonsterState::HUNT:            return "HUNT";
        case MonsterState::FLEE_LIGHT:      return "FLEE_LIGHT";
        case MonsterState::SABOTAGE_WINDOW: return "SABOTAGE_WINDOW";
        case MonsterState::UNSTUCK:         return "UNSTUCK";
    }
    return "?";
}

// Troca de estado: registra na telemetria (agrupando oscilações), zera caminho
// e detecção de preso e faz a entrada de cada estado (velocidade, sons, rota).
void Monster::TransitionTo(MonsterState next) {
    const bool changed = (state != next);

    if (changed) {
        constexpr double kFlapWindowSec = 0.4;
        const double now = Telemetry::Now();
        const bool quick = telemetryLastTransitionAt >= 0.0 && (now - telemetryLastTransitionAt) < kFlapWindowSec;
        if (quick && next == telemetryPrevState) {
            if (telemetryFlapCount == 0) {
                telemetryFlapStartedAt = telemetryLastTransitionAt;
                telemetryFlapA = telemetryPrevState;
                telemetryFlapB = state;
            }
            telemetryFlapCount++;
        } else {
            FlushStateFlap();
            const Vec2 mc = associated.box.Center();
            Telemetry::Fields f;
            f.Str("from", StateName(state)).Str("to", StateName(next)).Pos("", mc.x, mc.y);
            if (Character::player) {
                f.Num("distToPlayer", mc.Distance(Character::player->GetAssociated().box.Center()));
            }
            Telemetry::Event("monster_state", f);
        }
        telemetryPrevState = state;
        telemetryLastTransitionAt = now;
        Telemetry::SetIntense(next == MonsterState::CHASE || next == MonsterState::HUNT);
    }

    state = next;
    stateTimer = 0.0f;
    currentPath.clear();
    pathStep = 0;
    stairCrossing.active = false;   // a entrada do novo estado decide a rota
    stuckTimer = 0.0f;
    stuckRefPos = associated.box.Center();

    switch (next) {
        case MonsterState::PATROL:
            moveSpeed = tuning.speedPatrol;
            PickNextPatrolPoint();
            break;

        case MonsterState::INVESTIGATE:
            moveSpeed = tuning.speedInvestigate;
            RequestPath(lastKnownPlayerPos, lastKnownElevated);
            break;

        case MonsterState::CHASE:
            moveSpeed = tuning.speedChase;
            chaseGraceTimer = firstChaseGraceGiven ? tuning.chaseGraceDuration : tuning.firstChaseGraceDuration;
            firstChaseGraceGiven = true;
            chaseNoSightTimer = 0.0f;
            if (changed) TryVocalize(false);
            break;

        case MonsterState::HUNT:
            moveSpeed = tuning.speedHunt;
            chaseGraceTimer = tuning.chaseGraceDuration;
            if (changed) TryVocalize(true);
            break;

        case MonsterState::UNSTUCK:
            moveSpeed = kSpeedUnstuck;
            GameSfx::StopMonsterFootsteps();
            break;

        case MonsterState::FLEE_LIGHT: {
            moveSpeed = tuning.speedFlee * 2.0f;
            GameSfx::StopMonsterFootsteps();
            Vec2 lightPos;
            if (FindNearestLight(lightPos)) {
                fleeLightPos = lightPos;                       // a patrulha evita este lugar depois
                fleeLightAvoidTimer = tuning.fleeLightAvoidTime;
                const Vec2 away = associated.box.Center() - lightPos;
                if (away.Magnitude() > 0.001f) {
                    RequestPath(associated.box.Center() + away.Normalized() * tuning.fleeDistance, isElevated);
                }
            }
            break;
        }

        case MonsterState::SABOTAGE_WINDOW:
            moveSpeed = tuning.speedInvestigate;
            pathRefreshTimer = kPathRefreshInterval;   // calcula a rota já no 1º frame
            break;
    }
}

// Grava a oscilação em curso numa linha (chamada na próxima transição de
// verdade e na destruição, para a última do andar não se perder).
void Monster::FlushStateFlap() {
    if (telemetryFlapCount <= 0) return;
    Telemetry::Event("monster_flap", Telemetry::Fields()
        .Str("a", StateName(telemetryFlapA))
        .Str("b", StateName(telemetryFlapB))
        .Int("count", telemetryFlapCount + 1)   // +1: a troca que abriu a série
        .Num("seconds", Telemetry::Now() - telemetryFlapStartedAt));
    telemetryFlapCount = 0;
}

// Segue a rota; ao chegar (ou sem rota) espera um pouco e escolhe o próximo ponto.
// Depois de abrir uma janela, fica parado.
void Monster::UpdatePatrol(float dt) {
    if (postSabotageIdleTimer > 0.0f) {
        currentPath.clear();
        stateTimer = 0.0f;
        return;
    }
    MoveAlongPath(dt, moveSpeed);
    if (!HasReachedTarget() && !HasNoPath()) {
        stateTimer = 0.0f;
        return;
    }
    if (stateTimer >= kPatrolWaitTime) {
        PickNextPatrolPoint();
        stateTimer = 0.0f;
    }
}

// Vai até a última posição conhecida (recalculando a rota); chegou, perdeu a
// memória ou ficou sem caminho → volta a patrulhar.
void Monster::UpdateInvestigate(float dt) {
    if (!hasMemory) {
        TransitionTo(MonsterState::PATROL);
        return;
    }
    MoveAlongPath(dt, moveSpeed);
    if (HasReachedTarget() || (HasNoPath() && stateTimer >= kInvestigateNoPathTime)) {
        hasMemory = false;
        TransitionTo(MonsterState::PATROL);
        return;
    }
    if (pathRefreshTimer >= kPathRefreshInterval) {
        pathRefreshTimer = 0.0f;
        RequestPath(lastKnownPlayerPos, lastKnownElevated);
    }
}

// Vendo: persegue a posição atual. Sem ver por kChaseLostSightTime (ou fim da
// rota) → investiga o último lugar.
void Monster::UpdateChase(float dt) {
    Vec2 seen;
    bool seenElevated = false;
    if (CanSeeLitBrother(seen, seenElevated)) {
        lastKnownPlayerPos = seen;
        lastKnownElevated = seenElevated;
        memoryDecayTimer = 0.0f;
        chaseNoSightTimer = 0.0f;
        if (pathRefreshTimer >= kPathRefreshInterval) {
            pathRefreshTimer = 0.0f;
            RequestPath(seen, seenElevated);
        }
        MoveAlongPath(dt, moveSpeed);
        return;
    }
    chaseNoSightTimer += dt;
    MoveAlongPath(dt, moveSpeed);
    if (chaseNoSightTimer >= kChaseLostSightTime || HasReachedTarget() || HasNoPath()) {
        TransitionTo(MonsterState::INVESTIGATE);
    }
}

// Caçada cega atrás do irmãozão, gritando; acaba se alguém se esconder ou após kHuntMaxTime.
void Monster::UpdateHunt(float dt) {
    if (AnyBrotherHidden()) {
        TransitionTo(MonsterState::INVESTIGATE);
        return;
    }
    TryVocalize(true);   // grita de novo só se a caçada passar do cooldown
    if (Character::player && pathRefreshTimer >= kPathRefreshInterval) {
        pathRefreshTimer = 0.0f;
        const Vec2 playerPos = Character::player->GetAssociated().box.Center();
        lastKnownPlayerPos = playerPos;
        lastKnownElevated = Character::player->isElevated;
        RequestPath(playerPos, lastKnownElevated);
    }
    MoveAlongPath(dt, moveSpeed);
    if (stateTimer >= kHuntMaxTime) TransitionTo(MonsterState::INVESTIGATE);
}

// Foge da luz. Com memória, volta a perseguir após kFleeReturnToChaseTime; fora
// da luz por kFleeOutOfLightTime, patrulha. Sem rota, tenta 8 direções a partir
// de "longe da luz" até achar um ponto no escuro.
void Monster::UpdateFleeLight(float dt) {
    if (!currentPath.empty()) MoveAlongPath(dt, moveSpeed);

    if (stateTimer >= kFleeReturnToChaseTime && hasMemory) {
        TransitionTo(MonsterState::CHASE);
        return;
    }
    if (!IsSelfInLight()) {
        if (stateTimer >= kFleeOutOfLightTime) TransitionTo(MonsterState::PATROL);
        return;
    }
    stateTimer = 0.0f;

    if (!(HasReachedTarget() || HasNoPath()) || pathRefreshTimer < kPathRefreshInterval) return;
    pathRefreshTimer = 0.0f;

    Vec2 lightPos;
    if (!FindNearestLight(lightPos)) return;
    const Vec2 myPos = associated.box.Center();
    const Vec2 away = myPos - lightPos;
    if (away.Magnitude() <= 0.001f) return;

    const float baseAngle = std::atan2(away.y, away.x);
    constexpr int kTries = 8;
    for (int i = 0; i < kTries; i++) {
        const float angle = baseAngle + (static_cast<float>(i) / kTries) * 2.0f * kPi;
        const Vec2 candidate = myPos + Vec2(std::cos(angle), std::sin(angle)) * tuning.fleeDistance;
        if (!IsWorldPosInAnyLight(candidate)) {
            RequestPath(candidate, isElevated);
            return;
        }
    }
    RequestPath(myPos + Vec2(std::cos(baseAngle), std::sin(baseAngle)) * tuning.fleeDistance, isElevated);
}

// Sai de dentro da parede andando para longe do irmãozão, sem colisão. Livre
// (após o mínimo) ou no tempo máximo, volta a patrulhar — se ainda preso,
// teleporta para o ponto de patrulha mais próximo.
void Monster::UpdateUnstuck(float dt) {
    StageState* stage = Game::TryGetStageState();
    Vec2 myPos = associated.box.Center();
    Vec2 away = Character::player ? myPos - Character::player->GetAssociated().box.Center() : Vec2(1.0f, 0.0f);
    const float mag = away.Magnitude();
    away = (mag < 0.01f) ? Vec2(1.0f, 0.0f) : away * (1.0f / mag);

    associated.box.x += away.x * kSpeedUnstuck * dt;
    associated.box.y += away.y * kSpeedUnstuck * dt;
    myPos = associated.box.Center();

    const bool free = stage && stage->IsWorldPosNavigableFor(myPos, &associated, kNavFootRadius);
    if (!((free && stateTimer >= kUnstuckMinTime) || stateTimer >= kUnstuckMaxTime)) return;

    if (!free && !patrolPoints.empty()) {
        const Vec2 p = *std::min_element(patrolPoints.begin(), patrolPoints.end(),
            [&](const Vec2& a, const Vec2& b) { return myPos.Distance(a) < myPos.Distance(b); });
        associated.box.x = p.x - associated.box.w / 2.0f;
        associated.box.y = p.y - associated.box.h / 2.0f;
        isElevated = false;            // pontos de patrulha ficam no chão
        hasPrevCenter = false;         // o teleporte não conta como passo pela escada
    }
    damageCooldown = tuning.damageCooldownTime;   // não fere quem estiver colado na saída
    if (isElevated && !FootOnStairs()) isElevated = false;   // saiu da escada sem passar pelo tapete
    TransitionTo(MonsterState::PATROL);
}

// Vai até perto da janela (um ponto andável ao redor dela, fora do tapete) e a
// abre. Desiste se a janela mudar, ficar iluminada, demorar ou não houver caminho.
void Monster::UpdateSabotageWindow(float dt) {
    if (!targetWindow || targetWindow->GetState() != Window::WindowState::CLOSED ||
        IsWorldPosInAnyLight(targetWindow->GetAssociated().box.Center())) {
        targetWindow = nullptr;
        TransitionTo(MonsterState::PATROL);
        return;
    }
    if (stateTimer >= kSabotageMaxTime) {
        GiveUpWindow(strategicMode ? tuning.strategicSabotageRest : kWindowRestNormal);
        return;
    }

    const Vec2 winPos = targetWindow->GetAssociated().box.Center();
    const Vec2 myPos = associated.box.Center();
    const float dist = myPos.Distance(winPos);

    if (dist <= kSabotageReachDist) {
        targetWindow->Toggle();
        Telemetry::Event("window_opened", Telemetry::Fields().Pos("", winPos.x, winPos.y));
        targetWindow = nullptr;
        windowRadarTimer = -tuning.sabotageDelay;
        postSabotageIdleTimer = tuning.postSabotageIdle;
        TransitionTo(MonsterState::PATROL);
        return;
    }

    const float restAfterFail = strategicMode ? kWindowRestStrategic : kWindowRestNormal;
    if (pathRefreshTimer >= kPathRefreshInterval) {
        pathRefreshTimer = 0.0f;
        // O ponto andável mais perto do monstro entre 8 ao redor da janela.
        StageState* stage = Game::TryGetStageState();
        Vec2 best;
        float bestDist = 1e9f;
        bool found = false;
        for (int i = 0; stage && i < 8; i++) {
            const float angle = i * (kPi / 4.0f);
            const Vec2 c = winPos + Vec2(std::cos(angle), std::sin(angle)) * kSabotageApproachDist;
            if (!stage->IsWorldPosNavigableFor(c, &associated, kNavFootRadius)) continue;
            const float d = myPos.Distance(c);
            if (d < bestDist) { bestDist = d; best = c; found = true; }
        }
        if (!found) {
            GiveUpWindow(restAfterFail);
            return;
        }
        RequestPath(best);
    }

    if (!HasNoPath()) {
        MoveAlongPath(dt, moveSpeed);
    } else if (dist > kSabotageGiveUpDist && stateTimer >= 2.0f) {
        GiveUpWindow(restAfterFail);
    }
}

// Grito (caçada) ou rosnado (avistou alguém), um de cada vez: os dois dividem o
// mesmo cooldown, para nunca soarem em sequência.
void Monster::TryVocalize(bool scream) {
    if (vocalCooldown > 0.0f) return;
    if (scream) GameSfx::PlayMonsterScream();
    else        GameSfx::PlayMonsterSpot();
    vocalCooldown = tuning.vocalCooldown;
}

// Solta a janela, dá um descanso ao radar e volta a patrulhar.
void Monster::GiveUpWindow(float radarRest) {
    windowRadarTimer = -radarRest;
    targetWindow = nullptr;
    TransitionTo(MonsterState::PATROL);
}

// Próximo ponto de patrulha: evitando o lugar da última luz/tédio enquanto
// valer (um sorteado longe dele, ou o mais longe); senão aleatório ou em ordem.
void Monster::PickNextPatrolPoint() {
    if (patrolPoints.empty()) return;
    const int count = static_cast<int>(patrolPoints.size());
    if (count == 1) {
        patrolIndex = 0;
        RequestPath(patrolPoints[0]);
        return;
    }

    if (fleeLightAvoidTimer > 0.0f) {
        for (int tries = 0; tries < count * 2; ++tries) {
            const int cand = std::rand() % count;
            if (cand != patrolIndex && patrolPoints[cand].Distance(fleeLightPos) > tuning.fleeLightAvoidRadius) {
                patrolIndex = cand;
                RequestPath(patrolPoints[static_cast<size_t>(patrolIndex)]);
                return;
            }
        }
        int best = -1;
        float bestDist = -1.0f;
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
        while (next == patrolIndex) next = std::rand() % count;
        patrolIndex = next;
    } else {
        patrolIndex = (patrolIndex + 1) % count;
    }
    RequestPath(patrolPoints[static_cast<size_t>(patrolIndex)]);
}

void Monster::AddPatrolPoint(Vec2 worldPos) {
    patrolPoints.push_back(worldPos);
}

// Barulho no alcance, patrulhando ou investigando: vai até ele (com cooldown).
void Monster::NotifyNoise(Vec2 noiseWorldPos) {
    if (noiseCooldownTimer > 0.0f) return;
    if (state != MonsterState::PATROL && state != MonsterState::INVESTIGATE) return;
    if (associated.box.Center().Distance(noiseWorldPos) > tuning.noiseHearRadius) return;

    noiseCooldownTimer = tuning.noiseCooldown;
    lastKnownPlayerPos = noiseWorldPos;
    lastKnownElevated = false;
    hasMemory = true;
    memoryDecayTimer = 0.0f;
    if (state != MonsterState::INVESTIGATE) TransitionTo(MonsterState::INVESTIGATE);
    else RequestPath(noiseWorldPos);
}

void Monster::NotifyCollision(GameObject& /*other*/) {}

// ═════════════════════════════════════════════════════════════════════════════
//  Sensores
// ═════════════════════════════════════════════════════════════════════════════

// Toque num irmão visível (não escondido, não interagindo) NO MESMO NÍVEL da
// escada: dano de sanidade (menor na luz), memória, feedback na tela e caçada.
void Monster::CheckDamageCollision() {
    if (state == MonsterState::UNSTUCK || damageCooldown > 0.0f) return;
    StageState* stage = Game::TryGetStageState();
    if (stage && stage->IsMonsterBlindDebug()) return;

    const SDL_Rect dmgBox{
        static_cast<int>(associated.box.x + associated.box.w * kDamageBoxInset),
        static_cast<int>(associated.box.y + associated.box.h * kDamageBoxInset),
        static_cast<int>(associated.box.w * kDamageBoxScale),
        static_cast<int>(associated.box.h * kDamageBoxScale)};

    auto tryHit = [&](Character* c, float illumination, const char* who) {
        if (!c || c->isHidden || c->currentState == Character::ActionState::INTERACTING) return false;
        if (c->isElevated != isElevated) return false;   // embaixo da escada não alcança quem está em cima
        const SDL_Rect hb = c->GetHitRect();
        if (SDL_HasIntersection(&hb, &dmgBox) != SDL_TRUE) return false;

        const bool lit = stage && illumination >= tuning.illuminationThreshold;
        const float damage = lit ? tuning.sanityDamageLit : tuning.sanityDamageDark;
        c->sanity = std::max(0.0f, c->sanity - damage);
        const Vec2 p = c->GetAssociated().box.Center();
        lastKnownPlayerPos = p;
        lastKnownElevated = c->isElevated;
        Telemetry::Event("monster_hit", Telemetry::Fields()
            .Str("victim", who).Bool("lit", lit).Num("damage", damage)
            .Num("sanityAfter", c->sanity).Pos("", p.x, p.y));
        return true;
    };

    const bool hit = tryHit(Character::player, stage ? stage->bigIlluminationLevel : 0.0f, "big") ||
                     tryHit(Character::littleBrother, stage ? stage->smallIlluminationLevel : 0.0f, "small");
    if (!hit) return;

    damageCooldown = tuning.damageCooldownTime;
    hasMemory = true;
    memoryDecayTimer = 0.0f;
    if (stage) stage->TriggerMonsterHitFeedback();
    if (state != MonsterState::HUNT) TransitionTo(MonsterState::HUNT);
}

// Algum irmão iluminado, não escondido, dentro do alcance e com linha livre.
// Devolve também se ele está em cima da escada (para a rota dar a volta).
bool Monster::CanSeeLitBrother(Vec2& outPos, bool& outElevated) const {
    StageState* stage = Game::TryGetStageState();
    if (!stage || stage->IsMonsterBlindDebug()) return false;

    const Vec2 myPos = associated.box.Center();
    auto check = [&](Character* c, float illumination) {
        if (!c || c->isHidden || illumination < tuning.illuminationThreshold) return false;
        const Vec2 pos = c->GetAssociated().box.Center();
        if (myPos.Distance(pos) > tuning.sightRadius) return false;
        if (!stage->HasWalkableLine(myPos, pos, &associated, kSightLosRadius)) return false;
        outPos = pos;
        outElevated = c->isElevated;
        return true;
    };
    return check(Character::player, stage->bigIlluminationLevel) ||
           check(Character::littleBrother, stage->smallIlluminationLevel);
}

// Ponto dentro do raio de alguma luz do mundo (velas, lamparina) + margem. O isqueiro não conta.
bool Monster::IsWorldPosInAnyLight(Vec2 worldPos, float extraRadius) const {
    StageState* stage = Game::TryGetStageState();
    if (!stage) return false;
    for (const auto& light : stage->GetLights()) {
        if (!light.enabled || light.params.falloffRadiusPx <= 0.0f) continue;
        if (worldPos.Distance(light.worldPos) < light.params.falloffRadiusPx + extraRadius) return true;
    }
    return false;
}

// O corpo (90% da caixa) dentro de fleeLightRadiusFraction do raio de alguma luz do mundo.
bool Monster::IsSelfInLight() const {
    StageState* stage = Game::TryGetStageState();
    if (!stage) return false;

    Rect body = associated.box;
    body.x += body.w * 0.05f;
    body.y += body.h * 0.05f;
    body.w *= 0.90f;
    body.h *= 0.90f;

    for (const auto& light : stage->GetLights()) {
        if (!light.enabled || light.params.falloffRadiusPx <= 0.0f) continue;
        if (DistanceFromRectToPoint(body, light.worldPos) < light.params.falloffRadiusPx * tuning.fleeLightRadiusFraction) {
            return true;
        }
    }
    return false;
}

// Posição da luz do mundo acesa mais próxima. False se não houver nenhuma.
bool Monster::FindNearestLight(Vec2& outLightPos) const {
    StageState* stage = Game::TryGetStageState();
    if (!stage) return false;
    const Vec2 myPos = associated.box.Center();
    float best = 1e9f;
    for (const auto& light : stage->GetLights()) {
        if (!light.enabled) continue;
        const float d = myPos.Distance(light.worldPos);
        if (d < best) { best = d; outLightPos = light.worldPos; }
    }
    return best < 1e8f;
}

// Pés dentro da área de algum objeto de escada (Escada / Escada_Quebrada).
bool Monster::FootOnStairs() const {
    StageState* stage = Game::TryGetStageState();
    if (!stage) return false;
    const Vec2 foot(associated.box.Center().x, associated.box.y + associated.box.h);
    for (const auto& goPtr : stage->GetObjectArray()) {
        if (goPtr && goPtr->isStairs && goPtr->box.Contains(foot)) return true;
    }
    return false;
}

// Janela fechada, no escuro, mais próxima dentro do alcance do radar.
Window* Monster::FindNearbyClosedWindow() {
    StageState* stage = Game::TryGetStageState();
    if (!stage) return nullptr;

    Window* best = nullptr;
    float bestDist = tuning.windowRadarRange;
    const Vec2 myPos = associated.box.Center();
    for (const auto& goPtr : stage->GetObjectArray()) {
        GameObject* go = goPtr.get();
        if (!go || go->IsDead()) continue;
        Window* win = go->GetComponent<Window>();
        if (!win || win->GetState() != Window::WindowState::CLOSED) continue;
        const Vec2 winPos = go->box.Center();
        if (IsWorldPosInAnyLight(winPos)) continue;
        const float d = myPos.Distance(winPos);
        if (d < bestDist) { bestDist = d; best = win; }
    }
    return best;
}

bool Monster::AnyBrotherHidden() {
    return (Character::player && Character::player->isHidden) ||
           (Character::littleBrother && Character::littleBrother->isHidden);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Caminho
// ═════════════════════════════════════════════════════════════════════════════

// Rota até o destino. Outro nível da escada → dá a volta pela entrada dela
// (PlanStairCrossing). Na escada → reto (é uma rampa entre corrimãos). No chão
// → A* com o pé circular: destino inválido = sem rota; A* vazio mas destino
// perto = anda reto; já em cima do 1º nó = pula ele. Durante uma travessia,
// pedidos novos esperam ela terminar.
void Monster::RequestPath(Vec2 destination, bool destElevated) {
    if (stairCrossing.active) return;
    StageState* stage = Game::TryGetStageState();
    if (!stage) return;

    currentPath.clear();
    pathStep = 0;
    if (destElevated != isElevated) {
        PlanStairCrossing(destElevated);
        return;
    }
    if (isElevated) {
        currentPath.push_back(destination);
        return;
    }
    if (!stage->IsWorldPosNavigableFor(destination, &associated, kNavFootRadius)) return;

    const Vec2 myPos = associated.box.Center();
    currentPath = stage->FindPathWorld(myPos, destination, &associated, 4096, kNavFootRadius);
    if (currentPath.empty()) {
        if (myPos.Distance(destination) < kPathDirectFallback) currentPath.push_back(destination);
    } else if (currentPath.size() > 1 && myPos.Distance(currentPath[0]) <= kPathSkipFirstNode) {
        pathStep = 1;
    }
}

// Anda na direção do nó atual; perto dele, passa ao próximo.
void Monster::MoveAlongPath(float dt, float speed) {
    if (currentPath.empty() || pathStep >= static_cast<int>(currentPath.size())) return;
    const Vec2 dir = currentPath[static_cast<size_t>(pathStep)] - associated.box.Center();
    const float dist = dir.Magnitude();
    if (dist < kPathNodeReached) {
        pathStep++;
        return;
    }
    associated.box.x += dir.x / dist * speed * dt;
    associated.box.y += dir.y / dist * speed * dt;
}

// Vai até a entrada da escada mais próxima (A* no chão; reto se já estiver na
// escada) e atravessa o tapete em linha reta — subindo vira "em cima", descendo
// volta ao chão, igual aos irmãos. Os pontos são dos PÉS; o caminho usa o centro.
bool Monster::PlanStairCrossing(bool goingUp) {
    StageState* stage = Game::TryGetStageState();
    if (!stage) return false;

    const Vec2 myPos = associated.box.Center();
    const Vec2 footToCenter(0.0f, -associated.box.h * 0.5f);
    bool found = false;
    Vec2 entry, exit;
    float bestDist = 1e30f;
    for (const auto& goPtr : stage->GetObjectArray()) {
        StairTrigger* st = goPtr ? goPtr->GetComponent<StairTrigger>() : nullptr;
        if (!st) continue;
        const Rect& z = st->GetZone();
        const float cx = z.x + z.w * 0.5f;
        const Vec2 below(cx, z.y + z.h + kStairApproachPx);
        const Vec2 above(cx, z.y - kStairApproachPx);
        const Vec2 e = (goingUp ? below : above) + footToCenter;
        const float d = myPos.Distance(e);
        if (d < bestDist) {
            bestDist = d;
            entry = e;
            exit = (goingUp ? above : below) + footToCenter;
            found = true;
        }
    }
    if (!found) return false;

    currentPath.clear();
    pathStep = 0;
    if (!isElevated) {
        currentPath = stage->FindPathWorld(myPos, entry, &associated, 4096, kNavFootRadius);
        if (currentPath.size() > 1 && myPos.Distance(currentPath[0]) <= kPathSkipFirstNode) pathStep = 1;
    }
    if (currentPath.empty()) currentPath.push_back(entry);
    currentPath.push_back(exit);
    stairCrossing = StairCrossing{true, goingUp, 0.0f};
    return true;
}

// Mede a velocidade vertical (do frame anterior), aplica os gatilhos de escada
// pelos pés e encerra a travessia quando o nível virou (ou se demorar demais).
void Monster::UpdateStairState(float dt) {
    const Vec2 c = associated.box.Center();
    velocityY = (hasPrevCenter && dt > 0.0f) ? (c.y - prevCenterY) / dt : 0.0f;
    prevCenterY = c.y;
    hasPrevCenter = true;

    if (StageState* stage = Game::TryGetStageState()) {
        const Vec2 foot(c.x, associated.box.y + associated.box.h);
        for (const auto& goPtr : stage->GetObjectArray()) {
            if (StairTrigger* st = goPtr ? goPtr->GetComponent<StairTrigger>() : nullptr) {
                StairTrigger::ApplyCrossing(st->GetZone(), st->GetAnchorY(), foot, velocityY, isElevated, stairAnchorY);
            }
        }
    }

    if (!stairCrossing.active) return;
    if (isElevated == stairCrossing.goingUp) {
        stairCrossing.active = false;
        pathRefreshTimer = kPathRefreshInterval;   // replaneja já no novo nível
        return;
    }
    stairCrossing.timer += dt;
    if (stairCrossing.timer > kStairCrossingTimeout) stairCrossing.active = false;
}

bool Monster::HasReachedTarget() const {
    return !currentPath.empty() && pathStep >= static_cast<int>(currentPath.size());
}

bool Monster::HasNoPath() const {
    return currentPath.empty();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Debug
// ═════════════════════════════════════════════════════════════════════════════

// Só em build debug com [B]: hitbox de dano, círculo de navegação, estado e rota.
void Monster::Render() {
#ifdef DEBUG
    StageState* stage = Game::TryGetStageState();
    if (!stage || !stage->IsPhysicsDebugOn()) return;

    SDL_Renderer* r = Game::GetInstance().GetRenderer();
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    const float z = Camera::GetZoom();

    const Vec2 dmgTL = Camera::WorldToScreen(Vec2(associated.box.x + associated.box.w * kDamageBoxInset,
                                                  associated.box.y + associated.box.h * kDamageBoxInset));
    const SDL_Rect dmgBox{static_cast<int>(dmgTL.x), static_cast<int>(dmgTL.y),
                          static_cast<int>(associated.box.w * kDamageBoxScale * z),
                          static_cast<int>(associated.box.h * kDamageBoxScale * z)};
    SDL_SetRenderDrawColor(r, 255, 0, 0, 100);
    SDL_RenderFillRect(r, &dmgBox);

    const Vec2 c = associated.box.Center();
    const Vec2 cs = Camera::WorldToScreen(c);
    const float navR = kNavFootRadius * z;
    SDL_SetRenderDrawColor(r, 0, 255, 120, 230);
    float px = cs.x + navR, py = cs.y;
    for (int i = 1; i <= 40; ++i) {
        const float a = (static_cast<float>(i) / 40) * 2.0f * kPi;
        const float nx = cs.x + std::cos(a) * navR, ny = cs.y + std::sin(a) * navR;
        SDL_RenderDrawLineF(r, px, py, nx, ny);
        px = nx; py = ny;
    }

    std::string label = std::string("STATE: ") + StateName(state);
    if (strategicMode) label += " [STRAT]";
    if (auto font = Resources::GetFont("Recursos/font/times.ttf", 18)) {
        if (SDL_Surface* sf = TTF_RenderUTF8_Blended(font.get(), label.c_str(), SDL_Color{255, 255, 255, 255})) {
            if (SDL_Texture* tex = SDL_CreateTextureFromSurface(r, sf)) {
                const Vec2 anchor = Camera::WorldToScreen(Vec2(c.x, associated.box.y));   // texto não encolhe com o zoom
                const SDL_Rect dst{static_cast<int>(anchor.x) - sf->w / 2, static_cast<int>(anchor.y) - sf->h - 8, sf->w, sf->h};
                SDL_RenderCopy(r, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(sf);
        }
    }

    if (!currentPath.empty()) {
        SDL_SetRenderDrawColor(r, 0, 255, 0, 200);
        Vec2 prev = c;
        for (size_t i = static_cast<size_t>(pathStep); i < currentPath.size(); i++) {
            const Vec2 a = Camera::WorldToScreen(prev);
            const Vec2 b = Camera::WorldToScreen(currentPath[i]);
            SDL_RenderDrawLineF(r, a.x, a.y, b.x, b.y);
            prev = currentPath[i];
        }
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
#endif
}