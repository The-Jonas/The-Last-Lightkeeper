#include "gameplay/Window.h"
#include "gameplay/Candlestick.h"
#include "states/stage/StageState.h"
#include "engine/SpriteRenderer.h"
#include "states/stage/StageState.h"
#include "ui/Text.h"
#include "core/Game.h"
#include "world/Collider.h"
#include "states/stage/InternalHelpers.h"
#include "engine/Camera.h"
#include "audio/GameSfx.h" // Inclui o gerenciador de áudio que modificamos

Window::Window(GameObject& associated, std::string windowType, bool startsOpen, float windRadius)
    : Component(associated), windowType(windowType), windRadius(windRadius), windTimer(0.0f), animTimer(0.0f) {
    
    state = startsOpen ? WindowState::OPEN : WindowState::CLOSED;
    basePath = "Recursos/img/cenario/janelas/janela_";

    SpriteRenderer* sprite = new SpriteRenderer(associated, "");
    associated.AddComponent(sprite);
    UpdateVisuals();

    // Setup do Prompt de texto
    textObj = new GameObject();
    SDL_Color textColor = {255, 255, 255, 255};
    Text* promptText = new Text(*textObj, "Recursos/font/times.ttf", 14, Text::SOLID, "", textColor);
    textObj->AddComponent(promptText);
    UpdatePromptText();
}

Window::~Window() {
    delete textObj;
    GameSfx::StopWindLoop(); // Garante que o vento pare se a janela for destruída
}

void Window::Start() {
    if (state == WindowState::OPEN) {
        GameSfx::StartWindLoop();
    }
}

void Window::Update(float dt) {
    StageState* stage = Game::TryGetStageState();
    if (!stage) return;

    // 1. Atualiza a flag de prompt se o jogador estiver perto (igual ao Candlestick)
    // Assumindo que você tem uma função parecida no StageState
    const bool reachable = (stage->GetReachableWindow() == this);
    if (reachable != showPrompt) {
        showPrompt = reachable;
        if (showPrompt) UpdatePromptText();
    }

    // 2. Lógica de Animação (Transições)
    if (state == WindowState::OPENING || state == WindowState::CLOSING) {
        animTimer += dt;
        if (animTimer >= kAnimDuration) {
            animTimer = 0.0f;
            state = (state == WindowState::OPENING) ? WindowState::OPEN : WindowState::CLOSED;
            UpdateVisuals();
            UpdatePromptText();
            
            if (state == WindowState::OPEN) {
                GameSfx::StartWindLoop();
            } else {
                // Só para o vento se NENHUMA outra janela estiver aberta
                StageState* stage = Game::TryGetStageState();
                bool anyWindowOpen = false;
                if (stage) {
                    for (const auto& go : stage->GetObjectArray()) {
                        Window* win = go->GetComponent<Window>();
                        if (win && win != this &&
                           (win->GetState() == WindowState::OPEN || win->GetState() == WindowState::OPENING)) {
                            anyWindowOpen = true;
                            break;
                        }
                    }
                }
                if (!anyWindowOpen) {
                    GameSfx::StopWindLoop();
                }
            }
        }
    }

    // 3. Lógica do Vento Apagando Velas
    if (state == WindowState::OPEN) {
        windTimer += dt;
        if (windTimer >= kWindInterval) {
            windTimer = 0.0f;
            BlowOutNearbyCandles();
        }
    }
}

void Window::Render() {
    // Rótulo flutuante removido — indicação só no prompt central do rodapé.

#ifdef DEBUG
    // ==========================================
    // DEBUG VISUAL 
    // ==========================================
    // Mesma regra do monstro: estes contornos so aparecem com a tecla [B]
    // ligada. Antes desenhavam SEMPRE numa build DEBUG e enchiam o ecra.
    StageState* dbgStage = Game::TryGetStageState();
    if (!dbgStage || !dbgStage->IsPhysicsDebugOn()) return;

    SDL_Renderer* renderer = Game::GetInstance().GetRenderer();
    Vec2 center = associated.box.Center();
    
    // Mundo → tela COM zoom (igual ao sprite). Os raios são em pixels de MUNDO,
    // por isso também multiplicam pelo zoom — senão os círculos mentiam sobre o
    // alcance real assim que a câmera se afastava.
    const float z = Camera::GetZoom();
    const Vec2 screen = Camera::WorldToScreen(center);

    //ÁREA DE INTERAÇÃO (VERDE) - Raio de 260 px (bate com o teste em GetReachableWindow)
    stage_internal::DrawDebugCircle(renderer, screen.x, screen.y, 260.0f * z, 50, 255, 50, 180);

    //ÁREA DE VENTO (AZUL CLARO) - Usa o windRadius
    stage_internal::DrawDebugCircle(renderer, screen.x, screen.y, windRadius * z, 100, 200, 255, 180);
#endif
}

// O som de abrir/fechar sai DESTA janela: com a posicao, o jogador percebe
// qual das janelas do andar se mexeu sem ter de a ver. Sem jogador (transicoes,
// teardown) cai na versao antiga, ao centro.
void Window::PlayToggleSoundFromHere(bool opening) {
    if (Character::player) {
        const Vec2 here = associated.box.Center();
        const Vec2 ear = Character::player->GetAssociated().box.Center();
        GameSfx::PlayWindowToggle(opening, here.x, here.y, ear.x, ear.y);
    } else {
        GameSfx::PlayWindowToggle(opening);
    }
}

void Window::Toggle() {
    if (state == WindowState::OPENING || state == WindowState::CLOSING) return; // Ignora se já estiver animando

    if (state == WindowState::CLOSED) {
        state = WindowState::OPENING;
        PlayToggleSoundFromHere(true);
    } else if (state == WindowState::OPEN) {
        state = WindowState::CLOSING;
        PlayToggleSoundFromHere(false);
        windTimer = 0.0f; // Reseta o vento
    }
    
    animTimer = 0.0f;
    UpdateVisuals();
}

void Window::UpdateVisuals() {
    SpriteRenderer* sprite = associated.GetComponent<SpriteRenderer>();
    if (!sprite) return;

    std::string estado;
    switch (state) {
        case WindowState::CLOSED:  estado = "fechada.png"; break;
        case WindowState::OPENING: estado = "abrindo.png"; break;
        case WindowState::OPEN:    estado = "aberta.png"; break;
        case WindowState::CLOSING: estado = "abrindo.png"; break;
    }
    
    // A mágica acontece aqui! 
    // Exemplo: "janela_" + "1" + "_" + "fechada.png"
    sprite->Open(basePath + windowType + "_" + estado);
}

void Window::UpdatePromptText() {
    if (!textObj) return;
    Text* promptText = textObj->GetComponent<Text>();
    if (promptText) {
        promptText->SetText(state == WindowState::OPEN ? "[E] Fechar" : "[E] Abrir");
    }
}

void Window::BlowOutNearbyCandles() {
    StageState* stage = Game::TryGetStageState();
    if (!stage) return;

    Vec2 myCenter = associated.box.Center();
    bool blewOutAny = false;

    for (const auto& go : stage->GetObjectArray()) {
        Candlestick* candle = go->GetComponent<Candlestick>();
        if (candle && candle->IsLit()) {
            float distance = myCenter.Distance(go->box.Center());
            if (distance <= windRadius) {
                candle->SetLit(false);
                blewOutAny = true;
            }
        }
    }

    // Só toca o som se realmente apagou pelo menos uma vela
    if (blewOutAny) {
        GameSfx::PlayCandleBlowOut();
    }
}