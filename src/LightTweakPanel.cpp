#include "../include/LightTweakPanel.h"
#include "../include/InputManager.h"
#include "../include/Resources.h"
#include "../include/Game.h"

#define INCLUDE_SDL_TTF
#include "../include/SDL_include.h"

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
    "Criar luz em C / clique",
};

void destroyTex(SDL_Texture*& t) {
    if (t) {
        SDL_DestroyTexture(t);
        t = nullptr;
    }
}

void appendRowsForShape(LightMaskShape s, std::vector<int>& out) {
    out.clear();
    switch (s) {
    case LightMaskShape::Circle:
        out = {0, 1, 2, 3, 4, 5, 6, 15, 16, 17, 18, 19, 20, 21, 28};
        break;
    case LightMaskShape::Ellipse:
        out = {0, 1, 2, 3, 4, 5, 6, 7, 15, 16, 17, 18, 19, 20, 21, 28};
        break;
    case LightMaskShape::Cone:
        out = {0, 2, 3, 4, 5, 6, 8, 9, 10, 11, 15, 16, 17, 18, 19, 20, 21, 28};
        break;
    case LightMaskShape::SoftRect:
        out = {0, 2, 3, 4, 5, 6, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 28};
        break;
    case LightMaskShape::Torch:
        out = {0, 1, 2, 3, 4, 5, 6, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28};
        break;
    default:
        out = {0, 1, 2, 3, 4, 5, 6};
        break;
    }
}

} // namespace

LightTweakPanel::LightTweakPanel(LightMaskParams& paramsIn, LightMaskShape& shapeIn)
    : params(paramsIn), shape(shapeIn), lastShape(shapeIn) {
    refreshActiveRows();
}

LightTweakPanel::~LightTweakPanel() {
    for (int i = 0; i < kLogicalRows; i++) {
        destroyTex(rowLabelTex[i]);
    }
}

void LightTweakPanel::refreshActiveRows() {
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
        return 0.0f;
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
        break;
    default:
        break;
    }
}

bool LightTweakPanel::barHit(int mx, int my, int /*winW*/, int panelLeft, int panelW, int slotIndex, float* outN01) const {
    if (slotIndex < 0 || slotIndex >= slotCount()) {
        return false;
    }
    const int y = kFirstRowY + slotIndex * kRowH;
    if (logicalRowAtSlot(slotIndex) == 28) {
        return false;
    }
    const int by = y + kBarOffsetY;
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
    if (logicalRowAtSlot(slotIndex) != 28) {
        return false;
    }
    const int y = kFirstRowY + slotIndex * kRowH;
    const int bx = panelLeft + kPadX;
    const int by = y + 16;
    const int bw = std::max(20, panelW - kPadX * 2);
    const int bh = std::max(16, kRowH - 20);
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
        std::snprintf(buf, sizeof(buf), "%s", kRowLabels[logicalRow]);
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

    auto font = Resources::GetFont("Recursos/font/neodgm.ttf", 14);
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
    if (shape != lastShape) {
        lastShape = shape;
        refreshActiveRows();
    }

    if (input.KeyPress(LIGHT_PANEL_TOGGLE_KEY)) {
        visible = !visible;
    }
    if (!visible) {
        dragSlot = -1;
        return;
    }

    if (input.KeyPress(LIGHT_SHAPE_CYCLE_KEY)) {
        cycleShape();
        refreshActiveRows();
    }

    if ((shape == LightMaskShape::Circle || shape == LightMaskShape::Torch) && input.KeyPress(CREATE_LIGHT_KEY)) {
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
                   lr == 25 || lr == 26 || lr == 27) {
            step = 0.02f;
        } else if (lr == 23) {
            step = 0.03f;
        } else if (lr == 28) {
            step = 0.0f;
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
                createLightRequested = true;
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

void LightTweakPanel::Render(SDL_Renderer* renderer, int windowW, int /*windowH*/) {
    if (!visible || !renderer) {
        return;
    }

    int panelLeft = 0;
    int panelW = 0;
    layoutPanel(windowW, panelLeft, panelW);

    const int ptop = 6;
    const int nSlots = slotCount();
    const int pheight = kFirstRowY + std::max(1, nSlots) * kRowH + 16;

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
    std::snprintf(title, sizeof(title), "Afina luz [%s]  K=forma P=ocultar", shapeName());
    auto font = Resources::GetFont("Recursos/font/neodgm.ttf", 15);
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
        const int y = kFirstRowY + s * kRowH;
        const int bx = panelLeft + kPadX;
        const int bw = std::max(20, panelW - kPadX * 2);
        const int by = y + kBarOffsetY;

        if (s == focusedSlot) {
            SDL_SetRenderDrawColor(renderer, 70, 90, 120, 120);
            SDL_FRect hi{(float)(panelLeft + 2), (float)(y - 2), (float)(panelW - 4), (float)(kRowH - 4)};
            SDL_RenderFillRectF(renderer, &hi);
        }

        if (rowLabelTex[lr]) {
            SDL_Rect tdst{panelLeft + 4, y, rowLabelW[lr], rowLabelH[lr]};
            if (tdst.w > panelW - 8) {
                tdst.w = panelW - 8;
            }
            SDL_RenderCopy(renderer, rowLabelTex[lr], nullptr, &tdst);
        }

        if (lr == 28) {
            SDL_SetRenderDrawColor(renderer, 48, 82, 58, 255);
            SDL_FRect button{(float)bx, (float)(y + 16), (float)bw, (float)std::max(16, kRowH - 20)};
            SDL_RenderFillRectF(renderer, &button);
            SDL_SetRenderDrawColor(renderer, 100, 170, 120, 255);
            SDL_RenderDrawRectF(renderer, &button);
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
