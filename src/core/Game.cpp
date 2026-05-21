#include "core/Game.h"
#include "core/State.h"
#include "states/stage/StageState.h"
#include "core/InputManager.h"
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <string>
#include <algorithm>
#include <cctype>
#include <exception>

#define INCLUDE_SDL_TTF
#define INCLUDE_SDL_IMAGE
#define INCLUDE_SDL_MIXER
#include "SDL_include.h"

#include "nlohmann/json.hpp"

#include <iostream>

Game* Game::instance = nullptr;
int Game::masterVolumePercent = Game::MASTER_VOLUME_PERCENT;

namespace {

std::string TrimWhitespace(std::string s) {
    const auto notspace = [](unsigned char c) { return !std::isspace(c); };
    while (!s.empty() && !notspace(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && !notspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

void ApplyOptionalWindowSettingsFromFile(int& width, int& height) {
    std::ifstream f("config/settings.json");
    if (!f.is_open()) {
        return;
    }
    try {
        nlohmann::json j;
        f >> j;
        if (j.contains("window_width") && j["window_width"].is_number_integer()) {
            const int w = j["window_width"].get<int>();
            if (w >= 320 && w <= 16384) {
                width = w;
            }
        }
        if (j.contains("window_height") && j["window_height"].is_number_integer()) {
            const int h = j["window_height"].get<int>();
            if (h >= 240 && h <= 16384) {
                height = h;
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "config/settings.json ignorado (parse): " << ex.what() << std::endl;
    }
}

} // namespace

void Game::LoadEnvVolume() {
    std::ifstream env(".env");
    if (!env.is_open()) {
        return;
    }
    std::string line;
    while (std::getline(env, line)) {
        const std::string trimmedLine = TrimWhitespace(line);
        if (trimmedLine.empty() || trimmedLine.front() == '#') {
            continue;
        }
        const auto eqPos = trimmedLine.find('=');
        if (eqPos == std::string::npos) {
            continue;
        }
        std::string key = TrimWhitespace(trimmedLine.substr(0, eqPos));
        std::string value = TrimWhitespace(trimmedLine.substr(eqPos + 1));
        if (key.empty()) {
            continue;
        }
        if (key == "MASTER_VOLUME") {
            try {
                const int vol = std::stoi(value);
                if (vol >= 0 && vol <= 100) {
                    masterVolumePercent = vol;
                }
            } catch (const std::exception&) {
                // valores malformados são ignorados
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

Game::Game(std::string title) {
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

    // Codec DLLs/formatos antes de Mix_OpenAudio ajudam Mix_LoadWAV_RW a aceitar mp3/ogg/flac como chunk.
    {
        const int wantFormats = MIX_INIT_MP3 | MIX_INIT_OGG | MIX_INIT_FLAC | MIX_INIT_WAVPACK | MIX_INIT_MOD;
        const int loaded = Mix_Init(wantFormats);
        if ((loaded & MIX_INIT_MP3) == 0 || (loaded & MIX_INIT_OGG) == 0) {
            std::cerr << "Aviso: Mix_Init codecs (mp3=" << ((loaded & MIX_INIT_MP3) != 0)
                      << ", ogg=" << ((loaded & MIX_INIT_OGG) != 0) << ") — " << Mix_GetError()
                      << std::endl;
        }
    }

    // Inicializa SDL_Mixer
    // Buffer um pouco maior ajuda a mixar Mix_PlayMusic (OST) + Mix_PlayChannel (ambiente) sem underruns.
    if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 4096) == -1) {
        std::cerr << "Mix_OpenAudio falhou: " << Mix_GetError() << std::endl;
        exit(1);
    }
    Mix_AllocateChannels(32);
    // Canais 0..N-1 não são escolhidos por Mix_PlayChannel(-1): o anel de ondas usa o canal 0 explicitamente
    // (ver StageState). Efeitos continuam em 1+.
    Mix_ReserveChannels(1);
    const int masterVolume = (MIX_MAX_VOLUME * masterVolumePercent) / 100;
    Mix_Volume(-1, masterVolume);
    Mix_VolumeMusic(masterVolume);

    // Inicializa TTF
    if (TTF_Init() != 0) {
        std::cerr << "TTF_Init falhou: " << TTF_GetError() << std::endl;
        exit(1);
    }

    int winW = WINDOW_WIDTH;
    int winH = WINDOW_HEIGHT;
    ApplyOptionalWindowSettingsFromFile(winW, winH);

    //Cria a janela
    window = SDL_CreateWindow(
        title.c_str(),                  // Coloquei o título como sendo argumento do construtor de Game também 
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        winW,
        winH,
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
    windowsWidth = winW;
    windowsHeight = winH;
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
        instance = new Game("The Last LightKeeper");
    }
    return *instance;                               // O compilador resolve como uma referência
}

State& Game::GetCurrentState() {                    // Retorna o State atual
    return *stateStack.top(); 
}

StageState* Game::TryGetStageState() {
    return dynamic_cast<StageState*>(&GetInstance().GetCurrentState());
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