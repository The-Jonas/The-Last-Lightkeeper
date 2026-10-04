#include "ui/HorrorFx.h"

#include "core/Game.h"
#include "core/Resources.h"
#include "ui/KeyGlyphs.h"

#define INCLUDE_SDL_TTF
#include "SDL_include.h"

#include <algorithm>
#include <cmath>
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

// Aura de sangue em volta de um círculo: anel em degradê (transparente dentro,
// vermelho escuro no raio, sumindo para fora), com a borda de fora irregular.
void DrawBloodAura(SDL_Renderer* renderer, float cx, float cy, float radius, float spread, Uint8 alpha, int boil) {
    constexpr int kSegments = 48;
    std::vector<SDL_Vertex> verts;
    std::vector<int> idx;
    verts.reserve(kSegments * 3);
    idx.reserve(kSegments * 12);
    const float inner = std::max(0.0f, radius - spread * 0.35f);
    for (int i = 0; i < kSegments; ++i) {
        const float ang = kTwoPi * static_cast<float>(i) / kSegments;
        const float cs = std::cos(ang), sn = std::sin(ang);
        const float outer = radius + spread * (0.7f + 0.5f * Hash01(i * 17 + boil * 5));   // escorrendo irregular
        verts.push_back(Vert(cx + cs * inner,  cy + sn * inner,  90, 4, 4, 0));
        verts.push_back(Vert(cx + cs * radius, cy + sn * radius, 120, 8, 8, alpha));
        verts.push_back(Vert(cx + cs * outer,  cy + sn * outer,  70, 2, 2, 0));
    }
    for (int i = 0; i < kSegments; ++i) {
        const int a = i * 3, b = ((i + 1) % kSegments) * 3;
        idx.insert(idx.end(), {a, b, a + 1, b, b + 1, a + 1,             // de dentro até o raio
                               a + 1, b + 1, a + 2, b + 1, b + 2, a + 2});   // do raio para fora
    }
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
    float left = (anchor == HintAnchor::BottomCenter) ? x - totalW * 0.5f : x;
    const float top = ((anchor == HintAnchor::BottomCenter) ? y - totalH : y - totalH * 0.5f) + jy;
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

// Aura de sangue por trás + dois traços por volta: um grosso cor de sangue seco e
// um fino claro, ambos com o raio "fervendo" a cada 0,1 s. A volta não fecha certinho: passa um pouco do
// começo, como risco feito à mão.
void DrawAttentionRing(SDL_Renderer* renderer, const SDL_FRect& target, float alpha, float time) {
    if (!renderer || alpha <= 0.01f) return;
    const float u = Game::UiScale();
    const float cx = target.x + target.w * 0.5f;
    const float cy = target.y + target.h * 0.5f;
    const float pulse = 0.5f + 0.5f * std::sin(time * 3.2f);
    const float baseR = std::max(target.w, target.h) * 0.5f + (14.0f + 4.0f * pulse) * u;
    const float a = alpha * (0.65f + 0.35f * pulse) * Flicker(time, 1.7f);
    const int boil = static_cast<int>(std::floor(time / 0.1f));

    auto stroke = [&](float startAngle, float sweep, float noisePx, float thickness, SDL_Color c, int salt) {
        constexpr int kPoints = 44;
        std::vector<SDL_FPoint> pts;
        pts.reserve(kPoints + 1);
        for (int i = 0; i <= kPoints; ++i) {
            const float t = static_cast<float>(i) / kPoints;
            const float ang = startAngle + sweep * t;
            const float r = baseR + (Hash01(i * 13 + boil * 97 + salt) * 2.0f - 1.0f) * noisePx * u;
            pts.push_back({cx + std::cos(ang) * r, cy + std::sin(ang) * r});
        }
        DrawThickPolyline(renderer, pts, thickness * u, c);
    };

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    DrawBloodAura(renderer, cx, cy, baseR, (26.0f + 10.0f * pulse) * u, static_cast<Uint8>(150.0f * a), boil);
    stroke(-1.9f, kTwoPi * 1.08f, 2.5f, 9.0f, SDL_Color{105, 12, 10, static_cast<Uint8>(235.0f * a)}, 3);
    stroke(-1.6f, kTwoPi * 1.03f, 3.5f, 3.0f, SDL_Color{215, 195, 165, static_cast<Uint8>(180.0f * a)}, 11);
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

}  // namespace HorrorFx
