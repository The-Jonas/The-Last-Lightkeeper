#ifndef RADIAL_LIGHT_OVERLAY_H
#define RADIAL_LIGHT_OVERLAY_H

#define INCLUDE_SDL
#include "SDL_include.h"

#include "lighting/LightMaskTypes.h"
#include <vector>

class RadialLightOverlay {
public:
    struct ScreenLight {
        float x;
        float y;
        LightMaskShape shape;
        LightMaskParams params;
        float animationSeed = 0.0f;
    };

    RadialLightOverlay() = default;
    ~RadialLightOverlay();

    RadialLightOverlay(const RadialLightOverlay&) = delete;
    RadialLightOverlay& operator=(const RadialLightOverlay&) = delete;

    bool Init(SDL_Renderer* = nullptr, int = 0);

    void Render(SDL_Renderer* renderer, int mouseX, int mouseY, int windowW, int windowH, LightMaskShape shape,
                const LightMaskParams& params);
    void RenderMany(SDL_Renderer* renderer, int windowW, int windowH, const std::vector<ScreenLight>& lights,
                    const LightOcclusionContext& occlusion = {});

private:
    SDL_Texture* lowResMaskTex = nullptr;
    int lowResMaskAllocW = 0;
    int lowResMaskAllocH = 0;
    static float FalloffShape(float t01, const LightMaskParams& params);
    static float AlphaAt(float dx, float dy, LightMaskShape shape, const LightMaskParams& params, float dMax,
                         float innerLift, float rFalloff, float axisRad, float timeSec, float seed);
};

#endif
