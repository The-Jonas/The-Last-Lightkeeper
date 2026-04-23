#include "../include/RadialLightOverlay.h"
#include <algorithm>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
int g_prevMx = 0;
int g_prevMy = 0;
bool g_hasPrevMouse = false;

float ComputeAxisRad(const RadialLightOverlay::ScreenLight& light) {
    float axisRad = light.params.coneAxisDeg * static_cast<float>(M_PI) / 180.0f;
    if (light.shape == LightMaskShape::Cone && light.params.coneFollowMouse) {
        if (g_hasPrevMouse) {
            const float mdx = light.x - static_cast<float>(g_prevMx);
            const float mdy = light.y - static_cast<float>(g_prevMy);
            if (mdx * mdx + mdy * mdy > 2.0f) {
                axisRad = std::atan2(mdy, mdx);
            }
        }
        g_prevMx = static_cast<int>(light.x);
        g_prevMy = static_cast<int>(light.y);
        g_hasPrevMouse = true;
    }
    return axisRad;
}
} // namespace

float RadialLightOverlay::FalloffShape(float t01, const LightMaskParams& params) {
    const float t = std::max(0.0f, std::min(1.0f, t01));
    if (params.falloffCurve == LightFalloffCurve::Smoothstep) {
        return t * t * (3.0f - 2.0f * t);
    }
    const float g = std::max(0.05f, params.falloffGamma);
    return std::pow(t, g);
}

float RadialLightOverlay::AlphaAt(float dx, float dy, LightMaskShape shape, const LightMaskParams& params,
                                  float dMax, float innerLift, float rFalloff, float axisRad, int mouseX,
                                  int mouseY) {
    (void)mouseX;
    (void)mouseY;
    const float inner = std::max(0.0f, std::min(1.0f, innerLift));
    auto combine = [&](float t01) -> float {
        const float s = FalloffShape(t01, params);
        return dMax * (inner + (1.0f - inner) * s);
    };

    switch (shape) {
    case LightMaskShape::Circle: {
        const float d = std::sqrt(dx * dx + dy * dy);
        const float t = (rFalloff > 1e-4f) ? std::min(1.0f, d / rFalloff) : 1.0f;
        return combine(t);
    }
    case LightMaskShape::Ellipse: {
        const float a = std::max(0.2f, params.ellipseAspect);
        const float sx = rFalloff * a;
        const float sy = rFalloff / a;
        const float d = std::sqrt((dx / sx) * (dx / sx) + (dy / sy) * (dy / sy));
        const float t = std::min(1.0f, d);
        return combine(t);
    }
    case LightMaskShape::Cone: {
        const float half = params.coneHalfAngleDeg * static_cast<float>(M_PI) / 180.0f;
        const float L = std::max(8.0f, params.coneLengthPx);
        const float ca = std::cos(axisRad);
        const float sa = std::sin(axisRad);
        const float f = dx * ca + dy * sa;
        const float perpX = -sa;
        const float perpY = ca;
        const float perp = dx * perpX + dy * perpY;
        if (f <= 0.0f || f > L) {
            return dMax;
        }
        const float angAbs = std::fabs(std::atan2(perp, f));
        if (angAbs > half) {
            return dMax;
        }
        const float tr = std::min(1.0f, f / L);
        const float ta = (half > 1e-4f) ? std::min(1.0f, angAbs / half) : 0.0f;
        const float t = std::max(tr, ta);
        return combine(t);
    }
    case LightMaskShape::SoftRect: {
        const float hw = std::max(1.0f, params.rectHalfWidthPx);
        const float hh = std::max(1.0f, params.rectHalfHeightPx);
        const float band = std::max(4.0f, params.rectSoftBandPx);
        const float ox = std::max(0.0f, std::fabs(dx) - hw);
        const float oy = std::max(0.0f, std::fabs(dy) - hh);
        const float dist = std::sqrt(ox * ox + oy * oy);
        const float t = std::min(1.0f, dist / band);
        return combine(t);
    }
    default:
        return dMax;
    }
}

bool RadialLightOverlay::Init(SDL_Renderer*, int) {
    return true;
}

void RadialLightOverlay::Render(SDL_Renderer* renderer, int mouseX, int mouseY, int windowW, int windowH,
                                LightMaskShape shape, const LightMaskParams& params) {
    ScreenLight light{static_cast<float>(mouseX), static_cast<float>(mouseY), shape, params};
    std::vector<ScreenLight> lights{light};
    RenderMany(renderer, windowW, windowH, lights);
}

void RadialLightOverlay::RenderMany(SDL_Renderer* renderer, int windowW, int windowH, const std::vector<ScreenLight>& lights) {
    if (!renderer || windowW < 1 || windowH < 1) {
        return;
    }
    if (lights.empty()) {
        return;
    }

    struct PreparedLight {
        float x;
        float y;
        LightMaskShape shape;
        LightMaskParams params;
        float dMax;
        float rGeomMax;
        float rFalloff;
        float axisRad;
    };

    std::vector<PreparedLight> prepared;
    prepared.reserve(lights.size());
    for (const ScreenLight& light : lights) {
        const float c0[4][2] = {{0, 0}, {static_cast<float>(windowW), 0}, {static_cast<float>(windowW), static_cast<float>(windowH)}, {0, static_cast<float>(windowH)}};
        float rGeomMax = light.params.rMaxMinimo;
        for (int i = 0; i < 4; i++) {
            const float dx = c0[i][0] - light.x;
            const float dy = c0[i][1] - light.y;
            rGeomMax = std::max(rGeomMax, std::sqrt(dx * dx + dy * dy) + light.params.marginExtraAlemDoCanto);
        }
        const float rUser = std::max(8.0f, light.params.falloffRadiusPx) * light.params.fatorDicaDeRaio;
        const float rFalloff = std::min(std::max(rUser, light.params.rMaxMinimo), rGeomMax);
        prepared.push_back({light.x, light.y, light.shape, light.params,
                            static_cast<float>(std::min(255, static_cast<int>(light.params.darknessMax))),
                            rGeomMax, rFalloff, ComputeAxisRad(light)});
    }

    const int gridX = std::max(12, windowW / 48);
    const int gridY = std::max(8, windowH / 48);
    const float stepX = static_cast<float>(windowW) / static_cast<float>(gridX);
    const float stepY = static_cast<float>(windowH) / static_cast<float>(gridY);

    std::vector<SDL_Vertex> verts;
    std::vector<int> ind;
    verts.reserve(static_cast<size_t>((gridX + 1) * (gridY + 1)));
    ind.reserve(static_cast<size_t>(gridX * gridY * 6));

    for (int y = 0; y <= gridY; y++) {
        for (int x = 0; x <= gridX; x++) {
            const float px = std::min(static_cast<float>(windowW), x * stepX);
            const float py = std::min(static_cast<float>(windowH), y * stepY);
            float alpha = 255.0f;
            for (const PreparedLight& light : prepared) {
                const float a = AlphaAt(px - light.x, py - light.y, light.shape, light.params, light.dMax, light.params.innerLift,
                                        light.rFalloff, light.axisRad, static_cast<int>(light.x), static_cast<int>(light.y));
                alpha = std::min(alpha, a);
            }
            verts.push_back({{px, py}, {0, 0, 0, static_cast<Uint8>(std::max(0.0f, std::min(255.0f, alpha)))}, {0, 0}});
        }
    }

    auto idx = [&](int x, int y) { return y * (gridX + 1) + x; };
    for (int y = 0; y < gridY; y++) {
        for (int x = 0; x < gridX; x++) {
            const int i00 = idx(x, y);
            const int i10 = idx(x + 1, y);
            const int i11 = idx(x + 1, y + 1);
            const int i01 = idx(x, y + 1);
            ind.push_back(i00); ind.push_back(i10); ind.push_back(i11);
            ind.push_back(i00); ind.push_back(i11); ind.push_back(i01);
        }
    }

    if (verts.empty() || ind.empty()) {
        return;
    }

    SDL_BlendMode oldBlend;
    SDL_GetRenderDrawBlendMode(renderer, &oldBlend);
    Uint8 dr, dg, db, dColA;
    SDL_GetRenderDrawColor(renderer, &dr, &dg, &db, &dColA);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(renderer, nullptr, verts.data(), static_cast<int>(verts.size()), ind.data(),
                       static_cast<int>(ind.size()));

    SDL_SetRenderDrawBlendMode(renderer, oldBlend);
    SDL_SetRenderDrawColor(renderer, dr, dg, db, dColA);
}
