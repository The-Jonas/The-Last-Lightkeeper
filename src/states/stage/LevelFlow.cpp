#include "states/stage/StageState.h"
#include "states/stage/FirstLoadData.h"
#include "core/Game.h"
#include "core/GameData.h"
#include "core/Resources.h"
#include "core/SaveManager.h"
#include "engine/GameObject.h"
#include "gameplay/Box.h"
#include "audio/GameSfx.h"
#include "gameplay/Character.h"
#include "gameplay/HotbarComponent.h"
#include "gameplay/Item.h"
#include "states/EndState.h"
#include "states/LevelTransitionLoadingState.h"
#include "ui/Text.h"
#include "ui/InventoryWheel.h"
#include "world/SpawnFactory.h"

#define INCLUDE_SDL_TTF
#include "SDL_include.h"

#include <algorithm>
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

    inventory.ClearAll();
    if (resetInventory) {
        inventory.ClearAll();
        inventory.AddItem(cfg.startingFlashlight, cfg.startingFlashlightDurability);
        if (bigComp) {
            bigComp->NotifyInventoryLightChanged();
        }
        inventory.isLightToggledOn = false;
    }
    inventoryInitialized = true;

    SDL_Color hudColor = {230, 230, 230, 220};

    hudLine1 = new GameObject();
    hudLine1->z = 100;
    hudLine1->AddComponent(new Text(*hudLine1, "Recursos/font/TradeWinds-Regular.ttf", 18, Text::BLENDED,
                                     "WASD mover | 1/3 girar item | Ctrl trocar irmao | Q junto/separado",
                                    hudColor));
    AddObject(hudLine1);

    hudLine2 = new GameObject();
    hudLine2->z = 100;
    hudLine2->AddComponent(new Text(*hudLine2, "Recursos/font/TradeWinds-Regular.ttf", 18, Text::BLENDED,
                                     "E interagir/pegar/acender/consertar | F usar item/luz/oleo | Esc sair",
                                    hudColor));
    AddObject(hudLine2);

    hudLine3 = new GameObject();
    hudLine3->z = 100;
    hudLine3->AddComponent(new Text(*hudLine3, "Recursos/font/TradeWinds-Regular.ttf", 18, Text::BLENDED,
                                     "T trovao | L luzes | O sombras | M musica | B fisica | X luz cursor | C criar luz | P painel luz",
                                    hudColor));
    AddObject(hudLine3);

    hudFps = new GameObject();
    hudFps->z = 100;
    hudFps->AddComponent(new Text(*hudFps, "Recursos/font/TradeWinds-Regular.ttf", 18, Text::BLENDED, "FPS: 60",
                                  hudColor));
    AddObject(hudFps);

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
}

void StageState::BeginLevelTransition(int targetLevelIndex) {
    Game::GetInstance().Push(new LevelTransitionLoadingState(this, targetLevelIndex));
}

void StageState::TransitionToLevel(int targetLevelIndex) {
    const StageFirstLoadData cfg = LoadStageFirstLoadData();
    if (targetLevelIndex >= GetLevelCount(cfg)) {
        GameData::playerVictory = true;
        SaveCurrentProgress();
        popRequested = true;
        Game::GetInstance().Push(new EndState());
        return;
    }

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

    BuildLevelWorld(cfg, false);
    StartArray();
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
    partyMode = (preserved.partyMode == "INDEPENDENT") ? PartyMode::INDEPENDENT : PartyMode::TOGETHER;

    ShowLevelTitleBanner();
    SaveLevelCheckpoint();
}

void StageState::RenderLevelTitleBanner(SDL_Renderer* renderer) {
    if (!renderer || levelTitleTimer <= 0.0f) {
        return;
    }

    const int winW = Game::GetInstance().GetWindowsWidth();
    const int winH = Game::GetInstance().GetWindowsHeight();
    const float t = levelTitleTimer / kLevelTitleDuration;
    const Uint8 alpha = static_cast<Uint8>(255.0f * std::min(1.0f, t * 2.0f));

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, static_cast<Uint8>(alpha * 0.45f));
    const SDL_Rect backdrop{0, 0, winW, winH};
    SDL_RenderFillRect(renderer, &backdrop);

    char label[32];
    std::snprintf(label, sizeof(label), "Level %d", levelTitleNumber);

    auto font = Resources::GetFont("Recursos/font/TradeWinds-Regular.ttf", 72);
    if (!font) {
        return;
    }

    SDL_Color color{230, 220, 180, alpha};
    SDL_Surface* surface = TTF_RenderText_Blended(font.get(), label, color);
    if (!surface) {
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect dst{(winW - surface->w) / 2, (winH - surface->h) / 2, surface->w, surface->h};
    SDL_FreeSurface(surface);
    if (texture) {
        SDL_RenderCopy(renderer, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }
}
