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
constexpr int kChannelMonsterScream  = 6;  // grito
constexpr int kChannelMonsterSpot    = 7;  // som de ver o player
constexpr int kChannelMonsterSteps   = 8;  // passos do monstro
constexpr int kChannelMonsterCreak   = 9;  // rangidos de madeira (esporádicos)
constexpr int kChannelHeartbeat      = 10; // batimento cardíaco (sanidade baixa)
constexpr int kChannelMonsterStep0   = 32; // passos por-frame: pool rotativo GRANDE (32..47),
constexpr int kMonsterStepPoolCount  = 16; // separado da voz/one-shots p/ os passos se SOBREPOREM sem se cortar
//9+ → one-shots livres (Mix_PlayChannel(-1, ...) usa a partir daqui)

constexpr const char* kBoxStartPath = "Recursos/audio/SFX/CAIXAS/CAIXA_Madeira.mp3";
constexpr const char* kBoxStopPath = "Recursos/audio/SFX/CAIXAS/CAIXA_Madeira2.mp3";
constexpr const char* kBoxMovingLoopPath = "Recursos/audio/SFX/CAIXAS/CAIXA_MADEIRALONGO.mp3";

constexpr const char* kLighterOnPath = "Recursos/audio/SFX/ISQUEIRO/ISQUEIRO_ ABRINDO.mp3";
constexpr const char* kLighterOffPath = "Recursos/audio/SFX/ISQUEIRO/ISQUEIRO_FECHANDO.mp3";

constexpr const char* kFootstepStonePath = "Recursos/audio/SFX/PASSOS/PASSOS_pedra.mp3";
constexpr const char* kFootstepWoodPath = "Recursos/audio/SFX/PASSOS/PASSOS_Madeira.mp3";
constexpr const char* kFootstepStairsPath = "Recursos/audio/SFX/PASSOS/PASSOS_ESCADA.mp3";

constexpr const char* kThunderPaths[] = {
    "Recursos/audio/SFX/TROVAO/trovao_1.mp3",
    "Recursos/audio/SFX/TROVAO/trovao_2.mp3",
    "Recursos/audio/SFX/TROVAO/trovao_3.mp3",
    "Recursos/audio/SFX/TROVAO/trovao_4.mp3",
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

constexpr const char* kMonsterScreamPath = "Recursos/audio/SFX/MONSTRO/monstro_grito.wav";
constexpr const char* kMonsterSpotPath   = "Recursos/audio/SFX/MONSTRO/monstro_ve_player.mp3";
// Passos por-frame: um som por frame da animação (acompanha a cadência, fica
// frenético quando o monstro acelera). Um arquivo por frame (kAnimFrameCount=5).
constexpr const char* kMonsterStepPaths[] = {
    "Recursos/audio/SFX/MONSTRO/mns_step_1.mp3",
    "Recursos/audio/SFX/MONSTRO/mns_step_2.mp3",
    "Recursos/audio/SFX/MONSTRO/mns_step_3.mp3",
    "Recursos/audio/SFX/MONSTRO/mns_step_4.mp3",
    "Recursos/audio/SFX/MONSTRO/mns_step_5.mp3",
};
constexpr int kMonsterStepCount = 5;
// Rangidos de madeira: tocam ESPORÁDICAMENTE junto dos passos do monstro.
constexpr const char* kHeartbeatPath = "Recursos/audio/SFX/heartbeat.mp3";
constexpr const char* kWoodCreakPaths[] = {
    "Recursos/audio/SFX/MONSTRO/wood_creak_1.ogg",
    "Recursos/audio/SFX/MONSTRO/wood_creak_2.ogg",
    "Recursos/audio/SFX/MONSTRO/wood_creak_3.ogg",
};
constexpr int kWoodCreakCount = 3;

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
Sound gMonsterScreamSound;
Sound gMonsterSpotSound;
Sound gMonsterStepSounds[kMonsterStepCount];   // passos por-frame
int   gMonsterStepRot = 0;                      // rotação no pool de canais
Sound gWoodCreakSounds[kWoodCreakCount];
float gWoodCreakTimer = 0.0f;   // conta até o próximo rangido
Sound gHeartbeatSound;
bool  gHeartbeatActive = false;

bool gMonsterStepsActive = false;
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
    if (FileExists(kMonsterScreamPath)) gMonsterScreamSound.Open(kMonsterScreamPath);
    if (FileExists(kMonsterSpotPath))   gMonsterSpotSound.Open(kMonsterSpotPath);
    for (int i = 0; i < kMonsterStepCount; ++i) {
        if (FileExists(kMonsterStepPaths[i])) gMonsterStepSounds[i].Open(kMonsterStepPaths[i]);
    }
    for (int i = 0; i < kWoodCreakCount; ++i) {
        if (FileExists(kWoodCreakPaths[i])) gWoodCreakSounds[i].Open(kWoodCreakPaths[i]);
    }
    if (FileExists(kHeartbeatPath)) gHeartbeatSound.Open(kHeartbeatPath);
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

// ── Spatial audio ─────────────────────────────────────────────────────────
void SetChannelSpatial(int channel, float srcX, float srcY, float listX, float listY, float maxDist) {
    float dx   = srcX - listX;
    float dy   = srcY - listY;
    float dist = std::sqrt(dx * dx + dy * dy);

    const float kMaxAudibleDist = (maxDist > 1.0f) ? maxDist : 800.0f;

    // Volume baseado na distância
    float volumeRatio = 1.0f - std::min(1.0f, dist / kMaxAudibleDist);

    // Panorâmica baseada no eixo X
    float panRatio = 0.0f;
    if (dist > 0.001f) {
        panRatio = dx / kMaxAudibleDist;
        panRatio = std::max(-1.0f, std::min(1.0f, panRatio));
    }

    Uint8 leftVol  = static_cast<Uint8>(255.0f * volumeRatio * (1.0f - std::max(0.0f,  panRatio)));
    Uint8 rightVol = static_cast<Uint8>(255.0f * volumeRatio * (1.0f + std::min(0.0f, panRatio)));

    Mix_SetPanning(channel, leftVol, rightVol);
}

void ClearChannelSpatial(int channel) {
    Mix_SetPanning(channel, 255, 255);
}

// ─────────────────────────────────────────────────────────

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

// Parou de mover (mas ainda grudado): silencia o loop de arrasto SEM o "thud" de
// soltar. O loop volta assim que NotifyBoxSlide sinalizar movimento de novo.
void PauseBoxPushLoop() {
    if (gBoxIsMoving) {
        StopBoxMovingLoop();
        gBoxIsMoving = false;
    }
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

void PlayBigThunder() {
    EnsureLoaded();
    // trovao_1 (idx 0) e trovao_2 (idx 1) tocam AO MESMO TEMPO, no volume máximo.
    // Ignora o mute de gameplay: é um efeito de clímax, não um loop ambiental.
    for (int i = 0; i < 2 && i < kThunderCount; ++i) {
        if (gThunderSounds[i].IsOpen()) {
            gThunderSounds[i].Play();
            const int ch = gThunderSounds[i].GetChannel();
            if (ch >= 0) {
                Mix_Volume(ch, MIX_MAX_VOLUME);
            }
        }
    }
    // NÃO mexe em gThunderFlashTimer: o clarão visual do clímax é a própria tela
    // clareando (RenderSceneTransition), e evitamos um flash azul solto depois.
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

// Loops de gameplay SEM o vento (o vento é disparado por evento de janela e não
// seria re-armado sozinho). Público (declarado no header) para o PAUSAR usar.
void StopAllGameplayAudio() {
    StopFootstepLoop();
    StopCandleLoop();
    StopBoxMovingLoop();
    gBoxIsMoving = false;
    if (gHeartbeatActive) {
        Mix_HaltChannel(kChannelHeartbeat);
        gHeartbeatActive = false;
    }
}

void StopAllGameplay() {
    StopAllGameplayAudio();   // passos, vela, caixa
    StopWindLoop();           // vento
}

// ── Sons do monstro ───────────────────────────────────────────────────────
void PlayMonsterScream() {
    if (gGameplayMuted) return;
    EnsureLoaded();
    if (!Mix_Playing(kChannelMonsterScream) && gMonsterScreamSound.IsOpen()) {
        gMonsterScreamSound.Play();
        ApplySfxVolume(gMonsterScreamSound);   // toca no volume cheio do barramento de VFX
    }
}

void PlayMonsterSpot() {
    if (gGameplayMuted) return;
    EnsureLoaded();
    if (!Mix_Playing(kChannelMonsterSpot) && gMonsterSpotSound.IsOpen()) {
        gMonsterSpotSound.Play();
        ApplySfxVolume(gMonsterSpotSound);     // toca no volume cheio do barramento de VFX
    }
}
 
// Um passo por FRAME de animação: acompanha a cadência das pernas (fica frenético
// ao acelerar/fugir). Toca num pool rotativo de canais para passos rápidos se
// sobreporem em vez de se cortarem; volume/panorâmica espaciais como o resto.
void PlayMonsterStep(int frameIndex, float monsterX, float monsterY, float playerX, float playerY, float maxDist) {
    if (gGameplayMuted) return;
    EnsureLoaded();
    if (frameIndex < 0) frameIndex = 0;
    const int idx = frameIndex % kMonsterStepCount;
    if (!gMonsterStepSounds[idx].IsOpen()) return;

    const int ch = kChannelMonsterStep0 + (gMonsterStepRot++ % kMonsterStepPoolCount);
    Mix_PlayChannel(ch, gMonsterStepSounds[idx].GetChunk(), 0);

    // VOLUME por DISTÂNCIA (perto = ALTO, longe = baixo), com curva acentuada para
    // o gradiente ser bem perceptível. A distância vai no volume do canal; a
    // panorâmica (esquerda/direita) fica só com a DIREÇÃO, sem re-atenuar.
    const float dx = monsterX - playerX;
    const float dy = monsterY - playerY;
    const float dist = std::sqrt(dx * dx + dy * dy);
    const float maxAud = (maxDist > 1.0f) ? maxDist : 800.0f;
    float t = 1.0f - std::min(1.0f, dist / maxAud);   // 1 = colado, 0 = no limite
    t = t * t;                                         // curva: perto MUITO mais alto que longe
    const int vol = static_cast<int>(SfxChannelVolume() * (0.05f + 0.95f * t));
    Mix_Volume(ch, vol);

    // Panorâmica só pela direção horizontal (não re-atenua por distância — isso já
    // está no Mix_Volume acima). Assim perto=alto/longe=baixo fica bem marcado.
    float pan = 0.0f;
    if (dist > 1.0f) pan = std::max(-1.0f, std::min(1.0f, dx / maxAud));
    const Uint8 l = static_cast<Uint8>(255.0f * (1.0f - std::max(0.0f, pan)));
    const Uint8 r = static_cast<Uint8>(255.0f * (1.0f + std::min(0.0f, pan)));
    Mix_SetPanning(ch, l, r);
}

// Agora só cuida dos RANGIDOS de madeira esporádicos (os passos viraram por-frame,
// via PlayMonsterStep). maxDist grande => perto alto, longe baixo.
void UpdateMonsterFootsteps(float dt, float moveSpeed, float monsterX, float monsterY, float playerX, float playerY, bool fleeing) {
    (void)fleeing;
    if (gGameplayMuted || moveSpeed <= 10.0f) return;
    EnsureLoaded();

    const float stepsMaxDist = 1900.0f + moveSpeed * 4.0f;
    gWoodCreakTimer -= dt;
    if (gWoodCreakTimer <= 0.0f) {
        gWoodCreakTimer = 2.0f + static_cast<float>(rand() % 350) / 100.0f;   // 2.0–5.5s
        const int ci = rand() % kWoodCreakCount;
        if (gWoodCreakSounds[ci].IsOpen()) {
            Mix_PlayChannel(kChannelMonsterCreak, gWoodCreakSounds[ci].GetChunk(), 0);
            Mix_Volume(kChannelMonsterCreak, SfxChannelVolume());
            SetChannelSpatial(kChannelMonsterCreak, monsterX, monsterY, playerX, playerY, stepsMaxDist);
        }
    }
}
 
void StopMonsterFootsteps() {
    for (int i = 0; i < kMonsterStepPoolCount; ++i) {
        Mix_HaltChannel(kChannelMonsterStep0 + i);      // passos por-frame
        ClearChannelSpatial(kChannelMonsterStep0 + i);
    }
    Mix_HaltChannel(kChannelMonsterSteps);              // loop legado (se estiver ativo)
    ClearChannelSpatial(kChannelMonsterSteps);
    gMonsterStepsActive = false;
}

// Parada TOTAL de áudio de efeitos: usada nas transições (nível↔menu). Silencia
// TODOS os canais do mixer — inclusive os que StopAllGameplay não pega (rádio no
// canal 5, ondas, vento, grito/spot do monstro e quaisquer one-shots) — e zera os
// flags de loop. NÃO mexe na música (Mix_Music), que cada estado gerencia.
void HardStopAll() {
    StopAllGameplay();        // passos/vela/caixa/vento + reseta flags
    StopMonsterFootsteps();   // pool de passos do monstro + rangidos
    Mix_HaltChannel(-1);      // varre TODO o resto: rádio(5), ondas(0), vento(1), grito, spot...
    for (int ch = 0; ch < 48; ++ch) ClearChannelSpatial(ch);
    gHeartbeatActive = false;
    gBoxIsMoving = false;
}

void UpdateHeartbeat(float intensity01) {
    // intensity01: 0 = calmo (silêncio) → 1 = pânico (volume máximo). Loop cujo
    // volume acompanha a sanidade baixa (feedback de perigo).
    if (gGameplayMuted || intensity01 <= 0.02f || !gHeartbeatSound.IsOpen()) {
        if (gHeartbeatActive) {
            Mix_HaltChannel(kChannelHeartbeat);
            gHeartbeatActive = false;
        }
        return;
    }
    if (!gHeartbeatActive) {
        gHeartbeatSound.PlayLoopedOnChannel(kChannelHeartbeat);
        gHeartbeatActive = true;
    }
    const float v = std::min(1.0f, intensity01);
    Mix_Volume(kChannelHeartbeat, static_cast<int>(SfxChannelVolume() * v));
}

void PlayCandleLightUp() { PlaySound(gCandleLightUpSound); }
void PlayCandleBlow()    { PlaySound(gCandleBlowPlayerSound); }
void PlayRepair() { PlaySound(gRepairSound); }
void PlayClosetOpen()  { PlaySound(gClosetOpenSound); }
void PlayClosetClose() { PlaySound(gClosetCloseSound); }


} // namespace GameSfx
