#ifndef TITLESTATE_H
#define TITLESTATE_H

#include "core/State.h"
#include "core/Timer.h"
#include "audio/Music.h"
#include "core/Game.h"
#include "core/InputManager.h"
#include <array>

class GameObject;

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
    // ── Sliders de volume ─────────────────────────────────────────────────────
    enum class VolumeSliderKind { Master, Ambient, Vfx, Voice, Count };
    struct VolumeSliderUi {
        VolumeSliderKind kind;
        const char* label;
        int  value    = 0;
        bool dragging = false;
        SDL_Rect bar{0,0,0,0};
        SDL_Rect handle{0,0,0,0};
    };
    VolumeSliderUi sliders[static_cast<int>(VolumeSliderKind::Count)];
    static constexpr int kSliderW    = 400;
    static constexpr int kSliderH    = 14;
    static constexpr int kHandleW    = 22;
    static constexpr int kSliderRowH = 60;
    void InitSliders();
    void RecalcSliders(int panelX, int panelY, int panelW);
    void HandleSliderInput(int mx, int my, InputManager& input);
    void ApplySliderValue(VolumeSliderKind kind, int percent);
    void RenderSliders(SDL_Renderer* r, int panelX, int panelY);
    VolumeSliderUi* FindSliderAtPoint(int mx, int my);

    // ── Estado do menu ────────────────────────────────────────────────────────
    Music music;
    Timer fadeTimer;
    float fadeAlpha  = 0.0f;
    float pulseTimer = 0.0f;
    static constexpr float kFadeDuration = 3.0f;

    bool hasContinueSave = false;
    int  menuSelection   = 0;
    // 0=Novo Jogo  1=Continuar  2=Configuracoes  3=Creditos  4=Sair

    void StartNewGame();
    void StartContinue();
    void ActivateMenuSelection();

    // ── Assets no objectArray ─────────────────────────────────────────────────
    GameObject* bg         = nullptr;
    GameObject* logoGO     = nullptr;
    GameObject* charBodyGO = nullptr;

    // ── Assets fora do objectArray ────────────────────────────────────────────
    GameObject* armGO        = nullptr;
    GameObject* menuTexts[5] = {};  // 5 opcoes

    // ── Animacao do personagem ────────────────────────────────────────────────
    static constexpr int   kCharFrames       = 5;
    static constexpr float kCharFrameSeconds = 0.18f;
    float charAnimTimer  = 0.0f;
    int   charFrameIndex = 0;

    // ── Versao da arte do menu ────────────────────────────────────────────────
    // 1 = V2 (Luana) — versão usada. (V1 mantido nas paths caso se queira voltar.)
    int  menuVersion = 1;
    const char* ArmPath() const;
    void BodyFramePath(int frame1based, char* out, size_t n) const;
    void PreloadMenuVersion(int version);

    // ── Braco / lanterna ─────────────────────────────────────────────────────
    float armAngle       = 0.0f;
    float armAngleTarget = 0.0f;
    float shoulderX      = 0.0f;
    float shoulderY      = 0.0f;

    struct OptionPos { float cx = 0; float cy = 0; };
    std::array<OptionPos, 5> optionPositions;

    // ── Painel de configuracoes ───────────────────────────────────────────────
    bool configOpen         = false;
    int  configTab          = 0;   // 0=Volume  1=Video  2=Controles
    bool awaitingRebind     = false;
    GameAction rebindAction = GameAction::MoveUp;
    int  controlsSelection  = 0;
    static constexpr int kControlsRowCount = InputManager::ActionCount + 2;
    SDL_Rect controlsRowRects[InputManager::ActionCount + 2]{};
    SDL_Rect configCloseBtn{};
    SDL_Rect configTabVolume{};
    SDL_Rect configTabVideo{};
    SDL_Rect configTabControles{};

    // Aba "Video": 0=Modo de tela  1=Resolucao  2=Voltar (aplicam ao reiniciar).
    int  videoSelection = 0;
    static constexpr int kVideoRowCount = 3;
    SDL_Rect videoRowRects[kVideoRowCount]{};

    void OpenConfig();
    void UpdateConfig(InputManager& input);
    void RenderConfig(SDL_Renderer* r);
    void RenderConfigVolume(SDL_Renderer* r, int px, int py, int pw, int ph);
    void RenderConfigVideo(SDL_Renderer* r, int px, int py, int pw, int ph);
    void RenderConfigControles(SDL_Renderer* r, int px, int py, int pw, int ph);

    // ── Helpers de layout ────────────────────────────────────────────────────
    void LoadConfig();
    void LayoutAll();
    void UpdateCharAnim(float dt);
    void UpdateArm(float dt);
    void RenderLanternCone(SDL_Renderer* r);
    void RenderArm(SDL_Renderer* r);
    void RenderMenuTexts(SDL_Renderer* r);

    // Desfoque suave do irmãozão (braço + corpo). Reduz em CADEIA (÷2 por passo, cada
    // um faz média 2×2 de verdade — sem serrilhado/"pixelado") e amplia com filtragem
    // linear → foco nas opções do menu, não na animação.
    void RenderBlurredCharacter(SDL_Renderer* r);
    SDL_Texture* charBlurFull       = nullptr;   // braço+corpo em tamanho de tela
    static constexpr int kBlurLevels = 4;        // ÷2, ÷4, ÷8, ÷16
    SDL_Texture* charBlurChain[kBlurLevels] = {}; // pirâmide de redução

    // ── Layout carregado do JSON ──────────────────────────────────────────────
    float kCharW       = 720.0f;
    float kCharH       = 694.0f;
    float kCharCX      = 1200.0f;
    float kCharBottomY = 1080.0f;
    float kShoulderDX  = -235.0f;
    float kShoulderDY  = -390.0f;

    float kLogoW = 860.0f;
    float kLogoH = 860.0f * (809.0f / 2444.0f);
    float kLogoX = 100.0f;
    float kLogoY =  60.0f;

    float kMenuX       = 350.0f;
    float kMenuStartY  = 550.0f;
    float kMenuSpacing =  80.0f;
    int   kMenuFontSize = 46;

    float kArmW         = 420.0f;
    float kArmH         = 261.0f;
    float kLanternAlong = 0.72f;
    float kLanternPerp  = -18.0f;

    Uint8 kDarknessAlpha = 185;
};

#endif