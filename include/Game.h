#ifndef GAME_H                                  // Evita inclusão múltipla
#define GAME_H

#define INCLUDE_SDL
#include "SDL_include.h"                        // Usa a classe state

#include "State.h"
#include <string>
#include <stack>
#include <memory>

class State;

class Game {
public:
    static constexpr int MASTER_VOLUME_PERCENT = 20;
    static int masterVolumePercent;
    static void LoadEnvVolume();
    static void SetMasterVolume(int percent);
    static constexpr int WINDOW_WIDTH = 1920;
    static constexpr int WINDOW_HEIGHT = 1080;

    static Game& GetInstance();                 // Retorna instância única (singleton)
    SDL_Renderer* GetRenderer();                // Retorna o renderizador SDL
    SDL_Window* GetWindow();                    // Retorna a janela SDL
    State& GetCurrentState();                   // Retorna o estado atual do jogo
    void Run();                                 // Loop Principal do jogo

    ~Game();                                    // Destrutor

    float GetDeltaTime();                       // Retorna o valor de dt

    int GetWindowsWidth();                      // Funções get para altura e 
    int GetWindowsHeight();                     // largura da janela do jogo

    // true quando compilado com alvo `mingw32-make debug` (-DDEBUG)
    static bool IsDebugBuild();

    void Push(State* state);

private:
    Game(std::string title, int width, int height);

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