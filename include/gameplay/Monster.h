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
        CAMP_CLOSET,                                    // Viu os irmãos entrarem no armário e está esperando na frente
        FLEE_LIGHT,                                     // Entrou em área muito iluminada — recua
        LURK,                                           // Ronda nas bordas da luz, esperando ela sair/apagar
        SABOTAGE_WINDOW                                 // Vai dar prioridade a abrir a janela pra apagar as velas
    };

    Monster(GameObject& associated);
    ~Monster();

    void Start() override;  
    void Update(float dt) override;
    void Render() override;
    void NotifyCollision(GameObject& other) override;
    
    void AddPatrolPoint(Vec2 point);                    // Chamado pelo SpawnFactory / LevelManager para injetar waypoints do Tiled
    void NotifyClosetOccupied(Vec2 closetWorldPos);     // Chamado pelo Closet quando os irmãos entram nele

    // Getter para o Irmãozinho renderizar a silhueta
    float GetVisionRevealTimer() const { return visionRevealTimer; }
    bool isVisibleToLittleBrother() const {return visionRevealTimer > 0.0f; }
    MonsterState GetState() const { return state; } 

    void ActivateVision(float duration) { visionRevealTimer = duration;}

    GameObject& GetAssociated() { return associated; }

    static constexpr float kVisionRevealDuration = 0.3f;  // Mesmo valor do visionRevealTimer

private:

    void CheckDamageCollision();

    // ── Máquina de estados ────────────────────────────────────────────────────
    MonsterState state = MonsterState::PATROL;
 
    void TransitionTo(MonsterState next);
 
    void UpdatePatrol      (float dt);
    void UpdateInvestigate (float dt);
    void UpdateChase       (float dt);
    void UpdateHunt        (float dt);
    void UpdateCampCloset  (float dt);
    void UpdateFleeLigth   (float dt);
    void UpdateLurk        (float dt);
 
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
    static constexpr float kSpeedPatrol      = 85.0f;
    static constexpr float kSpeedInvestigate = 120.0f;
    static constexpr float kSpeedChase       = 170.0f;
    static constexpr float kSpeedHunt        = 185.0f;  // Mais rápido — ele está furioso
    static constexpr float kSpeedFlee        = 220.0f;  // Foge MAIS RÁPIDO que persegue — realista e deixa o jogador sentir que a luz funciona
 
    // ── Timers e memória ──────────────────────────────────────────────────────
    float stateTimer        = 0.0f;                     // Tempo geral no estado atual
    float pathRefreshTimer  = 0.0f;                     // Throttle de recálculo de path
 
    // Posição da última vez que viu luz (vai ficando "menos certa" com o tempo)
    Vec2  lastKnownPlayerPos;
    float memoryDecayTimer  = 0.0f;                     // Conta quanto faz que perdeu visão
    bool  hasMemory         = false;
 
    // Posição do armário onde os irmãos entraram
    Vec2  closetTargetPos;
    float campTimer         = 0.0f;
    static constexpr float kCampMaxTime = 18.0f;        // Segundos que ele fica de campana
 
    // Timer para o irmãozinho revelar a silhueta
    float visionRevealTimer = 0.0f;
 
    // Dano por toque
    float damageCooldown = 0.0f;
    static constexpr float kDamageCooldownTime = 1.5f;
    static constexpr float kSanityDamageDark = 80.0f;  // no escuro — perigoso
    static constexpr float kSanityDamageLit  = 50.0f;  // na luz — susto

    // Propriedades do LURK
    float lurkTimer = 0.0f;
    static constexpr float kLurkMaxTime = 12.0f; // Desiste de rondar após esse tempo
    static constexpr float kLurkRepositionInterval = 1.5f; // A cada quanto reavalia posição
 
    // ── Patrulha ──────────────────────────────────────────────────────────────
    std::vector<Vec2> patrolPoints;
    int  patrolIndex  = 0;
    bool patrolRandom = true;  // true = waypoint aleatório; false = sequencial
 
    void PickNextPatrolPoint();
 
    // ── Limites de visão ──────────────────────────────────────────────────────
    static constexpr float kSightRadius          = 800.0f;  // Raio máximo de visão na luz
    static constexpr float kIlluminationThreshold = 0.20f;  // Mínimo de luz para detectar
    static constexpr float kFleeThreshold         = 0.70f;  // Iluminação que faz ele recuar
    static constexpr float kMemoryDecayTime       = 10.0f;  // Segundos até esquecer posição
    static constexpr float kPathRefreshInterval   = 0.30f;  // Segundos entre recálculos de path
    static constexpr float kNavFootRadius         = 48.0f;
    static constexpr float kWindowRadarInterval   = 10.0f;   // Timer separado para janelas 
    static constexpr float kSightLosRadius        = 25.0f;  // raio fino p/ checar parede na linha de visão (1.6)

    float windowRadarTimer = 0.0f;                          // Timer separado para o radar de janelas (não compartilhado com pathfinding)

    // ── Variáveis de Sabotagem ────────────────────────────────────────────────
    Window* targetWindow = nullptr;
    Window* FindNearbyClosedWindow();
    void UpdateSabotageWindow(float dt);

    // ── Imunidade a luz ────────────────────────────────────────────────
    float chaseGraceTimer = 0.0f;
    static constexpr float kChaseGraceDuration = 3.0f;
    float chaseNoSightTimer = 0.0f;
    float spotSoundCooldown = 0.0f;
    static constexpr float kSpotSoundCooldown = 6.0f;
};

#endif