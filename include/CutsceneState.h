#ifndef CUTSCENE_STATE_H
#define CUTSCENE_STATE_H

#include "State.h"
#include "Music.h"
#include "GameObject.h"
#include <string>

class CutsceneState : public State {
public: 
    CutsceneState(std::string videoPath, std::string audioPath);        // Recebe o caminho do vídeo e do som do vídeo de fundo
    ~CutsceneState();

    void LoadAssets() override;                                        
    void Update(float dt) override;                                     
    void Render() override;

    void Start() override;
    void Pause() override;
    void Resume() override;

private:
    std::string videoPath;                                                   // Caminho do arquivo de vídeo
    std::string audioPath;                                                   // Caminho do arquivo de áudio

    Music backgroundAudio;                                                   // Som de fundo do vídeo
};



#endif