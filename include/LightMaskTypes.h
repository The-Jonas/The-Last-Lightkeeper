#ifndef LIGHT_MASK_TYPES_H
#define LIGHT_MASK_TYPES_H

#define INCLUDE_SDL
#include "SDL_include.h"

enum class LightFalloffCurve { Smoothstep, Power };

enum class LightMaskShape { Circle, Ellipse, Cone, SoftRect, Torch };

struct LightMaskParams {
    Uint8 darknessMax = 220;
    float falloffRadiusPx = 240.0f;
    float fatorDicaDeRaio = 1.2f;
    LightFalloffCurve falloffCurve = LightFalloffCurve::Smoothstep;
    float falloffGamma = 2.0f;
    float innerLift = 0.0f;
    int numRings = 48;
    int numSeg = 48;
    float marginExtraAlemDoCanto = 4.0f;
    float rMaxMinimo = 8.0f;

    float ellipseAspect = 1.0f;

    float coneHalfAngleDeg = 35.0f;
    float coneLengthPx = 280.0f;
    float coneAxisDeg = -90.0f;
    bool coneFollowMouse = false;

    float rectHalfWidthPx = 140.0f;
    float rectHalfHeightPx = 100.0f;
    float rectSoftBandPx = 72.0f;

    float shadowCastDistanceMul = 1.62f;
    float shadowMaxLengthPx = 600.0f;
    float shadowLengthByLightMul = 1.40f;
    float spriteShadowMinScale = 1.00f;
    float spriteShadowMaxScale = 2.40f;
    float shadowSoftness = 0.0f;
    int shadowSoftLayers = 1;
    float lightTemporalSmoothing = 0.20f;
    float lightGridStepPx = 12.0f;

    float torchAnimSpeed = 1.0f;
    float torchMotionRangePx = 6.0f;
    float torchWarpStrength = 0.30f;
    float torchPulseStrength = 0.22f;
    float torchColorWarmth = 1.0f;
    float torchColorStrength = 0.85f;
};

#endif
