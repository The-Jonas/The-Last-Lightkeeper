#include "audio/GameVoice.h"
#include "audio/Sound.h"
#include "core/Game.h"

#define INCLUDE_SDL_MIXER
#include "SDL_include.h"

#include <cstdlib>

namespace {

// ── Irmãozão ────────────────────────────────────────────────────────────────
constexpr const char* kBigCall    = "Recursos/audio/Dublagem/IRMÃOZÃO/Aqui.wav";
constexpr const char* kBigPickupA = "Recursos/audio/Dublagem/IRMÃOZÃO/Isso_vai_servir1.wav";
constexpr const char* kBigPickupB = "Recursos/audio/Dublagem/IRMÃOZÃO/E_Oque_temos.wav";
constexpr const char* kBigDrag    = "Recursos/audio/Dublagem/IRMÃOZÃO/isso_pesa.wav";
constexpr const char* kBigBagFull = "Recursos/audio/Dublagem/IRMÃOZÃO/minha_bolsa_ta_pesada.wav";
constexpr const char* kBigCant    = "Recursos/audio/Dublagem/IRMÃOZÃO/não_consigo.wav";

// ── Irmãozinho ──────────────────────────────────────────────────────────────
constexpr const char* kLilScaredA = "Recursos/audio/Dublagem/IRMÃOZINHO/Eu_tô_com_medo.wav";
constexpr const char* kLilScaredB = "Recursos/audio/Dublagem/IRMÃOZINHO/Eu_quero_ir_pra_casa.wav";
constexpr const char* kLilStayA   = "Recursos/audio/Dublagem/IRMÃOZINHO/E_se_o_monstro_estiver_la_fora.wav";
constexpr const char* kLilStayB   = "Recursos/audio/Dublagem/IRMÃOZINHO/eu_nao_quero.wav";
constexpr const char* kLilHide    = "Recursos/audio/Dublagem/IRMÃOZINHO/nao_sai_o_monstro.wav";

Sound gCall, gPickupA, gPickupB, gDrag, gBagFull, gCant;
Sound gScaredA, gScaredB, gStayA, gStayB, gHide;

bool gLoaded = false;
bool gMuted = false;
int gChannel = -1;            // canal de voz dedicado (somente uma fala por vez)
Uint32 gNextAllowedMs = 0;    // cooldown global entre falas

constexpr Uint32 kGlobalCooldownMs = 1200;

void EnsureLoaded() {
    if (gLoaded) {
        return;
    }
    gCall.Open(kBigCall);
    gPickupA.Open(kBigPickupA);
    gPickupB.Open(kBigPickupB);
    gDrag.Open(kBigDrag);
    gBagFull.Open(kBigBagFull);
    gCant.Open(kBigCant);
    gScaredA.Open(kLilScaredA);
    gScaredB.Open(kLilScaredB);
    gStayA.Open(kLilStayA);
    gStayB.Open(kLilStayB);
    gHide.Open(kLilHide);
    gLoaded = true;
}

bool VoiceBusy() {
    return gChannel >= 0 && Mix_Playing(gChannel);
}

// Toca a fala se: não estiver mudo, o cooldown global já passou e nenhuma outra
// fala estiver tocando. extraCooldownMs estende a pausa para falas que poderiam
// repetir muito (arrastar, esconder).
bool Play(Sound& sound, Uint32 extraCooldownMs = 0) {
    if (gMuted) {
        return false;
    }
    EnsureLoaded();
    const Uint32 now = SDL_GetTicks();
    if (now < gNextAllowedMs || VoiceBusy() || !sound.IsOpen()) {
        return false;
    }
    sound.Play();
    gChannel = sound.GetChannel();
    if (gChannel >= 0) {
        // Volume da dublagem = master × volume de voz (canal próprio).
        int vol = (MIX_MAX_VOLUME * Game::masterVolumePercent) / 100;
        vol = (vol * Game::voiceVolumePercent) / 100;
        Mix_Volume(gChannel, vol);
    }
    gNextAllowedMs = now + kGlobalCooldownMs + extraCooldownMs;
    return true;
}

bool Chance(int percent) {
    return (rand() % 100) < percent;
}

Sound& PickOf2(Sound& a, Sound& b) {
    return (rand() & 1) ? a : b;
}

} // namespace

namespace GameVoice {

void NotifyLoadingBegin() {
    gMuted = true;
    StopAll();
}

void NotifyLoadingEnd() {
    gMuted = false;
}

void StopAll() {
    if (gChannel >= 0 && Mix_Playing(gChannel)) {
        Mix_HaltChannel(gChannel);
    }
    gChannel = -1;
}

void OnCallToFollow()       { Play(gCall); }
void OnItemPickup()         { if (Chance(40)) Play(PickOf2(gPickupA, gPickupB)); }
void OnBagFull()            { Play(gBagFull); }
void OnDragObject()         { if (Chance(35)) Play(gDrag, 6000); }
void OnActionBlocked()      { Play(gCant, 800); }

void OnAskToStay()          { Play(PickOf2(gStayA, gStayB)); }
void OnBrothersTooFar()     { Play(PickOf2(gScaredA, gScaredB)); }
void OnHidingMonsterClose() { Play(gHide, 3000); }

} // namespace GameVoice
