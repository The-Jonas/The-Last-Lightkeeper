#ifndef LIGHT_SHADOW_PROFILE_H
#define LIGHT_SHADOW_PROFILE_H

// Optional light/shadow GPU+CPU timing. Enabled when compiled with DEBUG, or set env PROFILE_LIGHTS=1.

namespace LightShadowProfile {

bool IsActive();

// Call once at the start of the lights pass (before projected shadows / radial overlay).
void BeginLightsTiming();

void SetSpriteShadowBlockMs(double ms);
void SetRadialOverlayMs(double ms);
void SetVolumeShadowMs(double ms);

// Call after radial overlay and optional volume pass (end of light overlay section).
void EndLightsFrame();

} // namespace LightShadowProfile

#endif
