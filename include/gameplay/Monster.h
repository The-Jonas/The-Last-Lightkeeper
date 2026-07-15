#ifndef MONSTER_H
#define MONSTER_H

#include "engine/Component.h"
#include "engine/GameObject.h"
#include "core/Timer.h"
#include "math/Vec2.h"
#include "core/LevelManager.h"
#include "gameplay/Window.h"
#include <vector>
#include <string>
#include <sstream>
#include <cstdio> 

class Monster : public Component {
public:
    enum class MonsterState {
        PATROL,          // Patrulhando por waypoints
        INVESTIGATE,     // Verificando última posição de ruído ou visão
        HUNT,            // Caçada cega e agressiva após tocar no jogador
        CHASE,           // Perseguição ativa com linha de visão
        FLEE_LIGHT,      // Recuo devido a alta iluminação
        SABOTAGE_WINDOW, // Movendo-se para abrir uma janela
        UNSTUCK          // Estado fantasma para sair de dentro de paredes
    };

    Monster(GameObject& associated);
    ~Monster();

    void Start() override;  
    void Update(float dt) override;
    void Render() override;
    void NotifyCollision(GameObject& other) override;
    
    void AddPatrolPoint(Vec2 point);
    void NotifyNoise(Vec2 noiseWorldPos); 

    float GetVisionRevealTimer() const { return visionRevealTimer; }
    bool isVisibleToLittleBrother() const { return visionRevealTimer > 0.0f; }
    MonsterState GetState() const { return state; } 

    void ActivateVision(float duration) { visionRevealTimer = duration; }
    GameObject& GetAssociated() { return associated; }

    static constexpr float kVisionRevealDuration = 0.3f;

private:
    void CheckDamageCollision();
    void ApplyAnimFrame();
    void TransitionTo(MonsterState next);

    // ── Updates de Estado ─────────────────────────────────────────────────────
    void UpdatePatrol(float dt);
    void UpdateInvestigate(float dt);
    void UpdateChase(float dt);
    void UpdateHunt(float dt);
    void UpdateFleeLigth(float dt);
    void UpdateUnstuck(float dt);
    void UpdateSabotageWindow(float dt);

    // ── Sensores ──────────────────────────────────────────────────────────────
    bool CanSeeLitBrother(Vec2& outPos) const;
    bool IsWorldPosInAnyLight(Vec2 worldPos, float extraRadius = 0.0f) const;
    bool IsSelfInLight() const;

    // ── Movimento e Pathfinding ───────────────────────────────────────────────
    void MoveAlongPath(float dt, float speed);
    void RequestPath(Vec2 destination);
    bool HasReachedTarget() const;
    bool HasNoPath() const;
    
    std::vector<Vec2> currentPath;
    int pathStep = 0;
    Vec2 targetPos;
    float moveSpeed = 0.0f;

    // ── Animação ──────────────────────────────────────────────────────────────
    int animFrame = 0;
    float animDistAccum = 0.0f;
    bool facingLeft = false;
    float lastCenterX = 0.0f;

    static constexpr int kAnimFrameCount = 5;
    static constexpr float kAnimPxPerFrame = 32.0f; 

    // ── Lógica Central ────────────────────────────────────────────────────────
    MonsterState state = MonsterState::PATROL;
    float stateTimer = 0.0f;
    float pathRefreshTimer = 0.0f;

    Vec2 lastKnownPlayerPos;
    float memoryDecayTimer = 0.0f;
    bool hasMemory = false;

    // ── Configurações Dinâmicas (monster.json) ────────────────────────────────
    void LoadTuning();

    float kSpeedPatrol = 85.0f;
    float kSpeedInvestigate = 120.0f;
    float kSpeedChase = 170.0f;
    float kSpeedHunt = 185.0f;
    float kSpeedFlee = 220.0f;

    float kSightRadius = 800.0f;
    float kIlluminationThreshold = 0.20f;
    float kMemoryDecayTime = 10.0f;
    float kCampMaxTime = 18.0f;
    
    float kWindowRadarInterval = 6.0f;
    float kWindowRadarRange = 2900.0f;
    float kSabotageDelay = 4.0f;
    float kPostSabotageIdle = 8.0f;

    float kNoiseHearRadius = 950.0f;
    float kNoiseCooldown = 1.5f;

    float kFleeDistance = 500.0f;
    float kFleeLightAvoidTime = 12.0f;
    float kFleeLightAvoidRadius = 520.0f;
    float kFleeLightRadiusFraction = 0.85f;

    float kChaseGraceDuration = 1.5f;
    float kFirstChaseGraceDuration = 5.0f;
    float kChaseIgnoreLightChance = 0.50f;
    float kHuntIgnoreLightChance = 0.85f;

    float kDamageCooldownTime = 1.5f;
    float kSanityDamageDark = 80.0f;
    float kSanityDamageLit = 50.0f;

    // ── Constantes Estáticas ──────────────────────────────────────────────────
    static constexpr float kDamageBoxScale = 0.595f;
    static constexpr float kDamageBoxInset = (1.0f - kDamageBoxScale) * 0.5f;
    static constexpr float kFleeThreshold = 0.70f;
    static constexpr float kPathRefreshInterval = 0.30f;
    static constexpr float kNavFootRadius = 18.0f;
    static constexpr float kSightLosRadius = 25.0f;
    static constexpr float kSpotSoundCooldown = 6.0f;
    static constexpr float kHuntScreamInterval = 2.6f;
    static constexpr float kStuckMoveEpsilon = 6.0f;
    static constexpr float kStuckTime = 2.5f;
    static constexpr float kSpeedUnstuck = 130.0f;
    static constexpr float kUnstuckMinTime = 1.0f;
    static constexpr float kUnstuckMaxTime = 3.5f;
    static constexpr float kStrategicRadarInterval = 1.0f;

    // ── Timers e Flags de Controle ────────────────────────────────────────────
    float campTimer = 0.0f;
    float visionRevealTimer = 0.0f;
    float damageCooldown = 0.0f;
    float windowRadarTimer = 0.0f;
    float postSabotageIdleTimer = 0.0f;
    float noiseCooldownTimer = 0.0f;
    float fleeLightAvoidTimer = 0.0f;
    float chaseGraceTimer = 0.0f;
    float chaseNoSightTimer = 0.0f;
    float spotSoundCooldown = 0.0f;
    float huntScreamTimer = 0.0f;
    float stuckTimer = 0.0f;

    Vec2 fleeLightPos;
    Vec2 stuckRefPos;
    bool firstChaseGraceGiven = false;
    bool lightDecisionMade = false;
    bool lightIgnored = false;
    bool strategicMode = false;

    // ── Patrulha e Sabotagem ──────────────────────────────────────────────────
    std::vector<Vec2> patrolPoints;
    int patrolIndex = 0;
    bool patrolRandom = true;

    Window* targetWindow = nullptr;
    Window* FindNearbyClosedWindow();
    
    void PickNextPatrolPoint();
    static bool AnyBrotherHidden();
};

#endif