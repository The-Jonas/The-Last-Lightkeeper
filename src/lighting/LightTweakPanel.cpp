#include "lighting/LightTweakPanel.h"
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
    "Curva: 0=smooth 1=pow",
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
    "Visao: ligada",
    "Visao: meio-angulo",
    "Visao: borda suave (graus)",
    "Visao: alcance (px mundo)",
    "Visao: fade do alcance",
    "Visao: apice a frente",
    "Visao: vel. de giro",
    "Pes: raio (px mundo)",
    "Mascara: gamma do corte",
    "PB: forca do cinzento",
    "PB: levantar preto",
    "PB: forca do tom frio",
    "PB: ganho na visao",
    "Escuro ambiente (sem luz)",
    "PB: brilho da camada",
    "PB: tamanho do pixel",
    "PB: desfoque (px)",
    "Vinheta: forca",
    "Vinheta: inicio",
    "Itens so no campo de visao",
    "Itens: limiar de revelacao",
    "Interagiveis so na visao",
    "Barris so na visao",
    "Visao sem luz: escuridao",
};

void destroyTex(SDL_Texture*& t) {
    if (t) {
        SDL_DestroyTexture(t);
        t = nullptr;
    }
}

/// Pagina 1: campo de visao do jogador + filtro preto-e-branco. As mesmas
/// barras servem para experimentar o formato do cone e a forca do monocromatico
/// sem recompilar.
void appendVisionRows(std::vector<int>& out) {
    out = {32, 33, 34, 35, 36, 37, 38, 39, 40, 55,
           41, 46, 47, 48, 42, 43, 49, 50,
           51, 53, 54, 52, 44, 45, 0};
}

void appendRowsForShape(LightMaskShape s, std::vector<int>& out) {
    out.clear();
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
        out = {0, 1, 2, 3, 4, 5, 6, 16, 17, 28, 29, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 30, 31};
        break;
    default:
        out = {0, 1, 2, 3, 4, 5, 6};
        break;
    }
}

} // namespace

LightTweakPanel::LightTweakPanel(LightMaskParams& paramsIn, LightMaskShape& shapeIn, PlayerVisionParams* visionIn)
    : params(paramsIn), shape(shapeIn), vision(visionIn), lastShape(shapeIn) {
    refreshActiveRows();
}

LightTweakPanel::~LightTweakPanel() {
    for (int i = 0; i < kLogicalRows; i++) {
        destroyTex(rowLabelTex[i]);
    }
}

bool LightTweakPanel::isButtonRow(int logicalRow) {
    return logicalRow == 30;
}

bool LightTweakPanel::isToggleRow(int logicalRow) {
    return logicalRow == 31 || logicalRow == 32 || logicalRow == 51 || logicalRow == 53 || logicalRow == 54;
}

int LightTweakPanel::rowHeight() const {
    const int available = std::max(120, lastWinH - kFirstRowY - 20);
    const int rows = std::max(1, slotCount());
    return std::max(19, std::min(kRowH, available / rows));
}

int LightTweakPanel::barOffsetY() const {
    return std::max(9, rowHeight() - kBarH - 5);
}

bool LightTweakPanel::toggleRowValue(int logicalRow) const {
    if (logicalRow == 31) {
        return durabilityEnabled;
    }
    if (logicalRow == 32) {
        return vision != nullptr && vision->enabled;
    }
    if (logicalRow == 51) {
        return vision != nullptr && vision->hideItemsOutsideVision;
    }
    if (logicalRow == 53) {
        return vision != nullptr && vision->hideInteractablesOutsideVision;
    }
    if (logicalRow == 54) {
        return vision != nullptr && vision->hidePushablesOutsideVision;
    }
    return false;
}

void LightTweakPanel::flipToggleRow(int logicalRow) {
    if (logicalRow == 31) {
        durabilityEnabled = !durabilityEnabled;
    } else if (logicalRow == 32 && vision) {
        vision->enabled = !vision->enabled;
    } else if (logicalRow == 51 && vision) {
        vision->hideItemsOutsideVision = !vision->hideItemsOutsideVision;
    } else if (logicalRow == 53 && vision) {
        vision->hideInteractablesOutsideVision = !vision->hideInteractablesOutsideVision;
    } else if (logicalRow == 54 && vision) {
        vision->hidePushablesOutsideVision = !vision->hidePushablesOutsideVision;
    }
}

void LightTweakPanel::refreshActiveRows() {
    if (page == 1 && vision != nullptr) {
        appendVisionRows(activeRows);
        if (focusedSlot >= static_cast<int>(activeRows.size())) {
            focusedSlot = std::max(0, static_cast<int>(activeRows.size()) - 1);
        }
        return;
    }
    appendRowsForShape(shape, activeRows);
    if (focusedSlot >= static_cast<int>(activeRows.size())) {
        focusedSlot = std::max(0, static_cast<int>(activeRows.size()) - 1);
    }
}

int LightTweakPanel::logicalRowAtSlot(int slotIndex) const {
    if (slotIndex < 0 || slotIndex >= static_cast<int>(activeRows.size())) {
        return 0;
    }
    return activeRows[static_cast<size_t>(slotIndex)];
}

int LightTweakPanel::slotCount() const {
    return static_cast<int>(activeRows.size());
}

void LightTweakPanel::layoutPanel(int winW, int& outLeft, int& outPanelW) const {
    outPanelW = std::min(kPanelWMax, std::max(120, winW - kMarginX * 2));
    outLeft = std::max(kMarginX, winW - outPanelW - kMarginX);
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
        return (vision->monoPixelSizePx - 1.0f) / (32.0f - 1.0f);
    case 48:
        return vision->monoBlurPx / 24.0f;
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
    default:
        return 0.0f;
    }
}

void LightTweakPanel::setRowFromNormalized(int logicalRow, float n01) {
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
        durabilityEnabled = !durabilityEnabled;
        break;
    default:
        break;
    }
    if (vision == nullptr) {
        return;
    }
    switch (logicalRow) {
    case 32:
        vision->enabled = !vision->enabled;
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
        vision->monoPixelSizePx = 1.0f + u * (32.0f - 1.0f);
        break;
    case 48:
        vision->monoBlurPx = u * 24.0f;
        break;
    case 49:
        vision->vignetteStrength = u;
        break;
    case 50:
        vision->vignetteStart = u * 0.95f;
        break;
    case 51:
        vision->hideItemsOutsideVision = !vision->hideItemsOutsideVision;
        break;
    case 52:
        vision->itemRevealThreshold = std::max(0.02f, u);
        break;
    case 53:
        vision->hideInteractablesOutsideVision = !vision->hideInteractablesOutsideVision;
        break;
    case 54:
        vision->hidePushablesOutsideVision = !vision->hidePushablesOutsideVision;
        break;
    case 55:
        vision->unlitVisionDarkness = u * 0.95f;
        break;
    default:
        break;
    }
}

bool LightTweakPanel::barHit(int mx, int my, int /*winW*/, int panelLeft, int panelW, int slotIndex, float* outN01) const {
    if (slotIndex < 0 || slotIndex >= slotCount()) {
        return false;
    }
    const int rowH = rowHeight();
    const int y = kFirstRowY + slotIndex * rowH;
    const int lr = logicalRowAtSlot(slotIndex);
    if (isButtonRow(lr) || isToggleRow(lr)) {
        return false;
    }
    const int by = y + barOffsetY();
    const int bx = panelLeft + kPadX;
    const int bw = std::max(20, panelW - kPadX * 2);
    if (mx < bx || mx > bx + bw || my < by || my > by + kBarH) {
        return false;
    }
    if (outN01) {
        *outN01 = static_cast<float>(mx - bx) / static_cast<float>(bw);
    }
    return true;
}

bool LightTweakPanel::rowButtonHit(int mx, int my, int panelLeft, int panelW, int slotIndex) const {
    if (slotIndex < 0 || slotIndex >= slotCount()) {
        return false;
    }
    const int lr = logicalRowAtSlot(slotIndex);
    if (!isButtonRow(lr) && !isToggleRow(lr)) {
        return false;
    }
    const int rowH = rowHeight();
    const int y = kFirstRowY + slotIndex * rowH;
    const int bx = panelLeft + kPadX;
    const int by = y + std::max(2, rowH / 3);
    const int bw = std::max(20, panelW - kPadX * 2);
    const int bh = std::max(12, rowH - std::max(2, rowH / 3) - 3);
    return mx >= bx && mx <= bx + bw && my >= by && my <= by + bh;
}

void LightTweakPanel::rebuildRowLabel(SDL_Renderer* renderer, int logicalRow) {
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
        std::snprintf(buf, sizeof(buf), "%s: %.1f", kRowLabels[logicalRow], vision ? vision->monoPixelSizePx : 0.0f);
        break;
    case 48:
        std::snprintf(buf, sizeof(buf), "%s: %.1f", kRowLabels[logicalRow], vision ? vision->monoBlurPx : 0.0f);
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
    default:
        buf[0] = 0;
        break;
    }
    if (std::strcmp(rowLabelBuf[logicalRow], buf) == 0) {
        return;
    }
    std::snprintf(rowLabelBuf[logicalRow], sizeof(rowLabelBuf[logicalRow]), "%s", buf);

    destroyTex(rowLabelTex[logicalRow]);
    rowLabelW[logicalRow] = 0;
    rowLabelH[logicalRow] = 0;

    auto font = Resources::GetFont("Recursos/font/times.ttf", 14);
    if (!font) {
        return;
    }
    SDL_Color col{220, 220, 230, 255};
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font.get(), buf, col);
    if (!surf) {
        return;
    }
    rowLabelTex[logicalRow] = SDL_CreateTextureFromSurface(renderer, surf);
    rowLabelW[logicalRow] = surf->w;
    rowLabelH[logicalRow] = surf->h;
    SDL_FreeSurface(surf);
}

void LightTweakPanel::Update(InputManager& input, float /*dt*/, int windowW, int windowH) {
    lastWinH = std::max(240, windowH);
    if (shape != lastShape || page != lastPage) {
        lastShape = shape;
        lastPage = page;
        refreshActiveRows();
    }

    if (input.KeyPress(LIGHT_PANEL_TOGGLE_KEY)) {
        visible = !visible;
    }
    if (!visible) {
        dragSlot = -1;
        return;
    }

    if (input.KeyPress(PANEL_PAGE_TOGGLE_KEY)) {
        page = (page + 1) % kPageCount;
        focusedSlot = 0;
        dragSlot = -1;
        refreshActiveRows();
    }

    if (input.KeyPress(LIGHT_SHAPE_CYCLE_KEY)) {
        cycleShape();
        refreshActiveRows();
    }

    if (page == 0 && (shape == LightMaskShape::Circle || shape == LightMaskShape::Torch) &&
        input.KeyPress(CREATE_LIGHT_KEY)) {
        createLightRequested = true;
    }

    const int nSlots = slotCount();
    if (nSlots < 1) {
        return;
    }

    if (input.KeyPress(PANEL_ROW_PREV_KEY)) {
        focusedSlot = (focusedSlot + nSlots - 1) % nSlots;
    }
    if (input.KeyPress(PANEL_ROW_NEXT_KEY)) {
        focusedSlot = (focusedSlot + 1) % nSlots;
    }

    int panelLeft = 0;
    int panelW = 0;
    layoutPanel(windowW, panelLeft, panelW);

    auto nudgeRowKeyboard = [&](int lr, int dir) {
        if (dir == 0) {
            return;
        }
        switch (lr) {
        case 3:
            params.falloffCurve = (params.falloffCurve == LightFalloffCurve::Power) ? LightFalloffCurve::Smoothstep
                                                                                      : LightFalloffCurve::Power;
            return;
        case 11:
            params.coneFollowMouse = !params.coneFollowMouse;
            return;
        case 32:
        case 51:
        case 53:
        case 54:
            flipToggleRow(lr);
            return;
        case 19:
            params.shadowSoftLayers = std::max(1, std::min(4, params.shadowSoftLayers + dir));
            return;
        default:
            break;
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
        } else if (lr == 30 || lr == 31 || lr == 32 || lr == 51 || lr == 53 || lr == 54) {
            step = 0.0f;
        } else if (lr >= 33) {
            step = 0.02f;
        }
        if (step > 0.0f) {
            const float v = getRowNormalized(lr);
            setRowFromNormalized(lr, v + static_cast<float>(dir) * step);
        }
    };

    if (input.KeyPress(SDLK_EQUALS) || input.KeyPress(SDLK_PLUS) || input.KeyPress(SDLK_KP_PLUS)) {
        nudgeRowKeyboard(logicalRowAtSlot(focusedSlot), +1);
    }
    if (input.KeyPress(SDLK_MINUS) || input.KeyPress(SDLK_KP_MINUS)) {
        nudgeRowKeyboard(logicalRowAtSlot(focusedSlot), -1);
    }

    const int mx = input.GetMouseX();
    const int my = input.GetMouseY();

    if (input.MousePress(LEFT_MOUSE_BUTTON)) {
        float n = 0.0f;
        for (int s = 0; s < nSlots; s++) {
            if (rowButtonHit(mx, my, panelLeft, panelW, s)) {
                const int lr = logicalRowAtSlot(s);
                if (isButtonRow(lr)) {
                    createLightRequested = true;
                } else if (isToggleRow(lr)) {
                    flipToggleRow(lr);
                }
                focusedSlot = s;
                dragSlot = -1;
                break;
            }
            if (barHit(mx, my, windowW, panelLeft, panelW, s, &n)) {
                dragSlot = s;
                setRowFromNormalized(logicalRowAtSlot(s), n);
                focusedSlot = s;
                break;
            }
        }
    }
    if (input.MouseRelease(LEFT_MOUSE_BUTTON)) {
        dragSlot = -1;
    }
    if (dragSlot >= 0 && input.IsMouseDown(LEFT_MOUSE_BUTTON)) {
        float n = 0.0f;
        if (barHit(mx, my, windowW, panelLeft, panelW, dragSlot, &n)) {
            setRowFromNormalized(logicalRowAtSlot(dragSlot), n);
        } else {
            // Keep updating from horizontal position while dragging (cursor often leaves the thin bar vertically).
            const int bx = panelLeft + kPadX;
            const int bw = std::max(20, panelW - kPadX * 2);
            if (!isButtonRow(logicalRowAtSlot(dragSlot)) && !isToggleRow(logicalRowAtSlot(dragSlot)) && mx >= bx &&
                mx <= bx + bw) {
                n = static_cast<float>(mx - bx) / static_cast<float>(bw);
                setRowFromNormalized(logicalRowAtSlot(dragSlot), n);
            }
        }
    }

    SDL_Renderer* r = Game::GetInstance().GetRenderer();
    if (r) {
        for (int lr : activeRows) {
            rebuildRowLabel(r, lr);
        }
    }
}

bool LightTweakPanel::ConsumeCreateLightRequest() {
    const bool requested = createLightRequested;
    createLightRequested = false;
    return requested;
}

void LightTweakPanel::Render(SDL_Renderer* renderer, int windowW, int windowH) {
    if (!visible || !renderer) {
        return;
    }
    lastWinH = std::max(240, windowH);
    const int rowH = rowHeight();
    const int barDy = barOffsetY();

    int panelLeft = 0;
    int panelW = 0;
    layoutPanel(windowW, panelLeft, panelW);

    const int ptop = 6;
    const int nSlots = slotCount();
    const int pheight = kFirstRowY + std::max(1, nSlots) * rowH + 16;

    SDL_BlendMode oldBm;
    SDL_GetRenderDrawBlendMode(renderer, &oldBm);
    Uint8 dr, dg, db, da;
    SDL_GetRenderDrawColor(renderer, &dr, &dg, &db, &da);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 12, 12, 18, 210);
    SDL_FRect panelBg{(float)panelLeft, (float)ptop, (float)panelW, (float)pheight};
    SDL_RenderFillRectF(renderer, &panelBg);

    SDL_SetRenderDrawColor(renderer, 60, 60, 78, 255);
    SDL_FRect border{(float)panelLeft, (float)ptop, (float)panelW, (float)pheight};
    SDL_RenderDrawRectF(renderer, &border);

    char title[112];
    if (page == 1) {
        std::snprintf(title, sizeof(title), "Campo de visao / PB   \\=luz  P=ocultar");
    } else {
        std::snprintf(title, sizeof(title), "Afina luz [%s]  K=forma \\=visao", shapeName());
    }
    auto font = Resources::GetFont("Recursos/font/times.ttf", 15);
    if (font) {
        SDL_Color tc{255, 240, 200, 255};
        SDL_Surface* ts = TTF_RenderUTF8_Blended(font.get(), title, tc);
        if (ts) {
            SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
            SDL_FreeSurface(ts);
            if (tt) {
                int tw = 0;
                int th = 0;
                SDL_QueryTexture(tt, nullptr, nullptr, &tw, &th);
                const int maxTw = panelW - 12;
                if (tw > maxTw) {
                    th = (th * maxTw) / tw;
                    tw = maxTw;
                }
                SDL_Rect dst{panelLeft + 6, ptop + 4, tw, th};
                SDL_RenderCopy(renderer, tt, nullptr, &dst);
                SDL_DestroyTexture(tt);
            }
        }
    }

    for (int s = 0; s < nSlots; s++) {
        const int lr = logicalRowAtSlot(s);
        const int y = kFirstRowY + s * rowH;
        const int bx = panelLeft + kPadX;
        const int bw = std::max(20, panelW - kPadX * 2);
        const int by = y + barDy;

        if (s == focusedSlot) {
            SDL_SetRenderDrawColor(renderer, 70, 90, 120, 120);
            SDL_FRect hi{(float)(panelLeft + 2), (float)(y - 2), (float)(panelW - 4), (float)(rowH - 4)};
            SDL_RenderFillRectF(renderer, &hi);
        }

        if (rowLabelTex[lr]) {
            SDL_Rect tdst{panelLeft + 4, y, rowLabelW[lr], rowLabelH[lr]};
            if (tdst.w > panelW - 8) {
                tdst.w = panelW - 8;
            }
            SDL_RenderCopy(renderer, rowLabelTex[lr], nullptr, &tdst);
        }

        if (isButtonRow(lr)) {
            SDL_SetRenderDrawColor(renderer, 48, 82, 58, 255);
            const int btnDy = std::max(2, rowH / 3);
            SDL_FRect button{(float)bx, (float)(y + btnDy), (float)bw, (float)std::max(12, rowH - btnDy - 3)};
            SDL_RenderFillRectF(renderer, &button);
            SDL_SetRenderDrawColor(renderer, 100, 170, 120, 255);
            SDL_RenderDrawRectF(renderer, &button);
        } else if (isToggleRow(lr)) {
            const float cbSize = std::min(18.0f, std::max(10.0f, rowH - 8.0f));
            const float cbX = (float)bx;
            const float cbY = (float)(y + rowH / 2.0f - cbSize / 2.0f);
            SDL_SetRenderDrawColor(renderer, 40, 40, 52, 255);
            const SDL_FRect cbBg{cbX, cbY, cbSize, cbSize};
            SDL_RenderFillRectF(renderer, &cbBg);
            SDL_SetRenderDrawColor(renderer, 130, 130, 160, 255);
            SDL_RenderDrawRectF(renderer, &cbBg);
            if (toggleRowValue(lr)) {
                SDL_SetRenderDrawColor(renderer, 90, 210, 120, 255);
                const SDL_FRect cbCheck{cbX + 4.0f, cbY + 4.0f, cbSize - 8.0f, cbSize - 8.0f};
                SDL_RenderFillRectF(renderer, &cbCheck);
            }
        } else {
            const float n = std::max(0.0f, std::min(1.0f, getRowNormalized(lr)));
            SDL_SetRenderDrawColor(renderer, 40, 40, 52, 255);
            SDL_FRect track{(float)bx, (float)by, (float)bw, (float)kBarH};
            SDL_RenderFillRectF(renderer, &track);

            SDL_SetRenderDrawColor(renderer, 120, 200, 255, 255);
            SDL_FRect fill{(float)bx, (float)by, std::max(1.0f, n * (float)bw), (float)kBarH};
            SDL_RenderFillRectF(renderer, &fill);
        }
    }

    SDL_SetRenderDrawBlendMode(renderer, oldBm);
    SDL_SetRenderDrawColor(renderer, dr, dg, db, da);
}
