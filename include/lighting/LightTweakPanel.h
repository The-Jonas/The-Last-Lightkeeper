#ifndef LIGHT_TWEAK_PANEL_H
#define LIGHT_TWEAK_PANEL_H

#define INCLUDE_SDL
#include "SDL_include.h"

#include "lighting/LightMaskTypes.h"

#include <vector>

class InputManager;

class LightTweakPanel {
public:
    LightTweakPanel(LightMaskParams& params, LightMaskShape& shape, PlayerVisionParams* vision = nullptr);
    ~LightTweakPanel();

    LightTweakPanel(const LightTweakPanel&) = delete;
    LightTweakPanel& operator=(const LightTweakPanel&) = delete;

    void Update(InputManager& input, float dt, int windowW, int windowH);
    void Render(SDL_Renderer* renderer, int windowW, int windowH);
    bool ConsumeCreateLightRequest();

    bool visible = false;
    bool durabilityEnabled = true;
    /// 0 = luz/sombra (paginas antigas); 1 = campo de visao + filtro preto-e-branco.
    int page = 0;
    static constexpr int kPageCount = 2;

    static constexpr int kLogicalRows = 56;

private:
    static constexpr int kPanelWMax = 248;
    static constexpr int kMarginX = 10;
    static constexpr int kRowH = 40;
    static constexpr int kBarH = 9;
    static constexpr int kFirstRowY = 44;
    static constexpr int kPadX = 8;

    LightMaskParams& params;
    LightMaskShape& shape;
    PlayerVisionParams* vision = nullptr;
    int lastPage = 0;
    /// Altura da janela do ultimo frame — as linhas encolhem para caberem todas.
    int lastWinH = 1080;
    LightMaskShape lastShape = LightMaskShape::Circle;
    int focusedSlot = 0;
    int dragSlot = -1;
    bool createLightRequested = false;

    std::vector<int> activeRows;

    SDL_Texture* rowLabelTex[kLogicalRows]{};
    int rowLabelW[kLogicalRows]{};
    int rowLabelH[kLogicalRows]{};
    char rowLabelBuf[kLogicalRows][128]{};

    void refreshActiveRows();
    /// Altura de linha efectiva: `kRowH` quando cabe, menos quando ha linhas a
    /// mais para a altura da janela.
    int rowHeight() const;
    int barOffsetY() const;
    /// Linha desenhada como botao (acao imediata) em vez de barra.
    static bool isButtonRow(int logicalRow);
    /// Linha desenhada como caixa de seleccao (liga/desliga).
    static bool isToggleRow(int logicalRow);
    bool toggleRowValue(int logicalRow) const;
    void flipToggleRow(int logicalRow);
    int logicalRowAtSlot(int slotIndex) const;
    int slotCount() const;

    void rebuildRowLabel(SDL_Renderer* renderer, int logicalRow);
    void setRowFromNormalized(int logicalRow, float n01);
    float getRowNormalized(int logicalRow) const;
    const char* shapeName() const;
    void cycleShape();
    bool barHit(int mx, int my, int winW, int panelLeft, int panelW, int slotIndex, float* outN01) const;
    bool rowButtonHit(int mx, int my, int panelLeft, int panelW, int slotIndex) const;
    void layoutPanel(int winW, int& outLeft, int& outPanelW) const;
};

#endif
