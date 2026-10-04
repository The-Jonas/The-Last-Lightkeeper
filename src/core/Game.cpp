#include "core/Game.h"
#include "core/CrashHandler.h"
#include "core/InputManager.h"
#include "core/Telemetry.h"
#include "states/stage/StageState.h"
#include "ui/DialogueTuning.h"
#include "ui/FuelHudTuning.h"

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

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <fstream>
#include <iostream>
#include <typeinfo>
#include <vector>

namespace {

const char* kSettingsPath = "config/settings.json";
const char* kEnvPath      = ".env";

constexpr int   kReferenceW = 1920;          // resolução de referência da UI
constexpr int   kReferenceH = 1080;
constexpr float kUiScaleMin = 0.55f;
constexpr float kUiScaleMax = 2.50f;

constexpr int kMixChannels       = 48;       // 0-13 fixos, 14-31 voz/one-shots, 32-47 passos do monstro
constexpr int kReservedChannels  = 14;       // 0..13 nunca são escolhidos por Mix_PlayChannel(-1)
constexpr int kAudioBufferFrames = 4096;     // buffer maior evita underrun com música + ambiente

const char* kDisplayModeLabels[]   = {"Sem bordas", "Tela cheia", "Janela"};
const char* kDisplayModeSettings[] = {"borderless", "fullscreen", "windowed"};   // valor em settings.json

const int   kFpsCaps[]      = {0, 30, 60, 120, 144, 240};
const char* kFpsCapLabels[] = {"Sem limite", "30", "60", "120", "144", "240"};
constexpr int kFpsCapCount  = 6;

struct Resolution { int w; int h; };
std::vector<Resolution> gResolutions;
int gRecommendedIndex = 0;

// Monta (uma vez) a lista de resoluções, maiores primeiro, com a nativa garantida
// e marcada como recomendada. Precisa do SDL_Init(VIDEO) já feito.
void EnsureResolutionList() {
    if (!gResolutions.empty()) return;
    gResolutions = {
        {5120, 1440}, {3840, 2160}, {3840, 1080}, {3440, 1440}, {2560, 1600},
        {2560, 1440}, {2560, 1080}, {1920, 1200}, {1920, 1080}, {1680, 1050},
        {1600, 1024}, {1440, 1080}, {1440, 900},  {1400, 1050}, {1366, 768},
        {1360, 768},  {1280, 1024}, {1280, 960},  {1280, 800},  {1280, 768},
        {1280, 720},
    };

    int nativeW = kReferenceW, nativeH = kReferenceH;
    SDL_DisplayMode dm;
    if (SDL_GetDesktopDisplayMode(0, &dm) == 0 && dm.w > 0 && dm.h > 0) {
        nativeW = dm.w;
        nativeH = dm.h;
    }
    auto isNative = [&](const Resolution& r) { return r.w == nativeW && r.h == nativeH; };
    if (std::none_of(gResolutions.begin(), gResolutions.end(), isNative)) {
        gResolutions.push_back({nativeW, nativeH});
    }
    std::sort(gResolutions.begin(), gResolutions.end(),
              [](const Resolution& a, const Resolution& b) { return a.w * a.h > b.w * b.h; });
    gRecommendedIndex = static_cast<int>(
        std::find_if(gResolutions.begin(), gResolutions.end(), isNative) - gResolutions.begin());
}

// Tira espaços do começo e do fim.
std::string Trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

// Índice com volta (…, n-1, 0, 1, …) para ciclar listas.
int Wrap(int value, int n) {
    return ((value % n) + n) % n;
}

}  // namespace

Game* Game::instance = nullptr;

int  Game::masterVolumePercent  = 20;
int  Game::ambientVolumePercent = 50;
int  Game::sfxVolumePercent     = 100;
int  Game::voiceVolumePercent   = 100;

int  Game::brightnessPercent    = 100;
bool Game::brightnessCalibrated = false;
bool Game::reduceFlashing       = false;
bool Game::vsync                = true;
int  Game::fpsCapIndex          = 0;

int  Game::displayMode            = Game::kBorderless;
int  Game::resolutionIndex        = 0;   // LoadSettings define (padrão = nativa)
int  Game::appliedResolutionIndex = 0;

bool Game::captureWindowMode = false;
bool Game::debugMode         = false;

// ═════════════════════════════════════════════════════════════════════════════
//  Áudio
// ═════════════════════════════════════════════════════════════════════════════

// Master nos canais (teto de segurança) e na música pelo barramento de ambiente.
void Game::SetMasterVolume(int percent) {
    masterVolumePercent = std::clamp(percent, 0, 100);
    Mix_Volume(-1, (MIX_MAX_VOLUME * masterVolumePercent) / 100);
    Mix_VolumeMusic(MusicVolume());
}

// O ambiente também controla a música: atualiza na hora.
void Game::SetAmbientVolume(int percent) {
    ambientVolumePercent = std::clamp(percent, 0, 100);
    Mix_VolumeMusic(MusicVolume());
}

void Game::SetSfxVolume(int percent)   { sfxVolumePercent   = std::clamp(percent, 0, 100); }
void Game::SetVoiceVolume(int percent) { voiceVolumePercent = std::clamp(percent, 0, 100); }

// Volume de música/fundo = master × ambiente, em 0..MIX_MAX_VOLUME.
int Game::MusicVolume() {
    return (MIX_MAX_VOLUME * masterVolumePercent / 100) * ambientVolumePercent / 100;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Imagem
// ═════════════════════════════════════════════════════════════════════════════

void Game::SetBrightness(int percent) { brightnessPercent = std::clamp(percent, 50, 150); }

// Metade de cima do brilho: gama que clareia as sombras sem mexer no preto.
// 100 → 1.0 (neutro), 150 → 1.41.
float Game::BrightnessGamma() {
    const float b = static_cast<float>(std::clamp(brightnessPercent, 100, 150));
    return std::pow(2.0f, (b - 100.0f) / 100.0f);
}

// Metade de baixo: sobe o ponto de preto. 100 → 0 (neutro), 50 → 0.08 (≈20/255 vira preto).
float Game::BrightnessBlackPoint() {
    const float b = static_cast<float>(std::clamp(brightnessPercent, 50, 100));
    return (100.0f - b) / 50.0f * 0.08f;
}

// Liga/desliga o VSync no renderer atual (SDL 2.0.18+).
void Game::SetVSync(bool on) {
    vsync = on;
    if (instance && instance->renderer) {
        SDL_RenderSetVSync(instance->renderer, on ? 1 : 0);
    }
}

int Game::FpsCap()              { return kFpsCaps[std::clamp(fpsCapIndex, 0, kFpsCapCount - 1)]; }
const char* Game::FpsCapLabel() { return kFpsCapLabels[std::clamp(fpsCapIndex, 0, kFpsCapCount - 1)]; }
void Game::CycleFpsCap(int dir) { fpsCapIndex = Wrap(fpsCapIndex + dir, kFpsCapCount); }

// ═════════════════════════════════════════════════════════════════════════════
//  Modo de tela e resolução
// ═════════════════════════════════════════════════════════════════════════════

int Game::DisplayModeCount() { return kDisplayModeCount; }

const char* Game::DisplayModeLabel(int mode) {
    return kDisplayModeLabels[std::clamp(mode, 0, kDisplayModeCount - 1)];
}

const char* Game::CurrentDisplayModeLabel() { return DisplayModeLabel(displayMode); }

// Troca o modo de tela na hora e grava. O espaço lógico não muda (SDL_RenderSetLogicalSize),
// então o jogo continua igual em qualquer modo. Desliga o modo de gravação.
void Game::ApplyDisplayMode(int mode) {
    displayMode = std::clamp(mode, 0, kDisplayModeCount - 1);
    captureWindowMode = false;

    if (instance && instance->window) {
        SDL_Window* w = instance->window;
        int resW = kReferenceW, resH = kReferenceH;
        ResolutionAt(resolutionIndex, resW, resH);

        if (displayMode == kBorderless) {
            SDL_SetWindowFullscreen(w, SDL_WINDOW_FULLSCREEN_DESKTOP);
        } else if (displayMode == kFullscreen) {
            SDL_DisplayMode dm;
            SDL_zero(dm);                          // formato/Hz 0 = o SDL escolhe o mais próximo
            dm.w = resW;
            dm.h = resH;
            SDL_SetWindowDisplayMode(w, &dm);
            SDL_SetWindowFullscreen(w, SDL_WINDOW_FULLSCREEN);
        } else {
            SDL_SetWindowFullscreen(w, 0);
            SDL_SetWindowBordered(w, SDL_TRUE);
            // No máximo 90% da área de trabalho, mantendo a proporção.
            const int display = std::max(0, SDL_GetWindowDisplayIndex(w));
            SDL_DisplayMode desk;
            if (SDL_GetDesktopDisplayMode(display, &desk) == 0 && desk.w > 0 && desk.h > 0) {
                const float s = std::min({1.0f, 0.9f * desk.w / resW, 0.9f * desk.h / resH});
                resW = static_cast<int>(resW * s);
                resH = static_cast<int>(resH * s);
            }
            SDL_SetWindowSize(w, resW, resH);
            SDL_SetWindowPosition(w, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        }
    }
    SaveSettings();
}

int Game::ResolutionCount() {
    EnsureResolutionList();
    return static_cast<int>(gResolutions.size());
}

int Game::RecommendedResolutionIndex() {
    EnsureResolutionList();
    return gRecommendedIndex;
}

// Largura/altura da resolução `idx`; índice inválido cai na recomendada.
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

void Game::CycleResolution(int delta) {
    const int n = ResolutionCount();
    if (n > 0) resolutionIndex = Wrap(resolutionIndex + delta, n);
}

// Lança uma nova cópia do executável (Windows) e encerra esta.
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
    std::exit(0);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Modo de gravação
// ═════════════════════════════════════════════════════════════════════════════

// Ligado: janela com borda em 85% do ecrã (o DWM volta a compor e o OBS captura).
// Desligado: volta ao modo de tela escolhido nas Configurações.
void Game::SetCaptureWindowMode(bool on) {
    if (!instance || !instance->window) {
        captureWindowMode = on;
        return;
    }
    if (!on) {
        ApplyDisplayMode(displayMode);   // já zera captureWindowMode
        SDL_ShowCursor(SDL_DISABLE);
        return;
    }

    captureWindowMode = true;
    SDL_Window* w = instance->window;
    SDL_SetWindowFullscreen(w, 0);

    int px = 1280, py = 720;
    const int display = std::max(0, SDL_GetWindowDisplayIndex(w));
    SDL_DisplayMode dm;
    SDL_zero(dm);
    if (SDL_GetDesktopDisplayMode(display, &dm) == 0 && dm.w > 0 && dm.h > 0) {
        px = static_cast<int>(dm.w * 0.85f);
        py = static_cast<int>(dm.h * 0.85f);
    }
    SDL_SetWindowBordered(w, SDL_TRUE);
    SDL_SetWindowSize(w, px, py);
    SDL_SetWindowPosition(w, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowCursor(SDL_DISABLE);   // o SDL volta a mostrar o cursor ao trocar de modo
}

void Game::ToggleCaptureWindowMode() { SetCaptureWindowMode(!captureWindowMode); }

// ═════════════════════════════════════════════════════════════════════════════
//  Configurações
// ═════════════════════════════════════════════════════════════════════════════

// Lê settings.json. Chaves ausentes ou fora da faixa mantêm o valor atual.
void Game::LoadSettings() {
    resolutionIndex = RecommendedResolutionIndex();

    std::ifstream f(kSettingsPath);
    if (!f.is_open()) return;
    try {
        nlohmann::json j;
        f >> j;
        auto readInt = [&](const char* key, int lo, int hi, int& out) {
            if (j.contains(key) && j[key].is_number_integer()) {
                const int v = j[key].get<int>();
                if (v >= lo && v <= hi) out = v;
            }
        };
        auto readBool = [&](const char* key, bool& out) {
            if (j.contains(key) && j[key].is_boolean()) out = j[key].get<bool>();
        };

        readInt("master_volume", 0, 100, masterVolumePercent);
        readInt("ambient_volume", 0, 100, ambientVolumePercent);
        readInt("sfx_volume", 0, 100, sfxVolumePercent);
        readInt("voice_volume", 0, 100, voiceVolumePercent);
        readInt("brightness", 50, 150, brightnessPercent);
        readBool("brightness_calibrated", brightnessCalibrated);
        readBool("reduce_flashing", reduceFlashing);
        readBool("vsync", vsync);

        if (j.contains("display_mode") && j["display_mode"].is_string()) {
            const std::string m = j["display_mode"].get<std::string>();
            for (int i = 0; i < kDisplayModeCount; ++i) {
                if (m == kDisplayModeSettings[i]) displayMode = i;
            }
        }
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
        if (j.contains("fps_cap") && j["fps_cap"].is_number_integer()) {
            const int cap = j["fps_cap"].get<int>();
            for (int i = 0; i < kFpsCapCount; ++i) {
                if (kFpsCaps[i] == cap) { fpsCapIndex = i; break; }
            }
        }

        bool debug = false;
        readBool("debug", debug);
        if (debug) debugMode = true;

        if (j.contains("keybindings") && j["keybindings"].is_object()) {
            const auto& kb = j["keybindings"];
            InputManager& im = InputManager::GetInstance();
            for (int i = 0; i < InputManager::ActionCount; ++i) {
                const GameAction action = static_cast<GameAction>(i);
                const char* name = InputManager::ActionName(action);
                if (!kb.contains(name) || !kb[name].is_string()) continue;
                const SDL_Keycode kc = SDL_GetKeyFromName(kb[name].get<std::string>().c_str());
                if (kc != SDLK_UNKNOWN) im.SetBinding(action, kc);
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << kSettingsPath << " ignorado (parse): " << ex.what() << std::endl;
    }
}

// Grava settings.json por cima do existente, preservando chaves que o jogo não
// conhece (ex.: "debug" colocado à mão).
void Game::SaveSettings() {
    nlohmann::json j = nlohmann::json::object();
    {
        std::ifstream f(kSettingsPath);
        if (f.is_open()) {
            try { f >> j; } catch (const std::exception&) { j = nlohmann::json::object(); }
        }
    }
    if (!j.is_object()) j = nlohmann::json::object();

    j["master_volume"]         = masterVolumePercent;
    j["ambient_volume"]        = ambientVolumePercent;
    j["sfx_volume"]            = sfxVolumePercent;
    j["voice_volume"]          = voiceVolumePercent;
    j["brightness"]            = brightnessPercent;
    j["brightness_calibrated"] = brightnessCalibrated;
    j["reduce_flashing"]       = reduceFlashing;
    j["vsync"]                 = vsync;
    j["fps_cap"]               = FpsCap();
    j["display_mode"]          = kDisplayModeSettings[std::clamp(displayMode, 0, kDisplayModeCount - 1)];

    int rw = 0, rh = 0;
    ResolutionAt(resolutionIndex, rw, rh);
    j["window_width"]  = rw;
    j["window_height"] = rh;

    nlohmann::json kb = nlohmann::json::object();
    InputManager& im = InputManager::GetInstance();
    for (int i = 0; i < InputManager::ActionCount; ++i) {
        const GameAction action = static_cast<GameAction>(i);
        kb[InputManager::ActionName(action)] = SDL_GetKeyName(im.GetBinding(action));
    }
    j["keybindings"] = kb;
    j.erase("fullscreen");   // chave antiga, substituída por display_mode

    std::ofstream out(kSettingsPath, std::ios::trunc);
    if (out.is_open()) out << j.dump(2) << std::endl;
}

// .env legado: MASTER_VOLUME, AMBIENT_VOLUME, VFX_VOLUME, VOICE_VOLUME (0..100) e DEBUG.
// Lido antes do settings.json, que tem a palavra final.
void Game::LoadEnvFile() {
    std::ifstream env(kEnvPath);
    if (!env.is_open()) return;

    std::string line;
    while (std::getline(env, line)) {
        line = Trim(line);
        const auto eq = line.find('=');
        if (line.empty() || line.front() == '#' || eq == std::string::npos) continue;
        const std::string key   = Trim(line.substr(0, eq));
        const std::string value = Trim(line.substr(eq + 1));

        int* target = nullptr;
        if (key == "MASTER_VOLUME")       target = &masterVolumePercent;
        else if (key == "AMBIENT_VOLUME") target = &ambientVolumePercent;
        else if (key == "VFX_VOLUME")     target = &sfxVolumePercent;
        else if (key == "VOICE_VOLUME")   target = &voiceVolumePercent;
        else if (key == "DEBUG" && (value == "1" || value == "true" || value == "TRUE")) debugMode = true;

        if (target) {
            try {
                const int v = std::stoi(value);
                if (v >= 0 && v <= 100) *target = v;
            } catch (const std::exception&) {}   // valor malformado: ignora
        }
    }
}

bool Game::IsDebugBuild() {
#ifdef DEBUG
    return true;
#else
    return false;
#endif
}

// ═════════════════════════════════════════════════════════════════════════════
//  Construção e destruição
// ═════════════════════════════════════════════════════════════════════════════

// Ordem: .env → SDL → settings.json (precisa do SDL para a resolução nativa) →
// volumes → janela/renderer → modo de tela salvo → telemetria.
Game::Game(const std::string& title) {
    if (instance != nullptr) {
        std::cerr << "Erro: Já existe uma instância do Game rodando!" << std::endl;
        std::exit(1);
    }
    instance = this;

    LoadEnvFile();
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    InitSdl();

    LoadSettings();
    DialogueTuning::Load();
    FuelHudTuning::Load();
    if (IsDebugBuild()) debugMode = true;
    SetMasterVolume(masterVolumePercent);   // aplica master e música no mixer

    CreateWindowAndRenderer(title);

    if (displayMode != kBorderless) ApplyDisplayMode(displayMode);

    if (const char* wm = SDL_getenv("TLL_WINDOW_MODE")) {
        if (SDL_strcasecmp(wm, "windowed") == 0) SetCaptureWindowMode(true);
    }

    LogEnvironment();
}

// Inicializa SDL, SDL_image, SDL_mixer (codecs + canais) e SDL_ttf; aborta se algum falhar.
void Game::InitSdl() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL_Init falhou: " << SDL_GetError() << std::endl;
        std::exit(1);
    }

    const int imgFlags = IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_TIF;
    if (!(IMG_Init(imgFlags) & imgFlags)) {
        std::cerr << "IMG_Init falhou: " << IMG_GetError() << std::endl;
        std::exit(1);
    }

    // Codecs antes do Mix_OpenAudio: deixa o Mix_LoadWAV_RW aceitar mp3/ogg/flac como chunk.
    const int loaded = Mix_Init(MIX_INIT_MP3 | MIX_INIT_OGG | MIX_INIT_FLAC | MIX_INIT_WAVPACK | MIX_INIT_MOD);
    if ((loaded & MIX_INIT_MP3) == 0 || (loaded & MIX_INIT_OGG) == 0) {
        std::cerr << "Aviso: Mix_Init codecs (mp3=" << ((loaded & MIX_INIT_MP3) != 0)
                  << ", ogg=" << ((loaded & MIX_INIT_OGG) != 0) << ") — " << Mix_GetError() << std::endl;
    }
    if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, kAudioBufferFrames) == -1) {
        std::cerr << "Mix_OpenAudio falhou: " << Mix_GetError() << std::endl;
        std::exit(1);
    }
    // Os canais fixos do GameSfx (0..13) ficam reservados; se não, a voz e os
    // one-shots (Mix_PlayChannel(-1)) caíam neles e um passo do monstro cortava a fala.
    Mix_AllocateChannels(kMixChannels);
    Mix_ReserveChannels(kReservedChannels);

    if (TTF_Init() != 0) {
        std::cerr << "TTF_Init falhou: " << TTF_GetError() << std::endl;
        std::exit(1);
    }
}

// Cria a janela sem bordas na resolução escolhida e o renderer (prefere OpenGL,
// que o ScenePostFx precisa; cai no automático se falhar). Define o espaço lógico.
void Game::CreateWindowAndRenderer(const std::string& title) {
    int resW = kReferenceW, resH = kReferenceH;
    ResolutionAt(resolutionIndex, resW, resH);
    appliedResolutionIndex = resolutionIndex;

    window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              resW, resH, SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!window) {
        std::cerr << "SDL_CreateWindow falhou: " << SDL_GetError() << std::endl;
        std::exit(1);
    }

    // Filtragem linear em todas as texturas: a arte é pintada, não pixel-art.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    // TLL_RENDER_DRIVER=direct3d força o backend antigo (sem o filtro do ScenePostFx).
    const char* forcedDriver = SDL_getenv("TLL_RENDER_DRIVER");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, (forcedDriver && forcedDriver[0]) ? forcedDriver : "opengl");
    SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer (opengl) falhou: " << SDL_GetError()
                  << " — a tentar o backend automatico." << std::endl;
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "");
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    }
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer falhou: " << SDL_GetError() << std::endl;
        std::exit(1);
    }

    SDL_RenderSetVSync(renderer, vsync ? 1 : 0);
    SDL_RenderSetLogicalSize(renderer, kReferenceW, kReferenceH);
    SDL_ShowCursor(SDL_DISABLE);   // o mouse continua mirando a lanterna

    SDL_RendererInfo info;
    SDL_zero(info);
    if (SDL_GetRendererInfo(renderer, &info) == 0 && info.name) {
        std::cout << "[Render] backend SDL: " << info.name << std::endl;
    }

    windowsWidth  = kReferenceW;   
    windowsHeight = kReferenceH;
}

// Evento "env" da telemetria: máquina do tester e configurações com que abriu.
void Game::LogEnvironment() {
    SDL_RendererInfo info;
    SDL_zero(info);
    const char* backend = (SDL_GetRendererInfo(renderer, &info) == 0 && info.name) ? info.name : "?";
    SDL_DisplayMode dm;
    SDL_zero(dm);
    SDL_GetDesktopDisplayMode(0, &dm);
    Telemetry::Event("env", Telemetry::Fields()
        .Str("renderer", backend)
        .Int("logicalW", windowsWidth)
        .Int("logicalH", windowsHeight)
        .Int("desktopW", dm.w)
        .Int("desktopH", dm.h)
        .Int("refreshHz", dm.refresh_rate)
        .Int("cpuCores", SDL_GetCPUCount())
        .Int("ramMB", SDL_GetSystemRAM())
        .Str("displayMode", CurrentDisplayModeLabel())
        .Str("resolution", CurrentResolutionLabel())
        .Int("volMaster", masterVolumePercent)
        .Int("volAmbient", ambientVolumePercent)
        .Int("volSfx", sfxVolumePercent)
        .Int("volVoice", voiceVolumePercent)
        .Int("brightness", brightnessPercent)
        .Bool("reduceFlashing", reduceFlashing));
}

// Esvazia os estados antes de destruir o renderer (eles podem ter texturas) e encerra o SDL.
Game::~Game() {
    pendingState.reset();
    while (!stateStack.empty()) stateStack.pop();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    Mix_CloseAudio();
    Mix_Quit();
    IMG_Quit();
    SDL_Quit();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Instância, estados e loop
// ═════════════════════════════════════════════════════════════════════════════

Game& Game::GetInstance() {
    if (!instance) instance = new Game("The Last Lightkeeper");
    return *instance;
}

State& Game::GetCurrentState() { return *stateStack.top(); }

StageState* Game::TryGetStageState() {
    Game& g = GetInstance();
    if (g.stateStack.empty()) return nullptr;
    return dynamic_cast<StageState*>(g.stateStack.top().get());
}

SDL_Renderer* Game::GetRenderer()  { return renderer; }
SDL_Window*   Game::GetWindow()    { return window; }
float Game::GetDeltaTime()         { return dt; }
int   Game::GetWindowsWidth()      { return windowsWidth; }
int   Game::GetWindowsHeight()     { return windowsHeight; }

// Guarda o estado para entrar na pilha no começo do próximo frame (log + telemetria).
void Game::Push(State* state) {
    if (state) {
        CrashHandler::Log("Push estado: %s", typeid(*state).name());
        Telemetry::Event("state_push", Telemetry::Fields().Str("state", typeid(*state).name()));
    }
    pendingState.reset(state);
}

// Escala da UI pela altura lógica (1.0 em 1080p), limitada para não sumir nem estourar.
float Game::UiScale() {
    if (!instance || instance->windowsHeight <= 0) return 1.0f;
    return std::clamp(static_cast<float>(instance->windowsHeight) / kReferenceH, kUiScaleMin, kUiScaleMax);
}

// Escala que sempre cabe no ecrã: a menor entre largura e altura. Para peças
// largas (a caixa de diálogo, 2.54:1), que pela altura estourariam em 21:9.
float Game::UiFitScale() {
    if (!instance || instance->windowsWidth <= 0 || instance->windowsHeight <= 0) return 1.0f;
    const float byW = static_cast<float>(instance->windowsWidth) / kReferenceW;
    const float byH = static_cast<float>(instance->windowsHeight) / kReferenceH;
    return std::clamp(std::min(byW, byH), kUiScaleMin, kUiScaleMax);
}

void Game::CalculateDeltaTime() {
    const Uint32 now = SDL_GetTicks();
    dt = (now - frameStart) / 1000.0f;
    frameStart = now;
}

// Aplica o pop pedido pelo estado do topo (e retoma o de baixo) e depois o push pendente.
void Game::ApplyPendingStackChanges() {
    if (!stateStack.empty() && stateStack.top()->PopRequested()) {
        stateStack.pop();
        if (!stateStack.empty()) stateStack.top()->Resume();
    }
    if (pendingState) {
        if (!stateStack.empty()) stateStack.top()->Pause();
        stateStack.push(std::move(pendingState));
        stateStack.top()->Start();
    }
}

// Dorme o que falta para completar um quadro no limite de FPS escolhido.
void Game::LimitFrameRate(Uint64 frameBegin) {
    const int cap = FpsCap();
    if (cap <= 0) return;
    const double elapsedMs = (SDL_GetPerformanceCounter() - frameBegin) * 1000.0 / SDL_GetPerformanceFrequency();
    const double targetMs  = 1000.0 / cap;
    if (elapsedMs < targetMs) SDL_Delay(static_cast<Uint32>(targetMs - elapsedMs));
}

// Loop: tempo → input → atalhos globais (F11) → pilha → update/render → limite de FPS.
void Game::Run() {
    if (pendingState) {
        stateStack.push(std::move(pendingState));
        stateStack.top()->Start();
    }

    InputManager& input = InputManager::GetInstance();
    while (!stateStack.empty() && !stateStack.top()->QuitRequested()) {
        CalculateDeltaTime();
        Telemetry::FrameTick(dt);
        input.Update();

        if (input.QuitRequested()) Telemetry::SetEndReason("window_close");
        if (input.KeyPress(SDLK_F11)) ToggleCaptureWindowMode();

        ApplyPendingStackChanges();

        const Uint64 frameBegin = SDL_GetPerformanceCounter();
        if (!stateStack.empty()) {
            stateStack.top()->Update(dt);
            SDL_RenderClear(renderer);
            stateStack.top()->Render();
            SDL_RenderPresent(renderer);
        }
        LimitFrameRate(frameBegin);
    }
}