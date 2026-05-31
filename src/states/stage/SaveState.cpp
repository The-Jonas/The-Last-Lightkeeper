#include "states/stage/StageState.h"
#include "states/stage/FirstLoadData.h"
#include "core/SaveManager.h"
#include "core/Game.h"
#include "core/Resources.h"
#include "core/InputManager.h"
#include "engine/GameObject.h"
#include "gameplay/Character.h"
#include "gameplay/Box.h"
#include "gameplay/Candlestick.h"
#include "gameplay/Repairable.h"
#include "gameplay/ItemPickup.h"
#include "gameplay/Item.h"

#define INCLUDE_SDL_TTF
#include "SDL_include.h"

#include <algorithm>
#include <unordered_set>

namespace {

SavedCharacter CaptureCharacter(GameObject* object, Character* character) {
    SavedCharacter saved;
    if (!object || !character) {
        return saved;
    }
    saved.x = object->box.x;
    saved.y = object->box.y;
    saved.sanity = character->sanity;
    saved.isElevated = character->isElevated;
    saved.stairAnchorY = character->stairAnchorY;
    return saved;
}

void ApplyCharacter(const SavedCharacter& saved, GameObject* object, Character* character) {
    if (!object || !character) {
        return;
    }
    object->box.x = saved.x;
    object->box.y = saved.y;
    character->sanity = saved.sanity;
    character->isElevated = saved.isElevated;
    character->stairAnchorY = saved.stairAnchorY;
}

std::vector<ItemDef> BuildItemCatalog(const StageFirstLoadData& cfg) {
    std::vector<ItemDef> catalog = cfg.pickupCycle;
    catalog.push_back(cfg.startingFlashlight);
    return catalog;
}

} // namespace

SaveGameState StageState::CaptureSaveState() const {
    SaveGameState state;
    state.big = CaptureCharacter(bigCharacterObject, bigCharacter);
    state.small = CaptureCharacter(smallCharacterObject, smallCharacter);
    state.controlled = (controlledCharacter == bigCharacter) ? "big" : "small";
    state.partyMode = (partyMode == PartyMode::TOGETHER) ? "TOGETHER" : "INDEPENDENT";
    inventory.WriteToSave(state);
    state.escadaConsertada = level.escadaConsertada;

    std::unordered_set<int> alivePickupIds;
    for (const auto& go : objectArray) {
        if (!go) {
            continue;
        }
        if (ItemPickup* pickup = go->GetComponent<ItemPickup>()) {
            if (go->tiledId >= 0) {
                alivePickupIds.insert(go->tiledId);
            } else {
                SavedDroppedItem dropped;
                dropped.name = pickup->GetDef()->name;
                dropped.durability = pickup->GetDurability();
                dropped.x = go->box.x;
                dropped.y = go->box.y;
                dropped.heightLevel = pickup->GetHeightLevel();
                state.droppedItems.push_back(dropped);
            }
        }

        if (go->GetComponent<Box>() && go->tiledId >= 0) {
            state.boxPositions.push_back(SavedBoxPos{go->tiledId, go->box.x, go->box.y});
        }

        if (Candlestick* candle = go->GetComponent<Candlestick>()) {
            if (go->tiledId >= 0 && candle->IsLit()) {
                state.litCandleIds.push_back(go->tiledId);
            }
        }

        if (Repairable* repairable = go->GetComponent<Repairable>()) {
            if (go->tiledId >= 0 && repairable->IsRepaired()) {
                state.repairedIds.push_back(go->tiledId);
            }
        }
    }

    for (const EntitySpawn& spawn : level.entitySpawns) {
        if (spawn.type != "ItemSpawn" || spawn.tiledId < 0) {
            continue;
        }
        if (alivePickupIds.find(spawn.tiledId) == alivePickupIds.end()) {
            state.removedPickupIds.push_back(spawn.tiledId);
        }
    }

    return state;
}

void StageState::ApplySaveState(const SaveGameState& state) {
    const StageFirstLoadData cfg = LoadStageFirstLoadData();
    const std::vector<ItemDef> catalog = BuildItemCatalog(cfg);

    ApplyCharacter(state.big, bigCharacterObject, bigCharacter);
    ApplyCharacter(state.small, smallCharacterObject, smallCharacter);
    inventory.ReadFromSave(state, catalog);
    level.escadaConsertada = state.escadaConsertada;

    if (state.controlled == "small") {
        if (controlledCharacter == bigCharacter) {
            SwapControlledCharacter();
        }
    } else if (controlledCharacter == smallCharacter) {
        SwapControlledCharacter();
    }

    partyMode = (state.partyMode == "INDEPENDENT") ? PartyMode::INDEPENDENT : PartyMode::TOGETHER;

    std::unordered_set<int> removedIds(state.removedPickupIds.begin(), state.removedPickupIds.end());
    std::unordered_set<int> repairedIds(state.repairedIds.begin(), state.repairedIds.end());
    std::unordered_set<int> litIds(state.litCandleIds.begin(), state.litCandleIds.end());

    std::unordered_map<int, SavedBoxPos> boxPosById;
    for (const SavedBoxPos& boxPos : state.boxPositions) {
        boxPosById[boxPos.tiledId] = boxPos;
    }

    for (size_t i = 0; i < objectArray.size();) {
        GameObject* go = objectArray[i].get();
        if (!go) {
            ++i;
            continue;
        }

        if (ItemPickup* pickup = go->GetComponent<ItemPickup>()) {
            if (go->tiledId >= 0 && removedIds.count(go->tiledId) > 0) {
                pickup->Destroy();
                ++i;
                continue;
            }
            if (go->tiledId < 0) {
                pickup->Destroy();
                ++i;
                continue;
            }
        }

        if (go->GetComponent<Box>()) {
            auto it = boxPosById.find(go->tiledId);
            if (it != boxPosById.end()) {
                go->box.x = it->second.x;
                go->box.y = it->second.y;
            }
        }

        if (Candlestick* candle = go->GetComponent<Candlestick>()) {
            if (go->tiledId >= 0 && litIds.count(go->tiledId) > 0) {
                candle->SetLit(true);
            }
        }

        if (Repairable* repairable = go->GetComponent<Repairable>()) {
            if (go->tiledId >= 0 && repairedIds.count(go->tiledId) > 0) {
                repairable->ApplyRepairedState();
            }
        }

        ++i;
    }

    itemPickups.clear();
    for (const auto& go : objectArray) {
        if (!go || go->IsDead()) {
            continue;
        }
        if (ItemPickup* pickup = go->GetComponent<ItemPickup>()) {
            itemPickups.push_back(pickup);
        }
    }

    for (const SavedDroppedItem& dropped : state.droppedItems) {
        const ItemDef* def = nullptr;
        for (const ItemDef& itemDef : catalog) {
            if (itemDef.name == dropped.name) {
                def = &itemDef;
                break;
            }
        }
        if (!def) {
            continue;
        }
        ItemPickup* pickup = ItemPickup::Spawn(dropped.x, dropped.y, *def, dropped.durability, itemPickups);
        if (pickup) {
            pickup->SetHeightLevel(dropped.heightLevel);
            GameObject& itemObj = pickup->GetAssociated();
            itemObj.tiledId = -1;
            itemObj.z = 2;
            AddObject(&itemObj);
        }
    }

    RefreshCameraTargets();
    UpdateControlledCharacterVisuals();
}

bool StageState::SaveCurrentProgress() {
    SaveFile file;
    const StageFirstLoadData cfg = LoadStageFirstLoadData();
    if (!SaveManager::Load(file)) {
        file.version = SaveFile::kVersion;
        file.levelIndex = currentLevelIndex;
        file.levelPath = GetLevelDef(cfg, currentLevelIndex).mapPath;
        file.levelCheckpoint = CaptureSaveState();
    }
    file.current = CaptureSaveState();
    file.levelIndex = currentLevelIndex;
    file.levelPath = GetLevelDef(cfg, currentLevelIndex).mapPath;
    return SaveManager::Save(file);
}

bool StageState::SaveLevelCheckpoint() {
    const StageFirstLoadData cfg = LoadStageFirstLoadData();
    SaveFile file;
    file.version = SaveFile::kVersion;
    file.levelIndex = currentLevelIndex;
    file.levelPath = GetLevelDef(cfg, currentLevelIndex).mapPath;
    file.levelCheckpoint = CaptureSaveState();
    file.current = file.levelCheckpoint;
    return SaveManager::Save(file);
}

bool StageState::HandleQuitConfirmInput() {
    InputManager& input = InputManager::GetInstance();
    const int winW = Game::GetInstance().GetWindowsWidth();
    const int winH = Game::GetInstance().GetWindowsHeight();
    const int panelW = 760;
    const int panelH = 260;
    const int panelX = (winW - panelW) / 2;
    const int panelY = (winH - panelH) / 2;
    const int btnW = 260;
    const int btnH = 48;
    const int btnY = panelY + panelH - 80;

    quitConfirmSaveBtn = {panelX + 60, btnY, btnW, btnH};
    quitConfirmCancelBtn = {panelX + panelW - btnW - 60, btnY, btnW, btnH};

    if (input.KeyPress(SDLK_UP) || input.KeyPress(SDLK_DOWN) ||
        input.KeyPress(SDLK_LEFT) || input.KeyPress(SDLK_RIGHT) ||
        input.KeyPress(SDLK_TAB)) {
        quitConfirmSelection = 1 - quitConfirmSelection;
    }

    const int mx = input.GetMouseX();
    const int my = input.GetMouseY();
    SDL_Point mousePoint{mx, my};
    if (SDL_PointInRect(&mousePoint, &quitConfirmSaveBtn)) {
        quitConfirmSelection = 0;
    } else if (SDL_PointInRect(&mousePoint, &quitConfirmCancelBtn)) {
        quitConfirmSelection = 1;
    }

    if (input.KeyPress(SDLK_ESCAPE) || input.KeyPress(SDLK_n) || input.KeyPress(SDLK_2)) {
        quitConfirmOpen = false;
        return true;
    }

    const bool activateSelection =
        input.KeyPress(SDLK_RETURN) || input.KeyPress(SDLK_y) || input.KeyPress(SDLK_1) ||
        input.MousePress(SDL_BUTTON_LEFT);

    if (activateSelection) {
        if (input.MousePress(SDL_BUTTON_LEFT)) {
            if (SDL_PointInRect(&mousePoint, &quitConfirmSaveBtn)) {
                quitConfirmSelection = 0;
            } else if (SDL_PointInRect(&mousePoint, &quitConfirmCancelBtn)) {
                quitConfirmSelection = 1;
            } else {
                return false;
            }
        }

        if (quitConfirmSelection == 0) {
            SaveCurrentProgress();
            popRequested = true;
        }
        quitConfirmOpen = false;
        return true;
    }

    return false;
}

void StageState::RenderQuitConfirmModal(SDL_Renderer* renderer) {
    if (!renderer || !quitConfirmOpen) {
        return;
    }

    const int winW = Game::GetInstance().GetWindowsWidth();
    const int winH = Game::GetInstance().GetWindowsHeight();
    const int panelW = 760;
    const int panelH = 260;
    const int panelX = (winW - panelW) / 2;
    const int panelY = (winH - panelH) / 2;
    const int btnW = 260;
    const int btnH = 48;
    const int btnY = panelY + panelH - 80;

    quitConfirmSaveBtn = {panelX + 60, btnY, btnW, btnH};
    quitConfirmCancelBtn = {panelX + panelW - btnW - 60, btnY, btnW, btnH};

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    SDL_Rect backdrop{0, 0, winW, winH};
    SDL_RenderFillRect(renderer, &backdrop);

    SDL_Rect panel{panelX, panelY, panelW, panelH};
    SDL_SetRenderDrawColor(renderer, 35, 35, 42, 240);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 180, 160, 100, 255);
    SDL_RenderDrawRect(renderer, &panel);

    auto drawButton = [&](const SDL_Rect& rect, bool selected, const char* label) {
        SDL_SetRenderDrawColor(renderer, selected ? 200 : 80, selected ? 180 : 80, selected ? 100 : 80, 230);
        SDL_RenderFillRect(renderer, &rect);
        SDL_SetRenderDrawColor(renderer, 220, 200, 140, 255);
        SDL_RenderDrawRect(renderer, &rect);

        auto font = Resources::GetFont("Recursos/font/TradeWinds-Regular.ttf", 18);
        if (!font) {
            return;
        }
        SDL_Color color{240, 240, 240, 255};
        SDL_Surface* surface = TTF_RenderText_Blended(font.get(), label, color);
        if (!surface) {
            return;
        }
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_Rect dst{rect.x + (rect.w - surface->w) / 2, rect.y + (rect.h - surface->h) / 2, surface->w, surface->h};
        SDL_FreeSurface(surface);
        if (texture) {
            SDL_RenderCopy(renderer, texture, nullptr, &dst);
            SDL_DestroyTexture(texture);
        }
    };

    auto drawText = [&](const char* text, int x, int y, int size) {
        auto font = Resources::GetFont("Recursos/font/TradeWinds-Regular.ttf", size);
        if (!font) {
            return;
        }
        SDL_Color color{230, 230, 230, 255};
        SDL_Surface* surface = TTF_RenderText_Blended_Wrapped(font.get(), text, color, panelW - 80);
        if (!surface) {
            return;
        }
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_Rect dst{x, y, surface->w, surface->h};
        SDL_FreeSurface(surface);
        if (texture) {
            SDL_RenderCopy(renderer, texture, nullptr, &dst);
            SDL_DestroyTexture(texture);
        }
    };

    drawText("Are you sure? You may lose your unsaved progress doing this.", panelX + 40, panelY + 36, 22);
    drawButton(quitConfirmSaveBtn, quitConfirmSelection == 0, "Save and quit");
    drawButton(quitConfirmCancelBtn, quitConfirmSelection == 1, "Cancel");
}
