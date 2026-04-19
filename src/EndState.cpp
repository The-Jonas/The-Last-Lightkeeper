#include "../include/EndState.h"
#include "../include/GameData.h" 
#include "../include/Game.h"
#include "../include/InputManager.h"
#include "../include/GameObject.h"
#include "../include/SpriteRenderer.h"
#include "../include/Text.h"
#include "../include/TitleState.h"

EndState::EndState() {
    // Vazio
}

EndState::~EndState() {
    // Vazio
}

void EndState::LoadAssets() {
    // Decide o Background e Música baseado no fim
    GameObject* bgGO = new GameObject();
    std::string bgFile;
    std::string musicFile;

    if (GameData::playerVictory) {
        bgFile = "Recursos/img/Win.png";
        musicFile = "Recursos/audio/endStateWin.ogg";
    } else {
        bgFile = "Recursos/img/Lose.png";
        musicFile = "Recursos/audio/endStateLose.ogg";
    }

    SpriteRenderer* bg = new SpriteRenderer(*bgGO, bgFile);
    bgGO->AddComponent(bg);
    bgGO->box.x = 0;
    bgGO->box.y = 0;
    AddObject(bgGO);

    // Toca a música 
    backgroundMusic.Open(musicFile);
    backgroundMusic.Play();

    // Adiciona Texto de Instrução
    GameObject* textGO = new GameObject();
    textGO->box.x = 370;
    textGO->box.y = 800;

    SDL_Color color = {255, 255, 255, 255};                         // Branco
    // Pressione "ESC" para sair ou Espaço para jogar de novo"
    Text* text = new Text(*textGO, "Recursos/font/neodgm.ttf", 30, Text::BLENDED, "ESC para Sair | ESPACO para Menu", color);
    textGO->AddComponent(text);
    AddObject(textGO);
}

void EndState::Update(float dt) {
    InputManager& input = InputManager::GetInstance();

    // Sair do jogo (QUIT)
    if (input.QuitRequested() || input.KeyPress(ESCAPE_KEY)) {
        quitRequested = true;
    }

    // Voltar ao Menu (Espaço) - (Achei melhor que voltar para o jogo direto)
    if (input.KeyPress(SPACE_KEY)) {
        popRequested = true;
    }

    UpdateArray(dt);
}

void EndState::Render() {
    RenderArray();
}

void EndState::Start() {
    LoadAssets();
    StartArray();
    started = true;
}

void EndState::Pause() {}
void EndState::Resume() {}