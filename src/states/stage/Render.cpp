// ─────────────────────────────────────────────────────────────────────────────
//  StageState — Render do frame e as etapas em que ele se divide.
//
//  A cena é desenhada no renderTarget (chão → sombras → objetos → escuridão →
//  irmãos carimbados), passa pelo ScenePostFx para a tela e, por cima, vêm os
//  flashes, o HUD, os menus e as ferramentas de debug.
// ─────────────────────────────────────────────────────────────────────────────
#include "states/stage/StageState.h"
#include "states/stage/InternalHelpers.h"
#include "audio/GameSfx.h"
#include "audio/GameVoice.h"
#include "core/Game.h"
#include "core/Resources.h"
#include "engine/Camera.h"
#include "engine/GameObject.h"
#include "engine/SpriteRenderer.h"
#include "gameplay/Box.h"
#include "gameplay/Character.h"
#include "gameplay/Monster.h"
#include "gameplay/Repairable.h"
#include "lighting/LightShadowProfile.h"
#include "lighting/ScenePostFx.h"
#include "lighting/TopDownLightShadows.h"
#include "world/Collider.h"
#include "world/TileMap.h"

#define INCLUDE_SDL_TTF
#include "SDL_include.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace stage_internal;

namespace {

constexpr float kPi   = 3.14159265f;
constexpr int   kHudZ = 100;                         // z a partir do qual o objeto é HUD
const char*     kUiFont = "Recursos/font/times.ttf";

// Só mexe no ALFA do destino: leva o alfa para "cenário" (1.0) onde o sprite tem
// pixel, apagando a marca de "irmão" que estava embaixo.
SDL_BlendMode AlphaOnlyOverBlend() {
    return SDL_ComposeCustomBlendMode(
        SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE,                 SDL_BLENDOPERATION_ADD,
        SDL_BLENDFACTOR_ONE,  SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD);
}

// Pinta a fonte "por dentro" do alfa do destino: cor = fonte × alfa do destino.
SDL_BlendMode KeepDstAlphaBlend() {
    return SDL_ComposeCustomBlendMode(
        SDL_BLENDFACTOR_DST_ALPHA, SDL_BLENDFACTOR_ZERO, SDL_BLENDOPERATION_ADD,
        SDL_BLENDFACTOR_ZERO,      SDL_BLENDFACTOR_ONE,  SDL_BLENDOPERATION_ADD);
}

// Mistura para cor já multiplicada pelo alfa (o que a KeepDstAlphaBlend produz).
SDL_BlendMode PremultipliedBlend() {
    return SDL_ComposeCustomBlendMode(
        SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD,
        SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD);
}

// Cria (ou recria, se a cena mudou de tamanho) a cópia da cena e o rascunho.
bool EnsureSceneAux(SDL_Renderer* r, SDL_Texture* scene, SDL_Texture*& snap, SDL_Texture*& scratch) {
    int w = 0, h = 0;
    if (!scene || SDL_QueryTexture(scene, nullptr, nullptr, &w, &h) != 0) return false;
    auto ensure = [&](SDL_Texture*& t) {
        int tw = 0, th = 0;
        if (t && SDL_QueryTexture(t, nullptr, nullptr, &tw, &th) == 0 && tw == w && th == h) return true;
        if (t) SDL_DestroyTexture(t);
        t = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
        return t != nullptr;
    };
    return ensure(snap) && ensure(scratch);
}

// Devolve ao sprite os pixels EXATOS que ele tinha logo depois da escuridão
// (tirados de `snap`), só dentro de `areaIn` e só onde o sprite tem pixel.
// Respeita o alfa atual do sprite (ex.: pilar apagado pelo FadeEffect).
void RestoreSpriteFromSnapshot(SDL_Renderer* r, SDL_Texture* scene, SDL_Texture* snap,
                               SDL_Texture* scratch, SpriteRenderer* sprite, const SDL_Rect& areaIn) {
    SDL_Texture* tex = sprite->GetTexturePtr();
    if (!tex) return;

    // Recorta pela tela: com parte fora, o SDL_RenderCopy esticava o que sobrava.
    int tw = 0, th = 0;
    SDL_QueryTexture(scene, nullptr, nullptr, &tw, &th);
    const SDL_Rect bounds{0, 0, tw, th};
    SDL_Rect area;
    if (!SDL_IntersectRect(&areaIn, &bounds, &area)) return;

    // 1. Rascunho transparente na área.
    SDL_SetRenderTarget(r, scratch);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 0);
    SDL_RenderFillRect(r, &area);

    // 2. Sprite sem mistura: sobra só a silhueta (o alfa).
    SDL_RenderSetClipRect(r, &area);
    SDL_BlendMode prev = SDL_BLENDMODE_BLEND;
    SDL_GetTextureBlendMode(tex, &prev);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);
    sprite->Render();
    SDL_SetTextureBlendMode(tex, prev);

    // 3. Cópia da cena pintada por dentro da silhueta.
    SDL_SetTextureBlendMode(snap, KeepDstAlphaBlend());
    SDL_RenderCopy(r, snap, &area, &area);
    SDL_RenderSetClipRect(r, nullptr);

    // 4. Recorte colado de volta na cena.
    SDL_SetRenderTarget(r, scene);
    SDL_SetTextureBlendMode(scratch, PremultipliedBlend());
    SDL_RenderCopy(r, scratch, &area, &area);

    // 5. Objeto apagado (alfa < 255): apaga a marca de "irmão" embaixo, para o
    //    shader desfocá-lo por igual.
    const SDL_Color tint = sprite->GetTint();
    if (tint.a < 255) {
        SDL_RenderSetClipRect(r, &area);
        SDL_SetTextureBlendMode(tex, AlphaOnlyOverBlend());
        sprite->SetTint(tint.r, tint.g, tint.b, 255);
        sprite->Render();
        sprite->SetTint(tint.r, tint.g, tint.b, tint.a);
        SDL_SetTextureBlendMode(tex, prev);
        SDL_RenderSetClipRect(r, nullptr);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
}

// True se uma luz em `screen` com raio `radius` ainda alcança a tela.
bool LightTouchesScreen(const Vec2& screen, float radius) {
    Game& g = Game::GetInstance();
    return screen.x >= -radius && screen.y >= -radius &&
           screen.x <= g.GetWindowsWidth() + radius && screen.y <= g.GetWindowsHeight() + radius;
}

// Raio do círculo de debug que mostra até onde uma luz projeta sombra.
float ShadowDebugRadius(const LightMaskParams& p) {
    return std::max(24.0f, std::max(8.0f, p.falloffRadiusPx) * std::max(0.4f, p.fatorDicaDeRaio));
}

// Pé (centro da base da caixa) de um objeto, em coordenadas de tela.
Vec2 FootOnScreen(const GameObject* go) {
    const Rect& b = go->box;
    const float z = Camera::GetZoom();
    return Vec2((b.x + 0.5f * b.w - Camera::pos.x) * z, (b.y + b.h - Camera::pos.y) * z);
}

// Contorno de círculo com `segs` segmentos.
void DrawCircleOutline(SDL_Renderer* r, int cx, int cy, int radius, int segs) {
    for (int i = 0; i < segs; i++) {
        const float a0 = (static_cast<float>(i) / segs) * 2.0f * kPi;
        const float a1 = (static_cast<float>(i + 1) / segs) * 2.0f * kPi;
        SDL_RenderDrawLine(r, cx + static_cast<int>(std::cos(a0) * radius), cy + static_cast<int>(std::sin(a0) * radius),
                              cx + static_cast<int>(std::cos(a1) * radius), cy + static_cast<int>(std::sin(a1) * radius));
    }
}

// Retângulo cobrindo a tela inteira com a cor e o modo de mistura dados.
void FillScreen(SDL_Renderer* r, int winW, int winH, SDL_BlendMode mode, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca) {
    SDL_SetRenderDrawBlendMode(r, mode);
    SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
    const SDL_Rect full{0, 0, winW, winH};
    SDL_RenderFillRect(r, &full);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
}

// Desenha um texto de uma linha; devolve a altura (0 se não desenhou).
int DrawLine(SDL_Renderer* r, TTF_Font* font, const std::string& text, int x, int y, SDL_Color color, int* outW = nullptr) {
    if (!font) return 0;
    SDL_Surface* s = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!s) return 0;
    const int w = s->w, h = s->h;
    if (SDL_Texture* t = SDL_CreateTextureFromSurface(r, s)) {
        const SDL_Rect dst{x, y, w, h};
        SDL_RenderCopy(r, t, nullptr, &dst);
        SDL_DestroyTexture(t);
    }
    SDL_FreeSurface(s);
    if (outW) *outW = w;
    return h;
}

}  // namespace

// ═════════════════════════════════════════════════════════════════════════════
//  Render
// ═════════════════════════════════════════════════════════════════════════════

// Em transição de escada, só o efeito. Senão: cena no renderTarget, pós-processo
// para a tela e, por cima, flashes, HUD, menus e debug.
void StageState::Render() {
    SDL_Renderer* renderer = Game::GetInstance().GetRenderer();
    const int winW = Game::GetInstance().GetWindowsWidth();
    const int winH = Game::GetInstance().GetWindowsHeight();

    if (sceneTransitionActive && sceneTransitionFrame) {
        RenderSceneTransition(renderer);
        return;
    }

    // ── Cena (no renderTarget) ───────────────────────────────────────────────
    SDL_SetRenderTarget(renderer, renderTarget);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    level.RenderBackground(renderer);
    SortObjectsForDrawing();

    if (lightsEnabled) LightShadowProfile::BeginLightsTiming();
    FrameLighting fl = BuildFrameLighting();
    UpdatePlayerVision(lastFrameDt);   // cone + círculo dos pés (buraco na escuridão e filtro P&B)

    if (lightsEnabled && shadowsEnabled) {
        RenderCharacterShadows(renderer, fl);
    }

    std::vector<RadialLightOverlay::ScreenLight> screenLights;
    if (lightsEnabled && radialGeometry) {
        screenLights = CollectScreenLights(fl);
        // Aqui, antes das sombras: elas já perguntam "chega luz a este ponto?".
        BuildVisionLights(screenLights);
        RenderObjectShadows(screenLights);
    }

    const bool stampPassWillRun = scenePostFx && scenePostFx->IsAvailable() &&
                                  ScenePostFx::NoGrayStampBlendMode() != SDL_BLENDMODE_INVALID;
    const std::vector<DrawnSprite> drawOrder = RenderWorldObjects(stampPassWillRun);

    if (lightsEnabled && radialGeometry) {
        RenderDarknessAndWallShadows(renderer, screenLights);
        UpdateIlluminationLevels(fl);
    }
    if (lightsEnabled) LightShadowProfile::EndLightsFrame();
    if (lightsEnabled && shadowsEnabled && fl.showDebugTools) {
        RenderLightDebugCircles(renderer, fl);
    }
    if (stampPassWillRun) {
        RenderStampedSprites(renderer, drawOrder);
    }

    // ── Tela ─────────────────────────────────────────────────────────────────
    SDL_SetRenderTarget(renderer, nullptr);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    const bool postFxDrew = PresentScene(renderer, winW, winH);

    RenderMonsterEchoes(renderer);   // depois do pós-processo: fora do cone é onde eles servem
    if (!postFxDrew) RenderBrightnessFallback(renderer, winW, winH);
    RenderScreenFlashes(renderer, winW, winH);
    RenderSanityAberration(renderer);

    if (showMapPhysicsDebug) {
        level.RenderCollisionOverlay(renderer);
        RenderGameplayCollisionDebug(renderer);
        RenderCompanionFollowPathDebug(renderer);
    }
    if (lightTweakPanel && lightTweakPanel->visible) {
        lightTweakPanel->Render(renderer, winW, winH);
    }

    // ── HUD ──────────────────────────────────────────────────────────────────
    for (const auto& go : objectArray) {
        if (go->z >= kHudZ) go->Render();
    }
    RenderLittleBrotherPowerHud(renderer, winW, winH);
    RenderControlIndicator(renderer);
    RenderRepairOverlay(renderer, winW, winH);

    fuelFlameHud.Render(renderer, inventory, winW, winH);   // HUD de jogo fica por baixo dos menus
    RenderInteractionPrompt(renderer);
    RenderTutorials(renderer);
    RenderVoiceSubtitle(renderer);
    RenderLevelTitleBanner(renderer);

    // ── Overlays ─────────────────────────────────────────────────────────────
    RenderPauseMenu(renderer);
    settingsMenu.Render(renderer);
    RenderQuitConfirmModal(renderer);
    RenderDocumentFolder(renderer);
    RenderJournalViewer(renderer);
    RenderSaveToast(renderer);
    dialogueBox.Render(renderer, winW, winH);

    if (Game::debugMode) RenderDebugStatus(renderer, winW);

    // Transição que acabou de começar: congela este quadro já composto.
    if (sceneTransitionActive && !sceneTransitionFrame) {
        CaptureSceneFrame(renderer);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Cena: ordem, luzes e sombras
// ═════════════════════════════════════════════════════════════════════════════

// Ordena por z; no mesmo z, pela base (Y) com depthOffset; irmão na escada usa a
// âncora da escada (a menos que os dois estejam nela); empate → sub_z.
void StageState::SortObjectsForDrawing() {
    auto baseY = [](const std::shared_ptr<GameObject>& o) {
        const GameObject* ref = o->owner ? o->owner : o.get();
        return ref->box.y + ref->box.h + o->depthOffset;
    };
    std::sort(objectArray.begin(), objectArray.end(),
              [&](const std::shared_ptr<GameObject>& a, const std::shared_ptr<GameObject>& b) {
        if (a->z != b->z) return a->z < b->z;

        float ya = baseY(a), yb = baseY(b);
        Character* ca = a->GetComponent<Character>();
        Character* cb = b->GetComponent<Character>();
        const bool aUp = ca && ca->isElevated;
        const bool bUp = cb && cb->isElevated;
        if (!(aUp && bUp)) {
            if (aUp) ya = ca->stairAnchorY;
            if (bUp) yb = cb->stairAnchorY;
        }
        if (std::abs(ya - yb) > 0.01f) return ya < yb;
        return a->sub_z < b->sub_z;
    });
}

// Estado de luz do frame: isqueiro na mão, luz acesa, parâmetros da luz de mão
// (com a durabilidade) e se o painel de debug está visível.
StageState::FrameLighting StageState::BuildFrameLighting() const {
    FrameLighting fl;
    const bool lightHidden = Character::player && Character::player->hidePersonalLight;
    fl.showDebugTools       = lightTweakPanel && lightTweakPanel->visible;
    fl.lighterFromInventory = inventory.IsActiveLightLighter() && !lightHidden;
    fl.torchLit             = inventory.IsUsableLightActive();
    const bool durabilityOn = lightTweakPanel ? lightTweakPanel->durabilityEnabled : true;
    fl.lighterParams = (fl.lighterFromInventory && durabilityOn)
                           ? inventory.BuildLighterLightParams(lightMaskParams) : lightMaskParams;
    return fl;
}

// Sombras projetadas e de contato dos irmãos (no chão, antes dos sprites) e a
// luz que chega a cada um — `touch` para a sombra, generosa para a sanidade.
void StageState::RenderCharacterShadows(SDL_Renderer* renderer, FrameLighting& fl) {
    const Uint64 blockStart = LightShadowProfile::IsActive() ? SDL_GetPerformanceCounter() : 0;

    struct ShadowCast {
        Vec2 lightScreen;
        float touch = 0.0f;
        float lengthPx = 0.0f;
        Uint8 alpha = 0;
        float contact = 0.0f;
    };
    std::vector<ShadowCast> bigCasts, smallCasts;

    // Luz para a sanidade: ponto do corpo mais perto da luz (pé ou meio) contra um
    // raio casado com o brilho VISÍVEL (compensa o zoom relativo à zoom-base).
    // O `touch` das sombras é medido só no pé e encolhia com o zoom.
    constexpr float kSanityLitRadiusFrac = 1.25f;
    auto sanityIllum = [](GameObject* obj, const Vec2& lightScreen, const LightMaskParams& params) {
        if (!obj) return 0.0f;
        const Rect& bx = obj->box;
        const float z = Camera::GetZoom();
        const Vec2 mid((bx.x + 0.5f * bx.w - Camera::pos.x) * z, (bx.y + 0.5f * bx.h - Camera::pos.y) * z);
        const float d = std::min(FootOnScreen(obj).Distance(lightScreen), mid.Distance(lightScreen));
        const float zRatio = z / std::max(0.05f, Camera::GetBaseZoom());
        const float litRadius = std::max(8.0f, params.falloffRadiusPx) * zRatio * kSanityLitRadiusFrac;
        return Clamp01(1.0f - d / std::max(1.0f, litRadius));
    };

    // Mede uma luz contra um irmão: contato, luz recebida e (se a luz chega) a sombra.
    auto measure = [&](GameObject* obj, const Vec2& lightScreen, const LightMaskParams& params,
                       float& maxContact, float& maxTouch, std::vector<ShadowCast>& casts) {
        float touch = 0.0f;
        IsFootLit(obj, lightScreen, params, &touch);
        float dPx = 0.0f, maxPx = 1.0f;
        if (obj) ComputeShadowDistanceRate(FootOnScreen(obj), lightScreen, params, &dPx, &maxPx);

        const float contactRadius = std::max(6.0f, maxPx * 0.07f);
        const float contact = (dPx <= contactRadius) ? Clamp01(1.0f - dPx / contactRadius) : 0.0f;
        maxContact = std::max(maxContact, contact);
        maxTouch   = std::max(maxTouch, std::max(touch, sanityIllum(obj, lightScreen, params)));

        // Só há sombra se a luz CHEGA ao personagem; o peso também dá a opacidade.
        const float weight = ShadowTouchWeight(touch);
        if (weight > 0.0f) {
            const Uint8 alpha = static_cast<Uint8>(std::clamp(params.darknessMax * weight, 0.0f, 255.0f));
            casts.push_back({lightScreen, touch, params.shadowMaxLengthPx * (1.0f - touch), alpha, contact});
        }
    };
    auto measureLight = [&](const Vec2& lightScreen, const LightMaskParams& params) {
        measure(bigCharacterObject, lightScreen, params, fl.bigMaxContact, fl.bigMaxTouch, bigCasts);
        measure(smallCharacterObject, lightScreen, params, fl.smallMaxContact, fl.smallMaxTouch, smallCasts);
    };

    if (cursorPreviewLightEnabled) {
        measureLight(smoothedDynamicLightScreenPos, lightMaskParams);
    }
    if (fl.lighterFromInventory && hasSmoothedTorchLight) {
        measureLight(smoothedTorchLightScreenPos, fl.lighterParams);
    }
    int counted = 0;
    for (const LightInstance& light : lights) {
        if (!light.enabled) continue;
        if (counted >= maxActiveLights) break;
        const Vec2 lightScreen = WorldToScreen(light.worldPos);
        if (!LightTouchesScreen(lightScreen, std::max(32.0f, light.params.falloffRadiusPx * 1.6f))) continue;
        measureLight(lightScreen, light.params);
        counted++;
    }

    // Uma sombra projetada por irmão: a da luz que mais o toca.
    const bool bigLockedPreview = cursorPreviewLightEnabled && previewLightLockedToPlayer &&
                                  previewLightAnchorPlayer == bigCharacterObject;
    const bool smallLockedPreview = cursorPreviewLightEnabled && previewLightLockedToPlayer &&
                                    previewLightAnchorPlayer == smallCharacterObject;
    const bool bigHidden   = Character::player && Character::player->hidePersonalLight;
    const bool smallHidden = Character::littleBrother && Character::littleBrother->hidePersonalLight;

    auto castStrongest = [&](std::vector<ShadowCast>& casts, GameObject* obj, bool skip) {
        if (casts.empty() || skip) return;
        const ShadowCast& c = *std::max_element(casts.begin(), casts.end(),
            [](const ShadowCast& a, const ShadowCast& b) { return a.touch < b.touch; });
        if (c.contact < 0.50f) {
            RenderProjectedSpriteShadow(obj, c.lightScreen, c.touch, c.lengthPx, c.alpha, lightMaskParams);
        }
    };
    castStrongest(bigCasts, bigCharacterObject, bigLockedPreview || bigHidden);
    castStrongest(smallCasts, smallCharacterObject, smallLockedPreview || smallHidden);
    UpdateControlledCharacterVisuals();   // restaura as cores depois das sombras

    if (fl.showDebugTools) {
        DrawPlayerShadowTouchDebug(renderer, bigCharacterObject, 255, 120, 120);
        DrawPlayerShadowTouchDebug(renderer, smallCharacterObject, 130, 220, 255);
    }

    // Luz presa no irmão ou luz de mão acesa: contato garantido.
    if (bigLockedPreview && bigCharacterObject)     fl.bigMaxContact   = std::max(fl.bigMaxContact, 0.92f);
    if (smallLockedPreview && smallCharacterObject) fl.smallMaxContact = std::max(fl.smallMaxContact, 0.92f);
    if (fl.torchLit) {
        if (bigCharacterObject)   fl.bigMaxContact   = std::max(fl.bigMaxContact, 0.92f);
        if (smallCharacterObject) fl.smallMaxContact = std::max(fl.smallMaxContact, 0.92f);
    }
    if (bigCharacterObject && fl.bigMaxContact > 0.0f && !bigHidden) {
        DrawContactFootShadow(renderer, bigCharacterObject->box, fl.bigMaxContact);
    }
    if (smallCharacterObject && fl.smallMaxContact > 0.0f && !smallHidden) {
        DrawContactFootShadow(renderer, smallCharacterObject->box, fl.smallMaxContact);
    }

    if (LightShadowProfile::IsActive()) {
        const double ms = static_cast<double>(SDL_GetPerformanceCounter() - blockStart) * 1000.0 /
                          static_cast<double>(SDL_GetPerformanceFrequency());
        LightShadowProfile::SetSpriteShadowBlockMs(ms);
    }
}

// Luzes que chegam à tela neste frame: preview, luz de mão e as do mapa (até maxActiveLights).
std::vector<RadialLightOverlay::ScreenLight> StageState::CollectScreenLights(const FrameLighting& fl) const {
    constexpr float kCursorLightBlend = 0.28f;
    constexpr float kTorchLightBlend  = 0.28f;

    std::vector<RadialLightOverlay::ScreenLight> out;
    out.reserve(static_cast<size_t>(maxActiveLights + 2));
    if (cursorPreviewLightEnabled) {
        out.push_back({smoothedDynamicLightScreenPos.x, smoothedDynamicLightScreenPos.y, lightMaskShape,
                       lightMaskParams, kCursorLightBlend});
    }
    if (fl.lighterFromInventory && hasSmoothedTorchLight) {
        out.push_back({smoothedTorchLightScreenPos.x, smoothedTorchLightScreenPos.y, lightMaskShape,
                       fl.lighterParams, kTorchLightBlend});
    }
    int counted = 0;
    for (const LightInstance& light : lights) {
        if (!light.enabled) continue;
        if (counted >= maxActiveLights) break;
        const Vec2 s = WorldToScreen(light.worldPos);
        if (!LightTouchesScreen(s, std::max(32.0f, light.params.falloffRadiusPx * 1.4f))) continue;
        out.push_back({s.x, s.y, light.shape, light.params, light.animationSeed});
        counted++;
    }
    return out;
}

// Sombra dos objetos de cenário registrados: uma por objeto, da luz que mais o
// toca, sumindo junto com o objeto fora do campo de visão.
void StageState::RenderObjectShadows(const std::vector<RadialLightOverlay::ScreenLight>& screenLights) {
    for (GameObject* obj : testShadowObjects) {
        if (!obj || obj == bigCharacterObject || obj == smallCharacterObject) continue;
        const float visibility = VisibilityOfObject(*obj);
        if (visibility <= 0.01f) continue;

        Vec2 bestLight;
        float bestTouch = 0.0f, bestLength = 0.0f;
        Uint8 bestAlpha = 0;
        for (const auto& sl : screenLights) {
            const Vec2 lightScreen(sl.x, sl.y);
            float touch = 0.0f;
            IsFootLit(obj, lightScreen, sl.params, &touch);
            const float weight = ShadowTouchWeight(touch);
            if (weight > 0.0f && touch > bestTouch) {
                bestTouch  = touch;
                bestLight  = lightScreen;
                bestLength = sl.params.shadowMaxLengthPx * Clamp01(1.0f - touch);
                bestAlpha  = static_cast<Uint8>(std::clamp(sl.params.darknessMax * weight, 0.0f, 255.0f));
            }
        }
        if (bestTouch > 0.0f) {
            RenderSingleLightSpriteShadow(obj, bestLight, bestTouch, bestLength,
                                          static_cast<Uint8>(bestAlpha * visibility), lastFrameDt);
        }
    }
}

// Desenha os objetos do mundo na ordem do Y-sort. Interagíveis fora da visão
// somem/aparecem pela visibilidade; os irmãos e interagíveis que o shader
// "carimba" são desenhados invisíveis aqui e redesenhados depois da escuridão.
// Devolve a ordem desenhada (para o passo dos carimbos).
std::vector<StageState::DrawnSprite> StageState::RenderWorldObjects(bool stampPassWillRun) {
    std::vector<DrawnSprite> drawOrder;
    drawOrder.reserve(objectArray.size());
    GameObject* focus = GetInteractionFocus();

    for (const auto& goPtr : objectArray) {
        GameObject* go = goPtr.get();
        if (go->z >= kHudZ) continue;
        SpriteRenderer* sprite = go->GetComponent<SpriteRenderer>();

        bool  stamped = false;
        float shown = 1.0f, light = 1.0f;
        bool  faded = false;
        if (visionFrame.valid && ShouldHideOutsideVision(*go)) {
            shown = (go == focus) ? 1.0f : VisibilityOfObject(*go);
            if (shown <= 0.01f) continue;
            if (shown < 0.999f && sprite) {
                sprite->SetTint(255, 255, 255, static_cast<Uint8>(shown * 255.0f));
                faded = true;
            }
            if (sprite) {
                light = Clamp01(LightAmountAtScreen(WorldToScreen(go->box.Center())));
                stamped = true;
            }
        }
        // Irmãos: sempre carimbados — menos escondido no armário (o redesenho
        // devolveria o alfa cheio e desfaria a invisibilidade).
        Character* ch = go->GetComponent<Character>();
        if ((go == bigCharacterObject || go == smallCharacterObject) && visionFrame.valid &&
            !(ch && ch->isHidden) && sprite) {
            shown = 1.0f;
            light = 1.0f;
            stamped = true;
        }

        SpriteRenderer* hideSprite = (stamped && stampPassWillRun) ? sprite : nullptr;
        SDL_Color savedTint{255, 255, 255, 255};
        if (hideSprite) {
            savedTint = hideSprite->GetTint();
            hideSprite->SetTint(savedTint.r, savedTint.g, savedTint.b, 0);
        }
        const bool glow = RenderInteractionGlowIfNeeded(*go);
        go->Render();
        if (hideSprite) hideSprite->SetTint(savedTint.r, savedTint.g, savedTint.b, savedTint.a);

        if (sprite) drawOrder.push_back({go, sprite, stamped, shown, light, glow});
        if (faded)  sprite->SetTint(255, 255, 255, 255);
    }
    return drawOrder;
}

// Malha de escuridão por cima de tudo (as luzes + o buraco do campo de visão,
// que não projeta sombra nem conta para a sanidade) e as sombras das paredes.
void StageState::RenderDarknessAndWallShadows(SDL_Renderer* renderer,
                                              const std::vector<RadialLightOverlay::ScreenLight>& screenLights) {
    Game& g = Game::GetInstance();
    LightOcclusionContext occ;
    if (tileMapComp && tileSet) {
        occ.solidGrid  = &tileMapComp->GetLightOcclusionSolid();
        occ.mapWidth   = tileMapComp->GetWidth();
        occ.mapHeight  = tileMapComp->GetHeight();
        occ.tileWidth  = static_cast<float>(tileSet->GetTileWidth());
        occ.tileHeight = static_cast<float>(tileSet->GetTileHeight());
        occ.mapOriginX = mapOrigin.x;
        occ.mapOriginY = mapOrigin.y;
        occ.cameraX    = Camera::pos.x;
        occ.cameraY    = Camera::pos.y;
        occ.zoom       = Camera::GetZoom();
    }

    std::vector<RadialLightOverlay::ScreenLight> maskLights = screenLights;
    AppendVisionMaskLights(maskLights);
    radialGeometry->RenderMany(renderer, g.GetWindowsWidth(), g.GetWindowsHeight(), maskLights, occ);

    if (!shadowsEnabled || !staticShadowEdgesBuilt || staticShadowEdges.empty()) return;

    constexpr int kMaxShadowVolumes = 8;
    const std::vector<TopDownShadowEdge> noDynamic;
    const int n = std::min(static_cast<int>(screenLights.size()), kMaxShadowVolumes);
    for (int i = 0; i < n; i++) {
        const RadialLightOverlay::ScreenLight& sl = screenLights[i];
        // Luz dentro de um tile sólido não projeta sombra de parede.
        if (occ.IsEnabled()) {
            const int ltx = static_cast<int>((sl.x / occ.zoom + occ.cameraX - occ.mapOriginX) / occ.tileWidth);
            const int lty = static_cast<int>((sl.y / occ.zoom + occ.cameraY - occ.mapOriginY) / occ.tileHeight);
            if (ltx >= 0 && ltx < occ.mapWidth && lty >= 0 && lty < occ.mapHeight &&
                (*occ.solidGrid)[static_cast<size_t>(ltx + lty * occ.mapWidth)] != 0) {
                continue;
            }
        }
        TopDownLightShadows::RenderShadowVolumes(renderer, sl.x, sl.y, g.GetWindowsWidth(), g.GetWindowsHeight(),
                                                 staticShadowEdges, noDynamic, 90, sl.params.shadowMaxLengthPx,
                                                 sl.params.shadowSoftLayers, sl.params.shadowSoftness);
    }
}

// Luz que cada irmão recebe, lida pela sanidade. A luz de mão ilumina por
// inteiro o irmãozão; o irmãozinho só recebe se estiver perto. O clarão do
// trovão ilumina os dois.
void StageState::UpdateIlluminationLevels(const FrameLighting& fl) {
    bigLightContact   = fl.bigMaxContact;
    smallLightContact = fl.smallMaxContact;

    bigIlluminationLevel   = fl.bigMaxTouch;
    smallIlluminationLevel = fl.smallMaxTouch;
    if (fl.torchLit && bigCharacterObject) {
        bigIlluminationLevel = std::max(fl.bigMaxTouch, 0.92f);
        if (smallCharacterObject) {
            const float shareRadius = std::max(1.0f, lightMaskParams.falloffRadiusPx);
            const float dist = smallCharacterObject->box.Center().Distance(bigCharacterObject->box.Center());
            smallIlluminationLevel = std::max(fl.smallMaxTouch, 0.92f * std::clamp(1.0f - dist / shareRadius, 0.0f, 1.0f));
        }
    }
    const float thunder = GameSfx::GetThunderFlashStrength() * 0.88f;
    if (thunder > 0.01f) {
        bigIlluminationLevel   = std::max(bigIlluminationLevel, thunder);
        smallIlluminationLevel = std::max(smallIlluminationLevel, thunder);
    }
}

// Debug (painel de luz visível): alcance das sombras da luz de preview, da luz de mão e das luzes do mapa.
void StageState::RenderLightDebugCircles(SDL_Renderer* renderer, const FrameLighting& fl) {
    if (cursorPreviewLightEnabled) {
        DrawDebugCircle(renderer, smoothedDynamicLightScreenPos.x, smoothedDynamicLightScreenPos.y,
                        ShadowDebugRadius(lightMaskParams), 255, 210, 90, 130);
    }
    if (fl.lighterFromInventory && hasSmoothedTorchLight) {
        DrawDebugCircle(renderer, smoothedTorchLightScreenPos.x, smoothedTorchLightScreenPos.y,
                        ShadowDebugRadius(fl.lighterParams) * 0.85f, 255, 150, 70, 150);
    }
    int counted = 0;
    for (const LightInstance& light : lights) {
        if (!light.enabled) continue;
        if (counted >= maxActiveLights) break;
        const Vec2 s = WorldToScreen(light.worldPos);
        if (!LightTouchesScreen(s, std::max(32.0f, light.params.falloffRadiusPx * 1.6f))) continue;
        DrawDebugCircle(renderer, s.x, s.y, ShadowDebugRadius(light.params), 120, 220, 255, 95);
        counted++;
    }
}

// Depois da escuridão, na ordem do Y-sort:
//  • carimbados (irmãos e interagíveis na visão): redesenhados com brilho cheio
//    e com a luz que recebem gravada no ALFA — o shader lê isso para mantê-los
//    nítidos e devolver a cor na medida da luz;
//  • os demais que estão NA FRENTE de algo redesenhado voltam por cima, com os
//    pixels exatos da cópia da cena (ou uma tinta aproximada, sem a cópia).
void StageState::RenderStampedSprites(SDL_Renderer* renderer, const std::vector<DrawnSprite>& drawOrder) {
    const SDL_BlendMode stampBlend = ScenePostFx::NoGrayStampBlendMode();
    const float zoom = Camera::GetZoom();
    const float ambient = Clamp01(1.0f - lightMaskParams.ambientDarknessMax / 255.0f);

    auto screenRectOf = [&](const GameObject* o) {
        const Vec2 tl = WorldToScreen(Vec2(o->box.x, o->box.y));
        return SDL_Rect{static_cast<int>(std::floor(tl.x)), static_cast<int>(std::floor(tl.y)),
                        static_cast<int>(std::ceil(o->box.w * zoom)) + 1,
                        static_cast<int>(std::ceil(o->box.h * zoom)) + 1};
    };
    auto grown = [](SDL_Rect r, float frac) {   // + borda do contorno de interação
        const int dx = static_cast<int>(std::ceil(r.w * frac));
        const int dy = static_cast<int>(std::ceil(r.h * frac));
        return SDL_Rect{r.x - dx, r.y - dy, r.w + dx * 2, r.h + dy * 2};
    };

    // Cópia da cena como está agora (ordem certa, escuridão exata).
    const bool canRestore = EnsureSceneAux(renderer, renderTarget, sceneSnapshot, occluderScratch);
    if (canRestore) {
        int sw = 0, sh = 0;
        SDL_QueryTexture(renderTarget, nullptr, nullptr, &sw, &sh);
        const SDL_Rect full{0, 0, sw, sh};
        SDL_SetRenderTarget(renderer, sceneSnapshot);
        SDL_SetTextureBlendMode(renderTarget, SDL_BLENDMODE_NONE);
        SDL_RenderCopy(renderer, renderTarget, &full, &full);
        SDL_SetRenderTarget(renderer, renderTarget);
    }

    auto repaintOccluder = [&](const DrawnSprite& d, const SDL_Rect& area) {
        if (canRestore) {
            RestoreSpriteFromSnapshot(renderer, renderTarget, sceneSnapshot, occluderScratch, d.sprite, area);
            return;
        }
        const SDL_Color prev = d.sprite->GetTint();
        const float light = Clamp01(LightAmountAtScreen(WorldToScreen(d.obj->box.Center())));
        const Uint8 v = static_cast<Uint8>(255.0f * Clamp01(ambient + (1.0f - ambient) * light));
        SDL_RenderSetClipRect(renderer, &area);
        d.sprite->SetTint(v, v, v, prev.a);
        d.sprite->Render();
        d.sprite->SetTint(prev.r, prev.g, prev.b, prev.a);
        SDL_RenderSetClipRect(renderer, nullptr);
    };

    std::vector<SDL_Rect> dirty;   // áreas já redesenhadas por cima da escuridão
    dirty.reserve(8);

    for (const DrawnSprite& d : drawOrder) {
        if (!d.sprite || !d.obj) continue;

        if (d.stamped) {
            if (d.glow) RenderInteractionGlowIfNeeded(*d.obj);

            d.sprite->SetTint(255, 255, 255, static_cast<Uint8>(d.shown * 255.0f));
            d.sprite->Render();
            d.sprite->SetTint(255, 255, 255, 255);

            if (SDL_Texture* tex = d.sprite->GetTexturePtr()) {
                SDL_BlendMode prev = SDL_BLENDMODE_BLEND;
                SDL_GetTextureBlendMode(tex, &prev);
                if (SDL_SetTextureBlendMode(tex, stampBlend) == 0) {
                    d.sprite->SetTint(255, 255, 255, static_cast<Uint8>(1.0f + 254.0f * Clamp01(d.light)));
                    d.sprite->Render();
                    d.sprite->SetTint(255, 255, 255, 255);
                }
                SDL_SetTextureBlendMode(tex, prev);
            }
            const SDL_Rect r = screenRectOf(d.obj);
            dirty.push_back(d.glow ? grown(r, 0.10f) : r);
            continue;
        }

        // Não carimbado com contorno (barril, castiçal…): contorno por cima e o sprite de volta.
        if (d.glow) {
            RenderInteractionGlowIfNeeded(*d.obj);
            repaintOccluder(d, screenRectOf(d.obj));
            dirty.push_back(grown(screenRectOf(d.obj), 0.10f));
            continue;
        }

        // Não carimbado: só volta se estiver na frente de uma área redesenhada.
        if (dirty.empty() || d.sprite->GetTint().a == 0) continue;   // alfa 0 = escondido de propósito
        const SDL_Rect mine = screenRectOf(d.obj);
        SDL_Rect cover{0, 0, 0, 0};
        bool covers = false;
        for (const SDL_Rect& r : dirty) {
            SDL_Rect inter;
            if (!SDL_IntersectRect(&mine, &r, &inter)) continue;
            if (!covers) { cover = inter; covers = true; }
            else         { SDL_UnionRect(&cover, &inter, &cover); }
        }
        if (covers) repaintOccluder(d, cover);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Tela: pós-processo e efeitos
// ═════════════════════════════════════════════════════════════════════════════

// Copia a cena para a tela (sem mistura: os carimbos zeraram o alfa dos irmãos)
// e aplica o ScenePostFx por cima — P&B fora da visão, gama e ponto de preto do
// brilho. A cópia vai sempre antes: monta o viewport do letterbox e garante
// imagem se o shader não existir. True se o pós-processo desenhou.
bool StageState::PresentScene(SDL_Renderer* renderer, int winW, int winH) {
    SDL_SetTextureBlendMode(renderTarget, SDL_BLENDMODE_NONE);
    SDL_RenderCopy(renderer, renderTarget, nullptr, nullptr);

    if (!scenePostFx) {
        scenePostFx = std::make_unique<ScenePostFx>();
        scenePostFx->Init(renderer);
    }
    if (!scenePostFx->IsAvailable()) return false;
    scenePostFx->SetGamma(Game::BrightnessGamma());
    scenePostFx->SetBlackPoint(Game::BrightnessBlackPoint());
    return scenePostFx->Render(renderer, renderTarget, winW, winH, visionFrame, visionParams);
}

// Sem shader, o brilho vira um véu: preto abaixo de 100, cinza aditivo acima.
void StageState::RenderBrightnessFallback(SDL_Renderer* renderer, int winW, int winH) {
    const int b = Game::brightnessPercent;
    if (b < 100) {
        FillScreen(renderer, winW, winH, SDL_BLENDMODE_BLEND, 0, 0, 0,
                   static_cast<Uint8>((100 - b) / 100.0f * 175.0f));
    } else if (b > 100) {
        FillScreen(renderer, winW, winH, SDL_BLENDMODE_ADD, 120, 120, 120,
                   static_cast<Uint8>((b - 100) / 50.0f * 95.0f));
    }
}

// Clarão vermelho de dano e clarão do trovão (os dois atenuados com "Reduzir flashes").
void StageState::RenderScreenFlashes(SDL_Renderer* renderer, int winW, int winH) {
    if (damageFlashTimer > 0.0f) {
        const float mul = Game::reduceFlashing ? 0.35f : 1.0f;
        const float t = damageFlashTimer / kDamageFlashDuration;
        FillScreen(renderer, winW, winH, SDL_BLENDMODE_BLEND, 150, 10, 10,
                   static_cast<Uint8>(std::min(255.0f, 150.0f * t * mul)));
    }
    const float thunder = GameSfx::GetThunderFlashStrength();
    if (thunder > 0.01f) {
        const float mul = Game::reduceFlashing ? 0.25f : 1.0f;
        FillScreen(renderer, winW, winH, SDL_BLENDMODE_ADD, 205, 215, 255,
                   static_cast<Uint8>(std::min(255.0f, 230.0f * thunder * mul)));
    }
}

// Overlay de sanidade com aberração cromática: cópia vermelha à esquerda e
// azul à direita, afastadas pela intensidade (menos com "Reduzir flashes").
void StageState::RenderSanityAberration(SDL_Renderer* renderer) {
    SpriteRenderer* sprite = sanityOverlayObj ? sanityOverlayObj->GetComponent<SpriteRenderer>() : nullptr;
    if (!sprite || sanityOverlaySmoothedIntensity <= 0.001f) return;

    const float mul = Game::reduceFlashing ? 0.3f : 1.0f;
    const float offset = kChromaticAberrationMaxOffsetPx * sanityOverlaySmoothedIntensity * mul;
    const Rect base = sanityOverlayObj->box;
    const Uint8 alpha = static_cast<Uint8>(std::min(255.0f, 255.0f * sanityOverlaySmoothedIntensity));

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
    sanityOverlayObj->box.x = base.x - offset;
    sprite->SetTint(255, 30, 30, alpha);
    sprite->Render();
    sanityOverlayObj->box.x = base.x + offset;
    sprite->SetTint(30, 30, 255, alpha);
    sprite->Render();
    sanityOverlayObj->box = base;
    sprite->SetTint(255, 255, 255, 255);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

// ═════════════════════════════════════════════════════════════════════════════
//  HUD
// ═════════════════════════════════════════════════════════════════════════════

// Bola do poder do irmãozinho (canto inferior esquerdo, no lugar da roda):
// cheia = pronto, esvaziando = em uso, enchendo = recarga. Ao lado, "[F] Usar",
// vermelho enquanto indisponível.
void StageState::RenderLittleBrotherPowerHud(SDL_Renderer* renderer, int winW, int winH) {
    Character* small = Character::littleBrother;
    if (!small || controlledCharacter != small) return;

    constexpr int kRadius   = 40;
    constexpr int kSegments = 48;
    const float powerTimer = small->visionPowerTimer;
    const float cooldown   = small->visionCooldown;
    const bool active = powerTimer > 0.0f;
    const bool recharging = !active && cooldown > 0.0f;

    float fill = 1.0f;
    if (active)          fill = powerTimer / Character::kVisionDuration;
    else if (recharging) fill = 1.0f - cooldown / Character::kVisionCooldown;

    const int cx = static_cast<int>(winW * 0.12f + 32.0f);
    const int cy = static_cast<int>(winH - 100.0f);

    // Paleta: roxo em uso, cinza recarregando, branco pronto.
    const SDL_Color base   = active ? SDL_Color{180, 80, 255, 255} : recharging ? SDL_Color{100, 100, 120, 255} : SDL_Color{255, 255, 255, 255};
    const SDL_Color edge   = active ? SDL_Color{220, 140, 255, 210} : recharging ? SDL_Color{160, 160, 180, 180} : SDL_Color{255, 255, 255, 210};
    const SDL_Color center = active ? SDL_Color{40, 0, 80, 180}     : recharging ? SDL_Color{20, 20, 30, 160}    : SDL_Color{40, 40, 60, 180};

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Fundo: anéis do preto (centro) até a cor base (borda).
    for (int r = 0; r <= kRadius; r++) {
        const float t = static_cast<float>(r) / kRadius;
        const float t2 = t * t;
        SDL_SetRenderDrawColor(renderer, static_cast<Uint8>(base.r * t2), static_cast<Uint8>(base.g * t2),
                               static_cast<Uint8>(base.b * t2), static_cast<Uint8>(200.0f + 55.0f * t2));
        DrawCircleOutline(renderer, cx, cy, r, kSegments);
    }

    // Preenchimento em fatias a partir do topo, centro escuro → borda clara.
    const float start = -kPi / 2.0f;
    const int fillSegs = static_cast<int>(fill * kSegments);
    SDL_Vertex v[3];
    for (int i = 0; i < fillSegs; i++) {
        const float a0 = start + (static_cast<float>(i) / kSegments) * 2.0f * kPi;
        const float a1 = start + (static_cast<float>(i + 1) / kSegments) * 2.0f * kPi;
        v[0] = {{static_cast<float>(cx), static_cast<float>(cy)}, center, {0, 0}};
        v[1] = {{cx + std::cos(a0) * kRadius, cy + std::sin(a0) * kRadius}, edge, {0, 0}};
        v[2] = {{cx + std::cos(a1) * kRadius, cy + std::sin(a1) * kRadius}, edge, {0, 0}};
        SDL_RenderGeometry(renderer, nullptr, v, 3, nullptr, 0);
    }

    SDL_SetRenderDrawColor(renderer, 200, 200, 220, 255);
    DrawCircleOutline(renderer, cx, cy, kRadius, kSegments);

    // "[F] Usar"
    constexpr int kIconSize = 54;
    constexpr int kIconGap  = 20;
    const bool available = !active && !recharging;
    const SDL_Color kReady{235, 225, 195, 255};
    const SDL_Color kBusy{220, 55, 45, 255};
    const int iconX = cx + kRadius + kIconGap;
    const int iconY = cy - kIconSize / 2;

    if (auto keyTex = Resources::GetImage("Recursos/img/hud/key_f.png")) {
        const SDL_Rect dst{iconX, iconY, kIconSize, kIconSize};
        SDL_RenderCopy(renderer, keyTex.get(), nullptr, &dst);
        if (!available) {
            SDL_SetRenderDrawColor(renderer, kBusy.r, kBusy.g, kBusy.b, 255);
            for (int t = 1; t <= 3; ++t) {
                const SDL_Rect ol{iconX - t, iconY - t, kIconSize + 2 * t, kIconSize + 2 * t};
                SDL_RenderDrawRect(renderer, &ol);
            }
        }
    }
    if (auto font = Resources::GetFont(kUiFont, 22)) {
        int tw = 0, th = 0;
        TTF_SizeUTF8(font.get(), "Usar", &tw, &th);
        DrawLine(renderer, font.get(), "Usar", iconX + kIconSize / 2 - tw / 2, iconY + kIconSize + 2,
                 available ? kReady : kBusy);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
}

// Seta sobre o irmão controlado ao trocar: quica, pisca dourado↔branco e some.
void StageState::RenderControlIndicator(SDL_Renderer* renderer) {
    if (!controlledCharacter || controlIndicatorTimer <= 0.0f) return;

    const float zoom = Camera::GetZoom();
    const Rect& box = controlledCharacter->GetAssociated().box;
    const int cx = static_cast<int>((box.x - Camera::pos.x) * zoom) + static_cast<int>(box.w * zoom) / 2;
    const int screenY = static_cast<int>((box.y - Camera::pos.y) * zoom);
    const float elapsed = kControlIndicatorDuration - controlIndicatorTimer;

    float a01 = 1.0f;   // entra em 0,25 s, sai nos últimos 0,7 s
    if (controlIndicatorTimer < 0.7f) a01 = controlIndicatorTimer / 0.7f;
    else if (elapsed < 0.25f)         a01 = elapsed / 0.25f;
    a01 = a01 * a01 * (3.0f - 2.0f * a01);

    const float flash = 0.5f + 0.5f * std::sin(elapsed * 14.0f);
    const int w = static_cast<int>(22 * zoom);
    const int h = static_cast<int>(16 * zoom);
    const int topY = screenY - static_cast<int>(30 * zoom) + static_cast<int>(std::sin(elapsed * 9.0f) * 7.0f * zoom);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, static_cast<Uint8>(235 + 20 * flash), static_cast<Uint8>(205 + 50 * flash),
                           static_cast<Uint8>(90 + 165 * flash), static_cast<Uint8>(235 * a01));
    for (int row = 0; row < h; ++row) {
        const int halfW = static_cast<int>((w / 2) * (1.0f - static_cast<float>(row) / h));
        SDL_RenderDrawLine(renderer, cx - halfW, topY + row, cx + halfW, topY + row);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
}

// Tela preta do conserto da escada (cobre até o HUD). Um Repairable por vez.
void StageState::RenderRepairOverlay(SDL_Renderer* renderer, int winW, int winH) {
    for (const auto& goPtr : objectArray) {
        Repairable* rep = goPtr->GetComponent<Repairable>();
        if (!rep) continue;
        const float alpha = rep->GetRepairOverlayAlpha();
        if (alpha > 0.001f) {
            FillScreen(renderer, winW, winH, SDL_BLENDMODE_BLEND, 0, 0, 0,
                       static_cast<Uint8>(std::min(255.0f, alpha * 255.0f)));
            return;
        }
    }
}

// Legenda da fala tocando agora: caixa escura no rodapé, texto quebrado em 70% da tela.
void StageState::RenderVoiceSubtitle(SDL_Renderer* renderer) {
    std::string caption;
    if (!renderer || !GameVoice::GetActiveSubtitle(caption)) return;

    const int winW = Game::GetInstance().GetWindowsWidth();
    const int winH = Game::GetInstance().GetWindowsHeight();
    auto font = Resources::GetFont(kUiFont, 26);
    if (!font) return;

    SDL_Surface* sf = TTF_RenderUTF8_Blended_Wrapped(font.get(), caption.c_str(), SDL_Color{235, 232, 220, 255},
                                                     static_cast<Uint32>(winW * 0.7f));
    if (!sf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, sf);
    const int tw = sf->w, th = sf->h;
    SDL_FreeSurface(sf);
    if (!tex) return;

    constexpr int kPad = 14;
    const SDL_Rect bg{(winW - tw - kPad * 2) / 2, winH - th - kPad * 2 - 48, tw + kPad * 2, th + kPad * 2};
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
    SDL_RenderFillRect(renderer, &bg);
    const SDL_Rect dst{bg.x + kPad, bg.y + kPad, tw, th};
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Debug
// ═════════════════════════════════════════════════════════════════════════════

// Canto superior direito: estado dos atalhos de debug (verde = ligado) e, com
// [B], a legenda das cores de colisão.
void StageState::RenderDebugStatus(SDL_Renderer* renderer, int winW) {
    auto font = Resources::GetFont(kUiFont, 18);
    if (!font) return;

    struct Toggle { const char* label; bool on; };
    const Toggle toggles[] = {
        {"[B] Colisao/fisica: ",       showMapPhysicsDebug},
        {"[I] Invisivel p/ monstro: ", debugMonsterBlind},
        {"[G] Camera livre: ",         debugFreeCam},
    };
    int y = 8;
    for (const Toggle& t : toggles) {
        const std::string text = std::string(t.label) + (t.on ? "ON" : "OFF");
        const SDL_Color col = t.on ? SDL_Color{80, 255, 80, 255} : SDL_Color{170, 170, 170, 220};
        int tw = 0, th = 0;
        TTF_SizeUTF8(font.get(), text.c_str(), &tw, &th);
        if (DrawLine(renderer, font.get(), text, winW - tw - 10, y, col) > 0) y += th + 4;
    }
    if (!showMapPhysicsDebug) return;

    struct LegendItem { Uint8 r, g, b; const char* label; };
    const LegendItem legend[] = {
        {255,   0,   0, "Monstro: HURTBOX (dano ao irmao)"},
        {  0, 255, 120, "Monstro: colisao de NAV (CIRCULO)"},
        {255, 255,   0, "Monstro: bounds do sprite"},
        {255,   0, 255, "Monstro: collider (componente)"},
        {  0, 220, 255, "Caixa/barril (empurravel)"},
        {255, 190,  70, "Irmaozao: pe/colisao (CIRCULO amarelo)"},
        { 90, 255, 200, "Irmaozinho: pe/colisao (CIRCULO verde-agua)"},
        {255,  60,  60, "Jogador: HITBOX de dano (CIRCULO vermelho)"},
        {255,  60,  60, "Mapa: paredes/colisao estatica"},
        {  0, 220, 255, "Mapa: chao/escada"},
    };
    auto small = Resources::GetFont(kUiFont, 15);
    y += 6;
    for (const LegendItem& li : legend) {
        int tw = 0, th = 14;
        if (small) TTF_SizeUTF8(small.get(), li.label, &tw, &th);
        const int sw = th;                          // quadradinho do tamanho da altura do texto
        const int left = winW - (sw + 6 + tw) - 8;

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
        const SDL_Rect bg{left - 4, y - 1, sw + 6 + tw + 8, th + 2};
        SDL_RenderFillRect(renderer, &bg);
        SDL_SetRenderDrawColor(renderer, li.r, li.g, li.b, 255);
        const SDL_Rect swatch{left, y, sw, th};
        SDL_RenderFillRect(renderer, &swatch);
        DrawLine(renderer, small.get(), li.label, winW - tw - 8, y, SDL_Color{225, 225, 230, 235});
        y += th + 3;
    }
}

// Colliders em arame, coloridos por tipo (irmãozão, irmãozinho, monstro, caixa,
// cenário). Nos irmãos, em vez do Collider: caixa dos pés e hurtbox.
void StageState::RenderGameplayCollisionDebug(SDL_Renderer* renderer) const {
    if (!renderer) return;
    const float z = Camera::GetZoom();

    auto drawWorldRect = [&](const SDL_Rect& wr, Uint8 r, Uint8 g, Uint8 b) {
        const Vec2 tl = WorldToScreen(Vec2(static_cast<float>(wr.x), static_cast<float>(wr.y)));
        const SDL_FRect sr{tl.x, tl.y, wr.w * z, wr.h * z};
        SDL_SetRenderDrawColor(renderer, r, g, b, 210);
        SDL_RenderDrawRectF(renderer, &sr);
    };

    for (const auto& goPtr : objectArray) {
        GameObject* go = goPtr.get();
        Collider* col = go ? go->GetComponent<Collider>() : nullptr;
        if (!col) continue;
        const bool isBig = (go == bigCharacterObject);
        const bool isSmall = (go == smallCharacterObject);

        if (!isBig && !isSmall) {
            SDL_Color c{255, 215, 0, 255};                                   // cenário
            if (go->GetComponent<Monster>())  c = {255, 0, 255, 255};
            else if (go->GetComponent<Box>()) c = {0, 220, 255, 255};
            DrawColliderDebugWire(renderer, col->box, static_cast<float>(go->angleDeg), c.r, c.g, c.b, 215);
        }
        if (Character* ch = go->GetComponent<Character>()) {
            if (isBig)        drawWorldRect(ch->GetFootRect(), 255, 240, 60);
            else if (isSmall) drawWorldRect(ch->GetFootRect(), 60, 255, 180);
            else              drawWorldRect(ch->GetFootRect(), 80, 255, 120);
            drawWorldRect(ch->GetHitRect(), 255, 60, 60);
        }
    }
}

// Rota que o seguidor usa neste frame (A* ou linha reta), em amarelo, com os waypoints.
void StageState::RenderCompanionFollowPathDebug(SDL_Renderer* renderer) const {
    if (!renderer || companionFollowPathWorld.size() < 2) return;

    const float z = Camera::GetZoom();
    SDL_BlendMode oldBlend;
    SDL_GetRenderDrawBlendMode(renderer, &oldBlend);
    Uint8 dr, dg, db, da;
    SDL_GetRenderDrawColor(renderer, &dr, &dg, &db, &da);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 255, 235, 70, 210);
    for (size_t i = 1; i < companionFollowPathWorld.size(); i++) {
        const Vec2 a = WorldToScreen(companionFollowPathWorld[i - 1]);
        const Vec2 b = WorldToScreen(companionFollowPathWorld[i]);
        SDL_RenderDrawLineF(renderer, a.x, a.y, b.x, b.y);
    }
    for (const Vec2& wp : companionFollowPathWorld) {
        const Vec2 sp = WorldToScreen(wp);
        DrawDebugCircle(renderer, sp.x, sp.y, std::max(2.5f, 4.0f * z), 255, 220, 50, 175);
    }
    SDL_SetRenderDrawBlendMode(renderer, oldBlend);
    SDL_SetRenderDrawColor(renderer, dr, dg, db, da);
}