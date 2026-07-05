#include "audio/GameSfx.h"
#include "audio/Sound.h"
#include "core/Game.h"

#define INCLUDE_SDL_MIXER
#include "SDL_include.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <fstream>

namespace {

constexpr int kChannelWaves    = 0;  
constexpr int kChannelWind     = 1;
constexpr int kChannelCandle   = 2;
constexpr int kChannelFootstep = 3;
constexpr int kChannelBox      = 4;
constexpr int kChannelRadio    = 5;
//6+ → one-shots livres (Mix_PlayChannel(-1, ...) usa a partir daqui)

constexpr const char* kBoxStartPath = "Recursos/audio/SFX/CAIXAS/CAIXA_Madeira.mp3";
constexpr const char* kBoxStopPath = "Recursos/audio/SFX/CAIXAS/CAIXA_Madeira2.mp3";
constexpr const char* kBoxMovingLoopPath = "Recursos/audio/SFX/CAIXAS/CAIXA_MADEIRALONGO.mp3";

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
constexpr const char* kCandleLightUpPath = "Recursos/audio/SFX/VELA/VELA_ACENDENDO.mp3";
constexpr const char* kCandleBlowPlayerPath = "Recursos/audio/SFX/VELA/VELA_APAGANDO_JOGADOR.mp3";

constexpr const char* kWindowOpenPath = "Recursos/audio/SFX/JANELA/JANELA_ABRINDO.mp3";
constexpr const char* kWindowClosePath = "Recursos/audio/SFX/JANELA/JANELA_FECHANDO.mp3";
constexpr const char* kWindLoopPath = "Recursos/audio/SFX/JANELA/VENTO_LOOP.wav";
constexpr const char* kCandleBlowPath = "Recursos/audio/SFX/JANELA/VELA_APAGANDO.wav";

constexpr const char* kRepairPath = "Recursos/audio/SFX/ESCADA/reparando.mp3";

constexpr const char* kClosetOpenPath  = "Recursos/audio/SFX/ARMARIO/armario_abrindo.mp3";
constexpr const char* kClosetClosePath = "Recursos/audio/SFX/ARMARIO/armario_fechando.mp3";

constexpr int kThunderCount = 4;
constexpr float kMinFootstepSpeed = 35.0f;
constexpr float kThunderMinDelay = 18.0f;   
constexpr float kThunderMaxDelay = 42.0f;
constexpr float kThunderFlashDuration = 0.42f;
constexpr float kPostLoadingThunderDelay = 12.0f;
constexpr int kFootstepVolumePercent = 100;

Sound gBoxStartSound;
Sound gBoxStopSound;
Sound gBoxMovingLoopSound;
Sound gLighterOnSound;
Sound gLighterOffSound;
Sound gFootstepStoneSound;
Sound gFootstepWoodSound;
Sound gFootstepStairsSound;
Sound gThunderSounds[kThunderCount];
Sound gCandleLoopSound;
Sound gWindowOpenSound;
Sound gWindowCloseSound;
Sound gWindLoopSound;
Sound gCandleBlowOutSound;
Sound gCandleLightUpSound;
Sound gCandleBlowPlayerSound;
Sound gRepairSound;
Sound gClosetOpenSound;
Sound gClosetCloseSound;

bool gWindLoopActive = false;
bool gLoaded = false;
bool gGameplayMuted = false;
float gThunderTimer = 12.0f;
float gThunderFlashTimer = 0.0f;
bool gCandleLoopActive = false;
bool gBoxMovingLoopActive = false;
bool gBoxIsMoving = false;
bool gFootstepLoopActive = false;
FootstepSurface gFootstepLoopSurface = FootstepSurface::Stone;

bool FileExists(const char* path) {
    std::ifstream f(path);
    return f.good();
}

void EnsureLoaded() {
    if (gLoaded) {
        return;
    }
    gBoxStartSound.Open(kBoxStartPath);
    gBoxStopSound.Open(kBoxStopPath);
    gBoxMovingLoopSound.Open(kBoxMovingLoopPath);
    gLighterOnSound.Open(kLighterOnPath);
    gLighterOffSound.Open(kLighterOffPath);
    gFootstepStoneSound.Open(kFootstepStonePath);
    gFootstepWoodSound.Open(kFootstepWoodPath);
    gFootstepStairsSound.Open(kFootstepStairsPath);
    gWindowOpenSound.Open(kWindowOpenPath);
    gWindowCloseSound.Open(kWindowClosePath);
    
    
    // VENTO_LOOP.mp3 is an optional ambience asset that may not ship. Only try to
    // open it when present, so a missing file doesn't spam "Erro ao carregar som".
    if (FileExists(kWindLoopPath)) {
        gWindLoopSound.Open(kWindLoopPath);
    }
    if (FileExists(kCandleBlowPath)) {
        gCandleBlowOutSound.Open(kCandleBlowPath);
    }
    if (FileExists(kRepairPath)) gRepairSound.Open(kRepairPath);
    if (FileExists(kCandleLightUpPath)) gCandleLightUpSound.Open(kCandleLightUpPath);
    if (FileExists(kCandleBlowPlayerPath)) gCandleBlowPlayerSound.Open(kCandleBlowPlayerPath);
    if (FileExists(kClosetOpenPath))  gClosetOpenSound.Open(kClosetOpenPath);
    if (FileExists(kClosetClosePath)) gClosetCloseSound.Open(kClosetClosePath);
    for (int i = 0; i < kThunderCount; ++i) {
        gThunderSounds[i].Open(kThunderPaths[i]);
    }
    gCandleLoopSound.Open(kCandleLoopPath);
    gLoaded = true;
}

// Volume final do barramento de VFX = master × efeitos (sfx).
int SfxChannelVolume() {
    int vol = (MIX_MAX_VOLUME * Game::masterVolumePercent) / 100;
    vol = (vol * Game::sfxVolumePercent) / 100;
    return vol;
}

void ApplySfxVolume(Sound& sound) {
    const int channel = sound.GetChannel();
    if (channel >= 0) {
        Mix_Volume(channel, SfxChannelVolume());
    }
}

void PlaySound(Sound& sound) {
    if (gGameplayMuted) {
        return;
    }
    EnsureLoaded();
    if (sound.IsOpen()) {
        sound.Play();
        ApplySfxVolume(sound);   // todo VFX passa pelo barramento de efeitos
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

void StopBoxMovingLoop() {
    if (gBoxMovingLoopActive) {
        gBoxMovingLoopSound.Stop();
        gBoxMovingLoopActive = false;
    }
}

void StartBoxMovingLoop() {
    if (gGameplayMuted) {
        StopBoxMovingLoop();
        return;
    }
    EnsureLoaded();
    if (!gBoxMovingLoopSound.IsOpen()) {
        return;
    }
    const int channel = gBoxMovingLoopSound.GetChannel();
    if (gBoxMovingLoopActive && channel >= 0 && Mix_Playing(channel)) {
        return;
    }
    if (gBoxMovingLoopSound.PlayLoopedOnChannel(kChannelBox) >= 0) {
        gBoxMovingLoopActive = true;
        ApplySfxVolume(gBoxMovingLoopSound);
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
    if (gCandleLoopSound.PlayLoopedOnChannel(kChannelCandle) >= 0) {
        gCandleLoopActive = true;
        ApplySfxVolume(gCandleLoopSound);
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
    StopBoxMovingLoop();
    gBoxIsMoving = false;
}

void ApplyFootstepLoopVolume(Sound& sound) {
    const int channel = sound.GetChannel();
    if (channel < 0) {
        return;
    }
    int vol = SfxChannelVolume();
    vol = (vol * kFootstepVolumePercent) / 100;
    vol = std::min(MIX_MAX_VOLUME, vol + vol / 2);   // leve reforço nos passos
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
    if (next.PlayLoopedOnChannel(kChannelFootstep) >= 0) {
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
        ApplySfxVolume(gThunderSounds[idx]);   // trovão é um VFX
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
    EnsureLoaded();                                     // Força a carregar todos os sons "agora", antes da gameplay
    StopAllGameplayAudio();
    ResetThunderSchedule(kPostLoadingThunderDelay);
    gThunderFlashTimer = 0.0f;
}

void NotifyBoxSlide() {
    if (gGameplayMuted) {
        return;
    }

    if (!gBoxIsMoving) {
        PlaySound(gBoxStartSound);
        gBoxIsMoving = true;
    }
    StartBoxMovingLoop();
}

void MaintainBoxPushLoop() {
    if (gGameplayMuted || !gBoxIsMoving) {
        return;
    }
    StartBoxMovingLoop();
}

void NotifyBoxPushEnd() {
    if (gGameplayMuted) {
        StopBoxMovingLoop();
        gBoxIsMoving = false;
        return;
    }

    if (gBoxIsMoving) {
        StopBoxMovingLoop();
        PlaySound(gBoxStopSound);
        gBoxIsMoving = false;
        return;
    }

    StopBoxMovingLoop();
}

void PlayLighterToggle(bool turningOn) {
    PlaySound(turningOn ? gLighterOnSound : gLighterOffSound);
}

void UpdateBigBrotherFootsteps(float dt, float moveSpeed, bool isBigBrother, FootstepSurface surface) {
    if (!isBigBrother) {
        StopFootstepLoop();
        return;
    }

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

void PlayWindowToggle(bool opening) {
    if (gGameplayMuted) return;
    PlaySound(opening ? gWindowOpenSound : gWindowCloseSound);
}

void StartWindLoop() {
    if (gGameplayMuted || gWindLoopActive) return;
    EnsureLoaded();
    if (!gWindLoopSound.IsOpen()) return;   // optional asset absent → no-op
    if (gWindLoopSound.PlayLoopedOnChannel(kChannelWind) >= 0) {
        gWindLoopActive = true;
        ApplySfxVolume(gWindLoopSound);
    }
}

int CurrentSfxVolume() {
    return SfxChannelVolume();
}

void StopWindLoop() {
    if (gWindLoopActive) {
        gWindLoopSound.Stop();
        gWindLoopActive = false;
    }
}

void PlayCandleBlowOut() {
    PlaySound(gCandleBlowOutSound);
}

void StopAllGameplay() {
    StopAllGameplayAudio();   // passos, vela, caixa
    StopWindLoop();           // vento
}

void PlayCandleLightUp() { PlaySound(gCandleLightUpSound); }
void PlayCandleBlow()    { PlaySound(gCandleBlowPlayerSound); }
void PlayRepair() { PlaySound(gRepairSound); }
void PlayClosetOpen()  { PlaySound(gClosetOpenSound); }
void PlayClosetClose() { PlaySound(gClosetCloseSound); }

} // namespace GameSfx
