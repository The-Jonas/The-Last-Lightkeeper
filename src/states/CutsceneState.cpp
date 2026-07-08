#include "states/CutsceneState.h"
#include "ui/VideoPlayer.h"
#include "states/stage/StageState.h"
#include "core/Game.h"
#include "core/InputManager.h"
#include "core/Resources.h"
#include "engine/Camera.h"
#include "math/Vec2.h"

#define INCLUDE_SDL
#define INCLUDE_SDL_TTF
#define INCLUDE_SDL_MIXER
#include "SDL_include.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
// Converte "HH:MM:SS,mmm" (ou com '.') em segundos; -1 se inválido.
double ParseSrtTime(const std::string& s) {
    int h = 0, m = 0, sec = 0, ms = 0;
    if (std::sscanf(s.c_str(), "%d:%d:%d,%d", &h, &m, &sec, &ms) == 4 ||
        std::sscanf(s.c_str(), "%d:%d:%d.%d", &h, &m, &sec, &ms) == 4) {
        return h * 3600.0 + m * 60.0 + sec + ms / 1000.0;
    }
    return -1.0;
}

void TrimInPlace(std::string& str) {
    const size_t a = str.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) {
        str.clear();
        return;
    }
    const size_t b = str.find_last_not_of(" \t\r\n");
    str = str.substr(a, b - a + 1);
}
} // namespace

CutsceneState::CutsceneState(std::string videoPath, std::string audioPath, State* nextState) : videoPath(videoPath), audioPath(audioPath), nextState(nextState) {
    // Construtor recebe os caminhos do vídeo e do áudio
}

CutsceneState::~CutsceneState() {
    objectArray.clear();
    // Se a cena for destruída sem consumir o próximo estado (ex.: fechar o jogo
    // durante a cutscene), evita vazar o StageState pré-carregado.
    if (nextState != nullptr) {
        delete nextState;
        nextState = nullptr;
    }
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

    // Legendas opcionais: mesmo nome-base do vídeo, com extensão .srt.
    std::string srt = videoPath;
    const size_t dot = srt.find_last_of('.');
    if (dot != std::string::npos) {
        srt = srt.substr(0, dot);
    }
    srt += ".srt";
    LoadSubtitles(srt);
}

void CutsceneState::LoadSubtitles(const std::string& srtPath) {
    subtitles.clear();
    std::ifstream f(srtPath);
    if (!f.is_open()) {
        return;   // sem arquivo → cutscene sem legendas
    }

    std::string line;
    while (std::getline(f, line)) {
        // Avança até a linha de tempo "start --> end" (pula índices/linhas vazias).
        const size_t arrow = line.find("-->");
        if (arrow == std::string::npos) {
            continue;
        }

        std::string startStr = line.substr(0, arrow);
        std::string endStr = line.substr(arrow + 3);
        TrimInPlace(startStr);
        TrimInPlace(endStr);
        // O fim pode ter coordenadas de posição depois do tempo — fica só o 1º token.
        const size_t sp = endStr.find(' ');
        if (sp != std::string::npos) {
            endStr = endStr.substr(0, sp);
        }

        SubtitleCue cue;
        cue.start = ParseSrtTime(startStr);
        cue.end = ParseSrtTime(endStr);
        if (cue.start < 0.0 || cue.end < 0.0) {
            continue;
        }

        // Texto: linhas seguintes até uma linha em branco.
        std::string text;
        while (std::getline(f, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line.empty()) {
                break;
            }
            if (!text.empty()) {
                text += "\n";
            }
            text += line;
        }
        if (!text.empty()) {
            cue.text = std::move(text);
            subtitles.push_back(std::move(cue));
        }
    }
}

void CutsceneState::Start() {
    // O vídeo é renderizado em coordenadas de mundo menos Camera::pos; como o
    // stage pré-carregado pode ter mexido nos alvos da câmera, zeramos a posição
    // para o vídeo cobrir a tela a partir de (0,0).
    Camera::pos = Vec2(0, 0);

    LoadAssets();
    // OBS: o áudio NÃO começa aqui. Ele é iniciado no 1º Update junto do 1º avanço
    // do vídeo, para que imagem e som partam exatamente do mesmo instante.
    StartArray();
}

void CutsceneState::Finish() {
    if (finished) {
        return;
    }
    finished = true;
    backgroundAudio.Stop(0);
    popRequested = true;
    if (nextState != nullptr) {
        Game::GetInstance().Push(nextState);
        nextState = nullptr;
    }
}

void CutsceneState::Update(float dt) {
    InputManager& input = InputManager::GetInstance();

    if (input.QuitRequested()) {
        quitRequested = true;
        return;
    }

    // Clamp do dt para o vídeo: evita que um hitch (ex.: o 1º frame logo após o
    // carregamento do stage) faça a imagem pular à frente do áudio.
    float videoDt = dt;
    if (videoDt > 0.1f) {
        videoDt = 1.0f / 60.0f;
    }

    // Inicia o áudio junto do 1º avanço do vídeo → começam sincronizados.
    if (!audioStarted) {
        audioStarted = true;
        if (backgroundAudio.IsOpen()) {
            Mix_VolumeMusic(Game::MusicVolume());
            backgroundAudio.Play(1);
        }
    } else {
        // Relógio da cena para as legendas: usa o dt REAL (não o clampado do
        // vídeo) porque o áudio toca em tempo real — as legendas seguem a fala.
        playbackSec += dt;
    }

    // Detecção de atividade: qualquer input reexibe o tooltip; a ociosidade o some.
    bool activity = false;
    if (input.IsKeyDown(SPACE_KEY) || input.PollAnyKeyPressed() != 0) {
        activity = true;
    }
    if (input.IsMouseDown(LEFT_MOUSE_BUTTON)) {
        activity = true;
    }
    const int mx = input.GetMouseX();
    const int my = input.GetMouseY();
    if (mx != lastMouseX || my != lastMouseY) {
        activity = true;
        lastMouseX = mx;
        lastMouseY = my;
    }

    idleTimer = activity ? 0.0f : (idleTimer + dt);
    const float target = (idleTimer < kTooltipHideDelay) ? 1.0f : 0.0f;
    const float step = kTooltipFadeSpeed * dt;
    if (tooltipAlpha < target) {
        tooltipAlpha = std::min(target, tooltipAlpha + step);
    } else {
        tooltipAlpha = std::max(target, tooltipAlpha - step);
    }

    // Pular: acumula enquanto ESPAÇO estiver pressionado; solta = zera o progresso.
    if (input.IsKeyDown(SPACE_KEY)) {
        skipHoldTimer += dt;
        if (skipHoldTimer >= kSkipHoldDuration) {
            Finish();
            return;
        }
    } else {
        skipHoldTimer = 0.0f;
    }

    UpdateArray(videoDt);

    // Fim natural do vídeo → segue para o próximo estado.
    for (auto& go : objectArray) {
        VideoPlayer* vp = go->GetComponent<VideoPlayer>();
        if (vp && vp->HasEnded()) {
            Finish();
            return;
        }
    }
}

void CutsceneState::Render() {
    RenderArray();
    RenderSubtitle(Game::GetInstance().GetRenderer());
    RenderSkipHint(Game::GetInstance().GetRenderer());
}

void CutsceneState::RenderSubtitle(SDL_Renderer* renderer) {
    if (!renderer || subtitles.empty()) {
        return;
    }

    const std::string* active = nullptr;
    for (const SubtitleCue& c : subtitles) {
        if (playbackSec >= c.start && playbackSec <= c.end) {
            active = &c.text;
            break;
        }
    }
    if (!active) {
        return;
    }

    const int winW = Game::GetInstance().GetWindowsWidth();
    const int winH = Game::GetInstance().GetWindowsHeight();

    auto font = Resources::GetFont("Recursos/font/times.ttf", 30);
    if (!font) {
        return;
    }
    SDL_Color col{240, 238, 228, 255};
    const Uint32 wrap = static_cast<Uint32>(winW * 0.8f);
    SDL_Surface* sf = TTF_RenderUTF8_Blended_Wrapped(font.get(), active->c_str(), col, wrap);
    if (!sf) {
        return;
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, sf);
    const int tw = sf->w, th = sf->h;
    SDL_FreeSurface(sf);
    if (!tex) {
        return;
    }

    const int pad = 16;
    const int boxW = tw + pad * 2;
    const int boxH = th + pad * 2;
    const int boxX = (winW - boxW) / 2;
    const int boxY = winH - boxH - 70;   // barra inferior, acima do anel de skip

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 165);   // faixa escura p/ legibilidade
    SDL_Rect bg{boxX, boxY, boxW, boxH};
    SDL_RenderFillRect(renderer, &bg);

    SDL_Rect dst{boxX + pad, boxY + pad, tw, th};
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
}

void CutsceneState::RenderSkipHint(SDL_Renderer* renderer) {
    if (!renderer || finished || tooltipAlpha <= 0.01f) {
        return;
    }

    const int winW = Game::GetInstance().GetWindowsWidth();
    const int winH = Game::GetInstance().GetWindowsHeight();

    const float progress = std::min(1.0f, skipHoldTimer / kSkipHoldDuration);
    const float fade = std::min(1.0f, std::max(0.0f, tooltipAlpha));
    auto A = [&](float base) { return static_cast<Uint8>(base * fade); };

    // Geometria do canto inferior direito.
    const int outerR = 24;
    const int innerR = 17;   // anel de ~7px de espessura
    const int cx = winW - 64;
    const int cy = winH - 64;
    const float top = -static_cast<float>(M_PI) / 2.0f;   // 12 horas

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Disco de fundo (contraste sobre o vídeo).
    auto fillDisc = [&](int r, SDL_Color c) {
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
        for (int dy = -r; dy <= r; ++dy) {
            const int dx = static_cast<int>(std::sqrt(std::max(0.0f, static_cast<float>(r * r - dy * dy))));
            SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
        }
    };

    // Arco espesso (annulus) entre innerR e outerR, de a0 a a1 (radianos).
    auto arc = [&](float a0, float a1, SDL_Color c) {
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
        const int steps = std::max(2, static_cast<int>(std::fabs(a1 - a0) * outerR * 1.6f));
        for (int i = 0; i <= steps; ++i) {
            const float a = a0 + (a1 - a0) * (static_cast<float>(i) / steps);
            const float dx = std::cos(a);
            const float dy = std::sin(a);
            SDL_RenderDrawLine(renderer,
                cx + static_cast<int>(dx * innerR), cy + static_cast<int>(dy * innerR),
                cx + static_cast<int>(dx * outerR), cy + static_cast<int>(dy * outerR));
        }
    };

    fillDisc(outerR + 3, SDL_Color{0, 0, 0, A(120.0f)});          // halo escuro
    arc(0.0f, 2.0f * static_cast<float>(M_PI), SDL_Color{235, 235, 235, A(70.0f)});   // trilha
    if (progress > 0.0f) {                                        // progresso âmbar
        arc(top, top + progress * 2.0f * static_cast<float>(M_PI), SDL_Color{240, 214, 160, A(245.0f)});
    }

    // Ícone central "»" (indica "pular"), esmaece junto. A superfície do TTF
    // inclui o espaço da linha (ascent/descent), então centralizar a caixa deixa
    // o glifo alto; usamos as métricas de tinta do glifo para centralizar de fato.
    auto glyphFont = Resources::GetFont("Recursos/font/times.ttf", 18);
    if (glyphFont) {
        SDL_Color gc{235, 235, 235, A(230.0f)};
        SDL_Surface* gs = TTF_RenderUTF8_Blended(glyphFont.get(), "»", gc);
        if (gs) {
            SDL_Texture* gt = SDL_CreateTextureFromSurface(renderer, gs);
            const int gw = gs->w, gh = gs->h;
            SDL_FreeSurface(gs);
            if (gt) {
                int inkCenterFromTop = gh / 2;   // fallback
                int gminx, gmaxx, gminy, gmaxy, gadv;
                if (TTF_GlyphMetrics(glyphFont.get(), 0x00BB, &gminx, &gmaxx, &gminy, &gmaxy, &gadv) == 0) {
                    inkCenterFromTop = TTF_FontAscent(glyphFont.get()) - (gmaxy + gminy) / 2;
                }
                SDL_SetTextureAlphaMod(gt, A(230.0f));
                SDL_Rect gd{cx - gw / 2, cy - inkCenterFromTop, gw, gh};
                SDL_RenderCopy(renderer, gt, nullptr, &gd);
                SDL_DestroyTexture(gt);
            }
        }
    }

    // Tooltip "Segure ESPAÇO para pular" à esquerda do anel.
    auto font = Resources::GetFont("Recursos/font/times.ttf", 20);
    if (font) {
        SDL_Color col{225, 225, 225, 255};
        SDL_Surface* sf = TTF_RenderUTF8_Blended(font.get(), "Segure ESPAÇO para pular", col);
        if (sf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, sf);
            const int tw = sf->w, th = sf->h;
            SDL_FreeSurface(sf);
            if (tex) {
                SDL_SetTextureAlphaMod(tex, A(230.0f));
                SDL_Rect dst{cx - outerR - 16 - tw, cy - th / 2, tw, th};
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
        }
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
}

void CutsceneState::Pause() {
    // Vazio
}

void CutsceneState::Resume() {
    // Vazio
}
