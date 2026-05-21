#ifndef STAGE_INTERNAL_HELPERS_H
#define STAGE_INTERNAL_HELPERS_H

#include "lighting/LightMaskTypes.h"
#include "math/Rect.h"
#include "math/Vec2.h"

#define INCLUDE_SDL
#include "SDL_include.h"

class GameObject;

namespace stage_internal {

extern float gStageOstSilenceRecover;

void SetMouseConfinedToWindow(bool shouldConfine);
float Clamp01(float v);
void DrawDebugCircle(SDL_Renderer* renderer, float cx, float cy, float r, Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha);
void DrawDebugRect(SDL_Renderer* renderer, const Rect& rect, Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha);
void DrawColliderDebugWire(SDL_Renderer* renderer, const Rect& box, float angleDeg, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca);
void DrawDebugCross(SDL_Renderer* renderer, float x, float y, float halfSize, Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha);
void DrawPlayerShadowTouchDebug(SDL_Renderer* renderer, GameObject* go, Uint8 red, Uint8 green, Uint8 blue);
float ComputeShadowDistanceRate(const Vec2& pointScreen, const Vec2& lightScreenPos, const LightMaskParams& params,
                                float* outDistancePx = nullptr, float* outMaxDistancePx = nullptr);
bool IsFootLit(GameObject* go, const Vec2& lightScreenPos, const LightMaskParams& params, float* outIntensity = nullptr);
void RenderProjectedSpriteShadow(GameObject* go, const Vec2& lightScreenPos, float lightTouch, float shadowLengthPx, Uint8 shadowAlpha,
                                 const LightMaskParams& params);
void DrawContactFootShadow(SDL_Renderer* renderer, const Rect& box, float contactRate);

} // namespace stage_internal

#endif
