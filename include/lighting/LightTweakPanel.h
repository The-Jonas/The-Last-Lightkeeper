#ifndef LIGHT_TWEAK_PANEL_H
#define LIGHT_TWEAK_PANEL_H

#define INCLUDE_SDL
#include "SDL_include.h"

#include "lighting/LightMaskTypes.h"

#include <vector>

class InputManager;

/// Painel de afinacao (tecla \). Duas paginas: luz/sombra e campo de visao.
///
/// As linhas sao arrumadas em GRUPOS com titulo e distribuidas por VARIAS
/// COLUNAS. Uma coluna unica com trinta e tal barras obrigava a encolher cada
/// linha para 19 px e nao dava para ler nada; com colunas, cada linha fica com
/// altura inteira e o grupo diz de imediato o que a barra faz.
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

    static constexpr int kLogicalRows = 69;

private:
    // ── Medidas ─────────────────────────────────────────────────────────────
    static constexpr int kColW = 268;        ///< largura de uma coluna
    static constexpr int kColGap = 6;
    static constexpr int kRowH = 36;         ///< altura de uma linha com barra
    static constexpr int kHeaderH = 27;      ///< altura de um titulo de grupo
    static constexpr int kBarH = 7;
    static constexpr int kPadX = 9;
    static constexpr int kTitleH = 30;       ///< barra de titulo do painel
    static constexpr int kFootH = 20;        ///< rodape com as teclas
    static constexpr int kMargin = 8;
    static constexpr int kMaxCols = 5;

    /// Uma entrada do painel: ou um titulo de grupo, ou uma linha logica.
    /// O rectangulo e recalculado a cada frame por `layoutEntries`, e e o MESMO
    /// que o rato usa — o desenho e o clique nunca podem discordar.
    struct Entry {
        int row = -1;                 ///< >= 0 numa linha; -1 num titulo
        const char* header = nullptr; ///< != nullptr num titulo
        SDL_Rect box{0, 0, 0, 0};
    };

    LightMaskParams& params;
    LightMaskShape& shape;
    PlayerVisionParams* vision = nullptr;
    int lastPage = 0;
    LightMaskShape lastShape = LightMaskShape::Circle;
    int focusedEntry = 0;
    int dragEntry = -1;
    bool createLightRequested = false;

    // ── Gravacao dos valores ────────────────────────────────────────────────
    // Qualquer mexida marca `dirty`; a gravacao acontece `kSaveDelaySec` depois
    // da ULTIMA mexida. Sem essa espera, arrastar uma barra escrevia o ficheiro
    // a cada frame. Fechar o painel grava logo.
    static constexpr float kSaveDelaySec = 0.8f;
    bool dirty = false;
    float saveTimer = 0.0f;
    void markDirty();
    void flushSave();

    std::vector<Entry> entries;
    /// Rectangulo do painel inteiro neste frame (moldura, fundo, titulo).
    SDL_Rect panelBox{0, 0, 0, 0};
    /// Altura de linha efectiva: `kRowH` quando cabe, menos quando nem com o
    /// numero maximo de colunas os grupos cabem na janela.
    int rowH = kRowH;
    int headerH = kHeaderH;

    // ── Texto ───────────────────────────────────────────────────────────────
    // Cada pedaco de texto guarda a sua textura e o texto de que ela saiu. So
    // se refaz a textura quando o texto muda — sem isto, o painel criava e
    // destruia dezenas de texturas por frame.
    struct CachedText {
        SDL_Texture* tex = nullptr;
        int w = 0;
        int h = 0;
        char text[128]{};
    };
    static void freeText(CachedText& t);
    /// Garante que `t` mostra `s`. Devolve false quando nao ha textura.
    static bool ensureText(SDL_Renderer* renderer, CachedText& t, const char* s, int fontPx, SDL_Color col);
    static void blit(SDL_Renderer* renderer, const CachedText& t, int x, int y, int maxW);

    // Dois textos por linha: o NOME (fixo) e o VALOR (muda). Separa-los deixa
    // alinhar o valor a direita, em vez de o deixar cair fora da coluna no fim
    // de uma etiqueta comprida.
    CachedText rowName[kLogicalRows];
    CachedText rowValue[kLogicalRows];
    CachedText titleText;
    CachedText footText;
    /// Titulos dos grupos, um por entrada de `entries` (vazio nas linhas).
    std::vector<CachedText> headerText;

    void rebuildEntries();
    void layoutEntries(int winW, int winH);
    /// Indice da entrada por baixo do rato, ou -1. Ignora os titulos.
    int entryAtPoint(int mx, int my) const;
    /// Rectangulo da barra arrastavel de uma linha (vazio nas outras).
    SDL_Rect barRectOf(const Entry& e) const;
    /// Move o foco para a linha seguinte/anterior, saltando os titulos.
    void moveFocus(int dir);
    bool entryIsRow(int index) const;

    /// Linha desenhada como botao (acao imediata) em vez de barra.
    static bool isButtonRow(int logicalRow);
    /// Linha desenhada como caixa de seleccao (liga/desliga).
    static bool isToggleRow(int logicalRow);
    bool toggleRowValue(int logicalRow) const;
    void flipToggleRow(int logicalRow);

    void rebuildRowText(SDL_Renderer* renderer, int logicalRow);
    void releaseText();
    void setRowFromNormalized(int logicalRow, float n01);
    float getRowNormalized(int logicalRow) const;
    const char* shapeName() const;
    void cycleShape();
};

#endif
