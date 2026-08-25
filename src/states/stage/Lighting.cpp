#include "states/stage/StageState.h"
#include "states/stage/InternalHelpers.h"
#include "core/Game.h"
#include "engine/GameObject.h"
#include "engine/SpriteRenderer.h"
#include "math/Rect.h"
#include "world/TileSet.h"
#include "world/TileMap.h"
#include "core/InputManager.h"
#include "engine/Camera.h"
#include "engine/Component.h"
#include "gameplay/Character.h"
#include "world/Collider.h"
#include "world/Collision.h"
#include "core/GameData.h"
#include "states/EndState.h"
#include "ui/Text.h"
#include "lighting/TopDownLightShadows.h"
#include "lighting/LightShadowProfile.h"
#include "gameplay/Item.h"
#include "gameplay/ItemPickup.h"
#include "gameplay/HotbarComponent.h"
#include "gameplay/Box.h"
#include "ui/FadeEffect.h"
#include "gameplay/Repairable.h"
#include "gameplay/StairTrigger.h"
#include "gameplay/Jornal.h"
#include "gameplay/CandleStick.h"
#include "gameplay/RadioAsset.h"
#include "gameplay/Closet.h"
#include "gameplay/Window.h"
#include "core/Resources.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <array>
#include <queue>
#include <limits>
#include <unordered_map>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace stage_internal;
Vec2 StageState::ScreenToWorld(const Vec2& screenPos) const {
    const float z = Camera::GetZoom();
    if (z <= 1e-5f) {
        return Vec2(Camera::pos.x, Camera::pos.y);
    }
    return Vec2(screenPos.x / z + Camera::pos.x, screenPos.y / z + Camera::pos.y);
}

Vec2 StageState::WorldToScreen(const Vec2& worldPos) const {
    const float z = Camera::GetZoom();
    return Vec2((worldPos.x - Camera::pos.x) * z, (worldPos.y - Camera::pos.y) * z);
}

void StageState::CreateLightAtCursor() {
    if (lightMaskShape != LightMaskShape::Circle && lightMaskShape != LightMaskShape::Torch) {
        return;
    }
    InputManager& input = InputManager::GetInstance();
    LightInstance light;
    light.worldPos = ScreenToWorld(Vec2(static_cast<float>(input.GetMouseX()), static_cast<float>(input.GetMouseY())));
    if (tileMapComp && tileSet) {
        const int tileW = std::max(1, tileSet->GetTileWidth());
        const int tileH = std::max(1, tileSet->GetTileHeight());
        const int tx = static_cast<int>((light.worldPos.x - mapOrigin.x) / static_cast<float>(tileW));
        const int ty = static_cast<int>((light.worldPos.y - mapOrigin.y) / static_cast<float>(tileH));
        const auto& solid = tileMapComp->GetLightOcclusionSolid();
        if (!solid.empty() && tx >= 0 && ty >= 0 && tx < tileMapComp->GetWidth() && ty < tileMapComp->GetHeight()) {
            const size_t idx = static_cast<size_t>(tx + ty * tileMapComp->GetWidth());
            if (idx < solid.size() && solid[idx] != 0) {
                return; // do not place lights inside occluding cells
            }
        }
    }
    light.shape = lightMaskShape;
    light.params = lightMaskParams;
    light.enabled = true;

    // Prevent infinite oversaturation from stacking many lights in the same place:
    // if there is already a nearby light, refresh that one instead of adding another.
    const float overlapLimitWorld = std::max(24.0f, lightMaskParams.falloffRadiusPx * 0.18f);
    for (LightInstance& existing : lights) {
        if (existing.worldPos.Distance(light.worldPos) <= overlapLimitWorld) {
            existing.shape = light.shape;
            existing.params = light.params;
            existing.enabled = true;
            existing.worldPos = light.worldPos;
            return;
        }
    }

    static std::uint32_t sSeedCounter = 1u;
    sSeedCounter = sSeedCounter * 1664525u + 1013904223u;
    light.animationSeed = static_cast<float>(sSeedCounter & 0xFFFFu) / 65535.0f;
    lights.push_back(light);
    if (static_cast<int>(lights.size()) > maxActiveLights * 2) {
        lights.erase(lights.begin(), lights.begin() + (lights.size() - static_cast<size_t>(maxActiveLights * 2)));
    }
}

int StageState::CreateStaticLight(Vec2 pos, bool startsLit) {
    LightInstance light;
    light.worldPos = pos;
    light.shape = lightMaskShape;   // Usa o formato base da fase
    light.params = lightMaskParams; // Usa as cores/sombras base
    
    // Opcional: Se quiser que a luz do castiçal seja um pouco menor que a do poste:
    // light.params.falloffRadiusPx = 300.0f; 
    
    light.enabled = startsLit;
    
    // Semente aleatória para o fogo "tremer" de forma independente
    static std::uint32_t sSeed = 100;
    sSeed = sSeed * 1664525u + 1013904223u;
    light.animationSeed = static_cast<float>(sSeed & 0xFFFFu) / 65535.0f;
    
    lights.push_back(light);
    return lights.size() - 1; // Retorna o ID (posição no vetor)
}

int StageState::CreateStaticLight(Vec2 pos, bool startsLit, LightMaskShape shape, const LightMaskParams& params) {
    LightInstance light;
    light.worldPos = pos;
    light.shape = shape;
    light.params = params;
    light.enabled = startsLit;

    static std::uint32_t sSeedCustom = 200;
    sSeedCustom = sSeedCustom * 1664525u + 1013904223u;
    light.animationSeed = static_cast<float>(sSeedCustom & 0xFFFFu) / 65535.0f;

    lights.push_back(light);
    return static_cast<int>(lights.size() - 1);
}

void StageState::SetLightEnabled(int lightId, bool enabled) {
    if (lightId >= 0 && static_cast<size_t>(lightId) < lights.size()) {
        lights[static_cast<size_t>(lightId)].enabled = enabled;
    }
}

void StageState::UpdateInventoryLight() {
    const bool playerWantsLightHidden = Character::player && Character::player->hidePersonalLight;
    const bool wantLampLight =
        inventory.IsActiveLightLamp() && !playerWantsLightHidden && bigCharacterObject;

    if (!wantLampLight) {
        if (inventoryLightId >= 0) {
            SetLightEnabled(inventoryLightId, false);
        }
        return;
    }

    const Vec2 pos = bigCharacterObject->box.Center();
    const bool durabilityOn = lightTweakPanel ? lightTweakPanel->durabilityEnabled : true;
    const LightMaskParams params =
        durabilityOn ? inventory.BuildLampLightParams(lightMaskParams) : lightMaskParams;

    if (inventoryLightId < 0 || static_cast<size_t>(inventoryLightId) >= lights.size()) {
        inventoryLightId = CreateStaticLight(pos, true);
    }

    LightInstance& light = lights[static_cast<size_t>(inventoryLightId)];
    light.worldPos = pos;
    light.shape = lightMaskShape;
    light.params = params;
    light.enabled = true;
}

// ============================================================================
// CAMPO DE VISAO DO PERSONAGEM CONTROLADO
// ============================================================================
// Duas formas em coordenadas de TELA:
//   • um CONE apontado para onde o personagem olha (visao "para a frente");
//   • um CIRCULO pequeno colado aos pes (o que ele alcanca sem olhar).
// A malha de escuridao abre um buraco nestas duas formas (AppendVisionMaskLights)
// e o ScenePostFx devolve a COR so dentro delas. Tudo o resto do ecra fica em
// preto-e-branco, mesmo quando esta iluminado.

void StageState::UpdatePlayerVision(float dt) {
    visionFrame = PlayerVisionFrame{};

    if (!visionParams.enabled || controlledCharacter == nullptr) {
        return;
    }
    GameObject* body = controlledCharacterObject;
    if (body == nullptr) {
        return;
    }

    // ── Eixo alvo a partir da direcao de 4 vias (y da TELA cresce para baixo) ─
    float targetX = 0.0f;
    float targetY = 1.0f;
    switch (controlledCharacter->GetFacingDirection()) {
    case Character::Direction::UP:
        targetX = 0.0f;
        targetY = -1.0f;
        break;
    case Character::Direction::DOWN:
        targetX = 0.0f;
        targetY = 1.0f;
        break;
    case Character::Direction::LEFT:
        targetX = -1.0f;
        targetY = 0.0f;
        break;
    case Character::Direction::RIGHT:
    default:
        targetX = 1.0f;
        targetY = 0.0f;
        break;
    }
    const float targetRad = std::atan2(targetY, targetX);

    // ── Suavizacao: o cone VARRE ate a nova direcao em vez de saltar ─────────
    if (!visionAxisInitialized) {
        visionAxisRad = targetRad;
        visionAxisInitialized = true;
    } else {
        float delta = targetRad - visionAxisRad;
        while (delta > static_cast<float>(M_PI)) delta -= 2.0f * static_cast<float>(M_PI);
        while (delta < -static_cast<float>(M_PI)) delta += 2.0f * static_cast<float>(M_PI);
        const float maxStep = std::max(1.0f, visionParams.turnSpeedDegPerSec) * static_cast<float>(M_PI) / 180.0f *
                              std::max(0.0f, std::min(0.1f, dt));
        if (std::fabs(delta) <= maxStep) {
            visionAxisRad = targetRad;
        } else {
            visionAxisRad += (delta > 0.0f ? maxStep : -maxStep);
        }
        while (visionAxisRad > static_cast<float>(M_PI)) visionAxisRad -= 2.0f * static_cast<float>(M_PI);
        while (visionAxisRad < -static_cast<float>(M_PI)) visionAxisRad += 2.0f * static_cast<float>(M_PI);
    }

    const float dirX = std::cos(visionAxisRad);
    const float dirY = std::sin(visionAxisRad);

    // Os parametros estao em pixels de MUNDO: multiplica pelo zoom para chegar a
    // pixels de tela, senao o campo de visao encolhia quando a camera aproxima.
    const float zoom = std::max(0.05f, Camera::GetZoom());

    const Rect& b = body->box;
    const Vec2 chestWorld(b.x + 0.5f * b.w, b.y + 0.55f * b.h);
    const Vec2 chestScreen = WorldToScreen(chestWorld);

    const float apexForward = visionParams.coneOriginForwardPx * zoom;

    visionFrame.valid = true;
    visionFrame.coneX = chestScreen.x + dirX * apexForward;
    visionFrame.coneY = chestScreen.y + dirY * apexForward;
    visionFrame.dirX = dirX;
    visionFrame.dirY = dirY;
    visionFrame.halfAngleRad =
        std::max(2.0f, std::min(88.0f, visionParams.coneHalfAngleDeg)) * static_cast<float>(M_PI) / 180.0f;
    visionFrame.featherRad =
        std::max(0.5f, std::min(45.0f, visionParams.coneFeatherDeg)) * static_cast<float>(M_PI) / 180.0f;
    visionFrame.lengthPx = std::max(24.0f, visionParams.coneLengthPx) * zoom;
    visionFrame.lengthFeatherPx =
        std::max(4.0f, std::min(visionParams.coneLengthFeatherPx, visionParams.coneLengthPx * 0.9f)) * zoom;
    visionFrame.footRadiusPx = std::max(8.0f, visionParams.footRadiusPx) * zoom;
    visionFrame.maskGamma = std::max(0.2f, std::min(12.0f, visionParams.maskFalloffGamma));

    // ── Visao periferica: um circulo nos pes de CADA irmao ───────────────────
    // O companheiro tambem gera luz, mesmo sem estar a ser controlado.
    visionFrame.footCount = 0;
    auto addFoot = [&](GameObject* obj) {
        if (obj == nullptr || visionFrame.footCount >= PlayerVisionFrame::kMaxFeet) {
            return;
        }
        const Rect& fb = obj->box;
        const Vec2 foot = WorldToScreen(Vec2(fb.x + 0.5f * fb.w, fb.y + fb.h));
        visionFrame.footX[visionFrame.footCount] = foot.x;
        visionFrame.footY[visionFrame.footCount] = foot.y;
        visionFrame.footCount++;
    };
    addFoot(bigCharacterObject);
    addFoot(smallCharacterObject);
}

// Mesma conta que o fragment shader do `ScenePostFx` faz para `vis` — e a mesma
// que a malha de escuridao usa em `AlphaAt`. Serve para o codigo em C++ saber o
// que o jogador consegue ver (por agora: esconder os itens do chao).
// SE MUDAR AQUI, MUDE TAMBEM NO SHADER (kFragmentSrc).
float StageState::VisionVisibilityAtScreen(const Vec2& screenPos) const {
    if (!visionParams.enabled || !visionFrame.valid) {
        return 1.0f;
    }
    const float gamma = std::max(0.2f, visionFrame.maskGamma);

    float best = 0.0f;
    const float footR = std::max(1.0f, visionFrame.footRadiusPx);
    for (int i = 0; i < visionFrame.footCount && i < PlayerVisionFrame::kMaxFeet; i++) {
        const float dx = screenPos.x - visionFrame.footX[i];
        const float dy = screenPos.y - visionFrame.footY[i];
        const float t = std::min(1.0f, std::sqrt(dx * dx + dy * dy) / footR);
        best = std::max(best, 1.0f - std::pow(t, gamma));
    }

    const float dx = screenPos.x - visionFrame.coneX;
    const float dy = screenPos.y - visionFrame.coneY;
    const float fwd = dx * visionFrame.dirX + dy * visionFrame.dirY;
    const float perp = dx * (-visionFrame.dirY) + dy * visionFrame.dirX;
    const float L = std::max(1.0f, visionFrame.lengthPx);
    const float half = std::max(0.01f, visionFrame.halfAngleRad);
    const float feather = std::max(0.004f, visionFrame.featherRad);
    if (fwd > 0.0f && fwd <= L) {
        const float angAbs = std::fabs(std::atan2(perp, fwd));
        if (angAbs <= half + feather) {
            const float t = std::max(std::min(1.0f, fwd / L), std::min(1.0f, angAbs / half));
            const float angFade = std::max(0.0f, std::min(1.0f, (half + feather - angAbs) / feather));
            const float lenFade =
                std::max(0.0f, std::min(1.0f, (L - fwd) / std::max(1.0f, visionFrame.lengthFeatherPx)));
            best = std::max(best, (1.0f - std::pow(t, gamma)) * angFade * lenFade);
        }
    }
    return std::max(0.0f, std::min(1.0f, best));
}

// Que objetos somem fora do campo de visao. Tres categorias separadas porque o
// equilibrio entre estetica e jogabilidade e diferente em cada uma: um jornal a
// aparecer so quando olhas para ele e bom; um barril grande a sumir enquanto o
// empurras nao e.
bool StageState::ShouldHideOutsideVision(GameObject& go) const {
    if (!visionParams.enabled) {
        return false;
    }
    if (go.GetComponent<ItemPickup>() != nullptr) {
        return visionParams.hideItemsOutsideVision;
    }
    if (Box* box = go.GetComponent<Box>()) {
        return visionParams.hidePushablesOutsideVision && box->IsPushable();
    }
    if (!visionParams.hideInteractablesOutsideVision) {
        return false;
    }
    return go.GetComponent<Jornal>() != nullptr || go.GetComponent<Candlestick>() != nullptr ||
           go.GetComponent<RadioAsset>() != nullptr || go.GetComponent<Repairable>() != nullptr ||
           go.GetComponent<Window>() != nullptr || go.GetComponent<Closet>() != nullptr;
}

void StageState::AppendVisionMaskLights(std::vector<RadialLightOverlay::ScreenLight>& out) const {
    if (!visionFrame.valid) {
        return;
    }

    // Base comum: herda a qualidade/grelha da luz normal, mas com uma curva
    // "Power" agressiva — o interior do campo de visao fica LIMPO e a queda
    // acontece so junto a borda. Sem isto o cone parecia mais um candeeiro.
    LightMaskParams base = lightMaskParams;
    // O tecto tem de ser o ESCURO AMBIENTE, nao o `darknessMax` das luzes reais:
    // so assim a malha abre exatamente na mesma proporcao em que o shader devolve
    // a cor (`vis`). Com valores diferentes aparecia um halo colorido mas escuro
    // um pouco para la da borda do cone.
    base.darknessMax = lightMaskParams.ambientDarknessMax;
    base.falloffCurve = LightFalloffCurve::Power;
    base.falloffGamma = visionFrame.maskGamma;
    base.innerLift = 0.0f;
    base.fatorDicaDeRaio = 1.0f;
    base.coneFollowMouse = false;

    // `innerLift` e a fraccao da escuridao que SOBRA no centro da forma. O cone
    // e so VISAO: sem luz por perto continua escuro (o jogador ve as formas, nao
    // ve o sitio iluminado). Os circulos dos pes sao LUZ mesmo, e ficam a zero.
    // Uma luz real ganha na mesma, porque a malha guarda o MENOR alfa de todas.
    const float coneLift = std::max(0.0f, std::min(0.95f, visionParams.unlitVisionDarkness));

    // ── Cone ────────────────────────────────────────────────────────────────
    LightMaskParams coneParams = base;
    coneParams.innerLift = coneLift;
    coneParams.coneAxisDeg = visionAxisRad * 180.0f / static_cast<float>(M_PI);
    coneParams.coneHalfAngleDeg = visionFrame.halfAngleRad * 180.0f / static_cast<float>(M_PI);
    coneParams.coneFeatherDeg = visionFrame.featherRad * 180.0f / static_cast<float>(M_PI);
    coneParams.coneLengthPx = visionFrame.lengthPx;
    coneParams.coneLengthFeatherPx = visionFrame.lengthFeatherPx;
    coneParams.falloffRadiusPx = visionFrame.lengthPx;
    out.push_back({visionFrame.coneX, visionFrame.coneY, LightMaskShape::Cone, coneParams, 0.0f});

    // ── Circulos dos pes (um por irmao) ─────────────────────────────────────
    LightMaskParams footParams = base;
    footParams.falloffRadiusPx = visionFrame.footRadiusPx;
    for (int i = 0; i < visionFrame.footCount && i < PlayerVisionFrame::kMaxFeet; i++) {
        out.push_back({visionFrame.footX[i], visionFrame.footY[i], LightMaskShape::Circle, footParams, 0.0f});
    }
}
