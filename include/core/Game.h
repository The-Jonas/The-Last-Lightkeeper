#ifndef GAME_H
#define GAME_H

#define INCLUDE_SDL
#include "SDL_include.h"

#include "core/State.h"

#include <memory>
#include <stack>
#include <string>

class StageState;

// ─────────────────────────────────────────────────────────────────────────────
//  Game — singleton com a janela, o renderer, a pilha de estados e o loop.
//
//  As configurações do jogador (volumes, brilho, vídeo, teclas) são estáticas
//  e ficam em config/settings.json (LoadSettings / SaveSettings).
//
//  Espaço lógico: o jogo inteiro desenha na resolução escolhida (appliedResolution)
//  e o SDL escala para a janela com SDL_RenderSetLogicalSize, em qualquer modo.
// ─────────────────────────────────────────────────────────────────────────────
class Game {
public:
    enum DisplayMode { kBorderless = 0, kFullscreen = 1, kWindowed = 2, kDisplayModeCount = 3 };

    // ── Áudio (0..100) ───────────────────────────────────────────────────────
    static int masterVolumePercent;
    static int ambientVolumePercent;             // barramento "Música e ambiente"
    static int sfxVolumePercent;
    static int voiceVolumePercent;               // dublagem dos irmãos
    static void SetMasterVolume(int percent);    // aplica no mixer na hora
    static void SetAmbientVolume(int percent);   // aplica na música na hora
    static void SetSfxVolume(int percent);
    static void SetVoiceVolume(int percent);
    static int  MusicVolume();                   // master × ambiente, 0..MIX_MAX_VOLUME (use para toda música)

    // ── Imagem ───────────────────────────────────────────────────────────────
    static int  brightnessPercent;               // 50..150, 100 = neutro
    static bool brightnessCalibrated;            // já passou pela tela de calibração
    static bool reduceFlashing;                  // acessibilidade: atenua clarões
    static bool vsync;
    static int  fpsCapIndex;                     // índice na lista de limites (0 = sem limite)
    static void  SetBrightness(int percent);     // limita a 50..150
    static float BrightnessGamma();              // metade de cima (100..150) → gama ≥ 1
    static float BrightnessBlackPoint();         // metade de baixo (50..100) → ponto de preto 0..0.08
    static void  SetVSync(bool on);              // aplica na hora
    static int   FpsCap();                       // FPS máximo (0 = sem limite)
    static const char* FpsCapLabel();
    static void  CycleFpsCap(int dir);

    // ── Modo de tela e resolução ─────────────────────────────────────────────
    static int displayMode;                      // DisplayMode atual (aplicado na hora)
    static int resolutionIndex;                  // resolução escolhida (vale no próximo arranque)
    static int appliedResolutionIndex;           // resolução com que o jogo abriu
    static int DisplayModeCount();
    static const char* DisplayModeLabel(int mode);
    static const char* CurrentDisplayModeLabel();
    static void ApplyDisplayMode(int mode);      // troca o modo na hora e grava
    static int  ResolutionCount();
    static int  RecommendedResolutionIndex();    // a nativa da área de trabalho
    static void ResolutionAt(int idx, int& w, int& h);
    static std::string ResolutionLabelAt(int idx);   // "W x H" (+ " (Recomendado)")
    static std::string CurrentResolutionLabel();
    static void CycleResolution(int delta);      // só muda a escolha; aplicar exige reiniciar
    static void RestartApplication();            // relança o executável e encerra este

    // ── Modo de gravação (F11) ───────────────────────────────────────────────
    // Janela menor que o ecrã: o Windows deixa de promovê-la a "independent flip"
    // e o OBS volta a capturar com o foco no jogo (com OpenGL a captura congelava).
    // Liga com F11 ou arrancando com TLL_WINDOW_MODE=windowed.
    static bool captureWindowMode;
    static void SetCaptureWindowMode(bool on);
    static void ToggleCaptureWindowMode();

    // ── Configurações ────────────────────────────────────────────────────────
    static void LoadSettings();                  // lê config/settings.json
    static void SaveSettings();                  // grava, preservando chaves desconhecidas

    // ── Debug ────────────────────────────────────────────────────────────────
    static bool debugMode;                       // teclas de dev e HUD de dev (build debug, DEBUG=1 no .env ou "debug": true)
    static bool IsDebugBuild();                  // compilado com -DDEBUG

    // ── Instância, estados e loop ────────────────────────────────────────────
    static Game& GetInstance();
    static StageState* TryGetStageState();       // nullptr fora do gameplay — sempre verifique
    ~Game();

    void Run();                                  // loop principal até a pilha esvaziar ou pedir saída
    void Push(State* state);                     // empilha no início do próximo frame (Game vira dono)
    State& GetCurrentState();

    SDL_Renderer* GetRenderer();
    SDL_Window*   GetWindow();
    float GetDeltaTime();                        // segundos do último frame
    int   GetWindowsWidth();                     // largura do ESPAÇO LÓGICO (não da tela física)
    int   GetWindowsHeight();                    // altura do ESPAÇO LÓGICO

    static float UiScale();                      // escala da UI pela altura (1.0 em 1080p)
    static float UiFitScale();                   // escala que sempre cabe (menor entre largura e altura) — peças largas

private:
    explicit Game(const std::string& title);

    static void LoadEnvFile();                   // .env legado: volumes e DEBUG
    void InitSdl();                              // SDL, SDL_image, SDL_mixer, SDL_ttf
    void CreateWindowAndRenderer(const std::string& title);
    void LogEnvironment();                       // telemetria da máquina e das configurações
    void CalculateDeltaTime();
    void ApplyPendingStackChanges();             // pop pedido + push pendente
    void LimitFrameRate(Uint64 frameBegin);      // dorme o que falta para o limite de FPS

    static Game* instance;

    SDL_Window*   window   = nullptr;
    SDL_Renderer* renderer = nullptr;
    Uint32 frameStart = 0;                       // ticks do início do frame anterior
    float  dt = 0.0f;
    int    windowsWidth  = 0;                    // espaço lógico
    int    windowsHeight = 0;

    std::unique_ptr<State> pendingState;         // entra na pilha no próximo frame
    std::stack<std::unique_ptr<State>> stateStack;
};

#endif  // GAME_H