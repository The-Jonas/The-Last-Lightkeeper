#ifndef GAME_SFX_H
#define GAME_SFX_H

enum class FootstepSurface { Stone, Wood, Stairs };

namespace GameSfx {

void NotifyLoadingBegin();
void NotifyLoadingEnd();

void PlayBoxPushSound(bool sustainedPush);
void PlayLighterToggle(bool turningOn);
void UpdateBigBrotherFootsteps(float dt, float moveSpeed, bool isBigBrother, FootstepSurface surface);
void UpdateThunder(float dt);
void UpdateCandleProximity(bool playerNearCandle);

/// Dispara trovão imediato (som aleatório + clarão). Usado pela tecla T e pelo timer ambiental.
void TriggerThunderStrike();

/// 0..1 — força do clarão de trovão (decai ao longo do tempo).
float GetThunderFlashStrength();

} // namespace GameSfx

#endif
