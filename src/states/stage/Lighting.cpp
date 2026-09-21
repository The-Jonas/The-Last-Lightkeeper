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
#include "gameplay/Monster.h"
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
// e o ScenePostFx marca-as no ecra: la dentro a imagem fica NITIDA e com um
// realce de brilho e saturacao. Todo o resto do ecra continua a cores, mas
// DESFOCADO — e o desfoque, e nao a falta de cor, que aponta para onde o
// personagem olha.

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

    // Os parametros estao em pixels de MUNDO, mas o que se quer manter constante
    // e o TAMANHO NO ECRA do campo de visao: a zoom-base ele fica exactamente
    // como sempre foi, e afastar a camera passa a revelar MAIS MUNDO em vez de
    // encolher o cone. Por isso a escala e zoom/zoom-base (que da 1.0 na
    // zoom-base) e nao o zoom puro.
    const float zoom = std::max(0.05f, Camera::GetZoom()) / std::max(0.05f, Camera::GetBaseZoom());

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
    visionFrame.coneGamma = std::max(0.2f, std::min(16.0f, visionParams.coneEdgeGamma));

    // ── Precisa de luz para ver ──────────────────────────────────────────────
    // A rampa de proximidade parte dos PES do personagem controlado, nao do
    // apice do cone: e a distancia a ele que conta, olhe ele para onde olhar.
    visionFrame.requireLight = visionParams.requireLightToSee;
    visionFrame.lightGamma = std::max(0.5f, std::min(12.0f, visionParams.lightPerceptionGamma));
    visionFrame.unlitFadeRadiusPx = std::max(8.0f, visionParams.unlitFadeDistancePx) * zoom;
    const Vec2 playerFoot = WorldToScreen(Vec2(b.x + 0.5f * b.w, b.y + b.h));
    visionFrame.playerX = playerFoot.x;
    visionFrame.playerY = playerFoot.y;

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

// ============================================================================
// LUZ REAL DISPONIVEL: "chega luz a este pixel?"
// ============================================================================
// O cone diz para onde o jogador OLHA. Sozinho nao chega: numa sala as escuras
// ele nao distingue um jornal a dez metros so por estar virado para la. Por isso
// reduzimos cada fonte de luz da cena a um CIRCULO (centro + alcance) e usamos
// esses circulos como segundo factor. Um circulo e uma aproximacao grosseira das
// formas cone/rectangulo, mas aqui so decide "ha claridade nesta zona", nao
// desenha nada — a malha de escuridao continua a mandar no aspeto.
void StageState::BuildVisionLights(const std::vector<RadialLightOverlay::ScreenLight>& screenLights) {
    visionFrame.lightCount = 0;
    if (!visionFrame.valid) {
        return;
    }

    struct Sample {
        float x;
        float y;
        float r;
        float i;
        float distSq;
    };
    std::vector<Sample> samples;
    samples.reserve(screenLights.size());

    const float reach = std::max(0.05f, std::min(4.0f, visionParams.lightReachScale));

    for (const RadialLightOverlay::ScreenLight& sl : screenLights) {
        // Alcance por forma. O circulo/tocha usam o mesmo `rUser` que o
        // RadialLightOverlay calcula em RenderMany; o cone usa o comprimento; o
        // rectangulo usa a maior meia-aresta mais a banda suave.
        float radius = std::max(8.0f, sl.params.falloffRadiusPx) * sl.params.fatorDicaDeRaio;
        if (sl.shape == LightMaskShape::Cone) {
            radius = std::max(8.0f, sl.params.coneLengthPx);
        } else if (sl.shape == LightMaskShape::SoftRect) {
            radius = std::max(sl.params.rectHalfWidthPx, sl.params.rectHalfHeightPx) + sl.params.rectSoftBandPx;
        }
        radius *= reach;
        if (radius < 1.0f) {
            continue;
        }

        // `innerLift` e a fraccao da escuridao que a luz NAO remove no centro:
        // uma luz com lift alto ilumina pouco e nao deve devolver a cor toda.
        const float intensity = std::max(0.0f, std::min(1.0f, 1.0f - sl.params.innerLift));
        if (intensity <= 0.01f) {
            continue;
        }

        const float dx = sl.x - visionFrame.playerX;
        const float dy = sl.y - visionFrame.playerY;
        samples.push_back({sl.x, sl.y, radius, intensity, dx * dx + dy * dy});
    }

    // Ha mais luzes do que ranhuras no shader: fica com as MAIS PERTO do
    // personagem, que sao as que decidem o que ele ve.
    if (samples.size() > static_cast<size_t>(PlayerVisionFrame::kMaxLightSamples)) {
        std::partial_sort(samples.begin(), samples.begin() + PlayerVisionFrame::kMaxLightSamples, samples.end(),
                          [](const Sample& a, const Sample& b) { return a.distSq < b.distSq; });
        samples.resize(static_cast<size_t>(PlayerVisionFrame::kMaxLightSamples));
    }

    for (const Sample& sm : samples) {
        const int i = visionFrame.lightCount;
        visionFrame.lightX[i] = sm.x;
        visionFrame.lightY[i] = sm.y;
        visionFrame.lightR[i] = sm.r;
        visionFrame.lightI[i] = sm.i;
        visionFrame.lightCount++;
    }
}

// A regra completa e sempre GEOMETRIA x LUZ:
//   • geometria — `VisionVisibilityAtScreen`: o ponto cai dentro do cone ou do
//                 circulo dos pes, ou seja, o jogador esta virado para la;
//   • luz       — o maior entre `LightAmountAtScreen` (chega claridade) e
//                 `ProximityAtScreen` (esta ao alcance da mao).
// Fora do campo de visao da zero: nao aparece nada. Dentro do campo de visao mas
// as escuras e longe tambem tende para zero — e por isso que o FUNDO do cone fica
// cinzento e os objetos la ao fundo se apagam.
// SE MUDAR ESTAS CONTAS, MUDE TAMBEM NO SHADER (kFragmentSrc).
float StageState::LightAmountAtScreen(const Vec2& screenPos, bool includeCarriedLight) const {
    // Os circulos dos pes contam como luz a serio: e a claridade que o proprio
    // personagem traz consigo, e e o que faz um objeto encostado a ele aparecer.
    // O monstro e a excepcao (`includeCarriedLight` a false): esses circulos
    // existem mesmo com a luz apagada — sao visao periferica, nao sao chama — e
    // com eles o monstro voltaria a aparecer colado ao jogador no escuro.
    float best = 0.0f;
    const float footR = std::max(1.0f, visionFrame.footRadiusPx);
    for (int i = 0; includeCarriedLight && i < visionFrame.footCount && i < PlayerVisionFrame::kMaxFeet; i++) {
        const float dx = screenPos.x - visionFrame.footX[i];
        const float dy = screenPos.y - visionFrame.footY[i];
        const float t = std::min(1.0f, std::sqrt(dx * dx + dy * dy) / footR);
        best = std::max(best, 1.0f - std::pow(t, std::max(0.2f, visionFrame.maskGamma)));
    }

    const float gamma = std::max(0.5f, visionFrame.lightGamma);
    for (int i = 0; i < visionFrame.lightCount && i < PlayerVisionFrame::kMaxLightSamples; i++) {
        const float dx = screenPos.x - visionFrame.lightX[i];
        const float dy = screenPos.y - visionFrame.lightY[i];
        const float t = std::min(1.0f, std::sqrt(dx * dx + dy * dy) / std::max(1.0f, visionFrame.lightR[i]));
        best = std::max(best, visionFrame.lightI[i] * (1.0f - std::pow(t, gamma)));
    }
    return std::max(0.0f, std::min(1.0f, best));
}

// SE MUDAR ESTA CONTA, MUDE TAMBEM NO SHADER (kFragmentSrc).
float StageState::ProximityAtScreen(const Vec2& screenPos) const {
    const float dx = screenPos.x - visionFrame.playerX;
    const float dy = screenPos.y - visionFrame.playerY;
    const float r = std::max(1.0f, visionFrame.unlitFadeRadiusPx);
    return std::max(0.0f, std::min(1.0f, 1.0f - std::sqrt(dx * dx + dy * dy) / r));
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
            // `coneGamma` e nao `gamma`: o cone quer borda seca, os pes querem
            // borda macia.
            const float coneGamma = std::max(0.2f, visionFrame.coneGamma);
            best = std::max(best, (1.0f - std::pow(t, coneGamma)) * angFade * lenFade);
        }
    }
    return std::max(0.0f, std::min(1.0f, best));
}

// Que objetos somem fora do campo de visao. Tres categorias separadas porque o
// equilibrio entre estetica e jogabilidade e diferente em cada uma: um jornal a
// aparecer so quando olhas para ele e bom; um barril grande a sumir enquanto o
// empurras nao e.
float StageState::VisibilityOfObject(GameObject& go) const {
    if (!visionFrame.valid || !ShouldHideOutsideVision(go)) {
        return 1.0f;
    }
    const Vec2 screen = WorldToScreen(go.box.Center());
    // Porta geometrica: o `reveal` faz o interior do campo de visao contar como
    // 1 e so a borda dar valores intermedios.
    const float reveal = std::max(0.01f, visionParams.itemRevealThreshold);
    const float gate = std::max(0.0f, std::min(1.0f, VisionVisibilityAtScreen(screen) / reveal));

    if (Monster* monster = go.GetComponent<Monster>()) {
        // O monstro precisa de LUZ A SERIO: sem a rampa de proximidade e sem os
        // circulos dos pes (ver `LightAmountAtScreen`).
        const float lit = gate * LightAmountAtScreen(screen, false);
        // ... EXCEPTO quando a onda de uma passada dele acabou de passar por um
        // dos irmaos. Aí eles sabem onde ele esta sem precisarem de olhar, e o
        // ecra mostra-o durante esse instante (ver Monster::TriggerEchoReveal).
        return std::max(0.0f, std::min(1.0f, std::max(lit, monster->EchoRevealAmount())));
    }
    if (visionParams.requireLightToSee) {
        const float lit = std::max(LightAmountAtScreen(screen), ProximityAtScreen(screen));
        return std::max(0.0f, std::min(1.0f, gate * lit));
    }
    return gate;
}

bool StageState::ShouldHideOutsideVision(GameObject& go) const {
    if (!visionParams.enabled) {
        return false;
    }
    // Os castiçais sao a excepcao: o sprite fica SEMPRE visivel. Sao a
    // referencia visual do andar (o jogador usa-os para se orientar e para
    // saber o que ja acendeu), por isso so o filtro preto-e-branco os apanha
    // quando estao fora do campo de visao.
    if (go.GetComponent<Candlestick>() != nullptr) {
        return false;
    }
    // As ESCADAS seguem a mesma regra dos castiçais, e por um motivo mais forte:
    // sao o objectivo do andar. Um jogador que nao ve a escada nao sabe para
    // onde ir. O sprite fica sempre desenhado; o que muda com o campo de visao
    // e so o filtro (desfoque/cinzento) que o ScenePostFx aplica por cima.
    if (go.isStairs) {
        return false;
    }
    // O MONSTRO e o caso contrario: em vez de estar sempre a vista, so aparece
    // onde ha LUZ A SERIO e para onde o jogador esta mesmo a olhar. Ver
    // `Render`, que lhe da uma conta propria (sem a rampa de proximidade).
    if (go.GetComponent<Monster>() != nullptr) {
        return visionParams.hideMonsterOutsideLight;
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
    return go.GetComponent<Jornal>() != nullptr ||
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
    coneParams.coneLengthPx = visionFrame.lengthPx;
    coneParams.falloffRadiusPx = visionFrame.lengthPx;

    // A BORDA DA MALHA FICA SEMPRE MACIA, mesmo com o cone estrito. A malha de
    // escuridao e uma grelha grossa (~24 px) com cores interpoladas: um corte
    // seco nela nao da um risco recto, da uma escada. Quem desenha o risco recto
    // e o shader, que trabalha pixel a pixel. Aqui so interessa que a escuridao
    // levante na zona certa; a borda em si fica por conta do filtro.
    const float minFeatherRad = visionFrame.halfAngleRad * 0.08f;
    const float minLengthFeatherPx = visionFrame.lengthPx * 0.10f;
    coneParams.coneFeatherDeg =
        std::max(visionFrame.featherRad, minFeatherRad) * 180.0f / static_cast<float>(M_PI);
    coneParams.coneLengthFeatherPx = std::max(visionFrame.lengthFeatherPx, minLengthFeatherPx);
    coneParams.falloffGamma = std::min(visionFrame.coneGamma, 5.0f);
    out.push_back({visionFrame.coneX, visionFrame.coneY, LightMaskShape::Cone, coneParams, 0.0f});

    // ── Circulos dos pes (um por irmao) ─────────────────────────────────────
    LightMaskParams footParams = base;
    footParams.falloffRadiusPx = visionFrame.footRadiusPx;
    for (int i = 0; i < visionFrame.footCount && i < PlayerVisionFrame::kMaxFeet; i++) {
        out.push_back({visionFrame.footX[i], visionFrame.footY[i], LightMaskShape::Circle, footParams, 0.0f});
    }
}
