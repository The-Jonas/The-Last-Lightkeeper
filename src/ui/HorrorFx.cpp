#include "ui/HorrorFx.h"

#include "core/Game.h"
#include "core/Resources.h"
#include "ui/KeyGlyphs.h"

#define INCLUDE_SDL_TTF
#define INCLUDE_SDL_IMAGE
#include "SDL_include.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr float kTwoPi = 6.28318530718f;
const char* kHintFont = "Recursos/font/times.ttf";
constexpr int   kHintFontPx  = 22;                       // em 1080p
constexpr float kHintKeyScale = 2.4f;                    // um pouco menor que o banner antigo (3.0)
constexpr int   kLabelFontPx  = 26;                      // rótulo ao lado da tecla ("Usar item")

// Número pseudoaleatório estável 0..1 para um inteiro (o mesmo n dá sempre o mesmo valor).
float Hash01(int n) {
    const float s = std::sin(static_cast<float>(n) * 12.9898f + 78.233f) * 43758.5453f;
    return s - std::floor(s);
}

SDL_Vertex Vert(float x, float y, Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    SDL_Vertex v;
    v.position = {x, y};
    v.color = {r, g, b, a};
    v.tex_coord = {0.0f, 0.0f};
    return v;
}

// Linha grossa por uma lista de pontos (quads em SDL_RenderGeometry).
void DrawThickPolyline(SDL_Renderer* renderer, const std::vector<SDL_FPoint>& pts, float thickness,
                       SDL_Color c) {
    if (pts.size() < 2) return;
    std::vector<SDL_Vertex> verts;
    std::vector<int> idx;
    verts.reserve(pts.size() * 4);
    idx.reserve(pts.size() * 6);
    const float half = thickness * 0.5f;
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        const float dx = pts[i + 1].x - pts[i].x;
        const float dy = pts[i + 1].y - pts[i].y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.001f) continue;
        const float nx = -dy / len * half;
        const float ny =  dx / len * half;
        const int base = static_cast<int>(verts.size());
        verts.push_back(Vert(pts[i].x + nx,     pts[i].y + ny,     c.r, c.g, c.b, c.a));
        verts.push_back(Vert(pts[i].x - nx,     pts[i].y - ny,     c.r, c.g, c.b, c.a));
        verts.push_back(Vert(pts[i + 1].x + nx, pts[i + 1].y + ny, c.r, c.g, c.b, c.a));
        verts.push_back(Vert(pts[i + 1].x - nx, pts[i + 1].y - ny, c.r, c.g, c.b, c.a));
        idx.insert(idx.end(), {base, base + 1, base + 2, base + 1, base + 3, base + 2});
    }
    if (verts.empty()) return;
    SDL_RenderGeometry(renderer, nullptr, verts.data(), static_cast<int>(verts.size()),
                       idx.data(), static_cast<int>(idx.size()));
}

// Disco cheio (leque de triângulos) numa cor só.
void FillDisc(SDL_Renderer* renderer, float cx, float cy, float radius, SDL_Color c) {
    constexpr int kSegments = 24;
    std::vector<SDL_Vertex> verts;
    std::vector<int> idx;
    verts.reserve(kSegments + 1);
    verts.push_back(Vert(cx, cy, c.r, c.g, c.b, c.a));
    for (int i = 0; i < kSegments; ++i) {
        const float ang = kTwoPi * static_cast<float>(i) / kSegments;
        verts.push_back(Vert(cx + std::cos(ang) * radius, cy + std::sin(ang) * radius, c.r, c.g, c.b, c.a));
    }
    for (int i = 0; i < kSegments; ++i) idx.insert(idx.end(), {0, 1 + i, 1 + (i + 1) % kSegments});
    SDL_RenderGeometry(renderer, nullptr, verts.data(), static_cast<int>(verts.size()),
                       idx.data(), static_cast<int>(idx.size()));
}

// Encaixa a textura num quadrado mantendo a proporção.
SDL_FRect FitInside(SDL_Texture* tex, float x, float y, float size) {
    int w = 0, h = 0;
    SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
    if (w <= 0 || h <= 0) return {x, y, size, size};
    const float s = size / static_cast<float>(std::max(w, h));
    const float dw = w * s, dh = h * s;
    return {x + (size - dw) * 0.5f, y + (size - dh) * 0.5f, dw, dh};
}


// ── Brilho "assado" da chama ────────────────────────────────────────────────
// O brilho em volta da chama da HUD é gerado UMA vez por PNG (cada sprite de
// nível/quadro tem o seu) e guardado: alpha do PNG reduzido → espalhado para
// cima (labareda) → desfocado → pintado em degradê vertical (amarelo embaixo,
// laranja no meio, vermelho na ponta). Na tela é só um RenderCopy aditivo.
constexpr int   kGlowWorkPx   = 96;                      // lado maior do PNG reduzido (o brilho é borrado: pouco detalhe basta)
constexpr float kGlowPadSide  = 0.55f;                   // margem lateral, em frações da largura da chama
constexpr float kGlowPadTop   = 0.85f;                   // margem em cima (a luz sobe mais)
constexpr float kGlowPadBot   = 0.30f;                   // margem embaixo
constexpr float kGlowRiseKeep = 0.95f;                   // por pixel: quanto da luz "sobe" (maior = labaredas mais altas)
constexpr float kGlowNearR    = 0.06f;                   // desfoque do brilho colado no contorno (fração do lado)
constexpr float kGlowFarR     = 0.12f;                   // desfoque do halo largo
constexpr float kGlowNearGain = 1.8f;                    // força do brilho colado
constexpr float kGlowFarGain  = 2.2f;                    // força do halo largo
constexpr float kGlowGamma    = 0.75f;                   // < 1 = o brilho fraco alcança mais longe
constexpr float kGlowSpan     = 1.25f;                   // altura do degradê, em alturas da chama a partir da base
constexpr float kGlowOrangeAt = 0.40f;                   // onde o degradê vira laranja (0 base → 1 topo)

struct FlameGlow {
    SDL_Texture* tex = nullptr;                          // vive o jogo todo (poucas texturas pequenas)
    float padSide = 0, padTop = 0;                       // margens em pixels de trabalho
    int   workW = 0, workH = 0;                          // tamanho do PNG reduzido (sem margens)
};

// Desfoque de caixa separável, 3 passadas (≈ gaussiano). Raio em pixels.
void BoxBlur(std::vector<float>& img, int w, int h, int radius) {
    if (radius < 1) return;
    std::vector<float> tmp(img.size());
    const float inv = 1.0f / static_cast<float>(radius * 2 + 1);
    for (int pass = 0; pass < 3; ++pass) {
        for (int y = 0; y < h; ++y) {                    // horizontal
            float acc = 0.0f;
            for (int k = -radius; k <= radius; ++k) acc += img[y * w + std::min(std::max(k, 0), w - 1)];
            for (int x = 0; x < w; ++x) {
                tmp[y * w + x] = acc * inv;
                acc += img[y * w + std::min(x + radius + 1, w - 1)] - img[y * w + std::max(x - radius, 0)];
            }
        }
        for (int x = 0; x < w; ++x) {                    // vertical
            float acc = 0.0f;
            for (int k = -radius; k <= radius; ++k) acc += tmp[std::min(std::max(k, 0), h - 1) * w + x];
            for (int y = 0; y < h; ++y) {
                img[y * w + x] = acc * inv;
                acc += tmp[std::min(y + radius + 1, h - 1) * w + x] - tmp[std::max(y - radius, 0) * w + x];
            }
        }
    }
}

// Mistura duas cores (t 0..1).
void LerpColor(const float a[3], const float b[3], float t, float out[3]) {
    for (int i = 0; i < 3; ++i) out[i] = a[i] + (b[i] - a[i]) * t;
}

// Gera (ou devolve do cache) o brilho com a forma do PNG em `path`.
const FlameGlow* GetFlameGlow(SDL_Renderer* renderer, const std::string& path) {
    static std::unordered_map<std::string, FlameGlow> cache;
    auto it = cache.find(path);
    if (it != cache.end()) return it->second.tex ? &it->second : nullptr;
    FlameGlow& glow = cache[path];                       // falha também fica no cache (não tenta todo frame)

    SDL_Surface* loaded = IMG_Load(path.c_str());
    if (!loaded) return nullptr;
    SDL_Surface* src = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(loaded);
    if (!src) return nullptr;

    // 1) Alpha do PNG reduzido para kGlowWorkPx (média da área de cada pixel).
    const float scale = std::min(1.0f, static_cast<float>(kGlowWorkPx) / std::max(src->w, src->h));
    const int ww = std::max(1, static_cast<int>(std::ceil(src->w * scale)));
    const int wh = std::max(1, static_cast<int>(std::ceil(src->h * scale)));
    const int padS = static_cast<int>(ww * kGlowPadSide);
    const int padT = static_cast<int>(wh * kGlowPadTop);
    const int padB = static_cast<int>(wh * kGlowPadBot);
    const int cw = ww + padS * 2, ch = wh + padT + padB;
    std::vector<float> shape(static_cast<size_t>(cw) * ch, 0.0f);
    SDL_LockSurface(src);
    const Uint8* px = static_cast<const Uint8*>(src->pixels);
    for (int y = 0; y < wh; ++y) {
        const int sy0 = static_cast<int>(y / scale), sy1 = std::max(sy0 + 1, std::min(src->h, static_cast<int>((y + 1) / scale)));
        for (int x = 0; x < ww; ++x) {
            const int sx0 = static_cast<int>(x / scale), sx1 = std::max(sx0 + 1, std::min(src->w, static_cast<int>((x + 1) / scale)));
            float sum = 0.0f;
            int n = 0;
            for (int sy = sy0; sy < sy1 && sy < src->h; ++sy)
                for (int sx = sx0; sx < sx1 && sx < src->w; ++sx, ++n) sum += px[sy * src->pitch + sx * 4 + 3];
            shape[(y + padT) * cw + (x + padS)] = n ? sum / (255.0f * n) : 0.0f;
        }
    }
    SDL_UnlockSurface(src);
    SDL_FreeSurface(src);

    // 2) Labareda: a luz de cada pixel "sobe", apagando aos poucos.
    std::vector<float> risen = shape;
    for (int x = 0; x < cw; ++x)
        for (int y = ch - 2; y >= 0; --y)
            risen[y * cw + x] = std::max(risen[y * cw + x], risen[(y + 1) * cw + x] * kGlowRiseKeep);

    // 3) Dois desfoques: brilho colado no contorno + halo largo.
    std::vector<float> nearGlow = shape, farGlow = risen;
    const int side = std::max(ww, wh);
    BoxBlur(nearGlow, cw, ch, std::max(1, static_cast<int>(side * kGlowNearR)));
    BoxBlur(farGlow, cw, ch, std::max(1, static_cast<int>(side * kGlowFarR)));

    // 4) Degradê vertical pela altura da chama: amarelo na base → laranja → vermelho na ponta.
    static const float kYellow[3] = {255, 215, 90};
    static const float kOrange[3] = {255, 120, 25};
    static const float kRed[3]    = {200, 30, 10};
    SDL_Surface* out = SDL_CreateRGBSurfaceWithFormat(0, cw, ch, 32, SDL_PIXELFORMAT_RGBA32);
    if (!out) return nullptr;
    SDL_LockSurface(out);
    Uint8* op = static_cast<Uint8*>(out->pixels);
    const float flameBottom = static_cast<float>(padT + wh);
    const float span = wh * kGlowSpan;                                // da base da chama até onde fica vermelho
    for (int y = 0; y < ch; ++y) {
        const float up = std::min(1.0f, std::max(0.0f, (flameBottom - y) / span));   // 0 base → 1 topo
        float c[3];
        if (up < kGlowOrangeAt) LerpColor(kYellow, kOrange, up / kGlowOrangeAt, c);
        else                    LerpColor(kOrange, kRed, (up - kGlowOrangeAt) / (1.0f - kGlowOrangeAt), c);
        for (int x = 0; x < cw; ++x) {
            const int i = y * cw + x;
            const float v = std::min(1.0f, nearGlow[i] * kGlowNearGain + farGlow[i] * kGlowFarGain);
            Uint8* o = op + y * out->pitch + x * 4;
            o[0] = static_cast<Uint8>(c[0]);
            o[1] = static_cast<Uint8>(c[1]);
            o[2] = static_cast<Uint8>(c[2]);
            o[3] = static_cast<Uint8>(255.0f * std::pow(v, kGlowGamma));
        }
    }
    SDL_UnlockSurface(out);
    glow.tex = SDL_CreateTextureFromSurface(renderer, out);
    SDL_FreeSurface(out);
    if (!glow.tex) return nullptr;
    SDL_SetTextureBlendMode(glow.tex, SDL_BLENDMODE_ADD);
    SDL_SetTextureScaleMode(glow.tex, SDL_ScaleModeLinear);           // ampliado sem serrilhado
    glow.padSide = static_cast<float>(padS);
    glow.padTop  = static_cast<float>(padT);
    glow.workW = ww;
    glow.workH = wh;
    return &glow;
}

}  // namespace

namespace HorrorFx {

// Duas ondas fora de fase + uma queda leve a cada ~3 s, como pavio fraco
// (sutil de propósito: mais que isso lê como "piscando").
float Flicker(float time, float seed) {
    float f = 0.90f + 0.06f * std::sin(time * 7.3f + seed) + 0.04f * std::sin(time * 13.1f + seed * 2.1f);
    const float phase = std::fmod(time * 0.33f + seed * 0.17f, 1.0f);
    if (phase < 0.04f) f *= 0.82f;
    return std::max(0.7f, std::min(1.0f, f));
}

// Valor novo a cada 0,12 s: o tremor é em "saltos", não deslizando.
float Jitter(float time, float seed) {
    const int step = static_cast<int>(std::floor(time / 0.12f));
    return Hash01(step * 31 + static_cast<int>(seed * 1000.0f)) * 2.0f - 1.0f;
}

// Leque de triângulos: centro escuro, borda transparente, raio irregular por vértice.
void DrawSmudge(SDL_Renderer* renderer, float cx, float cy, float rx, float ry, Uint8 alpha, float seed) {
    if (!renderer || alpha == 0) return;
    constexpr int kSegments = 28;
    std::vector<SDL_Vertex> verts;
    std::vector<int> idx;
    verts.reserve(kSegments + 1);
    verts.push_back(Vert(cx, cy, 8, 6, 6, alpha));
    for (int i = 0; i < kSegments; ++i) {
        const float a = kTwoPi * static_cast<float>(i) / kSegments;
        const float wobble = 0.8f + 0.35f * Hash01(i + static_cast<int>(seed * 97.0f));
        verts.push_back(Vert(cx + std::cos(a) * rx * wobble, cy + std::sin(a) * ry * wobble, 8, 6, 6, 0));
    }
    for (int i = 0; i < kSegments; ++i) {
        idx.insert(idx.end(), {0, 1 + i, 1 + (i + 1) % kSegments});
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(renderer, nullptr, verts.data(), static_cast<int>(verts.size()),
                       idx.data(), static_cast<int>(idx.size()));
}

// Desenha um texto (alpha aplicado na textura) com o canto superior-esquerdo em (x, y).
void DrawHintText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, float x, float y, float a) {
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text.c_str(), SDL_Color{222, 206, 178, 255});
    if (!surf) return;
    if (SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf)) {
        SDL_SetTextureAlphaMod(tex, static_cast<Uint8>(255.0f * a));
        const SDL_FRect dst{x, y, static_cast<float>(surf->w), static_cast<float>(surf->h)};
        SDL_RenderCopyF(renderer, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

// [ícones] [teclas] rótulo, sobre uma mancha escura; tudo oscila junto com a
// chama. Item e tecla se juntam com "+": isqueiro + [F], isqueiro + combustível + [R].
void DrawKeyHint(SDL_Renderer* renderer, const std::string& keys, const std::vector<std::string>& iconPaths,
                 const std::string& label, float x, float y, HintAnchor anchor, float alpha, float time) {
    if (!renderer || alpha <= 0.01f || (keys.empty() && iconPaths.empty() && label.empty())) return;

    const float u = Game::UiScale();
    auto font = Resources::GetFont(kHintFont, std::max(12, static_cast<int>(std::lround(kHintFontPx * u))));
    auto labelFont = Resources::GetFont(kHintFont, std::max(14, static_cast<int>(std::lround(kLabelFontPx * u))));
    if (!font || !labelFont) return;

    // Teclas em várias linhas ("\n"), cada uma centrada: "[W]\n[A] [S] [D]" vira o T invertido.
    std::vector<std::string> keyLines;
    for (size_t start = 0; !keys.empty() && start <= keys.size();) {
        const size_t nl = keys.find('\n', start);
        keyLines.push_back(keys.substr(start, nl == std::string::npos ? std::string::npos : nl - start));
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    std::vector<int> lineW(keyLines.size(), 0), lineH(keyLines.size(), 0);
    int keysW = 0, keysH = 0;
    for (size_t i = 0; i < keyLines.size(); ++i) {
        KeyGlyphs::Measure(font.get(), keyLines[i], kHintKeyScale, lineW[i], lineH[i]);
        keysW = std::max(keysW, lineW[i]);
        keysH += lineH[i];
    }
    int labelW = 0, labelH = 0;
    if (!label.empty()) TTF_SizeUTF8(labelFont.get(), label.c_str(), &labelW, &labelH);
    int plusW = 0, plusH = 0;
    TTF_SizeUTF8(labelFont.get(), "+", &plusW, &plusH);
    const float rowH = static_cast<float>(std::max({keysH, labelH, static_cast<int>(40.0f * u)}));

    // Monta a fila de peças da esquerda para a direita (ícone, "+", teclas, rótulo).
    enum class Kind { Icon, Plus, Keys, Label };
    struct Piece { Kind kind; float w; std::shared_ptr<SDL_Texture> tex; };
    std::vector<Piece> pieces;
    const float iconSize = rowH * 1.05f;
    for (const std::string& path : iconPaths) {
        auto tex = path.empty() ? nullptr : Resources::GetImage(path);
        if (!tex) continue;
        if (!pieces.empty()) pieces.push_back({Kind::Plus, static_cast<float>(plusW), nullptr});
        pieces.push_back({Kind::Icon, iconSize, tex});
    }
    if (keysW > 0) {
        if (!pieces.empty()) pieces.push_back({Kind::Plus, static_cast<float>(plusW), nullptr});
        pieces.push_back({Kind::Keys, static_cast<float>(keysW), nullptr});
    }
    if (labelW > 0) pieces.push_back({Kind::Label, static_cast<float>(labelW), nullptr});
    if (pieces.empty()) return;

    const float gap = 10.0f * u;
    float totalW = gap * static_cast<float>(pieces.size() - 1);
    for (const Piece& p : pieces) totalW += p.w;
    const float totalH = std::max(iconSize, rowH);

    const float a = alpha * Flicker(time, x * 0.01f);
    const float jx = Jitter(time, 0.31f) * u;
    const float jy = Jitter(time, 0.77f) * u;
    float left = (anchor == HintAnchor::LeftMiddle) ? x : x - totalW * 0.5f;
    float top = y - totalH * 0.5f;                       // LeftMiddle
    if (anchor == HintAnchor::BottomCenter) top = y - totalH;
    if (anchor == HintAnchor::TopCenter)    top = y;
    top += jy;
    left += jx;

    DrawSmudge(renderer, left + totalW * 0.5f, top + totalH * 0.5f, totalW * 0.62f + 22.0f * u, totalH * 0.9f,
               static_cast<Uint8>(165.0f * alpha), 0.4f);

    float penX = left;
    for (const Piece& p : pieces) {
        switch (p.kind) {
        case Kind::Icon: {
            const SDL_FRect dst = FitInside(p.tex.get(), penX, top + (totalH - iconSize) * 0.5f, iconSize);
            SDL_SetTextureAlphaMod(p.tex.get(), static_cast<Uint8>(255.0f * a));
            SDL_SetTextureColorMod(p.tex.get(), 225, 215, 200);   // um pouco apagado, sem o branco da HUD
            SDL_RenderCopyF(renderer, p.tex.get(), nullptr, &dst);
            SDL_SetTextureColorMod(p.tex.get(), 255, 255, 255);
            break;
        }
        case Kind::Plus:
            DrawHintText(renderer, labelFont.get(), "+", penX, top + (totalH - plusH) * 0.5f, a);
            break;
        case Kind::Keys: {
            float lineY = top + (totalH - keysH) * 0.5f;
            for (size_t i = 0; i < keyLines.size(); ++i) {
                KeyGlyphs::Draw(renderer, font.get(), keyLines[i],
                                static_cast<int>(penX + (keysW - lineW[i]) * 0.5f), static_cast<int>(lineY),
                                SDL_Color{225, 212, 190, 255}, static_cast<Uint8>(255.0f * a), kHintKeyScale);
                lineY += static_cast<float>(lineH[i]);
            }
            break;
        }
        case Kind::Label:
            DrawHintText(renderer, labelFont.get(), label, penX, top + (totalH - labelH) * 0.5f, a);
            break;
        }
        penX += p.w + gap;
    }
}

// Linha com a altura "fervendo" a cada 0,1 s, como traço de carvão (dois riscos sobrepostos).
void DrawScratchLine(SDL_Renderer* renderer, float x1, float x2, float y, float thickness,
                     SDL_Color color, float time) {
    if (!renderer || x2 <= x1 || color.a == 0) return;
    const float u = Game::UiScale();
    const int boil = static_cast<int>(std::floor(time / 0.1f));
    constexpr int kPoints = 32;
    std::vector<SDL_FPoint> pts;
    pts.reserve(kPoints + 1);
    for (int i = 0; i <= kPoints; ++i) {
        const float t = static_cast<float>(i) / kPoints;
        const float wob = (Hash01(i * 7 + boil * 53) * 2.0f - 1.0f) * 2.0f * u;
        pts.push_back({x1 + (x2 - x1) * t, y + wob});
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    DrawThickPolyline(renderer, pts, thickness * u, color);
    // Pontas mais finas por cima, deslocadas: dá o aspecto de dois riscos sobrepostos.
    for (SDL_FPoint& p : pts) p.y += 1.5f * u;
    DrawThickPolyline(renderer, pts, thickness * 0.4f * u,
                      SDL_Color{color.r, color.g, color.b, static_cast<Uint8>(color.a * 0.6f)});
}

// Bolinhas da cabeça até a nuvem, depois a nuvem: todos os círculos primeiro em
// cor clara um pouco maiores (vira o contorno da união) e por cima em escuro.
void DrawThoughtBubble(SDL_Renderer* renderer, float headX, float headY, const std::string& iconPath,
                       float alpha, float time) {
    if (!renderer || alpha <= 0.01f) return;
    const float u = Game::UiScale();
    const int boil = static_cast<int>(std::floor(time / 0.12f));
    auto wob = [&](int salt) { return (Hash01(salt * 23 + boil * 7) * 2.0f - 1.0f) * 1.5f * u; };

    const float R = 34.0f * u;                           // tamanho da nuvem
    const float cx = headX + 46.0f * u;
    const float cy = headY - 96.0f * u;
    struct Blob { float x, y, r; };
    std::vector<Blob> blobs = {
        {headX + 8.0f * u,  headY - 14.0f * u, 4.0f * u},   // bolinhas subindo
        {headX + 18.0f * u, headY - 32.0f * u, 6.0f * u},
        {headX + 30.0f * u, headY - 54.0f * u, 9.0f * u},
        {cx,             cy,             R * 0.80f},        // nuvem
        {cx - R * 0.70f, cy + 6.0f * u,  R * 0.60f},
        {cx + R * 0.70f, cy + 6.0f * u,  R * 0.60f},
        {cx - R * 0.35f, cy - R * 0.45f, R * 0.60f},
        {cx + R * 0.35f, cy - R * 0.45f, R * 0.60f},
        {cx,             cy + R * 0.35f, R * 0.55f},
    };
    const Uint8 a = static_cast<Uint8>(255.0f * alpha);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (size_t i = 0; i < blobs.size(); ++i) {
        FillDisc(renderer, blobs[i].x + wob(static_cast<int>(i)), blobs[i].y + wob(static_cast<int>(i) + 40),
                 blobs[i].r + 2.5f * u, SDL_Color{205, 190, 165, a});
    }
    for (size_t i = 0; i < blobs.size(); ++i) {
        FillDisc(renderer, blobs[i].x + wob(static_cast<int>(i)), blobs[i].y + wob(static_cast<int>(i) + 40),
                 blobs[i].r, SDL_Color{18, 14, 14, a});
    }

    if (!iconPath.empty()) {
        if (auto icon = Resources::GetImage(iconPath)) {
            const float size = R * 1.25f;
            const SDL_FRect dst = FitInside(icon.get(), cx - size * 0.5f + wob(90), cy - size * 0.5f + wob(91), size);
            SDL_SetTextureAlphaMod(icon.get(), static_cast<Uint8>(a * Flicker(time, 3.1f)));
            SDL_SetTextureColorMod(icon.get(), 225, 215, 200);
            SDL_RenderCopyF(renderer, icon.get(), nullptr, &dst);
            SDL_SetTextureColorMod(icon.get(), 255, 255, 255);
        }
    }
}

// Fundo: volta inteira bem apagada. Por cima: o arco do progresso em cor de
// chama, levemente "fervendo". Ícone no centro, apagado como nas dicas.
void DrawProgressArc(SDL_Renderer* renderer, float cx, float cy, float radius, float progress,
                     const std::string& iconPath, float alpha, float time) {
    if (!renderer || alpha <= 0.01f) return;
    const float u = Game::UiScale();
    const int boil = static_cast<int>(std::floor(time / 0.1f));
    auto arc = [&](float sweep, float thickness, SDL_Color c, int salt) {
        const int points = std::max(2, static_cast<int>(48 * sweep / kTwoPi));
        std::vector<SDL_FPoint> pts;
        pts.reserve(points + 1);
        for (int i = 0; i <= points; ++i) {
            const float ang = -kTwoPi * 0.25f + sweep * static_cast<float>(i) / points;
            const float r = radius + (Hash01(i * 11 + boil * 41 + salt) * 2.0f - 1.0f) * 1.2f * u;
            pts.push_back({cx + std::cos(ang) * r, cy + std::sin(ang) * r});
        }
        DrawThickPolyline(renderer, pts, thickness * u, c);
    };

    DrawSmudge(renderer, cx, cy, radius * 1.6f, radius * 1.6f, static_cast<Uint8>(150.0f * alpha), 0.9f);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    arc(kTwoPi, 4.0f, SDL_Color{60, 50, 45, static_cast<Uint8>(160.0f * alpha)}, 5);
    const float p = std::max(0.0f, std::min(1.0f, progress));
    if (p > 0.0f) arc(kTwoPi * p, 5.0f, SDL_Color{235, 170, 80, static_cast<Uint8>(240.0f * alpha)}, 9);

    if (!iconPath.empty()) {
        if (auto icon = Resources::GetImage(iconPath)) {
            const float size = radius * 1.25f;
            const SDL_FRect dst = FitInside(icon.get(), cx - size * 0.5f, cy - size * 0.5f, size);
            SDL_SetTextureAlphaMod(icon.get(), static_cast<Uint8>(230.0f * alpha));
            SDL_SetTextureColorMod(icon.get(), 225, 215, 200);
            SDL_RenderCopyF(renderer, icon.get(), nullptr, &dst);
            SDL_SetTextureColorMod(icon.get(), 255, 255, 255);
        }
    }
}

// Degradê radial: centro na cor `c` (alpha c.a), borda transparente.
void DrawRadialGlow(SDL_Renderer* renderer, float cx, float cy, float radius, SDL_Color c) {
    constexpr int kSegments = 40;
    std::vector<SDL_Vertex> verts;
    std::vector<int> idx;
    verts.reserve(kSegments + 1);
    verts.push_back(Vert(cx, cy, c.r, c.g, c.b, c.a));
    for (int i = 0; i < kSegments; ++i) {
        const float ang = kTwoPi * static_cast<float>(i) / kSegments;
        verts.push_back(Vert(cx + std::cos(ang) * radius, cy + std::sin(ang) * radius, c.r, c.g, c.b, 0));
    }
    for (int i = 0; i < kSegments; ++i) idx.insert(idx.end(), {0, 1 + i, 1 + (i + 1) % kSegments});
    SDL_RenderGeometry(renderer, nullptr, verts.data(), static_cast<int>(verts.size()),
                       idx.data(), static_cast<int>(idx.size()));
}

// Destaque de brasa em modo aditivo (soma luz, como brilho de verdade).
// Round: halo redondo + silhueta acesa (slots da roda, pasta).
// Silhouette: o brilho com a forma do próprio desenho (GetFlameGlow), preso
// pela base, respirando e "lambendo" para cima. Por cima, fagulhas subindo.
void DrawEmberAura(SDL_Renderer* renderer, const SDL_FRect& target, const std::string& maskPath,
                   AuraShape shape, float intensity, float alpha, float time) {
    if (!renderer || alpha <= 0.01f) return;
    const float u = Game::UiScale();
    const float cx = target.x + target.w * 0.5f;
    const float cy = target.y + target.h * 0.5f;
    const float radius = std::max(target.w, target.h) * 0.5f;
    const float power = std::max(0.0f, std::min(1.0f, intensity));
    const float breath = 0.82f + 0.18f * std::sin(time * 2.6f);        // "respira" devagar
    const float a = alpha * breath * Flicker(time, 4.2f) * power;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
    const FlameGlow* glow = (shape == AuraShape::Silhouette && !maskPath.empty())
                                ? GetFlameGlow(renderer, maskPath) : nullptr;
    if (glow) {
        // Pixel de trabalho → tela, e a base da chama como âncora.
        const float k = target.w / static_cast<float>(glow->workW);
        int tw = 0, th = 0;
        SDL_QueryTexture(glow->tex, nullptr, nullptr, &tw, &th);
        const float fullW = tw * k, fullH = th * k;
        const float baseY = target.y + target.h;                       // base da chama na tela
        const float baseInTex = (glow->padTop + glow->workH) * k;      // base da chama dentro da textura
        // Camada 1: o brilho parado, colado no desenho. Camada 2: a mesma luz
        // esticada para cima e balançando, fraca — é ela que dá a vida.
        for (int layer = 0; layer < 2; ++layer) {
            const float lick = (layer == 0) ? 1.0f : 1.06f + 0.08f * std::sin(time * 4.3f) + 0.04f * std::sin(time * 7.1f);
            const float sway = (layer == 0) ? 0.0f : std::sin(time * 2.7f) * 3.0f * u;
            const float la = (layer == 0) ? 1.0f : 0.45f * Flicker(time, 9.7f);
            const float w = fullW * (layer == 0 ? 1.0f : 1.03f);
            const float h = fullH * lick;
            const SDL_FRect dst{cx - w * 0.5f + sway, baseY - baseInTex * lick, w, h};
            SDL_SetTextureAlphaMod(glow->tex, static_cast<Uint8>(std::min(255.0f, 255.0f * a * la)));
            SDL_RenderCopyF(renderer, glow->tex, nullptr, &dst);
        }
    } else {
        std::shared_ptr<SDL_Texture> mask = maskPath.empty() ? nullptr : Resources::GetWhiteMaskImage(maskPath);
        DrawRadialGlow(renderer, cx, cy, radius * 2.1f, SDL_Color{200, 40, 10, static_cast<Uint8>(150.0f * a)});
        DrawRadialGlow(renderer, cx, cy, radius * 1.35f, SDL_Color{255, 150, 30, static_cast<Uint8>(170.0f * a)});
        if (mask) {
            // Silhueta um pouco maior, brilhando em amarelo-alaranjado.
            SDL_SetTextureBlendMode(mask.get(), SDL_BLENDMODE_ADD);
            SDL_SetTextureColorMod(mask.get(), 255, 170, 50);
            for (int i = 0; i < 2; ++i) {
                const float scale = (i == 0) ? 1.16f : 1.08f;
                const float w = target.w * scale, h = target.h * scale;
                const SDL_FRect dst{cx - w * 0.5f, cy - h * 0.5f, w, h};
                SDL_SetTextureAlphaMod(mask.get(), static_cast<Uint8>((i == 0 ? 110.0f : 170.0f) * a));
                SDL_RenderCopyF(renderer, mask.get(), nullptr, &dst);
            }
            SDL_SetTextureBlendMode(mask.get(), SDL_BLENDMODE_BLEND);   // a máscara é compartilhada (destaque de interação)
            SDL_SetTextureColorMod(mask.get(), 255, 255, 255);
            SDL_SetTextureAlphaMod(mask.get(), 255);
        }
    }

    // Fagulhas: cada uma num ciclo próprio (sobe, gira, esfria do amarelo ao
    // vermelho e some). Na chama nascem do corpo do fogo e sobem mais alto;
    // quantas aparecem acompanha a força da chama.
    const bool flame = (glow != nullptr);
    const int kEmbers = flame ? 6 + static_cast<int>(6.0f * power) : 7;
    for (int e = 0; e < kEmbers; ++e) {
        const float speed = 0.35f + 0.25f * Hash01(e * 3 + 1);
        const float cycle = time * speed + Hash01(e * 7 + 2);
        const float life = cycle - std::floor(cycle);                    // 0 nasce → 1 apaga
        const int   gen = static_cast<int>(std::floor(cycle));           // cada volta, outra posição
        const float spread = flame ? 0.75f : 0.9f;
        const float startX = cx + (Hash01(e * 11 + gen * 31) - 0.5f) * target.w * spread;
        const float x = startX + std::sin(life * 6.0f + e) * 6.0f * u;
        const float startY = flame ? target.y + target.h * (0.35f + 0.3f * Hash01(e * 5 + gen * 13)) : cy - radius * 0.3f;
        const float rise = flame ? radius * 2.4f : radius * 1.8f;
        const float y = startY - life * rise;
        const float size = (2.5f + 2.5f * Hash01(e * 13 + gen * 17)) * u * (1.0f - 0.5f * life);
        const float ang = life * 4.0f + e;
        const Uint8 g = static_cast<Uint8>(200.0f * (1.0f - life));      // amarelo → vermelho
        const Uint8 ea = static_cast<Uint8>(230.0f * alpha * (1.0f - life) * (flame ? 0.4f + 0.6f * power : 1.0f));
        SDL_Vertex q[4];
        for (int k = 0; k < 4; ++k) {
            const float qa = ang + kTwoPi * 0.25f * static_cast<float>(k);
            q[k] = Vert(x + std::cos(qa) * size, y + std::sin(qa) * size * 0.8f, 255, g, 30, ea);
        }
        const int qi[6] = {0, 1, 2, 0, 2, 3};
        SDL_RenderGeometry(renderer, nullptr, q, 4, qi, 6);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
}

}  // namespace HorrorFx
