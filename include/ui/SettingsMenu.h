#ifndef SETTINGS_MENU_H
#define SETTINGS_MENU_H

#include "core/InputManager.h"

#define INCLUDE_SDL
#include "SDL_include.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Menu de configurações ÚNICO — o mesmo no título e na pausa.
//
//  Quem usa só faz:  Open()  →  a cada frame Update(dt) + Render(renderer)
//  enquanto IsOpen(). O menu grava as configurações ao fechar.
//
//  Teclado primeiro: W/S ou ↑↓ navegam, A/D ou ←→ ajustam (segurando repete
//  nos sliders), Enter/Espaço/E selecionam, ESC volta. O mouse é alternativa:
//  hover seleciona, clique ativa, clique nas setas troca a opção, arrastar mexe
//  o slider.
//
//  Visual no padrão do jogo: dourado para o que está ativo, tons neutros para
//  o resto, setas desenhadas (as mesmas da pasta), teclas com a arte do
//  KeyGlyphs e as opções separadas em seções (Áudio / Imagem / Jogo).
// ─────────────────────────────────────────────────────────────────────────────
class SettingsMenu {
public:
    void Open();                                                                    // abre na página principal
    void Close();                                                                   // fecha e grava settings.json
    bool IsOpen() const { return open; }

    void Update(float dt);                                                          // entrada da página atual
    void Render(SDL_Renderer* r);                                                   // desenha a página atual (com escurecimento de fundo)

private:
    enum Row {
        kMaster, kAmbient, kSfx, kVoice,                                            // Áudio
        kBrightness, kCalibrate, kDisplayMode, kResolution, kVSync, kFpsCap,        // Imagem
        kReduceFlash, kControls,                                                    // Jogo
        kBack,
        kRowCount
    };
    enum class Page { Main, Controls };

    static constexpr int kControlsRowCount = InputManager::ActionCount + 2;         // ações + Restaurar + Voltar
    static constexpr float kHoldDelay  = 0.35f;                                     // espera antes de repetir segurando A/D
    static constexpr float kHoldRepeat = 0.06f;                                     // intervalo da repetição

    bool open = false;
    Page page = Page::Main;
    int  selection = 0;
    int  controlsSelection = 0;
    bool awaitingRebind = false;
    GameAction rebindAction = GameAction::MoveUp;
    float rebindInvalidTimer = 0.0f;
    float holdTimer = 0.0f;
    bool dragging = false;

    // Áreas clicáveis, preenchidas no Render do frame anterior.
    SDL_Rect rowRects[kRowCount]{};
    SDL_Rect sliderRects[kRowCount]{};
    SDL_Rect leftArrowRects[kRowCount]{};
    SDL_Rect rightArrowRects[kRowCount]{};
    SDL_Rect controlsRowRects[kControlsRowCount]{};                                             

    void UpdateMain(float dt);
    void UpdateControls(float dt);
    void Adjust(int row, int dir);                                                  // A/D ou setas: muda slider ou opção da linha
    void Activate(int row);                                                         // Enter/clique: botões e opções
    void RenderMain(SDL_Renderer* r);
    void RenderControls(SDL_Renderer* r);

    static bool IsSlider(int row) { return row == kMaster || row == kAmbient || row == kSfx ||
                                           row == kVoice || row == kBrightness; }
    static bool IsOption(int row) { return row == kDisplayMode || row == kResolution || row == kVSync ||
                                           row == kFpsCap || row == kReduceFlash; }
    static void SliderRange(int row, int& lo, int& hi, int& step);
    static int  SliderValue(int row);
    static void SetSliderValue(int row, int v);
};

#endif