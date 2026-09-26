#include "ui/KeyGlyphs.h"

#include "core/Resources.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <vector>

namespace {

const char* kDir = "Recursos/img/ui/teclas/";

/// Nome da tecla (como o SDL a escreve) -> ficheiro da arte. O que nao esta
/// aqui cai no caso geral (uma letra ou um algarismo) e, se nem isso, volta a
/// aparecer como texto.
///
/// Os nomes dos ficheiros nao seguem um padrao unico — vieram assim dos
/// designers — por isso a tabela e explicita em vez de montar o caminho a
/// partir do nome.
const std::unordered_map<std::string, std::string>& SpecialFiles() {
    static const std::unordered_map<std::string, std::string> m = {
        {"space",       "0707_HUD_TECLA_FLV__espa\xc3\xa7o.png"},   // "espaço", em UTF-8
        {"escape",      "0707_HUD_TECLA_FLV__esc.png"},
        {"esc",         "0707_HUD_TECLA_FLV__esc.png"},
        {"left ctrl",   "0707_HUD_TECLA_FLV__ctrl.png"},
        {"right ctrl",  "0707_HUD_TECLA_FLV__ctrl.png"},
        {"ctrl",        "0707_HUD_TECLA_FLV__ctrl.png"},
        {"left shift",  "0707_HUD_TECLA_FLV__shift.png"},
        {"right shift", "0707_HUD_TECLA_FLV__shift.png"},
        {"shift",       "0707_HUD_TECLA_FLV__shift.png"},
        {"left alt",    "0707_HUD_TECLA_FLV__alt.png"},
        {"right alt",   "0707_HUD_TECLA_FLV__alt.png"},
        {"alt",         "0707_HUD_TECLA_FLV__alt.png"},
        {"up",          "0707_HUD_TECLA__cima.png"},
        {"down",        "0707_HUD_TECLA__baixo.png"},
        {"right",       "0707_HUD_TECLA__direita.png"},
        {"left",        "HUD_TECLA__esquerda.png"},
        {"return",      "HUD_TECLA__Enter.png"},
        {"enter",       "HUD_TECLA__Enter.png"},
        {"backspace",   "HUD_TECLA__Backspace.png"},
        {"tab",         "0707_HUD_TECLA_FLV__Tab.png"},
        {"-",           "0707_HUD_TECLA_FLV__-.png"},
    };
    return m;
}

std::string ToLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

/// Textura da tecla, ou nullptr quando nao ha arte para ela. O resultado fica
/// EM CACHE — incluindo a ausencia. Sem isso, uma tecla sem arte mandava o
/// `Resources::GetImage` tentar abrir (e queixar-se) o ficheiro a cada frame.
std::shared_ptr<SDL_Texture> KeyTexture(const std::string& keyName) {
    static std::unordered_map<std::string, std::shared_ptr<SDL_Texture>> cache;
    const std::string key = ToLower(keyName);
    const auto cached = cache.find(key);
    if (cached != cache.end()) {
        return cached->second;
    }

    std::string file;
    const auto special = SpecialFiles().find(key);
    if (special != SpecialFiles().end()) {
        file = special->second;
    } else if (key.size() == 1 && (std::isalnum(static_cast<unsigned char>(key[0])) != 0)) {
        file = std::string("0707_HUD_TECLA_FLV__") + key[0] + ".png";
    }

    std::shared_ptr<SDL_Texture> tex;
    if (!file.empty()) {
        tex = Resources::GetImage(std::string(kDir) + file);
    }
    cache[key] = tex;   // guarda tambem o nullptr
    return tex;
}

/// Um pedaco da linha: texto puro ou uma tecla.
struct Piece {
    std::string text;                      ///< texto a escrever (vazio numa tecla)
    std::shared_ptr<SDL_Texture> key;      ///< arte da tecla (nullptr num texto)
    int w = 0;
    int h = 0;
};

/// Parte a linha em pedacos e mede cada um. `keyH` sai da altura da fonte.
std::vector<Piece> Split(TTF_Font* font, const std::string& text, int keyH) {
    std::vector<Piece> pieces;
    if (!font) {
        return pieces;
    }

    auto pushText = [&](const std::string& s) {
        if (s.empty()) return;
        Piece p;
        p.text = s;
        TTF_SizeUTF8(font, s.c_str(), &p.w, &p.h);
        pieces.push_back(p);
    };

    size_t i = 0;
    std::string run;
    while (i < text.size()) {
        if (text[i] == '[') {
            const size_t close = text.find(']', i + 1);
            if (close != std::string::npos) {
                const std::string name = text.substr(i + 1, close - i - 1);
                if (std::shared_ptr<SDL_Texture> tex = KeyTexture(name)) {
                    pushText(run);
                    run.clear();
                    int tw = 0, th = 0;
                    SDL_QueryTexture(tex.get(), nullptr, nullptr, &tw, &th);
                    Piece p;
                    p.key = tex;
                    p.h = keyH;
                    p.w = (th > 0) ? static_cast<int>(keyH * (static_cast<float>(tw) / th)) : keyH;
                    pieces.push_back(p);
                } else {
                    run += text.substr(i, close - i + 1);   // sem arte: fica escrito
                }
                i = close + 1;
                continue;
            }
        }
        run += text[i];
        i++;
    }
    pushText(run);
    return pieces;
}

int KeyHeightFor(TTF_Font* font, float keyScale) {
    const int fontH = font ? TTF_FontHeight(font) : 16;
    return std::max(8, static_cast<int>(fontH * keyScale));
}

}  // namespace

namespace KeyGlyphs {

void Measure(TTF_Font* font, const std::string& text, float keyScale, int& outW, int& outH) {
    outW = 0;
    outH = font ? TTF_FontHeight(font) : 0;
    const int keyH = KeyHeightFor(font, keyScale);
    for (const Piece& p : Split(font, text, keyH)) {
        outW += p.w;
        outH = std::max(outH, p.h);
    }
}

void Draw(SDL_Renderer* renderer, TTF_Font* font, const std::string& text,
          int x, int y, SDL_Color color, Uint8 alpha, float keyScale) {
    if (!renderer || !font) {
        return;
    }
    const int keyH = KeyHeightFor(font, keyScale);
    const std::vector<Piece> pieces = Split(font, text, keyH);

    int lineH = TTF_FontHeight(font);
    for (const Piece& p : pieces) {
        lineH = std::max(lineH, p.h);
    }

    int cursor = x;
    for (const Piece& p : pieces) {
        // Tudo alinhado pelo MEIO da linha: a tecla e mais alta do que o texto,
        // e sem isto as letras ficavam a flutuar no topo.
        const int py = y + (lineH - p.h) / 2;
        if (p.key) {
            SDL_SetTextureAlphaMod(p.key.get(), alpha);
            const SDL_Rect dst{cursor, py, p.w, p.h};
            SDL_RenderCopy(renderer, p.key.get(), nullptr, &dst);
        } else if (!p.text.empty()) {
            SDL_Surface* sf = TTF_RenderUTF8_Blended(font, p.text.c_str(), color);
            if (sf) {
                if (SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, sf)) {
                    SDL_SetTextureAlphaMod(t, alpha);
                    const SDL_Rect dst{cursor, py, sf->w, sf->h};
                    SDL_RenderCopy(renderer, t, nullptr, &dst);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(sf);
            }
        }
        cursor += p.w;
    }
}

}  // namespace KeyGlyphs
