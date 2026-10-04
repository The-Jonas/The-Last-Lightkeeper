#include "states/stage/StageState.h"
#include "states/stage/FirstLoadData.h"
#include "core/Game.h"
#include "core/GameData.h"
#include "core/Resources.h"
#include "core/SaveManager.h"
#include "engine/GameObject.h"
#include "gameplay/Box.h"
#include "audio/GameSfx.h"
#include "audio/GameVoice.h"
#include "gameplay/Character.h"
#include "gameplay/HotbarComponent.h"
#include "gameplay/Item.h"
#include "states/EndState.h"
#include "core/Telemetry.h"
#include "states/LevelTransitionLoadingState.h"
#include "ui/Text.h"
#include "ui/InventoryWheel.h"
#include "ui/HorrorFx.h"
#include "world/SpawnFactory.h"

#define INCLUDE_SDL_TTF
#include "SDL_include.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_set>
#include <vector>

void StageState::SetInitialLevelIndex(int index) {
    currentLevelIndex = std::max(0, index);
}

void StageState::ShowLevelTitleBanner() {
    const StageFirstLoadData cfg = LoadStageFirstLoadData();
    levelTitleNumber = GetLevelDef(cfg, currentLevelIndex).displayNumber;
    levelTitleTimer = kLevelTitleDuration;
}

void StageState::ClearGameplayWorld() {
    Character::player = nullptr;
    Character::littleBrother = nullptr;
    objectArray.clear();
    // testShadowObjects guarda ponteiros CRUS para GameObjects de objectArray.
    // Ao limpar objectArray (shared_ptr) os objetos são destruídos; se estes
    // ponteiros ficarem, viram DANGLING e o loop de sombras (StageState::Render)
    // faz dynamic_cast em memória liberada → crash (use-after-free) após a
    // transição de fase. Limpar aqui junto com objectArray mantém o invariante.
    testShadowObjects.clear();
    monsterEchoes.clear();   // ondas dos passos do monstro do andar anterior
    monsterCache.reset();    // o objecto do monstro morreu com o objectArray
    bigCharacterObject = nullptr;
    smallCharacterObject = nullptr;
    bigCharacter = nullptr;
    smallCharacter = nullptr;
    controlledCharacterObject = nullptr;
    controlledCharacter = nullptr;
    companionCharacterObject = nullptr;
    companionCharacter = nullptr;
    hudLine1 = nullptr;
    hudLine2 = nullptr;
    hudLine3 = nullptr;
    hudFps = nullptr;
    hotbarObject = nullptr;
    inventoryWheelObject = nullptr;
    itemPickups.clear();
    jornals.clear();
    reachableJornal = nullptr;
    reachableCandle = nullptr;
    reachablePickup = nullptr;
    skippedPickupSpawnIds.clear();
    missedUniquePickupIdsAccum.clear();
    journalViewerOpen = false;
    journalViewerClosing = false;
    journalAnimTimer = 0.0f;
    journalCloseTimer = 0.0f;
    journalViewImagePath.clear();
    reachablePushBox = nullptr;
    GameSfx::NotifyBoxPushEnd();
    activePushBox = nullptr;
    wasPushingLastFrame = false;
    Box::SetActivePushTarget(nullptr);
    lights.clear();
    inventoryLightId = -1;
    hasSmoothedTorchLight = false;
    staticShadowEdges.clear();
    staticShadowEdgesBuilt = false;
    companionFollowPathWorld.clear();
    level.escadaConsertada = false;
    windowLockdownActive = false;
    sceneTransitionActive = false;
    if (sceneTransitionFrame) {
        SDL_DestroyTexture(sceneTransitionFrame);
        sceneTransitionFrame = nullptr;
    }
}

void StageState::BuildLevelWorld(const StageFirstLoadData& cfg, bool resetInventory) {
    GameObject* bigObject = new GameObject();
    Character* bigComp = new Character(*bigObject, "Recursos/img/personagens/Irmãozão", true);
    bigObject->AddComponent(bigComp);
    bigObject->z = 2;
    AddObject(bigObject);

    GameObject* smallObject = new GameObject();
    Character* smallComp = new Character(*smallObject, "Recursos/img/personagens/irmãozinho");
    smallObject->AddComponent(smallComp);
    smallObject->z = 2;
    AddObject(smallObject);

    const float centerX = cfg.navWorldW / 2.0f;
    const float centerY = cfg.navWorldH / 2.0f;

    float bigSpawnX = 0.0f;
    float bigSpawnY = 0.0f;
    float smallSpawnX = 0.0f;
    float smallSpawnY = 0.0f;
    bool bigFoundInTiled = false;
    bool smallFoundInTiled = false;

    for (const auto& spawn : level.entitySpawns) {
        if (spawn.type == "PlayerSpawn_Big") {
            bigSpawnX = spawn.x;
            bigSpawnY = spawn.y - bigObject->box.h;
            bigFoundInTiled = true;
            bigObject->tiledId = spawn.tiledId;
        } else if (spawn.type == "PlayerSpawn_Small") {
            smallSpawnX = spawn.x;
            smallSpawnY = spawn.y - smallObject->box.h;
            smallFoundInTiled = true;
            smallObject->tiledId = spawn.tiledId;
        } else if (spawn.type != "LevelTransition") {
            SpawnFactory::SpawnEntity(spawn, *this, cfg);
        }
    }

    for (const auto& zone : level.levelTransitionZones) {
        EntitySpawn spawn = zone;
        spawn.type = "LevelTransition";
        if (!spawn.properties.count("targetLevelIndex")) {
            spawn.properties["targetLevelIndex"] = GetCurrentLevelIndex() + 1;
        }
        SpawnFactory::SpawnEntity(spawn, *this, cfg);
    }

    if (!bigFoundInTiled) {
        bigSpawnX = centerX - bigObject->box.w * 0.5f;
        bigSpawnY = centerY - bigObject->box.h;
    }
    if (!smallFoundInTiled) {
        smallSpawnX = bigSpawnX - std::max(40.0f, smallObject->box.w * 1.2f);
        smallSpawnY = bigSpawnY;
    }

    bigObject->box.x = bigSpawnX;
    bigObject->box.y = bigSpawnY;
    smallObject->box.x = smallSpawnX;
    smallObject->box.y = smallSpawnY;

    previewLightLockedToPlayer = true;
    previewLightAnchorPlayer = bigObject;

    bigCharacterObject = bigObject;
    smallCharacterObject = smallObject;
    bigCharacter = bigComp;
    smallCharacter = smallComp;
    controlledCharacterObject = bigCharacterObject;
    controlledCharacter = bigCharacter;
    companionCharacterObject = smallCharacterObject;
    companionCharacter = smallCharacter;
    partyMode = PartyMode::TOGETHER;

    TriggerControlIndicator();   // anima o indicador ao iniciar/entrar no nível

    if (GetCurrentLevelIndex() == 1) {
        pendingWindowBreakDialogueTimer = 1.0f;
    }

    inventory.ClearAll();
    if (resetInventory) {
        inventory.ClearAll();
        inventory.AddItem(cfg.startingFlashlight, cfg.startingFlashlightDurability);
        // Começa APAGADO: acender é a primeira dica do 1º andar (tutorial/floors/Floor1.cpp).
        inventory.isLightToggledOn = false;
        if (bigComp) {
            bigComp->NotifyInventoryLightChanged();
        }
    }
    inventoryInitialized = true;

    // Roteiro de tutorial do andar (um save aplicado depois sobrescreve o que ele mudar).
    hints.EnterLevel(currentLevelIndex, *this);

    // HUD de desenvolvedor (instruções + FPS) — só criado em debugMode; jogadores
    // não veem essas linhas. As referências ficam nullptr fora de debug.
    if (Game::debugMode) {
        SDL_Color hudColor = {230, 230, 230, 220};

        hudLine1 = new GameObject();
        hudLine1->z = 100;
        hudLine1->AddComponent(new Text(*hudLine1, "Recursos/font/times.ttf", 18, Text::BLENDED,
                                         "WASD mover | 1/3 girar item | Ctrl trocar irmao | Q junto/separado",
                                        hudColor));
        AddObject(hudLine1);

        hudLine2 = new GameObject();
        hudLine2->z = 100;
        hudLine2->AddComponent(new Text(*hudLine2, "Recursos/font/times.ttf", 18, Text::BLENDED,
                                         "E interagir/pegar/acender/consertar | F5 pular level | F usar item/luz/oleo | Esc sair",
                                        hudColor));
        AddObject(hudLine2);

        hudLine3 = new GameObject();
        hudLine3->z = 100;
        hudLine3->AddComponent(new Text(*hudLine3, "Recursos/font/times.ttf", 18, Text::BLENDED,
                                         "T trovao | L luzes | O sombras | M musica | B fisica | X luz cursor | C criar luz | P painel luz | V fala irmao",
                                        hudColor));
        AddObject(hudLine3);

        hudFps = new GameObject();
        hudFps->z = 100;
        hudFps->AddComponent(new Text(*hudFps, "Recursos/font/times.ttf", 18, Text::BLENDED, "FPS: 60",
                                      hudColor));
        AddObject(hudFps);
    }

    GameObject* hotbarObj = new GameObject();
    HotbarComponent* hotbarComp = new HotbarComponent(*hotbarObj, inventory, bigCharacter, &controlledCharacter,
                                                itemPickups, [this](GameObject* obj) { AddObject(obj); },
                                                [this](Vec2 tl, float w, float h) {
                                                    return ClampPickupTopLeft(tl, w, h);
                                                });
    hotbarObj->AddComponent(hotbarComp);
    hotbarObj->z = 200;
    AddObject(hotbarObj);
    hotbarObject = hotbarObj;

    GameObject* wheelObj = new GameObject();
    InventoryWheel* wheelComp = new InventoryWheel(*wheelObj, inventory);
    wheelObj->AddComponent(wheelComp);
    wheelObj->z = 300;
    AddObject(wheelObj);
    inventoryWheelObject = wheelObj;

    RefreshCameraTargets();
    UpdateControlledCharacterVisuals();

    // ── Telemetria: comeco de andar ──────────────────────────────────────────
    // Os contadores do andar zeram aqui, e nao no fim do anterior: este ponto
    // e o unico por onde passam TODOS os arranques (jogo novo, continuar,
    // escada e reinicio no checkpoint).
    telemetryLevelElapsed = 0.0f;
    telemetryWalkedPx = 0.0f;
    telemetryLitSeconds = 0.0f;
    telemetryHasLastPos = false;
    telemetryStuckAccum = 0.0f;
    telemetrySampleTimer = 0.0f;
    telemetryPrevSanityBig = -1.0f;
    telemetryPrevSanitySmall = -1.0f;
    telemetryCliffAccum = 0.0f;
    telemetryCliffCooldown = 0.0f;
    telemetrySanityTier = 0;
    telemetryIdleAccum = 0.0f;
    telemetryIdleAnchor = Vec2(0.0f, 0.0f);
    Telemetry::SetIntense(false);   // o monstro do andar anterior ja nao conta
    Telemetry::Event("level_start", Telemetry::Fields()
        .Int("level", currentLevelIndex)
        .Str("map", GetLevelDef(cfg, currentLevelIndex).mapPath)
        .Str("loadMode", loadMode == LoadMode::Continue ? "continue" : "new")
        .Bool("resetInventory", resetInventory));
}

void StageState::BeginLevelTransition(int targetLevelIndex) {
    if (sceneTransitionActive) {
        return;   // já em transição
    }
    // Último andar: a escada leva ao EndState (vitória + créditos) com a mesma transição.
    BeginSceneTransition(targetLevelIndex, /*toEnd=*/currentLevelIndex == 2);
}

// ── Transição de cena "zoom-blur" estilo RE4 ────────────────────────────────
void StageState::BeginSceneTransition(int targetLevelIndex, bool toEnd) {
    if (sceneTransitionActive) {
        return;
    }
    sceneTransitionActive = true;
    sceneTransitionToEnd = toEnd;
    sceneTransitionTargetLevel = targetLevelIndex;
    sceneTransitionTimer = 0.0f;
    if (sceneTransitionFrame) {
        SDL_DestroyTexture(sceneTransitionFrame);
        sceneTransitionFrame = nullptr;   // (re)capturado no próximo Render
    }
    GameVoice::StopAll();   // corta falas durante a transição
    // O mundo congela durante a transição — silencia os passos (jogador e monstro)
    // e demais loops de gameplay para não ficarem tocando parados.
    GameSfx::StopAllGameplay();
    GameSfx::StopMonsterFootsteps();

    // Último andar: um grande trovão (trovao_1 + trovao_2 juntos, volume máximo)
    // marca a fuga para a luz do farol enquanto a tela clareia.
    if (toEnd) {
        GameSfx::PlayBigThunder();
    }
}

void StageState::CaptureSceneFrame(SDL_Renderer* renderer) {
    if (!renderer || !renderTarget) {
        return;   // sem alvo de cena não há o que congelar
    }
    const int winW = Game::GetInstance().GetWindowsWidth();
    const int winH = Game::GetInstance().GetWindowsHeight();

    if (sceneTransitionFrame) {
        SDL_DestroyTexture(sceneTransitionFrame);
        sceneTransitionFrame = nullptr;
    }

    // Filtragem linear → o zoom do quadro congelado fica suave (aparência borrada).
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    // Congela copiando o ALVO DA CENA (GPU→GPU). Evita SDL_RenderReadPixels do
    // backbuffer padrão, que é instável e chega a QUEBRAR em alguns renderers
    // (Direct3D no Windows) — era a causa do crash ao entrar no 2º andar.
    sceneTransitionFrame = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                             SDL_TEXTUREACCESS_TARGET, winW, winH);
    if (!sceneTransitionFrame) {
        return;
    }
    SDL_Texture* prev = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, sceneTransitionFrame);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, renderTarget, nullptr, nullptr);

    // ── O ALFA DO QUADRO CONGELADO TEM DE SER OPACO ─────────────────────────
    // O alvo da cena traz um CARIMBO no canal alfa por cima dos irmaos: e o
    // sinal que diz ao shader "isto e um personagem, mantem-no nitido e
    // devolve-lhe a cor". Nao e transparencia — mas o quadro congelado e
    // desenhado com BLEND, e ali o alfa passa a valer como transparencia: os
    // irmaos ficavam buracos no quadro e via-se o fundo atraves deles (branco,
    // porque a cor de limpeza ficava branca do frame anterior). Era o clarao
    // que aparecia ao chegar ao fim da escada.
    //
    // Aqui forca-se o alfa a 255 em todo o quadro, deixando o RGB intacto.
    static SDL_BlendMode opaqueAlpha = SDL_ComposeCustomBlendMode(
        SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD,   // RGB: fica o que la esta
        SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ZERO, SDL_BLENDOPERATION_ADD);  // alfa: passa a ser o da cor
    if (opaqueAlpha != SDL_BLENDMODE_INVALID) {
        SDL_SetRenderDrawBlendMode(renderer, opaqueAlpha);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        const SDL_Rect full{0, 0, winW, winH};
        SDL_RenderFillRect(renderer, &full);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }

    SDL_SetRenderTarget(renderer, prev);
    SDL_SetTextureBlendMode(sceneTransitionFrame, SDL_BLENDMODE_BLEND);
}

void StageState::RenderSceneTransition(SDL_Renderer* renderer) {
    if (!renderer || !sceneTransitionFrame) {
        return;
    }
    SDL_SetRenderTarget(renderer, nullptr);   // desenha direto na tela

    const int winW = Game::GetInstance().GetWindowsWidth();
    const int winH = Game::GetInstance().GetWindowsHeight();

    const float t = std::min(1.0f, sceneTransitionTimer / SceneTransitionDuration());
    const float e = t * t;               // ease-in (acelera o zoom)

    // Zoom-blur radial por acumulação: desenha o quadro congelado várias vezes,
    // cada cópia um pouco mais ampliada a partir do centro e mais transparente —
    // a sobreposição vira o "borrão" de zoom.
    constexpr int   kSamples = 14;
    constexpr float kMaxZoom = 0.32f;    // até +32% no fim
    for (int i = 0; i < kSamples; ++i) {
        const float f = (kSamples > 1) ? static_cast<float>(i) / (kSamples - 1) : 0.0f;
        const float zoom = 1.0f + kMaxZoom * e * f;
        const int w = static_cast<int>(winW * zoom);
        const int h = static_cast<int>(winH * zoom);
        const SDL_Rect dst{(winW - w) / 2, (winH - h) / 2, w, h};
        const Uint8 a = (i == 0) ? 255 : static_cast<Uint8>(150.0f * (1.0f - f));
        SDL_SetTextureAlphaMod(sceneTransitionFrame, a);
        SDL_RenderCopy(renderer, sceneTransitionFrame, nullptr, &dst);
    }
    SDL_SetTextureAlphaMod(sceneTransitionFrame, 255);

    // Escadas normais: escurece até o PRETO total antes do fim (corte limpo).
    // Último andar: a tela CLAREIA até o BRANCO total — a fuga para a luz do farol.
    const Uint8 fadeChannel = sceneTransitionToEnd ? 255 : 0;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, fadeChannel, fadeChannel, fadeChannel,
                           static_cast<Uint8>(std::min(255.0f, 300.0f * e)));
    const SDL_Rect full{0, 0, winW, winH};
    SDL_RenderFillRect(renderer, &full);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    // PRETO, nao branco: o `SDL_RenderClear` do inicio do frame seguinte usa a
    // ultima cor de desenho. Deixa-la branca fazia o ecra comecar BRANCO, e
    // entao qualquer buraco no quadro congelado (o carimbo dos irmaos) virava
    // um clarao com a forma deles. Era o "os dois ficam brancos" ao chegar ao
    // fim da escada.
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
}

void StageState::UpdateSceneTransition(float dt) {
    sceneTransitionTimer += dt;
    if (sceneTransitionTimer < SceneTransitionDuration()) {
        return;
    }

    // Efeito terminou → executa a transição real e libera o quadro congelado.
    if (sceneTransitionFrame) {
        SDL_DestroyTexture(sceneTransitionFrame);
        sceneTransitionFrame = nullptr;
    }
    sceneTransitionActive = false;

    if (sceneTransitionToEnd) {
        GameSfx::StopAllGameplay();
        GameVoice::StopAll();
        Telemetry::Event("victory", Telemetry::Fields()
            .Int("level", currentLevelIndex)
            .Num("levelTime", telemetryLevelElapsed));
        GameData::playerVictory = true;
        GameData::deathByMonster = false;
        popRequested = true;
        Game::GetInstance().Push(new EndState());
    } else {
        Game::GetInstance().Push(new LevelTransitionLoadingState(this, sceneTransitionTargetLevel));
    }
}

void StageState::TransitionToLevel(int targetLevelIndex) {
    const StageFirstLoadData cfg = LoadStageFirstLoadData();
    if (targetLevelIndex >= GetLevelCount(cfg)) {
        Telemetry::Event("victory", Telemetry::Fields().Int("level", currentLevelIndex));
        GameData::playerVictory = true;
        SaveCurrentProgress();
        popRequested = true;
        Game::GetInstance().Push(new EndState());
        return;
    }

    Telemetry::Event("level_end", Telemetry::Fields()
        .Int("level", currentLevelIndex)
        .Int("nextLevel", targetLevelIndex)
        .Num("levelTime", telemetryLevelElapsed)
        .Num("walkedPx", telemetryWalkedPx)
        .Num("litSeconds", telemetryLitSeconds));

    MarkMissedUniquePickupsOnLevelLeave();
    SaveGameState preserved = CaptureSaveState();
    preserved.missedUniquePickupIds = missedUniquePickupIdsAccum;
    std::unordered_set<int> skippedForNextLevel = skippedPickupSpawnIds;
    for (int id : preserved.removedPickupIds) {
        skippedForNextLevel.insert(id);
    }
    for (int id : preserved.missedUniquePickupIds) {
        skippedForNextLevel.insert(id);
    }

    ClearGameplayWorld();
    currentLevelIndex = targetLevelIndex;
    const LevelDef& levelDef = GetLevelDef(cfg, currentLevelIndex);
    level.LoadLevel(levelDef.mapPath, Game::GetInstance().GetRenderer());
    level.escadaConsertada = false;
    // A arte do novo andar tem outro retangulo: reinstala o enquadramento
    // (zoom-base + limites) antes de o nivel comecar a desenhar.
    ApplyCameraFraming(true);

    BuildLevelWorld(cfg, false);
    StartArray();
    RemoveCollectedJornalsFromWorld();
    ApplyLitCandleIds(preserved.litCandleIds, false);

    std::vector<ItemDef> catalog = cfg.pickupCycle;
    catalog.push_back(cfg.startingFlashlight);
    inventory.ReadFromSave(preserved, catalog);
    skippedPickupSpawnIds = std::move(skippedForNextLevel);
    if (bigCharacter) {
        bigCharacter->NotifyInventoryLightChanged();
    }

    if (preserved.controlled == "small") {
        if (controlledCharacter == bigCharacter) {
            SwapControlledCharacter();
        }
    } else if (controlledCharacter == smallCharacter) {
        SwapControlledCharacter();
    }

    // Per-level forced starting control overrides the carried-over control when
    // entering a fresh floor (e.g. level 2 begins on the little brother).
    if (levelDef.startControlled == "small" && controlledCharacter == bigCharacter) {
        SwapControlledCharacter();
    } else if (levelDef.startControlled == "big" && controlledCharacter == smallCharacter) {
        SwapControlledCharacter();
    }

    partyMode = (preserved.partyMode == "INDEPENDENT") ? PartyMode::INDEPENDENT : PartyMode::TOGETHER;

    ShowLevelTitleBanner();
    SaveLevelCheckpoint();
}

namespace {

// 1 → "I", 4 → "IV"… (andares do farol; acima de 39 volta a ser número).
std::string ToRoman(int n) {
    if (n <= 0 || n >= 40) return std::to_string(n);
    static const std::pair<int, const char*> kTable[] = {{10, "X"}, {9, "IX"}, {5, "V"}, {4, "IV"}, {1, "I"}};
    std::string out;
    for (const auto& [value, glyph] : kTable) {
        while (n >= value) { out += glyph; n -= value; }
    }
    return out;
}

// Texto em textura com alpha aplicado (TTF ignora o alpha da cor). Devolve w/h.
SDL_Texture* MakeTitleText(SDL_Renderer* r, TTF_Font* font, const char* text, SDL_Color color, int& w, int& h) {
    w = h = 0;
    SDL_Surface* s = TTF_RenderUTF8_Blended(font, text, color);
    if (!s) return nullptr;
    SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
    w = s->w;
    h = s->h;
    SDL_FreeSurface(s);
    return t;
}

// Faixa horizontal escura que some para cima e para baixo (gradiente em 2 quads).
void DrawTitleBand(SDL_Renderer* r, float winW, float cy, float halfH, Uint8 alpha) {
    auto v = [](float x, float y, Uint8 a) {
        SDL_Vertex vx;
        vx.position = {x, y};
        vx.color = {0, 0, 0, a};
        vx.tex_coord = {0.0f, 0.0f};
        return vx;
    };
    const SDL_Vertex verts[6] = {
        v(0.0f, cy - halfH, 0), v(winW, cy - halfH, 0),
        v(0.0f, cy, alpha),     v(winW, cy, alpha),
        v(0.0f, cy + halfH, 0), v(winW, cy + halfH, 0),
    };
    const int idx[12] = {0, 1, 2, 1, 3, 2, 2, 3, 4, 3, 5, 4};
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(r, nullptr, verts, 6, idx, 12);
}

}  // namespace

// Título do andar: faixa escura, "A N D A R" pequeno e o numeral romano grande,
// com sombra de sangue, risco embaixo e um tremor leve. Entra assentando
// (escala 1.08 → 1.0) com fade, segura e sai com fade — nunca corta seco.
void StageState::RenderLevelTitleBanner(SDL_Renderer* renderer) {
    if (!renderer || levelTitleTimer <= 0.0f) {
        return;
    }

    const float elapsed = kLevelTitleDuration - levelTitleTimer;
    float a = 1.0f;
    if (elapsed < kLevelTitleFadeIn)              a = elapsed / kLevelTitleFadeIn;
    else if (levelTitleTimer < kLevelTitleFadeOut) a = levelTitleTimer / kLevelTitleFadeOut;
    a = std::max(0.0f, std::min(1.0f, a));
    a = a * a * (3.0f - 2.0f * a);                                       // smoothstep
    const float settle = std::min(1.0f, elapsed / kLevelTitleFadeIn);
    const float scale = 1.08f - 0.08f * settle * settle * (3.0f - 2.0f * settle);

    const float u = Game::UiScale();
    const float winW = static_cast<float>(Game::GetInstance().GetWindowsWidth());
    const float winH = static_cast<float>(Game::GetInstance().GetWindowsHeight());
    const float cy = winH * 0.42f;

    auto smallFont = Resources::GetFont("Recursos/font/times.ttf", std::max(14, static_cast<int>(std::lround(30.0f * u))));
    auto bigFont   = Resources::GetFont("Recursos/font/times.ttf", std::max(40, static_cast<int>(std::lround(150.0f * u))));
    if (!smallFont || !bigFont) {
        return;
    }

    DrawTitleBand(renderer, winW, cy, 190.0f * u, static_cast<Uint8>(200.0f * a));

    const std::string numeral = ToRoman(levelTitleNumber);
    int sw = 0, sh = 0, bw = 0, bh = 0;
    SDL_Texture* small  = MakeTitleText(renderer, smallFont.get(), "A N D A R", SDL_Color{190, 170, 140, 255}, sw, sh);
    SDL_Texture* big    = MakeTitleText(renderer, bigFont.get(), numeral.c_str(), SDL_Color{232, 218, 190, 255}, bw, bh);
    SDL_Texture* shadow = MakeTitleText(renderer, bigFont.get(), numeral.c_str(), SDL_Color{110, 12, 10, 255}, bw, bh);

    const float bigW = bw * scale, bigH = bh * scale;
    const float bigTop = cy - bigH * 0.5f + 12.0f * u;
    const float jx = HorrorFx::Jitter(elapsed, 0.5f) * 1.5f * u;
    const float jy = HorrorFx::Jitter(elapsed, 0.9f) * 1.5f * u;

    if (small) {
        SDL_SetTextureAlphaMod(small, static_cast<Uint8>(255.0f * a));
        const SDL_FRect d{(winW - sw) * 0.5f, bigTop - sh - 4.0f * u, static_cast<float>(sw), static_cast<float>(sh)};
        SDL_RenderCopyF(renderer, small, nullptr, &d);
    }
    if (shadow) {
        SDL_SetTextureAlphaMod(shadow, static_cast<Uint8>(200.0f * a));
        const SDL_FRect d{(winW - bigW) * 0.5f + 5.0f * u - jx, bigTop + 5.0f * u - jy, bigW, bigH};
        SDL_RenderCopyF(renderer, shadow, nullptr, &d);
    }
    if (big) {
        SDL_SetTextureAlphaMod(big, static_cast<Uint8>(255.0f * a));
        const SDL_FRect d{(winW - bigW) * 0.5f + jx, bigTop + jy, bigW, bigH};
        SDL_RenderCopyF(renderer, big, nullptr, &d);
    }

    const float lineHalf = std::max(bigW, 260.0f * u) * 0.75f;
    HorrorFx::DrawScratchLine(renderer, winW * 0.5f - lineHalf, winW * 0.5f + lineHalf, bigTop + bigH * 0.92f,
                              4.0f, SDL_Color{110, 12, 10, static_cast<Uint8>(230.0f * a)}, elapsed);

    if (small)  SDL_DestroyTexture(small);
    if (big)    SDL_DestroyTexture(big);
    if (shadow) SDL_DestroyTexture(shadow);
}


