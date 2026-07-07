#include "audio/GameVoice.h"
#include "audio/Sound.h"
#include "core/Game.h"

#define INCLUDE_SDL_MIXER
#include "SDL_include.h"

#include <algorithm>
#include <cmath>
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

// A dublagem é propositalmente MUITO mais alta que o resto do jogo: as
// gravações são baixas e o master costuma ficar baixo por causa do ambiente.
// Por isso a voz é desacoplada do master e recebe um ganho forte, saturando
// no teto do mixer (MIX_MAX_VOLUME) — nunca fica abafada pelo ambiente.
constexpr int kVoiceGainPercent = 250;   // +25% sobre o reforço anterior

// Mix_Volume satura em 128, então não dá para deixar o irmãozão (gravado bem
// mais baixo que o irmãozinho) na mesma altura só com volume de canal. A
// solução é NORMALIZAR o PCM de cada fala no carregamento: amplificamos cada
// chunk até um pico-alvo comum, o que iguala a percepção de volume entre os
// dois irmãos e ainda sobe o nível geral. Formato do mixer = MIX_DEFAULT_FORMAT
// (AUDIO_S16), então as amostras são Sint16.
void NormalizeChunkToPeak(Mix_Chunk* c, float targetFrac, float maxGain) {
    if (!c || !c->abuf || c->alen < sizeof(Sint16)) {
        return;
    }
    Sint16* samples = reinterpret_cast<Sint16*>(c->abuf);
    const size_t n = c->alen / sizeof(Sint16);
    int peak = 1;
    for (size_t i = 0; i < n; ++i) {
        const int a = std::abs(static_cast<int>(samples[i]));
        if (a > peak) peak = a;
    }
    float gain = (targetFrac * 32767.0f) / static_cast<float>(peak);
    gain = std::min(gain, maxGain);
    if (gain <= 1.001f) {
        return;   // já está alto o suficiente; não amplifica (evitaria clipping/ruído)
    }
    for (size_t i = 0; i < n; ++i) {
        int v = static_cast<int>(std::lround(samples[i] * gain));
        v = std::max(-32768, std::min(32767, v));
        samples[i] = static_cast<Sint16>(v);
    }
}

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

    // Normaliza TODAS as falas ao mesmo pico -> irmãozão e irmãozinho ficam com
    // o mesmo volume percebido e o nível geral sobe (as gravações eram baixas).
    // targetFrac 0.97 = quase o fundo de escala; maxGain 10x cobre falas bem
    // baixas (irmãozão) sem estourar as que já vêm altas (irmãozinho).
    Sound* voiceLines[] = {&gCall, &gPickupA, &gPickupB, &gDrag, &gBagFull, &gCant,
                           &gScaredA, &gScaredB, &gStayA, &gStayB, &gHide};
    for (Sound* s : voiceLines) {
        if (s && s->IsOpen()) {
            NormalizeChunkToPeak(s->GetChunk(), 0.97f, 10.0f);
        }
    }

    gLoaded = true;
}

bool VoiceBusy() {
    return gChannel >= 0 && Mix_Playing(gChannel);
}

// Aplica o volume da dublagem no canal de voz atual (ver comentário de
// kVoiceGainPercent). Compartilhado entre a reprodução normal e o debug.
void ApplyVoiceChannelVolume() {
    if (gChannel >= 0) {
        int vol = (MIX_MAX_VOLUME * Game::voiceVolumePercent) / 100;
        vol = std::min(MIX_MAX_VOLUME, (vol * kVoiceGainPercent) / 100);
        Mix_Volume(gChannel, vol);
    }
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
    ApplyVoiceChannelVolume();
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

void DebugPlayRandomForControlled(bool bigBrother) {
    EnsureLoaded();
    // Escolhe uma fala aleatória do conjunto do irmão indicado.
    Sound* bigLines[] = {&gCall, &gPickupA, &gPickupB, &gDrag, &gBagFull, &gCant};
    Sound* lilLines[] = {&gScaredA, &gScaredB, &gStayA, &gStayB, &gHide};
    Sound* line = bigBrother
                      ? bigLines[rand() % (sizeof(bigLines) / sizeof(bigLines[0]))]
                      : lilLines[rand() % (sizeof(lilLines) / sizeof(lilLines[0]))];
    if (!line || !line->IsOpen()) {
        return;
    }
    // Debug: ignora mute/cooldown e corta a fala atual para sempre responder.
    StopAll();
    line->Play();
    gChannel = line->GetChannel();
    ApplyVoiceChannelVolume();
    gNextAllowedMs = SDL_GetTicks() + kGlobalCooldownMs;
}

} // namespace GameVoice
