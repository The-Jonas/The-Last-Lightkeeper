// ─────────────────────────────────────────────────────────────────────────────
//  StageState — save/load, confirmação de saída, prompt de interação, menu de
//  pausa e aviso de "progresso salvo". (Tutoriais: src/tutorial/.)
// ─────────────────────────────────────────────────────────────────────────────
#include "states/stage/StageState.h"
#include "states/stage/FirstLoadData.h"
#include "states/LoadingState.h"
#include "core/Game.h"
#include "core/InputManager.h"
#include "core/Resources.h"
#include "core/SaveManager.h"
#include "core/Telemetry.h"
#include "engine/GameObject.h"
#include "gameplay/Box.h"
#include "gameplay/Candlestick.h"
#include "gameplay/Character.h"
#include "gameplay/Item.h"
#include "gameplay/ItemPickup.h"
#include "gameplay/Jornal.h"
#include "gameplay/RadioAsset.h"
#include "gameplay/Repairable.h"
#include "gameplay/Window.h"

#define INCLUDE_SDL_TTF
#include "SDL_include.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace {

const char* kUiFont = "Recursos/font/times.ttf";

// ── Menu de pausa ────────────────────────────────────────────────────────────
enum PauseItem { kPauseContinue, kPauseSave, kPauseSettings, kPauseRestart, kPauseQuit };
const char* kPauseMenuLabels[] = {"Continuar", "Salvar", "Configurações", "Reiniciar Nivel", "Sair"};
const char* kPauseMenuIcons[]  = {
    "Recursos/img/menu/pause/icon_continuar.png",
    "Recursos/img/menu/pause/icon_salvar.png",
    "Recursos/img/menu/pause/icon_config.png",
    "Recursos/img/menu/pause/icon_reiniciar.png",
    "Recursos/img/menu/pause/icon_voltar.png",
};

// Posição e tamanho do modal de sair e dos seus dois botões (input e render usam o mesmo).
void QuitConfirmLayout(SDL_Rect& panel, SDL_Rect& saveBtn, SDL_Rect& cancelBtn) {
    const int winW = Game::GetInstance().GetWindowsWidth();
    const int winH = Game::GetInstance().GetWindowsHeight();
    constexpr int kPanelW = 760, kPanelH = 260, kBtnW = 260, kBtnH = 48;
    panel = {(winW - kPanelW) / 2, (winH - kPanelH) / 2, kPanelW, kPanelH};
    const int btnY = panel.y + kPanelH - 80;
    saveBtn   = {panel.x + 60, btnY, kBtnW, kBtnH};
    cancelBtn = {panel.x + kPanelW - kBtnW - 60, btnY, kBtnW, kBtnH};
}

// Desenha um texto em (x, y); wrapW > 0 quebra linhas nessa largura.
void DrawUiText(SDL_Renderer* r, TTF_Font* font, const char* text, int x, int y,
                SDL_Color color, int wrapW = 0) {
    if (!font || !text) return;
    SDL_Surface* s = (wrapW > 0) ? TTF_RenderUTF8_Blended_Wrapped(font, text, color, static_cast<Uint32>(wrapW))
                                 : TTF_RenderUTF8_Blended(font, text, color);
    if (!s) return;
    if (SDL_Texture* t = SDL_CreateTextureFromSurface(r, s)) {
        const SDL_Rect dst{x, y, s->w, s->h};
        SDL_RenderCopy(r, t, nullptr, &dst);
        SDL_DestroyTexture(t);
    }
    SDL_FreeSurface(s);
}

// Nome da tecla ligada à ação, ou `fallback` se o SDL não tiver nome para ela.
std::string KeyName(GameAction action, const char* fallback) {
    const char* k = SDL_GetKeyName(InputManager::GetInstance().GetBinding(action));
    return (k && k[0] != '\0') ? std::string(k) : std::string(fallback);
}

// Converte linhas de diálogo do jogo para o formato do save (enums viram int).
std::vector<SavedDialogueLine> ToSavedLines(const std::vector<DialogueBox::Line>& lines) {
    std::vector<SavedDialogueLine> out;
    out.reserve(lines.size());
    for (const DialogueBox::Line& l : lines) {
        out.push_back({static_cast<int>(l.speaker), static_cast<int>(l.listener),
                       static_cast<int>(l.emotion), static_cast<int>(l.listenerEmotion), l.text});
    }
    return out;
}

// Caminho inverso de ToSavedLines.
std::vector<DialogueBox::Line> FromSavedLines(const std::vector<SavedDialogueLine>& lines) {
    std::vector<DialogueBox::Line> out;
    out.reserve(lines.size());
    for (const SavedDialogueLine& l : lines) {
        out.push_back({static_cast<DialogueBox::Speaker>(l.speaker), static_cast<DialogueBox::Speaker>(l.listener),
                       static_cast<DialogueBox::Emotion>(l.emotion), static_cast<DialogueBox::Emotion>(l.listenerEmotion),
                       l.text});
    }
    return out;
}

// Posição, sanidade e estado de escada de um irmão, para o save.
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

// Devolve ao irmão o que CaptureCharacter guardou.
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

// Todos os itens que podem aparecer no save (ciclo de pickups + lanterna inicial).
std::vector<ItemDef> BuildItemCatalog(const StageFirstLoadData& cfg) {
    std::vector<ItemDef> catalog = cfg.pickupCycle;
    catalog.push_back(cfg.startingFlashlight);
    return catalog;
}

}  // namespace

// ═════════════════════════════════════════════════════════════════════════════
//  Save / load
// ═════════════════════════════════════════════════════════════════════════════

// Fotografa o andar atual: irmãos, inventário, itens, caixas, velas, consertos,
// documentos, log de diálogos e as dicas já aprendidas.
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

    for (const CollectedDocument& d : collectedDocuments) {
        SavedDocument sd;
        sd.imagePath       = d.imagePath;
        sd.title           = d.title;
        sd.soundPath       = d.soundPath;
        sd.level           = d.level;
        sd.order           = d.order;
        sd.zoomFactor      = d.zoomFactor;
        sd.zoomable        = d.zoomable;
        sd.unread          = d.unread;
        sd.dialogueContext = d.dialogueContext;
        sd.dialogue        = ToSavedLines(d.dialogueLines);
        state.documents.push_back(std::move(sd));
    }

    for (const DialogueLogEntry& e : dialogueLog) {
        SavedDialogueEntry se;
        se.key          = e.key;
        se.context      = e.context;
        se.level        = e.level;
        se.docImagePath = e.docImagePath;
        se.lines        = ToSavedLines(e.lines);
        state.dialogueLog.push_back(std::move(se));
    }

    state.learnedHints = hints.GetLearnedList();
    return state;
}

// Garante que toda vela do andar tem a sua luz registrada.
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

// Acende as velas da lista (por tiledId); com extinguishOthers, apaga as demais.
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

// Restaura um save sobre o andar já carregado: irmãos, inventário, documentos,
// log, itens removidos/soltos, caixas, consertos e velas.
void StageState::ApplySaveState(const SaveGameState& state) {
    const StageFirstLoadData cfg = LoadStageFirstLoadData();
    const std::vector<ItemDef> catalog = BuildItemCatalog(cfg);

    ApplyCharacter(state.big, bigCharacterObject, bigCharacter);
    ApplyCharacter(state.small, smallCharacterObject, smallCharacter);
    inventory.ReadFromSave(state, catalog);

    collectedDocuments.clear();
    for (const SavedDocument& sd : state.documents) {
        CollectedDocument d;
        d.imagePath       = sd.imagePath;
        d.title           = sd.title;
        d.soundPath       = sd.soundPath;
        d.level           = sd.level;
        d.order           = sd.order;
        d.zoomFactor      = sd.zoomFactor;
        d.zoomable        = sd.zoomable;
        d.unread          = sd.unread;
        d.dialogueContext = sd.dialogueContext;
        d.dialogueLines   = FromSavedLines(sd.dialogue);
        collectedDocuments.push_back(std::move(d));
    }

    dialogueLog.clear();
    for (const SavedDialogueEntry& se : state.dialogueLog) {
        DialogueLogEntry e;
        e.key          = se.key;
        e.context      = se.context;
        e.level        = se.level;
        e.docImagePath = se.docImagePath;
        e.lines        = FromSavedLines(se.lines);
        dialogueLog.push_back(std::move(e));
    }
    dialogueLogSelection = std::max(0, static_cast<int>(dialogueLog.size()) - 1);

    hints.SetLearnedList(state.learnedHints);
    knownDocumentKeysLevel = -1;                           // refaz a contagem na próxima abertura
    RemoveCollectedJornalsFromWorld();

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

    for (size_t i = 0; i < objectArray.size(); ++i) {   // índice: Destroy() pode mexer no vetor
        GameObject* go = objectArray[i].get();
        if (!go) {
            continue;
        }

        // Pickups soltos (tiledId < 0) são recriados pelo droppedItems abaixo;
        // os do mapa somem se foram pegos ou pulados.
        if (ItemPickup* pickup = go->GetComponent<ItemPickup>()) {
            if (go->tiledId < 0 || removedIds.count(go->tiledId) > 0 ||
                skippedPickupSpawnIds.count(go->tiledId) > 0) {
                pickup->Destroy();
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

// Grava o progresso atual. Sem save anterior, este vira também o checkpoint do andar.
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

// Grava o início do andar: checkpoint e progresso atual ficam iguais.
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

// ═════════════════════════════════════════════════════════════════════════════
//  Confirmação de saída
// ═════════════════════════════════════════════════════════════════════════════

// Setas/Tab alternam, mouse seleciona; Enter/Y/1/clique confirmam, ESC/N/2 cancelam.
// "Salvar e sair" grava e volta ao título. Retorna true se o modal fechou.
bool StageState::HandleQuitConfirmInput() {
    InputManager& input = InputManager::GetInstance();
    SDL_Rect panel;
    QuitConfirmLayout(panel, quitConfirmSaveBtn, quitConfirmCancelBtn);

    if (input.KeyPress(SDLK_UP) || input.KeyPress(SDLK_DOWN) || input.KeyPress(SDLK_LEFT) ||
        input.KeyPress(SDLK_RIGHT) || input.KeyPress(SDLK_TAB)) {
        quitConfirmSelection = 1 - quitConfirmSelection;
    }

    const SDL_Point mouse{input.GetMouseX(), input.GetMouseY()};
    const bool overSave   = SDL_PointInRect(&mouse, &quitConfirmSaveBtn);
    const bool overCancel = SDL_PointInRect(&mouse, &quitConfirmCancelBtn);
    if (overSave)   quitConfirmSelection = 0;
    if (overCancel) quitConfirmSelection = 1;

    if (input.KeyPress(SDLK_ESCAPE) || input.KeyPress(SDLK_n) || input.KeyPress(SDLK_2)) {
        Telemetry::Event("quit_confirm", Telemetry::Fields()
            .Int("level", currentLevelIndex).Str("choice", "cancelou"));
        quitConfirmOpen = false;
        return true;
    }

    const bool clicked = input.MousePress(SDL_BUTTON_LEFT);
    if (clicked && !overSave && !overCancel) {
        return false;   // clique fora dos botões não faz nada
    }
    if (!clicked && !input.KeyPress(SDLK_RETURN) && !input.KeyPress(SDLK_y) && !input.KeyPress(SDLK_1)) {
        return false;
    }

    Telemetry::Event("quit_confirm", Telemetry::Fields()
        .Int("level", currentLevelIndex)
        .Str("choice", quitConfirmSelection == 0 ? "salvou_e_saiu" : "cancelou")
        .Num("levelTime", telemetryLevelElapsed));
    if (quitConfirmSelection == 0) {
        Telemetry::SetEndReason("quit_stage");
        SaveCurrentProgress();
        popRequested = true;
    }
    quitConfirmOpen = false;
    return true;
}

// Véu escuro, painel com a pergunta e os botões "Salvar e sair" / "Cancelar".
void StageState::RenderQuitConfirmModal(SDL_Renderer* renderer) {
    if (!renderer || !quitConfirmOpen) {
        return;
    }
    const int winW = Game::GetInstance().GetWindowsWidth();
    const int winH = Game::GetInstance().GetWindowsHeight();
    SDL_Rect panel;
    QuitConfirmLayout(panel, quitConfirmSaveBtn, quitConfirmCancelBtn);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    const SDL_Rect backdrop{0, 0, winW, winH};
    SDL_RenderFillRect(renderer, &backdrop);

    SDL_SetRenderDrawColor(renderer, 35, 35, 42, 240);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 180, 160, 100, 255);
    SDL_RenderDrawRect(renderer, &panel);

    auto textFont   = Resources::GetFont(kUiFont, 22);
    auto buttonFont = Resources::GetFont(kUiFont, 18);
    DrawUiText(renderer, textFont.get(), "Tem certeza? Você pode perder o progresso não salvo.",
               panel.x + 40, panel.y + 36, SDL_Color{230, 230, 230, 255}, panel.w - 80);

    auto drawButton = [&](const SDL_Rect& rect, bool selected, const char* label) {
        const Uint8 c = selected ? 200 : 80;
        SDL_SetRenderDrawColor(renderer, c, selected ? 180 : 80, selected ? 100 : 80, 230);
        SDL_RenderFillRect(renderer, &rect);
        SDL_SetRenderDrawColor(renderer, 220, 200, 140, 255);
        SDL_RenderDrawRect(renderer, &rect);
        if (!buttonFont) return;
        int tw = 0, th = 0;
        TTF_SizeUTF8(buttonFont.get(), label, &tw, &th);
        DrawUiText(renderer, buttonFont.get(), label, rect.x + (rect.w - tw) / 2, rect.y + (rect.h - th) / 2,
                   SDL_Color{240, 240, 240, 255});
    };
    drawButton(quitConfirmSaveBtn, quitConfirmSelection == 0, "Salvar e sair");
    drawButton(quitConfirmCancelBtn, quitConfirmSelection == 1, "Cancelar");
}

// ═════════════════════════════════════════════════════════════════════════════
//  Prompt de interação (rodapé)
// ═════════════════════════════════════════════════════════════════════════════

// Caixa no rodapé com até 3 linhas "[tecla] ação" para o que o personagem
// controlado pode fazer agora (segue o mesmo alvo do contorno de interação).
void StageState::RenderInteractionPrompt(SDL_Renderer* renderer) {
    if (!renderer || IsPlayerInputFrozen()) {
        return;
    }

    // Até TRÊS linhas (irmãozinho escondido: sair + habilidade + trocar).
    std::string lines[3];
    int lineCount = 0;

    const bool hidden = controlledCharacter && controlledCharacter->isHidden;
    if (hidden) {
        // Escondidos: AMBOS os irmãos saem com Interact (E) e podem TROCAR de
        // personagem. Só o irmãozinho tem a HABILIDADE de visão, agora em UseItem (F).
        lines[lineCount++] = "[" + KeyName(GameAction::Interact, "E") + "] Sair do esconderijo";
        if (controlledCharacter == smallCharacter) {
            lines[lineCount++] = "[" + KeyName(GameAction::UseItem, "F") + "] Usar habilidade do irmaozinho";
        }
        lines[lineCount++] = "[" + KeyName(GameAction::SwapBrother, "Ctrl") + "] Trocar de personagem";
    } else if (controlledCharacter == bigCharacter) {
        // Irmão maior fora do esconderijo: todas as interações do rodapé.
        const char* action = nullptr;
        if (activePushBox) {
            action = "Soltar";
        } else if (reachableCloset) {
            action = "Esconder";
        } else if (reachableRepairable) {
            action = "Consertar";
        } else if (GameObject* focus = GetInteractionFocus()) {
            // O texto segue o MESMO alvo do contorno e do [E].
            if (reachablePushBox && focus == &reachablePushBox->GetAssociated()) {
                action = "Empurrar";
            } else if (reachableJornal && focus == &reachableJornal->GetAssociated()) {
                action = reachableJornal->IsCollectible() ? "Pegar" : "Ler";
            } else if (reachableCandle && focus == &reachableCandle->GetAssociated()) {
                action = reachableCandle->IsLit() ? "Apagar" : "Acender";
            } else if (reachableWindow && focus == &reachableWindow->GetAssociated()) {
                action = "Fechar";
            } else if (reachableRadio && focus == &reachableRadio->GetAssociated()) {
                action = reachableRadio->IsPlaying() ? "Desligar" : "Ligar";
            } else if (reachablePickup && focus == &reachablePickup->GetAssociated() &&
                    !IsPickupBlocked(reachablePickup)) {
                action = "Pegar";
            }
        }
        if (!action) {
            return;
        }
        lines[lineCount++] = "[" + KeyName(GameAction::Interact, "E") + "] " + std::string(action);
    } else {
        // Irmãozinho fora do esconderijo: só pode se ESCONDER num armário.
        if (!reachableCloset) {
            return;
        }
        lines[lineCount++] = "[" + KeyName(GameAction::Interact, "E") + "] Esconder";
    }

    if (lineCount == 0) {
        return;
    }

    auto font = Resources::GetFont(kUiFont, 24);
    if (!font) {
        return;
    }
    const SDL_Color color{240, 235, 220, 255};

    // Renderiza cada linha; empilha verticalmente dentro de uma única caixa.
    SDL_Texture* texs[3] = {nullptr, nullptr, nullptr};
    int tws[3] = {0, 0, 0};
    int ths[3] = {0, 0, 0};
    int maxW = 0;
    int totalH = 0;
    const int lineGap = 6;
    for (int i = 0; i < lineCount; ++i) {
        SDL_Surface* surface = TTF_RenderUTF8_Blended(font.get(), lines[i].c_str(), color);
        if (!surface) {
            continue;
        }
        texs[i] = SDL_CreateTextureFromSurface(renderer, surface);
        tws[i] = surface->w;
        ths[i] = surface->h;
        SDL_FreeSurface(surface);
        if (tws[i] > maxW) {
            maxW = tws[i];
        }
        if (i > 0) {
            totalH += lineGap;
        }
        totalH += ths[i];
    }
    if (maxW == 0) {
        for (int i = 0; i < lineCount; ++i) {
            if (texs[i]) SDL_DestroyTexture(texs[i]);
        }
        return;
    }

    Game& game = Game::GetInstance();
    const int winW = game.GetWindowsWidth();
    const int winH = game.GetWindowsHeight();
    const int padX = 18;
    const int padY = 10;
    const int bgW = maxW + padX * 2;
    const int bgH = totalH + padY * 2;
    const int bgX = (winW - bgW) / 2;
    const int bgY = (winH - 72) - bgH;   // caixa ancorada logo acima do rodapé

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 20, 20, 26, 185);
    const SDL_Rect bg{bgX, bgY, bgW, bgH};
    SDL_RenderFillRect(renderer, &bg);
    SDL_SetRenderDrawColor(renderer, 200, 180, 110, 200);
    SDL_RenderDrawRect(renderer, &bg);

    int curY = bgY + padY;
    for (int i = 0; i < lineCount; ++i) {
        if (!texs[i]) {
            continue;
        }
        const int lx = (winW - tws[i]) / 2;
        const SDL_Rect dst{lx, curY, tws[i], ths[i]};
        SDL_RenderCopy(renderer, texs[i], nullptr, &dst);
        SDL_DestroyTexture(texs[i]);
        curY += ths[i] + lineGap;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Menu de pausa
// ═════════════════════════════════════════════════════════════════════════════

// Volta o progresso ao checkpoint do andar e recarrega a fase por cima deste estado.
void StageState::RestartLevelFromCheckpoint() {
    Telemetry::Event("checkpoint_restart", Telemetry::Fields()
        .Int("level", currentLevelIndex)
        .Num("levelTime", telemetryLevelElapsed));
    // Volta o estado "current" para o início do nível e recarrega via Continue.
    SaveManager::RevertCurrentToCheckpoint();
    popRequested = true;
    Game::GetInstance().Push(new LoadingState(StageState::LoadMode::Continue));
}

// W/S ou setas navegam, mouse seleciona; Enter/Espaço/F/clique ativam; ESC fecha.
// Só é chamado com a pausa aberta e as Configurações fechadas.
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

    Telemetry::Event("pause_menu", Telemetry::Fields()
        .Int("level", currentLevelIndex)
        .Str("choice", kPauseMenuLabels[pauseMenuSelection]));

    switch (pauseMenuSelection) {
    case kPauseContinue:
        pauseMenuOpen = false;
        break;
    case kPauseSave:
        SaveCurrentProgress();
        ShowSaveToast();
        pauseMenuOpen = false;
        break;
    case kPauseSettings:
        settingsMenu.Open();
        break;
    case kPauseRestart:
        pauseMenuOpen = false;
        RestartLevelFromCheckpoint();
        break;
    case kPauseQuit:
        pauseMenuOpen = false;
        quitConfirmOpen = true;
        quitConfirmSelection = 0;
        break;
    default:
        break;
    }
}

// Prepara o desfoque da cena na GPU: reduz o renderTarget para pauseBlurTex
// (1/25); ampliado com filtragem linear, vira um borrão barato. Não desenha na
// tela. False se não houver cena para borrar.
bool StageState::BuildPauseBlurTexture(SDL_Renderer* renderer, int winW, int winH) {
    if (!renderTarget) {
        return false;
    }

    constexpr int kDownscale = 25;                                                  // quanto menor o alvo, mais forte o borrão
    const int smallW = std::max(1, winW / kDownscale);
    const int smallH = std::max(1, winH / kDownscale);

    if (!pauseBlurTex) {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");                            // linear
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
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    static const SDL_BlendMode kColorOnly = SDL_ComposeCustomBlendMode(
        SDL_BLENDFACTOR_ONE,  SDL_BLENDFACTOR_ZERO, SDL_BLENDOPERATION_ADD,         // cor: substitui
        SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE,  SDL_BLENDOPERATION_ADD);        // alpha: mantém
    SDL_BlendMode sceneBlend = SDL_BLENDMODE_NONE;
    SDL_GetTextureBlendMode(renderTarget, &sceneBlend);
    SDL_SetTextureBlendMode(renderTarget, kColorOnly);
    SDL_RenderCopy(renderer, renderTarget, nullptr, nullptr);
    SDL_SetTextureBlendMode(renderTarget, sceneBlend);                              // devolve o modo que o Render usa

    SDL_SetRenderTarget(renderer, prev);    
    return true;
}

// Cena borrada só dentro de `rect` (alinhada com o que está atrás dele) + véu
// escuro para o texto. Sem desfoque pronto, só um painel escuro.
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

// Cobre a tela inteira com a cena borrada, com opacidade `alpha` (0..1).
void StageState::DrawSceneBlur(SDL_Renderer* renderer, int winW, int winH, float alpha) {
    if (!BuildPauseBlurTexture(renderer, winW, winH)) return;
    alpha = std::max(0.0f, std::min(1.0f, alpha));
    SDL_SetTextureBlendMode(pauseBlurTex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(pauseBlurTex, static_cast<Uint8>(255.0f * alpha));
    const SDL_Rect full{0, 0, winW, winH};
    SDL_RenderCopy(renderer, pauseBlurTex, nullptr, &full);
    SDL_SetTextureAlphaMod(pauseBlurTex, 255);
}

// Cena borrada e escurecida, título "PAUSA", divisória e as cinco caixas com
// ícone + texto (a selecionada um pouco maior e acesa). Guarda os retângulos
// para o mouse.
void StageState::RenderPauseMenu(SDL_Renderer* renderer) {
    if (!renderer || !pauseMenuOpen) {
        return;
    }

    const int winW = Game::GetInstance().GetWindowsWidth();
    const int winH = Game::GetInstance().GetWindowsHeight();

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Cena borrada + véu escuro: foco no menu sem dar para estudar o jogo pausado.
    DrawSceneBlur(renderer, winW, winH, 1.0f);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 125);
    const SDL_Rect fullDim{0, 0, winW, winH};
    SDL_RenderFillRect(renderer, &fullDim);

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
    auto titleFont = Resources::GetFont(kUiFont, 48);
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
    auto font = Resources::GetFont(kUiFont, 26);
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

// ═════════════════════════════════════════════════════════════════════════════
//  Aviso "Progresso salvo"
// ═════════════════════════════════════════════════════════════════════════════

// Caixa verde no canto superior direito, com fade nos últimos 0,5 s.
void StageState::RenderSaveToast(SDL_Renderer* renderer) {
    if (!renderer || saveToastTimer <= 0.0f) {
        return;
    }
    auto font = Resources::GetFont(kUiFont, 22);
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
