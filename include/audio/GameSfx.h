#ifndef GAME_SFX_H
#define GAME_SFX_H 

enum class FootstepSurface { Stone, Wood, Stairs };

namespace GameSfx {

void NotifyLoadingBegin();
void NotifyLoadingEnd();

void PlayCandleLightUp();   // som de acender (fósforo/isqueiro na vela)
void PlayCandleBlow();      // som de soprar/apagar pelo jogador

/// Para imediatamente TODOS os loops de áudio de gameplay (passos, vela, caixa e
/// vento). Usado na morte/fim de jogo para que nenhum loop continue tocando
/// depois que o StageState para de atualizar.
void StopAllGameplay();

/// Como StopAllGameplay, mas SEM parar o vento (o loop de vento é disparado por
/// evento de janela e não seria re-armado sozinho). Usado ao PAUSAR: silencia os
/// loops que o mundo pausado deixaria tocando (passos, vela, caixa, batimento).
void StopAllGameplayAudio();

/// Parada TOTAL de efeitos (todos os canais do mixer, inclusive rádio/ondas/vento/
/// monstro). Usada nas transições nível↔menu para nenhum som sobreviver.
void HardStopAll();

// Funções para fazer o áudio ficar direcional
void SetChannelSpatial(int channel, float srcX, float srcY, float listX, float listY, float maxDist = 800.0f);
void ClearChannelSpatial(int channel);
// Passos por-frame da animação (um som por frame). Chamar quando o frame muda.
void PlayMonsterStep(int frameIndex, float monsterX, float monsterY, float playerX, float playerY, float maxDist);
// Agora só cuida dos rangidos de madeira esporádicos (passos = PlayMonsterStep).
void UpdateMonsterFootsteps(float dt, float moveSpeed, float monsterX, float monsterY, float playerX, float playerY, bool fleeing = false);

void NotifyBoxSlide();
void MaintainBoxPushLoop();
void PauseBoxPushLoop();     // pausa o loop de arrasto (parou de mover) — sem o "thud" de soltar
void NotifyBoxPushEnd();
void PlayLighterToggle(bool turningOn);
void UpdateBigBrotherFootsteps(float dt, float moveSpeed, bool isBigBrother, FootstepSurface surface);
void UpdateThunder(float dt);
void UpdateCandleProximity(bool playerNearCandle);

/// Dispara trovão imediato (som aleatório + clarão). Usado pela tecla T e pelo timer ambiental.
void TriggerThunderStrike();

/// Grande trovão: toca trovao_1 + trovao_2 JUNTOS no volume máximo. Usado no
/// clímax do último andar (fuga para a luz).
void PlayBigThunder();

/// 0..1 — força do clarão de trovão (decai ao longo do tempo).
float GetThunderFlashStrength();

// Som de reparar
void PlayRepair();

// Armario
void PlayClosetOpen();
void PlayClosetClose();

// Monstro
void PlayMonsterScream();          // grito do monstro
void PlayMonsterSpot();            // som de quando vê os irmãos
void StopMonsterFootsteps();

/// Batimento cardíaco: intensity01 0→1 (0 = silêncio, 1 = pânico). Volume do loop
/// acompanha a sanidade baixa. Chamar todo frame durante o gameplay.
void UpdateHeartbeat(float intensity01);

// FUNÇÕES DA JANELA E DO VENTO
void PlayWindowToggle(bool opening);
void StartWindLoop();
void StopWindLoop();
void PlayCandleBlowOut();

/// Volume atual do barramento de VFX (master × efeitos), 0..MIX_MAX_VOLUME.
/// Usado por sons de efeito tocados fora do GameSfx (ex.: pegar item).
int CurrentSfxVolume();

} // namespace GameSfx

#endif
