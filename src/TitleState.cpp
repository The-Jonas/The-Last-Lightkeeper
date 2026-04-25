#include "../include/TitleState.h"
#include "../include/StageState.h" 
#include "../include/Game.h"
#include "../include/GameObject.h"
#include "../include/SpriteRenderer.h"
#include "../include/InputManager.h"
#include "../include/Camera.h"
#include "../include/Text.h"
#include "../include/CutsceneState.h"

TitleState::TitleState() : State() {
    // Vazio, lógica pesada fica no LoadAssets/Start (achei fazer dessa forma com mais sentido)
}

TitleState::~TitleState() {
}

void TitleState::LoadAssets() {
    // Cria o GameObject da Tela de título
    GameObject* titleGo = new GameObject();

    // Adiciona o Sprite
    SpriteRenderer* titleSprite = new SpriteRenderer(*titleGo, "Recursos/img/Title.png");
    titleGo->AddComponent(titleSprite);

    // Posiciona no centro
    titleGo->box.x = 0;
    titleGo->box.y = 0;

    AddObject(titleGo);                                             // Adiciona ao Array de estado

    // -- CRIA O TEXTO
    GameObject* textGO = new GameObject();
    textGO->box.x = 440; 
    textGO->box.y = 800; 

    SDL_Color color = {255, 255, 255, 255};                         // Branco
    Text* text = new Text(*textGO, "Recursos/font/neodgm.ttf", 40, Text::BLENDED, "Pressione Espaco", color);
    textGO->AddComponent(text);

    AddObject(textGO);
}

void TitleState::Update(float dt) {
    // Atualiza o Input
    InputManager& input = InputManager::GetInstance();

    // Se apertar ESC ou clicar X, fecha o jogo
    if (input.QuitRequested() || input.KeyPress(ESCAPE_KEY)) {
        quitRequested = true;
    }

    // Se apertar Space, vai para o Jogo (StageState)
    if (input.KeyPress(SPACE_KEY)) {
        Game::GetInstance().Push(new CutsceneState("Recursos/video/video_cutscene.mpg", "Recursos/audio/audio_cutscene.wav"));                 // Cria o estado do jogo e empilha
    }

    // Lógica de piscar texto
    textBlinkTimer.Update(dt);

    // Percorre para achar o texto
    for (auto& go : objectArray) {
        Text* text = go->GetComponent<Text>();
        if (text) {

            if (textBlinkTimer.Get() > 0.5f) {                      // Pisca a cada 0.5s
                // Alterna a cor ou o texto (vamos alternar a cor alpha ou esconder)
                // Se timer < 0.5 desenha, > 0.5 e < 1.0 não desenha
                text->SetColor({15, 33, 60, 255});                  // Define azul sólido
            }

            // Uma forma simples de "sumir"
            if (textBlinkTimer.Get() > 0.5f) {
                text->SetColor({15, 33, 60, 0});                    // Mesma cor azul, mas transparente
            }
            if (textBlinkTimer.Get() > 1.5f) {
                text->SetColor({255, 255, 255, 255});               // Branco
                textBlinkTimer.Restart();
            }
        }
    }

    UpdateArray(dt);                                                // Atualiza os objetos
}

void TitleState::Render() {
    RenderArray();
}

void TitleState::Start() {
    LoadAssets();
    StartArray();
    started = true;
}

void TitleState::Pause() {
    // Nada aqui
}

void TitleState::Resume() {
    Camera::pos = Vec2(0, 0);                                       // Reseta câmera se necessário
}