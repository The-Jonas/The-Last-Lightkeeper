#ifndef MONSTER_H
#define MONSTER_H

#include "engine/Component.h"
#include "engine/GameObject.h"
#include "math/Vec2.h"

#include <vector>

class Window;

// ─────────────────────────────────────────────────────────────────────────────
//  Monstro — máquina de estados que patrulha, investiga ruídos, persegue quem
//  vê na luz, caça depois de tocar num irmão, foge da luz e abre janelas.
//
//  Os números de comportamento (velocidades, raios, tempos, chances, dano) vêm
//  de config/monster.json; sem o arquivo valem os padrões de Tuning.
// ─────────────────────────────────────────────────────────────────────────────
class Monster : public Component {
public:
    enum class MonsterState {
        PATROL,                                                                 // anda entre os pontos de patrulha
        INVESTIGATE,                                                            // vai até a última posição ouvida/vista
        HUNT,                                                                   // caçada cega depois de tocar num irmão
        CHASE,                                                                  // perseguição com linha de visão
        FLEE_LIGHT,                                                             // recuando da luz
        SABOTAGE_WINDOW,                                                        // indo abrir uma janela
        UNSTUCK                                                                 // saindo de dentro de parede
    };

    explicit Monster(GameObject& associated);
    ~Monster() override;

    void Start() override;
    void Update(float dt) override;
    void Render() override;                                                     // só debug (-DDEBUG + tecla [B])
    void NotifyCollision(GameObject& other) override;                           // não usa: o dano é por hitbox própria

    void AddPatrolPoint(Vec2 worldPos);
    void NotifyNoise(Vec2 noiseWorldPos);                                       // barulho do jogador por perto → investiga

    MonsterState GetState() const { return state; }
    static const char* StateName(MonsterState s);                               // texto do estado (debug e telemetria)
    GameObject& GetAssociated() { return associated; }

    // ── Habilidade do irmãozinho: revela o monstro por um tempo ─────────────
    void  ActivateVision(float duration) { visionRevealTimer = duration; }
    float GetVisionRevealTimer() const { return visionRevealTimer; }
    static constexpr float kVisionRevealDuration = 0.3f;

    // ── Eco dos passos ───────────────────────────────────────────────────────
    // Quando a onda de uma passada chega a um irmão, o monstro aparece por um
    // instante mesmo fora da visão (StageState::VisibilityOfObject).
    void  TriggerEchoReveal() { echoRevealTimer = kEchoRevealDuration; }
    float EchoRevealAmount() const {                     // 0..1, desvanecendo
        return (echoRevealTimer > 0.0f) ? (echoRevealTimer / kEchoRevealDuration) : 0.0f;
    }
    static constexpr float kEchoRevealDuration = 0.55f;
    static constexpr float kEchoStepPx         = 190.0f;                        // distância andada entre ondas (andando)
    static constexpr float kEchoStepFarPx      = 420.0f;                        // idem, correndo
    static constexpr float kEchoMinDistancePx  = 190.0f;                        // mais perto que isto, sem onda (abaixo do raio final da onda, 340)
    static constexpr float kEchoLiftPx         = 34.0f;                         // quanto a origem da onda sobe acima dos pés

private:
    // Números de comportamento — as chaves de config/monster.json são os nomes
    // em snake_case (speedPatrol → "speed_patrol").
    struct Tuning {
        float speedPatrol       = 85.0f;
        float speedInvestigate  = 120.0f;
        float speedChase        = 170.0f;
        float speedHunt         = 185.0f;
        float speedFlee         = 220.0f;                                       // a fuga usa o dobro disto

        float sightRadius           = 800.0f;                                   // alcance da visão (px de mundo)
        float illuminationThreshold = 0.20f;                                    // luz mínima no irmão para ser visto
        float memoryDecayTime       = 10.0f;                                    // esquece a última posição depois disto

        float campMaxTime            = 18.0f;                                   // irmão escondido por isto → modo cerco
        float strategicRadarInterval = 4.0f;                                    // intervalo entre sabotagens no cerco
        float strategicSabotageRest  = 8.0f;                                    // descanso no cerco depois de desistir de uma janela

        float boredTime      = 10.0f;                                           // rondando o mesmo lugar por isto → desiste
        float boredRadius    = 320.0f;                                          // "mesmo lugar"
        float boredAvoidTime = 8.0f;                                            // evita voltar lá por isto

        float windowRadarInterval = 6.0f;                                       // intervalo entre procuras de janela
        float windowRadarRange    = 2900.0f;                                    // alcance da procura
        float sabotageDelay       = 4.0f;                                       // espera depois de abrir uma
        float postSabotageIdle    = 8.0f;                                       // fica parado depois de abrir

        float noiseHearRadius = 950.0f;
        float noiseCooldown   = 1.5f;

        float fleeDistance            = 500.0f;                                 // quanto foge da luz
        float fleeLightAvoidTime      = 12.0f;                                  // evita o lugar da luz na patrulha por isto
        float fleeLightAvoidRadius    = 520.0f;
        float fleeLightRadiusFraction = 0.85f;                                  // fração do raio da luz que conta como "na luz"

        float chaseGraceDuration      = 1.5f;                                   // ignora a luz no início de uma perseguição
        float firstChaseGraceDuration = 5.0f;                                   // idem, na primeira do jogo
        float chaseIgnoreLightChance  = 0.50f;                                  // chance de ignorar a luz perseguindo
        float huntIgnoreLightChance   = 0.85f;                                  // idem, caçando

        float sanityDamageDark   = 80.0f;                                       // dano de um toque no escuro
        float sanityDamageLit    = 50.0f;                                       // dano de um toque na luz
        float damageCooldownTime = 1.5f;
    };

    // ── Constantes fixas ─────────────────────────────────────────────────────
    static constexpr int   kAnimFrameCount      = 5;
    static constexpr float kAnimPxPerFrame      = 32.0f;                        // distância andada por quadro de animação
    static constexpr float kDamageBoxScale      = 0.595f;                       // hitbox de dano = fração da caixa
    static constexpr float kDamageBoxInset      = (1.0f - kDamageBoxScale) * 0.5f;
    static constexpr float kPathRefreshInterval = 0.30f;
    static constexpr float kNavFootRadius       = 18.0f;                        // pé circular para a navegação
    static constexpr float kSightLosRadius      = 25.0f;                        // espessura da linha de visão
    static constexpr float kSpotSoundCooldown   = 6.0f;
    static constexpr float kHuntScreamInterval  = 2.6f;
    static constexpr float kStuckMoveEpsilon    = 6.0f;                         // andou menos que isto = parado
    static constexpr float kStuckTime           = 2.5f;                         // parado por isto = preso
    static constexpr float kSpeedUnstuck        = 130.0f;
    static constexpr float kUnstuckMinTime      = 1.0f;
    static constexpr float kUnstuckMaxTime      = 3.5f;

    static constexpr float kPatrolWaitTime        = 0.5f;                       // pausa ao chegar num ponto
    static constexpr float kInvestigateNoPathTime = 5.0f;                       // sem caminho por isto → volta a patrulhar
    static constexpr float kChaseLostSightTime    = 2.0f;                       // sem ver por isto → investiga
    static constexpr float kHuntMaxTime           = 8.0f;
    static constexpr float kFleeReturnToChaseTime = 4.0f;                       // fugindo com memória → volta a perseguir
    static constexpr float kFleeOutOfLightTime    = 1.5f;                       // fora da luz por isto → patrulha

    static constexpr float kSabotageMaxTime       = 26.0f;                      // desiste da janela depois disto
    static constexpr float kSabotageReachDist     = 480.0f;                     // perto assim já abre
    static constexpr float kSabotageGiveUpDist    = 540.0f;
    static constexpr float kSabotageApproachDist  = 440.0f;                     // pontos ao redor da janela (fora do tapete)
    static constexpr float kWindowRestNormal      = 15.0f;                      // descanso do radar ao desistir
    static constexpr float kWindowRestStrategic   = 4.0f;                       // idem, no cerco

    static constexpr float kPathDirectFallback = 150.0f;                        // sem A*, anda reto se o destino estiver perto assim
    static constexpr float kPathSkipFirstNode  = 64.0f;                         // já em cima do 1º nó: pula
    static constexpr float kPathNodeReached    = 12.0f;

    // ── Preparação ───────────────────────────────────────────────────────────
    void LoadTuning();                                   // config/monster.json → tuning
    void ApplyAnimFrame();

    // ── Etapas do Update ─────────────────────────────────────────────────────
    void TickTimers(float dt);
    void LogDebugState(float dt);                        // estado no console 1x/s (debugMode)
    bool UpdateLightSensor();                            // true = entrou em FLEE_LIGHT
    bool UpdateSightSensor();                            // true = viu um irmão
    void UpdateCampMode(float dt, bool sawBrother);      // irmão escondido tempo demais → cerco
    void UpdateBoredom(float dt, bool sawBrother);       // rondando o mesmo lugar → desiste
    void UpdateWindowRadar();                            // escolhe janela para abrir
    void UpdateStuckDetection(float dt);                 // parado tempo demais → UNSTUCK
    void UpdateFootstepsAndEcho(float dt);               // animação, passos, eco e virar o sprite

    // ── Estados ──────────────────────────────────────────────────────────────
    void TransitionTo(MonsterState next);
    void UpdatePatrol(float dt);
    void UpdateInvestigate(float dt);
    void UpdateChase(float dt);
    void UpdateHunt(float dt);
    void UpdateFleeLight(float dt);
    void UpdateUnstuck(float dt);
    void UpdateSabotageWindow(float dt);
    void PickNextPatrolPoint();
    void GiveUpWindow(float radarRest);                  // solta a janela e volta a patrulhar

    // ── Sensores ─────────────────────────────────────────────────────────────
    void CheckDamageCollision();
    bool CanSeeLitBrother(Vec2& outPos) const;
    bool IsWorldPosInAnyLight(Vec2 worldPos, float extraRadius = 0.0f) const;
    bool IsSelfInLight() const;
    bool FindNearestLight(Vec2& outLightPos) const;      // luz mais próxima (mapa ou luz de mão)
    Window* FindNearbyClosedWindow();
    static bool AnyBrotherHidden();

    // ── Caminho ──────────────────────────────────────────────────────────────
    void RequestPath(Vec2 destination);
    void MoveAlongPath(float dt, float speed);
    bool HasReachedTarget() const;
    bool HasNoPath() const;

    // ── Telemetria: oscilação de estado ──────────────────────────────────────
    // Trocas A→B→A em menos de 0,4 s são a condição tremendo na fronteira: em vez
    // de uma linha por troca, conta e grava UMA linha "monster_flap".
    void FlushStateFlap();

    // ── Estado ───────────────────────────────────────────────────────────────
    Tuning tuning;
    MonsterState state = MonsterState::PATROL;
    float stateTimer = 0.0f;
    float moveSpeed = 0.0f;

    std::vector<Vec2> currentPath;
    int   pathStep = 0;
    float pathRefreshTimer = 0.0f;

    Vec2  lastKnownPlayerPos;
    bool  hasMemory = false;
    float memoryDecayTimer = 0.0f;

    std::vector<Vec2> patrolPoints;
    int  patrolIndex = 0;
    bool patrolRandom = true;

    Window* targetWindow = nullptr;
    float windowRadarTimer = 0.0f;
    float postSabotageIdleTimer = 0.0f;
    bool  strategicMode = false;                         // modo cerco
    float campTimer = 0.0f;

    Vec2  boredAnchor{0.0f, 0.0f};
    float boredTimer = 0.0f;

    Vec2  fleeLightPos;                                  // lugar a evitar na patrulha
    float fleeLightAvoidTimer = 0.0f;
    float chaseGraceTimer = 0.0f;
    bool  firstChaseGraceGiven = false;
    float chaseNoSightTimer = 0.0f;
    bool  lightDecisionMade = false;                     // já sorteou se ignora esta luz
    bool  lightIgnored = false;

    Vec2  stuckRefPos;
    float stuckTimer = 0.0f;

    float damageCooldown = 0.0f;
    float noiseCooldownTimer = 0.0f;
    float spotSoundCooldown = 0.0f;
    float huntScreamTimer = 0.0f;
    float visionRevealTimer = 0.0f;
    float echoRevealTimer = 0.0f;
    float echoDistAccum = 0.0f;                          // distância andada desde a última onda
    float debugLogTimer = 0.0f;

    int   animFrame = 0;
    float animDistAccum = 0.0f;
    bool  facingLeft = false;
    float lastCenterX = 0.0f;

    MonsterState telemetryPrevState = MonsterState::PATROL;
    double telemetryLastTransitionAt = -1.0;
    int    telemetryFlapCount = 0;
    double telemetryFlapStartedAt = 0.0;
    MonsterState telemetryFlapA = MonsterState::PATROL;
    MonsterState telemetryFlapB = MonsterState::PATROL;
};

#endif