#include "audio/GameSfx.h"
#include "audio/Sound.h"
#include "core/Game.h"

#define INCLUDE_SDL_MIXER
#include "SDL_include.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <string>

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
constexpr int kChannelWindowBreak = 11;
constexpr int kChannelHudFireLoop  = 12;   // crepitar da aura da chama na HUD
constexpr int kChannelHudFireStart = 13;   // "fwoosh" quando a aura acende
constexpr int kChannelMonsterStep0   = 32; // passos por-frame: pool rotativo GRANDE (32..47),
constexpr int kMonsterStepPoolCount  = 16; // separado da voz/one-shots p/ os passos se SOBREPOREM sem se cortar
//9+ → one-shots livres (Mix_PlayChannel(-1, ...) usa a partir daqui)

// Caixas/barris: "empurrar" é o tranco curto do começo (o arrasto entra junto, em
// loop); "parar" é o assentar ao soltar. Variações sorteadas.
constexpr int kBoxStartCount = 3;
constexpr const char* kBoxStartPaths[kBoxStartCount] = {
    "Recursos/audio/SFX/CAIXAS/caixa_empurrar_1.ogg",
    "Recursos/audio/SFX/CAIXAS/caixa_empurrar_2.ogg",
    "Recursos/audio/SFX/CAIXAS/caixa_empurrar_3.ogg",
};
constexpr int kBoxStopCount = 2;
constexpr const char* kBoxStopPaths[kBoxStopCount] = {
    "Recursos/audio/SFX/CAIXAS/caixa_parar_1.ogg",
    "Recursos/audio/SFX/CAIXAS/caixa_parar_2.ogg",
};
constexpr const char* kBoxMovingLoopPath = "Recursos/audio/SFX/CAIXAS/caixa_arrastar_loop.ogg";
constexpr int kBoxLoopFadeMs = 90;          // soltar/parar o arrasto sem clique

constexpr const char* kLighterOnPath = "Recursos/audio/SFX/ISQUEIRO/ISQUEIRO_ ABRINDO.mp3";
constexpr const char* kLighterOffPath = "Recursos/audio/SFX/ISQUEIRO/ISQUEIRO_FECHANDO.mp3";
// Isqueiro com variações: "acender" = abrir a tampa + riscar a roda num arquivo só;
// kLighterFlameAt diz em que segundo do arquivo a chama pega (a luz acende ali).
// Sem os arquivos novos, cai nos dois antigos acima.
constexpr int kLighterVariants = 4;
constexpr const char* kLighterIgnitePaths[kLighterVariants] = {
    "Recursos/audio/SFX/ISQUEIRO/isqueiro_acender_1.ogg",
    "Recursos/audio/SFX/ISQUEIRO/isqueiro_acender_2.ogg",
    "Recursos/audio/SFX/ISQUEIRO/isqueiro_acender_3.ogg",
    "Recursos/audio/SFX/ISQUEIRO/isqueiro_acender_4.ogg",
};
constexpr float kLighterFlameAt[kLighterVariants] = {0.479f, 0.627f, 0.489f, 0.569f};   // medido no pico do risco
constexpr const char* kLighterClosePaths[kLighterVariants] = {
    "Recursos/audio/SFX/ISQUEIRO/isqueiro_fechar_1.ogg",
    "Recursos/audio/SFX/ISQUEIRO/isqueiro_fechar_2.ogg",
    "Recursos/audio/SFX/ISQUEIRO/isqueiro_fechar_3.ogg",
    "Recursos/audio/SFX/ISQUEIRO/isqueiro_fechar_4.ogg",
};
constexpr const char* kReloadPourPath = "Recursos/audio/SFX/ISQUEIRO/recarga_fluido.ogg";   // 2 s = Inventory::kReloadDuration
constexpr const char* kLampReloadPath = "Recursos/audio/pickup/recarga_lamparina.ogg";      // galão: "glub glub", 2 s

// Lamparina: abrir (acender) e fechar (apagar), cliques de metal.
constexpr int kLampToggleCount = 3;
constexpr const char* kLampOnPaths[kLampToggleCount] = {
    "Recursos/audio/pickup/lamparina_abrir_1.ogg",
    "Recursos/audio/pickup/lamparina_abrir_2.ogg",
    "Recursos/audio/pickup/lamparina_abrir_3.ogg",
};
constexpr const char* kLampOffPaths[kLampToggleCount] = {
    "Recursos/audio/pickup/lamparina_fechar_1.ogg",
    "Recursos/audio/pickup/lamparina_fechar_2.ogg",
    "Recursos/audio/pickup/lamparina_fechar_3.ogg",
};

// Coletar item: som próprio por tipo; o resto usa os genéricos.
constexpr int kPickupGenericCount = 7;
constexpr const char* kPickupGenericPaths[kPickupGenericCount] = {
    "Recursos/audio/pickup/coletar_1.ogg", "Recursos/audio/pickup/coletar_2.ogg",
    "Recursos/audio/pickup/coletar_3.ogg", "Recursos/audio/pickup/coletar_4.ogg",
    "Recursos/audio/pickup/coletar_5.ogg", "Recursos/audio/pickup/coletar_6.ogg",
    "Recursos/audio/pickup/coletar_7.ogg",
};
constexpr int kPickupFuelCount = 6;
constexpr const char* kPickupFuelPaths[kPickupFuelCount] = {
    "Recursos/audio/pickup/coletar_combustivel_1.ogg", "Recursos/audio/pickup/coletar_combustivel_2.ogg",
    "Recursos/audio/pickup/coletar_combustivel_3.ogg", "Recursos/audio/pickup/coletar_combustivel_4.ogg",
    "Recursos/audio/pickup/coletar_combustivel_5.ogg", "Recursos/audio/pickup/coletar_combustivel_6.ogg",
};
constexpr int kPickupLampCount = 4;
constexpr const char* kPickupLampPaths[kPickupLampCount] = {
    "Recursos/audio/pickup/coletar_lamparina_1.ogg", "Recursos/audio/pickup/coletar_lamparina_2.ogg",
    "Recursos/audio/pickup/coletar_lamparina_3.ogg", "Recursos/audio/pickup/coletar_lamparina_4.ogg",
};
constexpr int kPickupPlankCount = 3;
constexpr const char* kPickupPlankPaths[kPickupPlankCount] = {
    "Recursos/audio/pickup/coletar_tabua_1.ogg", "Recursos/audio/pickup/coletar_tabua_2.ogg",
    "Recursos/audio/pickup/coletar_tabua_3.ogg",
};

// Passos avulsos (um por pisada, tocados no quadro da animação em que o pé toca o chão).
constexpr int kStepVariants = 6;
constexpr const char* kStepPaths[3][kStepVariants] = {   // [FootstepSurface][variação]
    {"Recursos/audio/SFX/PASSOS/passo_pedra_1.ogg", "Recursos/audio/SFX/PASSOS/passo_pedra_2.ogg",
     "Recursos/audio/SFX/PASSOS/passo_pedra_3.ogg", "Recursos/audio/SFX/PASSOS/passo_pedra_4.ogg",
     "Recursos/audio/SFX/PASSOS/passo_pedra_5.ogg", "Recursos/audio/SFX/PASSOS/passo_pedra_6.ogg"},
    {"Recursos/audio/SFX/PASSOS/passo_madeira_1.ogg", "Recursos/audio/SFX/PASSOS/passo_madeira_2.ogg",
     "Recursos/audio/SFX/PASSOS/passo_madeira_3.ogg", "Recursos/audio/SFX/PASSOS/passo_madeira_4.ogg",
     "Recursos/audio/SFX/PASSOS/passo_madeira_5.ogg", "Recursos/audio/SFX/PASSOS/passo_madeira_6.ogg"},
    {"Recursos/audio/SFX/PASSOS/passo_escada_1.ogg", "Recursos/audio/SFX/PASSOS/passo_escada_2.ogg",
     "Recursos/audio/SFX/PASSOS/passo_escada_3.ogg", "Recursos/audio/SFX/PASSOS/passo_escada_4.ogg",
     "Recursos/audio/SFX/PASSOS/passo_escada_5.ogg", "Recursos/audio/SFX/PASSOS/passo_escada_6.ogg"},
};

constexpr const char* kFootstepStonePath = "Recursos/audio/SFX/PASSOS/PASSOS_pedra.mp3";
constexpr const char* kFootstepWoodPath = "Recursos/audio/SFX/PASSOS/PASSOS_Madeira.mp3";
constexpr const char* kFootstepStairsPath = "Recursos/audio/SFX/PASSOS/PASSOS_ESCADA.mp3";

constexpr const char* kThunderPaths[] = {
    "Recursos/audio/SFX/TROVAO/trovao_1.mp3",
    "Recursos/audio/SFX/TROVAO/trovao_2.mp3",
    "Recursos/audio/SFX/TROVAO/trovao_3.mp3",
    "Recursos/audio/SFX/TROVAO/trovao_4.mp3",
};

constexpr const char* kWindowBreakPath = "Recursos/audio/SFX/JANELA/janela_quebrar.ogg";

constexpr const char* kCandleLoopPath = "Recursos/audio/SFX/VELA/FOGO_VELA.mp3";
constexpr const char* kCandleLightUpPath = "Recursos/audio/SFX/VELA/VELA_ACENDENDO.mp3";
constexpr const char* kCandleBlowPlayerPath = "Recursos/audio/SFX/VELA/VELA_APAGANDO_JOGADOR.mp3";

constexpr const char* kWindowOpenPath = "Recursos/audio/SFX/JANELA/janela_abrir.ogg";
constexpr const char* kWindowClosePath = "Recursos/audio/SFX/JANELA/janela_fechar.ogg";
constexpr const char* kWindLoopPath = "Recursos/audio/SFX/JANELA/vento_loop.ogg";
constexpr const char* kCandleBlowPath = "Recursos/audio/SFX/JANELA/vela_apagando.ogg";

constexpr const char* kHudFireLoopPath  = "Recursos/audio/SFX/HUD/fogo_destaque_loop.ogg";
constexpr const char* kHudFireStartPath = "Recursos/audio/SFX/HUD/fogo_destaque_inicio.ogg";
// Mochila: zíper ao abrir/fechar a pasta, vasculhar ao trocar de item, papel ao
// folhear a pasta. Variações sorteadas (sem repetir a anterior) para não cansar.
constexpr const char* kBagOpenPath  = "Recursos/audio/SFX/MOCHILA/mochila_abrir.ogg";
constexpr const char* kBagClosePath = "Recursos/audio/SFX/MOCHILA/mochila_fechar.ogg";
constexpr const char* kItemCyclePaths[] = {
    "Recursos/audio/SFX/MOCHILA/item_trocar_1.ogg",
    "Recursos/audio/SFX/MOCHILA/item_trocar_2.ogg",
    "Recursos/audio/SFX/MOCHILA/item_trocar_3.ogg",
    "Recursos/audio/SFX/MOCHILA/item_trocar_4.ogg",
};
constexpr int kItemCycleCount = 4;
constexpr const char* kPaperPaths[] = {
    "Recursos/audio/SFX/MOCHILA/papel_1.ogg",
    "Recursos/audio/SFX/MOCHILA/papel_2.ogg",
    "Recursos/audio/SFX/MOCHILA/papel_3.ogg",
};
constexpr int kPaperCount = 3;
constexpr float kBagZipGain    = 0.80f;   // zíper da mochila
constexpr float kItemCycleGain = 0.60f;   // vasculhar (troca de item): discreto, repete muito
constexpr float kPaperGain     = 0.55f;   // folhear a pasta

constexpr float kHudFireLoopGain  = 0.55f;   // crepitar: fundo, não notificação
constexpr float kHudFireStartGain = 0.70f;   // "fwoosh" de entrada
constexpr float kHudFireStartAt   = 0.01f;   // nível abaixo do qual a aura conta como apagada

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

Sound gBoxStartSounds[kBoxStartCount];
Sound gBoxStopSounds[kBoxStopCount];
int   gLastBoxStart = -1;
int   gLastBoxStop = -1;
int   gBoxStartChannel = -1;   // tranco em curso (não empilha em arrancadas rápidas)
Sound gBoxMovingLoopSound;
Sound gLighterOnSound;
Sound gLighterOffSound;
Sound gLighterIgniteSounds[kLighterVariants];
Sound gLighterCloseSounds[kLighterVariants];
int   gLastLighterIgnite = -1;
int   gLastLighterClose = -1;
int   gLighterChannel = -1;          // canal do "acender" em curso (cancelar no meio)
Sound gReloadPourSound;
Sound gLampReloadSound;
Sound gLampOnSounds[kLampToggleCount];
Sound gLampOffSounds[kLampToggleCount];
int   gLastLampOn = -1, gLastLampOff = -1;
Sound gPickupGenericSounds[kPickupGenericCount];
Sound gPickupFuelSounds[kPickupFuelCount];
Sound gPickupLampSounds[kPickupLampCount];
Sound gPickupPlankSounds[kPickupPlankCount];
int   gLastPickupGeneric = -1, gLastPickupFuel = -1, gLastPickupLamp = -1, gLastPickupPlank = -1;
Sound gStepSounds[3][kStepVariants];
int   gLastStep[3] = {-1, -1, -1};
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
Sound gWindowBreakSound;
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
Sound gBagOpenSound;
Sound gBagCloseSound;
Sound gItemCycleSounds[kItemCycleCount];
Sound gPaperSounds[kPaperCount];
int   gLastItemCycle = -1;   // variação tocada por último (não repete)
int   gLastPaper = -1;
int   gItemCycleChannel = -1;   // canal do vasculhar em curso (troca rápida corta o anterior)
int   gPaperChannel = -1;
Sound gHudFireLoopSound;
Sound gHudFireStartSound;
bool  gHudFireLoopActive = false;
float gHudFireLastLevel = 0.0f;   // nível do frame anterior (o "fwoosh" só ao sair do zero)

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

// Abre cada arquivo da lista que existir (os que faltam ficam fechados e são pulados).
void OpenAll(Sound* sounds, const char* const* paths, int count) {
    for (int i = 0; i < count; ++i) {
        if (FileExists(paths[i])) sounds[i].Open(paths[i]);
    }
}

void EnsureLoaded() {
    if (gLoaded) {
        return;
    }
    for (int i = 0; i < kBoxStartCount; ++i) {
        if (FileExists(kBoxStartPaths[i])) gBoxStartSounds[i].Open(kBoxStartPaths[i]);
    }
    for (int i = 0; i < kBoxStopCount; ++i) {
        if (FileExists(kBoxStopPaths[i])) gBoxStopSounds[i].Open(kBoxStopPaths[i]);
    }
    gBoxMovingLoopSound.Open(kBoxMovingLoopPath);
    gLighterOnSound.Open(kLighterOnPath);
    gLighterOffSound.Open(kLighterOffPath);
    for (int i = 0; i < kLighterVariants; ++i) {
        if (FileExists(kLighterIgnitePaths[i])) gLighterIgniteSounds[i].Open(kLighterIgnitePaths[i]);
        if (FileExists(kLighterClosePaths[i]))  gLighterCloseSounds[i].Open(kLighterClosePaths[i]);
    }
    if (FileExists(kReloadPourPath)) gReloadPourSound.Open(kReloadPourPath);
    if (FileExists(kLampReloadPath)) gLampReloadSound.Open(kLampReloadPath);
    OpenAll(gLampOnSounds, kLampOnPaths, kLampToggleCount);
    OpenAll(gLampOffSounds, kLampOffPaths, kLampToggleCount);
    OpenAll(gPickupGenericSounds, kPickupGenericPaths, kPickupGenericCount);
    OpenAll(gPickupFuelSounds, kPickupFuelPaths, kPickupFuelCount);
    OpenAll(gPickupLampSounds, kPickupLampPaths, kPickupLampCount);
    OpenAll(gPickupPlankSounds, kPickupPlankPaths, kPickupPlankCount);
    for (int sfc = 0; sfc < 3; ++sfc) {
        for (int i = 0; i < kStepVariants; ++i) {
            if (FileExists(kStepPaths[sfc][i])) gStepSounds[sfc][i].Open(kStepPaths[sfc][i]);
        }
    }
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
    if (FileExists(kWindowBreakPath)) gWindowBreakSound.Open(kWindowBreakPath);
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
    if (FileExists(kBagOpenPath))  gBagOpenSound.Open(kBagOpenPath);
    if (FileExists(kBagClosePath)) gBagCloseSound.Open(kBagClosePath);
    for (int i = 0; i < kItemCycleCount; ++i) {
        if (FileExists(kItemCyclePaths[i])) gItemCycleSounds[i].Open(kItemCyclePaths[i]);
    }
    for (int i = 0; i < kPaperCount; ++i) {
        if (FileExists(kPaperPaths[i])) gPaperSounds[i].Open(kPaperPaths[i]);
    }
    if (FileExists(kHudFireLoopPath))  gHudFireLoopSound.Open(kHudFireLoopPath);
    if (FileExists(kHudFireStartPath)) gHudFireStartSound.Open(kHudFireStartPath);
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
        gBoxMovingLoopSound.FadeOut(kBoxLoopFadeMs);
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

// Toca um som num canal livre com o volume do barramento × gain. Devolve o canal (-1 = falhou).
int PlayWithGain(Sound& sound, float gain) {
    if (gGameplayMuted) return -1;
    EnsureLoaded();
    if (!sound.IsOpen()) return -1;
    const int ch = Mix_PlayChannel(-1, sound.GetChunk(), 0);
    if (ch >= 0) Mix_Volume(ch, static_cast<int>(SfxChannelVolume() * gain));
    return ch;
}

// Sorteia uma variação aberta diferente da última; -1 se nenhuma abriu.
int PickVariation(Sound* sounds, int count, int last) {
    int open = 0;
    for (int i = 0; i < count; ++i) if (sounds[i].IsOpen()) ++open;
    if (open == 0) return -1;
    for (int tries = 0; tries < 8; ++tries) {
        const int i = std::rand() % count;
        if (sounds[i].IsOpen() && (i != last || open == 1)) return i;
    }
    for (int i = 0; i < count; ++i) if (sounds[i].IsOpen()) return i;
    return -1;
}

// Uma variação sorteada; se a anterior ainda toca no `channel`, corta antes
// (trocar rápido não empilha barulho).
void PlayVariation(Sound* sounds, int count, int& last, int& channel, float gain) {
    if (gGameplayMuted) return;
    EnsureLoaded();
    const int i = PickVariation(sounds, count, last);
    if (i < 0) return;
    for (int k = 0; k < count && channel >= 0; ++k) {
        if (Mix_Playing(channel) && Mix_GetChunk(channel) == sounds[k].GetChunk()) {
            Mix_HaltChannel(channel);
            break;
        }
    }
    last = i;
    channel = PlayWithGain(sounds[i], gain);
}

// Corta o crepitar da aura da HUD (pausa, morte, carregamento).
void StopHudFireLoop() {
    if (gHudFireLoopActive) {
        Mix_HaltChannel(kChannelHudFireLoop);
        gHudFireLoopActive = false;
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

// Alcance audivel de uma janela. Generoso de proposito: o objectivo e o jogador
// perceber que o monstro abriu ALGUMA janela do andar, mesmo longe.
constexpr float kWindowAudibleDist = 2600.0f;

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

// Carrega todos os efeitos de uma vez, durante o "Carregando...", para a entrada
// no jogo não travar decodificando sons.
void Preload() {
    EnsureLoaded();
}


void NotifyBoxSlide() {
    if (gGameplayMuted) {
        return;
    }

    if (!gBoxIsMoving) {
        // Não empilha o "início de arrasto" se ele ainda estiver tocando (arrancadas
        // rápidas / re-grude faziam o CAIXA_Madeira soar 2–3x seguidas).
        if (gBoxStartChannel < 0 || !Mix_Playing(gBoxStartChannel)) {
            const int i = PickVariation(gBoxStartSounds, kBoxStartCount, gLastBoxStart);
            if (i >= 0) {
                gLastBoxStart = i;
                gBoxStartChannel = PlayWithGain(gBoxStartSounds[i], 1.0f);
            }
        }
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
        const int i = PickVariation(gBoxStopSounds, kBoxStopCount, gLastBoxStop);
        if (i >= 0) {
            gLastBoxStop = i;
            PlayWithGain(gBoxStopSounds[i], 1.0f);
        }
        gBoxIsMoving = false;
        return;
    }

    StopBoxMovingLoop();
}

// Ligar = PlayLighterIgnite (sem esperar a chama); desligar = fechar a tampa.
void PlayLighterToggle(bool turningOn) {
    if (turningOn) {
        PlayLighterIgnite();
        return;
    }
    CancelLighterIgnite();
    EnsureLoaded();
    const int i = PickVariation(gLighterCloseSounds, kLighterVariants, gLastLighterClose);
    if (i < 0) { PlaySound(gLighterOffSound); return; }
    gLastLighterClose = i;
    PlayWithGain(gLighterCloseSounds[i], 1.0f);
}

// Abre a tampa e risca (variação sorteada). Devolve em quantos segundos a chama
// pega no som — quem chama acende a luz nesse instante. 0 = acender já.
float PlayLighterIgnite() {
    if (gGameplayMuted) return 0.0f;
    EnsureLoaded();
    const int i = PickVariation(gLighterIgniteSounds, kLighterVariants, gLastLighterIgnite);
    if (i < 0) { PlaySound(gLighterOnSound); return 0.0f; }
    gLastLighterIgnite = i;
    gLighterChannel = PlayWithGain(gLighterIgniteSounds[i], 1.0f);
    return gLighterChannel >= 0 ? kLighterFlameAt[i] : 0.0f;
}

// Corta um "acender" que ainda está tocando (apertou [F] de novo, trocou de item…).
void CancelLighterIgnite() {
    if (gLighterChannel < 0 || !Mix_Playing(gLighterChannel)) return;
    for (int i = 0; i < kLighterVariants; ++i) {
        if (Mix_GetChunk(gLighterChannel) == gLighterIgniteSounds[i].GetChunk()) {
            Mix_HaltChannel(gLighterChannel);
            break;
        }
    }
    gLighterChannel = -1;
}

// Recarga (2 s): fluido escorrendo no isqueiro; na lamparina, o galão despejando.
void PlayReloadPour(bool lampTarget) {
    PlayWithGain(lampTarget && gLampReloadSound.IsOpen() ? gLampReloadSound : gReloadPourSound, 1.0f);
}

// Lamparina acendendo (abrir) ou apagando (fechar).
void PlayLampToggle(bool turningOn) {
    if (gGameplayMuted) return;
    EnsureLoaded();
    Sound* pool = turningOn ? gLampOnSounds : gLampOffSounds;
    int& last = turningOn ? gLastLampOn : gLastLampOff;
    const int i = PickVariation(pool, kLampToggleCount, last);
    if (i < 0) return;
    last = i;
    PlayWithGain(pool[i], 1.0f);
}

// Coletar: o som depende do item (combustível, lamparina, tábua); o resto, genérico.
void PlayItemPickup(const std::string& itemName) {
    if (gGameplayMuted) return;
    EnsureLoaded();
    Sound* pool = gPickupGenericSounds;
    int count = kPickupGenericCount;
    int* last = &gLastPickupGeneric;
    if (itemName == "Fuel")                  { pool = gPickupFuelSounds;  count = kPickupFuelCount;  last = &gLastPickupFuel; }
    else if (itemName == "Lamp")             { pool = gPickupLampSounds;  count = kPickupLampCount;  last = &gLastPickupLamp; }
    else if (itemName == "Tabua de Madeira") { pool = gPickupPlankSounds; count = kPickupPlankCount; last = &gLastPickupPlank; }
    int i = PickVariation(pool, count, *last);
    if (i < 0 && pool != gPickupGenericSounds) {          // sem o específico: cai no genérico
        pool = gPickupGenericSounds; count = kPickupGenericCount; last = &gLastPickupGeneric;
        i = PickVariation(pool, count, *last);
    }
    if (i < 0) return;
    *last = i;
    PlayWithGain(pool[i], 1.0f);
}

// Uma pisada na superfície (variação sorteada, sem repetir a anterior). Pisadas
// podem se sobrepor: a cauda de uma não corta a próxima.
void PlayFootstep(FootstepSurface surface) {
    if (gGameplayMuted) return;
    EnsureLoaded();
    const int sfc = static_cast<int>(surface);
    const int i = PickVariation(gStepSounds[sfc], kStepVariants, gLastStep[sfc]);
    if (i < 0) return;
    gLastStep[sfc] = i;
    PlayWithGain(gStepSounds[sfc][i], kFootstepVolumePercent / 100.0f);
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

// ── Versoes POSICIONADAS ────────────────────────────────────────────────────
// O `SetChannelSpatial` ja existia e so o monstro o usava. Uma janela a abrir
// do outro lado do andar soava exactamente como uma ao lado; agora vem do lado
// certo e mais baixa com a distancia.
void PlayWindowToggle(bool opening, float srcX, float srcY, float listenerX, float listenerY) {
    if (gGameplayMuted) return;
    Sound& s = opening ? gWindowOpenSound : gWindowCloseSound;
    PlaySound(s);
    const int ch = s.GetChannel();
    if (ch >= 0) {
        SetChannelSpatial(ch, srcX, srcY, listenerX, listenerY, kWindowAudibleDist);
    }
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
    StopHudFireLoop();        // não zera o nível: ao despausar o crepitar volta sem repetir o "fwoosh"
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

void PlayWindowBreak(float srcX, float srcY, float listenerX, float listenerY) {
    if (gGameplayMuted) return;
    PlayWindowBreak();
    const int ch = gWindowBreakSound.GetChannel();
    if (ch >= 0) {
        SetChannelSpatial(ch, srcX, srcY, listenerX, listenerY, kWindowAudibleDist);
    }
}

void PlayWindowBreak() {
    if (gGameplayMuted) return;
    EnsureLoaded();
    if (!Mix_Playing(kChannelWindowBreak) && gWindowBreakSound.IsOpen()) {
        gWindowBreakSound.Play();
        ApplySfxVolume(gWindowBreakSound);
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
    gHudFireLastLevel = 0.0f;   // próximo destaque da chama começa com o "fwoosh"
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

void PlayBagOpen()   { PlayWithGain(gBagOpenSound, kBagZipGain); }
void PlayBagClose()  { PlayWithGain(gBagCloseSound, kBagZipGain); }
void PlayItemCycle() { PlayVariation(gItemCycleSounds, kItemCycleCount, gLastItemCycle, gItemCycleChannel, kItemCycleGain); }
void PlayPaperFlip() { PlayVariation(gPaperSounds, kPaperCount, gLastPaper, gPaperChannel, kPaperGain); }

// Som da aura da chama na HUD. level01 = força do destaque (fade da aura × brilho
// da chama): 0 desliga. Saindo do zero toca o "fwoosh"; enquanto > 0 o crepitar
// fica em loop com o volume seguindo o nível. Chamar todo frame.
void UpdateHudFireAura(float level01) {
    const float level = std::max(0.0f, std::min(1.0f, level01));
    if (gGameplayMuted || level <= kHudFireStartAt) {
        StopHudFireLoop();
        gHudFireLastLevel = gGameplayMuted ? 0.0f : level;
        return;
    }
    EnsureLoaded();
    if (gHudFireLastLevel <= kHudFireStartAt && gHudFireStartSound.IsOpen()) {
        if (Mix_PlayChannel(kChannelHudFireStart, gHudFireStartSound.GetChunk(), 0) >= 0) {
            Mix_Volume(kChannelHudFireStart, static_cast<int>(SfxChannelVolume() * kHudFireStartGain));
        }
    }
    gHudFireLastLevel = level;
    if (!gHudFireLoopSound.IsOpen()) return;
    if (!gHudFireLoopActive) {
        gHudFireLoopActive = gHudFireLoopSound.PlayLoopedOnChannel(kChannelHudFireLoop) >= 0;
    }
    Mix_Volume(kChannelHudFireLoop, static_cast<int>(SfxChannelVolume() * kHudFireLoopGain * level));
}

void PlayCandleLightUp() { PlaySound(gCandleLightUpSound); }
void PlayCandleBlow()    { PlaySound(gCandleBlowPlayerSound); }
void PlayRepair() { PlaySound(gRepairSound); }
void PlayClosetOpen()  { PlaySound(gClosetOpenSound); }
void PlayClosetClose() { PlaySound(gClosetCloseSound); }


} // namespace GameSfx


