#include "lighting/LightTweakPanel.h"
#include "lighting/LightTweakStore.h"
#include "core/InputManager.h"
#include "core/Resources.h"
#include "core/Game.h"

#define INCLUDE_SDL_TTF
#include "SDL_include.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {

const char* kRowLabels[LightTweakPanel::kLogicalRows] = {
    "Escuro max (overlay)",
    "Raio / tamanho (px)",
    "Curva gamma (Power)",
    "Curva: potencia (pow)",
    "Claridade centro (lift)",
    "Anelos (qualidade)",
    "Segmentos",
    "Elipse: aspeto",
    "Cone: meio-angulo",
    "Cone: comprimento",
    "Cone: eixo (graus)",
    "Cone: seguir rato",
    "Rect: meia-largura",
    "Rect: meia-altura",
    "Rect: banda suave",
    "Sombra: limiar dist",
    "Sombra: comprimento max",
    "Sombra: escala por luz",
    "Sombra: suavidade",
    "Sombra: camadas soft",
    "Luz: suavizacao temporal",
    "Luz: grid passo px",
    "Tocha: velocidade anim",
    "Tocha: alcance movimento",
    "Tocha: distorcao borda",
    "Tocha: forca pulso",
    "Tocha: calor da cor",
    "Tocha: intensidade cor",
    "Sombra sprite: escala min",
    "Sombra sprite: escala max",
    "Criar luz em C / clique",
    "Durabilidade (itens)",
    "Campo de visao: ligado",
    "Cone: abertura (meio-ang.)",
    "Cone: borda lateral (graus)",
    "Cone: alcance (px mundo)",
    "Cone: borda da ponta (px)",
    "Cone: apice a frente (px)",
    "Cone: velocidade de giro",
    "Pes: raio da luz (px mundo)",
    "Pes: dureza da borda",
    "PB: forca do cinzento",
    "PB: levantar preto",
    "PB: forca do tom frio",
    "Visao: realce (brilho)",
    "Escuro ambiente (sem luz)",
    "PB: brilho da camada",
    "Luz: devolve a cor",
    "Desfoque fora da visao (px)",
    "Vinheta: forca",
    "Vinheta: inicio",
    "Itens so no campo de visao",
    "Itens: limiar de revelar",
    "Interagiveis so na visao",
    "Barris so na visao",
    "Cone sem luz: escuridao",
    "Ver so onde ha luz",
    "Luz: alcance que revela",
    "Luz: dureza do alcance",
    "Sem luz: ve ate (px mundo)",
    "Cone: dureza da borda",
    "PB: realce das luzes",
    "PB: halo das luzes",
    "Tocha: brilho da chama",
    "Tocha: raio da chama",
    "Luz: tambem foca (nitidez)",
    "Camera: afastar (zoom)",
    "Visao: realce (saturacao)",
    "Monstro so onde ha luz",
};

// Intervalo da barra do zoom-base. O minimo casa com o limite da propria
// `Camera` (abaixo disto os irmaos ficam pequenos demais para se lerem).
constexpr float kCameraZoomMin = 0.50f;
constexpr float kCameraZoomMax = 1.00f;

// ── GRUPOS ──────────────────────────────────────────────────────────────────
// Cada grupo e um titulo mais as linhas que lhe pertencem. E daqui que sai a
// ordem do painel. Uma linha que nao esteja em nenhum grupo NAO se perde: cai
// no grupo "Outros" no fim (ver `rebuildEntries`), que serve de aviso de que
// falta arruma-la.
struct RowGroup {
    const char* title;
    std::vector<int> rows;
};

/// Pagina 0 — luz e sombra. Os grupos listam TODAS as linhas possiveis; cada
/// forma de luz usa so as suas, por isso o filtro em `rebuildEntries` corta o
/// que nao interessa e deita fora os grupos que ficam vazios.
const std::vector<RowGroup>& lightGroups() {
    static const std::vector<RowGroup> g = {
        {"Forma da luz", {0, 1, 2, 3, 4, 7, 8, 9, 10, 11, 12, 13, 14}},
        {"Tocha", {22, 23, 24, 25, 26, 27, 63, 64}},
        {"Sombras", {15, 16, 17, 18, 19, 28, 29}},
        {"Qualidade", {5, 6, 20, 21}},
        {"Accoes", {30, 31}},
    };
    return g;
}

/// Que linhas fazem sentido em cada forma de luz. Mesma lista de sempre; so a
/// ORDEM passou a vir dos grupos.
void rowsForShape(LightMaskShape s, std::vector<int>& out) {
    switch (s) {
    case LightMaskShape::Circle:
        out = {0, 1, 2, 3, 4, 5, 6, 16, 17, 28, 29, 18, 19, 20, 21, 30, 31};
        break;
    case LightMaskShape::Ellipse:
        out = {0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 28, 29, 18, 19, 20, 21, 30, 31};
        break;
    case LightMaskShape::Cone:
        out = {0, 2, 3, 4, 5, 6, 8, 9, 10, 11, 16, 17, 28, 29, 18, 19, 20, 21, 30, 31};
        break;
    case LightMaskShape::SoftRect:
        out = {0, 2, 3, 4, 5, 6, 12, 13, 14, 16, 17, 28, 29, 18, 19, 20, 21, 30, 31};
        break;
    case LightMaskShape::Torch:
        out = {0, 1, 2, 3, 4, 5, 6, 16, 17, 28, 29, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 63, 64, 30, 31};
        break;
    default:
        out = {0, 1, 2, 3, 4, 5, 6};
        break;
    }
}

/// Pagina 1 — campo de visao. A ordem dos grupos segue a ordem por que se
/// afina: primeiro a camara e a forma do cone, depois o que marca a visao
/// (foco e realce), depois a luz, e so no fim o preto-e-branco, que hoje vem
/// desligado.
const std::vector<RowGroup>& visionGroups() {
    static const std::vector<RowGroup> g = {
        {"Camara", {66}},
        {"Campo de visao", {32, 33, 60, 34, 35, 36, 37, 38}},
        {"Pes (visao periferica)", {39, 40}},
        {"Marca da visao: foco e realce", {48, 44, 67, 65}},
        {"Luz para ver", {56, 57, 58, 59, 55, 47}},
        {"Escuridao", {45, 0}},
        {"Objectos fora da visao", {51, 53, 54, 68, 52}},
        {"Chama", {63, 64}},
        {"Preto-e-branco (opcional)", {41, 46, 61, 62, 42, 43, 49, 50}},
    };
    return g;
}

} // namespace

LightTweakPanel::LightTweakPanel(LightMaskParams& paramsIn, LightMaskShape& shapeIn, PlayerVisionParams* visionIn)
    : params(paramsIn), shape(shapeIn), vision(visionIn), lastShape(shapeIn) {
    rebuildEntries();
}

LightTweakPanel::~LightTweakPanel() {
    flushSave();   // sair do jogo com uma mexida por gravar nao a perde
    releaseText();
}

void LightTweakPanel::markDirty() {
    dirty = true;
    saveTimer = 0.0f;   // conta a partir da ULTIMA mexida, nao da primeira
}

void LightTweakPanel::flushSave() {
    if (!dirty) {
        return;
    }
    dirty = false;
    saveTimer = 0.0f;
    LightTweakStore::Save(params, shape, vision ? *vision : PlayerVisionParams{}, durabilityEnabled);
}

bool LightTweakPanel::isButtonRow(int logicalRow) {
    return logicalRow == 30;
}

// TODAS as linhas de liga/desliga. Passar uma linha booleana por aqui e o que
// lhe da caixa de seleccao, clique no meio da linha e as teclas +/-. As linhas
// 3 e 11 estavam de fora e apareciam como BARRA, que e uma forma estranha de
// mostrar um sim/nao.
bool LightTweakPanel::isToggleRow(int logicalRow) {
    switch (logicalRow) {
    case 3:    // curva da luz: smoothstep ou potencia
    case 11:   // cone segue o rato
    case 31:   // durabilidade dos itens
    case 32:   // campo de visao ligado
    case 51:   // itens so no campo de visao
    case 53:   // interagiveis so no campo de visao
    case 54:   // barris so no campo de visao
    case 56:   // ver so onde ha luz
    case 68:   // monstro so onde ha luz
        return true;
    default:
        return false;
    }
}

bool LightTweakPanel::toggleRowValue(int logicalRow) const {
    switch (logicalRow) {
    case 3:
        return params.falloffCurve == LightFalloffCurve::Power;
    case 11:
        return params.coneFollowMouse;
    case 31:
        return durabilityEnabled;
    case 32:
        return vision != nullptr && vision->enabled;
    case 51:
        return vision != nullptr && vision->hideItemsOutsideVision;
    case 53:
        return vision != nullptr && vision->hideInteractablesOutsideVision;
    case 54:
        return vision != nullptr && vision->hidePushablesOutsideVision;
    case 56:
        return vision != nullptr && vision->requireLightToSee;
    case 68:
        return vision != nullptr && vision->hideMonsterOutsideLight;
    default:
        return false;
    }
}

void LightTweakPanel::flipToggleRow(int logicalRow) {
    // Passa pelo mesmo caminho de qualquer outra mexida: le o valor, inverte-o
    // e MANDA-O. Assim ha uma unica funcao a escrever cada campo, e um clique
    // nunca pode cancelar-se contra uma segunda inversao.
    if (!isToggleRow(logicalRow)) {
        return;
    }
    if (logicalRow != 3 && logicalRow != 11 && logicalRow != 31 && vision == nullptr) {
        return;   // pagina do campo de visao sem parametros: nao ha o que mexer
    }
    setRowFromNormalized(logicalRow, toggleRowValue(logicalRow) ? 0.0f : 1.0f);
}

bool LightTweakPanel::entryIsRow(int index) const {
    return index >= 0 && index < static_cast<int>(entries.size()) && entries[static_cast<size_t>(index)].row >= 0;
}

void LightTweakPanel::rebuildEntries() {
    entries.clear();

    const std::vector<RowGroup>* groups = nullptr;
    std::vector<int> allowed;
    if (page == 1 && vision != nullptr) {
        groups = &visionGroups();
    } else {
        groups = &lightGroups();
        rowsForShape(shape, allowed);
    }

    auto isAllowed = [&](int r) {
        if (allowed.empty()) {
            return true;
        }
        return std::find(allowed.begin(), allowed.end(), r) != allowed.end();
    };

    std::vector<int> placed;
    for (const RowGroup& g : *groups) {
        std::vector<int> keep;
        for (int r : g.rows) {
            if (isAllowed(r)) {
                keep.push_back(r);
            }
        }
        if (keep.empty()) {
            continue;   // grupo inteiro fora desta forma de luz
        }
        entries.push_back(Entry{-1, g.title, SDL_Rect{0, 0, 0, 0}});
        for (int r : keep) {
            entries.push_back(Entry{r, nullptr, SDL_Rect{0, 0, 0, 0}});
            placed.push_back(r);
        }
    }

    // Rede de seguranca: uma linha que a pagina use mas que nao esteja em
    // nenhum grupo continua a aparecer, em vez de desaparecer sem aviso.
    std::vector<int> leftovers;
    for (int r : allowed) {
        if (std::find(placed.begin(), placed.end(), r) == placed.end()) {
            leftovers.push_back(r);
        }
    }
    if (!leftovers.empty()) {
        entries.push_back(Entry{-1, "Outros", SDL_Rect{0, 0, 0, 0}});
        for (int r : leftovers) {
            entries.push_back(Entry{r, nullptr, SDL_Rect{0, 0, 0, 0}});
        }
    }

    if (!entryIsRow(focusedEntry)) {
        focusedEntry = 0;
        moveFocus(+1);
    }
}

void LightTweakPanel::moveFocus(int dir) {
    const int n = static_cast<int>(entries.size());
    if (n < 1) {
        focusedEntry = 0;
        return;
    }
    for (int step = 1; step <= n; step++) {
        const int i = ((focusedEntry + dir * step) % n + n) % n;
        if (entries[static_cast<size_t>(i)].row >= 0) {
            focusedEntry = i;
            return;
        }
    }
    focusedEntry = 0;
}

// ── DISTRIBUICAO POR COLUNAS ────────────────────────────────────────────────
// Os grupos NAO se partem ao meio: um titulo vai sempre com as suas linhas para
// a mesma coluna. Primeiro procura-se o menor numero de colunas que caiba na
// janela; so quando nem o maximo chega e que a altura da linha encolhe.
void LightTweakPanel::layoutEntries(int winW, int winH) {
    const int n = static_cast<int>(entries.size());
    if (n < 1) {
        panelBox = SDL_Rect{0, 0, 0, 0};
        return;
    }

    const int colW = std::min(kColW, std::max(150, winW - kMargin * 2));
    const int maxColsByWidth =
        std::max(1, std::min(kMaxCols, (winW - kMargin * 2 + kColGap) / (colW + kColGap)));

    // Alturas de cada grupo, em unidades de "um titulo + n linhas".
    struct Block {
        int first = 0;
        int count = 0;
    };
    std::vector<Block> blocks;
    for (int i = 0; i < n; i++) {
        if (entries[static_cast<size_t>(i)].row < 0 || blocks.empty()) {
            blocks.push_back(Block{i, 1});
        } else {
            blocks.back().count++;
        }
    }

    auto blockHeight = [&](const Block& b, int rh, int hh) {
        int h = 0;
        for (int i = b.first; i < b.first + b.count; i++) {
            h += (entries[static_cast<size_t>(i)].row < 0) ? hh : rh;
        }
        return h;
    };

    // Distribui os blocos por `cols` colunas e devolve a altura da mais alta,
    // ou -1 quando um bloco sozinho ja nao cabe.
    std::vector<int> blockCol(blocks.size(), 0);
    auto tryFit = [&](int cols, int rh, int hh, int availH, bool commit) {
        std::vector<int> used(static_cast<size_t>(cols), 0);
        int col = 0;
        for (size_t bi = 0; bi < blocks.size(); bi++) {
            const int bh = blockHeight(blocks[bi], rh, hh);
            if (used[static_cast<size_t>(col)] > 0 && used[static_cast<size_t>(col)] + bh > availH &&
                col + 1 < cols) {
                col++;
            }
            if (commit) {
                blockCol[bi] = col;
            }
            used[static_cast<size_t>(col)] += bh;
        }
        int tallest = 0;
        for (int u : used) {
            tallest = std::max(tallest, u);
        }
        return tallest;
    };

    const int chromeH = kTitleH + kFootH + kMargin * 2;
    const int availH = std::max(120, winH - kMargin * 2 - chromeH);

    rowH = kRowH;
    headerH = kHeaderH;
    int cols = 1;
    for (; cols <= maxColsByWidth; cols++) {
        if (tryFit(cols, rowH, headerH, availH, false) <= availH) {
            break;
        }
    }
    if (cols > maxColsByWidth) {
        // Nem com todas as colunas cabe: encolhe a linha ate caber. Nunca
        // abaixo de 30 px — e ai que o nome e a barra deixam de caber um por
        // cima do outro e comecavam a sobrepor-se.
        cols = maxColsByWidth;
        while (rowH > 30 && tryFit(cols, rowH, headerH, availH, false) > availH) {
            rowH--;
            headerH = std::max(20, rowH - 9);
        }
    }
    const int tallest = tryFit(cols, rowH, headerH, availH, true);

    const int bodyW = cols * colW + (cols - 1) * kColGap;
    const int bodyH = std::max(rowH, tallest);
    panelBox.w = bodyW + kMargin * 2;
    panelBox.h = bodyH + chromeH;
    panelBox.x = std::max(kMargin, winW - panelBox.w - kMargin);
    panelBox.y = kMargin;

    std::vector<int> colY(static_cast<size_t>(cols), panelBox.y + kMargin + kTitleH);
    for (size_t bi = 0; bi < blocks.size(); bi++) {
        const int c = blockCol[bi];
        const int x = panelBox.x + kMargin + c * (colW + kColGap);
        for (int i = blocks[bi].first; i < blocks[bi].first + blocks[bi].count; i++) {
            Entry& e = entries[static_cast<size_t>(i)];
            const int h = (e.row < 0) ? headerH : rowH;
            e.box = SDL_Rect{x, colY[static_cast<size_t>(c)], colW, h};
            colY[static_cast<size_t>(c)] += h;
        }
    }
}

SDL_Rect LightTweakPanel::barRectOf(const Entry& e) const {
    if (e.row < 0 || isButtonRow(e.row) || isToggleRow(e.row)) {
        return SDL_Rect{0, 0, 0, 0};
    }
    const int bx = e.box.x + kPadX;
    const int bw = std::max(20, e.box.w - kPadX * 2);
    // A barra fica colada ao fundo da linha, logo abaixo do texto.
    const int by = e.box.y + e.box.h - kBarH - 5;
    return SDL_Rect{bx, by, bw, kBarH};
}

int LightTweakPanel::entryAtPoint(int mx, int my) const {
    for (int i = 0; i < static_cast<int>(entries.size()); i++) {
        const Entry& e = entries[static_cast<size_t>(i)];
        if (e.row < 0) {
            continue;
        }
        if (mx >= e.box.x && mx < e.box.x + e.box.w && my >= e.box.y && my < e.box.y + e.box.h) {
            return i;
        }
    }
    return -1;
}

const char* LightTweakPanel::shapeName() const {
    switch (shape) {
    case LightMaskShape::Circle:
        return "Circulo";
    case LightMaskShape::Ellipse:
        return "Elipse";
    case LightMaskShape::Cone:
        return "Cone";
    case LightMaskShape::SoftRect:
        return "Rect suave";
    case LightMaskShape::Torch:
        return "Tocha";
    default:
        return "?";
    }
}

void LightTweakPanel::cycleShape() {
    markDirty();
    const int v = (static_cast<int>(shape) + 1) % 5;
    shape = static_cast<LightMaskShape>(v);
}

float LightTweakPanel::getRowNormalized(int logicalRow) const {
    switch (logicalRow) {
    case 0:
        return static_cast<float>(params.darknessMax) / 255.0f;
    case 1:
        return (params.falloffRadiusPx - 40.0f) / (600.0f - 40.0f);
    case 2:
        return (params.falloffGamma - 0.15f) / (4.0f - 0.15f);
    case 3:
        return (params.falloffCurve == LightFalloffCurve::Power) ? 1.0f : 0.0f;
    case 4:
        return params.innerLift / 0.85f;
    case 5:
        return static_cast<float>(params.numRings - 8) / static_cast<float>(48 - 8);
    case 6:
        return static_cast<float>(params.numSeg - 8) / static_cast<float>(48 - 8);
    case 7:
        return (params.ellipseAspect - 0.2f) / (2.5f - 0.2f);
    case 8:
        return (params.coneHalfAngleDeg - 5.0f) / (85.0f - 5.0f);
    case 9:
        return (params.coneLengthPx - 60.0f) / (700.0f - 60.0f);
    case 10:
        return (params.coneAxisDeg + 180.0f) / 360.0f;
    case 11:
        return params.coneFollowMouse ? 1.0f : 0.0f;
    case 12:
        return (params.rectHalfWidthPx - 10.0f) / (400.0f - 10.0f);
    case 13:
        return (params.rectHalfHeightPx - 10.0f) / (400.0f - 10.0f);
    case 14:
        return (params.rectSoftBandPx - 8.0f) / (250.0f - 8.0f);
    case 15:
        return (params.shadowCastDistanceMul - 0.5f) / (2.2f - 0.5f);
    case 16:
        return (params.shadowMaxLengthPx - 40.0f) / (800.0f - 40.0f);
    case 17:
        return (params.shadowLengthByLightMul - 0.35f) / (2.60f - 0.35f);
    case 18:
        return params.shadowSoftness;
    case 19:
        return static_cast<float>(params.shadowSoftLayers - 1) / 3.0f;
    case 20:
        return (params.lightTemporalSmoothing - 0.01f) / (0.95f - 0.01f);
    case 21:
        return (params.lightGridStepPx - 12.0f) / (64.0f - 12.0f);
    case 22:
        return (params.torchAnimSpeed - 0.15f) / (4.0f - 0.15f);
    case 23:
        return (params.torchMotionRangePx - 0.0f) / (30.0f - 0.0f);
    case 24:
        return params.torchWarpStrength;
    case 25:
        return params.torchPulseStrength;
    case 26:
        return params.torchColorWarmth / 2.0f;
    case 27:
        return params.torchColorStrength;
    case 28:
        return (params.spriteShadowMinScale - 0.60f) / (2.20f - 0.60f);
    case 29:
        return (params.spriteShadowMaxScale - 1.00f) / (4.20f - 1.00f);
    case 30:
        return 0.0f;
    case 31:
        return durabilityEnabled ? 1.0f : 0.0f;
    case 63:
        return params.torchGlowStrength / 4.0f;
    case 64:
        return (params.torchGlowRadiusScale - 0.05f) / (1.60f - 0.05f);
    default:
        break;
    }
    if (vision == nullptr) {
        return 0.0f;
    }
    switch (logicalRow) {
    case 32:
        return vision->enabled ? 1.0f : 0.0f;
    case 33:
        return (vision->coneHalfAngleDeg - 5.0f) / (85.0f - 5.0f);
    case 34:
        return (vision->coneFeatherDeg - 0.5f) / (30.0f - 0.5f);
    case 35:
        return (vision->coneLengthPx - 80.0f) / (2600.0f - 80.0f);
    case 36:
        return vision->coneLengthFeatherPx / 700.0f;
    case 37:
        return (vision->coneOriginForwardPx + 40.0f) / (160.0f);
    case 38:
        return (vision->turnSpeedDegPerSec - 90.0f) / (2000.0f - 90.0f);
    case 39:
        return (vision->footRadiusPx - 16.0f) / (420.0f - 16.0f);
    case 40:
        return (vision->maskFalloffGamma - 1.0f) / (8.0f - 1.0f);
    case 41:
        return vision->grayStrength;
    case 42:
        return vision->monoLift / 0.25f;
    case 43:
        return vision->monoTintStrength;
    case 44:
        return (vision->visionColorGain - 0.60f) / (2.00f - 0.60f);
    case 45:
        return static_cast<float>(params.ambientDarknessMax) / 255.0f;
    case 46:
        return vision->monoGain / 1.5f;
    case 47:
        return vision->lightColorStrength;
    case 48:
        return vision->outsideBlurPx / 24.0f;
    case 49:
        return vision->vignetteStrength;
    case 50:
        return vision->vignetteStart / 0.95f;
    case 51:
        return vision->hideItemsOutsideVision ? 1.0f : 0.0f;
    case 52:
        return vision->itemRevealThreshold;
    case 53:
        return vision->hideInteractablesOutsideVision ? 1.0f : 0.0f;
    case 54:
        return vision->hidePushablesOutsideVision ? 1.0f : 0.0f;
    case 55:
        return vision->unlitVisionDarkness / 0.95f;
    case 56:
        return vision->requireLightToSee ? 1.0f : 0.0f;
    case 57:
        return (vision->lightReachScale - 0.20f) / (3.00f - 0.20f);
    case 58:
        return (vision->lightPerceptionGamma - 0.50f) / (8.00f - 0.50f);
    case 59:
        return (vision->unlitFadeDistancePx - 40.0f) / (900.0f - 40.0f);
    case 60:
        return (vision->coneEdgeGamma - 1.0f) / (16.0f - 1.0f);
    case 61:
        return vision->monoHighlightGain / 2.0f;
    case 62:
        return vision->monoLightGlow / 2.0f;
    case 65:
        return vision->lightSharpenStrength;
    case 66:
        return (vision->cameraZoom - kCameraZoomMin) / (kCameraZoomMax - kCameraZoomMin);
    case 67:
        return (vision->visionSaturation - 0.00f) / (2.00f - 0.00f);
    case 68:
        return vision->hideMonsterOutsideLight ? 1.0f : 0.0f;
    default:
        return 0.0f;
    }
}

void LightTweakPanel::setRowFromNormalized(int logicalRow, float n01) {
    markDirty();
    const float u = std::max(0.0f, std::min(1.0f, n01));
    switch (logicalRow) {
    case 0:
        params.darknessMax = static_cast<Uint8>(u * 255.0f);
        break;
    case 1:
        params.falloffRadiusPx = 40.0f + u * (600.0f - 40.0f);
        break;
    case 2:
        params.falloffGamma = 0.15f + u * (4.0f - 0.15f);
        break;
    case 3:
        params.falloffCurve = (u >= 0.5f) ? LightFalloffCurve::Power : LightFalloffCurve::Smoothstep;
        break;
    case 4:
        params.innerLift = u * 0.85f;
        break;
    case 5:
        params.numRings = 8 + static_cast<int>(u * static_cast<float>(48 - 8) + 0.5f);
        break;
    case 6:
        params.numSeg = 8 + static_cast<int>(u * static_cast<float>(48 - 8) + 0.5f);
        break;
    case 7:
        params.ellipseAspect = 0.2f + u * (2.5f - 0.2f);
        break;
    case 8:
        params.coneHalfAngleDeg = 5.0f + u * (85.0f - 5.0f);
        break;
    case 9:
        params.coneLengthPx = 60.0f + u * (700.0f - 60.0f);
        break;
    case 10:
        params.coneAxisDeg = -180.0f + u * 360.0f;
        break;
    case 11:
        params.coneFollowMouse = (u >= 0.5f);
        break;
    case 12:
        params.rectHalfWidthPx = 10.0f + u * (400.0f - 10.0f);
        break;
    case 13:
        params.rectHalfHeightPx = 10.0f + u * (400.0f - 10.0f);
        break;
    case 14:
        params.rectSoftBandPx = 8.0f + u * (250.0f - 8.0f);
        break;
    case 15:
        params.shadowCastDistanceMul = 0.5f + u * (2.2f - 0.5f);
        break;
    case 16:
        params.shadowMaxLengthPx = 40.0f + u * (800.0f - 40.0f);
        break;
    case 17:
        params.shadowLengthByLightMul = 0.35f + u * (2.60f - 0.35f);
        break;
    case 18:
        params.shadowSoftness = u;
        break;
    case 19:
        params.shadowSoftLayers = 1 + static_cast<int>(u * 3.0f + 0.5f);
        break;
    case 20:
        params.lightTemporalSmoothing = 0.01f + u * (0.95f - 0.01f);
        break;
    case 21:
        params.lightGridStepPx = 12.0f + u * (64.0f - 12.0f);
        break;
    case 22:
        params.torchAnimSpeed = 0.15f + u * (4.0f - 0.15f);
        break;
    case 23:
        params.torchMotionRangePx = u * 30.0f;
        break;
    case 24:
        params.torchWarpStrength = u;
        break;
    case 25:
        params.torchPulseStrength = u;
        break;
    case 26:
        params.torchColorWarmth = u * 2.0f;
        break;
    case 27:
        params.torchColorStrength = u;
        break;
    case 28:
        params.spriteShadowMinScale = 0.60f + u * (2.20f - 0.60f);
        if (params.spriteShadowMaxScale < params.spriteShadowMinScale + 0.05f) {
            params.spriteShadowMaxScale = params.spriteShadowMinScale + 0.05f;
        }
        break;
    case 29:
        params.spriteShadowMaxScale = 1.00f + u * (4.20f - 1.00f);
        if (params.spriteShadowMaxScale < params.spriteShadowMinScale + 0.05f) {
            params.spriteShadowMinScale = params.spriteShadowMaxScale - 0.05f;
        }
        break;
    case 30:
        break;
    case 31:
        durabilityEnabled = (u >= 0.5f);
        break;
    case 63:
        params.torchGlowStrength = u * 4.0f;
        break;
    case 64:
        params.torchGlowRadiusScale = 0.05f + u * (1.60f - 0.05f);
        break;
    default:
        break;
    }
    if (vision == nullptr) {
        return;
    }
    switch (logicalRow) {
    case 32:
        vision->enabled = (u >= 0.5f);
        break;
    case 33:
        vision->coneHalfAngleDeg = 5.0f + u * (85.0f - 5.0f);
        break;
    case 34:
        vision->coneFeatherDeg = 0.5f + u * (30.0f - 0.5f);
        break;
    case 35:
        vision->coneLengthPx = 80.0f + u * (2600.0f - 80.0f);
        break;
    case 36:
        vision->coneLengthFeatherPx = u * 700.0f;
        break;
    case 37:
        vision->coneOriginForwardPx = -40.0f + u * 160.0f;
        break;
    case 38:
        vision->turnSpeedDegPerSec = 90.0f + u * (2000.0f - 90.0f);
        break;
    case 39:
        vision->footRadiusPx = 16.0f + u * (420.0f - 16.0f);
        break;
    case 40:
        vision->maskFalloffGamma = 1.0f + u * (8.0f - 1.0f);
        break;
    case 41:
        vision->grayStrength = u;
        break;
    case 42:
        vision->monoLift = u * 0.25f;
        break;
    case 43:
        vision->monoTintStrength = u;
        break;
    case 44:
        vision->visionColorGain = 0.60f + u * (2.00f - 0.60f);
        break;
    case 45:
        params.ambientDarknessMax = static_cast<Uint8>(u * 255.0f);
        break;
    case 46:
        vision->monoGain = u * 1.5f;
        break;
    case 47:
        vision->lightColorStrength = u;
        break;
    case 48:
        vision->outsideBlurPx = u * 24.0f;
        break;
    case 49:
        vision->vignetteStrength = u;
        break;
    case 50:
        vision->vignetteStart = u * 0.95f;
        break;
    case 51:
        vision->hideItemsOutsideVision = (u >= 0.5f);
        break;
    case 52:
        vision->itemRevealThreshold = std::max(0.02f, u);
        break;
    case 53:
        vision->hideInteractablesOutsideVision = (u >= 0.5f);
        break;
    case 54:
        vision->hidePushablesOutsideVision = (u >= 0.5f);
        break;
    case 55:
        vision->unlitVisionDarkness = u * 0.95f;
        break;
    case 56:
        vision->requireLightToSee = (u >= 0.5f);
        break;
    case 57:
        vision->lightReachScale = 0.20f + u * (3.00f - 0.20f);
        break;
    case 58:
        vision->lightPerceptionGamma = 0.50f + u * (8.00f - 0.50f);
        break;
    case 59:
        vision->unlitFadeDistancePx = 40.0f + u * (900.0f - 40.0f);
        break;
    case 60:
        vision->coneEdgeGamma = 1.0f + u * (16.0f - 1.0f);
        break;
    case 61:
        vision->monoHighlightGain = u * 2.0f;
        break;
    case 62:
        vision->monoLightGlow = u * 2.0f;
        break;
    case 65:
        vision->lightSharpenStrength = u;
        break;
    case 66:
        vision->cameraZoom = kCameraZoomMin + u * (kCameraZoomMax - kCameraZoomMin);
        break;
    case 67:
        vision->visionSaturation = u * 2.00f;
        break;
    case 68:
        vision->hideMonsterOutsideLight = (u >= 0.5f);
        break;
    default:
        break;
    }
}


// ── Texto em cache ──────────────────────────────────────────────────────────

void LightTweakPanel::freeText(CachedText& t) {
    if (t.tex) {
        SDL_DestroyTexture(t.tex);
        t.tex = nullptr;
    }
    t.w = 0;
    t.h = 0;
    t.text[0] = 0;
}

bool LightTweakPanel::ensureText(SDL_Renderer* renderer, CachedText& t, const char* s, int fontPx, SDL_Color col) {
    if (!renderer || !s) {
        return t.tex != nullptr;
    }
    if (t.tex != nullptr && std::strcmp(t.text, s) == 0) {
        return true;   // ja e este texto: nao ha nada a fazer
    }
    freeText(t);
    std::snprintf(t.text, sizeof(t.text), "%s", s);
    if (t.text[0] == 0) {
        return false;
    }
    auto font = Resources::GetFont("Recursos/font/times.ttf", fontPx);
    if (!font) {
        return false;
    }
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font.get(), t.text, col);
    if (!surf) {
        return false;
    }
    t.tex = SDL_CreateTextureFromSurface(renderer, surf);
    t.w = surf->w;
    t.h = surf->h;
    SDL_FreeSurface(surf);
    return t.tex != nullptr;
}

void LightTweakPanel::blit(SDL_Renderer* renderer, const CachedText& t, int x, int y, int maxW) {
    if (!renderer || !t.tex || t.w < 1) {
        return;
    }
    SDL_Rect dst{x, y, t.w, t.h};
    SDL_Rect src{0, 0, t.w, t.h};
    if (maxW > 0 && dst.w > maxW) {
        // Corta em vez de esmagar: um nome comprido perde as ultimas letras,
        // mas as outras continuam a ler-se com a mesma forma.
        src.w = maxW;
        dst.w = maxW;
    }
    SDL_RenderCopy(renderer, t.tex, &src, &dst);
}

void LightTweakPanel::releaseText() {
    for (int i = 0; i < kLogicalRows; i++) {
        freeText(rowName[i]);
        freeText(rowValue[i]);
    }
    freeText(titleText);
    freeText(footText);
    for (CachedText& t : headerText) {
        freeText(t);
    }
    headerText.clear();
}

void LightTweakPanel::rebuildRowText(SDL_Renderer* renderer, int logicalRow) {
    if (!renderer || logicalRow < 0 || logicalRow >= kLogicalRows) {
        return;
    }
    char buf[96];
    switch (logicalRow) {
    case 0:
        std::snprintf(buf, sizeof(buf), "%s: %u", kRowLabels[logicalRow], static_cast<unsigned>(params.darknessMax));
        break;
    case 1:
        std::snprintf(buf, sizeof(buf), "%s: %.0f", kRowLabels[logicalRow], params.falloffRadiusPx);
        break;
    case 2:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], params.falloffGamma);
        break;
    case 3:
        std::snprintf(buf, sizeof(buf), "%s: %s", kRowLabels[logicalRow],
                      params.falloffCurve == LightFalloffCurve::Power ? "pow" : "smooth");
        break;
    case 4:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], params.innerLift);
        break;
    case 5:
        std::snprintf(buf, sizeof(buf), "%s: %d", kRowLabels[logicalRow], params.numRings);
        break;
    case 6:
        std::snprintf(buf, sizeof(buf), "%s: %d", kRowLabels[logicalRow], params.numSeg);
        break;
    case 7:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], params.ellipseAspect);
        break;
    case 8:
        std::snprintf(buf, sizeof(buf), "%s: %.0f", kRowLabels[logicalRow], params.coneHalfAngleDeg);
        break;
    case 9:
        std::snprintf(buf, sizeof(buf), "%s: %.0f", kRowLabels[logicalRow], params.coneLengthPx);
        break;
    case 10:
        std::snprintf(buf, sizeof(buf), "%s: %.0f", kRowLabels[logicalRow], params.coneAxisDeg);
        break;
    case 11:
        std::snprintf(buf, sizeof(buf), "%s: %s", kRowLabels[logicalRow], params.coneFollowMouse ? "sim" : "nao");
        break;
    case 12:
        std::snprintf(buf, sizeof(buf), "%s: %.0f", kRowLabels[logicalRow], params.rectHalfWidthPx);
        break;
    case 13:
        std::snprintf(buf, sizeof(buf), "%s: %.0f", kRowLabels[logicalRow], params.rectHalfHeightPx);
        break;
    case 14:
        std::snprintf(buf, sizeof(buf), "%s: %.0f", kRowLabels[logicalRow], params.rectSoftBandPx);
        break;
    case 15:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], params.shadowCastDistanceMul);
        break;
    case 16:
        std::snprintf(buf, sizeof(buf), "%s: %.0f", kRowLabels[logicalRow], params.shadowMaxLengthPx);
        break;
    case 17:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], params.shadowLengthByLightMul);
        break;
    case 18:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], params.shadowSoftness);
        break;
    case 19:
        std::snprintf(buf, sizeof(buf), "%s: %d", kRowLabels[logicalRow], params.shadowSoftLayers);
        break;
    case 20:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], params.lightTemporalSmoothing);
        break;
    case 21:
        std::snprintf(buf, sizeof(buf), "%s: %.0f", kRowLabels[logicalRow], params.lightGridStepPx);
        break;
    case 22:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], params.torchAnimSpeed);
        break;
    case 23:
        std::snprintf(buf, sizeof(buf), "%s: %.1f", kRowLabels[logicalRow], params.torchMotionRangePx);
        break;
    case 24:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], params.torchWarpStrength);
        break;
    case 25:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], params.torchPulseStrength);
        break;
    case 26:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], params.torchColorWarmth);
        break;
    case 27:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], params.torchColorStrength);
        break;
    case 28:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], params.spriteShadowMinScale);
        break;
    case 29:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], params.spriteShadowMaxScale);
        break;
    case 30:
        std::snprintf(buf, sizeof(buf), "%s", kRowLabels[logicalRow]);
        break;
    case 31:
        std::snprintf(buf, sizeof(buf), "%s: %s", kRowLabels[logicalRow],
                      durabilityEnabled ? "ligado" : "desligado");
        break;
    case 32:
        std::snprintf(buf, sizeof(buf), "%s: %s", kRowLabels[logicalRow],
                      (vision && vision->enabled) ? "ligado" : "desligado");
        break;
    case 33:
        std::snprintf(buf, sizeof(buf), "%s: %.0f", kRowLabels[logicalRow], vision ? vision->coneHalfAngleDeg : 0.0f);
        break;
    case 34:
        std::snprintf(buf, sizeof(buf), "%s: %.1f", kRowLabels[logicalRow], vision ? vision->coneFeatherDeg : 0.0f);
        break;
    case 35:
        std::snprintf(buf, sizeof(buf), "%s: %.0f", kRowLabels[logicalRow], vision ? vision->coneLengthPx : 0.0f);
        break;
    case 36:
        std::snprintf(buf, sizeof(buf), "%s: %.0f", kRowLabels[logicalRow], vision ? vision->coneLengthFeatherPx : 0.0f);
        break;
    case 37:
        std::snprintf(buf, sizeof(buf), "%s: %.0f", kRowLabels[logicalRow], vision ? vision->coneOriginForwardPx : 0.0f);
        break;
    case 38:
        std::snprintf(buf, sizeof(buf), "%s: %.0f", kRowLabels[logicalRow], vision ? vision->turnSpeedDegPerSec : 0.0f);
        break;
    case 39:
        std::snprintf(buf, sizeof(buf), "%s: %.0f", kRowLabels[logicalRow], vision ? vision->footRadiusPx : 0.0f);
        break;
    case 40:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], vision ? vision->maskFalloffGamma : 0.0f);
        break;
    case 41:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], vision ? vision->grayStrength : 0.0f);
        break;
    case 42:
        std::snprintf(buf, sizeof(buf), "%s: %.3f", kRowLabels[logicalRow], vision ? vision->monoLift : 0.0f);
        break;
    case 43:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], vision ? vision->monoTintStrength : 0.0f);
        break;
    case 44:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], vision ? vision->visionColorGain : 0.0f);
        break;
    case 45:
        std::snprintf(buf, sizeof(buf), "%s: %u", kRowLabels[logicalRow],
                      static_cast<unsigned>(params.ambientDarknessMax));
        break;
    case 46:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], vision ? vision->monoGain : 0.0f);
        break;
    case 47:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], vision ? vision->lightColorStrength : 0.0f);
        break;
    case 48:
        std::snprintf(buf, sizeof(buf), "%s: %.1f", kRowLabels[logicalRow], vision ? vision->outsideBlurPx : 0.0f);
        break;
    case 49:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], vision ? vision->vignetteStrength : 0.0f);
        break;
    case 50:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], vision ? vision->vignetteStart : 0.0f);
        break;
    case 51:
        std::snprintf(buf, sizeof(buf), "%s: %s", kRowLabels[logicalRow],
                      (vision && vision->hideItemsOutsideVision) ? "sim" : "nao");
        break;
    case 52:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], vision ? vision->itemRevealThreshold : 0.0f);
        break;
    case 53:
        std::snprintf(buf, sizeof(buf), "%s: %s", kRowLabels[logicalRow],
                      (vision && vision->hideInteractablesOutsideVision) ? "sim" : "nao");
        break;
    case 54:
        std::snprintf(buf, sizeof(buf), "%s: %s", kRowLabels[logicalRow],
                      (vision && vision->hidePushablesOutsideVision) ? "sim" : "nao");
        break;
    case 55:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow],
                      vision ? vision->unlitVisionDarkness : 0.0f);
        break;
    case 56:
        std::snprintf(buf, sizeof(buf), "%s: %s", kRowLabels[logicalRow],
                      (vision && vision->requireLightToSee) ? "sim" : "nao");
        break;
    case 57:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], vision ? vision->lightReachScale : 0.0f);
        break;
    case 58:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow],
                      vision ? vision->lightPerceptionGamma : 0.0f);
        break;
    case 59:
        std::snprintf(buf, sizeof(buf), "%s: %.0f", kRowLabels[logicalRow],
                      vision ? vision->unlitFadeDistancePx : 0.0f);
        break;
    case 60:
        std::snprintf(buf, sizeof(buf), "%s: %.1f", kRowLabels[logicalRow], vision ? vision->coneEdgeGamma : 0.0f);
        break;
    case 61:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], vision ? vision->monoHighlightGain : 0.0f);
        break;
    case 62:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], vision ? vision->monoLightGlow : 0.0f);
        break;
    case 63:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], params.torchGlowStrength);
        break;
    case 64:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], params.torchGlowRadiusScale);
        break;
    case 65:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], vision ? vision->lightSharpenStrength : 0.0f);
        break;
    case 66:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], vision ? vision->cameraZoom : 1.0f);
        break;
    case 67:
        std::snprintf(buf, sizeof(buf), "%s: %.2f", kRowLabels[logicalRow], vision ? vision->visionSaturation : 1.0f);
        break;
    case 68:
        std::snprintf(buf, sizeof(buf), "%s: %s", kRowLabels[logicalRow],
                      (vision && vision->hideMonsterOutsideLight) ? "sim" : "nao");
        break;
    default:
        buf[0] = 0;
        break;
    }
    // O `switch` acima ja montou "Nome: valor" numa so linha. O painel mostra
    // as duas metades em sitios diferentes, por isso o VALOR e o que sobra
    // depois do nome — assim nenhuma das dezenas de linhas acima precisa de
    // mudar de forma.
    const char* label = kRowLabels[logicalRow];
    const char* value = buf;
    const size_t labelLen = label ? std::strlen(label) : 0;
    if (labelLen > 0 && std::strncmp(buf, label, labelLen) == 0) {
        value = buf + labelLen;
        if (*value == ':') {
            value++;
        }
        while (*value == ' ') {
            value++;
        }
    }

    const SDL_Color nameCol{206, 208, 220, 255};
    const SDL_Color valueCol{255, 214, 140, 255};
    ensureText(renderer, rowName[logicalRow], label ? label : "", 14, nameCol);
    // Uma linha de liga/desliga ja diz tudo pela caixa; repetir "sim"/"nao" ao
    // lado so ocupava espaco.
    ensureText(renderer, rowValue[logicalRow], isToggleRow(logicalRow) || isButtonRow(logicalRow) ? "" : value, 14,
               valueCol);
}

bool LightTweakPanel::ConsumeCreateLightRequest() {
    const bool requested = createLightRequested;
    createLightRequested = false;
    return requested;
}

void LightTweakPanel::Update(InputManager& input, float dt, int windowW, int windowH) {
    // Gravacao adiada: so escreve o ficheiro quando o utilizador para de mexer.
    if (dirty) {
        saveTimer += std::max(0.0f, dt);
        if (saveTimer >= kSaveDelaySec) {
            flushSave();
        }
    }
    if (shape != lastShape || page != lastPage) {
        lastShape = shape;
        lastPage = page;
        rebuildEntries();
    }

    if (input.KeyPress(LIGHT_PANEL_TOGGLE_KEY)) {
        visible = !visible;
        if (!visible) {
            flushSave();   // fechar o painel grava logo, sem esperar
        }
    }
    if (!visible) {
        dragEntry = -1;
        return;
    }

    if (input.KeyPress(PANEL_PAGE_TOGGLE_KEY)) {
        page = (page + 1) % kPageCount;
        lastPage = page;
        focusedEntry = 0;
        dragEntry = -1;
        rebuildEntries();
    }

    if (input.KeyPress(LIGHT_SHAPE_CYCLE_KEY)) {
        cycleShape();
        lastShape = shape;
        rebuildEntries();
    }

    if (page == 0 && (shape == LightMaskShape::Circle || shape == LightMaskShape::Torch) &&
        input.KeyPress(CREATE_LIGHT_KEY)) {
        createLightRequested = true;
    }

    if (entries.empty()) {
        return;
    }

    // A geometria tem de existir ANTES de responder ao rato: e a mesma que o
    // `Render` usa, por isso o que se ve e o que se clica nunca discordam.
    layoutEntries(windowW, windowH);

    if (input.KeyPress(PANEL_ROW_PREV_KEY)) {
        moveFocus(-1);
    }
    if (input.KeyPress(PANEL_ROW_NEXT_KEY)) {
        moveFocus(+1);
    }

    auto nudgeRowKeyboard = [&](int lr, int dir) {
        if (dir == 0) {
            return;
        }
        // TODAS as linhas de liga/desliga passam por aqui. A lista escrita a
        // mao que aqui estava esquecia-se da durabilidade, e por isso as teclas
        // +/- nao lhe faziam nada.
        if (isToggleRow(lr)) {
            flipToggleRow(lr);
            return;
        }
        if (isButtonRow(lr)) {
            createLightRequested = true;
            return;
        }
        if (lr == 19) {
            markDirty();
            params.shadowSoftLayers = std::max(1, std::min(4, params.shadowSoftLayers + dir));
            return;
        }

        float step = 0.03f;
        if (lr == 5 || lr == 6) {
            step = 0.08f;
        } else if (lr == 10) {
            step = 5.0f / 360.0f;
        } else if (lr == 2 || lr == 4 || lr == 15 || lr == 17 || lr == 18 || lr == 20 || lr == 22 || lr == 24 ||
                   lr == 25 || lr == 26 || lr == 27 || lr == 28 || lr == 29) {
            step = 0.02f;
        } else if (lr == 23) {
            step = 0.03f;
        } else if (lr >= 33) {
            step = 0.02f;
        }
        const float v = getRowNormalized(lr);
        setRowFromNormalized(lr, v + static_cast<float>(dir) * step);
    };

    if (entryIsRow(focusedEntry)) {
        const int lr = entries[static_cast<size_t>(focusedEntry)].row;
        if (input.KeyPress(SDLK_EQUALS) || input.KeyPress(SDLK_PLUS) || input.KeyPress(SDLK_KP_PLUS)) {
            nudgeRowKeyboard(lr, +1);
        }
        if (input.KeyPress(SDLK_MINUS) || input.KeyPress(SDLK_KP_MINUS)) {
            nudgeRowKeyboard(lr, -1);
        }
    }

    const int mx = input.GetMouseX();
    const int my = input.GetMouseY();

    if (input.MousePress(LEFT_MOUSE_BUTTON)) {
        const int hit = entryAtPoint(mx, my);
        if (hit >= 0) {
            focusedEntry = hit;
            const Entry& e = entries[static_cast<size_t>(hit)];
            dragEntry = -1;
            if (isButtonRow(e.row)) {
                createLightRequested = true;
            } else if (isToggleRow(e.row)) {
                // Clique em QUALQUER ponto da linha, nao so dentro da caixinha:
                // a caixa tem 14 px de lado e acertar-lhe era um exercicio.
                flipToggleRow(e.row);
            } else {
                const SDL_Rect bar = barRectOf(e);
                dragEntry = hit;
                if (bar.w > 0) {
                    setRowFromNormalized(e.row, static_cast<float>(mx - bar.x) / static_cast<float>(bar.w));
                }
            }
        }
    }
    if (input.MouseRelease(LEFT_MOUSE_BUTTON)) {
        dragEntry = -1;
    }
    if (dragEntry >= 0 && entryIsRow(dragEntry) && input.IsMouseDown(LEFT_MOUSE_BUTTON)) {
        // O cursor sai da barra (que e fina) mal se comeca a arrastar, por isso
        // so a posicao HORIZONTAL conta enquanto o botao estiver em baixo.
        const Entry& e = entries[static_cast<size_t>(dragEntry)];
        const SDL_Rect bar = barRectOf(e);
        if (bar.w > 0) {
            setRowFromNormalized(e.row, static_cast<float>(mx - bar.x) / static_cast<float>(bar.w));
        }
    }

    SDL_Renderer* r = Game::GetInstance().GetRenderer();
    if (r) {
        for (const Entry& e : entries) {
            if (e.row >= 0) {
                rebuildRowText(r, e.row);
            }
        }
    }
}

// ── Desenho ─────────────────────────────────────────────────────────────────

namespace {

void fillRect(SDL_Renderer* r, const SDL_Rect& box, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
    SDL_RenderFillRect(r, &box);
}

void frameRect(SDL_Renderer* r, const SDL_Rect& box, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
    SDL_RenderDrawRect(r, &box);
}

} // namespace

void LightTweakPanel::Render(SDL_Renderer* renderer, int windowW, int windowH) {
    if (!visible || !renderer || entries.empty()) {
        return;
    }
    layoutEntries(windowW, windowH);
    if (headerText.size() != entries.size()) {
        for (CachedText& t : headerText) {
            freeText(t);
        }
        headerText.assign(entries.size(), CachedText{});
    }

    SDL_BlendMode oldBm;
    SDL_GetRenderDrawBlendMode(renderer, &oldBm);
    Uint8 dr, dg, db, da;
    SDL_GetRenderDrawColor(renderer, &dr, &dg, &db, &da);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    fillRect(renderer, panelBox, 14, 15, 21, 232);
    frameRect(renderer, panelBox, 74, 78, 98, 255);

    // ── Barra de titulo ─────────────────────────────────────────────────────
    const SDL_Rect titleBar{panelBox.x + 1, panelBox.y + 1, panelBox.w - 2, kTitleH - 2};
    fillRect(renderer, titleBar, 30, 33, 46, 255);
    char title[128];
    if (page == 1) {
        std::snprintf(title, sizeof(title), "Campo de visao");
    } else {
        std::snprintf(title, sizeof(title), "Luz e sombra  [%s]", shapeName());
    }
    if (ensureText(renderer, titleText, title, 16, SDL_Color{255, 226, 168, 255})) {
        blit(renderer, titleText, titleBar.x + 10, titleBar.y + (titleBar.h - titleText.h) / 2, titleBar.w - 20);
    }
    // Marcador da pagina activa, encostado a direita do titulo.
    for (int i = 0; i < kPageCount; i++) {
        const SDL_Rect dot{titleBar.x + titleBar.w - 12 - (kPageCount - 1 - i) * 14,
                           titleBar.y + titleBar.h / 2 - 3, 7, 7};
        if (i == page) {
            fillRect(renderer, dot, 255, 214, 140, 255);
        } else {
            frameRect(renderer, dot, 120, 124, 148, 255);
        }
    }

    // ── Linhas ──────────────────────────────────────────────────────────────
    for (size_t i = 0; i < entries.size(); i++) {
        const Entry& e = entries[i];

        if (e.row < 0) {
            const SDL_Color hc{150, 178, 214, 255};
            if (ensureText(renderer, headerText[i], e.header ? e.header : "", 13, hc)) {
                blit(renderer, headerText[i], e.box.x + 2, e.box.y + e.box.h - headerText[i].h - 5, e.box.w - 4);
            }
            const SDL_Rect rule{e.box.x + 2, e.box.y + e.box.h - 3, e.box.w - 4, 1};
            fillRect(renderer, rule, 68, 80, 104, 255);
            continue;
        }

        if (static_cast<int>(i) == focusedEntry) {
            const SDL_Rect hi{e.box.x, e.box.y, e.box.w, e.box.h};
            fillRect(renderer, hi, 62, 84, 120, 110);
            const SDL_Rect tick{e.box.x, e.box.y, 3, e.box.h};
            fillRect(renderer, tick, 255, 214, 140, 255);
        }

        // Centra o texto no espaco que sobra ACIMA da barra, para o nome e a
        // barra nunca se tocarem quando a linha encolhe.
        const int textH = std::max(rowName[e.row].h, rowValue[e.row].h);
        const int textY = e.box.y + std::max(1, (e.box.h - kBarH - 5 - textH) / 2);
        int nameX = e.box.x + kPadX;
        int nameMaxW = e.box.w - kPadX * 2;

        if (isToggleRow(e.row)) {
            const int cb = std::min(15, std::max(11, rowH - 14));
            const SDL_Rect boxRect{e.box.x + kPadX, e.box.y + (e.box.h - cb) / 2, cb, cb};
            fillRect(renderer, boxRect, 26, 28, 38, 255);
            frameRect(renderer, boxRect, 140, 144, 172, 255);
            if (toggleRowValue(e.row)) {
                const SDL_Rect mark{boxRect.x + 3, boxRect.y + 3, cb - 6, cb - 6};
                fillRect(renderer, mark, 120, 220, 150, 255);
            }
            nameX = boxRect.x + cb + 7;
            nameMaxW = e.box.x + e.box.w - kPadX - nameX;
            blit(renderer, rowName[e.row], nameX, e.box.y + (e.box.h - rowName[e.row].h) / 2, nameMaxW);
            continue;
        }

        if (isButtonRow(e.row)) {
            const SDL_Rect btn{e.box.x + kPadX, e.box.y + 3, e.box.w - kPadX * 2, e.box.h - 7};
            fillRect(renderer, btn, 48, 82, 58, 255);
            frameRect(renderer, btn, 104, 176, 124, 255);
            blit(renderer, rowName[e.row], btn.x + std::max(6, (btn.w - rowName[e.row].w) / 2),
                 btn.y + (btn.h - rowName[e.row].h) / 2, btn.w - 12);
            continue;
        }

        // Linha normal: nome a esquerda, valor a direita, barra por baixo.
        const int valueW = rowValue[e.row].w;
        blit(renderer, rowName[e.row], nameX, textY, std::max(20, nameMaxW - valueW - 8));
        if (valueW > 0) {
            blit(renderer, rowValue[e.row], e.box.x + e.box.w - kPadX - valueW, textY, valueW);
        }

        const SDL_Rect bar = barRectOf(e);
        if (bar.w > 0) {
            fillRect(renderer, bar, 34, 36, 48, 255);
            frameRect(renderer, bar, 58, 62, 80, 255);
            const float n = std::max(0.0f, std::min(1.0f, getRowNormalized(e.row)));
            SDL_Rect fill{bar.x, bar.y, std::max(1, static_cast<int>(n * bar.w)), bar.h};
            fillRect(renderer, fill, 122, 196, 255, 255);
            // Pega: diz onde agarrar, e onde esta o valor quando a barra esta
            // quase vazia ou quase cheia.
            const SDL_Rect grip{bar.x + std::min(bar.w - 3, std::max(0, fill.w - 2)), bar.y - 2, 3, bar.h + 4};
            fillRect(renderer, grip, 240, 246, 255, 255);
        }
    }

    // ── Rodape com as teclas ────────────────────────────────────────────────
    const SDL_Rect foot{panelBox.x + 1, panelBox.y + panelBox.h - kFootH - 1, panelBox.w - 2, kFootH};
    fillRect(renderer, foot, 24, 26, 36, 255);
    const char* footStr = (page == 1) ? "\\ fecha   P muda de pagina   setas escolhem   +/- afinam"
                                      : "\\ fecha   P muda de pagina   K muda a forma   +/- afinam";
    if (ensureText(renderer, footText, footStr, 12, SDL_Color{150, 154, 176, 255})) {
        blit(renderer, footText, foot.x + 10, foot.y + (foot.h - footText.h) / 2, foot.w - 20);
    }

    SDL_SetRenderDrawBlendMode(renderer, oldBm);
    SDL_SetRenderDrawColor(renderer, dr, dg, db, da);
}
