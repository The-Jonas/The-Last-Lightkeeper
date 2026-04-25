#include "../include/CutsceneState.h"
#include "../include/VideoPlayer.h"
#include "../include/StageState.h"
#include "../include/Game.h"
#include "../include/InputManager.h"

CutsceneState::CutsceneState(std::string videoPath, std::string audioPath, State* nextState) : videoPath(videoPath), audioPath(audioPath), nextState(nextState) {
    // Construtor recebe os caminhos do vídeo e do áudio
}

CutsceneState::~CutsceneState() {
    objectArray.clear();
}

void CutsceneState::LoadAssets() {
    // Criamos um GameObject para o vídeo e adicionamos o componente VideoPlayer
    GameObject* videoGo = new GameObject();
    videoGo->AddComponent(new VideoPlayer(*videoGo, videoPath));
    
    // Posicionamos na origem
    videoGo->box.x = 0;
    videoGo->box.y = 0;

    AddObject(videoGo);
    
    // Carregamos o áudio sincronizado com o vídeo
    backgroundAudio.Open(audioPath);
}

void CutsceneState::Start() {
    LoadAssets();
    backgroundAudio.Play();  
    StartArray();
}

void CutsceneState::Update(float dt) {
    InputManager& input = InputManager::GetInstance();

    // Lógica para pular (skip): Se apertar ESC ou Espaço, encerra a cena.
    if (input.QuitRequested()) quitRequested = true;
    if (input.KeyPress(ESCAPE_KEY) || input.KeyPress(SPACE_KEY)) {
        backgroundAudio.Stop();
        popRequested = true;

        if (nextState != nullptr) { 
            Game::GetInstance().Push(nextState);
        }
        return; 
    }

    UpdateArray(dt);

    // Lógica do fim do vídeo: 
    // Procuramos o componente VideoPlayer no array de objetos e verificamos se o vídeo terminou
    for (auto& go: objectArray) {
        VideoPlayer* vp = go->GetComponent<VideoPlayer>();
        if (vp && vp->HasEnded()) {
            backgroundAudio.Stop();
            popRequested = true;

            if (nextState != nullptr) { 
                Game::GetInstance().Push(nextState);
            }
            return; 
        }
    }
}

void CutsceneState::Render() {
    RenderArray();
}

void CutsceneState::Pause() {
    // Vazio
}

void CutsceneState::Resume() {
    // Vazio
}