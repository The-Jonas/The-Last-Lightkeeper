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

// Funções para fazer o áudio ficar direcional
void SetChannelSpatial(int channel, float srcX, float srcY, float listX, float listY);
void ClearChannelSpatial(int channel);
void UpdateMonsterFootsteps(float dt, float moveSpeed, float monsterX, float monsterY, float playerX, float playerY);

void NotifyBoxSlide();
void MaintainBoxPushLoop();
void NotifyBoxPushEnd();
void PlayLighterToggle(bool turningOn);
void UpdateBigBrotherFootsteps(float dt, float moveSpeed, bool isBigBrother, FootstepSurface surface);
void UpdateThunder(float dt);
void UpdateCandleProximity(bool playerNearCandle);

/// Dispara trovão imediato (som aleatório + clarão). Usado pela tecla T e pelo timer ambiental.
void TriggerThunderStrike();

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
