#include "states/stage/InternalHelpers.h"
#include "core/Game.h"
#include "engine/GameObject.h"
#include "engine/SpriteRenderer.h"
#include "engine/Camera.h"
#include "lighting/LightMaskTypes.h"
#define INCLUDE_SDL
#include "SDL_include.h"
#include <algorithm>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace stage_internal {

float gStageOstSilenceRecover = 0.0f; // Debounce Mix_PlayMusic — Mix_PlayMusic a cada frame atrasa chunks.

void SetMouseConfinedToWindow(bool shouldConfine) {
    SDL_Window* window = Game::GetInstance().GetWindow();
    if (!window) {
        return;
    }

    SDL_SetWindowGrab(window, shouldConfine ? SDL_TRUE : SDL_FALSE);
    if (shouldConfine) {
        SDL_WarpMouseInWindow(window, Game::GetInstance().GetWindowsWidth() / 2, Game::GetInstance().GetWindowsHeight() / 2);
    }
}

float Clamp01(float v) {
    return std::max(0.0f, std::min(1.0f, v));
}

void DrawDebugCircle(SDL_Renderer* renderer, float cx, float cy, float r, Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha) {
    if (!renderer || r <= 1.0f) {
        return;
    }
    SDL_BlendMode oldBlend;
    SDL_GetRenderDrawBlendMode(renderer, &oldBlend);
    Uint8 dr, dg, db, da;
    SDL_GetRenderDrawColor(renderer, &dr, &dg, &db, &da);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, red, green, blue, alpha);
    constexpr int kSeg = 72;
    for (int i = 0; i < kSeg; i++) {
        const float a0 = (static_cast<float>(i) / static_cast<float>(kSeg)) * 2.0f * static_cast<float>(M_PI);
        const float a1 = (static_cast<float>(i + 1) / static_cast<float>(kSeg)) * 2.0f * static_cast<float>(M_PI);
        const int x0 = static_cast<int>(std::lround(cx + std::cos(a0) * r));
        const int y0 = static_cast<int>(std::lround(cy + std::sin(a0) * r));
        const int x1 = static_cast<int>(std::lround(cx + std::cos(a1) * r));
        const int y1 = static_cast<int>(std::lround(cy + std::sin(a1) * r));
        SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
    }
    SDL_SetRenderDrawBlendMode(renderer, oldBlend);
    SDL_SetRenderDrawColor(renderer, dr, dg, db, da);
}

void DrawDebugRect(SDL_Renderer* renderer, const Rect& rect, Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha) {
    if (!renderer || rect.w <= 0.0f || rect.h <= 0.0f) {
        return;
    }
    SDL_BlendMode oldBlend;
    SDL_GetRenderDrawBlendMode(renderer, &oldBlend);
    Uint8 dr, dg, db, da;
    SDL_GetRenderDrawColor(renderer, &dr, &dg, &db, &da);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, red, green, blue, alpha);
    SDL_FRect rf{rect.x, rect.y, rect.w, rect.h};
    SDL_RenderDrawRectF(renderer, &rf);
    SDL_SetRenderDrawBlendMode(renderer, oldBlend);
    SDL_SetRenderDrawColor(renderer, dr, dg, db, da);
}

/// OBB outline (matches `Collider::Render` / sprite space): (world − camera) × zoom.
void DrawColliderDebugWire(SDL_Renderer* renderer, const Rect& box, float angleDeg, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca) {
    if (!renderer || box.w < 0.5f || box.h < 0.5f) {
        return;
    }
    Vec2 center(box.Center());
    const float angleRad = angleDeg * (M_PI / 180.0f);
    const float z = Camera::GetZoom();
    auto corner = [&](float x, float y) {
        Vec2 w = (Vec2(x, y) - center).Rotated(angleRad) + center;
        return Vec2((w.x - Camera::pos.x) * z, (w.y - Camera::pos.y) * z);
    };
    SDL_Point pts[5];
    Vec2 p = corner(box.x, box.y);
    pts[0] = {(int)p.x, (int)p.y};
    p = corner(box.x + box.w, box.y);
    pts[1] = {(int)p.x, (int)p.y};
    p = corner(box.x + box.w, box.y + box.h);
    pts[2] = {(int)p.x, (int)p.y};
    p = corner(box.x, box.y + box.h);
    pts[3] = {(int)p.x, (int)p.y};
    pts[4] = pts[0];

    SDL_BlendMode oldBlend;
    SDL_GetRenderDrawBlendMode(renderer, &oldBlend);
    Uint8 dr, dg, db, da;
    SDL_GetRenderDrawColor(renderer, &dr, &dg, &db, &da);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, cr, cg, cb, ca);
    SDL_RenderDrawLines(renderer, pts, 5);
    SDL_SetRenderDrawBlendMode(renderer, oldBlend);
    SDL_SetRenderDrawColor(renderer, dr, dg, db, da);
}

void DrawDebugCross(SDL_Renderer* renderer, float x, float y, float halfSize, Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha) {
    if (!renderer || halfSize < 1.0f) {
        return;
    }
    SDL_BlendMode oldBlend;
    SDL_GetRenderDrawBlendMode(renderer, &oldBlend);
    Uint8 dr, dg, db, da;
    SDL_GetRenderDrawColor(renderer, &dr, &dg, &db, &da);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, red, green, blue, alpha);
    SDL_RenderDrawLine(renderer, static_cast<int>(std::lround(x - halfSize)), static_cast<int>(std::lround(y)),
                       static_cast<int>(std::lround(x + halfSize)), static_cast<int>(std::lround(y)));
    SDL_RenderDrawLine(renderer, static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y - halfSize)),
                       static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y + halfSize)));
    SDL_SetRenderDrawBlendMode(renderer, oldBlend);
    SDL_SetRenderDrawColor(renderer, dr, dg, db, da);
}

void DrawPlayerShadowTouchDebug(SDL_Renderer* renderer, GameObject* go, Uint8 red, Uint8 green, Uint8 blue) {
    if (!renderer || !go) {
        return;
    }
    const Rect& b = go->box;
    const float z = Camera::GetZoom();
    const Rect screenRect((b.x - Camera::pos.x) * z, (b.y - Camera::pos.y) * z, b.w * z, b.h * z);
    DrawDebugRect(renderer, screenRect, red, green, blue, 190);
    const float footX = (b.x + 0.5f * b.w - Camera::pos.x) * z;
    const float footY = (b.y + b.h - Camera::pos.y) * z;
    DrawDebugCross(renderer, footX, footY, std::max(3.0f, 5.0f * z), red, green, blue, 240);
}

float ComputeShadowDistanceRate(const Vec2& pointScreen, const Vec2& lightScreenPos, const LightMaskParams& params,
                                float* outDistancePx, float* outMaxDistancePx) {
    const float visualRadius = std::max(8.0f, params.falloffRadiusPx) * std::max(0.4f, params.fatorDicaDeRaio);
    const float maxShadowDist = std::max(1.0f, visualRadius);
    const float d = pointScreen.Distance(lightScreenPos);
    if (outDistancePx) {
        *outDistancePx = d;
    }
    if (outMaxDistancePx) {
        *outMaxDistancePx = maxShadowDist;
    }
    return Clamp01(1.0f - (d / maxShadowDist));
}

bool IsFootLit(GameObject* go, const Vec2& lightScreenPos, const LightMaskParams& params, float* outIntensity) {
    if (!go) {
        return false;
    }
    const Rect& b = go->box;
    const Vec2 footWorld(b.x + 0.5f * b.w, b.y + b.h);
    const Vec2 footScreen((footWorld.x - Camera::pos.x) * Camera::GetZoom(), (footWorld.y - Camera::pos.y) * Camera::GetZoom());
    const float rate = ComputeShadowDistanceRate(footScreen, lightScreenPos, params);
    if (outIntensity) {
        *outIntensity = rate;
    }
    return rate > 0.0f;
}

void RenderProjectedSpriteShadow(GameObject* go, const Vec2& lightScreenPos, float lightTouch, float shadowLengthPx, Uint8 shadowAlpha,
                                 const LightMaskParams& params) {
    (void)params;
    if (!go || lightTouch <= 0.01f || shadowLengthPx <= 1.0f) {
        return;
    }
    SpriteRenderer* sprite = go->GetComponent<SpriteRenderer>();
    if (!sprite) {
        return;
    }

    const Rect originalBox = go->box;
    const double originalAngle = go->angleDeg;

    const Vec2 foot(go->box.x + 0.5f * go->box.w, go->box.y + go->box.h);
    const Vec2 lightWorld(lightScreenPos.x / Camera::GetZoom() + Camera::pos.x, lightScreenPos.y / Camera::GetZoom() + Camera::pos.y);
    Vec2 dir = foot - lightWorld;
    if (dir.Magnitude() < 1e-3f) {
        return;
    }
    dir = dir.Normalized();

    const float distance01 = Clamp01(1.0f - lightTouch); // 0 = close light, 1 = far light
    const float fastStretch = std::pow(distance01, 0.60f);
    const float stretch = 0.82f + fastStretch * 2.35f;
    const float widen = 1.00f + fastStretch * 0.26f;

    Rect shadowBox = originalBox;
    shadowBox.w = std::max(2.0f, originalBox.w * widen);
    shadowBox.h = std::max(2.0f, originalBox.h * stretch);
    const double angDeg = std::atan2(dir.y, dir.x) * (180.0 / M_PI) + 90.0;
    const double angRad = angDeg * (M_PI / 180.0);
    const Vec2 footAnchor(originalBox.x + 0.5f * originalBox.w, originalBox.y + originalBox.h);
    const Vec2 localFootToCenter(0.0f, -shadowBox.h * 0.5f);
    const Vec2 rotatedFootToCenter(localFootToCenter.x * std::cos(angRad) - localFootToCenter.y * std::sin(angRad),
                                   localFootToCenter.x * std::sin(angRad) + localFootToCenter.y * std::cos(angRad));
    const Vec2 center = footAnchor + rotatedFootToCenter;
    shadowBox.x = center.x - shadowBox.w * 0.5f;
    shadowBox.y = center.y - shadowBox.h * 0.5f;

    go->box = shadowBox;
    go->angleDeg = angDeg;
    const Uint8 spriteShadowAlpha = static_cast<Uint8>(std::max(18, std::min(205, static_cast<int>(shadowAlpha))));
    sprite->SetTint(0, 0, 0, spriteShadowAlpha);
    go->Render();

    go->box = originalBox;
    go->angleDeg = originalAngle;
}

void DrawContactFootShadow(SDL_Renderer* renderer, const Rect& box, float contactRate) {
    if (!renderer || contactRate <= 0.0f) {
        return;
    }
    const float z = Camera::GetZoom();
    const float footX = (box.x + 0.5f * box.w - Camera::pos.x) * z;
    const float footY = (box.y + box.h - Camera::pos.y) * z;
    const float base = std::max(2.0f, std::min(box.w, box.h) * z);
    const float r = base * (0.14f + 0.16f * Clamp01(contactRate));
    if (r <= 0.5f) {
        return;
    }

    SDL_BlendMode oldBlend;
    SDL_GetRenderDrawBlendMode(renderer, &oldBlend);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    const int seg = 24;
    const int blurLayers = 2;
    const float baseAlpha = std::max(24.0f, std::min(220.0f, 255.0f * contactRate));
    for (int layer = 0; layer < blurLayers; layer++) {
        const float tLayer = static_cast<float>(layer) / static_cast<float>(std::max(1, blurLayers - 1));
        const float layerRadius = r * (1.0f + 0.45f * tLayer);
        const float layerAlphaF = baseAlpha * std::pow(1.0f - tLayer, 1.7f);
        if (layerAlphaF < 1.0f) {
            continue;
        }
        SDL_Color col{0, 0, 0, static_cast<Uint8>(layerAlphaF)};
        std::vector<SDL_Vertex> verts;
        std::vector<int> inds;
        verts.reserve(static_cast<size_t>(seg + 2));
        inds.reserve(static_cast<size_t>(seg * 3));
        verts.push_back({{footX, footY}, col, {0, 0}});
        for (int i = 0; i <= seg; i++) {
            const float t = (static_cast<float>(i) / static_cast<float>(seg)) * 2.0f * static_cast<float>(M_PI);
            verts.push_back({{footX + std::cos(t) * layerRadius, footY + std::sin(t) * layerRadius}, col, {0, 0}});
        }
        for (int i = 1; i <= seg; i++) {
            inds.push_back(0);
            inds.push_back(i);
            inds.push_back(i + 1);
        }
        SDL_RenderGeometry(renderer, nullptr, verts.data(), static_cast<int>(verts.size()), inds.data(),
                           static_cast<int>(inds.size()));
    }

    SDL_SetRenderDrawBlendMode(renderer, oldBlend);
}
}

