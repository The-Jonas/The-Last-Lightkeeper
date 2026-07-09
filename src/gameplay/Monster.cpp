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
// Frames do ciclo de "andar" do monstro (arquivos separados, mesma dimensão).
const char* kMonsterFramePaths[] = {
    "Recursos/img/personagens/monstro/monstro_f1.png",
    "Recursos/img/personagens/monstro/monstro_f2.png",
    "Recursos/img/personagens/monstro/monstro_f3.png",
    "Recursos/img/personagens/monstro/monstro_f4.png",
    "Recursos/img/personagens/monstro/monstro_f5.png",
};
} // namespace

Monster::Monster(GameObject& associated): Component(associated) {}

Monster::~Monster() {
    GameSfx::StopMonsterFootsteps();
}

void Monster::LoadTuning() {
    std::ifstream f("Recursos/data/monster.json");
    if (!f.is_open()) {
        return;   // sem arquivo → mantém os defaults do header
    }
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
        rd("flee_light_avoid_time", kFleeLightAvoidTime);
        rd("flee_light_avoid_radius", kFleeLightAvoidRadius);
        rd("flee_light_radius_fraction", kFleeLightRadiusFraction);
        rd("chase_grace_duration", kChaseGraceDuration);
        rd("chase_ignore_light_chance", kChaseIgnoreLightChance);
        rd("hunt_ignore_light_chance", kHuntIgnoreLightChance);
        rd("flee_distance", kFleeDistance);
        rd("sanity_damage_dark", kSanityDamageDark);
        rd("sanity_damage_lit", kSanityDamageLit);
        rd("damage_cooldown_time", kDamageCooldownTime);
    } catch (const std::exception&) {
        // JSON inválido → mantém os defaults (não quebra o jogo).
    }
}

void Monster::Start() {
    LoadTuning();   // sobrescreve os parâmetros pelo Recursos/data/monster.json (se existir)
    // Pré-carrega os 5 frames (cache) para as trocas em jogo não engasgarem.
    for (const char* p : kMonsterFramePaths) {
        Resources::GetImage(p);
    }
    SpriteRenderer* sr = new SpriteRenderer(associated, kMonsterFramePaths[0]);
    associated.AddComponent(sr);
    associated.box.y -= associated.box.h;   // ancora os "pés" na posição de spawn
    TransitionTo(MonsterState::PATROL);
}

void Monster::ApplyAnimFrame() {
    SpriteRenderer* sr = associated.GetComponent<SpriteRenderer>();
    if (!sr) return;
    sr->Open(kMonsterFramePaths[animFrame]);   // troca a textura do frame atual
    sr->SetFrameCount(1, 1);
    sr->SetFrame(0);
}

// ─────────────────────────────────────────────────────────────────────────────
//  UPDATE PRINCIPAL
// ─────────────────────────────────────────────────────────────────────────────
void Monster::Update(float dt) {
    stateTimer       += dt;
    pathRefreshTimer += dt;
    windowRadarTimer += dt;  // timer separado para radar de janelas
    if (spotSoundCooldown > 0.0f) spotSoundCooldown -= dt;
    if (huntScreamTimer  > 0.0f) huntScreamTimer  -= dt;
    if (noiseCooldownTimer > 0.0f) noiseCooldownTimer -= dt;

    // DEBUG: imprime estado atual a cada ~1s (só em debugMode — nada de spam no console em release).
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
    if (fleeLightAvoidTimer > 0.0f) fleeLightAvoidTimer -= dt;

    if (hasMemory) {
        memoryDecayTimer += dt;
        if (memoryDecayTimer >= kMemoryDecayTime) {
            hasMemory = false;
        }
    }

    // ── 0. Recuperação de "preso na geometria" ───────────────────────────────
    // Enquanto está saindo da parede, ignora TODOS os sensores (luz/visão/janela)
    // e não causa dano — só atravessa a geometria pra longe do jogador e retoma.
    if (state == MonsterState::UNSTUCK) {
        UpdateUnstuck(dt);
        return;
    }

    // ── 1. Sensor de luz — prioridade máxima ─────────────────────────────────
    // Fora de CHASE/HUNT: SEMPRE foge da luz. Em CHASE/HUNT: graça de entrada +
    // chance (rolada UMA vez por exposição) de ignorar a luz e seguir na pressão.
    if (state != MonsterState::FLEE_LIGHT) {
        const bool inLight = IsSelfInLight();
        const bool inGrace = chaseGraceTimer > 0.0f;

        if (!inLight || inGrace) {
            // Fora da luz ou protegido pela graça → zera a decisão (re-rola na próxima).
            lightDecisionMade = false;
            lightIgnored      = false;
        } else if (!lightDecisionMade) {
            // Acabou de pisar na luz (fora da graça) → decide UMA vez se ignora.
            lightDecisionMade = true;
            float ignoreChance = 0.0f;   // patrulha/investiga/sabota: nunca ignora
            if (state == MonsterState::CHASE)     ignoreChance = kChaseIgnoreLightChance;
            else if (state == MonsterState::HUNT) ignoreChance = kHuntIgnoreLightChance;
            lightIgnored = (static_cast<float>(rand() % 1000) / 1000.0f) < ignoreChance;
        }

        if (inLight && !inGrace && !lightIgnored) {
            TransitionTo(MonsterState::FLEE_LIGHT);
            return;
        }
    }

    if (chaseGraceTimer > 0.0f) chaseGraceTimer -= dt;

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

    // ── 2.5. Escalada de cerco (sem mais campana no armário) ─────────────────
    // Enquanto os irmãos ficam escondidos, conta o tempo; passado kCampMaxTime,
    // entra no modo estratégico (abre TODAS as janelas → apaga as velas → força
    // eles a sair). O monstro NÃO fica mais parado na frente do armário.
    if (AnyBrotherHidden()) {
        if (!strategicMode) {
            campTimer += dt;
            if (campTimer >= kCampMaxTime) {
                strategicMode = true;
                windowRadarTimer = kStrategicRadarInterval;   // procura a 1ª janela já
            }
        }
    } else {
        campTimer = 0.0f;
    }

    // ── 3. Radar de janelas — timer PRÓPRIO, não interfere com pathfinding ───
    // Normal: só quando livre (sem memória) e ocioso, no intervalo longo.
    // Estratégico: ignora memória e reavalia num intervalo curto — o monstro
    // vai abrindo uma janela atrás da outra até dominar o andar.
    // Respeita uma pista fresca dos irmãos (hasMemory): só caça janelas quando não
    // está no encalço. No modo estratégico o intervalo é curto (caça relentless).
    if ((state == MonsterState::PATROL || state == MonsterState::INVESTIGATE) && !hasMemory) {
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

    // ── 4. Despacha para o sub-update ─────────────────────────────────────────
    switch (state) {
        case MonsterState::PATROL:          UpdatePatrol(dt);         break;
        case MonsterState::INVESTIGATE:     UpdateInvestigate(dt);    break;
        case MonsterState::CHASE:           UpdateChase(dt);          break;
        case MonsterState::HUNT:            UpdateHunt(dt);           break;
        case MonsterState::FLEE_LIGHT:      UpdateFleeLigth(dt);      break;
        case MonsterState::SABOTAGE_WINDOW: UpdateSabotageWindow(dt); break;
        case MonsterState::UNSTUCK:         /* tratado no topo do Update */ break;
    }

    // ── 5. Detecção de "preso na geometria" ───────────────────────────────────
    // Nos estados em que ele DEVE estar se movendo, se a posição não avança por
    // kStuckTime segundos (ex.: A* não acha rota porque ele está dentro de uma
    // parede), entra em UNSTUCK pra se recuperar.
    {
        const bool movingState =
            state == MonsterState::PATROL || state == MonsterState::INVESTIGATE ||
            state == MonsterState::CHASE  || state == MonsterState::HUNT;
        if (movingState) {
            if (associated.box.Center().Distance(stuckRefPos) > kStuckMoveEpsilon) {
                stuckRefPos = associated.box.Center();
                stuckTimer  = 0.0f;
            } else {
                stuckTimer += dt;
                if (stuckTimer >= kStuckTime) {
                    TransitionTo(MonsterState::UNSTUCK);
                }
            }
        } else {
            stuckRefPos = associated.box.Center();
            stuckTimer  = 0.0f;
        }
    }

    {
        Vec2 myPos = associated.box.Center();
        Vec2 plPos = Character::player 
            ? Character::player->GetAssociated().box.Center() 
            : myPos;
    
        // Só toca o som se realmente estiver se movendo (tem path e não chegou)
        float realSpeed = (!currentPath.empty() && pathStep < static_cast<int>(currentPath.size()))
            ? moveSpeed
            : 0.0f;

        const bool fleeing = (state == MonsterState::FLEE_LIGHT);

        // Anima o ciclo de "andar" enquanto se move, avançando frames pela distância
        // percorrida (assim a cadência das pernas acompanha a velocidade do estado —
        // patrulha lenta, caçada frenética — sem escorregar os pés). Ao FUGIR da luz,
        // as pernas ficam ainda mais frenéticas (px/frame menor).
        if (realSpeed > 0.0f) {
            const float pxPerFrame = fleeing ? kAnimPxPerFrame * 0.6f : kAnimPxPerFrame;
            const float stepsMaxDist = 1900.0f + moveSpeed * 4.0f;
            animDistAccum += realSpeed * dt;
            bool changed = false;
            // Um passo POR FRAME de animação — o PlayMonsterStep fica DENTRO do laço,
            // então mesmo correndo (vários frames num tick) cada avanço toca um passo,
            // sem pular nenhum. Usa só os 5 mns_step_*.mp3 (pool rotativo no GameSfx).
            while (animDistAccum >= pxPerFrame) {
                animDistAccum -= pxPerFrame;
                animFrame = (animFrame + 1) % kAnimFrameCount;
                changed = true;
                GameSfx::PlayMonsterStep(animFrame, myPos.x, myPos.y, plPos.x, plPos.y, stepsMaxDist);
            }
            if (changed) {
                ApplyAnimFrame();
            }
        }

        // Rangidos de madeira esporádicos sob o peso do monstro em movimento
        // (temporizador interno; só soa quando ele realmente anda).
        GameSfx::UpdateMonsterFootsteps(dt, realSpeed, myPos.x, myPos.y,
                                        plPos.x, plPos.y, fleeing);

        // Espelhamento por direção: vira ao andar para a ESQUERDA (arte base olha
        // para a direita); sem espelho ao andar para a direita. Mantém a última
        // direção quando praticamente parado. Aplicado por último para não ser
        // zerado pelo ApplyAnimFrame.
        const float dx = myPos.x - lastCenterX;
        lastCenterX = myPos.x;
        if (dx < -0.5f) {
            facingLeft = true;
        } else if (dx > 0.5f) {
            facingLeft = false;
        }
        if (SpriteRenderer* sr = associated.GetComponent<SpriteRenderer>()) {
            sr->SetFlip(facingLeft ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
        }
    }

    CheckDamageCollision();
}

void Monster::Render() {
    // Silhueta é desenhada pelo MonsterSilhouette (z=100, acima da escuridão)

#ifdef DEBUG
    // Só desenha as caixas quando a visualização de física/colisão está LIGADA
    // pela tecla [B] (StageState::showMapPhysicsDebug) — antes poluía sempre.
    {
        StageState* dbgStage = Game::TryGetStageState();
        if (!dbgStage || !dbgStage->IsPhysicsDebugOn()) return;
    }

    SDL_Renderer* dbgR = Game::GetInstance().GetRenderer();
    SDL_SetRenderDrawBlendMode(dbgR, SDL_BLENDMODE_BLEND);

    // HURTBOX / caixa de DANO (o que machuca o irmão) — vermelho preenchido.
    SDL_Rect dmgBox = {
        static_cast<int>(associated.box.x + associated.box.w * kDamageBoxInset) - static_cast<int>(Camera::pos.x),
        static_cast<int>(associated.box.y + associated.box.h * kDamageBoxInset) - static_cast<int>(Camera::pos.y),
        static_cast<int>(associated.box.w * kDamageBoxScale),
        static_cast<int>(associated.box.h * kDamageBoxScale)
    };
    SDL_SetRenderDrawColor(dbgR, 255, 0, 0, 100);
    SDL_RenderFillRect(dbgR, &dmgBox);

    // Box física (bounds do sprite) — amarelo.
    SDL_Rect physBox = {
        static_cast<int>(associated.box.x) - static_cast<int>(Camera::pos.x),
        static_cast<int>(associated.box.y) - static_cast<int>(Camera::pos.y),
        static_cast<int>(associated.box.w),
        static_cast<int>(associated.box.h)
    };
    SDL_SetRenderDrawColor(dbgR, 255, 255, 0, 255);
    SDL_RenderDrawRect(dbgR, &physBox);

    // COLISÃO DE NAVEGAÇÃO — CÍRCULO verde de raio kNavFootRadius no centro do
    // monstro. É este círculo (não um retângulo) que o pathfinding usa contra a
    // geometria e os props (círculo × OBB), por isso o monstro DESLIZA nas quinas.
    {
        const Vec2  c   = associated.box.Center();
        const float ccx = c.x - Camera::pos.x;
        const float ccy = c.y - Camera::pos.y;
        const float rad = kNavFootRadius;
        SDL_SetRenderDrawColor(dbgR, 0, 255, 120, 230);
        constexpr int kSegs = 40;
        float px = ccx + rad, py = ccy;
        for (int i = 1; i <= kSegs; ++i) {
            const float a = (static_cast<float>(i) / kSegs) * 2.0f * 3.14159265f;
            const float nx = ccx + std::cos(a) * rad;
            const float ny = ccy + std::sin(a) * rad;
            SDL_RenderDrawLineF(dbgR, px, py, nx, ny);
            px = nx; py = ny;
        }
    }
 
    // Estado atual — texto via círculo colorido no centro
    {
        Uint8 r = 255, g = 255, b = 255;
        switch (state) {
            case MonsterState::PATROL:          r=100; g=255; b=100; break; // verde
            case MonsterState::INVESTIGATE:     r=255; g=200; b=0;   break; // amarelo
            case MonsterState::CHASE:           r=255; g=100; b=0;   break; // laranja
            case MonsterState::HUNT:            r=255; g=0;   b=0;   break; // vermelho
            case MonsterState::FLEE_LIGHT:      r=0;   g=100; b=255; break; // azul
            case MonsterState::SABOTAGE_WINDOW: r=0;   g=255; b=255; break; // ciano
            case MonsterState::UNSTUCK:         r=180; g=180; b=180; break; // cinza (recuperando)
        }
        SDL_Rect stateIndicator = {
            static_cast<int>(associated.box.Center().x) - 5 - static_cast<int>(Camera::pos.x),
            static_cast<int>(associated.box.Center().y) - 5 - static_cast<int>(Camera::pos.y),
            10, 10
        };
        SDL_SetRenderDrawColor(dbgR, r, g, b, 255);
        SDL_RenderFillRect(dbgR, &stateIndicator);

        // Rótulo de texto "STATE: <estado>" acima da cabeça do monstro, para
        // acompanhar a lógica/comportamento em tempo real (só em build DEBUG).
        const char* stName = "?";
        switch (state) {
            case MonsterState::PATROL:          stName = "PATROL";          break;
            case MonsterState::INVESTIGATE:     stName = "INVESTIGATE";     break;
            case MonsterState::CHASE:           stName = "CHASE";           break;
            case MonsterState::HUNT:            stName = "HUNT";            break;
            case MonsterState::FLEE_LIGHT:      stName = "FLEE_LIGHT";      break;
            case MonsterState::SABOTAGE_WINDOW: stName = "SABOTAGE_WINDOW"; break;
            case MonsterState::UNSTUCK:         stName = "UNSTUCK";         break;
        }
        std::string label = std::string("STATE: ") + stName;
        if (strategicMode) label += "  [STRAT]";
        auto font = Resources::GetFont("Recursos/font/times.ttf", 18);
        if (font) {
            SDL_Color txtCol{r, g, b, 255};   // mesma cor do estado, p/ leitura rápida
            SDL_Surface* sf = TTF_RenderUTF8_Blended(font.get(), label.c_str(), txtCol);
            if (sf) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(dbgR, sf);
                const int tw = sf->w, th = sf->h;
                SDL_FreeSurface(sf);
                if (tex) {
                    SDL_Rect dst{
                        static_cast<int>(associated.box.Center().x - Camera::pos.x) - tw / 2,
                        static_cast<int>(associated.box.y - Camera::pos.y) - th - 8,
                        tw, th
                    };
                    SDL_RenderCopy(dbgR, tex, nullptr, &dst);
                    SDL_DestroyTexture(tex);
                }
            }
        }
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

    bool isActualTransition = (state != next);
 
    state      = next;
    stateTimer = 0.0f;
    currentPath.clear();
    pathStep = 0;

    // Cada estado começa com a janela de "preso" zerada (evita falso-positivo
    // logo na entrada) e a referência de progresso na posição atual.
    stuckTimer  = 0.0f;
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
            chaseGraceTimer   = kChaseGraceDuration;
            chaseNoSightTimer = 0.0f;
            if (isActualTransition && spotSoundCooldown <= 0.0f) {
                GameSfx::PlayMonsterSpot();
                spotSoundCooldown = kSpotSoundCooldown;
            }
            break;

        case MonsterState::HUNT:
            moveSpeed = kSpeedHunt;
            chaseGraceTimer = kChaseGraceDuration;   // graça de entrada também ao caçar
            if (isActualTransition) {
                GameSfx::PlayMonsterScream();
                huntScreamTimer = kHuntScreamInterval;   // agenda os próximos gritos da caçada
            }
            break;

        case MonsterState::UNSTUCK:
            moveSpeed = kSpeedUnstuck;
            GameSfx::StopMonsterFootsteps();   // fantasma: sem passos enquanto atravessa
            break;

        case MonsterState::FLEE_LIGHT: {
            moveSpeed = kSpeedFlee * 2.0f;  // fugindo da luz: DOBRO da velocidade (frenético)
            GameSfx::StopMonsterFootsteps();

            StageState* stageFlee = Game::TryGetStageState();
            Vec2  myPos           = associated.box.Center();
            Vec2  fleeDir(0.0f, 0.0f);
            float closestThreatDist = 1e9f;

            Vec2 threatPos = myPos;   // foco de luz mais próximo (a ser lembrado)
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
                Vec2  torchPos;
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

            // Lembra desse foco de luz p/ a patrulha evitá-lo por um tempo (quebra
            // o ciclo de voltar direto para a mesma luz depois de fugir).
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
            pathRefreshTimer = kPathRefreshInterval; // Força o A* a calcular a rota IMEDIATAMENTE!
            break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  COLISÃO DE DANO
// ─────────────────────────────────────────────────────────────────────────────
void Monster::CheckDamageCollision() {
    if (state == MonsterState::UNSTUCK) return;   // hitbox de dano removida durante a recuperação
    if (damageCooldown > 0.0f) return;
    {
        StageState* s = Game::TryGetStageState();
        if (s && s->IsMonsterBlindDebug()) return;   // debug: não causa dano ao jogador invisível
    }

    SDL_Rect dmgBox = {
        static_cast<int>(associated.box.x + associated.box.w * kDamageBoxInset),
        static_cast<int>(associated.box.y + associated.box.h * kDamageBoxInset),
        static_cast<int>(associated.box.w * kDamageBoxScale),
        static_cast<int>(associated.box.h * kDamageBoxScale)
    };

    auto CheckPlayerHit = [&](Character* c) -> bool {
        if (!c) return false;
        // Não acerta quem está escondido no armário (flag confiável) nem quem está
        // no meio de uma interação.
        if (c->isHidden || c->currentState == Character::ActionState::INTERACTING) return false;
        // HITBOX do jogador agora é um CÍRCULO (corpo), testado contra a caixa de
        // dano (AABB) do monstro — acerto redondo, não retangular.
        const Vec2  pc = c->GetHitCircleCenter();
        const float pr = c->GetHitCircleRadius();
        const float closestX = std::max<float>(dmgBox.x, std::min<float>(pc.x, dmgBox.x + dmgBox.w));
        const float closestY = std::max<float>(dmgBox.y, std::min<float>(pc.y, dmgBox.y + dmgBox.h));
        const float dx = pc.x - closestX;
        const float dy = pc.y - closestY;
        return (dx * dx + dy * dy) <= (pr * pr);
    };

    bool hit = false;

    if (CheckPlayerHit(Character::player)) {
        StageState* stage = Game::TryGetStageState();
        bool lit = stage && stage->bigIlluminationLevel >= kIlluminationThreshold;
        Character::player->sanity -= lit ? kSanityDamageLit : kSanityDamageDark;
        if (Character::player->sanity < 0.0f) Character::player->sanity = 0.0f;
        lastKnownPlayerPos = Character::player->GetAssociated().box.Center();
        hit = true;
        if (Game::debugMode) std::cout << "[Monster] Dano no Irmaozao! " 
            << (lit ? kSanityDamageLit : kSanityDamageDark) 
            << (lit ? " (iluminado)" : " (escuro)") << "\n";
    }
    else if (CheckPlayerHit(Character::littleBrother)) {
        StageState* stage = Game::TryGetStageState();
        bool lit = stage && stage->smallIlluminationLevel >= kIlluminationThreshold;
        Character::littleBrother->sanity -= lit ? kSanityDamageLit : kSanityDamageDark;
        if (Character::littleBrother->sanity < 0.0f) Character::littleBrother->sanity = 0.0f;
        lastKnownPlayerPos = Character::littleBrother->GetAssociated().box.Center();
        hit = true;
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
        chaseNoSightTimer  = 0.0f;  // reseta o timer de perda de visão

        if (pathRefreshTimer >= kPathRefreshInterval) {
            pathRefreshTimer = 0.0f;
            RequestPath(seenPos);
        }
        MoveAlongPath(dt, moveSpeed);
    } else {
        // Perdeu a visão — conta o tempo
        chaseNoSightTimer += dt;
        MoveAlongPath(dt, moveSpeed);

        // Após 2s sem ver ou chegou no último ponto → INVESTIGATE
        if (chaseNoSightTimer >= 2.0f || HasReachedTarget() || HasNoPath()) {
            TransitionTo(MonsterState::INVESTIGATE);
        }
    }
}

void Monster::UpdateHunt(float dt) {
    // Se os irmãos se esconderam no armário, o monstro perde o rastro: para de
    // cravar em cima do esconderijo e vai investigar a última posição conhecida.
    if ((Character::player && Character::player->isHidden) ||
        (Character::littleBrother && Character::littleBrother->isHidden)) {
        TransitionTo(MonsterState::INVESTIGATE);
        return;
    }

    // Caçada barulhenta: grita repetidamente enquanto persegue (não só na entrada
    // do estado), deixando a perseguição mais aterrorizante. O PlayMonsterScream
    // não sobrepõe (só toca se o canal estiver livre).
    if (huntScreamTimer <= 0.0f) {
        GameSfx::PlayMonsterScream();
        huntScreamTimer = kHuntScreamInterval;
    }

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

// ─────────────────────────────────────────────────────────────────────────────
//  RECUPERAÇÃO — "PRESO NA GEOMETRIA"
// ─────────────────────────────────────────────────────────────────────────────
// Vira "fantasma": sem dano (CheckDamageCollision sai cedo em UNSTUCK) e sem
// pathfinding/colisão — anda em linha reta PRA LONGE do jogador atravessando a
// parede até voltar a um ponto navegável, e então retoma a patrulha. Nunca fica
// preso aqui: há um teto de tempo e, se ainda estiver enfiado na geometria, ele
// teleporta pro waypoint de patrulha mais próximo (garantidamente navegável).
void Monster::UpdateUnstuck(float dt) {
    StageState* stage = Game::TryGetStageState();
    Vec2 myPos = associated.box.Center();

    // Direção: para LONGE do jogador (afasta-se enquanto se desenrosca).
    Vec2 away(1.0f, 0.0f);
    if (Character::player) {
        away = myPos - Character::player->GetAssociated().box.Center();
    }
    float mag = std::sqrt(away.x * away.x + away.y * away.y);
    if (mag < 0.01f) { away = Vec2(1.0f, 0.0f); mag = 1.0f; }
    away.x /= mag;
    away.y /= mag;

    // Move direto, SEM A* e SEM colisão — só pra sair de dentro da parede.
    associated.box.x += away.x * kSpeedUnstuck * dt;
    associated.box.y += away.y * kSpeedUnstuck * dt;

    myPos = associated.box.Center();
    const bool navigableNow =
        stage && stage->IsWorldPosNavigableFor(myPos, &associated, kNavFootRadius);

    // Retoma quando já voltou a um ponto navegável (após um tempo mínimo) OU
    // quando estoura o teto de tempo.
    if ((navigableNow && stateTimer >= kUnstuckMinTime) || stateTimer >= kUnstuckMaxTime) {
        // Se estourou o tempo ainda dentro da geometria, teleporta pro waypoint de
        // patrulha mais próximo (navegável) — evita recomeçar preso e entrar em loop.
        if (!navigableNow && !patrolPoints.empty()) {
            int   best  = 0;
            float bestD = 1e18f;
            for (size_t i = 0; i < patrolPoints.size(); ++i) {
                float d = myPos.Distance(patrolPoints[i]);
                if (d < bestD) { bestD = d; best = static_cast<int>(i); }
            }
            const Vec2 p = patrolPoints[static_cast<size_t>(best)];
            associated.box.x = p.x - associated.box.w / 2.0f;
            associated.box.y = p.y - associated.box.h / 2.0f;
        }
        damageCooldown = kDamageCooldownTime;   // carência: não machuca no instante que retoma
        TransitionTo(MonsterState::PATROL);      // volta ao normal (sensores/visão religam)
    }
}

void Monster::UpdateFleeLigth(float dt) {
    if (!currentPath.empty()) {
        MoveAlongPath(dt, moveSpeed);
    }

    // Ficou muito tempo fugindo com memória → retribui com investida
    if (stateTimer >= 4.0f && hasMemory) {
        TransitionTo(MonsterState::CHASE);
        return;
    }

    // Saiu da luz com cool-down de pânico de 1.5s → volta a PATRULHAR (sem LURK).
    if (!IsSelfInLight()) {
        if (stateTimer >= 1.5f) {
            TransitionTo(MonsterState::PATROL);
        }
        return; // ainda no cool-down, continua correndo
    }

    // Ainda na luz — reseta o timer de pânico e recalcula fuga
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

            // Tenta 8 ângulos diferentes até achar um ponto navegável fora da luz
            bool found = false;
            constexpr int kNumAngles = 8;

            for (int i = 0; i < kNumAngles && !found; i++) {
                float angleOffset = (static_cast<float>(i) / kNumAngles) * 2.0f * 3.14159265f;
                float angle = std::atan2(fleeDir.y, fleeDir.x) + angleOffset;
                Vec2 dir(std::cos(angle), std::sin(angle));
                Vec2 candidate = myPos + dir * kFleeDistance;

                // Só vai para pontos que não estão iluminados
                if (!IsWorldPosInAnyLight(candidate)) {
                    RequestPath(candidate);
                    found = true;
                }
            }

            // Se não achou nenhum ponto no escuro, foge em qualquer direção navegável
            if (!found) {
                float angle = std::atan2(fleeDir.y, fleeDir.x);
                RequestPath(myPos + Vec2(std::cos(angle), std::sin(angle)) * kFleeDistance);
            }
        }
    }
}

void Monster::NotifyCollision(GameObject& other) {
    // Colisão física tratada pelo LevelManager.
    // Dano é checado em CheckDamageCollision() dentro do Update().
}

// #2: o jogador empurrou uma caixa/barril e fez barulho. Se estiver perto o
// bastante e o monstro não estiver já ocupado (perseguindo/caçando/fugindo/etc.),
// ele memoriza a posição do ruído e vai investigar. Throttled por noiseCooldownTimer.
void Monster::NotifyNoise(Vec2 noiseWorldPos) {
    if (noiseCooldownTimer > 0.0f) return;

    // Só reage a ruído quando está "sem alvo certo": patrulhando, já investigando
    // (redireciona). Perseguição/caçada/fuga/sabotagem/unstuck já têm prioridade
    // e não devem ser interrompidas por um empurrão.
    if (state != MonsterState::PATROL &&
        state != MonsterState::INVESTIGATE) {
        return;
    }

    Vec2 myPos = associated.box.Center();
    if (myPos.Distance(noiseWorldPos) > kNoiseHearRadius) return;

    noiseCooldownTimer = kNoiseCooldown;
    lastKnownPlayerPos = noiseWorldPos;
    hasMemory          = true;
    memoryDecayTimer   = 0.0f;
    if (state != MonsterState::INVESTIGATE) {
        TransitionTo(MonsterState::INVESTIGATE);
    } else {
        RequestPath(noiseWorldPos);   // já investigando: redireciona para o novo ruído
    }
}

bool Monster::AnyBrotherHidden() {
    return (Character::player && Character::player->isHidden) ||
           (Character::littleBrother && Character::littleBrother->isHidden);
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
    if (stage->IsMonsterBlindDebug()) return false;   // debug: invisível para o monstro

    auto Check = [&](Character* c, float illumLevel) -> bool {
        if (!c) return false;
        // Escondido no armário: invisível para o monstro, mesmo que a tocha/relâmpago
        // mantenham a iluminação alta. Sem isso o monstro continua "vendo" e perseguindo.
        if (c->isHidden) return false;
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

    // Fração do raio real da luz que já assusta o monstro. Antes 0.60 (só o
    // núcleo brilhante) fazia ele quase ignorar as luzes; agora maior → foge bem
    // mais cedo/mais vezes. Ajustável via monster.json (flee_light_radius_fraction).
    const float kLightRadiusMultiplier = kFleeLightRadiusFraction;

    Rect myBox = associated.box;
    myBox.x += myBox.w * 0.05f;
    myBox.y += myBox.h * 0.05f;
    myBox.w *= 0.90f;
    myBox.h *= 0.90f;

    for (const auto& light : stage->GetLights()) {
        if (!light.enabled) continue;
        const float radius = light.params.falloffRadiusPx * kLightRadiusMultiplier;
        if (radius <= 0.0f) continue;
        if (DistanceFromRectToPoint(myBox, light.worldPos) < radius) return true;
    }

    Vec2  torchPos;
    float torchRadius = 0.0f;
    if (stage->GetActiveTorchWorldPos(torchPos, torchRadius)) {
        if (DistanceFromRectToPoint(myBox, torchPos) < torchRadius * kLightRadiusMultiplier) 
            return true;
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
        if (Game::debugMode) std::cout << "[PATROL] Sem waypoints!\n";
        return;
    }

    if (patrolPoints.size() == 1) {
        patrolIndex = 0;
        RequestPath(patrolPoints[0]);
        return;
    }

    const int count = static_cast<int>(patrolPoints.size());

    // Se fugiu de uma luz há pouco, evita waypoints perto dela — senão o monstro
    // volta direto para a mesma luz e reentra no ciclo PATROL→FLEE_LIGHT.
    if (fleeLightAvoidTimer > 0.0f) {
        int best = -1;
        float bestDist = -1.0f;
        // 1ª opção: um waypoint aleatório FORA do raio de evitação (e ≠ atual).
        for (int tries = 0; tries < count * 2; ++tries) {
            const int cand = rand() % count;
            if (cand == patrolIndex) continue;
            if (patrolPoints[cand].Distance(fleeLightPos) > kFleeLightAvoidRadius) {
                patrolIndex = cand;
                RequestPath(patrolPoints[static_cast<size_t>(patrolIndex)]);
                return;
            }
        }
        // Nenhum fora do raio: vai para o MAIS LONGE possível da luz.
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
        while (next == patrolIndex)
            next = rand() % count;
        patrolIndex = next;
    } else {
        patrolIndex = (patrolIndex + 1) % count;
    }

    if (Game::debugMode) {
        std::cout << "[PATROL] Indo para waypoint " << patrolIndex
                  << " (" << patrolPoints[patrolIndex].x << ","
                  << patrolPoints[patrolIndex].y << ")\n";
    }
    RequestPath(patrolPoints[static_cast<size_t>(patrolIndex)]);
}

// ─────────────────────────────────────────────────────────────────────────────
//  JANELAS (SABOTAGEM)
// ─────────────────────────────────────────────────────────────────────────────
Window* Monster::FindNearbyClosedWindow() {
    StageState* stage = Game::TryGetStageState();
    if (!stage) return nullptr;

    Window* bestWin = nullptr;
    float closestDist = kWindowRadarRange; // Radar (monster.json) — sabota janelas de longe
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

    // 2. TIMEOUT ABSOLUTO: Se demorar demais, pune o radar e desiste! (Maior agora
    //    porque o radar alcança janelas bem mais distantes — precisa de tempo p/ chegar.)
    if (stateTimer >= 26.0f) {
        if (Game::debugMode) std::cout << "[MONSTER] Preso em obstaculo. Desistindo da janela por 15s!\n";
        windowRadarTimer = strategicMode ? -4.0f : -15.0f; 
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

        // #5 Delay entre sabotagens: em vez de emendar direto na próxima janela,
        // o monstro volta a patrulhar por kSabotageDelay antes de o radar reengatar.
        // Armar o timer negativo faz a próxima varredura só acontecer depois da pausa
        // (vale tanto no modo normal quanto no estratégico — mantém o cerco, mas com ritmo).
        windowRadarTimer = -kSabotageDelay;
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
            windowRadarTimer = strategicMode ? -4.0f : -15.0f; 
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
        windowRadarTimer = strategicMode ? -4.0f : -15.0f; 
        targetWindow = nullptr;
        TransitionTo(MonsterState::PATROL);
    }
}