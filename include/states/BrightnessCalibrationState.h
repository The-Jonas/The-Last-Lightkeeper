#ifndef BRIGHTNESS_CALIBRATION_STATE_H
#define BRIGHTNESS_CALIBRATION_STATE_H

#include "core/State.h"

#include <string>


// ─────────────────────────────────────────────────────────────────────────────
//  Tela de calibração de brilho.
//
//  Três silhuetas do isqueiro em cinzas conhecidos sobre o "escuro do jogo".
//  O jogador ajusta o brilho até a da esquerda ficar QUASE invisível. Os
//  cinzas passam pela MESMA gama da cena (Game::BrightnessGamma), então o
//  que ele calibra aqui é exatamente o que vê jogando.
//
//  firstLaunch = true  → aberta no 1º arranque; ao confirmar empilha o título.
//  firstLaunch = false → aberta pelas Configurações; ESC desfaz e volta.
//
//  Valores ajustáveis em config/calibration.json (opcional):
//    { "background": 12, "symbols": [20, 32, 56] }
// ─────────────────────────────────────────────────────────────────────────────
class BrightnessCalibrationState : public State {
public:
    explicit BrightnessCalibrationState(bool firstLaunch);

    void LoadAssets() override;
    void Update(float dt) override;
    void Render() override;

    void Start() override;
    void Pause() override;
    void Resume() override;

private:
    void LoadTuning();                                      // lê config/calibration.json (se existir)
    void Finish(bool keep);                                 // confirma (keep) ou desfaz e sai
    void Step(int delta);                                   // muda o brilho em `delta` pontos
    static int ApplyGamma(int gray);                        // cinza 0..255 → cinza com a gama atual

    bool firstLaunch = true;
    int  startBrightness = 100;                             // para desfazer com ESC (aberta pelas Configurações)
    float holdTimer = 0.0f;                                 // repetição ao segurar A/D

    int backgroundGray = 0;                                 // o escuro real do jogo (madeira fora do cone)
    int symbolGray[3] = {20, 32, 56};                       // quase invisível / visível / nítido

    static constexpr int   kStep = 2;                       // pontos de brilho por toque
    static constexpr float kHoldDelay = 0.35f;              // espera antes de repetir segurando
    static constexpr float kHoldRepeat = 0.06f;             // intervalo da repetição
};

#endif              