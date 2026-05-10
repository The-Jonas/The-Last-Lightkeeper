#include "../include/Game.h"
#include "../include/State.h"
#include "../include/InputManager.h"
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>

#define INCLUDE_SDL_TTF
#define INCLUDE_SDL_IMAGE
#define INCLUDE_SDL_MIXER
#include "../include/SDL_include.h"

#include <iostream>

Game* Game::instance = nullptr;
int Game::masterVolumePercent = Game::MASTER_VOLUME_PERCENT;

void Game::LoadEnvVolume() {
    std::ifstream env(".env");
    if (env.is_open()) {
        std::string line;
        while (std::getline(env, line)) {
            std::istringstream iss(line);
            std::string key;
            if (std::getline(iss, key, '=')) {
                std::string value;
                if (std::getline(iss, value)) {
                    if (key == "MASTER_VOLUME") {
                        int vol = std::stoi(value);
                        if (vol >= 0 && vol <= 100) {
                            masterVolumePercent = vol;
                        }
                    }
                }
            }
        }
    }
}

void Game::SetMasterVolume(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    masterVolumePercent = percent;
    const int vol = (MIX_MAX_VOLUME * masterVolumePercent) / 100;
    Mix_Volume(-1, vol);
    Mix_VolumeMusic(vol);
}

Game::Game(std::string title, int width, int heigh) {
    if (instance != nullptr) {
        std::cerr << "Erro: Já existe uma instância do Game rodando!" << std::endl;
        exit(1);
    }

    instance = this;                  // Define a instância atual

    LoadEnvVolume();                  // Carrega volume do .env
    srand(time(NULL));                // Inicializando o gerador de números aleátorios

    // Inicializa SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL_Init falhou: " << SDL_GetError() << std::endl;
        exit(1);
    }

    // Inicializa SDL_Image
    int imgFlags = IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_TIF;
    if (!(IMG_Init(imgFlags) & imgFlags)) {
        std::cerr << "IMG_Init falhou: " << IMG_GetError() << std::endl;
        exit(1);
    }

    // Inicializa SDL_Mixer
    if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 1024) == -1) {
        std::cerr << "Mix_OpenAudio falhou: " << Mix_GetError() << std::endl;
        exit(1);
    }
    Mix_AllocateChannels(32);
    const int masterVolume = (MIX_MAX_VOLUME * masterVolumePercent) / 100;
    Mix_Volume(-1, masterVolume);
    Mix_VolumeMusic(masterVolume);

    // Inicializa TTF
    if (TTF_Init() != 0) {
        std::cerr << "TTF_Init falhou: " << TTF_GetError() << std::endl;
        exit(1);
    }
        
    //Cria a janela
    window = SDL_CreateWindow(
        title.c_str(),                  // Coloquei o título como sendo argumento do construtor de Game também 
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        heigh,
        0                              // Não utilizaremos fullscreen
    );

    if (!window) {
        std::cerr << "SDL_CreateWindow falhou: " << SDL_GetError() << std::endl;
        exit(1);
    }

    //Cria Renderizador
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED); // SDL_RENDERER_ACCELERATED, para requisitar o uso de OpenGL ou Direct3D.
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer falhou: " << SDL_GetError() << std::endl;
        exit(1);
    }
    
    storedState = nullptr;                         // Inicializa o ponteiro de troca de estado

    //Inicia membros
    windowsWidth = width;
    windowsHeight = heigh;
    frameStart = 0;
    dt = 0.0f;
}

// Destrutor

Game::~Game() {
    if (storedState) delete storedState;                // Deletar o storedState não nulo se tiver
    while (!stateStack.empty()) stateStack.pop();       // Esvaziar a pilha de estados

    SDL_DestroyRenderer(renderer);      // Destroi Renderizador
    SDL_DestroyWindow(window);          // Detroi a Janela

    TTF_Quit();                         // Encerra TTF
    Mix_CloseAudio();                   // Encerra Audio
    Mix_Quit();                         // Finaliza Mixer
    IMG_Quit();                         // Finaliza imagem
    SDL_Quit();                         // Finaliza SDL
    
}

Game& Game::GetInstance() {                         // Se não tiver instância do game, cria e retorna a instância
    if (!instance){
        instance = new Game("The Last LightKeeper", 1920, 1080);
    }
    return *instance;                               // O compilador resolve como uma referência
}

State& Game::GetCurrentState() {                    // Retorna o State atual
    return *stateStack.top(); 
}

SDL_Renderer* Game::GetRenderer(){                  // Retorna o Renderizador
    return renderer; 
}

SDL_Window* Game::GetWindow() {                     // Retorna a Janela
    return window;
}

void Game::Push(State* state) {
    storedState = state;                            // Guarda para empilhar no início do frame                  
}

void Game::CalculateDeltaTime() {       
    int currentTicks = SDL_GetTicks();              // Nos diz quantos milissegundos se passaram
    dt = (currentTicks - frameStart) / 1000.0f;     // Calcula intervalo de tempo, transforma em segundos e armazena em dt
    frameStart = currentTicks;                      // Atualiza frameStart
}

float Game::GetDeltaTime() {                        // GetDeltaTime retorna dt para entidades interessadas (como State)
    return dt;
}

int Game::GetWindowsWidth() {                       // Retorna a largura da janela do jogo
    if (window) {
        SDL_GetWindowSize(window, &windowsWidth, &windowsHeight);
    }
    return windowsWidth;
}

int Game::GetWindowsHeight() {                      // Retorna a altura da janela do jogo
    if (window) {
        SDL_GetWindowSize(window, &windowsWidth, &windowsHeight);
    }
    return windowsHeight;
}

bool Game::IsDebugBuild() {
#ifdef DEBUG
    return true;
#else
    return false;
#endif
}

void Game::Run() {

    if (storedState) {
        stateStack.emplace(storedState);
        storedState = nullptr;
        stateStack.top()->Start();
    }

    while (!stateStack.empty() && !stateStack.top()->QuitRequested()) {
        CalculateDeltaTime();
        InputManager::GetInstance().Update();

        // Gerencia Pilha (Pop)
        if (stateStack.top()->PopRequested()) {
            stateStack.pop();
            if (!stateStack.empty()) stateStack.top()->Resume();
        }

        // Gerencia Pilha (Push)
        if (storedState) {
            if (!stateStack.empty()) stateStack.top()->Pause();
            stateStack.emplace(storedState);
            storedState = nullptr;
            stateStack.top()->Start();
        }

        // Executa Estado Atual
        if (!stateStack.empty()) {
            stateStack.top()->Update(dt);
            SDL_RenderClear(renderer);
            stateStack.top()->Render();
            SDL_RenderPresent(renderer);
        }
    }
}