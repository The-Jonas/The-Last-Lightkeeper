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
    // Sprite placeholder — troca quando tiver o asset final
    SpriteRenderer* sr = new SpriteRenderer(associated, "Recursos/img/personagens/monstro/monstro_placeholder.png");
    associated.AddComponent(sr);
    associated.box.y -= associated.box.h;
 
    // Começa patrulhando
    TransitionTo(MonsterState::PATROL);
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
    if (state != MonsterState::HUNT && state != MonsterState::FLEE_LIGHT && CanSeeLitBrother(seenPos)) {
        lastKnownPlayerPos = seenPos;
        hasMemory          = true;
        memoryDecayTimer   = 0.0f;
 
        if (state != MonsterState::CHASE)
            TransitionTo(MonsterState::CHASE);
    }

    // ── Radar de Janelas (Só quando não está em combate ou fugindo) ───────────
    if ((state == MonsterState::PATROL || state == MonsterState::INVESTIGATE) && pathRefreshTimer >= kPathRefreshInterval) {
        Window* nearbyWin = FindNearbyClosedWindow();
        if (nearbyWin) {
            targetWindow = nearbyWin;
            TransitionTo(MonsterState::SABOTAGE_WINDOW);
        }
    }

    // ── Despacha para o sub-update do estado atual ────────────────────────────
    switch (state) {
        case MonsterState::PATROL:       UpdatePatrol(dt);      break;
        case MonsterState::INVESTIGATE:  UpdateInvestigate(dt); break;
        case MonsterState::CHASE:        UpdateChase(dt);       break;
        case MonsterState::HUNT:         UpdateHunt(dt);        break;
        case MonsterState::CAMP_CLOSET:  UpdateCampCloset(dt);  break;
        case MonsterState::FLEE_LIGHT:   UpdateFleeLigth(dt);   break;
        case MonsterState::SABOTAGE_WINDOW: UpdateSabotageWindow(dt); break;
    }

    CheckDamageCollision();
}

void Monster::Render() {
    // Silhueta é desenhada pelo MonsterSilhouette (z=100, acima da escuridão)
    
#ifdef DEBUG
    // ── DEBUG: CAIXA DE DANO (Vermelha) ──
    // Recalculamos a mesma caixa usada no CheckDamageCollision
    SDL_Rect dmgBox = {
        static_cast<int>(associated.box.x + associated.box.w * 0.1f),
        static_cast<int>(associated.box.y + associated.box.h * 0.1f),
        static_cast<int>(associated.box.w * 0.8f),
        static_cast<int>(associated.box.h * 0.8f)
    };

    // Subtrai a câmera para desenhar no lugar certo da tela
    dmgBox.x -= Camera::pos.x;
    dmgBox.y -= Camera::pos.y;

    // Desenha um retângulo vermelho translúcido
    SDL_SetRenderDrawBlendMode(Game::GetInstance().GetRenderer(), SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(Game::GetInstance().GetRenderer(), 255, 0, 0, 100); 
    SDL_RenderFillRect(Game::GetInstance().GetRenderer(), &dmgBox);
    
    // Desenha também a borda da Hitbox sólida (amarela, opcional, pra ver onde ele colide com parede)
    SDL_Rect physBox = {
        static_cast<int>(associated.box.x) - static_cast<int>(Camera::pos.x),
        static_cast<int>(associated.box.y) - static_cast<int>(Camera::pos.y),
        static_cast<int>(associated.box.w),
        static_cast<int>(associated.box.h)
    };
    SDL_SetRenderDrawColor(Game::GetInstance().GetRenderer(), 255, 255, 0, 255); 
    SDL_RenderDrawRect(Game::GetInstance().GetRenderer(), &physBox);
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
        case MonsterState::FLEE_LIGHT:
            moveSpeed = kSpeedChase;
            // Procura o waypoint MAIS LONGE do jogador para garantir que a fuga é segura!
            if (!patrolPoints.empty() && Character::player) {
                Vec2 playerPos = Character::player->GetAssociated().box.Center();
                int bestIdx = 0;
                float maxDist = -1.0f;
                for (size_t i = 0; i < patrolPoints.size(); ++i) {
                    float d = patrolPoints[i].Distance(playerPos);
                    if (d > maxDist) { maxDist = d; bestIdx = static_cast<int>(i); }
                }
                RequestPath(patrolPoints[bestIdx]);
            }
            break;
        case MonsterState::SABOTAGE_WINDOW:
            moveSpeed = kSpeedInvestigate; // Ou a velocidade que preferir
            break;
        
    }
}


// ─────────────────────────────────────────────────────────────────────────────
//  COLISÃO DE DANO (Customizada pelo tamanho do Sprite)
// ─────────────────────────────────────────────────────────────────────────────
void Monster::CheckDamageCollision() {
    if (damageCooldown > 0.0f) return;

    // Criamos uma Hitbox virtual que pega 85% do tamanho da imagem inteira do monstro
    // (Retirando 10% das bordas para não ser injusto com os pixels transparentes do sprite)
    SDL_Rect dmgBox = {
        static_cast<int>(associated.box.x + associated.box.w * 0.1f),
        static_cast<int>(associated.box.y + associated.box.h * 0.1f),
        static_cast<int>(associated.box.w * 0.85f),
        static_cast<int>(associated.box.h * 0.85f)
    };

    // Função lambda (interna) auxiliar para checar a intersecção contra um irmão
    auto CheckPlayerHit = [&](Character* c) {
        if (!c) return false;
        SDL_Rect pBox = {
            static_cast<int>(c->GetAssociated().box.x),
            static_cast<int>(c->GetAssociated().box.y),
            static_cast<int>(c->GetAssociated().box.w),
            static_cast<int>(c->GetAssociated().box.h)
        };
        // Checa se os dois retângulos estão se batendo na tela
        return SDL_HasIntersection(&dmgBox, &pBox) == SDL_TRUE;
    };

    bool hit = false;
    
    // Checa o Irmãozão
    if (CheckPlayerHit(Character::player)) {
        Character::player->sanity -= kSanityDamageOnTouch;
        if (Character::player->sanity < 0.0f) Character::player->sanity = 0.0f;
        lastKnownPlayerPos = Character::player->GetAssociated().box.Center();
        hit = true;
        std::cout << "[Monster] Dano no Irmaozao! Sanidade -= " << kSanityDamageOnTouch << "\n";
    }
    // Checa o Irmãozinho (só se não bateu no mais velho neste frame)
    else if (CheckPlayerHit(Character::littleBrother)) {
        Character::littleBrother->sanity -= kSanityDamageOnTouch;
        if (Character::littleBrother->sanity < 0.0f) Character::littleBrother->sanity = 0.0f;
        lastKnownPlayerPos = Character::littleBrother->GetAssociated().box.Center();
        hit = true;
        std::cout << "[Monster] Dano no Irmaozinho! Sanidade -= " << kSanityDamageOnTouch << "\n";
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
//  SUB-UPDATES
// ─────────────────────────────────────────────────────────────────────────────
void Monster::UpdatePatrol(float dt) {
    MoveAlongPath(dt, moveSpeed);

    if (HasReachedTarget()) {
        // Ao invés de spammar, ele espera 0.5 segundos ao chegar no ponto antes de escolher o próximo.
        // Isso resolve o bug dele travar se o pathfinder falhar.
        if (stateTimer >= 0.5f) {
            PickNextPatrolPoint();
            stateTimer = 0.0f;
        }
    } else {
        stateTimer = 0.0f; // Mantém o timer zerado enquanto estiver andando
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
    // Tenta andar pelo caminho traçado (se houver)
    if (!currentPath.empty()) {
        MoveAlongPath(dt, moveSpeed);
    } else {
        // FALLBACK MAGNÉTICO: Se não achou um caminho ou o mapa não tem Patrol Points,
        // ele foge "na marra", correndo fisicamente na direção oposta ao jogador!
        if (Character::player) {
            Vec2 myPos = associated.box.Center();
            Vec2 threatPos = Character::player->GetAssociated().box.Center();
            
            // Vetor que aponta do jogador PARA o monstro (direção oposta)
            Vec2 dir = myPos - threatPos;
            float dist = std::sqrt(dir.x * dir.x + dir.y * dir.y);
            
            if (dist > 0.001f) {
                associated.box.x += (dir.x / dist) * moveSpeed * dt;
                associated.box.y += (dir.y / dist) * moveSpeed * dt;
            }
        }
    }
 
    // ==========================================
    // REGRAS DE SAÍDA DA FUGA
    // ==========================================
    
    if (!IsSelfInLight()) {
        // REGRA 1: SÓ sai do modo fuga se a luz REALMENTE parou de tocar nele.
        if (hasMemory) {
            TransitionTo(MonsterState::INVESTIGATE);
        } else {
            TransitionTo(MonsterState::PATROL);
        }
    } 
    else if (HasReachedTarget() && !currentPath.empty()) {
        // REGRA 2: Chegou no ponto de fuga, mas a luz do jogador continua tocando nele?
        // Ao invés de parar e travar, ele reseta o estado de fuga pra procurar um lugar ainda mais longe!
        TransitionTo(MonsterState::FLEE_LIGHT);
    }
}

void Monster::NotifyCollision(GameObject& other) {
// Como a checagem de dano roda na Update através da CheckDamageCollision, 
// a Engine de física (LevelManager) cuida apenas de impedir o monstro de atravessar a parede.
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
 
bool Monster::IsWorldPosInAnyLight(Vec2 worldPos, float extraRadius) const {
    StageState* stage = Game::TryGetStageState();
    if (!stage) return false;

    // Checa todas as luzes do CENÁRIO (velas, lanternas fixas, etc.)
    for (const auto& light : stage->GetLights()) {
        if (!light.enabled) continue;

        const float radius = light.params.falloffRadiusPx;
        if (radius <= 0.0f) continue;

        const float dist = worldPos.Distance(light.worldPos);
        if (dist < (radius + extraRadius)) {
            return true;
        }
    }

    // Checa a tocha/isqueiro do personagem (se estiver ativa)
    Vec2 torchPos;
    float torchRadius = 0.0f;
    if (stage->GetActiveTorchWorldPos(torchPos, torchRadius)) {
        const float dist = worldPos.Distance(torchPos);
        if (dist < (torchRadius + extraRadius)) {
            return true;
        }
    }

    return false;
}

bool Monster::IsSelfInLight() const {
    const float myRadius = associated.box.w * 0.45f;
    return IsWorldPosInAnyLight(associated.box.Center(), myRadius);
}

// ─────────────────────────────────────────────────────────────────────────────
//  PATHFINDING
// ─────────────────────────────────────────────────────────────────────────────
void Monster::RequestPath(Vec2 destination) {
    StageState* stage = Game::TryGetStageState();
    if (!stage) return;

    targetPos   = destination;

    if (!stage->IsWorldPosNavigableFor(destination, &associated)) {
        currentPath.clear();
        pathStep = 0;
        return;
    }

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

// ─────────────────────────────────────────────────────────────────────────────
//  INTELIGÊNCIA DE JANELAS (SABOTAGEM)
// ─────────────────────────────────────────────────────────────────────────────
Window* Monster::FindNearbyClosedWindow() {
    StageState* stage = Game::TryGetStageState();
    if (!stage) return nullptr;

    Window* bestWin = nullptr;
    float closestDist = 400.0f; // Distância que o monstro "sente" que tem uma janela
    Vec2 myPos = associated.box.Center();

    // Varre todos os objetos do mapa atrás de janelas
    for (const auto& goPtr : stage->GetObjectArray()) {
        GameObject* go = goPtr.get();
        if (!go || go->IsDead()) continue;

        Window* win = go->GetComponent<Window>();
        if (win && win->GetState() == Window::WindowState::CLOSED) {
            Vec2 winPos = go->box.Center();
            float dist = myPos.Distance(winPos);
            
            if (dist < closestDist) {
                // O MONSTRO SÓ ABRE SE A JANELA NÃO ESTIVER ILUMINADA!
                if (!IsWorldPosInAnyLight(winPos)) {
                    closestDist = dist;
                    bestWin = win;
                }
            }
        }
    }
    return bestWin;
}

void Monster::UpdateSabotageWindow(float dt) {
    // Aborta a sabotagem se: perdeu referência, a janela já foi aberta, ou alguém iluminou a janela!
    if (!targetWindow || targetWindow->GetState() != Window::WindowState::CLOSED || IsWorldPosInAnyLight(targetWindow->GetAssociated().box.Center())) {
        targetWindow = nullptr;
        TransitionTo(MonsterState::PATROL);
        return;
    }

    Vec2 winPos = targetWindow->GetAssociated().box.Center();
    float dist = associated.box.Center().Distance(winPos);

    if (dist <= 90.0f) {
        // Chegou perto o suficiente da janela, abre!
        targetWindow->Toggle();
        targetWindow = nullptr;
        TransitionTo(MonsterState::PATROL);
    } else {
        // Continua andando até a janela
        if (pathRefreshTimer >= kPathRefreshInterval) {
            pathRefreshTimer = 0.0f;
            RequestPath(winPos);
        }
        MoveAlongPath(dt, moveSpeed);
    }
}