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
void StageState::Render(){

    SDL_Renderer* renderer = Game::GetInstance().GetRenderer();

    // 1. PINTA O VAZIO DE PRETO
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // 2. DESENHA TODO O CENÁRIO (Parede, Chão)
    level.RenderBackground(renderer);

    // ============================
    // 3. ORDENAÇÃO Z/Y SORTING
    // ============================
    auto compareObjects = [](const std::shared_ptr<GameObject>& a, const std::shared_ptr<GameObject>& b) {
        
        // 1. Z-Sort Absoluto: Só desempata se forem de andares ou escopos visuais completamente diferentes (Chão vs UI)
        if (a->z != b->z) return a->z < b->z;

        // --- 2. EXCEÇÃO DINÂMICA DA ESCADA CURVA ---
        Character* charA = a->GetComponent<Character>();
        Character* charB = b->GetComponent<Character>();
        
        bool a_elevated = (charA && charA->isElevated);
        bool b_elevated = (charB && charB->isElevated);
        
        // Se 'a' é o jogador subindo e 'b' é a escada: 'a' SEMPRE desenha por cima (Retorna falso para 'a' vir depois)
        if (a_elevated && b->isStairs) return false; 
        
        // Se 'b' é o jogador subindo e 'a' é a escada: 'b' SEMPRE desenha por cima
        if (b_elevated && a->isStairs) return true;

        //============================================
        
        // 3. Y-Sort Relativo (Calcula a base real do objeto + o Deslocamento Fantasma)
        float base_a = (a->owner != nullptr) ? (a->owner->box.y + a->owner->box.h) : (a->box.y + a->box.h);
        float base_b = (b->owner != nullptr) ? (b->owner->box.y + b->owner->box.h) : (b->box.y + b->box.h);
        
        float sortingY_a = base_a + a->depthOffset;
        float sortingY_b = base_b + b->depthOffset;
        
        float epsilon = 0.01f; 
        if (std::abs(sortingY_a - sortingY_b) > epsilon) {
            return sortingY_a < sortingY_b;
        }
        
        // 4. Fallback para objetos no exato mesmo pixel de profundidade
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
    const bool torchFromInventory = inventory.IsUsableLightActive();
    const bool bigCircleOnlyLight =
        cursorPreviewLightEnabled && previewLightLockedToPlayer && previewLightAnchorPlayer == bigCharacterObject;
    const bool smallCircleOnlyLight =
        cursorPreviewLightEnabled && previewLightLockedToPlayer && previewLightAnchorPlayer == smallCharacterObject;
    float bigMaxContact = 0.0f;
    float smallMaxContact = 0.0f;

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

        if (torchFromInventory && hasSmoothedTorchLight) {
            renderShadowsForLight(smoothedTorchLightScreenPos, lightMaskParams);
        }

        if (showDebugTools && cursorPreviewLightEnabled) {
            const float previewShadowRadius = std::max(24.0f, std::max(8.0f, lightMaskParams.falloffRadiusPx) * std::max(0.4f, lightMaskParams.fatorDicaDeRaio));
            DrawDebugCircle(g.GetRenderer(), smoothedDynamicLightScreenPos.x, smoothedDynamicLightScreenPos.y, previewShadowRadius, 255, 210, 90, 130);
            if (torchFromInventory && hasSmoothedTorchLight) {
                DrawDebugCircle(g.GetRenderer(), smoothedTorchLightScreenPos.x, smoothedTorchLightScreenPos.y,
                                previewShadowRadius * 0.85f, 255, 150, 70, 150);
            }
        } else if (showDebugTools && torchFromInventory && hasSmoothedTorchLight) {
            const float previewShadowRadius = std::max(24.0f, std::max(8.0f, lightMaskParams.falloffRadiusPx) * std::max(0.4f, lightMaskParams.fatorDicaDeRaio));
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

        constexpr size_t kMaxSpriteShadowsPerPlayer = 2;
        std::sort(bigShadowCasts.begin(), bigShadowCasts.end(), [](const SpriteShadowCast& a, const SpriteShadowCast& b) { return a.touch > b.touch; });
        std::sort(smallShadowCasts.begin(), smallShadowCasts.end(), [](const SpriteShadowCast& a, const SpriteShadowCast& b) { return a.touch > b.touch; });
        
        if (bigShadowCasts.size() > kMaxSpriteShadowsPerPlayer) bigShadowCasts.resize(kMaxSpriteShadowsPerPlayer);
        if (smallShadowCasts.size() > kMaxSpriteShadowsPerPlayer) smallShadowCasts.resize(kMaxSpriteShadowsPerPlayer);
        
        for (const SpriteShadowCast& c : bigShadowCasts) {
            if (!bigCircleOnlyLight && c.contact < 0.50f) {
                RenderProjectedSpriteShadow(bigCharacterObject, c.lightScreen, c.touch, c.lengthPx, c.alpha, lightMaskParams);
            }
        }
        for (const SpriteShadowCast& c : smallShadowCasts) {
            if (!smallCircleOnlyLight && c.contact < 0.50f) {
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
        if (torchFromInventory && hasSmoothedTorchLight && bigCharacterObject) {
            bigMaxContact = std::max(bigMaxContact, 0.92f);
        }

        if (bigCharacterObject && bigMaxContact > 0.0f) {
            DrawContactFootShadow(g.GetRenderer(), bigCharacterObject->box, bigMaxContact);
        }
        if (smallCharacterObject && smallMaxContact > 0.0f) {
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
    // 5. AGORA DESENHAMOS A LISTA ORDENADA INTEIRA SEM REGRAS
    // ===================================================================
    constexpr int kHudZ = 100;
    for (const auto& go : objectArray) {
        if (go->z < kHudZ) {
            go->Render();
        }
    }

    // ===================================================================
    // 6. DEBUG E SISTEMA DE LUZ RADIAL
    // ===================================================================

    if (lightsEnabled && radialGeometry != nullptr) {
        std::vector<RadialLightOverlay::ScreenLight> screenLights;
        screenLights.reserve(static_cast<size_t>(maxActiveLights + 2));
        constexpr float kCursorLightBlend = 0.28f;
        constexpr float kTorchLightBlend = 0.28f;
        if (cursorPreviewLightEnabled) {
            screenLights.push_back({smoothedDynamicLightScreenPos.x, smoothedDynamicLightScreenPos.y, lightMaskShape,
                                    lightMaskParams, kCursorLightBlend});
        }
        if (torchFromInventory && hasSmoothedTorchLight) {
            screenLights.push_back({smoothedTorchLightScreenPos.x, smoothedTorchLightScreenPos.y, lightMaskShape,
                                    lightMaskParams, kTorchLightBlend});
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

    // 7. HUD FICA ACIMA DE TUDO (Z >= 100)
    for (const auto& go : objectArray) {
        if (go->z >= kHudZ) {
            go->Render();
        }
    }
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
        Uint8 pr = 200, pg = 230, pb = 255;
        if (isBig) {
            pr = 255;
            pg = 190;
            pb = 70;
        } else if (isSmall) {
            pr = 90;
            pg = 255;
            pb = 200;
        }
        DrawColliderDebugWire(renderer, col->box, static_cast<float>(go->angleDeg), pr, pg, pb, 215);

        if (go->GetComponent<Character>() != nullptr) {
            const float rFoot = go->box.w * 0.25f;
            const Vec2 footWorld(go->box.x + go->box.w * 0.5f, go->box.y + go->box.h - rFoot);
            const Vec2 screen = WorldToScreen(footWorld);
            const float rScreen = std::max(1.5f, rFoot * z);
            Uint8 fr = 80, fg = 255, fb = 120;
            if (isBig) {
                fr = 255;
                fg = 240;
                fb = 60;
            } else if (isSmall) {
                fr = 60;
                fg = 255;
                fb = 180;
            }
            DrawDebugCircle(renderer, screen.x, screen.y, rScreen, fr, fg, fb, 185);
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
