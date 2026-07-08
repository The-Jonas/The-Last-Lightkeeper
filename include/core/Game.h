#ifndef GAME_H                                  // Evita inclusão múltipla
#define GAME_H

#define INCLUDE_SDL
#include "SDL_include.h"                        // Usa a classe state

#include "core/State.h"
#include <string>
#include <stack>
#include <memory>

class State;
class StageState;

class Game {
public:
    static constexpr int MASTER_VOLUME_PERCENT = 20;
    static constexpr int AMBIENT_VOLUME_PERCENT = 50;
    static constexpr int SFX_VOLUME_PERCENT = 100;
    static constexpr int VOICE_VOLUME_PERCENT = 100;   // dublagem (falas dos irmãos)
    static int masterVolumePercent;
    static int ambientVolumePercent;
    static int sfxVolumePercent;
    static int voiceVolumePercent;
    static int brightnessPercent;   // 50..150, 100 = normal (overlay de brilho)
    static bool fullscreen;         // janela em tela cheia (borderless desktop)
    static bool reduceFlashing;     // acessibilidade: atenua clarões/flashes de tela
    static void LoadEnvVolume();
    static void SetMasterVolume(int percent);
    static void SetAmbientVolume(int percent);
    static void SetSfxVolume(int percent);
    static void SetVoiceVolume(int percent);
    static void SetBrightness(int percent);
    static void SetFullscreen(bool on);
    // Volume da música/fundo constante = master × "Fundo" (ambientVolumePercent).
    // 0..MIX_MAX_VOLUME. Toda música de fundo deve usar isto (não o master puro).
    static int MusicVolume();
    // Configurações unificadas em config/settings.json (volume/brilho/fullscreen/debug).
    static void LoadSettings();
    static void SaveSettings();
    static constexpr int WINDOW_WIDTH = 1920;
    static constexpr int WINDOW_HEIGHT = 1080;

    // Runtime debug toggle. True for debug builds (-DDEBUG) or when opted in via
    // `DEBUG=1` in .env. Gates developer-only keys and the on-screen dev HUD so
    // players never see them.
    static bool debugMode;

    static Game& GetInstance();                 // Retorna instância única (singleton)
    SDL_Renderer* GetRenderer();                // Retorna o renderizador SDL
    SDL_Window* GetWindow();                    // Retorna a janela SDL
    State& GetCurrentState();                   // Retorna o estado atual do jogo
    /// Null when the active state is not gameplay (title, loading, etc.). Always check before use.
    static StageState* TryGetStageState();
    void Run();                                 // Loop Principal do jogo

    ~Game();                                    // Destrutor

    float GetDeltaTime();                       // Retorna o valor de dt

    int GetWindowsWidth();                      // Funções get para altura e 
    int GetWindowsHeight();                     // largura da janela do jogo

    // true quando compilado com alvo `mingw32-make debug` (-DDEBUG)
    static bool IsDebugBuild();

    void Push(State* state);

private:
    Game(std::string title);

    static Game* instance;                      // Instância única da classe Game
    SDL_Window* window;                         // Janela do jogo
    SDL_Renderer* renderer;                     // Renderizador SDL

    void CalculateDeltaTime();                  // Vai calcular o dt e ser chamado em cada iteração do game loop
    int frameStart;                             // Usado para calcular diferença de tempo entre frames 
    float dt;                                   // Convertido o resultado em segundos, vamos armazenar aqui em dt
    
    int windowsWidth, windowsHeight;            // Variáveis que criei para pegar os valores do tamanho da janela

    State* storedState;
    std::stack<std::unique_ptr<State>> stateStack;
    
};

#endif //GAME_H