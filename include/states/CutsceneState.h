#ifndef CUTSCENE_STATE_H
#define CUTSCENE_STATE_H

#include "core/State.h"
#include "audio/Music.h"
#include "engine/GameObject.h"
#include <string>
#include <vector>

struct SDL_Renderer;

class CutsceneState : public State {
public:
    CutsceneState(std::string videoPath, std::string audioPath, State* nextState = nullptr);        // Recebe o caminho do vídeo e do som do vídeo de fundo
    ~CutsceneState();

    void LoadAssets() override;
    void Update(float dt) override;
    void Render() override;

    void Start() override;
    void Pause() override;
    void Resume() override;

private:
    void Finish();                                                          // Encerra a cena e empilha o próximo estado (se houver)
    void RenderSkipHint(SDL_Renderer* renderer);                            // Tooltip + anel de progresso do "segurar para pular"

    // Legendas cronometradas da cutscene (arquivo .srt ao lado do vídeo).
    struct SubtitleCue {
        double start = 0.0;   // segundos
        double end = 0.0;     // segundos
        std::string text;
    };
    std::vector<SubtitleCue> subtitles;
    double playbackSec = 0.0;                    // relógio da cena (segue o áudio)
    void LoadSubtitles(const std::string& srtPath);
    void RenderSubtitle(SDL_Renderer* renderer);

    std::string videoPath;                                                   // Caminho do arquivo de vídeo
    std::string audioPath;                                                   // Caminho do arquivo de áudio
    Music backgroundAudio;                                                   // Som de fundo do vídeo

    State* nextState;

    // Pular segurando ESPAÇO: o timer acumula enquanto a tecla fica pressionada e
    // zera ao soltar; ao encher (kSkipHoldDuration) a cena é pulada. Um anel no
    // canto inferior direito preenche conforme o progresso.
    float skipHoldTimer = 0.0f;
    bool  finished = false;
    static constexpr float kSkipHoldDuration = 3.0f;

    // Áudio começa junto do 1º avanço do vídeo → ambos partem sincronizados.
    bool audioStarted = false;

    // Tooltip discreto: aparece com qualquer input e some (fade-out) depois de um
    // tempo ocioso. tooltipAlpha 0..1 é interpolado; idleTimer mede a inatividade.
    float tooltipAlpha = 1.0f;
    float idleTimer = 0.0f;
    int   lastMouseX = -1;
    int   lastMouseY = -1;
    static constexpr float kTooltipHideDelay = 2.5f;   // segundos ociosos até sumir
    static constexpr float kTooltipFadeSpeed = 3.0f;   // 1/seg (≈0.33s de fade)
};

#endif