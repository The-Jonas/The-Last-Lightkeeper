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
#include "gameplay/Monster.h"
#include "ui/FadeEffect.h"
#include "gameplay/Repairable.h"
#include "gameplay/StairTrigger.h"
#include "core/Resources.h"
#include "audio/GameSfx.h"
#include "audio/GameVoice.h"
#include "gameplay/Window.h"
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
void StageState::Render(){
    SDL_Renderer* renderer = Game::GetInstance().GetRenderer();
    int winW = Game::GetInstance().GetWindowsWidth();
    int winH = Game::GetInstance().GetWindowsHeight();

    // Já em transição com o quadro congelado: desenha só o efeito zoom-blur.
    if (sceneTransitionActive && sceneTransitionFrame) {
        RenderSceneTransition(renderer);
        return;
    }

    // Redireciona todos os desenhos a partir de agora para a nossa textura!
    SDL_SetRenderTarget(renderer, renderTarget);

    // 1. PINTA O VAZIO DE PRETO
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // 2. DESENHA TODO O CENÁRIO (Parede, Chão)
    level.RenderBackground(renderer);

    // ============================
    // 3. ORDENAÇÃO Z/Y SORTING
    // ============================
    auto compareObjects = [](const std::shared_ptr<GameObject>& a, const std::shared_ptr<GameObject>& b) {
        
        // 1. Z-Sort Absoluto
        if (a->z != b->z) return a->z < b->z;

        // 2. Y-Sort Relativo (Calcula a base real)
        float base_a = (a->owner != nullptr) ? (a->owner->box.y + a->owner->box.h) : (a->box.y + a->box.h);
        float base_b = (b->owner != nullptr) ? (b->owner->box.y + b->owner->box.h) : (b->box.y + b->box.h);
        
        float sortingY_a = base_a + a->depthOffset;
        float sortingY_b = base_b + b->depthOffset;

        // =========================================================
        // A MÁGICA ISOMÉTRICA (ANCORAGEM DE PROFUNDIDADE)
        // =========================================================
        Character* charA = a->GetComponent<Character>();
        Character* charB = b->GetComponent<Character>();
        
        bool a_isElevated = (charA && charA->isElevated);
        bool b_isElevated = (charB && charB->isElevated);

        // Se AMBOS estão na escada, nós ignoramos a âncora falsa. 
        // Deixamos eles usarem o Y real (base_a e base_b) para que 
        // quem estiver no degrau de baixo seja desenhado na frente!
        if (!(a_isElevated && b_isElevated)) {
            // Caso contrário (seja um personagem contra o cenário, ou um personagem na escada 
            // e outro no chão), nós forçamos a âncora da escada.
            if (a_isElevated) sortingY_a = charA->stairAnchorY; 
            if (b_isElevated) sortingY_b = charB->stairAnchorY;
        }

        float epsilon = 0.01f; 
        if (std::abs(sortingY_a - sortingY_b) > epsilon) {
            return sortingY_a < sortingY_b;
        }
        
        // 3. Fallback
        return a->sub_z < b->sub_z;
    };
    std::sort(objectArray.begin(), objectArray.end(), compareObjects);

    // ===================================================================
    // 4. LUZES E SOMBRAS VÃO PARA O CHÃO (Antes de desenhar os sprites!)
    // ===================================================================
    Game& g = Game::GetInstance();
    const bool showDebugTools = (lightTweakPanel && lightTweakPanel->visible);
    if (lightsEnabled) {
        LightShadowProfile::BeginLightsTiming();
    }

    const bool playerWantsLightHidden = Character::player && Character::player->hidePersonalLight;
    const bool lighterFromInventory =
        inventory.IsActiveLightLighter() && !playerWantsLightHidden;
    const bool torchIsActuallyLit = inventory.IsUsableLightActive();
    const bool durabilityOn = lightTweakPanel ? lightTweakPanel->durabilityEnabled : true;
    const LightMaskParams lighterLightParams =
        lighterFromInventory && durabilityOn ? inventory.BuildLighterLightParams(lightMaskParams) : lightMaskParams;

    const bool bigCircleOnlyLight =
        cursorPreviewLightEnabled && previewLightLockedToPlayer && previewLightAnchorPlayer == bigCharacterObject;
    const bool smallCircleOnlyLight =
        cursorPreviewLightEnabled && previewLightLockedToPlayer && previewLightAnchorPlayer == smallCharacterObject;
    float bigMaxContact = 0.0f;
    float smallMaxContact = 0.0f;
    float bigMaxTouch = 0.0f;
    float smallMaxTouch = 0.0f;

    if (lightsEnabled && shadowsEnabled) {
        Uint64 shadowBlockStart = 0;
        if (LightShadowProfile::IsActive()) {
            shadowBlockStart = SDL_GetPerformanceCounter();
        }
        struct SpriteShadowCast {
            Vec2 lightScreen;
            float touch = 0.0f;
            float lengthPx = 0.0f;
            Uint8 alpha = 0;
            float contact = 0.0f;
        };
        std::vector<SpriteShadowCast> bigShadowCasts;
        std::vector<SpriteShadowCast> smallShadowCasts;
        bigShadowCasts.reserve(6);
        smallShadowCasts.reserve(6);

        auto renderShadowsForLight = [&](const Vec2& lightScreen, const LightMaskParams& params) {
            float touchBig = 0.0f;
            float touchSmall = 0.0f;
            IsFootLit(bigCharacterObject, lightScreen, params, &touchBig);
            IsFootLit(smallCharacterObject, lightScreen, params, &touchSmall);
            const float distBig = 1.0f - touchBig;
            const float distSmall = 1.0f - touchSmall;

            float dBigPx = 0.0f, maxBigPx = 1.0f;
            float dSmallPx = 0.0f, maxSmallPx = 1.0f;
            if (bigCharacterObject) {
                const Rect& b = bigCharacterObject->box;
                const Vec2 foot((b.x + 0.5f * b.w - Camera::pos.x) * Camera::GetZoom(),
                                (b.y + b.h - Camera::pos.y) * Camera::GetZoom());
                ComputeShadowDistanceRate(foot, lightScreen, params, &dBigPx, &maxBigPx);
            }
            if (smallCharacterObject) {
                const Rect& b = smallCharacterObject->box;
                const Vec2 foot((b.x + 0.5f * b.w - Camera::pos.x) * Camera::GetZoom(),
                                (b.y + b.h - Camera::pos.y) * Camera::GetZoom());
                ComputeShadowDistanceRate(foot, lightScreen, params, &dSmallPx, &maxSmallPx);
            }

            const float bigContactRadiusPx = std::max(6.0f, maxBigPx * 0.07f);
            const float smallContactRadiusPx = std::max(6.0f, maxSmallPx * 0.07f);
            const float bigContact = (dBigPx <= bigContactRadiusPx) ? Clamp01(1.0f - dBigPx / bigContactRadiusPx) : 0.0f;
            const float smallContact = (dSmallPx <= smallContactRadiusPx) ? Clamp01(1.0f - dSmallPx / smallContactRadiusPx) : 0.0f;
            bigMaxContact = std::max(bigMaxContact, bigContact);
            smallMaxContact = std::max(smallMaxContact, smallContact);

            // ── Iluminação para a SANIDADE ────────────────────────────────
            // O `touch` acima (medido só no PÉ, com raio de sombra reduzido) é
            // ótimo para as sombras, mas cruel para a sanidade: o jogador podia
            // estar visivelmente DENTRO do círculo de luz do castiçal e mesmo
            // assim tomar dano "no escuro". Aqui medimos de forma generosa e
            // coerente com o brilho VISÍVEL: pegamos o ponto do CORPO mais perto
            // da luz (pé ou centro) e um raio casado com o falloff visível,
            // compensando o zoom da câmera (o `touch` de sombra não compensa e
            // por isso encolhia a área "iluminada" quando a câmera dava zoom).
            constexpr float kSanityLitRadiusFrac = 1.25f;
            auto sanityIllum = [&](GameObject* obj) -> float {
                if (!obj) return 0.0f;
                const Rect& bx = obj->box;
                const float z = Camera::GetZoom();
                const Vec2 footPt((bx.x + 0.5f * bx.w - Camera::pos.x) * z,
                                  (bx.y + bx.h - Camera::pos.y) * z);
                const Vec2 midPt((bx.x + 0.5f * bx.w - Camera::pos.x) * z,
                                 (bx.y + 0.5f * bx.h - Camera::pos.y) * z);
                const float d = std::min(footPt.Distance(lightScreen), midPt.Distance(lightScreen));
                const float litRadius = std::max(8.0f, params.falloffRadiusPx) * z * kSanityLitRadiusFrac;
                return Clamp01(1.0f - d / std::max(1.0f, litRadius));
            };

            // Captura o nível real de luz batendo no sprite (para sombras) e a
            // iluminação generosa (para a sanidade).
            bigMaxTouch = std::max(bigMaxTouch, std::max(touchBig, sanityIllum(bigCharacterObject)));
            smallMaxTouch = std::max(smallMaxTouch, std::max(touchSmall, sanityIllum(smallCharacterObject)));

            if (touchBig > 0.0f) {
                const float shadowLengthPx = params.shadowMaxLengthPx * distBig;
                const Uint8 shadowAlpha = static_cast<Uint8>(std::max(0.0f, std::min(255.0f, params.darknessMax * touchBig)));
                bigShadowCasts.push_back({lightScreen, touchBig, shadowLengthPx, shadowAlpha, bigContact});
            }
            if (touchSmall > 0.0f) {
                const float shadowLengthPx = params.shadowMaxLengthPx * distSmall;
                const Uint8 shadowAlpha = static_cast<Uint8>(std::max(0.0f, std::min(255.0f, params.darknessMax * touchSmall)));
                smallShadowCasts.push_back({lightScreen, touchSmall, shadowLengthPx, shadowAlpha, smallContact});
            }
        };

        if (cursorPreviewLightEnabled) {
            renderShadowsForLight(smoothedDynamicLightScreenPos, lightMaskParams);
        }

        if (lighterFromInventory && hasSmoothedTorchLight) {
            renderShadowsForLight(smoothedTorchLightScreenPos, lighterLightParams);
        }

        if (showDebugTools && cursorPreviewLightEnabled) {
            const float previewShadowRadius = std::max(24.0f, std::max(8.0f, lightMaskParams.falloffRadiusPx) * std::max(0.4f, lightMaskParams.fatorDicaDeRaio));
            DrawDebugCircle(g.GetRenderer(), smoothedDynamicLightScreenPos.x, smoothedDynamicLightScreenPos.y, previewShadowRadius, 255, 210, 90, 130);
            if (lighterFromInventory && hasSmoothedTorchLight) {
                const float previewShadowRadius = std::max(
                    24.0f, std::max(8.0f, lighterLightParams.falloffRadiusPx) *
                               std::max(0.4f, lighterLightParams.fatorDicaDeRaio));
                DrawDebugCircle(g.GetRenderer(), smoothedTorchLightScreenPos.x, smoothedTorchLightScreenPos.y,
                                previewShadowRadius * 0.85f, 255, 150, 70, 150);
            }
        } else if (showDebugTools && lighterFromInventory && hasSmoothedTorchLight) {
            const float previewShadowRadius = std::max(
                24.0f, std::max(8.0f, lighterLightParams.falloffRadiusPx) *
                           std::max(0.4f, lighterLightParams.fatorDicaDeRaio));
            DrawDebugCircle(g.GetRenderer(), smoothedTorchLightScreenPos.x, smoothedTorchLightScreenPos.y,
                            previewShadowRadius * 0.85f, 255, 150, 70, 150);
        }

        int renderedLights = 0;
        for (const LightInstance& light : lights) {
            if (!light.enabled) continue;
            if (renderedLights >= maxActiveLights) break;

            const Vec2 lightScreen = WorldToScreen(light.worldPos);
            const float cullRadius = std::max(32.0f, light.params.falloffRadiusPx * 1.6f);
            if (lightScreen.x < -cullRadius || lightScreen.y < -cullRadius ||
                lightScreen.x > static_cast<float>(g.GetWindowsWidth()) + cullRadius ||
                lightScreen.y > static_cast<float>(g.GetWindowsHeight()) + cullRadius) {
                continue;
            }

            renderShadowsForLight(lightScreen, light.params);
            if (showDebugTools) {
                const float placedShadowRadius = std::max(24.0f, std::max(8.0f, light.params.falloffRadiusPx) * std::max(0.4f, light.params.fatorDicaDeRaio));
                DrawDebugCircle(g.GetRenderer(), lightScreen.x, lightScreen.y, placedShadowRadius, 120, 220, 255, 95);
            }
            renderedLights++;
        }

        constexpr size_t kMaxSpriteShadowsPerPlayer = 1;
        std::sort(bigShadowCasts.begin(), bigShadowCasts.end(), [](const SpriteShadowCast& a, const SpriteShadowCast& b) { return a.touch > b.touch; });
        std::sort(smallShadowCasts.begin(), smallShadowCasts.end(), [](const SpriteShadowCast& a, const SpriteShadowCast& b) { return a.touch > b.touch; });
        
        if (bigShadowCasts.size() > kMaxSpriteShadowsPerPlayer) bigShadowCasts.resize(kMaxSpriteShadowsPerPlayer);
        if (smallShadowCasts.size() > kMaxSpriteShadowsPerPlayer) smallShadowCasts.resize(kMaxSpriteShadowsPerPlayer);

        const bool bigHidden = Character::player && Character::player->hidePersonalLight;
        const bool smallHidden = Character::littleBrother && Character::littleBrother->hidePersonalLight;
        
        for (const SpriteShadowCast& c : bigShadowCasts) {
            if (!bigCircleOnlyLight && c.contact < 0.50f && !bigHidden) {
                RenderProjectedSpriteShadow(bigCharacterObject, c.lightScreen, c.touch, c.lengthPx, c.alpha, lightMaskParams);
            }
        }
        for (const SpriteShadowCast& c : smallShadowCasts) {
            if (!smallCircleOnlyLight && c.contact < 0.50f && !smallHidden) {
                RenderProjectedSpriteShadow(smallCharacterObject, c.lightScreen, c.touch, c.lengthPx, c.alpha, lightMaskParams);
            }
        }
        
        UpdateControlledCharacterVisuals(); // Restaura as cores originais pós-sombra

        if (showDebugTools) {
            DrawPlayerShadowTouchDebug(g.GetRenderer(), bigCharacterObject, 255, 120, 120);
            DrawPlayerShadowTouchDebug(g.GetRenderer(), smallCharacterObject, 130, 220, 255);
        }

        if (cursorPreviewLightEnabled && previewLightLockedToPlayer && previewLightAnchorPlayer) {
            if (bigCharacterObject && previewLightAnchorPlayer == bigCharacterObject) {
                bigMaxContact = std::max(bigMaxContact, 0.92f);
            }
            if (smallCharacterObject && previewLightAnchorPlayer == smallCharacterObject) {
                smallMaxContact = std::max(smallMaxContact, 0.92f);
            }
        }

        if (torchIsActuallyLit && bigCharacterObject) {
            bigMaxContact = std::max(bigMaxContact, 0.92f);
        }
        if (torchIsActuallyLit && smallCharacterObject) {
            smallMaxContact = std::max(smallMaxContact, 0.92f);
        }
        

    if (bigCharacterObject && bigMaxContact > 0.0f && !bigHidden) {
        DrawContactFootShadow(g.GetRenderer(), bigCharacterObject->box, bigMaxContact);
    }
    if (smallCharacterObject && smallMaxContact > 0.0f && !smallHidden) {
        DrawContactFootShadow(g.GetRenderer(), smallCharacterObject->box, smallMaxContact);
    }
        if (LightShadowProfile::IsActive()) {
            const Uint64 shadowBlockEnd = SDL_GetPerformanceCounter();
            const double ms = static_cast<double>(shadowBlockEnd - shadowBlockStart) * 1000.0 /
                              static_cast<double>(SDL_GetPerformanceFrequency());
            LightShadowProfile::SetSpriteShadowBlockMs(ms);
        }
    }

    // ===================================================================
    // 4.5 PRÉ-CALCULA AS LUZES DA TELA E CARIMBA SOMBRAS DOS OBJETOS
    // ===================================================================
    std::vector<RadialLightOverlay::ScreenLight> screenLights;

    if (lightsEnabled && radialGeometry != nullptr) {
        screenLights.reserve(static_cast<size_t>(maxActiveLights + 2));
        constexpr float kCursorLightBlend = 0.28f;
        constexpr float kTorchLightBlend = 0.28f;

        if (cursorPreviewLightEnabled) {
            screenLights.push_back({smoothedDynamicLightScreenPos.x, smoothedDynamicLightScreenPos.y, lightMaskShape,
                                    lightMaskParams, kCursorLightBlend});
        }
        if (lighterFromInventory && hasSmoothedTorchLight) {
            screenLights.push_back({smoothedTorchLightScreenPos.x, smoothedTorchLightScreenPos.y, lightMaskShape,
                                    lighterLightParams, kTorchLightBlend});
        }
        
        int renderedLights = 0;
        for (LightInstance& light : lights) {
            if (!light.enabled) continue;
            if (renderedLights >= maxActiveLights) break;

            const Vec2 lightScreen = WorldToScreen(light.worldPos);
            const float cullRadius = std::max(32.0f, light.params.falloffRadiusPx * 1.4f);
            if (lightScreen.x < -cullRadius || lightScreen.y < -cullRadius ||
                lightScreen.x > static_cast<float>(g.GetWindowsWidth()) + cullRadius ||
                lightScreen.y > static_cast<float>(g.GetWindowsHeight()) + cullRadius) {
                continue;
            }
            screenLights.push_back({lightScreen.x, lightScreen.y, light.shape, light.params, light.animationSeed});
            renderedLights++;
        }
        for (GameObject* obj : testShadowObjects) {
            if (!obj) continue;
            if (obj == bigCharacterObject || obj == smallCharacterObject) continue;
            Vec2 bestLightScreen;
            float bestTouch = 0.0f;
            float bestLengthPx = 0.0f;
            Uint8 bestAlpha = 0;

            for (const auto& sl : screenLights) {
                Vec2 lightScreen(sl.x, sl.y);
                float touch = 0.0f;
                IsFootLit(obj, lightScreen, sl.params, &touch);
                if (touch > bestTouch) {
                    bestTouch = touch;
                    bestLightScreen = lightScreen;
                    const float distance01 = Clamp01(1.0f - touch);
                    bestLengthPx = sl.params.shadowMaxLengthPx * distance01;
                    bestAlpha = static_cast<Uint8>(std::max(0.0f, std::min(255.0f, sl.params.darknessMax * touch)));
                }
            }

            if (bestTouch > 0.0f) {
                RenderSingleLightSpriteShadow(obj, bestLightScreen, bestTouch, bestLengthPx, bestAlpha, lastFrameDt);
            }
        }
    }

    // ===================================================================
    // 5. AGORA DESENHAMOS A LISTA ORDENADA INTEIRA SEM REGRAS
    // ===================================================================
    constexpr int kHudZ = 100;
    for (const auto& go : objectArray) {
        if (go->z < kHudZ) {
            RenderInteractionGlowIfNeeded(*go);
            go->Render();
        }
    }

    // ===================================================================
    // 6. MÁSCARA DE ESCURIDÃO GERAL E SOMBRAS DE PAREDE
    // ===================================================================

    if (lightsEnabled && radialGeometry != nullptr) {
    
        LightOcclusionContext occCtx;
        if (tileMapComp && tileSet) {
            occCtx.solidGrid = &tileMapComp->GetLightOcclusionSolid();
            occCtx.mapWidth = tileMapComp->GetWidth();
            occCtx.mapHeight = tileMapComp->GetHeight();
            occCtx.tileWidth = static_cast<float>(tileSet->GetTileWidth());
            occCtx.tileHeight = static_cast<float>(tileSet->GetTileHeight());
            occCtx.mapOriginX = mapOrigin.x;
            occCtx.mapOriginY = mapOrigin.y;
            occCtx.cameraX = Camera::pos.x;
            occCtx.cameraY = Camera::pos.y;
            occCtx.zoom = Camera::GetZoom();
        }
        // Joga a escuridão por cima de tudo (chão + sombras + sprites) respeitando os raios de luz:
        radialGeometry->RenderMany(g.GetRenderer(), g.GetWindowsWidth(), g.GetWindowsHeight(), screenLights, occCtx);


        if (shadowsEnabled && staticShadowEdgesBuilt && !staticShadowEdges.empty()) {
            const std::vector<TopDownShadowEdge> noDynamic;
            const int maxShadowVolumes = shadowsEnabled ? 8 : 0;
            for (int si = 0; si < static_cast<int>(screenLights.size()) && si < maxShadowVolumes; si++) {
                const RadialLightOverlay::ScreenLight& sl = screenLights[si];

                if (occCtx.IsEnabled()) {
                    const float lxWorld = sl.x / occCtx.zoom + occCtx.cameraX;
                    const float lyWorld = sl.y / occCtx.zoom + occCtx.cameraY;
                    const int ltx = static_cast<int>((lxWorld - occCtx.mapOriginX) / occCtx.tileWidth);
                    const int lty = static_cast<int>((lyWorld - occCtx.mapOriginY) / occCtx.tileHeight);
                    if (ltx >= 0 && ltx < occCtx.mapWidth && lty >= 0 && lty < occCtx.mapHeight) {
                        if ((*occCtx.solidGrid)[static_cast<size_t>(ltx + lty * occCtx.mapWidth)] != 0) {
                            continue;
                        }
                    }
                }

                TopDownLightShadows::RenderShadowVolumes(
                    g.GetRenderer(), sl.x, sl.y,
                    g.GetWindowsWidth(), g.GetWindowsHeight(),
                    staticShadowEdges, noDynamic,
                    90, sl.params.shadowMaxLengthPx,
                    sl.params.shadowSoftLayers, sl.params.shadowSoftness);
            }
        }

        bigLightContact = bigMaxContact;
        smallLightContact = smallMaxContact;

        // Salva a iluminação real para o sistema de Sanidade ler!
        const float thunderBoost = GameSfx::GetThunderFlashStrength() * 0.88f;

        // ── SANIDADE INDEPENDENTE ────────────────────────────────────────────
        // A luz de mão (isqueiro/lâmpada) fica com o irmão GRANDE (ver
        // GetActiveTorchWorldPos). Ela ilumina por completo só o PORTADOR; o
        // outro irmão só recebe a luz se estiver perto o bastante. Assim um pode
        // ficar no escuro (drenando sanidade) enquanto o outro está seguro.
        this->bigIlluminationLevel   = bigMaxTouch;
        this->smallIlluminationLevel = smallMaxTouch;
        if (torchIsActuallyLit && bigCharacterObject) {
            this->bigIlluminationLevel = std::max(bigMaxTouch, 0.92f);  // portador sempre iluminado
            if (smallCharacterObject) {
                const float shareRadius = std::max(1.0f, lightMaskParams.falloffRadiusPx);
                const float dist = smallCharacterObject->box.Center().Distance(bigCharacterObject->box.Center());
                const float falloff = std::clamp(1.0f - dist / shareRadius, 0.0f, 1.0f);
                this->smallIlluminationLevel = std::max(smallMaxTouch, 0.92f * falloff);
            }
        }

        if (thunderBoost > 0.01f) {
            this->bigIlluminationLevel = std::max(this->bigIlluminationLevel, thunderBoost);
            this->smallIlluminationLevel = std::max(this->smallIlluminationLevel, thunderBoost);
        }
    }

    if (lightsEnabled) {
        LightShadowProfile::EndLightsFrame();
    }

    if (lightsEnabled && shadowsEnabled && showDebugTools) {
        if (cursorPreviewLightEnabled) {
            const float previewShadowRadius = std::max(24.0f, std::max(8.0f, lightMaskParams.falloffRadiusPx) * std::max(0.4f, lightMaskParams.fatorDicaDeRaio));
            DrawDebugCircle(g.GetRenderer(), smoothedDynamicLightScreenPos.x, smoothedDynamicLightScreenPos.y, previewShadowRadius, 255, 210, 90, 130);
        }

        int renderedLights = 0;
        for (const LightInstance& light : lights) {
            if (!light.enabled) continue;
            if (renderedLights >= maxActiveLights) break;
            const Vec2 lightScreen = WorldToScreen(light.worldPos);
            const float cullRadius = std::max(32.0f, light.params.falloffRadiusPx * 1.6f);
            if (lightScreen.x < -cullRadius || lightScreen.y < -cullRadius ||
                lightScreen.x > static_cast<float>(g.GetWindowsWidth()) + cullRadius ||
                lightScreen.y > static_cast<float>(g.GetWindowsHeight()) + cullRadius) {
                continue;
            }
            const float placedShadowRadius = std::max(24.0f, std::max(8.0f, light.params.falloffRadiusPx) * std::max(0.4f, light.params.fatorDicaDeRaio));
            DrawDebugCircle(g.GetRenderer(), lightScreen.x, lightScreen.y, placedShadowRadius, 120, 220, 255, 95);
            renderedLights++;
        }
    }

    if (lightTweakPanel && lightTweakPanel->visible) {
        lightTweakPanel->Render(g.GetRenderer(), g.GetWindowsWidth(), g.GetWindowsHeight());
    }

    if (showMapPhysicsDebug) {
        SDL_Renderer* dbgR = g.GetRenderer();
        level.RenderCollisionOverlay(dbgR);
        RenderGameplayCollisionDebug(dbgR);
        RenderCompanionFollowPathDebug(dbgR);
    }

    // Devolve o controle para o Monitor real!
    SDL_SetRenderTarget(renderer, nullptr);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_RenderCopy(renderer, renderTarget, nullptr, nullptr);

    // Brilho do jogador (overlay): <100 escurece, >100 clareia (lift de sombras).
    const int brightness = Game::brightnessPercent;
    if (brightness < 100) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        const Uint8 a = static_cast<Uint8>((100 - brightness) / 100.0f * 175.0f);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, a);
        const SDL_Rect r{0, 0, winW, winH};
        SDL_RenderFillRect(renderer, &r);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    } else if (brightness > 100) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
        const Uint8 a = static_cast<Uint8>((brightness - 100) / 50.0f * 95.0f);
        SDL_SetRenderDrawColor(renderer, 120, 120, 120, a);
        const SDL_Rect r{0, 0, winW, winH};
        SDL_RenderFillRect(renderer, &r);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }

    // Flash vermelho de dano (toque do monstro) — decai com damageFlashTimer.
    if (damageFlashTimer > 0.0f) {
        const float t = damageFlashTimer / kDamageFlashDuration;
        // Acessibilidade: atenua o clarão de dano quando "Reduzir flashes" está ligado.
        const float flashMul = Game::reduceFlashing ? 0.35f : 1.0f;
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        const Uint8 flashAlpha = static_cast<Uint8>(std::min(255.0f, 150.0f * t * flashMul));
        SDL_SetRenderDrawColor(renderer, 150, 10, 10, flashAlpha);
        const SDL_Rect dmgRect{0, 0, winW, winH};
        SDL_RenderFillRect(renderer, &dmgRect);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    }

    const float thunderFlash = GameSfx::GetThunderFlashStrength();
    if (thunderFlash > 0.01f) {
        // Acessibilidade: o clarão do trovão é o mais forte (aditivo) — reduz bastante.
        const float thunderMul = Game::reduceFlashing ? 0.25f : 1.0f;
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
        const Uint8 flashAlpha = static_cast<Uint8>(std::min(255.0f, 230.0f * thunderFlash * thunderMul));
        SDL_SetRenderDrawColor(renderer, 205, 215, 255, flashAlpha);
        const SDL_Rect flashRect{0, 0, winW, winH};
        SDL_RenderFillRect(renderer, &flashRect);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    }


    if (sanityOverlayObj) {
        SpriteRenderer* overlaySprite = sanityOverlayObj->GetComponent<SpriteRenderer>();

        if (overlaySprite && sanityOverlaySmoothedIntensity > 0.001f) {
            // Acessibilidade: reduz a aberração cromática (separação RGB enjoativa).
            const float aberrationMul = Game::reduceFlashing ? 0.3f : 1.0f;
            const float offsetPx = kChromaticAberrationMaxOffsetPx * sanityOverlaySmoothedIntensity * aberrationMul;
            const Rect baseBox = sanityOverlayObj->box;
            const Uint8 baseAlpha = static_cast<Uint8>(std::min(255.0f, 255.0f * sanityOverlaySmoothedIntensity));

            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);

            // Canal VERMELHO — desloca para a esquerda
            sanityOverlayObj->box.x = baseBox.x - offsetPx;
            sanityOverlayObj->box.y = baseBox.y;
            overlaySprite->SetTint(255, 30, 30, baseAlpha);
            overlaySprite->Render();

            // Canal AZUL — desloca para a direita
            sanityOverlayObj->box.x = baseBox.x + offsetPx;
            sanityOverlayObj->box.y = baseBox.y;
            overlaySprite->SetTint(30, 30, 255, baseAlpha);
            overlaySprite->Render();

            sanityOverlayObj->box = baseBox;
            overlaySprite->SetTint(255, 255, 255, 255);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        }
    }

    // ====================================
    // 7. HUD FICA ACIMA DE TUDO (Z >= 100)
    // ====================================

    for (const auto& go : objectArray) {
        if (go->z >= kHudZ) {
            go->Render();
        }
    }

    // ============================================================
    // Barra de sanidade acima da cabeça REMOVIDA de vez (inclusive em debug): o
    // jogador não vê mais o número/estado da sanidade — o feedback vem só pelos
    // indicadores de tela (overlay "spider-verse" + batimento cardíaco). A mecânica
    // de sanidade continua igual; apenas não é mais exibida aqui.

    // ── HUD DO PODER DO IRMÃOZINHO ────────────────────────────────────────────
    // Mostra uma bola no canto inferior esquerdo quando controlando o irmãozinho.
    // Cheia = pode usar o poder. Diminuindo = poder ativo. Enchendo = cooldown.
    // Substitui o hotbar do irmãozão (que não aparece para o irmãozinho).
    {
        Character* small = Character::littleBrother;
        bool isControllingSmall = (controlledCharacter == small && small != nullptr);
 
        if (isControllingSmall) {
            // Lê os timers do poder do irmãozinho
            float powerTimer  = small->visionPowerTimer;   // tempo restante ativo
            float cooldown    = small->visionCooldown;     // tempo restante de recarga
            float kDuration   = Character::kVisionDuration;
            float kCooldown   = Character::kVisionCooldown;
 
            // Calcula o preenchimento (0.0 a 1.0)
            float fill = 1.0f;
            if (powerTimer > 0.0f) {
                // Poder ativo — esvazia conforme o tempo passa
                fill = powerTimer / kDuration;
            } else if (cooldown > 0.0f) {
                // Recarregando — enche conforme o cooldown passa
                fill = 1.0f - (cooldown / kCooldown);
            }
            // Se nem powerTimer nem cooldown > 0, fill = 1.0 (pronto pra usar)
 
            // Posição: canto inferior esquerdo, mesmo lugar do hotbar
            int cx = static_cast<int>(winW * 0.12f + 32.0f);
            int cy = static_cast<int>(winH - 100.0f);
            constexpr int kRadius     = 40;   // raio da bola
            constexpr int kSegments   = 48;   // suavidade do círculo
 
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

            // Cor base dependendo do estado
            Uint8 baseR, baseG, baseB;
            if (powerTimer > 0.0f) {
                baseR = 180; baseG = 80;  baseB = 255; // roxo — poder ativo
            } else if (cooldown > 0.0f) {
                baseR = 100; baseG = 100; baseB = 120; // cinza — cooldown
            } else {
                baseR = 255; baseG = 255; baseB = 255; // branco — pronto
            }

            // Gradiente radial: centro preto → borda na cor base
            for (int r = 0; r <= kRadius; r++) {
                float t       = static_cast<float>(r) / static_cast<float>(kRadius);
                float tCurved = t * t; // concentra sombra no centro

                Uint8 colR = static_cast<Uint8>(baseR * tCurved);
                Uint8 colG = static_cast<Uint8>(baseG * tCurved);
                Uint8 colB = static_cast<Uint8>(baseB * tCurved);
                Uint8 colA = static_cast<Uint8>(200.0f + 55.0f * tCurved);

                SDL_SetRenderDrawColor(renderer, colR, colG, colB, colA);

                for (int i = 0; i < kSegments; i++) {
                    float a0 = (static_cast<float>(i)     / kSegments) * 2.0f * 3.14159265f;
                    float a1 = (static_cast<float>(i + 1) / kSegments) * 2.0f * 3.14159265f;
                    SDL_RenderDrawLine(renderer,
                        cx + static_cast<int>(std::cos(a0) * r),
                        cy + static_cast<int>(std::sin(a0) * r),
                        cx + static_cast<int>(std::cos(a1) * r),
                        cy + static_cast<int>(std::sin(a1) * r));
                }
            }

            // Preenchimento sólido com gradiente radial — centro escuro, borda brilhante
            {
                float startAngle = -3.14159265f / 2.0f;
                int fillSegs = static_cast<int>(fill * kSegments);

                // Cor da borda (brilhante)
                SDL_Color edgeColor;
                // Cor do centro (escura — mesma base mas bem mais escura)
                SDL_Color centerColor;

                if (powerTimer > 0.0f) {
                    edgeColor   = {220, 140, 255, 210}; // roxo claro na borda
                    centerColor = {40,  0,   80,  180}; // roxo escuro no centro
                } else if (cooldown > 0.0f) {
                    edgeColor   = {160, 160, 180, 180}; // cinza claro na borda
                    centerColor = {20,  20,  30,  160}; // quase preto no centro
                } else {
                    edgeColor   = {255, 255, 255, 210}; // branco na borda
                    centerColor = {40,  40,  60,  180}; // azul escuro no centro
                }

                SDL_Vertex verts[3];

                for (int i = 0; i < fillSegs; i++) {
                    float a0 = startAngle + (static_cast<float>(i)     / kSegments) * 2.0f * 3.14159265f;
                    float a1 = startAngle + (static_cast<float>(i + 1) / kSegments) * 2.0f * 3.14159265f;

                    // Centro — cor escura
                    verts[0].position  = {static_cast<float>(cx), static_cast<float>(cy)};
                    verts[0].color     = centerColor;
                    verts[0].tex_coord = {0, 0};

                    // Borda esquerda — cor brilhante
                    verts[1].position = {
                        static_cast<float>(cx) + std::cos(a0) * static_cast<float>(kRadius),
                        static_cast<float>(cy) + std::sin(a0) * static_cast<float>(kRadius)
                    };
                    verts[1].color     = edgeColor;
                    verts[1].tex_coord = {0, 0};

                    // Borda direita — cor brilhante
                    verts[2].position = {
                        static_cast<float>(cx) + std::cos(a1) * static_cast<float>(kRadius),
                        static_cast<float>(cy) + std::sin(a1) * static_cast<float>(kRadius)
                    };
                    verts[2].color     = edgeColor;
                    verts[2].tex_coord = {0, 0};

                    SDL_RenderGeometry(renderer, nullptr, verts, 3, nullptr, 0);
                }
            }

            // Borda externa fina
            SDL_SetRenderDrawColor(renderer, 200, 200, 220, 255);
            for (int i = 0; i < kSegments; i++) {
                float a0 = (static_cast<float>(i)     / kSegments) * 2.0f * 3.14159265f;
                float a1 = (static_cast<float>(i + 1) / kSegments) * 2.0f * 3.14159265f;
                SDL_RenderDrawLine(renderer,
                    cx + static_cast<int>(std::cos(a0) * kRadius),
                    cy + static_cast<int>(std::sin(a0) * kRadius),
                    cx + static_cast<int>(std::cos(a1) * kRadius),
                    cy + static_cast<int>(std::sin(a1) * kRadius));
            }

            // "[F] Usar" ao lado da bola do poder — igual ao irmãozão (que mostra
            // no slot ativo da roda). Fica VERMELHO (texto + contorno no ícone da
            // tecla F) quando o poder está em uso ou recarregando (indisponível).
            {
                const bool available = (powerTimer <= 0.0f && cooldown <= 0.0f);
                const SDL_Color kReady{235, 225, 195, 255};
                const SDL_Color kBusy {220, 55, 45, 255};
                const SDL_Color col = available ? kReady : kBusy;

                auto keyTex = Resources::GetImage("Recursos/img/hud/key_f.png");
                auto font   = Resources::GetFont("Recursos/font/times.ttf", 22);

                constexpr int imgSize = 54;
                constexpr int hintGap = 20;
                const int iconX = cx + kRadius + hintGap;   // canto sup-esq do ícone
                const int iconY = cy - imgSize / 2;

                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

                if (keyTex) {
                    SDL_Rect dst{ iconX, iconY, imgSize, imgSize };
                    SDL_RenderCopy(renderer, keyTex.get(), nullptr, &dst);
                    // Contorno vermelho ao redor do ícone da tecla F quando indisponível.
                    if (!available) {
                        SDL_SetRenderDrawColor(renderer, kBusy.r, kBusy.g, kBusy.b, 255);
                        for (int t = 1; t <= 3; ++t) {
                            SDL_Rect ol{ iconX - t, iconY - t, imgSize + 2 * t, imgSize + 2 * t };
                            SDL_RenderDrawRect(renderer, &ol);
                        }
                    }
                }

                if (font) {
                    if (SDL_Surface* s = TTF_RenderUTF8_Blended(font.get(), "Usar", col)) {
                        if (SDL_Texture* txt = SDL_CreateTextureFromSurface(renderer, s)) {
                            SDL_Rect td{ iconX + imgSize / 2 - s->w / 2,
                                         iconY + imgSize + 2, s->w, s->h };
                            SDL_RenderCopy(renderer, txt, nullptr, &td);
                            SDL_DestroyTexture(txt);
                        }
                        SDL_FreeSurface(s);
                    }
                }
            }

            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        }
    }
    // ── FIM HUD PODER ─────────────────────────────────────────────────────────

    // Indicador "quem estou controlando" — seta apontando para o irmão controlado,
    // que quica + pisca branco por alguns segundos e some suavemente (4.4).
    if (controlledCharacter && controlIndicatorTimer > 0.0f) {
        const float zoom = Camera::GetZoom();
        const Rect& cbox = controlledCharacter->GetAssociated().box;
        const int screenX = static_cast<int>((cbox.x - Camera::pos.x) * zoom);
        const int screenY = static_cast<int>((cbox.y - Camera::pos.y) * zoom);
        const int charW = static_cast<int>(cbox.w * zoom);
        const int cx = screenX + charW / 2;

        const float elapsed = kControlIndicatorDuration - controlIndicatorTimer;

        // Quica para cima/baixo.
        const float bounce = std::sin(elapsed * 9.0f) * 7.0f * zoom;

        // Fade suave: entra em 0.25s, sai (eased) nos últimos 0.7s.
        float a01 = 1.0f;
        if (controlIndicatorTimer < 0.7f) a01 = controlIndicatorTimer / 0.7f;
        else if (elapsed < 0.25f) a01 = elapsed / 0.25f;
        a01 = a01 * a01 * (3.0f - 2.0f * a01);   // smoothstep (eased-out)

        // Pisca branco (lerp dourado <-> branco).
        const float flash = 0.5f + 0.5f * std::sin(elapsed * 14.0f);
        const Uint8 r = static_cast<Uint8>(235 + (255 - 235) * flash);
        const Uint8 g = static_cast<Uint8>(205 + (255 - 205) * flash);
        const Uint8 b = static_cast<Uint8>(90 + (255 - 90) * flash);
        const Uint8 a = static_cast<Uint8>(235 * a01);

        const int w = static_cast<int>(22 * zoom);
        const int h = static_cast<int>(16 * zoom);
        const int topY = screenY - static_cast<int>(30 * zoom) + static_cast<int>(bounce);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, r, g, b, a);
        for (int row = 0; row < h; ++row) {
            const float t = (h > 0) ? static_cast<float>(row) / static_cast<float>(h) : 0.0f;
            const int halfW = static_cast<int>((w / 2) * (1.0f - t));
            SDL_RenderDrawLine(renderer, cx - halfW, topY + row, cx + halfW, topY + row);
        }
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    }

    // Overlay de reparo da escada — cobre tudo incluindo HUD
    for (const auto& goPtr : objectArray) {
        Repairable* rep = goPtr->GetComponent<Repairable>();
        if (!rep) continue;
        float alpha = rep->GetRepairOverlayAlpha();
        if (alpha > 0.001f) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0,
                static_cast<Uint8>(std::min(255.0f, alpha * 255.0f)));
            SDL_Rect fs = { 0, 0, winW, winH };
            SDL_RenderFillRect(renderer, &fs);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            break; // só um Repairable por vez
        }
    }

    RenderInteractionPrompt(renderer);
    RenderTutorials(renderer);
    RenderMonsterScare(renderer);
    RenderVoiceSubtitle(renderer);
    RenderLevelTitleBanner(renderer);
    RenderPauseMenu(renderer);
    RenderSettingsPanel(renderer);
    RenderControlsPanel(renderer);
    RenderQuitConfirmModal(renderer);
    RenderJournalViewer(renderer);
    RenderSaveToast(renderer);

    // Status dos toggles de debug (canto superior direito) — verde=ON, cinza=OFF.
    if (Game::debugMode) {
        auto dfont = Resources::GetFont("Recursos/font/times.ttf", 18);
        if (dfont) {
            struct DbgLine { std::string text; bool on; };
            const DbgLine dls[] = {
                {std::string("[B] Colisao/fisica: ") + (showMapPhysicsDebug ? "ON" : "OFF"), showMapPhysicsDebug},
                {std::string("[I] Invisivel p/ monstro: ") + (debugMonsterBlind ? "ON" : "OFF"), debugMonsterBlind},
                {std::string("[G] Camera livre: ") + (debugFreeCam ? "ON" : "OFF"), debugFreeCam},
            };
            int yy = 8;
            for (const auto& dl : dls) {
                SDL_Color col = dl.on ? SDL_Color{80, 255, 80, 255} : SDL_Color{170, 170, 170, 220};
                SDL_Surface* sf = TTF_RenderUTF8_Blended(dfont.get(), dl.text.c_str(), col);
                if (!sf) continue;
                SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, sf);
                const int tw = sf->w, th = sf->h;
                SDL_FreeSurface(sf);
                if (tex) {
                    SDL_Rect dst{winW - tw - 10, yy, tw, th};
                    SDL_RenderCopy(renderer, tex, nullptr, &dst);
                    SDL_DestroyTexture(tex);
                    yy += th + 4;
                }
            }

            // LEGENDA das cores de colisão — só com [B] ligado. Cada item: um
            // quadradinho da cor + o que ela significa.
            if (showMapPhysicsDebug) {
                struct LegItem { Uint8 r, g, b; const char* label; };
                const LegItem legend[] = {
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
                auto lfont = Resources::GetFont("Recursos/font/times.ttf", 15);
                yy += 6;
                for (const auto& li : legend) {
                    SDL_Color tcol{225, 225, 230, 235};
                    SDL_Surface* sf = lfont ? TTF_RenderUTF8_Blended(lfont.get(), li.label, tcol) : nullptr;
                    const int th = sf ? sf->h : 14;
                    const int sw = th;   // lado do quadradinho = altura do texto
                    SDL_Texture* tex = sf ? SDL_CreateTextureFromSurface(renderer, sf) : nullptr;
                    const int tw = sf ? sf->w : 0;
                    if (sf) SDL_FreeSurface(sf);
                    // fundo escuro atrás da linha p/ leitura sobre o cenário
                    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
                    SDL_Rect bg{winW - (sw + 6 + tw) - 12, yy - 1, (sw + 6 + tw) + 8, th + 2};
                    SDL_RenderFillRect(renderer, &bg);
                    // quadradinho da cor
                    SDL_SetRenderDrawColor(renderer, li.r, li.g, li.b, 255);
                    SDL_Rect sw_rect{winW - (sw + 6 + tw) - 8, yy, sw, th};
                    SDL_RenderFillRect(renderer, &sw_rect);
                    // rótulo
                    if (tex) {
                        SDL_Rect dst{winW - tw - 8, yy, tw, th};
                        SDL_RenderCopy(renderer, tex, nullptr, &dst);
                        SDL_DestroyTexture(tex);
                    }
                    yy += th + 3;
                }
            }
        }
    }

    // RenderFinalEscape(renderer);   // ANTIGO: clarões da luz do farol (DESATIVADO)

    // Se a transição acabou de começar, congela ESTE quadro (já composto) para o
    // efeito zoom-blur dos frames seguintes.
    if (sceneTransitionActive && !sceneTransitionFrame) {
        CaptureSceneFrame(renderer);
    }
}

void StageState::RenderVoiceSubtitle(SDL_Renderer* renderer) {
    if (!renderer) {
        return;
    }
    std::string caption;
    if (!GameVoice::GetActiveSubtitle(caption)) {
        return;   // ninguém falando agora
    }

    const int winW = Game::GetInstance().GetWindowsWidth();
    const int winH = Game::GetInstance().GetWindowsHeight();

    auto font = Resources::GetFont("Recursos/font/times.ttf", 26);
    if (!font) {
        return;
    }
    SDL_Color col{235, 232, 220, 255};
    const Uint32 wrap = static_cast<Uint32>(winW * 0.7f);
    SDL_Surface* sf = TTF_RenderUTF8_Blended_Wrapped(font.get(), caption.c_str(), col, wrap);
    if (!sf) {
        return;
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, sf);
    const int tw = sf->w, th = sf->h;
    SDL_FreeSurface(sf);
    if (!tex) {
        return;
    }

    const int pad = 14;
    const int boxW = tw + pad * 2;
    const int boxH = th + pad * 2;
    const int boxX = (winW - boxW) / 2;
    const int boxY = winH - boxH - 48;   // barra inferior, acima da borda

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);   // fundo semitransparente p/ legibilidade
    SDL_Rect bg{boxX, boxY, boxW, boxH};
    SDL_RenderFillRect(renderer, &bg);

    SDL_Rect dst{boxX + pad, boxY + pad, tw, th};
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
}

void StageState::RenderGameplayCollisionDebug(SDL_Renderer* renderer) const {
    if (!renderer) {
        return;
    }
    const float z = Camera::GetZoom();
    for (const auto& goPtr : objectArray) {
        GameObject* go = goPtr.get();
        if (!go) {
            continue;
        }
        Collider* col = go->GetComponent<Collider>();
        if (!col) {
            continue;
        }
        const bool isBig = (go == bigCharacterObject);
        const bool isSmall = (go == smallCharacterObject);
        // Cor por TIPO de collider (ver a legenda desenhada por RenderDebugToggleStatus):
        //  laranja=irmãozão · verde-água=irmãozinho · magenta=monstro ·
        //  ciano=caixa/barril (empurrável) · dourado=footprint de cenário (nav).
        Uint8 pr, pg, pb;
        if (isBig) {
            pr = 255; pg = 190; pb = 70;                 // irmãozão
        } else if (isSmall) {
            pr = 90;  pg = 255; pb = 200;                // irmãozinho
        } else if (go->GetComponent<Monster>() != nullptr) {
            pr = 255; pg = 0;   pb = 255;                // monstro
        } else if (go->GetComponent<Box>() != nullptr) {
            pr = 0;   pg = 220; pb = 255;                // caixa/barril empurrável
        } else {
            pr = 255; pg = 215; pb = 0;                  // footprint de cenário (pilar/mesa/armário...)
        }
        // Para os IRMÃOS não desenhamos o retângulo do Collider: a colisão real deles
        // é QUADRADA (caixa dos pés) + hurtbox QUADRADA — desenhadas logo abaixo.
        if (!isBig && !isSmall) {
            DrawColliderDebugWire(renderer, col->box, static_cast<float>(go->angleDeg), pr, pg, pb, 215);
        }

        if (Character* ch = go->GetComponent<Character>()) {
            // Caixas AABB estáveis do jogador (não mudam com a animação).
            auto drawWorldRect = [&](const SDL_Rect& wr, Uint8 cr, Uint8 cg, Uint8 cb) {
                const Vec2 tl = WorldToScreen(Vec2(static_cast<float>(wr.x), static_cast<float>(wr.y)));
                SDL_FRect sr{ tl.x, tl.y, wr.w * z, wr.h * z };
                SDL_SetRenderDrawColor(renderer, cr, cg, cb, 210);
                SDL_RenderDrawRectF(renderer, &sr);
            };
            // Caixa dos PÉS (colisão com o cenário) — verde/amarelo.
            Uint8 fr = 80, fg = 255, fb = 120;
            if (isBig)        { fr = 255; fg = 240; fb = 60; }
            else if (isSmall) { fr = 60;  fg = 255; fb = 180; }
            drawWorldRect(ch->GetFootRect(), fr, fg, fb);
            // HURTBOX (dano do monstro) — vermelho.
            drawWorldRect(ch->GetHitRect(), 255, 60, 60);
        }
    }
}

/// Polylinha amarela: rota que o seguidor usa neste frame (A* ou linha reta) — só faz sentido com `PartyMode::TOGETHER`.
void StageState::RenderCompanionFollowPathDebug(SDL_Renderer* renderer) const {
    if (!renderer || companionFollowPathWorld.size() < 2) {
        return;
    }
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
