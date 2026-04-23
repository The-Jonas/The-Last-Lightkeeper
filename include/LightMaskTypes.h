#ifndef LIGHT_MASK_TYPES_H
#define LIGHT_MASK_TYPES_H

#define INCLUDE_SDL
#include "SDL_include.h"

enum class LightFalloffCurve { Smoothstep, Power };

enum class LightMaskShape { Circle, Ellipse, Cone, SoftRect };

struct LightMaskParams {
    Uint8 darknessMax = 220;
    float falloffRadiusPx = 240.0f;
    float fatorDicaDeRaio = 1.2f;
    LightFalloffCurve falloffCurve = LightFalloffCurve::Smoothstep;
    float falloffGamma = 2.0f;
    float innerLift = 0.0f;
    int numRings = 36;
    int numSeg = 40;
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

    float shadowCastDistanceMul = 1.15f;
};

#endif
