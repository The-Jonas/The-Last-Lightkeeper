#include "states/stage/StageState.h"
#include "states/stage/FirstLoadData.h"
#include "states/LoadingState.h"
#include "core/SaveManager.h"
#include "core/Game.h"
#include "core/Resources.h"
#include "core/InputManager.h"
#include "engine/GameObject.h"
#include "gameplay/Character.h"
#include "gameplay/Box.h"
#include "gameplay/Candlestick.h"
#include "gameplay/Window.h"
#include "gameplay/Repairable.h"
#include "gameplay/ItemPickup.h"
#include "gameplay/Item.h"
#include "audio/GameVoice.h"

#define INCLUDE_SDL_TTF
#include "SDL_include.h"

#include <algorithm>
#include <cstdio>
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
    state.missedUniquePickupIds = missedUniquePickupIdsAccum;
    state.escadaConsertada = level.escadaConsertada;

    std::unordered_set<int> alivePickupIds;
    for (const auto& go : objectArray) {
        if (!go || go->IsDead()) {
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

void StageState::RegisterAllCandleLights() {
    for (const auto& goPtr : objectArray) {
        GameObject* go = goPtr.get();
        if (!go || go->IsDead()) {
            continue;
        }
        if (Candlestick* candle = go->GetComponent<Candlestick>()) {
            candle->EnsureLightRegistered(*this);
        }
    }
}

void StageState::ApplyLitCandleIds(const std::vector<int>& litIds, bool extinguishOthers) {
    RegisterAllCandleLights();
    std::unordered_set<int> litSet(litIds.begin(), litIds.end());
    for (const auto& goPtr : objectArray) {
        GameObject* go = goPtr.get();
        if (!go || go->IsDead() || go->tiledId < 0) {
            continue;
        }
        Candlestick* candle = go->GetComponent<Candlestick>();
        if (!candle) {
            continue;
        }
        if (litSet.count(go->tiledId) > 0) {
            candle->SetLit(true);
        } else if (extinguishOthers) {
            candle->SetLit(false);
        }
    }
}

void StageState::ApplySaveState(const SaveGameState& state) {
    const StageFirstLoadData cfg = LoadStageFirstLoadData();
    const std::vector<ItemDef> catalog = BuildItemCatalog(cfg);

    ApplyCharacter(state.big, bigCharacterObject, bigCharacter);
    ApplyCharacter(state.small, smallCharacterObject, smallCharacter);
    inventory.ReadFromSave(state, catalog);
    missedUniquePickupIdsAccum = state.missedUniquePickupIds;
    MergeSkippedPickupIds(state.removedPickupIds, state.missedUniquePickupIds);
    if (bigCharacter) {
        bigCharacter->NotifyInventoryLightChanged();
    }
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
            if (go->tiledId >= 0 && (removedIds.count(go->tiledId) > 0 ||
                                      skippedPickupSpawnIds.count(go->tiledId) > 0)) {
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

        if (Repairable* repairable = go->GetComponent<Repairable>()) {
            if (go->tiledId >= 0 && repairedIds.count(go->tiledId) > 0) {
                repairable->ApplyRepairedState();
            }
        }

        ++i;
    }

    ApplyLitCandleIds(state.litCandleIds);

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

        auto font = Resources::GetFont("Recursos/font/times.ttf", 18);
        if (!font) {
            return;
        }
        SDL_Color color{240, 240, 240, 255};
        SDL_Surface* surface = TTF_RenderUTF8_Blended(font.get(), label, color);
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
        auto font = Resources::GetFont("Recursos/font/times.ttf", size);
        if (!font) {
            return;
        }
        SDL_Color color{230, 230, 230, 255};
        SDL_Surface* surface = TTF_RenderUTF8_Blended_Wrapped(font.get(), text, color, panelW - 80);
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

    drawText("Tem certeza? Voce pode perder o progresso nao salvo.", panelX + 40, panelY + 36, 22);
    drawButton(quitConfirmSaveBtn, quitConfirmSelection == 0, "Salvar e sair");
    drawButton(quitConfirmCancelBtn, quitConfirmSelection == 1, "Cancelar");
}

void StageState::RenderInteractionPrompt(SDL_Renderer* renderer) {
    if (!renderer) {
        return;
    }
    // Só o irmão maior interage; e não mostra durante menu/modais/jornal.
    if (controlledCharacter != bigCharacter || IsPlayerInputFrozen()) {
        return;
    }

    // Escolhe a AÇÃO do interagível de maior prioridade ao alcance.
    const char* action = nullptr;
    if (bigCharacter && bigCharacter->isHidden) {
        // Escondido no armário: mantém SEMPRE o aviso de como sair.
        action = "Sair do esconderijo";
    } else if (!activePushBox) {
        if (reachablePushBox) {
            action = "Empurrar";
        } else if (reachableCloset) {
            action = "Esconder";
        } else if (reachableRepairable) {  
            action = "Consertar";
        } else if (reachableJornal) {
            action = "Ler";
        } else if (reachableCandle) {
            action = reachableCandle->IsLit() ? "Apagar" : "Acender";
        } else if (reachableWindow) {
            if (reachableWindow->GetState() == Window::WindowState::OPEN) {
                action = "Fechar";
            }
        } else if (reachableRadio) {                          
            action = reachableRadio->IsPlaying() ? "Desligar" : "Ligar";
        } else if (reachablePickup && IsPickupStillTracked(reachablePickup) &&
                   !IsPickupBlocked(reachablePickup)) {
            action = "Pegar";
        }
    }
    if (!action) {
        return;
    }

    // O prefixo segue a tecla atualmente vinculada à ação Interagir (4.1).
    const char* keyName = SDL_GetKeyName(InputManager::GetInstance().GetBinding(GameAction::Interact));
    if (!keyName || keyName[0] == '\0') {
        keyName = "E";
    }
    char label[64];
    std::snprintf(label, sizeof(label), "[%s] %s", keyName, action);

    auto font = Resources::GetFont("Recursos/font/times.ttf", 24);
    if (!font) {
        return;
    }
    SDL_Color color{240, 235, 220, 255};
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font.get(), label, color);
    if (!surface) {
        return;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    const int tw = surface->w;
    const int th = surface->h;
    SDL_FreeSurface(surface);
    if (!texture) {
        return;
    }

    Game& game = Game::GetInstance();
    const int winW = game.GetWindowsWidth();
    const int winH = game.GetWindowsHeight();
    const int padX = 18;
    const int padY = 10;
    const int textX = (winW - tw) / 2;
    const int textY = winH - 110;
    const SDL_Rect bg{textX - padX, textY - padY, tw + padX * 2, th + padY * 2};

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 20, 20, 26, 185);
    SDL_RenderFillRect(renderer, &bg);
    SDL_SetRenderDrawColor(renderer, 200, 180, 110, 200);
    SDL_RenderDrawRect(renderer, &bg);

    const SDL_Rect dst{textX, textY, tw, th};
    SDL_RenderCopy(renderer, texture, nullptr, &dst);
    SDL_DestroyTexture(texture);
}

namespace {
const char* kPauseMenuLabels[] = {"Continuar", "Salvar", "Configuracoes", "Reiniciar nivel", "Sair"};
}

void StageState::RestartLevelFromCheckpoint() {
    // Volta o estado "current" para o início do nível e recarrega via Continue.
    SaveManager::RevertCurrentToCheckpoint();
    popRequested = true;
    Game::GetInstance().Push(new LoadingState(StageState::LoadMode::Continue));
}

void StageState::HandlePauseMenuInput() {
    InputManager& input = InputManager::GetInstance();

    if (input.KeyPress(SDLK_ESCAPE)) {   // ESC fecha o menu (retoma)
        pauseMenuOpen = false;
        return;
    }

    if (input.KeyPress(SDLK_UP) || input.KeyPress(SDLK_w)) {
        pauseMenuSelection = (pauseMenuSelection + kPauseMenuItemCount - 1) % kPauseMenuItemCount;
    }
    if (input.KeyPress(SDLK_DOWN) || input.KeyPress(SDLK_s)) {
        pauseMenuSelection = (pauseMenuSelection + 1) % kPauseMenuItemCount;
    }

    // Hover do mouse seleciona (rects definidos no render do frame anterior).
    SDL_Point mp{input.GetMouseX(), input.GetMouseY()};
    for (int i = 0; i < kPauseMenuItemCount; ++i) {
        if (SDL_PointInRect(&mp, &pauseMenuItemRects[i])) {
            pauseMenuSelection = i;
        }
    }

    bool activate = input.KeyPress(SDLK_RETURN) || input.KeyPress(SDLK_SPACE) || input.KeyPress(SDLK_f);
    if (input.MousePress(SDL_BUTTON_LEFT)) {
        for (int i = 0; i < kPauseMenuItemCount; ++i) {
            if (SDL_PointInRect(&mp, &pauseMenuItemRects[i])) {
                pauseMenuSelection = i;
                activate = true;
                break;
            }
        }
    }
    if (!activate) {
        return;
    }

    switch (pauseMenuSelection) {
    case 0:  // Continuar
        pauseMenuOpen = false;
        break;
    case 1:  // Salvar
        SaveCurrentProgress();
        ShowSaveToast();
        pauseMenuOpen = false;
        break;
    case 2:  // Configuracoes
        settingsPanelOpen = true;
        settingsSelection = 0;
        break;
    case 3:  // Reiniciar nivel
        pauseMenuOpen = false;
        RestartLevelFromCheckpoint();
        break;
    case 4:  // Sair
        pauseMenuOpen = false;
        quitConfirmOpen = true;
        quitConfirmSelection = 0;
        break;
    default:
        break;
    }
}

// (O losango decorativo foi substituído pela imagem "linha_divisoria.png".)

// #19 Gera o desfoque do cenário (GPU): reduz o alvo da cena (renderTarget) para
// um alvo pequeno; ao ampliar depois com filtragem linear surge o borrão barato
// (sem SDL_RenderReadPixels, que quebra em alguns backends). Retorna false se não
// houver cena para borrar. NÃO desenha nada na tela — só prepara pauseBlurTex.
bool StageState::BuildPauseBlurTexture(SDL_Renderer* renderer, int winW, int winH) {
    if (!renderTarget) {
        return false;
    }

    constexpr int kDownscale = 8;   // quanto menor o alvo, mais forte o borrão
    const int smallW = std::max(1, winW / kDownscale);
    const int smallH = std::max(1, winH / kDownscale);

    if (!pauseBlurTex) {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");   // linear
        pauseBlurTex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                         SDL_TEXTUREACCESS_TARGET, smallW, smallH);
    }
    if (!pauseBlurTex) {
        return false;
    }

    // Filtragem linear nas duas texturas para suavizar as duas etapas de escala.
    SDL_SetTextureScaleMode(renderTarget, SDL_ScaleModeLinear);
    SDL_SetTextureScaleMode(pauseBlurTex, SDL_ScaleModeLinear);

    SDL_Texture* prev = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, pauseBlurTex);
    SDL_RenderCopy(renderer, renderTarget, nullptr, nullptr);
    SDL_SetRenderTarget(renderer, prev);
    return true;
}

// #19 Desenha o cenário borrado APENAS atrás de um box (recorte), não na tela
// inteira. Amplia pauseBlurTex para a tela cheia mas com clip no retângulo do box,
// então o borrão fica alinhado com o que está por trás daquele box. Véu escuro só
// dentro do box para contraste do texto. Fora dos boxes o jogo aparece nítido.
void StageState::DrawBlurBehindRect(SDL_Renderer* renderer, const SDL_Rect& rect, int winW, int winH) {
    if (!pauseBlurTex) {
        // Sem desfoque disponível: painel escuro simples só no box.
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 10, 10, 16, 205);
        SDL_RenderFillRect(renderer, &rect);
        return;
    }
    SDL_RenderSetClipRect(renderer, &rect);
    const SDL_Rect full{0, 0, winW, winH};
    SDL_SetTextureAlphaMod(pauseBlurTex, 255);
    SDL_RenderCopy(renderer, pauseBlurTex, nullptr, &full);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 8, 8, 14, 140);
    SDL_RenderFillRect(renderer, &rect);
    SDL_RenderSetClipRect(renderer, nullptr);
}

void StageState::RenderPauseMenu(SDL_Renderer* renderer) {
    if (!renderer || !pauseMenuOpen) {
        return;
    }

    const int winW = Game::GetInstance().GetWindowsWidth();
    const int winH = Game::GetInstance().GetWindowsHeight();

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Escurece a cena para dar foco ao menu (sem borrão de tela cheia).
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 125);
    const SDL_Rect fullDim{0, 0, winW, winH};
    SDL_RenderFillRect(renderer, &fullDim);

    // Ícone de cada opção — MESMA ordem de kPauseMenuLabels
    // {Continuar, Salvar, Configuracoes, Reiniciar nivel, Sair}.
    static const char* kPauseMenuIcons[] = {
        "Recursos/img/menu/pause/icon_continuar.png",
        "Recursos/img/menu/pause/icon_salvar.png",
        "Recursos/img/menu/pause/icon_config.png",
        "Recursos/img/menu/pause/icon_reiniciar.png",
        "Recursos/img/menu/pause/icon_voltar.png",
    };

    const int n = kPauseMenuItemCount;
    const int gap = 10;

    // Caixa de seleção (proporção do PNG) — dimensiona pela ALTURA p/ caber tudo.
    auto boxTex = Resources::GetImage("Recursos/img/menu/pause/caixa_selecao.png");
    float boxAspect = 1281.0f / 394.0f;
    if (boxTex) { int tw, th; if (SDL_QueryTexture(boxTex.get(), nullptr, nullptr, &tw, &th) == 0 && th > 0) boxAspect = static_cast<float>(tw) / th; }

    int boxH = std::min(112, (winH - 210 - (n - 1) * gap) / n);
    boxH = std::max(72, boxH);
    int boxW = static_cast<int>(boxH * boxAspect);
    if (boxW > winW * 0.55f) { boxW = static_cast<int>(winW * 0.55f); boxH = static_cast<int>(boxW / boxAspect); }
    const int optionsH = n * boxH + (n - 1) * gap;

    // Divisória proporcional (largura ~ da caixa).
    auto divTex = Resources::GetImage("Recursos/img/menu/pause/linha_divisoria.png");
    float divAspect = 1034.0f / 230.0f;
    if (divTex) { int tw, th; if (SDL_QueryTexture(divTex.get(), nullptr, nullptr, &tw, &th) == 0 && th > 0) divAspect = static_cast<float>(tw) / th; }
    const int divW = static_cast<int>(boxW * 0.92f);
    const int divH = static_cast<int>(divW / divAspect);

    // Título "PAUSA" (texto).
    auto titleFont = Resources::GetFont("Recursos/font/times.ttf", 48);
    SDL_Texture* titleTex = nullptr;
    int titleW = 0, titleH = 48;
    if (titleFont) {
        SDL_Color tc{228, 208, 148, 255};
        if (SDL_Surface* s = TTF_RenderUTF8_Blended(titleFont.get(), "PAUSA", tc)) {
            titleTex = SDL_CreateTextureFromSurface(renderer, s);
            titleW = s->w; titleH = s->h; SDL_FreeSurface(s);
        }
    }

    // Bloco vertical centralizado: título · divisória · lista de opções.
    const int gTitleDiv = 8, gDivOpts = 14;
    const int totalH = titleH + gTitleDiv + divH + gDivOpts + optionsH;
    int y = std::max(8, (winH - totalH) / 2);

    if (titleTex) {
        SDL_Rect d{(winW - titleW) / 2, y, titleW, titleH};
        SDL_RenderCopy(renderer, titleTex, nullptr, &d);
        SDL_DestroyTexture(titleTex);
    }
    y += titleH + gTitleDiv;

    // Divisória DECORATIVA entre o título e as opções.
    if (divTex) {
        SDL_Rect d{(winW - divW) / 2, y, divW, divH};
        SDL_RenderCopy(renderer, divTex.get(), nullptr, &d);
    }
    y += divH + gDivOpts;

    // Opções: cada uma é a caixa de seleção com ícone à esquerda + texto dentro.
    const int boxX = (winW - boxW) / 2;
    auto font = Resources::GetFont("Recursos/font/times.ttf", 26);
    for (int i = 0; i < n; ++i) {
        const SDL_Rect r{boxX, y + i * (boxH + gap), boxW, boxH};
        pauseMenuItemRects[i] = r;
        const bool sel = (i == pauseMenuSelection);

        // Caixa: selecionado = um pouco maior + brilho total; normal = apagado.
        SDL_Rect dr = r;
        if (sel) { const int gx = static_cast<int>(boxW * 0.03f), gy = static_cast<int>(boxH * 0.03f); dr = {r.x - gx, r.y - gy, r.w + 2 * gx, r.h + 2 * gy}; }
        if (boxTex) {
            SDL_SetTextureColorMod(boxTex.get(), sel ? 255 : 160, sel ? 255 : 160, sel ? 255 : 165);
            SDL_SetTextureAlphaMod(boxTex.get(), sel ? 255 : 225);
            SDL_RenderCopy(renderer, boxTex.get(), nullptr, &dr);
        }

        // Ícone à esquerda, preservando proporção, centrado verticalmente.
        const int iconSlot = static_cast<int>(boxH * 0.30f);   // ícones -50% (era 0.60)
        const int iconPad  = static_cast<int>(boxH * 0.24f);
        const int textLeft = r.x + iconPad + iconSlot + iconPad;
        if (auto icon = Resources::GetImage(kPauseMenuIcons[i])) {
            int tw = 0, th = 0; SDL_QueryTexture(icon.get(), nullptr, nullptr, &tw, &th);
            const float ia = (th > 0) ? static_cast<float>(tw) / th : 1.0f;
            int iw = iconSlot, ih = iconSlot;
            if (ia >= 1.0f) ih = static_cast<int>(iconSlot / ia); else iw = static_cast<int>(iconSlot * ia);
            SDL_Rect id{ r.x + iconPad + (iconSlot - iw) / 2, r.y + (boxH - ih) / 2, iw, ih };
            SDL_SetTextureColorMod(icon.get(), sel ? 255 : 195, sel ? 255 : 195, sel ? 255 : 195);
            SDL_SetTextureAlphaMod(icon.get(), sel ? 255 : 220);
            SDL_RenderCopy(renderer, icon.get(), nullptr, &id);
            SDL_SetTextureColorMod(icon.get(), 255, 255, 255);
            SDL_SetTextureAlphaMod(icon.get(), 255);
        }

        // Texto do item, centralizado no espaço à direita do ícone.
        if (font) {
            SDL_Color c = sel ? SDL_Color{250, 240, 212, 255} : SDL_Color{200, 196, 190, 235};
            if (SDL_Surface* s = TTF_RenderUTF8_Blended(font.get(), kPauseMenuLabels[i], c)) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                const int tw = s->w, th = s->h; SDL_FreeSurface(s);
                if (t) {
                    const int areaL = textLeft, areaR = r.x + boxW - iconPad;
                    const int tx = areaL + ((areaR - areaL) - tw) / 2;
                    SDL_Rect d{ tx, r.y + (boxH - th) / 2, tw, th };
                    SDL_RenderCopy(renderer, t, nullptr, &d);
                    SDL_DestroyTexture(t);
                }
            }
        }
    }

    if (boxTex) { SDL_SetTextureColorMod(boxTex.get(), 255, 255, 255); SDL_SetTextureAlphaMod(boxTex.get(), 255); }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

void StageState::RenderSaveToast(SDL_Renderer* renderer) {
    if (!renderer || saveToastTimer <= 0.0f) {
        return;
    }
    auto font = Resources::GetFont("Recursos/font/times.ttf", 22);
    if (!font) {
        return;
    }
    float alpha = 1.0f;
    if (saveToastTimer < 0.5f) {
        alpha = saveToastTimer / 0.5f;  // fade out nos últimos 0.5s
    }
    const Uint8 a = static_cast<Uint8>(std::min(255.0f, alpha * 255.0f));

    SDL_Color c{225, 240, 215, 255};
    SDL_Surface* s = TTF_RenderUTF8_Blended(font.get(), "Progresso salvo", c);
    if (!s) {
        return;
    }
    SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
    const int tw = s->w;
    const int th = s->h;
    SDL_FreeSurface(s);
    if (!t) {
        return;
    }
    SDL_SetTextureAlphaMod(t, a);

    const int winW = Game::GetInstance().GetWindowsWidth();
    const int padX = 16;
    const int padY = 8;
    const int x = winW - tw - 44;
    const int y = 44;
    const SDL_Rect bg{x - padX, y - padY, tw + padX * 2, th + padY * 2};

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 20, 30, 20, static_cast<Uint8>(alpha * 175.0f));
    SDL_RenderFillRect(renderer, &bg);
    SDL_SetRenderDrawColor(renderer, 120, 160, 110, static_cast<Uint8>(alpha * 205.0f));
    SDL_RenderDrawRect(renderer, &bg);

    const SDL_Rect d{x, y, tw, th};
    SDL_RenderCopy(renderer, t, nullptr, &d);
    SDL_DestroyTexture(t);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

namespace {
const char* kSettingsLabels[] = {"Volume Master", "Volume Fundo", "Volume Efeitos",
                                 "Volume Dublagem", "Brilho", "Tela cheia",
                                 "Reduzir flashes", "Controles", "Voltar"};
void SettingsRowRange(int row, int& lo, int& hi) {
    lo = (row == 4) ? 50 : 0;     // brilho 50..150; volumes 0..100
    hi = (row == 4) ? 150 : 100;
}
int SettingsRowValue(int row) {
    switch (row) {
    case 0: return Game::masterVolumePercent;
    case 1: return Game::ambientVolumePercent;
    case 2: return Game::sfxVolumePercent;
    case 3: return Game::voiceVolumePercent;
    case 4: return Game::brightnessPercent;
    default: return 0;
    }
}
}  // namespace

void StageState::HandleSettingsPanelInput() {
    InputManager& input = InputManager::GetInstance();

    auto applyValue = [this](int row, int v) {
        switch (row) {
        case 0: Game::SetMasterVolume(v); break;
        case 1: Game::SetAmbientVolume(v); oceanAmbient_.RefreshVolume(); break;
        case 2: Game::SetSfxVolume(v); break;
        case 3: Game::SetVoiceVolume(v); break;
        case 4: Game::SetBrightness(v); break;
        default: break;
        }
    };
    auto closePanel = [this]() {
        settingsPanelOpen = false;
        settingsDragging = false;
        Game::SaveSettings();
    };
    auto openControls = [this]() {
        controlsPanelOpen = true;
        controlsSelection = 0;
        awaitingRebind = false;
    };

    if (input.KeyPress(SDLK_ESCAPE)) {   // ESC volta ao menu de pausa (salva)
        closePanel();
        return;
    }

    if (input.KeyPress(SDLK_UP) || input.KeyPress(SDLK_w)) {
        settingsSelection = (settingsSelection + kSettingsRowCount - 1) % kSettingsRowCount;
    }
    if (input.KeyPress(SDLK_DOWN) || input.KeyPress(SDLK_s)) {
        settingsSelection = (settingsSelection + 1) % kSettingsRowCount;
    }

    // Ajuste por teclado nas linhas de slider; setas alternam a tela cheia.
    if (settingsSelection < kSettingsSliderCount) {
        int delta = 0;
        if (input.KeyPress(SDLK_LEFT) || input.KeyPress(SDLK_a)) delta = -5;
        if (input.KeyPress(SDLK_RIGHT) || input.KeyPress(SDLK_d)) delta = 5;
        if (delta != 0) {
            int lo, hi;
            SettingsRowRange(settingsSelection, lo, hi);
            int v = SettingsRowValue(settingsSelection) + delta;
            if (v < lo) v = lo;
            if (v > hi) v = hi;
            applyValue(settingsSelection, v);
        }
    } else if (settingsSelection == 5) {
        if (input.KeyPress(SDLK_LEFT) || input.KeyPress(SDLK_RIGHT) ||
            input.KeyPress(SDLK_a) || input.KeyPress(SDLK_d)) {
            Game::SetFullscreen(!Game::fullscreen);
        }
    } else if (settingsSelection == 6) {
        if (input.KeyPress(SDLK_LEFT) || input.KeyPress(SDLK_RIGHT) ||
            input.KeyPress(SDLK_a) || input.KeyPress(SDLK_d)) {
            Game::reduceFlashing = !Game::reduceFlashing;
        }
    }

    // Mouse: hover seleciona; clique/arrasto nos sliders; clique no toggle/voltar.
    SDL_Point mp{input.GetMouseX(), input.GetMouseY()};
    for (int i = 0; i < kSettingsRowCount; ++i) {
        if (SDL_PointInRect(&mp, &settingsRowRects[i])) {
            settingsSelection = i;
        }
    }
    if (input.MousePress(SDL_BUTTON_LEFT)) {
        bool onSlider = false;
        for (int i = 0; i < kSettingsSliderCount; ++i) {
            if (SDL_PointInRect(&mp, &settingsSliderRects[i])) {
                settingsSelection = i;
                settingsDragging = true;
                onSlider = true;
                break;
            }
        }
        if (!onSlider) {
            if (SDL_PointInRect(&mp, &settingsRowRects[5])) {
                Game::SetFullscreen(!Game::fullscreen);
            } else if (SDL_PointInRect(&mp, &settingsRowRects[6])) {
                Game::reduceFlashing = !Game::reduceFlashing;
            } else if (SDL_PointInRect(&mp, &settingsRowRects[7])) {
                openControls();
                return;
            } else if (SDL_PointInRect(&mp, &settingsRowRects[8])) {
                closePanel();
                return;
            }
        }
    }
    if (settingsDragging) {
        if (input.IsMouseDown(SDL_BUTTON_LEFT) && settingsSelection < kSettingsSliderCount) {
            const SDL_Rect& sr = settingsSliderRects[settingsSelection];
            int lo, hi;
            SettingsRowRange(settingsSelection, lo, hi);
            float frac = (sr.w > 0) ? static_cast<float>(mp.x - sr.x) / static_cast<float>(sr.w) : 0.0f;
            if (frac < 0.0f) frac = 0.0f;
            if (frac > 1.0f) frac = 1.0f;
            applyValue(settingsSelection, lo + static_cast<int>(frac * (hi - lo) + 0.5f));
        } else {
            settingsDragging = false;
        }
    }

    // Enter/Espaco/F: alterna tela cheia / reduzir flashes, abre Controles ou Voltar.
    if (input.KeyPress(SDLK_RETURN) || input.KeyPress(SDLK_SPACE) || input.KeyPress(SDLK_f)) {
        if (settingsSelection == 5) {
            Game::SetFullscreen(!Game::fullscreen);
        } else if (settingsSelection == 6) {
            Game::reduceFlashing = !Game::reduceFlashing;
        } else if (settingsSelection == 7) {
            openControls();
            return;
        } else if (settingsSelection == 8) {
            closePanel();
            return;
        }
    }
}

void StageState::RenderSettingsPanel(SDL_Renderer* renderer) {
    if (!renderer || !settingsPanelOpen) {
        return;
    }
    const int winW = Game::GetInstance().GetWindowsWidth();
    const int winH = Game::GetInstance().GetWindowsHeight();

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 185);
    const SDL_Rect full{0, 0, winW, winH};
    SDL_RenderFillRect(renderer, &full);

    const int panelW = 640;
    const int rowH = 50;
    const int gap = 10;
    const int contentTop = 96;
    const int panelH = contentTop + kSettingsRowCount * (rowH + gap) + 20;
    const int px = (winW - panelW) / 2;
    const int py = (winH - panelH) / 2;

    SDL_SetRenderDrawColor(renderer, 30, 30, 38, 242);
    const SDL_Rect panel{px, py, panelW, panelH};
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 180, 160, 100, 255);
    SDL_RenderDrawRect(renderer, &panel);

    auto labelFont = Resources::GetFont("Recursos/font/times.ttf", 20);
    auto titleFont = Resources::GetFont("Recursos/font/times.ttf", 32);

    auto drawText = [&](const char* str, int tx, int ty, SDL_Color c, TTF_Font* fnt, bool centerX, int centerW) {
        if (!fnt || !str) return;
        SDL_Surface* sf = TTF_RenderUTF8_Blended(fnt, str, c);
        if (!sf) return;
        SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, sf);
        const int w = sf->w;
        const int h = sf->h;
        SDL_FreeSurface(sf);
        if (!t) return;
        const int dx = centerX ? (tx + (centerW - w) / 2) : tx;
        const SDL_Rect d{dx, ty, w, h};
        SDL_RenderCopy(renderer, t, nullptr, &d);
        SDL_DestroyTexture(t);
    };

    drawText("Configuracoes", px, py + 28, SDL_Color{220, 200, 140, 255}, titleFont.get(), true, panelW);

    for (int i = 0; i < kSettingsRowCount; ++i) {
        const int rowY = py + contentTop + i * (rowH + gap);
        const SDL_Rect rowRect{px + 30, rowY, panelW - 60, rowH};
        settingsRowRects[i] = rowRect;
        const bool sel = (i == settingsSelection);

        if (sel) {
            SDL_SetRenderDrawColor(renderer, 60, 55, 40, 220);
            SDL_RenderFillRect(renderer, &rowRect);
        }

        const SDL_Color labelColor = sel ? SDL_Color{245, 235, 205, 255} : SDL_Color{195, 195, 195, 255};
        drawText(kSettingsLabels[i], rowRect.x + 18, rowY + (rowH - 24) / 2, labelColor, labelFont.get(), false, 0);

        if (i < kSettingsSliderCount) {
            const int barX = px + 280;
            const int barW = 230;
            const int barY = rowY + rowH / 2 - 5;
            const int barH = 10;
            const SDL_Rect sliderHit{barX, rowY + 8, barW, rowH - 16};
            settingsSliderRects[i] = sliderHit;

            int lo, hi;
            SettingsRowRange(i, lo, hi);
            const int val = SettingsRowValue(i);
            const float frac = (hi > lo) ? static_cast<float>(val - lo) / static_cast<float>(hi - lo) : 0.0f;

            SDL_SetRenderDrawColor(renderer, 60, 60, 66, 255);
            const SDL_Rect barBg{barX, barY, barW, barH};
            SDL_RenderFillRect(renderer, &barBg);
            SDL_SetRenderDrawColor(renderer, sel ? 220 : 150, sel ? 190 : 140, sel ? 90 : 70, 255);
            const SDL_Rect barFill{barX, barY, static_cast<int>(barW * frac), barH};
            SDL_RenderFillRect(renderer, &barFill);
            const SDL_Rect handle{barX + static_cast<int>(barW * frac) - 4, barY - 5, 8, barH + 10};
            SDL_SetRenderDrawColor(renderer, 230, 220, 180, 255);
            SDL_RenderFillRect(renderer, &handle);

            char valBuf[16];
            std::snprintf(valBuf, sizeof(valBuf), "%d", val);
            drawText(valBuf, barX + barW + 18, rowY + (rowH - 24) / 2, labelColor, labelFont.get(), false, 0);
        } else if (i == 5) {
            drawText(Game::fullscreen ? "Ligado" : "Desligado", px + 280, rowY + (rowH - 24) / 2,
                     labelColor, labelFont.get(), false, 0);
        } else if (i == 6) {
            drawText(Game::reduceFlashing ? "Ligado" : "Desligado", px + 280, rowY + (rowH - 24) / 2,
                     labelColor, labelFont.get(), false, 0);
        }
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

void StageState::HandleControlsPanelInput() {
    InputManager& input = InputManager::GetInstance();

    // Capturando a próxima tecla para a ação selecionada.
    if (awaitingRebind) {
        if (input.KeyPress(SDLK_ESCAPE)) {   // ESC cancela a captura
            awaitingRebind = false;
            return;
        }
        const int key = input.PollAnyKeyPressed();
        if (key != 0) {
            input.SetBinding(rebindAction, key);
            awaitingRebind = false;
        }
        return;
    }

    if (input.KeyPress(SDLK_ESCAPE)) {   // ESC volta para Configurações (salva)
        controlsPanelOpen = false;
        Game::SaveSettings();
        return;
    }

    if (input.KeyPress(SDLK_UP) || input.KeyPress(SDLK_w)) {
        controlsSelection = (controlsSelection + kControlsRowCount - 1) % kControlsRowCount;
    }
    if (input.KeyPress(SDLK_DOWN) || input.KeyPress(SDLK_s)) {
        controlsSelection = (controlsSelection + 1) % kControlsRowCount;
    }

    SDL_Point mp{input.GetMouseX(), input.GetMouseY()};
    for (int i = 0; i < kControlsRowCount; ++i) {
        if (SDL_PointInRect(&mp, &controlsRowRects[i])) {
            controlsSelection = i;
        }
    }

    bool activate = input.KeyPress(SDLK_RETURN) || input.KeyPress(SDLK_SPACE) || input.KeyPress(SDLK_f);
    if (input.MousePress(SDL_BUTTON_LEFT)) {
        for (int i = 0; i < kControlsRowCount; ++i) {
            if (SDL_PointInRect(&mp, &controlsRowRects[i])) {
                controlsSelection = i;
                activate = true;
                break;
            }
        }
    }
    if (!activate) {
        return;
    }

    if (controlsSelection < InputManager::ActionCount) {
        awaitingRebind = true;
        rebindAction = static_cast<GameAction>(controlsSelection);
    } else if (controlsSelection == InputManager::ActionCount) {  // Restaurar padrão
        input.ResetBindingsToDefault();
    } else {  // Voltar
        controlsPanelOpen = false;
        Game::SaveSettings();
    }
}

void StageState::RenderControlsPanel(SDL_Renderer* renderer) {
    if (!renderer || !controlsPanelOpen) {
        return;
    }
    const int winW = Game::GetInstance().GetWindowsWidth();
    const int winH = Game::GetInstance().GetWindowsHeight();

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
    const SDL_Rect full{0, 0, winW, winH};
    SDL_RenderFillRect(renderer, &full);

    const int panelW = 580;
    const int rowH = 38;
    const int gap = 6;
    const int contentTop = 84;
    const int panelH = contentTop + kControlsRowCount * (rowH + gap) + 20;
    const int px = (winW - panelW) / 2;
    const int py = (winH - panelH) / 2;

    SDL_SetRenderDrawColor(renderer, 30, 30, 38, 244);
    const SDL_Rect panel{px, py, panelW, panelH};
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 180, 160, 100, 255);
    SDL_RenderDrawRect(renderer, &panel);

    auto labelFont = Resources::GetFont("Recursos/font/times.ttf", 18);
    auto titleFont = Resources::GetFont("Recursos/font/times.ttf", 30);

    // align: 0 = esquerda em tx, 1 = centralizado em [tx, tx+refW], 2 = direita em tx
    auto drawText = [&](const char* str, int tx, int ty, SDL_Color c, TTF_Font* fnt, int align, int refW) {
        if (!fnt || !str) return;
        SDL_Surface* sf = TTF_RenderUTF8_Blended(fnt, str, c);
        if (!sf) return;
        SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, sf);
        const int w = sf->w;
        const int h = sf->h;
        SDL_FreeSurface(sf);
        if (!t) return;
        int dx = tx;
        if (align == 1) dx = tx + (refW - w) / 2;
        else if (align == 2) dx = tx - w;
        const SDL_Rect d{dx, ty, w, h};
        SDL_RenderCopy(renderer, t, nullptr, &d);
        SDL_DestroyTexture(t);
    };

    drawText("Controles", px, py + 24, SDL_Color{220, 200, 140, 255}, titleFont.get(), 1, panelW);

    for (int i = 0; i < kControlsRowCount; ++i) {
        const int rowY = py + contentTop + i * (rowH + gap);
        const SDL_Rect rr{px + 24, rowY, panelW - 48, rowH};
        controlsRowRects[i] = rr;
        const bool sel = (i == controlsSelection);
        if (sel) {
            SDL_SetRenderDrawColor(renderer, 60, 55, 40, 220);
            SDL_RenderFillRect(renderer, &rr);
        }
        const SDL_Color lc = sel ? SDL_Color{245, 235, 205, 255} : SDL_Color{195, 195, 195, 255};
        const int textY = rowY + (rowH - 22) / 2;

        if (i < InputManager::ActionCount) {
            const GameAction a = static_cast<GameAction>(i);
            drawText(InputManager::ActionLabel(a), rr.x + 14, textY, lc, labelFont.get(), 0, 0);

            const char* keyStr;
            const bool capturing = (awaitingRebind && rebindAction == a);
            if (capturing) {
                keyStr = "Pressione uma tecla...";
            } else {
                keyStr = SDL_GetKeyName(InputManager::GetInstance().GetBinding(a));
                if (!keyStr || keyStr[0] == '\0') keyStr = "?";
            }
            const SDL_Color kc = capturing ? SDL_Color{230, 200, 90, 255} : lc;
            drawText(keyStr, rr.x + rr.w - 14, textY, kc, labelFont.get(), 2, 0);
        } else if (i == InputManager::ActionCount) {
            drawText("Restaurar padrao", rr.x + 14, textY, lc, labelFont.get(), 0, 0);
        } else {
            drawText("Voltar", rr.x + 14, textY, lc, labelFont.get(), 0, 0);
        }
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

// Contadores POR SESSÃO (persistem entre níveis/instâncias de StageState).
namespace {
int sLighterTutShown = 0;
int sSwapTutShown = 0;
int sAbilityTutShown = 0;
int sMoveTutShown = 0;
int sPickupTutShown = 0;
int sRefuelTutShown = 0;
int sCycleTutShown = 0;        // dica de trocar item na roda (1x por sessão)
int sLighterEmptyTutShown = 0; // aviso "luz apagou" (1x por sessão)
bool sFarVoiceArmed = true;   // fala de medo do irmãozinho (sem limite de 3x)
}

void StageState::UpdateTutorials(float dt) {
    if (lighterTutTimer > 0.0f) lighterTutTimer -= dt;
    if (swapTutTimer > 0.0f) swapTutTimer -= dt;
    if (abilityTutTimer > 0.0f) abilityTutTimer -= dt;
    if (moveTutTimer > 0.0f) moveTutTimer -= dt;
    if (pickupTutTimer > 0.0f) pickupTutTimer -= dt;
    if (refuelTutTimer > 0.0f) refuelTutTimer -= dt;
    if (cycleTutTimer > 0.0f) cycleTutTimer -= dt;
    if (lighterEmptyTutTimer > 0.0f) lighterEmptyTutTimer -= dt;

    InputManager& imTut = InputManager::GetInstance();

    // Tutorial de MOVIMENTO (WASD): só no começo. Some para sempre assim que o
    // jogador anda pela primeira vez; se ficar parado por uns segundos sem nunca
    // ter andado, mostra a dica uma vez.
    if (!moveTutDone) {
        const bool movingInput = imTut.ActionDown(GameAction::MoveUp)   ||
                                 imTut.ActionDown(GameAction::MoveDown) ||
                                 imTut.ActionDown(GameAction::MoveLeft) ||
                                 imTut.ActionDown(GameAction::MoveRight);
        if (movingInput) {
            moveTutDone = true;
            moveTutTimer = 0.0f;
        } else if (IsPartyReady() && !IsPlayerInputFrozen()) {
            noMoveAccum += dt;
            if (noMoveAccum > 3.0f && sMoveTutShown < 1 && moveTutTimer <= 0.0f) {
                moveTutTimer = kTutorialDisplayDuration;
                sMoveTutShown++;
            }
        }
    }

    // Tutorial de PEGAR ITEM (E): controlando o irmãozão e parado perto de um
    // item por um tempinho sem pegá-lo.
    {
        const bool nearPickup = controlledCharacter == bigCharacter &&
                                GetReachablePickup() != nullptr && !IsPlayerInputFrozen();
        if (nearPickup) {
            pickupNearAccum += dt;
            if (pickupNearAccum > 1.2f && pickupTutArmed &&
                sPickupTutShown < kMaxTutorialShows && pickupTutTimer <= 0.0f) {
                pickupTutTimer = kTutorialDisplayDuration;
                sPickupTutShown++;
                pickupTutArmed = false;
            }
        } else {
            pickupNearAccum = 0.0f;
            pickupTutArmed = true;   // re-arma ao se afastar
        }
    }

    // Tutorial de TROCAR ITEM na roda (1/3): dispara ao pegar o primeiro item
    // novo, quando a roda passa a ter mais de um item para alternar. Só uma vez
    // por sessão. prevStackCount começa em -1 para ignorar o item inicial.
    {
        const int stackCount = inventory.GetStackCount();
        if (prevStackCount < 0) {
            prevStackCount = stackCount;            // baseline: ignora o isqueiro inicial
        } else if (stackCount > prevStackCount) {
            if (sCycleTutShown < 1 && stackCount >= 2 &&
                controlledCharacter == bigCharacter && cycleTutTimer <= 0.0f) {
                cycleTutTimer = kTutorialDisplayDuration;
                sCycleTutShown++;
            }
            prevStackCount = stackCount;
        } else if (stackCount < prevStackCount) {
            prevStackCount = stackCount;            // usou/combinou algo; acompanha
        }
    }

    // Aviso "luz apagou": quando a fonte de luz chega a zero pela 1ª vez, avisa
    // que é preciso combustível para reabastecer. Só uma vez por sessão.
    {
        if (sLighterEmptyTutShown < 1 && controlledCharacter == bigCharacter &&
            inventory.HasDepletedLightSource() && lighterEmptyTutTimer <= 0.0f) {
            lighterEmptyTutTimer = kTutorialDisplayDuration;
            sLighterEmptyTutShown++;
        }
    }

    // Tutorial de REABASTECER (F): só quando o jogador está SEGURANDO o
    // combustível (item selecionado na roda). Ensina a despejá-lo no isqueiro.
    {
        const bool holdingOil = controlledCharacter == bigCharacter &&
                                inventory.IsActiveItemFuel();
        if (holdingOil && refuelTutArmed &&
            sRefuelTutShown < kMaxTutorialShows && refuelTutTimer <= 0.0f) {
            refuelTutTimer = kTutorialDisplayDuration;
            sRefuelTutShown++;
            refuelTutArmed = false;
        }
        if (!holdingOil) {
            refuelTutArmed = true;
        }
    }

    // Tutorial do isqueiro ("aperte F para acender"): só aparece quando o
    // jogador está MORRENDO no escuro (drenando sanidade por falta de luz) E
    // está com o ISQUEIRO na mão (item selecionado) mas apagado. SÓ para o
    // irmãozão — é ele quem usa o isqueiro/itens.
    {
        // Mesma darknessThreshold usada na sanidade (Character.cpp): abaixo de
        // 10% de luz o personagem está no escuro drenando sanidade.
        const float controlledIllum = (controlledCharacter == bigCharacter)
                                           ? bigIlluminationLevel
                                           : smallIlluminationLevel;
        const bool dyingInShadow = controlledIllum < 0.1f;

        // Não dispara durante o showcase inicial: como a luz é acesa
        // automaticamente logo no começo, ensinar "aperte F para ligar" nesse
        // momento confunde. O tutorial fica adiado até o showcase terminar (e,
        // naturalmente, só reaparece quando a luz apaga de novo).
        // Só ensina "aperte F para acender" se o isqueiro AINDA TEM COMBUSTÍVEL.
        // Se a durabilidade acabou, apertar F não liga a luz — então não mostra.
        const Inventory::ItemStack* activeStack = inventory.GetActiveStack();
        const bool lighterHasFuel = activeStack && !activeStack->durabilities.empty() &&
                                    activeStack->durabilities.front() > 0;

        const bool cond = !autoLightShowcasePending &&
                          controlledCharacter == bigCharacter &&
                          inventory.IsActiveItemLighter() &&
                          lighterHasFuel &&
                          !inventory.IsUsableLightActive() &&
                          dyingInShadow;
        if (cond && lighterTutArmed && sLighterTutShown < kMaxTutorialShows && lighterTutTimer <= 0.0f) {
            lighterTutTimer = kTutorialDisplayDuration;
            sLighterTutShown++;
            lighterTutArmed = false;
        }
        if (!cond) {
            lighterTutArmed = true;   // re-arma quando a condição passa
        }
    }

    // Tutorial de troca: irmãos longe demais (distância linear).
    if (IsPartyReady() && bigCharacterObject && smallCharacterObject) {
        const float dist = bigCharacterObject->box.Center().Distance(smallCharacterObject->box.Center());
        const bool cond = dist > kSwapTutFarDist;
        if (cond && swapTutArmed && sSwapTutShown < kMaxTutorialShows && swapTutTimer <= 0.0f) {
            swapTutTimer = kTutorialDisplayDuration;
            sSwapTutShown++;
            swapTutArmed = false;
        }
        if (dist < kSwapTutNearDist) {
            swapTutArmed = true;
        }

        // Ao se afastarem demais, alterna entre o MEDO do irmãozinho e a
        // REPREENSÃO do irmãozão ("para de ser medroso") — assim as duas falas
        // de bronca também entram em jogo. Sem o limite de 3x do tutorial — o
        // cooldown global de voz já evita repetição.
        static bool sScoldTurn = false;
        if (cond && sFarVoiceArmed) {
            if (sScoldTurn) {
                GameVoice::OnScoldFear();
            } else {
                GameVoice::OnBrothersTooFar();
            }
            sScoldTurn = !sScoldTurn;
            sFarVoiceArmed = false;
        }
        if (dist < kSwapTutNearDist) {
            sFarVoiceArmed = true;
        }
    }

    // Tutorial da habilidade do irmãozinho: aparece ao assumir o controle dele
    // (ex.: ao começar o 2º andar controlando o irmãozinho).
    if (IsPartyReady() && smallCharacter && controlledCharacter == smallCharacter) {
        if (abilityTutArmed && sAbilityTutShown < kMaxTutorialShows && abilityTutTimer <= 0.0f) {
            abilityTutTimer = kTutorialDisplayDuration;
            sAbilityTutShown++;
            abilityTutArmed = false;
        }
    } else {
        abilityTutArmed = true;   // re-arma ao voltar a controlar o irmãozão
    }
}

void StageState::RenderTutorials(SDL_Renderer* renderer) {
    if (!renderer || IsPlayerInputFrozen()) {
        return;
    }

    auto drawBanner = [&](float timer, const std::string& text, int y) {
        if (timer <= 0.0f) return;
        const float elapsed = kTutorialDisplayDuration - timer;
        float a01 = 1.0f;
        if (timer < 0.8f) a01 = timer / 0.8f;             // fade out
        else if (elapsed < 0.4f) a01 = elapsed / 0.4f;    // fade in
        a01 = a01 * a01 * (3.0f - 2.0f * a01);            // smoothstep (eased)

        auto font = Resources::GetFont("Recursos/font/times.ttf", 24);
        if (!font) return;
        SDL_Color col{245, 232, 200, 255};
        SDL_Surface* sf = TTF_RenderUTF8_Blended(font.get(), text.c_str(), col);
        if (!sf) return;
        SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, sf);
        const int tw = sf->w;
        const int th = sf->h;
        SDL_FreeSurface(sf);
        if (!t) return;
        SDL_SetTextureAlphaMod(t, static_cast<Uint8>(255.0f * a01));

        const int winW = Game::GetInstance().GetWindowsWidth();
        const int padX = 22;
        const int padY = 12;
        const int x = (winW - tw) / 2;
        const SDL_Rect bg{x - padX, y - padY, tw + padX * 2, th + padY * 2};

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 18, 18, 24, static_cast<Uint8>(200.0f * a01));
        SDL_RenderFillRect(renderer, &bg);
        SDL_SetRenderDrawColor(renderer, 200, 180, 110, static_cast<Uint8>(220.0f * a01));
        SDL_RenderDrawRect(renderer, &bg);
        const SDL_Rect d{x, y, tw, th};
        SDL_RenderCopy(renderer, t, nullptr, &d);
        SDL_DestroyTexture(t);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    };

    InputManager& im = InputManager::GetInstance();

    // Cada tooltip só aparece para o irmão a que se aplica.
    const bool controllingBig = controlledCharacter == bigCharacter;
    const bool controllingSmall = controlledCharacter == smallCharacter;

    // Resolve a tecla vinculada a uma ação (com rótulo de reserva).
    auto keyName = [](int code, const char* fallback) {
        const char* k = SDL_GetKeyName(code);
        return (k && k[0] != '\0') ? std::string(k) : std::string(fallback);
    };

    // Empilha os banners visíveis de cima para baixo, sem sobreposição.
    int y = 70;

    if (moveTutTimer > 0.0f) {
        const std::string up = keyName(im.GetBinding(GameAction::MoveUp), "W");
        const std::string lf = keyName(im.GetBinding(GameAction::MoveLeft), "A");
        const std::string dn = keyName(im.GetBinding(GameAction::MoveDown), "S");
        const std::string rt = keyName(im.GetBinding(GameAction::MoveRight), "D");
        drawBanner(moveTutTimer, "Use [" + up + "] [" + lf + "] [" + dn + "] [" + rt + "] para se mover", y);
        y += 56;
    }
    if (lighterTutTimer > 0.0f && controllingBig) {
        drawBanner(lighterTutTimer,
                   "Pressione [" + keyName(im.GetBinding(GameAction::UseItem), "F") + "] para ligar seu isqueiro", y);
        y += 56;
    }
    if (pickupTutTimer > 0.0f && controllingBig) {
        drawBanner(pickupTutTimer,
                   "Pressione [" + keyName(im.GetBinding(GameAction::Interact), "E") + "] para pegar o item", y);
        y += 56;
    }
    if (cycleTutTimer > 0.0f && controllingBig) {
        const std::string prev = keyName(im.GetBinding(GameAction::CyclePrev), "Left");
        const std::string next = keyName(im.GetBinding(GameAction::CycleNext), "Right");
        drawBanner(cycleTutTimer,
                   "Use [" + prev + "] e [" + next + "] para trocar de item na mochila", y);
        y += 56;
    }
    if (lighterEmptyTutTimer > 0.0f && controllingBig) {
        drawBanner(lighterEmptyTutTimer,
                   "Sua luz apagou! Use combustivel para reabastece-la", y);
        y += 56;
    }
    if (refuelTutTimer > 0.0f && controllingBig) {
        drawBanner(refuelTutTimer,
                   "Combustivel: aperte [" + keyName(im.GetBinding(GameAction::UseItem), "F") +
                       "] nele e depois no isqueiro para reabastecer", y);
        y += 56;
    }
    if (swapTutTimer > 0.0f) {
        drawBanner(swapTutTimer,
                   "Pressione [" + keyName(im.GetBinding(GameAction::SwapBrother), "Ctrl") + "] para trocar de personagem", y);
        y += 56;
    }
    if (abilityTutTimer > 0.0f && controllingSmall) {
        drawBanner(abilityTutTimer,
                   "Pressione [" + keyName(im.GetBinding(GameAction::Interact), "E") + "] para usar a habilidade do irmaozinho", y);
        y += 56;
    }
}
