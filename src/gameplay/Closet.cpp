#include "gameplay/Closet.h"
#include "gameplay/Character.h"
#include "core/InputManager.h"
#include "states/stage/StageState.h"
#include "engine/SpriteRenderer.h"
#include "world/Collider.h"
#include "lighting/LightMaskTypes.h"
#include "engine/Camera.h"
#include "ui/Text.h"
#include "core/Game.h"
#include <iostream>

// ── Parâmetros da fresta — para manipular sempre que quiser ─────────────────────
static constexpr float kFrestaConeLengthPx    = 300.0f; // Comprimento do cone
static constexpr float kFrestaHalfAngleDeg    =  70.0f; // Meia-abertura do cone
static constexpr float kFrestaFalloffRadiusPx = 350.0f; // Raio do falloff radial
// ────────────────────────────────────────────────────────────────────────────────

Closet::Closet(GameObject& associated, const std::string& direction)
    : Component(associated), direction(direction), showPrompt(false), isOccupied(false), frestaLightId(-1) {
    
    // Caminho dinâmico baseado na direção que veio do Tiled
    std::string basePath = "Recursos/img/objetos/armario/armario_";
    SpriteRenderer* sprite = new SpriteRenderer(associated, basePath + direction + ".png");
    associated.AddComponent(sprite);

    textObj = new GameObject();
    SDL_Color textColor = {255, 255, 255, 255};
    Text* promptText = new Text(*textObj, "Recursos/font/TradeWinds-Regular.ttf", 14, Text::SOLID, "[E] Esconder", textColor);
    textObj->AddComponent(promptText);
}

Closet::~Closet() {
    delete textObj; 
}

void Closet::Start() {
    StageState* stage = Game::TryGetStageState();
    if (!stage) return;

    Vec2 lightPos;
    float coneAxisDeg;
        
    // ── AJUSTE DE ALTURA DA LUZ ──
    // Aumente esse valor para fazer a luz subir ainda mais em direção ao centro da porta
    float subirFrestaPx = 230.0f; 

    // Calcula de onde a luz da fresta deve sair, agora com a altura corrigida
    if (direction == "frente") {
        lightPos = Vec2(associated.box.Center().x, associated.box.y + associated.box.h - subirFrestaPx);
        coneAxisDeg = 90.0f;
    } else if (direction == "esquerda") {
        lightPos = Vec2(associated.box.x, associated.box.Center().y - subirFrestaPx);
        coneAxisDeg = 180.0f;
    } else if (direction == "direita") {
        lightPos = Vec2(associated.box.x + associated.box.w, associated.box.Center().y - subirFrestaPx);
        coneAxisDeg = 0.0f;
    } else { // "costas"
        lightPos = Vec2(associated.box.Center().x, associated.box.y - subirFrestaPx);
        coneAxisDeg = -90.0f;
    }

    LightMaskParams frestaParams      = stage->GetLightMaskParams();
    frestaParams.coneLengthPx         = kFrestaConeLengthPx;
    frestaParams.coneHalfAngleDeg     = kFrestaHalfAngleDeg;
    frestaParams.coneAxisDeg          = coneAxisDeg;
    frestaParams.coneFollowMouse      = false;                          
    frestaParams.falloffRadiusPx      = kFrestaFalloffRadiusPx;

    frestaParams.torchAnimSpeed       = 0.4f;
    frestaParams.torchMotionRangePx   = 3.0f;
    frestaParams.torchWarpStrength    = 0.10f;
    frestaParams.torchPulseStrength   = 0.08f;

    frestaLightId = stage->CreateStaticLight(lightPos, false, LightMaskShape::Cone, frestaParams);
}

void Closet::Update(float dt) {
    showPrompt = false;

    if (!Character::player) return;

    StageState* stage = Game::TryGetStageState();
    // Checa se ainda há combustível no isqueiro (usando o sistema existente de inventário)
    bool hasLight = stage && stage->GetInventory().IsUsableLightActive();

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

        if (InputManager::GetInstance().KeyPress(SDLK_e)) {
            ExitCloset();
        }

    } else {
        SDL_Rect reachBox = Character::player->GetInteractionRect(1);
        SDL_Rect interactZone = GetInteractionRect();

        // Só exibe a opção de esconder se encostar na FRENTE da porta correta
        if (SDL_HasIntersection(&reachBox, &interactZone)) {
            showPrompt = true;
            if (InputManager::GetInstance().KeyPress(SDLK_e)) {
                EnterCloset();
            }
        }
    }
}

void Closet::Render() {
    if (showPrompt && textObj && !isOccupied) {
        // Exibe o texto acima do armário independentemente da direção
        textObj->box.x = associated.box.Center().x - (textObj->box.w / 2.0f);
        textObj->box.y = associated.box.y - 30; 
        textObj->Render();
    }

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

// ── Zona de interação na borda EXTERNA da porta ───────────────────────────────
SDL_Rect Closet::GetInteractionRect() const {
    
    // Profundidade (distância que o jogador pode ficar afastado para interagir)
    int depth = 65; 

    if (direction == "frente") {
        int w = associated.box.w * 0.5f; 
        int h = depth;
        // Subimos o Y um pouquinho (-10) para a zona de interação entrar de leve na colisão
        return { (int)associated.box.Center().x - (w / 2), (int)(associated.box.y + associated.box.h) - 180, w, h };
        
    } else if (direction == "esquerda") {
        int w = depth;
        // A altura da interação pega 80% da altura total do armário
        int h = associated.box.h * 0.8f; 
        return { (int)associated.box.x - w + 10, (int)associated.box.Center().y - (h / 2), w, h };
        
    } else if (direction == "direita") {
        int w = depth;
        int h = associated.box.h * 0.8f; 
        return { (int)(associated.box.x + associated.box.w) - 10, (int)associated.box.Center().y - (h / 2), w, h };
    }
    
    return { (int)associated.box.x, (int)associated.box.y, (int)associated.box.w, (int)associated.box.h };
}

void Closet::EnterCloset() {
    isOccupied = true;
    
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
        c->hidePersonalLight = true;                                                        // Serve pra apagar o circulo de luz dos personagens
        SpriteRenderer* sprite = c->GetAssociated().GetComponent<SpriteRenderer>();
        if (sprite) sprite->SetTint(255, 255, 255, 0); // Fica invisível
    };

    HideChar(Character::player);
    HideChar(Character::littleBrother);
}

void Closet::ExitCloset() {
    isOccupied = false;

    StageState* stage = Game::TryGetStageState();
    if (stage && frestaLightId != -1) {
        stage->SetLightEnabled(frestaLightId, false); // Garante que a fresta desliga ao sair
    }

    auto ShowChar = [&](Character* c, bool isLittleBrother) {
        if (!c) return;
        c->interactTimer = 0.0f;
        c->currentState = Character::ActionState::NORMAL;
        c->hidePersonalLight = false;
        
        // --- RESTAURA AS POSIÇÕES SALVAS ---
        if (isLittleBrother) {
            c->GetAssociated().box.x = brotherEntryPos.x;
            c->GetAssociated().box.y = brotherEntryPos.y;
        } else {
            c->GetAssociated().box.x = playerEntryPos.x;
            c->GetAssociated().box.y = playerEntryPos.y;
        }

        SpriteRenderer* sprite = c->GetAssociated().GetComponent<SpriteRenderer>();
        if (sprite) sprite->SetTint(255, 255, 255, 255);
        
        Collider* col = c->GetAssociated().GetComponent<Collider>();
        if (col) col->Update(0);
    };

    ShowChar(Character::player, false);
    ShowChar(Character::littleBrother, true);
}


