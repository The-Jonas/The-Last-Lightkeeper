#include "audio/GameSfx.h"
#include "audio/Sound.h"
#include "core/Game.h"

#define INCLUDE_SDL_MIXER
#include "SDL_include.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>

namespace {

constexpr const char* kBoxShortPaths[] = {
    "Recursos/audio/SFX/CAIXAS/CAIXA_Madeira.mp3",
    "Recursos/audio/SFX/CAIXAS/CAIXA_Madeira2.mp3",
};
constexpr const char* kBoxLongPath = "Recursos/audio/SFX/CAIXAS/CAIXA_MADEIRALONGO.mp3";

constexpr const char* kLighterOnPath = "Recursos/audio/SFX/ISQUEIRO/ISQUEIRO_ ABRINDO.mp3";
constexpr const char* kLighterOffPath = "Recursos/audio/SFX/ISQUEIRO/ISQUEIRO_FECHANDO.mp3";

constexpr const char* kFootstepStonePath = "Recursos/audio/SFX/PASSOS/PASSOS_pedra.mp3";
constexpr const char* kFootstepWoodPath = "Recursos/audio/SFX/PASSOS/PASSOS_Madeira.mp3";
constexpr const char* kFootstepStairsPath = "Recursos/audio/SFX/PASSOS/PASSOS_ESCADA.mp3";

constexpr const char* kThunderPaths[] = {
    "Recursos/audio/SFX/TROVÃO/ES_Weather, Thunder, Thunder Clap, Heavy Impact, Dark, Gritty 01 - Epidemic Sound.mp3",
    "Recursos/audio/SFX/TROVÃO/ES_Weather, Thunder, Thunder Clap, Heavy Impact, Dark, Gritty 17 - Epidemic Sound.mp3",
    "Recursos/audio/SFX/TROVÃO/ES_Weather, Thunder, Thunder Clap, Heavy Impact, Explosive 04 - Epidemic Sound.mp3",
    "Recursos/audio/SFX/TROVÃO/ES_Weather, Thunder, Thunder Clap, Heavy Impact, Explosive, Bright 03 - Epidemic Sound.mp3",
};

constexpr const char* kCandleLoopPath = "Recursos/audio/SFX/VELA/FOGO_VELA.mp3";

constexpr int kBoxShortCount = 2;
constexpr int kThunderCount = 4;

constexpr float kBoxPushCooldown = 0.22f;
constexpr float kMinFootstepSpeed = 35.0f;
constexpr float kThunderMinDelay = 18.0f;
constexpr float kThunderMaxDelay = 42.0f;
constexpr float kThunderFlashDuration = 0.42f;
constexpr float kPostLoadingThunderDelay = 12.0f;
constexpr int kFootstepVolumePercent = 100;

Sound gBoxShortSounds[kBoxShortCount];
Sound gBoxLongSound;
Sound gLighterOnSound;
Sound gLighterOffSound;
Sound gFootstepStoneSound;
Sound gFootstepWoodSound;
Sound gFootstepStairsSound;
Sound gThunderSounds[kThunderCount];
Sound gCandleLoopSound;

bool gLoaded = false;
bool gGameplayMuted = false;
float gBoxPushCooldownTimer = 0.0f;
float gThunderTimer = 12.0f;
float gThunderFlashTimer = 0.0f;
bool gCandleLoopActive = false;
bool gFootstepLoopActive = false;
FootstepSurface gFootstepLoopSurface = FootstepSurface::Stone;

void EnsureLoaded() {
    if (gLoaded) {
        return;
    }
    for (int i = 0; i < kBoxShortCount; ++i) {
        gBoxShortSounds[i].Open(kBoxShortPaths[i]);
    }
    gBoxLongSound.Open(kBoxLongPath);
    gLighterOnSound.Open(kLighterOnPath);
    gLighterOffSound.Open(kLighterOffPath);
    gFootstepStoneSound.Open(kFootstepStonePath);
    gFootstepWoodSound.Open(kFootstepWoodPath);
    gFootstepStairsSound.Open(kFootstepStairsPath);
    for (int i = 0; i < kThunderCount; ++i) {
        gThunderSounds[i].Open(kThunderPaths[i]);
    }
    gCandleLoopSound.Open(kCandleLoopPath);
    gLoaded = true;
}

void PlaySound(Sound& sound) {
    if (gGameplayMuted) {
        return;
    }
    EnsureLoaded();
    if (sound.IsOpen()) {
        sound.Play();
    }
}

Sound& FootstepSound(FootstepSurface surface) {
    switch (surface) {
    case FootstepSurface::Wood:
        return gFootstepWoodSound;
    case FootstepSurface::Stairs:
        return gFootstepStairsSound;
    case FootstepSurface::Stone:
    default:
        return gFootstepStoneSound;
    }
}

void StopCandleLoop() {
    if (gCandleLoopActive) {
        gCandleLoopSound.Stop();
        gCandleLoopActive = false;
    }
}

void StartCandleLoop() {
    if (gGameplayMuted) {
        StopCandleLoop();
        return;
    }
    EnsureLoaded();
    if (!gCandleLoopSound.IsOpen()) {
        return;
    }
    const int channel = gCandleLoopSound.GetChannel();
    if (gCandleLoopActive && channel >= 0 && Mix_Playing(channel)) {
        return;
    }
    if (gCandleLoopSound.PlayLooped() >= 0) {
        gCandleLoopActive = true;
    }
}

void StopFootstepLoop() {
    if (!gFootstepLoopActive) {
        return;
    }
    gFootstepStoneSound.Stop();
    gFootstepWoodSound.Stop();
    gFootstepStairsSound.Stop();
    gFootstepLoopActive = false;
}

void StopAllGameplayAudio() {
    StopFootstepLoop();
    StopCandleLoop();
    gBoxPushCooldownTimer = 0.0f;
}

void ApplyFootstepLoopVolume(Sound& sound) {
    const int channel = sound.GetChannel();
    if (channel < 0) {
        return;
    }
    int vol = (MIX_MAX_VOLUME * Game::masterVolumePercent) / 100;
    vol = (vol * kFootstepVolumePercent) / 100;
    vol = std::min(MIX_MAX_VOLUME, vol + vol / 2);
    Mix_Volume(channel, vol);
}

void EnsureFootstepLoop(FootstepSurface surface) {
    if (gGameplayMuted) {
        StopFootstepLoop();
        return;
    }
    EnsureLoaded();
    if (gFootstepLoopActive && gFootstepLoopSurface == surface) {
        Sound& active = FootstepSound(surface);
        const int channel = active.GetChannel();
        if (channel >= 0 && Mix_Playing(channel)) {
            ApplyFootstepLoopVolume(active);
            return;
        }
    }

    StopFootstepLoop();
    Sound& next = FootstepSound(surface);
    if (!next.IsOpen()) {
        return;
    }
    if (next.PlayLooped() >= 0) {
        gFootstepLoopSurface = surface;
        gFootstepLoopActive = true;
        ApplyFootstepLoopVolume(next);
    }
}

void ResetThunderSchedule(float minDelay) {
    const float span = kThunderMaxDelay - kThunderMinDelay;
    gThunderTimer = minDelay + static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * span;
}

void TriggerThunderStrikeInternal() {
    EnsureLoaded();
    const int idx = rand() % kThunderCount;
    if (gThunderSounds[idx].IsOpen()) {
        gThunderSounds[idx].Play();
    }
    gThunderFlashTimer = kThunderFlashDuration;
}

} // namespace

namespace GameSfx {

void NotifyLoadingBegin() {
    gGameplayMuted = true;
    StopAllGameplayAudio();
}

void NotifyLoadingEnd() {
    gGameplayMuted = false;
    StopAllGameplayAudio();
    ResetThunderSchedule(kPostLoadingThunderDelay);
    gThunderFlashTimer = 0.0f;
}

void PlayBoxPushSound(bool sustainedPush) {
    if (gGameplayMuted) {
        return;
    }
    EnsureLoaded();
    if (gBoxPushCooldownTimer > 0.0f) {
        return;
    }
    gBoxPushCooldownTimer = kBoxPushCooldown;
    if (sustainedPush && gBoxLongSound.IsOpen()) {
        gBoxLongSound.Play();
    } else {
        const int idx = rand() % kBoxShortCount;
        if (gBoxShortSounds[idx].IsOpen()) {
            gBoxShortSounds[idx].Play();
        }
    }
}

void PlayLighterToggle(bool turningOn) {
    PlaySound(turningOn ? gLighterOnSound : gLighterOffSound);
}

void UpdateBigBrotherFootsteps(float dt, float moveSpeed, bool isBigBrother, FootstepSurface surface) {
    if (!isBigBrother) {
        StopFootstepLoop();
        return;
    }

    gBoxPushCooldownTimer = std::max(0.0f, gBoxPushCooldownTimer - dt);

    if (gGameplayMuted || moveSpeed < kMinFootstepSpeed) {
        StopFootstepLoop();
        return;
    }

    EnsureFootstepLoop(surface);
}

void UpdateThunder(float dt) {
    gThunderFlashTimer = std::max(0.0f, gThunderFlashTimer - dt);
    if (gGameplayMuted) {
        return;
    }

    gThunderTimer -= dt;
    if (gThunderTimer > 0.0f) {
        return;
    }

    TriggerThunderStrikeInternal();
    ResetThunderSchedule(kThunderMinDelay);
}

void TriggerThunderStrike() {
    if (gGameplayMuted) {
        return;
    }
    TriggerThunderStrikeInternal();
    ResetThunderSchedule(kThunderMinDelay);
}

void UpdateCandleProximity(bool playerNearCandle) {
    if (gGameplayMuted) {
        StopCandleLoop();
        return;
    }
    if (playerNearCandle) {
        if (!gCandleLoopActive) {
            StartCandleLoop();
        }
    } else {
        StopCandleLoop();
    }
}

float GetThunderFlashStrength() {
    if (gThunderFlashTimer <= 0.0f || kThunderFlashDuration <= 0.0f) {
        return 0.0f;
    }
    return gThunderFlashTimer / kThunderFlashDuration;
}

} // namespace GameSfx
