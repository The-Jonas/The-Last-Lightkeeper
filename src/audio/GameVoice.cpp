#include "audio/GameVoice.h"
#include "audio/Sound.h"
#include "core/Game.h"

#define INCLUDE_SDL_MIXER
#include "SDL_include.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace {

// ── Irmãozão ────────────────────────────────────────────────────────────────
constexpr const char* kBigCall    = "Recursos/audio/Dublagem/IRMÃOZÃO/Aqui.wav";
constexpr const char* kBigPickupA = "Recursos/audio/Dublagem/IRMÃOZÃO/Isso_vai_servir1.wav";
constexpr const char* kBigPickupB = "Recursos/audio/Dublagem/IRMÃOZÃO/E_Oque_temos.wav";
constexpr const char* kBigDrag    = "Recursos/audio/Dublagem/IRMÃOZÃO/isso_pesa.wav";
constexpr const char* kBigBagFull = "Recursos/audio/Dublagem/IRMÃOZÃO/minha_bolsa_ta_pesada.wav";
constexpr const char* kBigCant    = "Recursos/audio/Dublagem/IRMÃOZÃO/não_consigo.wav";
// Repreensões do irmãozão ao irmãozinho medroso.
constexpr const char* kBigScoldA  = "Recursos/audio/Dublagem/IRMÃOZÃO/para_de_ser_medroso.wav";
constexpr const char* kBigScoldB  = "Recursos/audio/Dublagem/IRMÃOZÃO/Quer_que_o_monstro_coma_voce.wav";

// ── Irmãozinho ──────────────────────────────────────────────────────────────
constexpr const char* kLilScaredA = "Recursos/audio/Dublagem/IRMÃOZINHO/Eu_tô_com_medo.wav";
constexpr const char* kLilScaredB = "Recursos/audio/Dublagem/IRMÃOZINHO/Eu_quero_ir_pra_casa.wav";
constexpr const char* kLilStayA   = "Recursos/audio/Dublagem/IRMÃOZINHO/E_se_o_monstro_estiver_la_fora.wav";
constexpr const char* kLilStayB   = "Recursos/audio/Dublagem/IRMÃOZINHO/eu_nao_quero.wav";
constexpr const char* kLilHide    = "Recursos/audio/Dublagem/IRMÃOZINHO/nao_sai_o_monstro.wav";

// ── Legendas (subtitles) ────────────────────────────────────────────────────
// Texto exibido enquanto cada fala toca. Inferido dos nomes dos arquivos —
// AJUSTE aqui para bater exatamente com o que foi dublado.
constexpr const char* kTxtBigCall    = "Aqui!";
constexpr const char* kTxtBigPickupA = "Isso vai servir.";
constexpr const char* kTxtBigPickupB = "É o que temos...";
constexpr const char* kTxtBigDrag    = "Ah, isso pesa...";
constexpr const char* kTxtBigBagFull = "Minha bolsa tá pesada.";
constexpr const char* kTxtBigCant    = "Não consigo.";
constexpr const char* kTxtBigScoldA  = "Para de ser medroso.";
constexpr const char* kTxtBigScoldB  = "Quer que o monstro te coma?";
constexpr const char* kTxtLilScaredA = "Eu tô com medo.";
constexpr const char* kTxtLilScaredB = "Eu quero ir pra casa.";
constexpr const char* kTxtLilStayA   = "E se o monstro estiver lá fora?";
constexpr const char* kTxtLilStayB   = "Eu não quero.";
constexpr const char* kTxtLilHide    = "Não sai! O monstro...";

Sound gCall, gPickupA, gPickupB, gDrag, gBagFull, gCant;
Sound gScoldA, gScoldB;
Sound gScaredA, gScaredB, gStayA, gStayB, gHide;

bool gLoaded = false;
bool gMuted = false;
int gChannel = -1;            // canal de voz dedicado (somente uma fala por vez)
Uint32 gNextAllowedMs = 0;    // cooldown global entre falas
std::string gCurrentSubtitle; // legenda da fala tocando agora (vazia = nenhuma)
Uint32 gSubtitleEndMs = 0;    // instante em que a legenda deve sumir (fim do clipe)

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
    gScoldA.Open(kBigScoldA);
    gScoldB.Open(kBigScoldB);
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
                           &gScoldA, &gScoldB,
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
bool Play(Sound& sound, const char* subtitle, Uint32 extraCooldownMs = 0) {
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
    gCurrentSubtitle = subtitle ? subtitle : "";

    // Duração real do clipe → a legenda some exatamente no fim (não dependemos do
    // estado do canal, que pode ser reaproveitado por um SFX depois).
    Uint32 durMs = 1500;   // fallback
    if (Mix_Chunk* c = sound.GetChunk()) {
        int freq = 44100, channels = 2;
        Uint16 fmt = AUDIO_S16SYS;
        if (Mix_QuerySpec(&freq, &fmt, &channels)) {
            const int bytesPerSample = SDL_AUDIO_BITSIZE(fmt) / 8;
            const double bytesPerSec = static_cast<double>(freq) * channels * bytesPerSample;
            if (bytesPerSec > 0.0 && c->alen > 0) {
                durMs = static_cast<Uint32>(c->alen * 1000.0 / bytesPerSec);
            }
        }
    }
    gSubtitleEndMs = now + durMs;
    return true;
}

bool Chance(int percent) {
    return (rand() % 100) < percent;
}

// Escolhe aleatoriamente uma das duas falas — e legenda com o texto da escolhida.
bool PlayOneOf2(Sound& a, const char* ta, Sound& b, const char* tb, Uint32 extraCooldownMs = 0) {
    return (rand() & 1) ? Play(a, ta, extraCooldownMs) : Play(b, tb, extraCooldownMs);
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
    gCurrentSubtitle.clear();
    gSubtitleEndMs = 0;
}

bool GetActiveSubtitle(std::string& out) {
    if (gMuted || gCurrentSubtitle.empty() || SDL_GetTicks() >= gSubtitleEndMs) {
        return false;
    }
    out = gCurrentSubtitle;
    return true;
}

void OnCallToFollow()       { Play(gCall, kTxtBigCall); }
void OnItemPickup()         { if (Chance(40)) PlayOneOf2(gPickupA, kTxtBigPickupA, gPickupB, kTxtBigPickupB); }
void OnPickupWoodPlank()    { Play(gPickupA, kTxtBigPickupA); }   // "Isso vai servir." SEMPRE (tábua de madeira)
void OnBagFull()            { Play(gBagFull, kTxtBigBagFull); }
void OnDragObject()         { if (Chance(35)) Play(gDrag, kTxtBigDrag, 6000); }
void OnActionBlocked()      { Play(gCant, kTxtBigCant, 800); }
void OnScoldFear()          { PlayOneOf2(gScoldA, kTxtBigScoldA, gScoldB, kTxtBigScoldB); }  // irmãozão repreende o medroso

// #4 "E se o monstro estiver lá fora?" (gStayA) foi movida para um gatilho
// aleatório enquanto escondido no armário (ver Closet). O protesto de "ficar
// parado" agora usa só "Eu não quero." (gStayB), que é o que faz sentido ali.
void OnAskToStay()          { Play(gStayB, kTxtLilStayB); }
void OnBrothersTooFar()     { PlayOneOf2(gScaredA, kTxtLilScaredA, gScaredB, kTxtLilScaredB); }
void OnHidingMonsterClose() { Play(gHide, kTxtLilHide, 3000); }
void OnHidingWhatIfMonster(){ Play(gStayA, kTxtLilStayA, 3000); }  // #4 sussurro medroso no armário

void DebugPlayRandomForControlled(bool bigBrother) {
    EnsureLoaded();
    // Escolhe uma fala aleatória do conjunto do irmão indicado.
    Sound* bigLines[] = {&gCall, &gPickupA, &gPickupB, &gDrag, &gBagFull, &gCant, &gScoldA, &gScoldB};
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
    gCurrentSubtitle.clear();   // debug: sem legenda associada
    gSubtitleEndMs = 0;
}

} // namespace GameVoice
