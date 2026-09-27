#ifndef TITLESTATE_H
#define TITLESTATE_H

#include "core/State.h"
#include "core/Timer.h"
#include "audio/Music.h"
#include "ui/SettingsMenu.h"

#define INCLUDE_SDL
#include "SDL_include.h"

#include <array>
#include <cstddef>

class GameObject;

// ─────────────────────────────────────────────────────────────────────────────
//  Tela de título: parede + logo, o irmãozão de costas (desfocado) apontando a
//  lanterna para a opção selecionada, e o menu de configurações compartilhado.
// ─────────────────────────────────────────────────────────────────────────────
class TitleState : public State {
public:
    TitleState();
    ~TitleState();

    void LoadAssets() override;
    void Update(float dt) override;
    void Render() override;
    void Start() override;
    void Pause() override;
    void Resume() override;

private:
    enum MenuOption { kContinue, kNewGame, kSettings, kCredits, kQuit, kOptionCount };

    // Posições e tamanhos em tela 1920x1080. Os valores abaixo são o padrão;
    // Recursos/img/menu/menu_config.json sobrescreve o que tiver.
    struct Layout {
        float charCX       = 1200.0f;                                       // centro X do irmãozão
        float charBottomY  = 1080.0f;                                       // pé do irmãozão
        float charW        = 720.0f;                                        // largura do sprite do corpo
        float charH        = 694.0f;                                        // altura do sprite do corpo
        float shoulderDX   = -235.0f;                                       // ombro em relação ao centro X
        float shoulderDY   = -390.0f;                                       // ombro em relação ao pé

        float logoX        = 100.0f;
        float logoY        = 60.0f;
        float logoW        = 860.0f;
        float logoH        = 860.0f * (809.0f / 2444.0f);                   // proporção da arte do logo

        float menuX        = 350.0f;                                        // X das opções
        float menuStartY   = 550.0f;                                        // Y da primeira opção visível
        float menuSpacing  = 80.0f;                                         // distância entre opções
        int   menuFontSize = 46;

        float armW         = 420.0f;                                        // tamanho do braço (pivô no ombro)
        float armH         = 261.0f;
        float lanternAlong = 0.72f;                                         // posição da lente ao longo do braço (0..1)
        float lanternPerp  = -18.0f;                                        // deslocamento da lente perpendicular ao braço

        Uint8 darknessAlpha = 185;                                          // película escura sobre parede e logo
    };

    struct OptionPos { float cx = 0.0f; float cy = 0.0f; };                 // centro de uma opção (tela)

    static constexpr int   kCharFrames       = 5;                           // quadros da respiração do irmãozão
    static constexpr float kCharFrameSeconds = 0.18f;                       // duração de cada quadro
    static constexpr float kFadeDuration     = 3.0f;                        // fade-in das opções
    static constexpr float kArmAngleOffset   = -10.0f;                      // correção da mira (+ desce, − sobe)
    static constexpr float kArmTurnSpeed     = 7.0f;                        // suavização do giro do braço
    static constexpr int   kBlurLevels       = 4;                           // pirâmide ÷2, ÷4, ÷8, ÷16
    static constexpr bool  kUseLuanaArt      = true;                        // true = arte V2 (Luana), false = V1

    // ── Estado ───────────────────────────────────────────────────────────────
    SettingsMenu settingsMenu;
    Music  music;
    Layout layout;

    Timer fadeTimer;
    float fadeAlpha  = 0.0f;                                                // 0..255, cresce durante kFadeDuration
    float pulseTimer = 0.0f;                                                // brilho pulsante da opção selecionada

    bool hasContinueSave = false;                                           // sem save, "Continuar" some e é pulado
    int  menuSelection   = kNewGame;

    GameObject* bg         = nullptr;                                       // no objectArray
    GameObject* logoGO     = nullptr;                                       // no objectArray
    GameObject* charBodyGO = nullptr;                                       // no objectArray (desenhado à mão, com desfoque)
    GameObject* menuTexts[kOptionCount] = {};                               // fora do objectArray (dono: este estado)
    std::array<OptionPos, kOptionCount> optionPositions;

    float charAnimTimer  = 0.0f;
    int   charFrameIndex = 0;

    float armAngle  = 0.0f;                                                 // graus, rotação atual do braço
    float shoulderX = 0.0f;                                                 // pivô do braço (tela)
    float shoulderY = 0.0f;

    SDL_Texture* charBlurFull = nullptr;                                    // braço+corpo em tamanho de tela
    SDL_Texture* charBlurChain[kBlurLevels] = {};                           // níveis reduzidos do desfoque

    // ── Preparação ───────────────────────────────────────────────────────────
    void LoadLayout();                                                      // lê menu_config.json para `layout`
    void PreloadArt();                                                      // carrega a arte com filtragem linear
    void CreateObjects();                                                   // parede, logo, corpo e textos do menu

    // ── Menu ─────────────────────────────────────────────────────────────────
    bool IsOptionEnabled(int option) const;                                 // "Continuar" só com save
    void MoveSelection(int dir);                                            // anda ±1 pulando opções desativadas
    void ActivateSelection();                                               // executa a opção selecionada
    void StartNewGame();
    void StartContinue();

    // ── Layout e animação ────────────────────────────────────────────────────
    void  LayoutAll();                                                      // posiciona objetos, ombro e opções
    float ArmAngleTo(int option) const;                                     // ângulo para o braço mirar numa opção
    void  UpdateArm(float dt);                                              // gira o braço suavemente até a opção
    void  UpdateCharAnim(float dt);                                         // troca o quadro do corpo
    void  UpdateMenuColors();                                               // cinza / dourado pulsante / desativado

    // ── Desenho ──────────────────────────────────────────────────────────────
    void RenderDarkness(SDL_Renderer* r);                                   // película escura
    void RenderLanternCone(SDL_Renderer* r);                                // feixe + círculo na opção
    void RenderMenuTexts();                                                 // opções visíveis
    void RenderArm(SDL_Renderer* r);                                        // braço girado no ombro
    void RenderCharacter(SDL_Renderer* r);                                  // braço + corpo sem desfoque
    void RenderBlurredCharacter(SDL_Renderer* r);                           // braço + corpo desfocados
    bool EnsureBlurTargets(SDL_Renderer* r, int w, int h);                  // cria os alvos do desfoque
    void DestroyBlurTargets();

    // ── Caminhos da arte (V1 / V2 têm a mesma proporção) ─────────────────────
    static const char* ArmPath();
    static void BodyFramePath(int frame1based, char* out, std::size_t n);
};

#endif