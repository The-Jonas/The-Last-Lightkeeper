#include "core/Game.h"
#include "core/State.h"
#include "states/stage/StageState.h"
#include "core/InputManager.h"
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <exception>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#define INCLUDE_SDL_TTF
#define INCLUDE_SDL_IMAGE
#define INCLUDE_SDL_MIXER
#include "SDL_include.h"

#include "nlohmann/json.hpp"

#include <iostream>

Game* Game::instance = nullptr;
int Game::masterVolumePercent = Game::MASTER_VOLUME_PERCENT;
int Game::ambientVolumePercent = Game::AMBIENT_VOLUME_PERCENT;
int Game::sfxVolumePercent = Game::SFX_VOLUME_PERCENT;
int Game::voiceVolumePercent = Game::VOICE_VOLUME_PERCENT;
int Game::brightnessPercent = 100;
bool Game::fullscreen = false;
bool Game::reduceFlashing = false;
bool Game::debugMode = false;
int Game::displayMode = 0;        // 0 = sem bordas (padrão seguro)
int Game::resolutionIndex = 0;    // definido ao montar a lista (nativo = recomendado)
int Game::committedDisplayMode = 0;
int Game::committedResolutionIndex = 0;

namespace {

struct ResEntry { int w; int h; };

// Lista curada de resoluções (a nativa da área de trabalho é inserida se faltar
// e marcada como "Recomendado"). Montada uma vez em EnsureResolutionList().
std::vector<ResEntry> gResolutions;
int gRecommendedIndex = 0;

void EnsureResolutionList() {
    if (!gResolutions.empty()) return;
    gResolutions = {
        // Altas / ultrawide
        {5120, 1440}, {3840, 2160}, {3840, 1080}, {3440, 1440}, {2560, 1600},
        {2560, 1440}, {2560, 1080}, {1920, 1200},
        // Comuns
        {1920, 1080}, {1680, 1050}, {1600, 1024}, {1440, 1080}, {1440, 900},
        {1400, 1050}, {1366, 768},  {1360, 768},  {1280, 1024}, {1280, 960},
        {1280, 800},  {1280, 768},  {1280, 720},
    };
    // Resolução nativa da área de trabalho → garante presença + marca recomendada.
    int nativeW = 1920, nativeH = 1080;
    SDL_DisplayMode dm;
    if (SDL_GetDesktopDisplayMode(0, &dm) == 0 && dm.w > 0 && dm.h > 0) {
        nativeW = dm.w; nativeH = dm.h;
    }
    bool found = false;
    for (const auto& e : gResolutions) {
        if (e.w == nativeW && e.h == nativeH) { found = true; break; }
    }
    if (!found) gResolutions.push_back({nativeW, nativeH});
    // Ordena por área decrescente (maiores no topo, como no dropdown do exemplo).
    std::sort(gResolutions.begin(), gResolutions.end(),
              [](const ResEntry& a, const ResEntry& b) {
                  return (a.w * a.h) > (b.w * b.h);
              });
    gRecommendedIndex = 0;
    for (int i = 0; i < static_cast<int>(gResolutions.size()); ++i) {
        if (gResolutions[i].w == nativeW && gResolutions[i].h == nativeH) {
            gRecommendedIndex = i; break;
        }
    }
}

const char* kDisplayModeLabels[] = {"Sem bordas", "Tela cheia", "Janela"};
constexpr int kDisplayModeCount = 3;


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
        } else if (key == "AMBIENT_VOLUME") {
            try {
                const int vol = std::stoi(value);
                if (vol >= 0 && vol <= 100) {
                    ambientVolumePercent = vol;
                }
            } catch (const std::exception&) {
            }
        } else if (key == "VFX_VOLUME") {
            try {
                const int vol = std::stoi(value);
                if (vol >= 0 && vol <= 100) {
                    sfxVolumePercent = vol;
                }
            } catch (const std::exception&) {
            }
        } else if (key == "VOICE_VOLUME") {
            try {
                const int vol = std::stoi(value);
                if (vol >= 0 && vol <= 100) {
                    voiceVolumePercent = vol;
                }
            } catch (const std::exception&) {
            }
        } else if (key == "DEBUG") {
            if (value == "1" || value == "true" || value == "TRUE") {
                debugMode = true;
            }
        }
    }
}

int Game::MusicVolume() {
    int vol = (MIX_MAX_VOLUME * masterVolumePercent) / 100;
    vol = (vol * ambientVolumePercent) / 100;   // barramento "Fundo"
    return vol;
}

void Game::SetMasterVolume(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    masterVolumePercent = percent;
    // Blanket master nos canais (segurança) + música pelo barramento de fundo.
    Mix_Volume(-1, (MIX_MAX_VOLUME * masterVolumePercent) / 100);
    Mix_VolumeMusic(MusicVolume());
}

void Game::SetAmbientVolume(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    ambientVolumePercent = percent;
    // "Fundo" também controla a música — atualiza ao vivo.
    Mix_VolumeMusic(MusicVolume());
}

void Game::SetSfxVolume(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    sfxVolumePercent = percent;
}

void Game::SetVoiceVolume(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    voiceVolumePercent = percent;
}

void Game::SetBrightness(int percent) {
    if (percent < 50) percent = 50;
    if (percent > 150) percent = 150;
    brightnessPercent = percent;
}

void Game::SetFullscreen(bool on) {
    fullscreen = on;
    if (instance && instance->window) {
        SDL_SetWindowFullscreen(instance->window, on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    }
}

int Game::DisplayModeCount() { return kDisplayModeCount; }

const char* Game::DisplayModeLabel(int mode) {
    if (mode < 0 || mode >= kDisplayModeCount) mode = 0;
    return kDisplayModeLabels[mode];
}

const char* Game::CurrentDisplayModeLabel() { return DisplayModeLabel(displayMode); }

int Game::ResolutionCount() {
    EnsureResolutionList();
    return static_cast<int>(gResolutions.size());
}

int Game::RecommendedResolutionIndex() {
    EnsureResolutionList();
    return gRecommendedIndex;
}

void Game::ResolutionAt(int idx, int& w, int& h) {
    EnsureResolutionList();
    if (idx < 0 || idx >= static_cast<int>(gResolutions.size())) idx = gRecommendedIndex;
    w = gResolutions[static_cast<size_t>(idx)].w;
    h = gResolutions[static_cast<size_t>(idx)].h;
}

std::string Game::ResolutionLabelAt(int idx) {
    int w = 0, h = 0;
    ResolutionAt(idx, w, h);
    std::string s = std::to_string(w) + " x " + std::to_string(h);
    if (idx == RecommendedResolutionIndex()) s += " (Recomendado)";
    return s;
}

std::string Game::CurrentResolutionLabel() { return ResolutionLabelAt(resolutionIndex); }

void Game::SetResolutionIndex(int idx) {
    const int n = ResolutionCount();
    if (n <= 0) return;
    if (idx < 0) idx = 0;
    if (idx >= n) idx = n - 1;
    resolutionIndex = idx;
}

void Game::CycleDisplayMode(int delta) {
    displayMode = ((displayMode + delta) % kDisplayModeCount + kDisplayModeCount) % kDisplayModeCount;
    // Mantém o bool legado coerente (Sem bordas/Tela cheia = "fullscreen").
    fullscreen = (displayMode != 2);
}

void Game::CycleResolution(int delta) {
    const int n = ResolutionCount();
    if (n <= 0) return;
    resolutionIndex = ((resolutionIndex + delta) % n + n) % n;
}

// ── Aplicar/reverter vídeo (modo de tela + resolução aplicam no próximo boot) ──
bool Game::VideoSettingsDirty() {
    return displayMode != committedDisplayMode || resolutionIndex != committedResolutionIndex;
}

void Game::ApplyVideoSettings() {
    committedDisplayMode = displayMode;
    committedResolutionIndex = resolutionIndex;
    fullscreen = (displayMode != 2);
    SaveSettings();   // grava os valores já comprometidos
}

void Game::RevertVideoSettings() {
    displayMode = committedDisplayMode;
    resolutionIndex = committedResolutionIndex;
    fullscreen = (displayMode != 2);
}

void Game::RestartApplication() {
#ifdef _WIN32
    wchar_t path[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));
        if (CreateProcessW(path, nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }
#endif
    std::exit(0);   // encerra esta instância (a nova já foi lançada acima)
}

void Game::LoadSettings() {
    std::ifstream f("config/settings.json");
    if (!f.is_open()) {
        return;
    }
    try {
        nlohmann::json j;
        f >> j;
        auto readPercent = [&](const char* key, int& out) {
            if (j.contains(key) && j[key].is_number_integer()) {
                const int v = j[key].get<int>();
                if (v >= 0 && v <= 100) out = v;
            }
        };
        readPercent("master_volume", masterVolumePercent);
        readPercent("ambient_volume", ambientVolumePercent);
        readPercent("sfx_volume", sfxVolumePercent);
        readPercent("voice_volume", voiceVolumePercent);
        if (j.contains("brightness") && j["brightness"].is_number_integer()) {
            const int b = j["brightness"].get<int>();
            if (b >= 50 && b <= 150) brightnessPercent = b;
        }
        if (j.contains("fullscreen") && j["fullscreen"].is_boolean()) {
            fullscreen = j["fullscreen"].get<bool>();
        }
        // O jogo é SEMPRE tela cheia sem bordas — não há mais opção de modo de
        // janela. A "resolução" é a resolução lógica de render (ver Game ctor).
        displayMode = 0;
        resolutionIndex = RecommendedResolutionIndex();   // padrão = nativa
        if (j.contains("window_width") && j.contains("window_height") &&
            j["window_width"].is_number_integer() && j["window_height"].is_number_integer()) {
            const int w = j["window_width"].get<int>();
            const int h = j["window_height"].get<int>();
            for (int i = 0; i < ResolutionCount(); ++i) {
                int rw = 0, rh = 0;
                ResolutionAt(i, rw, rh);
                if (rw == w && rh == h) { resolutionIndex = i; break; }
            }
        }
        fullscreen = (displayMode != 2);
        // Baseline "comprometido" = o que foi carregado (usado pelo fluxo Aplicar).
        committedDisplayMode = displayMode;
        committedResolutionIndex = resolutionIndex;
        if (j.contains("reduce_flashing") && j["reduce_flashing"].is_boolean()) {
            reduceFlashing = j["reduce_flashing"].get<bool>();
        }
        if (j.contains("debug") && j["debug"].is_boolean() && j["debug"].get<bool>()) {
            debugMode = true;
        }
        if (j.contains("keybindings") && j["keybindings"].is_object()) {
            InputManager& im = InputManager::GetInstance();
            for (int i = 0; i < InputManager::ActionCount; ++i) {
                const GameAction action = static_cast<GameAction>(i);
                const char* name = InputManager::ActionName(action);
                if (j["keybindings"].contains(name) && j["keybindings"][name].is_string()) {
                    const std::string keyName = j["keybindings"][name].get<std::string>();
                    const SDL_Keycode kc = SDL_GetKeyFromName(keyName.c_str());
                    if (kc != SDLK_UNKNOWN) {
                        im.SetBinding(action, kc);
                    }
                }
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "config/settings.json ignorado (parse): " << ex.what() << std::endl;
    }
}

void Game::SaveSettings() {
    // Preserva chaves existentes (window_width/height/debug) lendo antes de gravar.
    nlohmann::json j = nlohmann::json::object();
    {
        std::ifstream f("config/settings.json");
        if (f.is_open()) {
            try {
                f >> j;
            } catch (const std::exception&) {
                j = nlohmann::json::object();
            }
        }
    }
    if (!j.is_object()) {
        j = nlohmann::json::object();
    }
    j["master_volume"] = masterVolumePercent;
    j["ambient_volume"] = ambientVolumePercent;
    j["sfx_volume"] = sfxVolumePercent;
    j["voice_volume"] = voiceVolumePercent;
    j["brightness"] = brightnessPercent;
    // Vídeo: grava os valores COMPROMETIDOS (só mudam via "Aplicar"), não os
    // pendentes que o usuário está pré-visualizando na UI.
    j["fullscreen"] = (committedDisplayMode != 2);
    j["reduce_flashing"] = reduceFlashing;
    j["display_mode"] = (committedDisplayMode == 0) ? "borderless" : (committedDisplayMode == 1) ? "fullscreen" : "windowed";
    {
        int rw = 0, rh = 0;
        ResolutionAt(committedResolutionIndex, rw, rh);
        j["window_width"] = rw;
        j["window_height"] = rh;
    }

    {
        InputManager& im = InputManager::GetInstance();
        nlohmann::json kb = nlohmann::json::object();
        for (int i = 0; i < InputManager::ActionCount; ++i) {
            const GameAction action = static_cast<GameAction>(i);
            kb[InputManager::ActionName(action)] = SDL_GetKeyName(im.GetBinding(action));
        }
        j["keybindings"] = kb;
    }

    std::ofstream out("config/settings.json", std::ios::trunc);
    if (out.is_open()) {
        out << j.dump(2) << std::endl;
    }
}

Game::Game(std::string title) {
    if (instance != nullptr) {
        std::cerr << "Erro: Já existe uma instância do Game rodando!" << std::endl;
        exit(1);
    }

    instance = this;                  // Define a instância atual

    LoadEnvVolume();                  // Carrega volume (e flag DEBUG) do .env (legado)
    srand(time(NULL));                // Inicializando o gerador de números aleátorios

    // Inicializa SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL_Init falhou: " << SDL_GetError() << std::endl;
        exit(1);
    }

    // Só APÓS SDL_Init(VIDEO) a resolução nativa pode ser consultada
    // (EnsureResolutionList usa SDL_GetDesktopDisplayMode).
    LoadSettings();                   // config/settings.json (fonte unificada)
    if (IsDebugBuild()) {
        debugMode = true;             // builds de debug sempre habilitam ferramentas de dev
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
    Mix_AllocateChannels(48);   // 0-13 fixos, 14-31 voz/one-shots, 32-47 pool de passos do monstro
    // Reserva 0..13 para os canais FIXOS de SFX (waves..heartbeat=10, pool de passos
    // do monstro=11..13). Antes reservava só 9, então Mix_PlayChannel(-1) — usado pela
    // VOZ e pelos one-shots — caía nos canais 9..13 e um passo do monstro (canal fixo
    // 11..13) HALTAVA a fala no meio da frase. Com 14 reservados, a voz/one-shots vão
    // para 14..31 e nunca colidem com os SFX fixos.
    Mix_ReserveChannels(14);
    // Canais 0..13 não são escolhidos por Mix_PlayChannel(-1): usados explicitamente
    // pelos loops/one-shots fixos (ver GameSfx). Voz e efeitos livres ficam em 14+.
    const int masterVolume = (MIX_MAX_VOLUME * masterVolumePercent) / 100;
    Mix_Volume(-1, masterVolume);
    Mix_VolumeMusic(MusicVolume());

    // Inicializa TTF
    if (TTF_Init() != 0) {
        std::cerr << "TTF_Init falhou: " << TTF_GetError() << std::endl;
        exit(1);
    }

    int winW = WINDOW_WIDTH;
    int winH = WINDOW_HEIGHT;
    ApplyOptionalWindowSettingsFromFile(winW, winH);

    // O jogo roda SEMPRE em tela cheia SEM BORDAS (borderless desktop). A
    // "resolução" escolhida nas Configurações é a resolução LÓGICA de render:
    // o jogo desenha nessa resolução e o SDL escala para preencher a tela
    // (SDL_RenderSetLogicalSize). Assim a resolução tem efeito sem trocar o modo
    // de vídeo do monitor nem sair do fullscreen sem bordas.
    (void)winW; (void)winH;
    int resW = 1920, resH = 1080;
    ResolutionAt(committedResolutionIndex, resW, resH);
    displayMode = 0;
    fullscreen = true;

    //Cria a janela em fullscreen borderless
    window = SDL_CreateWindow(
        title.c_str(),                  // Coloquei o título como sendo argumento do construtor de Game também
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        resW,
        resH,
        SDL_WINDOW_FULLSCREEN_DESKTOP
    );

    if (!window) {
        std::cerr << "SDL_CreateWindow falhou: " << SDL_GetError() << std::endl;
        exit(1);
    }

    // ===== [EXPERIMENTO: FILTRAGEM LINEAR GLOBAL] ============================
    // Filtragem LINEAR (bilinear) como PADRÃO de todas as texturas — a arte do
    // jogo é pintada (não pixel-art), então o (down/up)scale fica suave em vez de
    // "pixelado". Precisa ser definido ANTES de criar texturas/renderer.
    // >>> Se ficar ruim (borrado demais), REVERTER: remover esta linha. <<<
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");   // "0"=nearest (padrão), "1"=linear
    // =========================================================================

    //Cria Renderizador
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED); // SDL_RENDERER_ACCELERATED, para requisitar o uso de OpenGL ou Direct3D.
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer falhou: " << SDL_GetError() << std::endl;
        exit(1);
    }

    SDL_ShowCursor(SDL_DISABLE);   // esconde o cursor do mouse (o mouse ainda mira a lanterna)

    // Resolução LÓGICA de render = resolução escolhida. O SDL escala esse alvo
    // para preencher a tela cheia sem bordas (com letterbox se o aspecto diferir).
    SDL_RenderSetLogicalSize(renderer, resW, resH);

    storedState = nullptr;                         // Inicializa o ponteiro de troca de estado

    //Inicia membros — o "tamanho da janela" para o jogo é o ESPAÇO LÓGICO
    // (a resolução escolhida), não o tamanho físico da tela. Assim câmera, HUD e
    // alvos de render trabalham todos na resolução escolhida.
    windowsWidth = resW;
    windowsHeight = resH;
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
        instance = new Game("The Last Lightkeeper");
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

// Devolve o ESPAÇO LÓGICO (resolução escolhida), não o tamanho físico da tela —
// todo o jogo (câmera, HUD, alvos de render, mouse) trabalha nesse espaço.
int Game::GetWindowsWidth() {
    return windowsWidth;
}

int Game::GetWindowsHeight() {
    return windowsHeight;
}

// 1.0 a 1080p de altura; proporcional nas demais resoluções lógicas.
float Game::UiScale() {
    if (!instance || instance->windowsHeight <= 0) return 1.0f;
    float s = static_cast<float>(instance->windowsHeight) / 1080.0f;
    if (s < 0.55f) s = 0.55f;   // não deixa a UI ilegível em resoluções minúsculas
    if (s > 2.50f) s = 2.50f;
    return s;
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