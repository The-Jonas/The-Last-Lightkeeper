#include "gameplay/Closet.h"
#include "gameplay/Character.h"
#include "core/InputManager.h"
#include "states/stage/StageState.h"
#include "engine/SpriteRenderer.h"
#include "world/Collider.h"
#include "lighting/LightMaskTypes.h"
#include "engine/Camera.h"
#include "gameplay/Monster.h"
#include "ui/Text.h"
#include "core/Game.h"
#include "audio/GameVoice.h"
#include <iostream>

// ── Parâmetros da fresta — para manipular sempre que quiser ─────────────────────
static constexpr float kFrestaConeLengthPx    = 420.0f; // mais longo — alcança mais longe
static constexpr float kFrestaHalfAngleDeg    =  45.0f; // muito mais fino (era 80)
static constexpr float kFrestaFalloffRadiusPx = 420.0f; // falloff maior para acompanhar
// ────────────────────────────────────────────────────────────────────────────────

Closet::Closet(GameObject& associated)
    : Component(associated), direction("frente"), isOccupied(false), frestaLightId(-1) {

    SpriteRenderer* sprite = new SpriteRenderer(associated, "Recursos/img/objetos/Armario.png");
    associated.AddComponent(sprite);
}

Closet::~Closet() {
}

void Closet::Start() {
    // Luz da fresta criada no primeiro Update() — TryGetStageState() 
    // pode retornar nullptr aqui durante transição de level
}

void Closet::Update(float dt) {
    StageState* stage = Game::TryGetStageState();
    if (!Character::player) return; 
    
    if (!stage) return;

    if (frestaLightId == -1) {
        float subirFrestaPx = 180.0f;
        Vec2  lightPos    = Vec2(associated.box.Center().x,
                                 associated.box.y + associated.box.h - subirFrestaPx);

        LightMaskParams frestaParams    = stage->GetLightMaskParams();
        frestaParams.coneLengthPx       = kFrestaConeLengthPx;
        frestaParams.coneHalfAngleDeg   = kFrestaHalfAngleDeg;
        frestaParams.coneAxisDeg        = 90.0f;
        frestaParams.coneFollowMouse    = false;
        frestaParams.falloffRadiusPx    = kFrestaFalloffRadiusPx;
        frestaParams.torchAnimSpeed     = 0.4f;
        frestaParams.torchMotionRangePx = 3.0f;
        frestaParams.torchWarpStrength  = 0.10f;
        frestaParams.torchPulseStrength = 0.08f;

        frestaLightId = stage->CreateStaticLight(lightPos, false,
                                                  LightMaskShape::Cone, frestaParams);
    }


    // Checa se ainda há combustível no isqueiro (usando o sistema existente de inventário)
    bool hasLight = stage && stage->GetInventory().IsUsableLightActive();

    // Congela a interação enquanto um overlay (menu/modal) está ativo.
    const bool inputFrozen = stage && stage->IsPlayerInputFrozen();

    // AMBOS os irmãos podem entrar/sair do armário (E). Ao esconder, os dois se
    // escondem juntos; usa-se o alcance do personagem CONTROLADO no momento.
    Character* controlled = stage->GetControlledCharacter();
    const bool canUseCloset =
        controlled == stage->GetBigCharacterComponent() ||
        controlled == stage->GetSmallCharacterComponent();

    if (isOccupied) {
        // Atualiza a fresta: apaga se o isqueiro acabar, acende se tiver combustível
        if (stage && frestaLightId != -1) {
            stage->SetLightEnabled(frestaLightId, hasLight);
        }

        // Mantém os personagens presos (sanidade será drenada pelo Character.cpp se a luz apagar)
        auto PinChar = [&](Character* c) {
            if (!c) return;
            c->GetAssociated().box.x = associated.box.Center().x - c->GetAssociated().box.w / 2.0f;
            c->GetAssociated().box.y = associated.box.Center().y - c->GetAssociated().box.h / 2.0f;
            c->interactTimer = 99999.0f;
        };
        PinChar(Character::player);
        PinChar(Character::littleBrother);

        // Escondidos e o monstro se aproxima → irmãozinho sussurra apavorado.
        // Latch por evento: fala UMA vez quando o monstro chega perto e só volta a
        // poder falar depois que ele se afasta de novo — antes o poll por frame
        // refazia a fala a cada ~4s enquanto o monstro rondava o armário.
        if (stage) {
            const Vec2 closetCenter = associated.box.Center();
            for (const auto& goPtr : stage->GetObjectArray()) {
                GameObject* go = goPtr.get();
                if (!go || go->IsDead()) {
                    continue;
                }
                if (go->GetComponent<Monster>()) {
                    const float d = closetCenter.Distance(go->box.Center());
                    if (hideVoiceArmed && d < 480.0f) {
                        GameVoice::OnHidingMonsterClose();
                        hideVoiceArmed = false;         // já avisou nesta aproximação
                    } else if (d > 640.0f) {
                        hideVoiceArmed = true;          // monstro se afastou → rearmar
                    }
                    break;
                }
            }
        }

        // #4 Sussurro aleatório "E se o monstro estiver lá fora?" enquanto escondido.
        // Conta regressiva independente do latch acima; ao zerar, tenta falar e
        // sorteia o próximo intervalo. GameVoice ignora o pedido se já houver voz
        // tocando ou o cooldown global não tiver passado — então nunca atropela.
        whisperTimer -= dt;
        if (whisperTimer <= 0.0f) {
            GameVoice::OnHidingWhatIfMonster();
            ArmWhisperTimer();
        }

        if (!inputFrozen && canUseCloset &&
            InputManager::GetInstance().ActionPress(GameAction::Interact)) {
            ExitCloset();
        }

    } else {
        SDL_Rect reachBox = canUseCloset ? controlled->GetInteractionRect(1)
                                         : Character::player->GetInteractionRect(1);
        SDL_Rect interactZone = GetInteractionRect();

        // Aceita esconder com qualquer irmão controlado, encostando na FRENTE da
        // porta correta.
        if (canUseCloset && SDL_HasIntersection(&reachBox, &interactZone)) {
            if (stage) {
                stage->SetReachableCloset(this);   // alimenta o prompt central do rodapé
            }
            if (!inputFrozen && InputManager::GetInstance().ActionPress(GameAction::Interact)) {
                EnterCloset();
            }
        }
    }
}

void Closet::Render() {
    // Rótulo flutuante removido — indicação só no prompt central do rodapé.

#ifdef DEBUG
    // --- DEBUG: DESENHA A ZONA DE INTERAÇÃO COM A CÂMERA ---
    SDL_Rect interactZone = GetInteractionRect();
    
    // Subtrai a posição da câmera para desenhar no lugar certo da tela
    interactZone.x -= Camera::pos.x;
    interactZone.y -= Camera::pos.y; 

    SDL_SetRenderDrawBlendMode(Game::GetInstance().GetRenderer(), SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(Game::GetInstance().GetRenderer(), 0, 255, 0, 150); 
    SDL_RenderFillRect(Game::GetInstance().GetRenderer(), &interactZone);
#endif
}

SDL_Rect Closet::GetInteractionRect() const {
    
    int w     = static_cast<int>(associated.box.w * 0.8f);                  // 80% da largura
    int h     = 120;                                                        // aumentado de 65 para 120
    int x     = static_cast<int>(associated.box.Center().x) - w / 2;
    int y     = static_cast<int>(associated.box.y + associated.box.h) - 140; // borda inferior do sprite

    return { x, y, w, h };
}

void Closet::ArmWhisperTimer() {
    // Cooldown GRANDE entre os sussurros "E se o monstro estiver lá fora?": 45–90 s,
    // para a fala ficar rara (antes 8–16 s tornava repetitiva).
    whisperTimer = 45.0f + static_cast<float>(rand() % 4500) / 100.0f;
}

void Closet::EnterCloset() {
    GameSfx::PlayClosetOpen();
    isOccupied = true;
    hideVoiceArmed = true;   // cada esconderijo começa podendo avisar do monstro uma vez
    ArmWhisperTimer();       // #4 primeiro sussurro só depois de alguns segundos escondido
    
    // --- SALVA A POSIÇÃO DE ENTRADA EXATA DE CADA UM ---
    if (Character::player) {
        playerEntryPos = Vec2(Character::player->GetAssociated().box.x, Character::player->GetAssociated().box.y);
    }
    if (Character::littleBrother) {
        brotherEntryPos = Vec2(Character::littleBrother->GetAssociated().box.x, Character::littleBrother->GetAssociated().box.y);
    }

    auto HideChar = [](Character* c) {
        if (!c) return;
        c->ClearMovement();
        c->currentState = Character::ActionState::INTERACTING;
        c->isHidden = true;                                                                 // Fonte de verdade de "escondido" p/ o monstro (visão/perseguição/dano)
        c->hidePersonalLight = true;                                                        // Serve pra apagar o circulo de luz dos personagens
        SpriteRenderer* sprite = c->GetAssociated().GetComponent<SpriteRenderer>();
        if (sprite) sprite->SetTint(255, 255, 255, 0); // Fica invisível
    };

    HideChar(Character::player);
    HideChar(Character::littleBrother);
    // (O monstro NÃO faz mais campana no armário — o cerco às janelas é disparado
    //  por AnyBrotherHidden() dentro de Monster::Update, sem precisar de aviso.)
}

void Closet::ExitCloset() {
    GameSfx::PlayClosetClose();
    isOccupied = false;

    StageState* stage = Game::TryGetStageState();
    if (stage && frestaLightId != -1) {
        stage->SetLightEnabled(frestaLightId, false); // Garante que a fresta desliga ao sair
    }

    auto ShowChar = [&](Character* c, bool isLittleBrother) {
        if (!c) return;
        c->interactTimer = 0.0f;
        c->currentState = Character::ActionState::NORMAL;
        c->isHidden = false;
        c->hidePersonalLight = false;
        
        // --- RESTAURA AS POSIÇÕES SALVAS ---
        GameObject& go = c->GetAssociated();
        if (isLittleBrother) {
            go.box.x = brotherEntryPos.x;
            go.box.y = brotherEntryPos.y;
        } else {
            go.box.x = playerEntryPos.x;
            go.box.y = playerEntryPos.y;
        }

        // Afasta o personagem do armário ao sair, para não ficar "grudado"/preso
        // nele. Empurra na direção de saída (centro do armário → personagem); se
        // ficou praticamente em cima, empurra para a frente (para baixo).
        const Vec2 closetCenter = associated.box.Center();
        const Vec2 charCenter(go.box.x + go.box.w * 0.5f, go.box.y + go.box.h * 0.5f);
        Vec2 away = charCenter - closetCenter;
        if (away.Magnitude() < 1.0f) away = Vec2(0.0f, 1.0f);
        away = away.Normalized();
        const float kExitPush = 90.0f;   // distância extra do armário
        go.box.x += away.x * kExitPush;
        go.box.y += away.y * kExitPush;

        SpriteRenderer* sprite = c->GetAssociated().GetComponent<SpriteRenderer>();
        if (sprite) sprite->SetTint(255, 255, 255, 255);
        
        Collider* col = c->GetAssociated().GetComponent<Collider>();
        if (col) col->Update(0);
    };

    ShowChar(Character::player, false);
    ShowChar(Character::littleBrother, true);

    // Às vezes a posição de saída fica DENTRO de uma colisão (parede atrás do
    // armário) e o personagem trava — pior ainda segurando uma tecla de andar ao
    // entrar/sair. Descarta os comandos de movimento acumulados durante o esconde
    // e empurra para fora testando AS DUAS colisões do movimento.
    auto EnsureFree = [&](Character* c) {
        if (!c || !stage) {
            return;
        }
        c->ClearMovement();
        stage->UnstickCharacter(c);
    };
    EnsureFree(Character::player);
    EnsureFree(Character::littleBrother);
}


