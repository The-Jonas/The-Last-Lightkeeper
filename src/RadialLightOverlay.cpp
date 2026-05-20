#include "../include/RadialLightOverlay.h"
#include <algorithm>
#include <cmath>
#include <vector>

#include "../include/LightShadowProfile.h"

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

struct TorchAnimState {
    float swayX = 0.0f;
    float swayY = 0.0f;
    float radialMul = 1.0f;
    float warmPulse = 1.0f;
};

float Clamp01f(float v) {
    return std::max(0.0f, std::min(1.0f, v));
}

float Hash1(float x) {
    const float s = std::sin(x * 127.1f + 311.7f) * 43758.5453f;
    return s - std::floor(s);
}

float SmoothNoise1D(float x, float seed) {
    const float p = std::floor(x);
    const float f = x - p;
    const float u = f * f * (3.0f - 2.0f * f);
    const float a = Hash1(p + seed * 91.13f);
    const float b = Hash1((p + 1.0f) + seed * 91.13f);
    return a + (b - a) * u; // 0..1
}

void SmoothRandomDir(float phase, float seed, float salt, float& outX, float& outY) {
    const float k0 = std::floor(phase);
    const float f = phase - k0;
    const float u = f * f * (3.0f - 2.0f * f);
    const float a0 = Hash1((k0 + salt) * 1.31f + seed * 17.0f) * 2.0f * static_cast<float>(M_PI);
    const float a1 = Hash1((k0 + 1.0f + salt) * 1.31f + seed * 17.0f) * 2.0f * static_cast<float>(M_PI);
    const float x = std::cos(a0) + (std::cos(a1) - std::cos(a0)) * u;
    const float y = std::sin(a0) + (std::sin(a1) - std::sin(a0)) * u;
    const float invLen = 1.0f / std::max(1e-4f, std::sqrt(x * x + y * y));
    outX = x * invLen;
    outY = y * invLen;
}

TorchAnimState ComputeTorchAnim(const LightMaskParams& params, float timeSec, float seed) {
    const float t = timeSec * std::max(0.15f, params.torchAnimSpeed);
    const float motion = std::max(0.0f, params.torchMotionRangePx);
    const float warp = Clamp01f(params.torchWarpStrength);
    const float pulse = Clamp01f(params.torchPulseStrength);
    TorchAnimState s;

    // Non-orbital random sway: independent smooth noise channels.
    const float sx1 = (SmoothNoise1D(t * 1.7f, seed * 1.3f + 11.0f) - 0.5f) * 2.0f;
    const float sx2 = (SmoothNoise1D(t * 4.9f, seed * 2.1f + 37.0f) - 0.5f) * 2.0f;
    const float sy1 = (SmoothNoise1D(t * 1.9f, seed * 2.7f + 19.0f) - 0.5f) * 2.0f;
    const float sy2 = (SmoothNoise1D(t * 5.3f, seed * 3.4f + 53.0f) - 0.5f) * 2.0f;
    s.swayX = motion * (0.72f * sx1 + 0.28f * sx2);
    s.swayY = motion * (0.70f * sy1 + 0.30f * sy2);

    // Irregular pulse/jitter (non-rhythmic).
    const float p1 = (SmoothNoise1D(t * 2.2f, seed * 4.1f + 71.0f) - 0.5f) * 2.0f;
    const float p2 = (SmoothNoise1D(t * 6.1f, seed * 5.7f + 97.0f) - 0.5f) * 2.0f;
    const float pulseNoise = 0.68f * p1 + 0.32f * p2;
    s.radialMul = std::max(0.62f, std::min(1.70f, 1.0f + pulse * (0.32f * pulseNoise) + warp * (0.08f * p2)));
    s.warmPulse = std::max(0.55f, std::min(1.45f, 1.0f + pulse * (0.36f * p1 + 0.12f * p2)));
    return s;
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
                                  float dMax, float innerLift, float rFalloff, float axisRad, float timeSec,
                                  float seed) {
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
        const float featherAng = std::max(0.04f, half * 0.18f);
        if (angAbs > half + featherAng) {
            return dMax;
        }
        const float tr = std::min(1.0f, f / L);
        const float ta = (half > 1e-4f) ? std::min(1.0f, angAbs / half) : 0.0f;
        const float t = std::max(tr, ta);
        const float radialAlpha = combine(t);
        const float angFade = std::max(0.0f, std::min(1.0f, (half + featherAng - angAbs) / featherAng));
        const float lenFeather = std::max(12.0f, L * 0.14f);
        const float lenFade = std::max(0.0f, std::min(1.0f, (L - f) / lenFeather));
        const float edgeFade = angFade * lenFade;
        return dMax - (dMax - radialAlpha) * edgeFade;
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
    case LightMaskShape::Torch: {
        const float t = timeSec * std::max(0.15f, params.torchAnimSpeed);
        const float warp = Clamp01f(params.torchWarpStrength);
        const float dRaw = std::sqrt(dx * dx + dy * dy);
        const float inv = 1.0f / std::max(1e-3f, dRaw);
        const float nx = dx * inv;
        const float ny = dy * inv;
        // Lightweight directional noise: different border portions move independently.
        const float edgeNoise = warp *
                                (0.16f * std::sin((nx * 1.70f + ny * 2.30f) * 9.0f + t * 4.2f + seed * 6.1f) +
                                 0.11f * std::sin((nx * -2.90f + ny * 1.10f) * 13.0f - t * 2.8f + seed * 9.4f) +
                                 0.08f * std::sin((nx * 3.40f + ny * -3.80f) * 7.0f + t * 6.5f + seed * 3.7f));
        const float radialMul = std::max(0.62f, std::min(1.75f, 1.0f + edgeNoise));
        const float d = dRaw;
        const float rEff = std::max(10.0f, rFalloff * radialMul);
        const float tt = (rEff > 1e-4f) ? std::min(1.0f, d / rEff) : 1.0f;
        return combine(tt);
    }
    default:
        return dMax;
    }
}

bool RadialLightOverlay::Init(SDL_Renderer*, int) {
    return true;
}

RadialLightOverlay::~RadialLightOverlay() {
    if (lowResMaskTex != nullptr) {
        SDL_DestroyTexture(lowResMaskTex);
        lowResMaskTex = nullptr;
    }
}

void RadialLightOverlay::Render(SDL_Renderer* renderer, int mouseX, int mouseY, int windowW, int windowH,
                                LightMaskShape shape, const LightMaskParams& params) {
    ScreenLight light{static_cast<float>(mouseX), static_cast<float>(mouseY), shape, params};
    std::vector<ScreenLight> lights{light};
    RenderMany(renderer, windowW, windowH, lights);
}

void RadialLightOverlay::RenderMany(SDL_Renderer* renderer, int windowW, int windowH, const std::vector<ScreenLight>& lights,
                                     const LightOcclusionContext& occlusion) {
    if (!renderer || windowW < 1 || windowH < 1) {
        return;
    }
    if (lights.empty()) {
        return;
    }

    Uint64 perfStart = 0;
    if (LightShadowProfile::IsActive()) {
        perfStart = SDL_GetPerformanceCounter();
    }

    const float timeSec = static_cast<float>(SDL_GetTicks()) * 0.001f;

    struct PreparedLight {
        float x;
        float y;
        LightMaskShape shape;
        LightMaskShape evalShape;
        LightMaskParams params;
        float dMax;
        float rGeomMax;
        float rFalloff;
        float axisRad;
        float seed;
        float tintStrength;
        float tintWarmth;
        float torchKx;
        float torchKy;
        float torchKxy;
        float torchKquad;
        float torchAx1;
        float torchAy1;
        float torchAx2;
        float torchAy2;
        float torchAx3;
        float torchAy3;
        float torchSpikeAmp;
        float cullRadiusSq;
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
        const float seed = light.animationSeed;
        float evalX = light.x;
        float evalY = light.y;
        float evalRFalloff = rFalloff;
        LightMaskShape evalShape = light.shape;
        float tintStrength = 0.0f;
        float tintWarmth = 0.0f;

        float torchKx = 0.0f;
        float torchKy = 0.0f;
        float torchKxy = 0.0f;
        float torchKquad = 0.0f;
        float torchAx1 = 1.0f;
        float torchAy1 = 0.0f;
        float torchAx2 = 0.0f;
        float torchAy2 = 1.0f;
        float torchAx3 = 0.7071f;
        float torchAy3 = 0.7071f;
        float torchSpikeAmp = 0.0f;
        if (light.shape == LightMaskShape::Torch) {
            const TorchAnimState anim = ComputeTorchAnim(light.params, timeSec, seed);
            evalX += anim.swayX * 0.55f;
            evalY += anim.swayY * 0.40f;
            evalRFalloff = std::max(10.0f, evalRFalloff * std::max(0.70f, std::min(1.45f, anim.radialMul)));
            evalShape = LightMaskShape::Torch;
            tintStrength = std::max(0.0f, std::min(1.0f, light.params.torchColorStrength));
            tintWarmth = std::max(0.0f, std::min(2.0f, light.params.torchColorWarmth));
            const float warp = Clamp01f(light.params.torchWarpStrength);
            const float tt = timeSec * std::max(0.15f, light.params.torchAnimSpeed);
            torchKx = warp * 0.26f * std::sin(tt * 1.7f + seed * 13.1f);
            torchKy = warp * 0.24f * std::sin(tt * 2.1f + seed * 17.3f + 1.1f);
            torchKxy = warp * 0.20f * std::sin(tt * 2.8f + seed * 19.7f + 2.4f);
            torchKquad = warp * 0.18f * std::sin(tt * 3.5f + seed * 23.9f + 0.6f);
            SmoothRandomDir(tt * 0.45f, seed, 1.3f, torchAx1, torchAy1);
            SmoothRandomDir(tt * 0.63f, seed, 7.9f, torchAx2, torchAy2);
            SmoothRandomDir(tt * 0.39f, seed, 13.7f, torchAx3, torchAy3);
            torchSpikeAmp = warp * (0.22f + 0.28f * std::max(0.0f, std::min(1.0f, light.params.torchPulseStrength)));
        }

        if (occlusion.IsEnabled()) {
            const float lxWorld = evalX / occlusion.zoom + occlusion.cameraX;
            const float lyWorld = evalY / occlusion.zoom + occlusion.cameraY;
            const int ltx = static_cast<int>((lxWorld - occlusion.mapOriginX) / occlusion.tileWidth);
            const int lty = static_cast<int>((lyWorld - occlusion.mapOriginY) / occlusion.tileHeight);
            if (ltx >= 0 && ltx < occlusion.mapWidth && lty >= 0 && lty < occlusion.mapHeight) {
                if ((*occlusion.solidGrid)[static_cast<size_t>(ltx + lty * occlusion.mapWidth)] != 0) {
                    continue;
                }
            }
        }

        float influenceR = evalRFalloff;
        if (light.shape == LightMaskShape::Cone || evalShape == LightMaskShape::Cone) {
            influenceR = std::max(influenceR, light.params.coneLengthPx * 0.5f + evalRFalloff * 0.35f);
        }
        if (light.shape == LightMaskShape::SoftRect || evalShape == LightMaskShape::SoftRect) {
            const float hw = light.params.rectHalfWidthPx + light.params.rectSoftBandPx;
            const float hh = light.params.rectHalfHeightPx + light.params.rectSoftBandPx;
            influenceR = std::max(influenceR, std::sqrt(hw * hw + hh * hh));
        }
        if (light.shape == LightMaskShape::Torch) {
            influenceR = std::max(influenceR, evalRFalloff * 2.0f + light.params.torchMotionRangePx * 1.2f);
        }
        const float margin = 32.0f + 0.25f * influenceR;
        const float cullR = influenceR + margin;
        const float cullRadiusSq = cullR * cullR;

        const float dMax =
            std::max(0.0f, std::min(255.0f, static_cast<float>(light.params.darknessMax)));
        prepared.push_back({evalX, evalY, light.shape, evalShape, light.params,
                            dMax,
                            rGeomMax, evalRFalloff, ComputeAxisRad(light), seed, tintStrength, tintWarmth,
                            torchKx, torchKy, torchKxy, torchKquad,
                            torchAx1, torchAy1, torchAx2, torchAy2, torchAx3, torchAy3, torchSpikeAmp,
                            cullRadiusSq});
    }

    if (prepared.empty()) {
        return;
    }

    bool halfRes = prepared[0].params.radialMaskHalfResolution;
    switch (prepared[0].params.lightQualityPreset) {
    case LightQualityPreset::Quality:
        halfRes = false;
        break;
    case LightQualityPreset::Performance:
        halfRes = true;
        break;
    default:
        break;
    }
    const int lowW = std::max(1, (windowW + 1) / 2);
    const int lowH = std::max(1, (windowH + 1) / 2);

    auto runDarknessPass = [&](int passW, int passH, const std::vector<PreparedLight>& prep) {
        float gridStep = std::max(12.0f, std::min(64.0f, prep[0].params.lightGridStepPx));
        switch (prep[0].params.lightQualityPreset) {
        case LightQualityPreset::Quality:
            gridStep = std::max(14.0f, gridStep * 0.95f);
            break;
        case LightQualityPreset::Balanced:
            gridStep = std::max(20.0f, gridStep);
            break;
        case LightQualityPreset::Performance:
            gridStep = std::max(28.0f, gridStep);
            break;
        case LightQualityPreset::Custom:
        default:
            break;
        }
        const int gridX = std::max(20, static_cast<int>(std::ceil(static_cast<float>(passW) / gridStep)));
        const int gridY = std::max(12, static_cast<int>(std::ceil(static_cast<float>(passH) / gridStep)));
        const float stepX = static_cast<float>(passW) / static_cast<float>(gridX);
        const float stepY = static_cast<float>(passH) / static_cast<float>(gridY);

        std::vector<SDL_Vertex> verts;
        std::vector<int> ind;
        verts.reserve(static_cast<size_t>((gridX + 1) * (gridY + 1)));
        ind.reserve(static_cast<size_t>(gridX * gridY * 6));

        for (int y = 0; y <= gridY; y++) {
            for (int x = 0; x <= gridX; x++) {
                const float px = std::min(static_cast<float>(passW), x * stepX);
                const float py = std::min(static_cast<float>(passH), y * stepY);
                float alpha = 255.0f;
                float torchWarm = 0.0f;
                float torchWarmth = 0.0f;
                for (const PreparedLight& light : prep) {
                    const float lx = px - light.x;
                    const float ly = py - light.y;
                    const float distSq = lx * lx + ly * ly;
                    if (distSq > light.cullRadiusSq) {
                        continue;
                    }
                    float a = 255.0f;
                    if (light.shape == LightMaskShape::Torch) {
                        const float d = std::sqrt(distSq);
                        const float inv = 1.0f / std::max(1e-3f, d);
                        const float nx = lx * inv;
                        const float ny = ly * inv;
                        const float dirWarp = light.torchKx * nx + light.torchKy * ny + light.torchKxy * (nx * ny) +
                                              light.torchKquad * (nx * nx - ny * ny);
                        const float s1 = std::fabs(nx * light.torchAx1 + ny * light.torchAy1);
                        const float s2 = std::fabs(nx * light.torchAx2 + ny * light.torchAy2);
                        const float s3 = std::fabs(nx * light.torchAx3 + ny * light.torchAy3);
                        const float p1 = s1 * s1 * s1 * s1 * s1 * s1;
                        const float p2 = s2 * s2 * s2 * s2 * s2 * s2;
                        const float p3 = s3 * s3 * s3 * s3 * s3 * s3;
                        const float spikeWarp = light.torchSpikeAmp * std::max(p1, std::max(p2, p3));
                        const float rEff = std::max(8.0f, light.rFalloff *
                                                            std::max(0.60f, std::min(1.65f, 1.0f + dirWarp + spikeWarp)));
                        const float t01 = std::min(1.0f, d / rEff);
                        const float s = FalloffShape(t01, light.params);
                        const float inner = Clamp01f(light.params.innerLift);
                        a = light.dMax * (inner + (1.0f - inner) * s);
                    } else {
                        a = AlphaAt(lx, ly, light.evalShape, light.params, light.dMax, light.params.innerLift, light.rFalloff,
                                    light.axisRad, timeSec, light.seed);
                    }
                    alpha = std::min(alpha, a);
                    if (light.shape == LightMaskShape::Torch) {
                        const float lit = 1.0f - std::max(0.0f, std::min(1.0f, a / std::max(1.0f, light.dMax)));
                        torchWarm = std::max(torchWarm, lit * light.tintStrength);
                        torchWarmth = std::max(torchWarmth, light.tintWarmth);
                    }
                }
                const float warm = std::max(0.0f, std::min(1.0f, torchWarm));
                const Uint8 vr = static_cast<Uint8>(std::lround((42.0f + 36.0f * torchWarmth) * warm));
                const Uint8 vg = static_cast<Uint8>(std::lround((12.0f + 10.0f * torchWarmth) * warm));
                const Uint8 vb = static_cast<Uint8>(std::lround((2.0f + 4.0f * torchWarmth) * warm));
                verts.push_back({{px, py}, {vr, vg, vb, static_cast<Uint8>(std::max(0.0f, std::min(255.0f, alpha)))}, {0, 0}});
            }
        }

        std::vector<SDL_Vertex> glowVerts;
        std::vector<int> glowInd;
        glowVerts.reserve(prep.size() * 18u);
        glowInd.reserve(prep.size() * 48u);
        for (const PreparedLight& light : prep) {
            if (light.shape != LightMaskShape::Torch || light.tintStrength <= 0.001f) {
                continue;
            }
            const TorchAnimState anim = ComputeTorchAnim(light.params, timeSec, light.seed);
            const float cx = light.x + anim.swayX * 0.25f;
            const float cy = light.y + anim.swayY * 0.18f;
            const float rr = std::max(20.0f, light.rFalloff * (0.42f + 0.12f * (anim.radialMul - 1.0f)));
            const float warm = light.tintWarmth;
            const Uint8 cr = static_cast<Uint8>(std::min(255.0f, (185.0f + 48.0f * warm) * light.tintStrength));
            const Uint8 cg = static_cast<Uint8>(std::min(235.0f, (95.0f + 30.0f * warm) * light.tintStrength));
            const Uint8 cb = static_cast<Uint8>(std::min(140.0f, (18.0f + 20.0f * warm) * light.tintStrength));
            const Uint8 ca = static_cast<Uint8>(std::min(140.0f, 110.0f * light.tintStrength));

            const int base = static_cast<int>(glowVerts.size());
            glowVerts.push_back({{cx, cy}, {cr, cg, cb, ca}, {0, 0}});
            constexpr int kGlowSeg = 12;
            for (int i = 0; i <= kGlowSeg; i++) {
                const float a = (static_cast<float>(i) / static_cast<float>(kGlowSeg)) * 2.0f * static_cast<float>(M_PI);
                glowVerts.push_back({{cx + std::cos(a) * rr, cy + std::sin(a) * rr}, {cr, cg, cb, 0}, {0, 0}});
            }
            for (int i = 0; i < kGlowSeg; i++) {
                glowInd.push_back(base);
                glowInd.push_back(base + 1 + i);
                glowInd.push_back(base + 1 + i + 1);
            }
        }

        auto idx = [&](int gx, int gy) { return gy * (gridX + 1) + gx; };
        for (int gy = 0; gy < gridY; gy++) {
            for (int gx = 0; gx < gridX; gx++) {
                const int i00 = idx(gx, gy);
                const int i10 = idx(gx + 1, gy);
                const int i11 = idx(gx + 1, gy + 1);
                const int i01 = idx(gx, gy + 1);
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
        if (!glowVerts.empty() && !glowInd.empty()) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
            SDL_RenderGeometry(renderer, nullptr, glowVerts.data(), static_cast<int>(glowVerts.size()), glowInd.data(),
                               static_cast<int>(glowInd.size()));
        }

        SDL_SetRenderDrawBlendMode(renderer, oldBlend);
        SDL_SetRenderDrawColor(renderer, dr, dg, db, dColA);
    };

    if (halfRes) {
        if (lowResMaskTex == nullptr || lowResMaskAllocW != lowW || lowResMaskAllocH != lowH) {
            if (lowResMaskTex != nullptr) {
                SDL_DestroyTexture(lowResMaskTex);
                lowResMaskTex = nullptr;
            }
            lowResMaskTex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, lowW, lowH);
            if (lowResMaskTex == nullptr) {
                runDarknessPass(windowW, windowH, prepared);
            } else {
                SDL_SetTextureBlendMode(lowResMaskTex, SDL_BLENDMODE_BLEND);
                lowResMaskAllocW = lowW;
                lowResMaskAllocH = lowH;
            }
        }

        if (lowResMaskTex != nullptr) {
            std::vector<PreparedLight> prepLow;
            prepLow.reserve(prepared.size());
            const float sx = static_cast<float>(lowW) / static_cast<float>(windowW);
            const float sy = static_cast<float>(lowH) / static_cast<float>(windowH);
            const float smin = std::min(sx, sy);
            for (PreparedLight p : prepared) {
                p.x *= sx;
                p.y *= sy;
                p.rFalloff *= smin;
                p.rGeomMax *= smin;
                p.cullRadiusSq *= smin * smin;
                p.params.falloffRadiusPx *= smin;
                p.params.rMaxMinimo *= smin;
                p.params.marginExtraAlemDoCanto *= smin;
                p.params.coneLengthPx *= smin;
                p.params.rectHalfWidthPx *= smin;
                p.params.rectHalfHeightPx *= smin;
                p.params.rectSoftBandPx *= smin;
                p.params.torchMotionRangePx *= smin;
                prepLow.push_back(std::move(p));
            }

            SDL_Texture* prevTarget = SDL_GetRenderTarget(renderer);
            SDL_SetRenderTarget(renderer, lowResMaskTex);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
            SDL_RenderClear(renderer);

            runDarknessPass(lowW, lowH, prepLow);

            SDL_SetRenderTarget(renderer, prevTarget);

            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

            SDL_BlendMode oldBlend2;
            SDL_GetRenderDrawBlendMode(renderer, &oldBlend2);
            Uint8 dr2, dg2, db2, da2;
            SDL_GetRenderDrawColor(renderer, &dr2, &dg2, &db2, &da2);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            const SDL_FRect dst{0.0f, 0.0f, static_cast<float>(windowW), static_cast<float>(windowH)};
            SDL_RenderCopyF(renderer, lowResMaskTex, nullptr, &dst);
            SDL_SetRenderDrawBlendMode(renderer, oldBlend2);
            SDL_SetRenderDrawColor(renderer, dr2, dg2, db2, da2);
        }
    } else {
        runDarknessPass(windowW, windowH, prepared);
    }

    if (LightShadowProfile::IsActive()) {
        const Uint64 perfEnd = SDL_GetPerformanceCounter();
        const double ms =
            static_cast<double>(perfEnd - perfStart) * 1000.0 / static_cast<double>(SDL_GetPerformanceFrequency());
        LightShadowProfile::SetRadialOverlayMs(ms);
    }
}
