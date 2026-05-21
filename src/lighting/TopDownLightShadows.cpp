#include "lighting/TopDownLightShadows.h"
#include "engine/Camera.h"
#include "lighting/LightShadowProfile.h"

#include <algorithm>
#include <cmath>

namespace TopDownLightShadows {
namespace {

Vec2 NormalizeOrZero(Vec2 v) {
    const float len = v.Magnitude();
    if (len < 1e-4f) {
        return Vec2(0.0f, 0.0f);
    }
    return v * (1.0f / len);
}

Vec2 WorldToScreen(const Vec2& w) {
    const float z = Camera::GetZoom();
    return Vec2((w.x - Camera::pos.x) * z, (w.y - Camera::pos.y) * z);
}

} // namespace

void BuildShadowEdgesFromSolidGrid(int mapW, int mapH, float tileW, float tileH, float mapOriginX, float mapOriginY,
                                   const std::vector<std::uint8_t>& solid, std::vector<TopDownShadowEdge>& outEdges) {
    outEdges.clear();
    if (mapW < 1 || mapH < 1 || static_cast<int>(solid.size()) != mapW * mapH) {
        return;
    }

    auto isSolid = [&](int x, int y) -> bool {
        if (x < 0 || y < 0 || x >= mapW || y >= mapH) {
            return false;
        }
        return solid[static_cast<size_t>(x + y * mapW)] != 0;
    };

    for (int y = 0; y < mapH; y++) {
        for (int x = 0; x < mapW; x++) {
            if (!isSolid(x, y)) {
                continue;
            }
            const float x0 = mapOriginX + static_cast<float>(x) * tileW;
            const float y0 = mapOriginY + static_cast<float>(y) * tileH;
            const float x1 = x0 + tileW;
            const float y1 = y0 + tileH;

            if (!isSolid(x - 1, y)) {
                outEdges.push_back({Vec2(x0, y0), Vec2(x0, y1)});
            }
            if (!isSolid(x + 1, y)) {
                outEdges.push_back({Vec2(x1, y0), Vec2(x1, y1)});
            }
            if (!isSolid(x, y - 1)) {
                outEdges.push_back({Vec2(x0, y0), Vec2(x1, y0)});
            }
            if (!isSolid(x, y + 1)) {
                outEdges.push_back({Vec2(x0, y1), Vec2(x1, y1)});
            }
        }
    }
}

void AppendAabbShadowEdges(const Rect& worldAabb, std::vector<TopDownShadowEdge>& outEdges) {
    const float x0 = worldAabb.x;
    const float y0 = worldAabb.y;
    const float x1 = worldAabb.x + worldAabb.w;
    const float y1 = worldAabb.y + worldAabb.h;
    if (worldAabb.w < 1.0f || worldAabb.h < 1.0f) {
        return;
    }
    outEdges.push_back({Vec2(x0, y0), Vec2(x1, y0)});
    outEdges.push_back({Vec2(x1, y0), Vec2(x1, y1)});
    outEdges.push_back({Vec2(x1, y1), Vec2(x0, y1)});
    outEdges.push_back({Vec2(x0, y1), Vec2(x0, y0)});
}

void AppendCircleShadowEdges(const Vec2& centerWorld, float radiusWorld, int segments, std::vector<TopDownShadowEdge>& outEdges) {
    if (radiusWorld < 0.5f) {
        return;
    }
    segments = std::max(8, std::min(64, segments));
    const float twoPi = 6.28318530718f;
    const float dth = twoPi / static_cast<float>(segments);
    for (int i = 0; i < segments; i++) {
        const float t0 = static_cast<float>(i) * dth;
        const float t1 = static_cast<float>(i + 1) * dth;
        outEdges.push_back(
            {Vec2(centerWorld.x + std::cos(t0) * radiusWorld, centerWorld.y + std::sin(t0) * radiusWorld),
             Vec2(centerWorld.x + std::cos(t1) * radiusWorld, centerWorld.y + std::sin(t1) * radiusWorld)});
    }
}

void RenderShadowVolumes(SDL_Renderer* renderer, float lightScreenX, float lightScreenY, int windowW, int windowH,
                         const std::vector<TopDownShadowEdge>& staticEdgesWorld,
                         const std::vector<TopDownShadowEdge>& dynamicEdgesWorld, Uint8 shadowAlpha,
                         float shadowLengthPx, int softnessLayers, float softness) {
    if (!renderer || windowW < 1 || windowH < 1) {
        return;
    }

    Uint64 perfStart = 0;
    if (LightShadowProfile::IsActive()) {
        perfStart = SDL_GetPerformanceCounter();
    }

    const Vec2 L(lightScreenX, lightScreenY);
    const float extend = std::max(8.0f, std::min(420.0f, shadowLengthPx));
    const int baseLayers = std::max(1, std::min(6, softnessLayers));
    const float softness01 = std::max(0.0f, std::min(1.0f, softness));
    // Add automatic feather layers so shadows look less hard-edged even with low settings.
    const int extraFeatherLayers = 1 + static_cast<int>(std::round(softness01 * 2.0f));
    const int layers = std::max(1, std::min(5, baseLayers + extraFeatherLayers));

    std::vector<SDL_Vertex> verts;
    std::vector<int> ind;
    verts.clear();
    ind.clear();
    verts.reserve((staticEdgesWorld.size() + dynamicEdgesWorld.size()) * 4u * static_cast<size_t>(layers));
    ind.reserve((staticEdgesWorld.size() + dynamicEdgesWorld.size()) * 6u * static_cast<size_t>(layers));

    auto pushQuad = [&](const Vec2& Aw, const Vec2& Bw, float layerSpread, Uint8 layerAlpha) {
        const Vec2 As = WorldToScreen(Aw);
        const Vec2 Bs = WorldToScreen(Bw);
        const Vec2 dA = NormalizeOrZero(As - L);
        const Vec2 dB = NormalizeOrZero(Bs - L);
        if (dA.Magnitude() < 1e-6f || dB.Magnitude() < 1e-6f) {
            return;
        }
        const Vec2 A2 = As + dA * extend + dA * layerSpread;
        const Vec2 B2 = Bs + dB * extend + dB * layerSpread;

        const SDL_Color col{0, 0, 0, layerAlpha};
        const int base = static_cast<int>(verts.size());
        verts.push_back({{As.x, As.y}, col, {0, 0}});
        verts.push_back({{Bs.x, Bs.y}, col, {0, 0}});
        verts.push_back({{B2.x, B2.y}, col, {0, 0}});
        verts.push_back({{A2.x, A2.y}, col, {0, 0}});
        ind.push_back(base + 0);
        ind.push_back(base + 1);
        ind.push_back(base + 2);
        ind.push_back(base + 0);
        ind.push_back(base + 2);
        ind.push_back(base + 3);
    };

    auto drawEdgeSet = [&](const std::vector<TopDownShadowEdge>& edges) {
        for (const TopDownShadowEdge& e : edges) {
            const Vec2 As = WorldToScreen(e.a);
            const Vec2 Bs = WorldToScreen(e.b);
            const float midX = (As.x + Bs.x) * 0.5f;
            const float midY = (As.y + Bs.y) * 0.5f;
            const float dxL = midX - L.x;
            const float dyL = midY - L.y;
            const float edgeLen = (Bs - As).Magnitude();
            const float reach = extend + edgeLen * 0.5f + 120.0f;
            if (dxL * dxL + dyL * dyL > reach * reach) {
                continue;
            }
            for (int i = 0; i < layers; i++) {
                const float t = static_cast<float>(i) / static_cast<float>(std::max(1, layers - 1));
                // Non-linear ramp keeps the contact area dark while softening far edge.
                const float softnessRamp = t * t;
                const float spread = softnessRamp * (extend * (0.18f + 0.72f * softness01));
                const float alphaRamp = std::pow(1.0f - t, 1.35f + 1.25f * softness01);
                const float alphaFloor = 0.02f + 0.04f * (1.0f - softness01);
                const Uint8 layerAlpha = static_cast<Uint8>(
                    std::max(1.0f, shadowAlpha * std::max(alphaFloor, alphaRamp)));
                pushQuad(e.a, e.b, spread, layerAlpha);
            }
        }
    };
    drawEdgeSet(staticEdgesWorld);
    drawEdgeSet(dynamicEdgesWorld);

    if (verts.empty() || ind.empty()) {
        return;
    }

    SDL_BlendMode oldBlend;
    SDL_GetRenderDrawBlendMode(renderer, &oldBlend);
    Uint8 dr, dg, db, da;
    SDL_GetRenderDrawColor(renderer, &dr, &dg, &db, &da);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(renderer, nullptr, verts.data(), static_cast<int>(verts.size()), ind.data(),
                       static_cast<int>(ind.size()));

    SDL_SetRenderDrawBlendMode(renderer, oldBlend);
    SDL_SetRenderDrawColor(renderer, dr, dg, db, da);

    if (LightShadowProfile::IsActive()) {
        const Uint64 perfEnd = SDL_GetPerformanceCounter();
        const double ms =
            static_cast<double>(perfEnd - perfStart) * 1000.0 / static_cast<double>(SDL_GetPerformanceFrequency());
        LightShadowProfile::SetVolumeShadowMs(ms);
    }
}

} // namespace TopDownLightShadows
