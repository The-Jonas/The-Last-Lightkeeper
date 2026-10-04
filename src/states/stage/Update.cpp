// ─────────────────────────────────────────────────────────────────────────────
//  StageState — Update do frame e as etapas em que ele se divide.
// ─────────────────────────────────────────────────────────────────────────────
#include "states/stage/StageState.h"
#include "states/stage/InternalHelpers.h"
#include "states/EndState.h"
#include "audio/GameSfx.h"
#include "audio/GameVoice.h"
#include "audio/Sound.h"
#include "core/Game.h"
#include "core/GameData.h"
#include "core/InputManager.h"
#include "core/SaveManager.h"
#include "core/Telemetry.h"
#include "engine/Camera.h"
#include "engine/GameObject.h"
#include "engine/SpriteRenderer.h"
#include "gameplay/Candlestick.h"
#include "gameplay/Character.h"
#include "gameplay/Monster.h"
#include "gameplay/Window.h"
#include "ui/Text.h"
#include "world/Collider.h"
#include "world/Collision.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace stage_internal;

namespace {

constexpr float kDegToRad = 3.14159265358979f / 180.0f;

Sound gMonsterHitSfx[2];              // impacto do monstro, carregados na 1ª pancada
bool  gMonsterHitSfxLoaded = false;

// Desconta dt de um timer que para em zero.
void TickDown(float& timer, float dt) {
    if (timer > 0.0f) timer = std::max(0.0f, timer - dt);
}

// Aproxima `value` de `target` com suavização independente do FPS
// (`smoothing` 0..1 = fração por quadro a 60 FPS).
Vec2 SmoothTowards(const Vec2& value, const Vec2& target, float smoothing, float dt) {
    const float s = std::max(0.01f, std::min(0.95f, smoothing));
    const float a = 1.0f - std::pow(1.0f - s, dt * 60.0f);
    return value + (target - value) * a;
}

// Menor sanidade entre os dois irmãos (100 se algum não existir).
float LowestSanity() {
    return std::min(Character::player ? Character::player->sanity : 100.0f,
                    Character::littleBrother ? Character::littleBrother->sanity : 100.0f);
}

}  // namespace

// Pancada do monstro: som, tremor, clarão vermelho e a janela que atribui a morte a ele.
void StageState::TriggerMonsterHitFeedback() {
    if (!gMonsterHitSfxLoaded) {
        gMonsterHitSfx[0].Open("Recursos/audio/Hit0.wav");
        gMonsterHitSfx[1].Open("Recursos/audio/Hit1.wav");
        gMonsterHitSfxLoaded = true;
    }
    gMonsterHitSfx[std::rand() % 2].Play();
    Camera::AddTrauma(0.65f);
    damageFlashTimer    = kDamageFlashDuration;
    lastMonsterHitTimer = kMonsterHitDeathWindow;   // o evento "monster_hit" é gravado pelo Monster
}

// ═════════════════════════════════════════════════════════════════════════════
//  Update
// ═════════════════════════════════════════════════════════════════════════════

// Ordem: áudio, eventos agendados e dicas do tutorial → overlays que congelam o
// mundo (transição, documento, pasta, sair, pausa) → input de gameplay → objetos → posições e
// câmera → luzes → colisões → efeitos de sanidade → derrota.
void StageState::Update(float dt) {
    lastFrameDt = dt;
    UpdateTelemetry(dt);
    dynamicColliderCacheDirty = true;
    SDL_ShowCursor((pauseMenuOpen || quitConfirmOpen) ? SDL_ENABLE : SDL_DISABLE);

    UpdateStageMusic(dt);

    InputManager& input = InputManager::GetInstance();
    if (input.QuitRequested()) {
        quitRequested = true;
    }

    // Transição de escada: congela tudo e só avança o efeito.
    if (sceneTransitionActive) {
        UpdateSceneTransition(dt);
        return;
    }

    UpdatePendingWindowBreak(dt);
    UpdateOceanAmbient(dt);
    hints.Update(*this, BuildHintContext(), dt);   // antes dos overlays: a dica vê a pasta abrir

    const Vec2 prevBigPos   = bigCharacterObject   ? Vec2(bigCharacterObject->box.x, bigCharacterObject->box.y)     : Vec2(0.0f, 0.0f);
    const Vec2 prevSmallPos = smallCharacterObject ? Vec2(smallCharacterObject->box.x, smallCharacterObject->box.y) : Vec2(0.0f, 0.0f);
    TickDown(companionPathRefreshTimer, dt);

    // Overlays que pausam o mundo.
    if (journalViewerOpen || journalViewerClosing) {
        dialogueBox.Update(dt);
        UpdateJournalViewer(dt);
        return;
    }
    if (documentFolderOpen) {
        dialogueBox.Update(dt);
        UpdateDocumentFolder(dt);
        return;
    }
    if (quitConfirmOpen) {
        HandleQuitConfirmInput();
        return;
    }

    TickOverlayTimers(dt);

    if (HandleEscapeKey()) {
        return;
    }
    if (pauseMenuOpen) {
        if (settingsMenu.IsOpen()) {
            settingsMenu.Update(dt);
        } else {
            HandlePauseMenuInput();
        }
        return;   // a pausa congela o mundo (e "Reiniciar nível" já pediu o pop)
    }
    if (popRequested) {
        return;
    }

    if (Game::debugMode) {
        HandleDebugKeys(input);
    }

    // Movimento e parceiro.
    if (IsPartyReady()) {
        HandlePartyInput();
        IssueMovementFromInput(controlledCharacter, controlledCharacterObject);
        if (companionStartDelay > 0) {
            companionStartDelay--;
            return;
        }
        UpdateCompanionBehavior();
    }

    if (input.KeyPress(TAB_KEY)) {
        OpenDocumentFolder();
        return;
    }
    UpdateBoxInteraction();
    TryOpenJournalOnKeyPress();
    TryInteractCandleOnKeyPress();
    TryInteractWindowOnKeyPress();
    TryInteractRadioOnKeyPress();
    UpdateFarApartVoice();

    UpdateInventoryLight();
    UpdateMonsterEchoes(dt);
    dialogueBox.Update(dt);
    fuelFlameHud.Update(dt);

    // Recalculados pelos Update dos objetos neste frame.
    reachableCloset = nullptr;
    reachableRepairable = nullptr;
    repairableInReachNoItem = false;
    repairableHeldItemNeeded.clear();
    UpdateArray(dt);

    UpdateWindowLockdown(dt);
    reachablePickup = FindClosestReachableItem();

    // Lendo um documento a luz não gasta combustível.
    if (inventory.IsUsableLightActive() && !journalViewerOpen &&
        (!lightTweakPanel || lightTweakPanel->durabilityEnabled)) {
        inventory.TickUsingDurability(dt);
    }

    ApplyMapBoundsAndWalkability(bigCharacterObject, prevBigPos);
    ApplyMapBoundsAndWalkability(smallCharacterObject, prevSmallPos);
    if (IsPartyReady()) {
        EnforceMaxDistance();
        ApplyMapBoundsAndWalkability(bigCharacterObject, prevBigPos);
        ApplyMapBoundsAndWalkability(smallCharacterObject, prevSmallPos);
        UpdateControlledCharacterVisuals();
        RefreshCameraTargets();
    }
    ApplyCoupledPushMovement(prevBigPos);

    UpdateCamera(dt, input);
    UpdateHudInstructions();

    if (Game::debugMode && lightTweakPanel) {
        lightTweakPanel->Update(input, dt, Game::GetInstance().GetWindowsWidth(), Game::GetInstance().GetWindowsHeight());
        if (lightTweakPanel->ConsumeCreateLightRequest()) {
            CreateLightAtCursor();
        }
    }
    UpdateFpsHud(dt);

    UpdatePreviewLightAnchor(input);
    UpdateLightSmoothing(dt, input);
    CheckObjectCollisions();

    GameSfx::UpdateThunder(dt);
    if (Game::debugMode && input.KeyPress(THUNDER_TEST_KEY)) {
        GameSfx::TriggerThunderStrike();
    }

    UpdateSanityFeedback(dt);
    CheckDefeat();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Etapas do Update
// ═════════════════════════════════════════════════════════════════════════════

// Se a trilha parou sozinha por 0,5 s, volta a tocar (sem chamar Mix_PlayMusic todo frame).
void StageState::UpdateStageMusic(float dt) {
    if (!musicMuted && music.IsOpen() && Mix_PlayingMusic() == 0) {
        gStageOstSilenceRecover += dt;
        if (gStageOstSilenceRecover >= 0.5f) {
            music.Play(-1);
            gStageOstSilenceRecover = 0.0f;
        }
    } else {
        gStageOstSilenceRecover = 0.0f;
    }
}

// Janela quebrada: primeiro o estrondo (vindo de onde o monstro está), 1 s depois a fala.
void StageState::UpdatePendingWindowBreak(float dt) {
    if (pendingWindowBreakDialogueTimer > 0.0f) {
        pendingWindowBreakDialogueTimer -= dt;
        if (pendingWindowBreakDialogueTimer <= 0.0f) {
            pendingWindowBreakDialogueTimer = -1.0f;
            Monster* breaker = FindMonster();
            if (breaker && controlledCharacterObject) {
                const Vec2 src = breaker->GetAssociated().box.Center();
                const Vec2 ear = controlledCharacterObject->box.Center();
                GameSfx::PlayWindowBreak(src.x, src.y, ear.x, ear.y);
            } else {
                GameSfx::PlayWindowBreak();
            }
            pendingWindowBreakLineTimer = 1.0f;
        }
    }

    if (pendingWindowBreakLineTimer > 0.0f) {
        pendingWindowBreakLineTimer -= dt;
        if (pendingWindowBreakLineTimer <= 0.0f) {
            pendingWindowBreakLineTimer = -1.0f;
            PlayDialogue("event:window_break", "Ao ouvir uma janela quebrar", {
                {DialogueBox::Speaker::LittleBrother, DialogueBox::Speaker::BigBrother,
                 DialogueBox::Emotion::Doubt, DialogueBox::Emotion::Fear,
                 "Que barulho foi esse?"}});
        }
    }
}

// Mantém as ondas tocando (depois do atraso de retomada) com o volume atual.
void StageState::UpdateOceanAmbient(float dt) {
    if (!oceanWavesChunk) return;
    if (ambientResumeDelay > 0.0f) {
        ambientResumeDelay -= dt;
    } else {
        oceanAmbient_.EnsurePlaying();
        oceanAmbient_.RefreshVolume();
    }
}

// Timers de tela que só descem até zero.
void StageState::TickOverlayTimers(float dt) {
    TickDown(levelTitleTimer, dt);
    TickDown(damageFlashTimer, dt);
    TickDown(saveToastTimer, dt);
    TickDown(lastMonsterHitTimer, dt);
    TickDown(controlIndicatorTimer, dt);
}

// ESC: abre a pausa (silenciando os loops de gameplay, já que o mundo para).
// Com a pausa aberta o ESC fica para ela.
// True = o frame acabou aqui.
bool StageState::HandleEscapeKey() {
    if (!InputManager::GetInstance().KeyPress(ESCAPE_KEY)) return false;

    if (!pauseMenuOpen) {
        pauseMenuOpen = true;
        pauseMenuSelection = 0;
        GameSfx::StopAllGameplayAudio();
        return true;
    }
    return false;
}

// Atalhos de desenvolvedor (só com Game::debugMode).
void StageState::HandleDebugKeys(InputManager& input) {
    if (input.KeyPress(LIGHTS_TOGGLE_KEY))               lightsEnabled = !lightsEnabled;
    if (input.KeyPress(SHADOWS_TOGGLE_KEY))              shadowsEnabled = !shadowsEnabled;
    if (input.KeyPress(SDLK_F5))                         BeginLevelTransition(currentLevelIndex + 1);
    if (input.KeyPress(MAP_PHYSICS_DEBUG_KEY))           showMapPhysicsDebug = !showMapPhysicsDebug;
    if (input.KeyPress(CURSOR_PREVIEW_LIGHT_TOGGLE_KEY)) cursorPreviewLightEnabled = !cursorPreviewLightEnabled;
    if (input.KeyPress(MONSTER_BLIND_TOGGLE_KEY))        debugMonsterBlind = !debugMonsterBlind;
    if (input.KeyPress(FREE_CAMERA_TOGGLE_KEY))          debugFreeCam = !debugFreeCam;

    if (input.KeyPress(MUSIC_MUTE_TOGGLE_KEY)) {
        musicMuted = !musicMuted;
        Mix_VolumeMusic(musicMuted ? 0 : Game::MusicVolume());
        oceanAmbient_.RefreshVolume();
    }
    if (input.KeyPress(CREATE_LIGHT_KEY) &&
        (lightMaskShape == LightMaskShape::Circle || lightMaskShape == LightMaskShape::Torch)) {
        CreateLightAtCursor();
    }
    if (input.KeyPress(VOICE_TEST_KEY)) {
        GameVoice::DebugPlayRandomForControlled(controlledCharacter == bigCharacter);
    }
}

// Câmera normal (zoom-base do painel + seguir os alvos) ou, em debug, câmera
// livre que anda para o lado em que o mouse está.
void StageState::UpdateCamera(float dt, InputManager& input) {
    if (Game::debugMode && debugFreeCam) {
        constexpr float kFreeCamPanSpeed = 1200.0f;   // px/s com o mouse na borda
        const float halfW = std::max(1, Game::GetInstance().GetWindowsWidth()) * 0.5f;
        const float halfH = std::max(1, Game::GetInstance().GetWindowsHeight()) * 0.5f;
        Camera::pos.x += (input.GetMouseX() - halfW) / halfW * kFreeCamPanSpeed * dt;
        Camera::pos.y += (input.GetMouseY() - halfH) / halfH * kFreeCamPanSpeed * dt;
        return;
    }
    Camera::SetBaseZoom(visionParams.cameraZoom);   // empurrado todo frame: a barra do painel fica ao vivo
    Camera::Update(dt);
}

// Monitor de FPS (debug): média móvel e texto colorido atualizado a cada 0,1 s.
void StageState::UpdateFpsHud(float dt) {
    if (dt > 1e-6f) {
        fpsSmoothed += (1.0f / dt - fpsSmoothed) * 0.10f;
    }
    fpsUiRefreshTimer += dt;
    if (!hudFps || fpsUiRefreshTimer < 0.10f) return;
    fpsUiRefreshTimer = 0.0f;

    Text* fpsText = hudFps->GetComponent<Text>();
    if (!fpsText) return;

    const float instantFps = (dt > 1e-6f) ? (1.0f / dt) : 0.0f;
    const bool steady = std::fabs(instantFps - fpsSmoothed) <= 2.0f;
    SDL_Color color{220, 80, 80, 240};
    const char* quality = "LOW";
    if (fpsSmoothed >= 58.0f && steady) {
        color = {90, 235, 120, 240};
        quality = "HEALTHY";
    } else if (fpsSmoothed >= 50.0f) {
        color = {245, 210, 90, 240};
        quality = steady ? "OK" : "UNSTEADY";
    }
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "FPS: %.1f (%s)", fpsSmoothed, quality);
    fpsText->SetColor(color);
    fpsText->SetText(buffer);
}

// Botão direito sobre um irmão prende a luz de preview nele (o mais perto do
// mouse se os dois estiverem embaixo); fora deles, solta.
void StageState::UpdatePreviewLightAnchor(InputManager& input) {
    if (!IsPartyReady() || !input.MousePress(SDL_BUTTON_RIGHT)) return;

    const float z = Camera::GetZoom();
    auto screenRectOf = [&](const GameObject* go) {
        const Rect& b = go->box;
        return Rect((b.x - Camera::pos.x) * z, (b.y - Camera::pos.y) * z, b.w * z, b.h * z);
    };
    const Vec2 mouse(static_cast<float>(input.GetMouseX()), static_cast<float>(input.GetMouseY()));
    const bool onBig   = bigCharacterObject && screenRectOf(bigCharacterObject).Contains(mouse);
    const bool onSmall = smallCharacterObject && screenRectOf(smallCharacterObject).Contains(mouse);

    if (!onBig && !onSmall) {
        previewLightLockedToPlayer = false;
        previewLightAnchorPlayer = nullptr;
        return;
    }
    if (onBig && onSmall) {
        const Vec2 mw = ScreenToWorld(mouse);
        previewLightAnchorPlayer = (mw.Distance(bigCharacterObject->box.Center()) <=
                                    mw.Distance(smallCharacterObject->box.Center()))
                                       ? bigCharacterObject : smallCharacterObject;
    } else {
        previewLightAnchorPlayer = onBig ? bigCharacterObject : smallCharacterObject;
    }
    previewLightLockedToPlayer = true;
}

// Suaviza a posição na tela da luz de preview (mouse ou irmão, sem entrar em
// parede) e a do isqueiro do irmãozão.
void StageState::UpdateLightSmoothing(float dt, InputManager& input) {
    const Vec2 mouse(static_cast<float>(input.GetMouseX()), static_cast<float>(input.GetMouseY()));
    Vec2 target = smoothedDynamicLightScreenPos;

    if (cursorPreviewLightEnabled) {
        target = mouse;
        if (previewLightLockedToPlayer && (!IsPartyReady() ||
            (previewLightAnchorPlayer != bigCharacterObject && previewLightAnchorPlayer != smallCharacterObject))) {
            previewLightLockedToPlayer = false;
            previewLightAnchorPlayer = nullptr;
        }
        if (previewLightLockedToPlayer && previewLightAnchorPlayer) {
            target = WorldToScreen(previewLightAnchorPlayer->box.Center());
        }

        // Dentro de parede: puxa para o tile andável mais perto, se houver caminho livre até ele.
        int tx = 0, ty = 0, ntx = 0, nty = 0;
        if (WorldToTile(ScreenToWorld(target), tx, ty) && !IsTileWalkable(tx, ty) &&
            FindNearestWalkableTile(tx, ty, ntx, nty)) {
            const Vec2 clampedWorld = TileCenterToWorld(ntx, nty);
            const bool blocked = hasSmoothedDynamicLight &&
                                 !HasWalkableLine(ScreenToWorld(smoothedDynamicLightScreenPos), clampedWorld);
            target = blocked ? smoothedDynamicLightScreenPos : WorldToScreen(clampedWorld);
        }
    } else if (!hasSmoothedDynamicLight) {
        target = mouse;   // 1º frame: começa do mouse para o suavizador existir
    }

    if (!hasSmoothedDynamicLight) {
        smoothedDynamicLightScreenPos = target;
        hasSmoothedDynamicLight = true;
    } else {
        smoothedDynamicLightScreenPos =
            SmoothTowards(smoothedDynamicLightScreenPos, target, lightMaskParams.lightTemporalSmoothing, dt);
    }

    const bool lightHidden = Character::player && Character::player->hidePersonalLight;
    if (!inventory.IsActiveLightLighter() || lightHidden || !bigCharacterObject) {
        hasSmoothedTorchLight = false;
        return;
    }
    const Vec2 torchTarget = WorldToScreen(bigCharacterObject->box.Center());
    if (!hasSmoothedTorchLight) {
        smoothedTorchLightScreenPos = torchTarget;
        hasSmoothedTorchLight = true;
    } else {
        smoothedTorchLightScreenPos =
            SmoothTowards(smoothedTorchLightScreenPos, torchTarget, lightMaskParams.lightTemporalSmoothing, dt);
    }
}

// Testa cada par de objetos com Collider ativo e notifica os dois quando colidem.
void StageState::CheckObjectCollisions() {
    for (size_t i = 0; i < objectArray.size(); i++) {
        GameObject* goA = objectArray[i].get();
        Collider* colA = goA->GetComponent<Collider>();
        if (!colA || !colA->IsEnabled()) continue;
        for (size_t j = i + 1; j < objectArray.size(); j++) {
            GameObject* goB = objectArray[j].get();
            Collider* colB = goB->GetComponent<Collider>();
            if (!colB || !colB->IsEnabled()) continue;
            if (Collision::IsColliding(colA->box, colB->box, goA->angleDeg * kDegToRad, goB->angleDeg * kDegToRad)) {
                goA->NotifyCollision(*goB);
                goB->NotifyCollision(*goA);
            }
        }
    }
}

// Sanidade baixa: overlay de rabiscos (quadro e alfa pela intensidade suavizada),
// vertigem da câmera e batimento cardíaco.
void StageState::UpdateSanityFeedback(float dt) {
    constexpr float kOverlayThreshold    = 95.0f;   // aparece abaixo disto
    constexpr float kOverlayMinIntensity = 0.22f;   // já visível ao cruzar o limiar
    constexpr float kHeartbeatStart      = 60.0f;   // batimento abaixo disto

    const float lowest = LowestSanity();

    if (SpriteRenderer* overlay = sanityOverlayObj ? sanityOverlayObj->GetComponent<SpriteRenderer>() : nullptr) {
        float target = 0.0f;
        if (lowest < kOverlayThreshold) {
            const float raw = 1.0f - lowest / kOverlayThreshold;   // 0 no limiar, 1 com sanidade 0
            target = kOverlayMinIntensity + raw * (1.0f - kOverlayMinIntensity);
        }
        sanityOverlaySmoothedIntensity += (target - sanityOverlaySmoothedIntensity) * std::min(1.0f, 4.0f * dt);

        const int frame = std::clamp(
            static_cast<int>(sanityOverlaySmoothedIntensity * (kSanityOverlayFrameCount - 1) + 0.5f),
            0, kSanityOverlayFrameCount - 1);
        if (frame != sanityOverlayFrameIndex) {
            sanityOverlayFrameIndex = frame;
            overlay->SetFrame(frame);
        }
        overlay->SetTint(255, 255, 255, static_cast<Uint8>(std::min(255.0f, 255.0f * sanityOverlaySmoothedIntensity)));
    }
    Camera::SetVertigo(sanityOverlaySmoothedIntensity);

    GameSfx::UpdateHeartbeat(std::min(1.0f, std::max(0.0f, (kHeartbeatStart - lowest) / kHeartbeatStart)));
}

// Derrota (sanidade zerada ou irmão perdido): cala o gameplay, volta o save ao
// checkpoint, registra a causa e empilha o EndState.
void StageState::CheckDefeat() {
    const bool sanityDefeat = (bigCharacter && bigCharacter->sanity <= 0.0f) ||
                              (smallCharacter && smallCharacter->sanity <= 0.0f);
    if (!sanityDefeat && IsPartyReady()) return;

    GameSfx::StopAllGameplay();   // o StageState para de atualizar: os loops não parariam sozinhos
    GameVoice::StopAll();
    SaveManager::RevertCurrentToCheckpoint();
    GameData::playerVictory = false;
    GameData::deathByMonster = (lastMonsterHitTimer > 0.0f);

    Telemetry::Fields f;
    f.Int("level", currentLevelIndex)
     .Str("cause", GameData::deathByMonster ? "monster" : "darkness")
     .Num("levelTime", telemetryLevelElapsed)
     .Num("walkedPx", telemetryWalkedPx)
     .Bool("lightOn", inventory.IsUsableLightActive());
    if (controlledCharacterObject) {
        const Vec2 c = controlledCharacterObject->box.Center();
        f.Pos("", c.x, c.y);
    }
    if (bigCharacter)   f.Num("sanityBig", bigCharacter->sanity);
    if (smallCharacter) f.Num("sanitySmall", smallCharacter->sanity);
    Telemetry::Event("death", f);

    popRequested = true;
    Game::GetInstance().Push(new EndState());
}

// ═════════════════════════════════════════════════════════════════════════════
//  Janelas
// ═════════════════════════════════════════════════════════════════════════════

// True se há ao menos uma janela e todas estão abertas (o monstro dominou o andar).
bool StageState::AreAllWindowsOpen() const {
    int total = 0, open = 0;
    for (const auto& go : objectArray) {
        Window* w = go ? go->GetComponent<Window>() : nullptr;
        if (!w) continue;
        ++total;
        const Window::WindowState s = w->GetState();
        if (s == Window::WindowState::OPEN || s == Window::WindowState::OPENING) ++open;
    }
    return total > 0 && open == total;
}

// Todas as janelas abertas trancam as velas (e apagam todas com um sopro); uma
// janela fechada destranca.
void StageState::UpdateWindowLockdown(float /*dt*/) {
    const bool allOpen = AreAllWindowsOpen();
    if (allOpen == windowLockdownActive) return;
    windowLockdownActive = allOpen;

    for (const auto& go : objectArray) {
        if (Candlestick* c = go ? go->GetComponent<Candlestick>() : nullptr) {
            c->SetLockdown(allOpen);
        }
    }
    if (allOpen) GameSfx::PlayCandleBlowOut();
}

