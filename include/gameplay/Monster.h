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
        PATROL,                                         // Andando aleatoriamente pelos pontos de spawn/waypoints
        INVESTIGATE,                                    // Indo até a última posição onde viu a luz / ouviu algo
        HUNT,                                           // Tocou em um irmão no escuro - perseguição agressiva ás cegas
        CHASE,                                          // Viu os irmãos e está correndo atrás (ou tocou neles no escuro)
        FLEE_LIGHT,                                     // Entrou em área muito iluminada — recua
        SABOTAGE_WINDOW,                                // Vai dar prioridade a abrir a janela pra apagar as velas
        UNSTUCK                                         // Preso na geometria: vira "fantasma" (sem dano) e sai andando p/ longe do jogador
    };

    Monster(GameObject& associated);
    ~Monster();

    void Start() override;  
    void Update(float dt) override;
    void Render() override;
    void NotifyCollision(GameObject& other) override;
    
    void AddPatrolPoint(Vec2 point);                    // Chamado pelo SpawnFactory / LevelManager para injetar waypoints do Tiled
    void NotifyNoise(Vec2 noiseWorldPos);               // #2 Chamado ao empurrar caixas/barris: o monstro ouve e investiga

    // Getter para o Irmãozinho renderizar a silhueta
    float GetVisionRevealTimer() const { return visionRevealTimer; }
    bool isVisibleToLittleBrother() const {return visionRevealTimer > 0.0f; }
    MonsterState GetState() const { return state; } 

    void ActivateVision(float duration) { visionRevealTimer = duration;}

    GameObject& GetAssociated() { return associated; }

    static constexpr float kVisionRevealDuration = 0.3f;  // Mesmo valor do visionRevealTimer

private:

    void CheckDamageCollision();

    // ── Animação de "andar" (5 frames em arquivos separados, trocados por
    // distância percorrida para não escorregar) ───────────────────────────────
    void  ApplyAnimFrame();
    int   animFrame     = 0;
    float animDistAccum = 0.0f;
    static constexpr int   kAnimFrameCount = 5;
    static constexpr float kAnimPxPerFrame = 32.0f;   // avança 1 frame a cada 32px andados

    // Espelhamento: a arte base olha para a DIREITA; ao andar para a esquerda o
    // sprite é espelhado. Mantém a última direção quando parado.
    bool  facingLeft   = false;
    float lastCenterX  = 0.0f;

    // ── Máquina de estados ────────────────────────────────────────────────────
    MonsterState state = MonsterState::PATROL;
 
    void TransitionTo(MonsterState next);
 
    void UpdatePatrol      (float dt);
    void UpdateInvestigate (float dt);
    void UpdateChase       (float dt);
    void UpdateHunt        (float dt);
    void UpdateFleeLigth   (float dt);
    void UpdateUnstuck     (float dt);   // recuperação de "preso na geometria"
 
    // ── Sensores ──────────────────────────────────────────────────────────────
    // Retorna true se qualquer irmão estiver iluminado E dentro do raio de visão
    bool CanSeeLitBrother(Vec2& outPos) const;
 
    // Retorna true se a posição mundana está em área iluminada demais para o monstro
    bool IsWorldPosInAnyLight(Vec2 worldPos, float extraRadius = 0.0f) const;
 
    // Retorna true se o monstro está sobre tile iluminado demais (usa sua própria pos)
    bool IsSelfInLight() const;
 
    // ── Movimento / Pathfinding ───────────────────────────────────────────────
    void MoveAlongPath(float dt, float speed);
    void RequestPath(Vec2 destination);
    bool HasReachedTarget() const;   // tinha caminho e o consumiu até o fim
    bool HasNoPath() const;          // não há caminho usável (falha do pathfinder) — 1.7
 
    std::vector<Vec2> currentPath;
    int               pathStep = 0;
 
    Vec2  targetPos;                                    // Destino atual
    float moveSpeed = 0.0f;                             // Interpolado por estado
 
    // ── Parâmetros de velocidade por estado ───────────────────────────────────
    // NÃO são mais constexpr: viram membros ajustáveis por Recursos/data/monster.json
    // (LoadTuning() sobrescreve os defaults abaixo). Todos os usos continuam iguais.
    float kSpeedPatrol      = 85.0f;
    float kSpeedInvestigate = 120.0f;
    float kSpeedChase       = 170.0f;
    float kSpeedHunt        = 185.0f;  // Mais rápido — ele está furioso
    float kSpeedFlee        = 220.0f;  // Foge MAIS RÁPIDO que persegue — realista e deixa o jogador sentir que a luz funciona
    void LoadTuning();                 // lê Recursos/data/monster.json (defaults acima)
 
    // ── Timers e memória ──────────────────────────────────────────────────────
    float stateTimer        = 0.0f;                     // Tempo geral no estado atual
    float pathRefreshTimer  = 0.0f;                     // Throttle de recálculo de path
 
    // Posição da última vez que viu luz (vai ficando "menos certa" com o tempo)
    Vec2  lastKnownPlayerPos;
    float memoryDecayTimer  = 0.0f;                     // Conta quanto faz que perdeu visão
    bool  hasMemory         = false;
 
    // Tempo que os irmãos estão escondidos (sem mais campana no armário). Ao passar
    // de kCampMaxTime, dispara o modo estratégico (cerco às janelas).
    float campTimer         = 0.0f;
    float kCampMaxTime = 18.0f;        // Segundos escondidos até o cerco às janelas (monster.json)

    // Timer para o irmãozinho revelar a silhueta
    float visionRevealTimer = 0.0f;

    // Dano por toque (kDamageCooldownTime / kSanityDamage* ajustáveis via monster.json)
    float damageCooldown = 0.0f;
    float kDamageCooldownTime = 1.5f;
    float kSanityDamageDark = 80.0f;  // no escuro — perigoso
    float kSanityDamageLit  = 50.0f;  // na luz — susto
    // Hitbox de dano: fração da box do monstro. Reduzida 30% (0.85 → 0.595).
    static constexpr float kDamageBoxScale = 0.595f;
    static constexpr float kDamageBoxInset = (1.0f - kDamageBoxScale) * 0.5f;

    // Memória do foco de luz que causou a última fuga. Por um tempo, a patrulha
    // EVITA waypoints perto dele — assim o monstro não volta direto para a mesma
    // luz e entra no ciclo PATROL→FLEE_LIGHT→PATROL→mesma luz. (monster.json)
    Vec2  fleeLightPos;
    float fleeLightAvoidTimer = 0.0f;
    float kFleeLightAvoidTime   = 12.0f;
    float kFleeLightAvoidRadius = 520.0f;
    float kFleeDistance         = 500.0f;  // quão longe recua ao fugir da luz (monster.json)
    float kFleeLightRadiusFraction = 0.85f; // fração do raio da luz que já faz o monstro fugir (era 0.60) (monster.json)

    // ── Patrulha ──────────────────────────────────────────────────────────────
    std::vector<Vec2> patrolPoints;
    int  patrolIndex  = 0;
    bool patrolRandom = true;  // true = waypoint aleatório; false = sequencial
 
    void PickNextPatrolPoint();
 
    // ── Limites de visão ──────────────────────────────────────────────────────
    // Ajustáveis via monster.json (ver LoadTuning): raio de visão, limiar de luz,
    // memória, radar de janelas.
    float kSightRadius          = 800.0f;  // Raio máximo de visão na luz
    float kIlluminationThreshold = 0.20f;  // Mínimo de luz para detectar
    float kMemoryDecayTime       = 10.0f;  // Segundos até esquecer posição
    float kWindowRadarInterval   = 6.0f;    // procura janelas com mais frequência
    float kWindowRadarRange      = 2900.0f; // alcance do radar de janelas (sabota de longe)
    float kSabotageDelay         = 4.0f;    // #5 pausa (patrulha) imposta depois de abrir uma janela, antes da próxima
    float kPostSabotageIdle      = 8.0f;    // depois de abrir a janela, fica PARADO estes segundos antes de voltar a patrulhar
    static constexpr float kFleeThreshold         = 0.70f;  // Iluminação que faz ele recuar
    static constexpr float kPathRefreshInterval   = 0.30f;  // Segundos entre recálculos de path
    static constexpr float kNavFootRadius         = 18.0f;  // footprint de navegação BEM menor: passa por corredores/quinas sem travar (o dano NÃO usa isto)
    static constexpr float kSightLosRadius        = 25.0f;  // raio fino p/ checar parede na linha de visão (1.6)

    float windowRadarTimer = 0.0f;                          // Timer separado para o radar de janelas (não compartilhado com pathfinding)
    float postSabotageIdleTimer = 0.0f;                     // >0: monstro fica parado após abrir uma janela (conta regressiva)

    // ── Audição de ruído (empurrar caixas/barris) ─────────────────────────────
    // #2: quando o jogador empurra uma caixa/barril perto, o monstro "ouve" e vai
    // investigar. Throttle p/ não re-disparar a cada frame de empurrão. (monster.json)
    float noiseCooldownTimer = 0.0f;
    float kNoiseHearRadius   = 950.0f;   // só ouve empurrões dentro deste raio
    float kNoiseCooldown     = 1.5f;     // intervalo mínimo entre reações a ruído

    // ── Variáveis de Sabotagem ────────────────────────────────────────────────
    Window* targetWindow = nullptr;
    Window* FindNearbyClosedWindow();
    void UpdateSabotageWindow(float dt);

    // ── Modo estratégico ──────────────────────────────────────────────────────
    // Depois que os irmãos ficam escondidos por kCampMaxTime (contado em Update,
    // sem campana no armário), o monstro passa a caçar TODAS as janelas do andar
    // até abri-las — o que apaga as velas e força o jogador a fugir do nível.
    bool strategicMode = false;
    static constexpr float kStrategicRadarInterval = 1.0f;  // reavalia janela alvo mais rápido
    static bool AnyBrotherHidden();                          // algum irmão está escondido no armário?

    // ── Luz durante CHASE/HUNT ─────────────────────────────────────────────────
    // Ao ENTRAR em CHASE/HUNT há uma GRAÇA curta em que ignora a luz (não foge só
    // por avistar o jogador numa área iluminada). Passada a graça, cada vez que
    // PISA na luz há uma CHANCE de ignorá-la (maior caçando); senão, foge. Fora de
    // CHASE/HUNT (patrulha/investiga/sabota) SEMPRE foge da luz. (monster.json)
    float chaseGraceTimer = 0.0f;
    float kChaseGraceDuration     = 1.5f;   // graça de entrada (ignora luz)
    // Na 1ª perseguição (CHASE) a graça é garantida por mais tempo, para o monstro
    // COMPROMETER-SE com a caçada e não parar logo de cara. Depois, volta ao normal.
    float kFirstChaseGraceDuration = 5.0f;
    bool  firstChaseGraceGiven = false;
    float kChaseIgnoreLightChance = 0.50f;  // CHASE: chance de ignorar a luz
    float kHuntIgnoreLightChance  = 0.85f;  // HUNT: chance (maior) de ignorar a luz
    bool  lightDecisionMade = false;        // já rolou a decisão desta exposição à luz?
    bool  lightIgnored      = false;        // decidiu ignorar a luz atual?
    float chaseNoSightTimer = 0.0f;
    float spotSoundCooldown = 0.0f;
    static constexpr float kSpotSoundCooldown = 6.0f;
    // Caçada barulhenta: grita periodicamente enquanto está em HUNT (não só na entrada).
    float huntScreamTimer = 0.0f;
    static constexpr float kHuntScreamInterval = 2.6f;

    // ── Recuperação de "preso na geometria" ───────────────────────────────────
    Vec2  stuckRefPos;                                  // última posição em que houve progresso
    float stuckTimer = 0.0f;                            // tempo sem progresso num estado ativo
    static constexpr float kStuckMoveEpsilon = 6.0f;    // px de avanço p/ NÃO contar como preso
    static constexpr float kStuckTime        = 2.5f;    // s parado (estado ativo) → considerado preso
    static constexpr float kSpeedUnstuck     = 130.0f;  // velocidade "fantasma" saindo da parede
    static constexpr float kUnstuckMinTime   = 1.0f;    // fica fantasma por pelo menos isto
    static constexpr float kUnstuckMaxTime   = 3.5f;    // teto: retoma à força depois disto
};

#endif