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
void StageState::Update(float dt){
    // Chamadas a Mix_PlayMusic em todo frame fazem SDL_mixer reorganizar música e pode matar/samples atrasarem ondas.
    if (!musicMuted && music.IsOpen() && Mix_PlayingMusic() == 0) {
        gStageOstSilenceRecover += dt;
        if (gStageOstSilenceRecover >= 0.5f) {
            music.Play(-1);
            gStageOstSilenceRecover = 0.f;
        }
    } else {
        gStageOstSilenceRecover = 0.f;
    }

    InputManager& input = InputManager::GetInstance();

    if (oceanWavesChunk) {
        oceanAmbient_.EnsurePlaying();
        oceanAmbient_.RefreshVolume();
    }

    Vec2 prevBigPos(0.0f, 0.0f);
    Vec2 prevSmallPos(0.0f, 0.0f);
    if (bigCharacterObject) {
        prevBigPos = Vec2(bigCharacterObject->box.x, bigCharacterObject->box.y);
    }
    if (smallCharacterObject) {
        prevSmallPos = Vec2(smallCharacterObject->box.x, smallCharacterObject->box.y);
    }

    // QuitRequested (Clicar no X da janela) -> Fecha o jogo
    if (input.QuitRequested()) {
        quitRequested = true;
    }

    // ESC -> Volta para o menu
    if (input.KeyPress(ESCAPE_KEY)) {
        popRequested = true;
    }

    if (input.KeyPress(LIGHTS_TOGGLE_KEY)) {
        lightsEnabled = !lightsEnabled;
    }
    if (input.KeyPress(SHADOWS_TOGGLE_KEY)) {
        shadowsEnabled = !shadowsEnabled;
    }
    if (input.KeyPress(MUSIC_MUTE_TOGGLE_KEY)) {
        musicMuted = !musicMuted;
        const int masterVolume = (MIX_MAX_VOLUME * Game::masterVolumePercent) / 100;
        Mix_VolumeMusic(musicMuted ? 0 : masterVolume);
        oceanAmbient_.RefreshVolume();
    }
    if (input.KeyPress(MAP_PHYSICS_DEBUG_KEY)) {
        showMapPhysicsDebug = !showMapPhysicsDebug;
    }
    if (input.KeyPress(CURSOR_PREVIEW_LIGHT_TOGGLE_KEY)) {
        cursorPreviewLightEnabled = !cursorPreviewLightEnabled;
    }

    if (input.KeyPress(CREATE_LIGHT_KEY) &&
        (lightMaskShape == LightMaskShape::Circle || lightMaskShape == LightMaskShape::Torch)) {
        CreateLightAtCursor();
    }

    if (IsPartyReady()) {
        HandlePartyInput();
        IssueMovementFromInput(controlledCharacter, controlledCharacterObject);
        UpdateCompanionBehavior();
    }

    UpdateArray(dt);                                                                    // Percorre o vetor de GameObjects chamando o Update de cada um
    inventory.TickUsingDurability(dt);                                                 // slot "usando" degrada sempre, mesmo controlando o irmãozinho
    ApplyMapBoundsAndWalkability(bigCharacterObject, prevBigPos);
    ApplyMapBoundsAndWalkability(smallCharacterObject, prevSmallPos);
    if (IsPartyReady()) {
        EnforceMaxDistance();
        ApplyMapBoundsAndWalkability(bigCharacterObject, prevBigPos);
        ApplyMapBoundsAndWalkability(smallCharacterObject, prevSmallPos);
        UpdateControlledCharacterVisuals();
        RefreshCameraTargets();
    }
    Camera::Update(dt);                                                                 // Atualizando a camera cada iteração do gameloop
    UpdateHudInstructions();

    if (lightTweakPanel) {
        lightTweakPanel->Update(input, dt, Game::GetInstance().GetWindowsWidth(), Game::GetInstance().GetWindowsHeight());
        if (lightTweakPanel->ConsumeCreateLightRequest()) {
            CreateLightAtCursor();
        }
    }

    // FPS monitor: suaviza leitura e atualiza HUD periodicamente para evitar custo de textura a cada frame.
    if (dt > 1e-6f) {
        const float instantFps = 1.0f / dt;
        const float smoothAlpha = 0.10f; // EMA simples para estabilidade visual
        fpsSmoothed += (instantFps - fpsSmoothed) * smoothAlpha;
    }
    fpsUiRefreshTimer += dt;
    if (hudFps && fpsUiRefreshTimer >= 0.10f) {
        fpsUiRefreshTimer = 0.0f;
        Text* fpsText = hudFps->GetComponent<Text>();
        if (fpsText) {
            const float instantFps = (dt > 1e-6f) ? (1.0f / dt) : 0.0f;
            const bool steady = std::fabs(instantFps - fpsSmoothed) <= 2.0f;
            SDL_Color fpsColor = {220, 80, 80, 240}; // vermelho: queda forte
            const char* quality = "LOW";
            if (fpsSmoothed >= 58.0f && steady) {
                fpsColor = {90, 235, 120, 240};      // verde: saudável + estável
                quality = "HEALTHY";
            } else if (fpsSmoothed >= 50.0f) {
                fpsColor = {245, 210, 90, 240};      // amarelo: aceitável, com quedas
                quality = steady ? "OK" : "UNSTEADY";
            }

            char fpsBuffer[64];
            std::snprintf(fpsBuffer, sizeof(fpsBuffer), "FPS: %.1f (%s)", fpsSmoothed, quality);
            fpsText->SetColor(fpsColor);
            fpsText->SetText(fpsBuffer);
        }
    }

    if (IsPartyReady() && input.MousePress(SDL_BUTTON_RIGHT)) {
        const int mx = input.GetMouseX();
        const int my = input.GetMouseY();
        const float z = Camera::GetZoom();
        auto screenRectOf = [&](const GameObject* go) -> Rect {
            const Rect& b = go->box;
            return Rect((b.x - Camera::pos.x) * z, (b.y - Camera::pos.y) * z, b.w * z, b.h * z);
        };
        const Vec2 mouse(static_cast<float>(mx), static_cast<float>(my));
        const bool onBig = bigCharacterObject && screenRectOf(bigCharacterObject).Contains(mouse);
        const bool onSmall = smallCharacterObject && screenRectOf(smallCharacterObject).Contains(mouse);
        if (onBig || onSmall) {
            if (onBig && onSmall) {
                const Vec2 mb = bigCharacterObject->box.Center();
                const Vec2 ms = smallCharacterObject->box.Center();
                const Vec2 mworld = ScreenToWorld(mouse);
                previewLightAnchorPlayer =
                    (mworld.Distance(mb) <= mworld.Distance(ms)) ? bigCharacterObject : smallCharacterObject;
            } else {
                previewLightAnchorPlayer = onBig ? bigCharacterObject : smallCharacterObject;
            }
            previewLightLockedToPlayer = true;
        } else {
            bool blockUnlock = false;
            if (hotbarObject) {
                if (HotbarComponent* hb = hotbarObject->GetComponent<HotbarComponent>()) {
                    if (hb->BlocksLightPointerUnlock(mx, my)) {
                        blockUnlock = true;
                    }
                }
            }
            if (!blockUnlock) {
                previewLightLockedToPlayer = false;
                previewLightAnchorPlayer = nullptr;
            }
        }
    }

    Vec2 targetLightScreen(smoothedDynamicLightScreenPos.x, smoothedDynamicLightScreenPos.y);
    if (cursorPreviewLightEnabled) {
        targetLightScreen =
            Vec2(static_cast<float>(input.GetMouseX()), static_cast<float>(input.GetMouseY()));
        if (previewLightLockedToPlayer) {
            if (!IsPartyReady() ||
                (previewLightAnchorPlayer != bigCharacterObject && previewLightAnchorPlayer != smallCharacterObject)) {
                previewLightLockedToPlayer = false;
                previewLightAnchorPlayer = nullptr;
            }
        }
        if (previewLightLockedToPlayer && previewLightAnchorPlayer) {
            // Luz presa ao jogador: origem no centro do sprite (não nos pés); sombras de contato continuam nos pés.
            const Rect& ab = previewLightAnchorPlayer->box;
            targetLightScreen = WorldToScreen(ab.Center());
        }

        {
            Vec2 tw = ScreenToWorld(targetLightScreen);
            int tx, ty;
            if (WorldToTile(tw, tx, ty) && !IsTileWalkable(tx, ty)) {
                int ntx, nty;
                if (FindNearestWalkableTile(tx, ty, ntx, nty)) {
                    Vec2 clampedWorld = TileCenterToWorld(ntx, nty);
                    if (hasSmoothedDynamicLight) {
                        Vec2 currentWorld = ScreenToWorld(smoothedDynamicLightScreenPos);
                        if (!HasWalkableLine(currentWorld, clampedWorld)) {
                            targetLightScreen = smoothedDynamicLightScreenPos;
                        } else {
                            targetLightScreen = WorldToScreen(clampedWorld);
                        }
                    } else {
                        targetLightScreen = WorldToScreen(clampedWorld);
                    }
                }
            }
        }
    } else if (!hasSmoothedDynamicLight) {
        // Primeiro frame com preview desligado: inicializa a partir do rato para o smoother existir.
        targetLightScreen =
            Vec2(static_cast<float>(input.GetMouseX()), static_cast<float>(input.GetMouseY()));
    }

    if (!hasSmoothedDynamicLight) {
        smoothedDynamicLightScreenPos = targetLightScreen;
        hasSmoothedDynamicLight = true;
    } else {
        const float s = std::max(0.01f, std::min(0.95f, lightMaskParams.lightTemporalSmoothing));
        const float lerpA = 1.0f - std::pow(1.0f - s, dt * 60.0f);
        smoothedDynamicLightScreenPos = smoothedDynamicLightScreenPos + (targetLightScreen - smoothedDynamicLightScreenPos) * lerpA;
    }

    if (inventory.IsUsableLightActive() && bigCharacterObject) {
        const Vec2 targetTorchScreen = WorldToScreen(bigCharacterObject->box.Center());
        if (!hasSmoothedTorchLight) {
            smoothedTorchLightScreenPos = targetTorchScreen;
            hasSmoothedTorchLight = true;
        } else {
            const float s = std::max(0.01f, std::min(0.95f, lightMaskParams.lightTemporalSmoothing));
            const float lerpA = 1.0f - std::pow(1.0f - s, dt * 60.0f);
            smoothedTorchLightScreenPos =
                smoothedTorchLightScreenPos + (targetTorchScreen - smoothedTorchLightScreenPos) * lerpA;
        }
    } else {
        hasSmoothedTorchLight = false;
    }

    // Loop duplo para testar pares de objetos
    for (size_t i = 0; i < objectArray.size(); i++) {
        // Para garantir que um par só é testado uma vez, usamos 'j' começando em 'i + 1'
        for (size_t j = i + 1; j < objectArray.size(); j++) {
            // Pega os GameObjects
            GameObject* goA = objectArray[i].get();
            GameObject* goB = objectArray[j].get();

            // Somente os que tem o componente collider
            Collider* colliderA = goA->GetComponent<Collider>();
            Collider* colliderB = goB->GetComponent<Collider>();

            if (colliderA && colliderB) { //..Se ambos existirem
                // "Use a box dos Colliders mas a rotação do GameObject"
                // Lembrando que isColliding espera radianos e meu angleDeg é em graus
                float angleDegA = goA->angleDeg * (M_PI / 180.0);
                float angleDegB = goB->angleDeg * (M_PI / 180.0);

                if (Collision::IsColliding(colliderA->box, colliderB->box, angleDegA, angleDegB)) {
                    // "Se houver colisão, notifique ambos os GameObjects"
                    goA->NotifyCollision(*goB);
                    goB->NotifyCollision(*goA);
                }
            }
        }    
    }

    for (size_t i = 0; i < objectArray.size();) {               
        if(objectArray[i]->IsDead()) {                          // Se o GameObject está morto 
            objectArray.erase(objectArray.begin() + i);         // Remova-o do array (Com iterador do ínicio somado á posição do elemento)
        } else {
            i++;                                                // Se não, avança para o próximo
        }
    }

    // VERIFICAÇÃO DE FIM DE JOGO

    // 1. Derrota: Jogador morreu
    if (!IsPartyReady()) {
        GameData::playerVictory = false;                        // Se o ponteiro é nulo, o jogador morreu

        // Empilha a tela de fim e pausa a fase atual
        popRequested = true;                                    // Remove o StageState atual
        Game::GetInstance().Push(new EndState());
    }
}
