#include "../include/LightShadowProfile.h"
#include "../include/Game.h"

#define INCLUDE_SDL
#include "../include/SDL_include.h"

#include <cstdlib>
#include <cstring>

namespace LightShadowProfile {
namespace {

bool EnvWantsProfile() {
    static int cached = -1;
    if (cached < 0) {
        const char* e = std::getenv("PROFILE_LIGHTS");
        cached =
            (e && e[0] != '\0' && std::strcmp(e, "0") != 0 && std::strcmp(e, "false") != 0) ? 1 : 0;
    }
    return cached == 1;
}

double g_spriteMs = 0.0;
double g_radialMs = 0.0;
double g_volumeMs = 0.0;

double g_emaSpriteMs = 0.0;
double g_emaRadialMs = 0.0;
double g_emaVolumeMs = 0.0;

Uint32 g_lastLogMs = 0;
const Uint32 kLogIntervalMs = 1000;

} // namespace

bool IsActive() {
    return Game::IsDebugBuild() || EnvWantsProfile();
}

void BeginLightsTiming() {
    if (!IsActive()) {
        return;
    }
    g_spriteMs = 0.0;
    g_radialMs = 0.0;
    g_volumeMs = 0.0;
}

void SetSpriteShadowBlockMs(double ms) {
    if (!IsActive()) {
        return;
    }
    g_spriteMs = ms;
}

void SetRadialOverlayMs(double ms) {
    if (!IsActive()) {
        return;
    }
    g_radialMs = ms;
}

void SetVolumeShadowMs(double ms) {
    if (!IsActive()) {
        return;
    }
    g_volumeMs = ms;
}

void EndLightsFrame() {
    if (!IsActive()) {
        return;
    }

    const double k = 0.12;
    g_emaSpriteMs = g_emaSpriteMs * (1.0 - k) + g_spriteMs * k;
    g_emaRadialMs = g_emaRadialMs * (1.0 - k) + g_radialMs * k;
    g_emaVolumeMs = g_emaVolumeMs * (1.0 - k) + g_volumeMs * k;

    const Uint32 now = SDL_GetTicks();
    if (g_lastLogMs == 0) {
        g_lastLogMs = now;
    }
    if (now - g_lastLogMs < kLogIntervalMs) {
        return;
    }
    g_lastLogMs = now;

    SDL_Log("[lights profile] sprite_shadows=%.2f ms (ema) | radial_overlay=%.2f ms | volume_shadows=%.2f ms",
            g_emaSpriteMs, g_emaRadialMs, g_emaVolumeMs);
}

} // namespace LightShadowProfile
