#ifndef STAGESTATE_H
#define STAGESTATE_H

#define INCLUDE_SDL
#define INCLUDE_SDL_MIXER
#include "SDL_include.h"

#include "core/State.h"
#include "core/InputManager.h"
#include "core/LevelManager.h"
#include "core/SaveData.h"
#include "audio/Music.h"
#include "world/TileSet.h"
#include "lighting/LightMaskTypes.h"
#include "lighting/RadialLightOverlay.h"
#include "lighting/ScenePostFx.h"
#include "lighting/LightTweakPanel.h"
#include "lighting/TopDownLightShadows.h"
#include "gameplay/Inventory.h"
#include "gameplay/Character.h"
#include "gameplay/RadioAsset.h"
#include "states/stage/FirstLoadData.h"
#include "states/stage/OceanAmbientController.h"
#include "tutorial/HintSystem.h"
#include "math/Vec2.h"
#include "ui/FuelFlameHud.h"
#include "ui/DialogueBox.h"
#include "ui/SettingsMenu.h"

#include <memory>
#include <unordered_set>
#include <vector>

class GameObject;
class SpriteRenderer;
class TileMap;
class Box;
class ItemPickup;
class Jornal;
class Candlestick;
class Window;
class Closet;
class Repairable;
class Monster;

// ─────────────────────────────────────────────────────────────────────────────
//  StageState — o andar em jogo.
//
//  O header é dividido por ASSUNTO. Cada seção tem a parte pública (usada por
//  componentes como Character, Monster, HotbarComponent) e a privada. Para
//  adicionar algo novo: ache a seção do assunto e ponha lá — público só se
//  outra classe precisar.
//
//   1. Ciclo de vida             8. Campo de visão          15. Tutoriais
//   2. Andares e transição       9. Render (etapas)         16. Menus
//   3. Save                     10. Interação               17. Áudio
//   4. Irmãos                   11. Inventário e itens      18. HUD
//   5. Câmera                   12. Diálogos                19. Debug
//   6. Navegação e colisão      13. Documentos e pasta      20. Telemetria
//   7. Luz e sombras            14. Monstro e sanidade
//
//  Arquivos: Update.cpp (frame), Render.cpp (desenho), SaveState.cpp (save e
//  menus), LevelFlow.cpp (andares), JournalViewer.cpp (documentos),
//  Navigation.cpp, Lighting.cpp, BoxInteraction.cpp, MonsterEcho.cpp…
//  Tutorial: src/tutorial/ (HintSystem, roteiros por andar).
// ─────────────────────────────────────────────────────────────────────────────
class StageState : public State {
    friend class SpawnFactory;

    // ═════════════════════════════════════════════════════════════════════════
    //  1. Ciclo de vida
    // ═════════════════════════════════════════════════════════════════════════
public:
    enum class LoadMode { NewGame, Continue };

    explicit StageState(LoadMode mode = LoadMode::NewGame);
    ~StageState();

    void LoadAssets() override;                          // pode ser chamado pelo LoadingState antes do Start
    void Start() override;
    void Pause() override;
    void Resume() override;
    void Update(float dt) override;
    void Render() override;

    LevelManager level;                                  // mapa do Tiled: colisão, zonas e spawns
    float lastFrameDt = 0.016f;

    const std::vector<std::shared_ptr<GameObject>>& GetObjectArray() const { return objectArray; }
    // True com algum overlay que congela o input de gameplay (pausa, sair, documento, pasta).
    bool IsPlayerInputFrozen() const { return pauseMenuOpen || quitConfirmOpen || journalViewerOpen || documentFolderOpen; }

private:
    LoadMode loadMode = LoadMode::NewGame;
    bool levelContentLoaded = false;                     // LoadAssets já rodou (o Start não recarrega)

    // ═════════════════════════════════════════════════════════════════════════
    //  2. Andares e transição
    // ═════════════════════════════════════════════════════════════════════════
public:
    void SetInitialLevelIndex(int index);
    void BeginLevelTransition(int targetLevelIndex);     // escada: transição até o próximo andar (ou o fim)
    void TransitionToLevel(int targetLevelIndex);
    int  GetCurrentLevelIndex() const { return currentLevelIndex; }
    void RenderLevelTitleBanner(SDL_Renderer* renderer);

    // Transição estilo RE4 em todas as escadas: congela o quadro e faz um zoom
    // borrado escurecendo antes de carregar o próximo andar (ou o EndState).
    bool  sceneTransitionActive = false;
    bool  sceneTransitionToEnd  = false;                 // último andar → EndState (vitória)
    int   sceneTransitionTargetLevel = 0;
    float sceneTransitionTimer  = 0.0f;
    SDL_Texture* sceneTransitionFrame = nullptr;         // quadro congelado
    static constexpr float kSceneTransitionDuration    = 1.5f;
    static constexpr float kSceneTransitionEndDuration = 2.2f;
    float SceneTransitionDuration() const {
        return sceneTransitionToEnd ? kSceneTransitionEndDuration : kSceneTransitionDuration;
    }
    void BeginSceneTransition(int targetLevelIndex, bool toEnd);
    void UpdateSceneTransition(float dt);
    void CaptureSceneFrame(SDL_Renderer* renderer);
    void RenderSceneTransition(SDL_Renderer* renderer);

private:
    int   currentLevelIndex = 0;
    float levelTitleTimer = 0.0f;
    int   levelTitleNumber = 1;
    static constexpr float kLevelTitleDuration = 4.2f;
    static constexpr float kLevelTitleFadeIn   = 0.9f;
    static constexpr float kLevelTitleFadeOut  = 1.3f;
    TileSet* tileSet;                                    // tileset ativo no mapa
    std::unique_ptr<TileSet> dungeonTileSet;
    Vec2  mapOrigin{0.0f, 0.0f};
    float levelWorldW = 0.0f;
    float levelWorldH = 0.0f;

    void ClearGameplayWorld();
    void BuildLevelWorld(const StageFirstLoadData& cfg, bool resetInventory);
    void ShowLevelTitleBanner();
    void RestartLevelFromCheckpoint();                   // "Reiniciar nível" da pausa

    // ═════════════════════════════════════════════════════════════════════════
    //  3. Save
    // ═════════════════════════════════════════════════════════════════════════
public:
    SaveGameState CaptureSaveState() const;
    void ApplySaveState(const SaveGameState& state);
    bool SaveCurrentProgress();
    bool SaveLevelCheckpoint();

private:
    std::unordered_set<int> skippedPickupSpawnIds;       // pickups do mapa que não renascem
    std::vector<int> missedUniquePickupIdsAccum;
    void MarkMissedUniquePickupsOnLevelLeave();
    void MergeSkippedPickupIds(const std::vector<int>& removed, const std::vector<int>& missed);

    // ═════════════════════════════════════════════════════════════════════════
    //  4. Irmãos
    // ═════════════════════════════════════════════════════════════════════════
public:
    GameObject* GetBigCharacter() { return bigCharacterObject; }
    GameObject* GetSmallCharacter() { return smallCharacterObject; }
    Character*  GetBigCharacterComponent() const { return bigCharacter; }
    Character*  GetSmallCharacterComponent() const { return smallCharacter; }
    Character*  GetControlledCharacter() const { return controlledCharacter; }
    // Tira o personagem de dentro de colisão (ex.: saindo do armário), testando
    // a geometria do mapa e os tiles.
    void UnstickCharacter(Character* c);
    void SetPartyTogether(bool together);                // false = irmãozinho fica parado (roteiro do 1º andar)

    // Seta "quem estou controlando": aparece no início e a cada troca.
    float controlIndicatorTimer = 0.0f;
    static constexpr float kControlIndicatorDuration = 2.5f;
    void TriggerControlIndicator() { controlIndicatorTimer = kControlIndicatorDuration; }

private:
    enum class PartyMode {
        TOGETHER,                                        // o parceiro segue
        INDEPENDENT                                      // o parceiro fica parado
    };

    GameObject* bigCharacterObject;                      // irmãozão
    GameObject* smallCharacterObject;                    // irmãozinho
    Character*  bigCharacter;
    Character*  smallCharacter;
    GameObject* controlledCharacterObject;
    Character*  controlledCharacter;
    GameObject* companionCharacterObject;                // o que não está sendo controlado
    Character*  companionCharacter;
    PartyMode   partyMode;
    int companionStartDelay = 0;                         // frames antes do parceiro começar a seguir

    float companionPathRefreshTimer = 0.0f;
    static constexpr float kCompanionPathRefreshInterval = 0.35f;
    std::vector<Vec2> cachedCompanionPath;
    int  companionPathIndex = 0;                         // waypoint atual no caminho em cache
    bool companionHolding = false;                       // seguidor parado no ponto atrás do líder (histerese)
    std::vector<Vec2> companionFollowPathWorld;          // rota do seguidor neste frame (debug)

    bool IsPartyReady() const;                           // referências da dupla válidas
    void HandlePartyInput();                             // troca de personagem e modo
    void IssueMovementFromInput(Character* character, GameObject* object);   // WASD no controlado
    void UpdateCompanionBehavior();
    void IssueFollowCommand(Character* follower, GameObject* followerObject, GameObject* leaderObject, bool allowCatchup);
    void EnforceMaxDistance();                           // distância máxima entre os dois
    void SwapControlledCharacter();
    void UpdateControlledCharacterVisuals();             // destaque de quem está sob controle

    // Fala ao se afastarem demais: alterna o medo do irmãozinho e a bronca do irmãozão.
    bool farVoiceArmed = true;                           // re-arma quando voltam a ficar perto
    bool scoldTurn = false;
    static constexpr float kFarVoiceDistance  = 660.0f;
    static constexpr float kFarVoiceRearmDist = 340.0f;
    void UpdateFarApartVoice();

    // ═════════════════════════════════════════════════════════════════════════
    //  5. Câmera
    // ═════════════════════════════════════════════════════════════════════════
private:
    void RefreshCameraTargets();                         // alvos da câmera (dupla + principal)
    // Zoom-base e limites do andar. No carregamento (snap) e ao voltar de outro
    // estado, porque menu/loading devolvem a câmera ao neutro.
    void ApplyCameraFraming(bool snap);
    void UpdateCamera(float dt, InputManager& input);    // câmera normal ou livre (debug)
    Vec2 ScreenToWorld(const Vec2& screenPos) const;
    Vec2 WorldToScreen(const Vec2& worldPos) const;

    // ═════════════════════════════════════════════════════════════════════════
    //  6. Navegação e colisão
    // ═════════════════════════════════════════════════════════════════════════
public:
    // footRadius > 0 = pé CIRCULAR centrado no tile (agentes grandes, ex.: monstro);
    // <= 0 = pé padrão na base da caixa.
    std::vector<Vec2> FindPathWorld(const Vec2& fromWorld, const Vec2& toWorld, const GameObject* agent = nullptr,
                                    int nodeBudget = 4096, float footRadius = -1.0f) const;
    bool IsWorldPosNavigableFor(const Vec2& worldPos, const GameObject* agent, float footRadius = -1.0f) const;
    bool HasWalkableLine(const Vec2& fromWorld, const Vec2& toWorld) const;          // linha de visão do monstro
    bool HasWalkableLine(const Vec2& fromWorld, const Vec2& toWorld, const GameObject* agent, float footRadius = -1.0f) const;

private:
    TileMap* tileMapComp = nullptr;
    int navTilePx = 64;                                  // grade sintética quando não há TileMap
    int navGridWidthTiles = 0;
    int navGridHeightTiles = 0;
    std::unordered_set<int> walkableTileIds{0, 1, 2, 7, 8, 9, 31, 37, 38};
    mutable std::vector<GameObject*> dynamicColliderCache;
    mutable bool dynamicColliderCacheDirty = true;
    mutable GameObject* monsterNavObstacle = nullptr;    // monstro como obstáculo para os irmãos

    void ApplyMapBoundsAndWalkability(GameObject* characterObject, const Vec2& previousPos);
    bool IsBoxWalkableOnMapLayer(const Rect& box) const;
    bool IsTileWalkable(int tx, int ty) const;
    bool IsTileNavigableFor(const GameObject* agent, int tx, int ty, float footRadius = -1.0f) const;   // tile + cenário + dinâmicos
    Vec2 TileCenterToWorld(int tx, int ty) const;
    bool WorldToTile(const Vec2& worldPos, int& outTx, int& outTy) const;
    bool FindNearestWalkableTile(int startTx, int startTy, int& outTx, int& outTy, int maxRadius = 8,
                                 const GameObject* agent = nullptr, float footRadius = -1.0f) const;
    bool HasNavigationGrid() const;                      // matriz de tiles OU grade sintética
    int  NavTileWidthPx() const;
    int  NavTileHeightPx() const;
    Vec2 ClampPickupTopLeft(Vec2 topLeft, float itemW, float itemH) const;   // item não nasce fora do mapa
    void RefreshDynamicColliderCache() const;
    void CheckObjectCollisions();                        // pares de Collider → NotifyCollision

    // ═════════════════════════════════════════════════════════════════════════
    //  7. Luz e sombras
    // ═════════════════════════════════════════════════════════════════════════
public:
    struct LightInstance {
        Vec2 worldPos;
        LightMaskShape shape = LightMaskShape::Circle;
        LightMaskParams params;
        bool enabled = true;
        float animationSeed = 0.0f;
    };

    int  CreateStaticLight(Vec2 pos, bool startsLit);
    int  CreateStaticLight(Vec2 pos, bool startsLit, LightMaskShape shape, const LightMaskParams& params);
    void SetLightEnabled(int lightId, bool enabled);
    void UpdateInventoryLight();
    const std::vector<LightInstance>& GetLights() const { return lights; }
    const LightMaskParams& GetLightMaskParams() const { return lightMaskParams; }

    // Posição em MUNDO da luz de mão do irmãozão, se estiver acesa (o raio encolhe com a carga).
    bool GetActiveTorchWorldPos(Vec2& outPos, float& outFalloffRadiusPx) const {
        const bool hidden = Character::player && Character::player->hidePersonalLight;
        if (!inventory.IsActiveLightLighter() || hidden || !bigCharacterObject) return false;
        outPos = bigCharacterObject->box.Center();
        outFalloffRadiusPx = lightMaskParams.falloffRadiusPx * inventory.GetSelectedLightFuelRatio();
        return true;
    }

    float bigLightContact = 0.0f;                        // sombra de contato nos pés
    float smallLightContact = 0.0f;
    float bigIlluminationLevel = 0.0f;                   // luz recebida (a sanidade e o monstro leem)
    float smallIlluminationLevel = 0.0f;

    // Objetos de cenário com sombra de sprite (vazio = sem sombras de objeto).
    std::vector<GameObject*> testShadowObjects;
    void RegisterTestShadowObject(GameObject* go) { testShadowObjects.push_back(go); }

private:
    std::vector<LightInstance> lights;
    RadialLightOverlay* radialGeometry;                  // malha de escuridão
    LightMaskParams lightMaskParams;
    LightMaskShape  lightMaskShape;
    std::vector<TopDownShadowEdge> staticShadowEdges;    // sombras das paredes
    bool staticShadowEdgesBuilt = false;
    int  maxActiveLights = 24;
    bool lightsEnabled = true;
    bool shadowsEnabled = true;
    int  inventoryLightId = -1;

    Vec2 smoothedTorchLightScreenPos{0.0f, 0.0f};        // luz de mão suavizada (tela)
    bool hasSmoothedTorchLight = false;
    Vec2 smoothedDynamicLightScreenPos{0.0f, 0.0f};      // luz de preview suavizada (tela)
    bool hasSmoothedDynamicLight = false;
    bool cursorPreviewLightEnabled = false;              // luz de preview (debug) no mouse ou num irmão
    bool previewLightLockedToPlayer = false;
    GameObject* previewLightAnchorPlayer = nullptr;

    std::unique_ptr<LightTweakPanel> lightTweakPanel;    // painel de ajuste (debug, tecla \)
    bool tweakDurabilityOnLoad = true;                   // lido de config/lighting.json antes do painel existir

    void CreateLightAtCursor();
    void RegisterAllCandleLights();
    void ApplyLitCandleIds(const std::vector<int>& litIds, bool extinguishOthers = true);
    void UpdatePreviewLightAnchor(InputManager& input);  // botão direito prende a luz de preview num irmão
    void UpdateLightSmoothing(float dt, InputManager& input);

    // ═════════════════════════════════════════════════════════════════════════
    //  8. Campo de visão
    // ═════════════════════════════════════════════════════════════════════════
    // Cone para onde o controlado olha + círculo nos pés. Dentro, a escuridão
    // some e a cor volta; fora, a cena fica em preto e branco (ScenePostFx).
private:
    PlayerVisionParams visionParams;
    PlayerVisionFrame  visionFrame;
    float visionAxisRad = 0.0f;                          // eixo do cone suavizado
    bool  visionAxisInitialized = false;

    void UpdatePlayerVision(float dt);                   // uma vez por frame, no começo do Render
    // Luzes sintéticas que abrem o buraco da visão na escuridão (sem sombra, sem sanidade).
    void AppendVisionMaskLights(std::vector<RadialLightOverlay::ScreenLight>& out) const;
    // Reduz as luzes REAIS do frame a círculos de tela. Depois de montar as luzes, antes dos objetos.
    void BuildVisionLights(const std::vector<RadialLightOverlay::ScreenLight>& screenLights);
    float VisionVisibilityAtScreen(const Vec2& screenPos) const;                      // geometria do cone: 0..1
    float LightAmountAtScreen(const Vec2& screenPos, bool includeCarriedLight = true) const;   // luz real: 0..1
    float ProximityAtScreen(const Vec2& screenPos) const;                             // perto do controlado: 0..1
    bool  ShouldHideOutsideVision(GameObject& go) const;                              // interagível que some fora da visão
    // Quanto do objeto o jogador vê (0..1): cone, luz e exceção do monstro. Sprite e sombra usam o mesmo.
    float VisibilityOfObject(GameObject& go) const;

    // ═════════════════════════════════════════════════════════════════════════
    //  9. Render (etapas — Render.cpp)
    // ═════════════════════════════════════════════════════════════════════════
private:
    struct FrameLighting {                               // luz do frame, montada e lida pelas etapas
        bool showDebugTools = false;
        bool lighterFromInventory = false;               // luz de mão na mão (e não escondida)
        bool torchLit = false;
        LightMaskParams lighterParams;                   // com a carga
        float bigMaxContact = 0.0f, smallMaxContact = 0.0f;
        float bigMaxTouch = 0.0f,   smallMaxTouch = 0.0f;
    };
    struct DrawnSprite {                                 // objeto desenhado, na ordem do Y-sort
        GameObject* obj;
        SpriteRenderer* sprite;
        bool  stamped;                                   // redesenhado depois da escuridão
        float shown;                                     // visibilidade 0..1
        float light;                                     // luz recebida 0..1
        bool  glow;                                      // contorno de interação
    };

    std::unique_ptr<ScenePostFx> scenePostFx;
    SDL_Texture* renderTarget    = nullptr;              // a cena é desenhada aqui
    SDL_Texture* sceneSnapshot   = nullptr;              // cópia da cena logo depois da escuridão
    SDL_Texture* occluderScratch = nullptr;              // rascunho para recortar um objeto da cópia

    void SortObjectsForDrawing();
    FrameLighting BuildFrameLighting() const;
    void RenderCharacterShadows(SDL_Renderer* renderer, FrameLighting& fl);
    std::vector<RadialLightOverlay::ScreenLight> CollectScreenLights(const FrameLighting& fl) const;
    void RenderObjectShadows(const std::vector<RadialLightOverlay::ScreenLight>& screenLights);
    std::vector<DrawnSprite> RenderWorldObjects(bool stampPassWillRun);
    void RenderDarknessAndWallShadows(SDL_Renderer* renderer, const std::vector<RadialLightOverlay::ScreenLight>& screenLights);
    void UpdateIlluminationLevels(const FrameLighting& fl);
    void RenderStampedSprites(SDL_Renderer* renderer, const std::vector<DrawnSprite>& drawOrder);
    bool PresentScene(SDL_Renderer* renderer, int winW, int winH);    // cena + ScenePostFx na tela
    void RenderBrightnessFallback(SDL_Renderer* renderer, int winW, int winH);
    void RenderScreenFlashes(SDL_Renderer* renderer, int winW, int winH);   // dano e trovão

    // ═════════════════════════════════════════════════════════════════════════
    //  10. Interação ([E] e contornos)
    // ═════════════════════════════════════════════════════════════════════════
    // Os "reachable" são recalculados a cada frame; o foco é o mais próximo.
public:
    GameObject*  GetInteractionFocus() const;
    void         RenderInteractionPrompt(SDL_Renderer* renderer);   // "[E] ação" no rodapé
    Box*         GetReachablePushBox() const { return reachablePushBox; }
    Box*         GetActivePushBox() const { return activePushBox; }
    Jornal*      GetReachableJornal() const { return reachableJornal; }
    Candlestick* GetReachableCandle() const { return reachableCandle; }
    Window*      GetReachableWindow() const { return reachableWindow; }
    RadioAsset*  GetReachableRadio() const { return reachableRadio; }
    Closet*      GetReachableCloset() const { return reachableCloset; }
    void SetReachableCloset(Closet* c) { reachableCloset = c; }                  // Closet::Update
    void SetReachableRepairable(Repairable* r) { reachableRepairable = r; }      // Repairable::Update
    void SetRepairableInReachNoItem(bool v) { repairableInReachNoItem = v; }     // no vão sem a tábua (aviso)
    void SetRepairableNeedsHeldItem(const std::string& item) { repairableHeldItemNeeded = item; }   // tem, mas não na mão

    bool IsPushBoxCloserThanItem(ItemPickup* item, Box* box) const;
    bool IsJornalCloserThanItemAndBox(Jornal* jornal, ItemPickup* item, Box* box) const;
    bool IsCandleClosestForInteraction(Candlestick* candle) const;
    bool IsWindowClosestForInteraction(Window* window) const;
    Window* FindClosestReachableWindow() const;
    void TryInteractWindowOnKeyPress();

private:
    Box*         reachablePushBox = nullptr;
    Box*         activePushBox = nullptr;                // caixa/barril sendo empurrado
    Vec2         pushBoxOffset{0.0f, 0.0f};
    bool         wasPushingLastFrame = false;
    Jornal*      reachableJornal = nullptr;
    Candlestick* reachableCandle = nullptr;
    Window*      reachableWindow = nullptr;
    Closet*      reachableCloset = nullptr;
    RadioAsset*  reachableRadio = nullptr;
    Repairable*  reachableRepairable = nullptr;
    bool         repairableInReachNoItem = false;
    std::string  repairableHeldItemNeeded;               // item que falta pôr na mão para consertar ("" = nada)

    float GetInteractableDistance(const GameObject& obj) const;
    bool  RenderInteractionGlowIfNeeded(GameObject& go);
    void  UpdateBoxInteraction();
    void  DetachActivePushBox();                         // solta a caixa (som, estado, velocidade)
    void  ApplyCoupledPushMovement(const Vec2& prevPlayerPos);
    Box*  FindClosestReachablePushBox() const;
    Candlestick* FindClosestReachableCandle() const;
    bool  IsPlayerNearLitCandle() const;
    void  TryInteractCandleOnKeyPress();
    RadioAsset* FindClosestReachableRadio() const;
    void  TryInteractRadioOnKeyPress();

    // ═════════════════════════════════════════════════════════════════════════
    //  11. Inventário e itens
    // ═════════════════════════════════════════════════════════════════════════
public:
    Inventory& GetInventory() { return inventory; }
    ItemPickup* GetReachablePickup() const { return reachablePickup; }
    void NotifyItemPickupCollected(ItemPickup* pickup);
    bool ShouldSkipPickupSpawn(int tiledId) const;
    bool IsPickupBlocked(ItemPickup* pickup) const;      // bolsa não aceita (contorno vermelho)

private:
    Inventory inventory;
    bool inventoryInitialized = false;
    GameObject* hotbarObject = nullptr;
    GameObject* inventoryWheelObject = nullptr;
    std::vector<ItemPickup*> itemPickups;
    ItemPickup* reachablePickup = nullptr;

    ItemPickup* FindClosestReachableItem() const;
    bool IsPickupStillTracked(ItemPickup* pickup) const;

    // ═════════════════════════════════════════════════════════════════════════
    //  12. Diálogos
    // ═════════════════════════════════════════════════════════════════════════
public:
    struct DialogueLogEntry {
        std::string key;                                 // "andar:origem" — não duplica gatilho repetível
        std::string context;                             // "Ao chegar no farol" (Tiled: dialogue_context)
        int level = 0;
        std::string docImagePath;                        // documento de origem (vazio = não veio de papel)
        std::vector<DialogueBox::Line> lines;
    };
    std::vector<DialogueLogEntry> dialogueLog;           // na ordem em que aconteceram

    void QueueDialogue(DialogueBox::Speaker speaker, DialogueBox::Speaker listener,
                       DialogueBox::Emotion emotion, DialogueBox::Emotion listenerEmotion,
                       const std::string& text) {
        dialogueBox.Queue(speaker, listener, emotion, listenerEmotion, text);
    }
    // Toca uma conversa e registra no log (uma vez por chave).
    void PlayDialogue(const std::string& key, const std::string& context,
                      const std::vector<DialogueBox::Line>& lines, const std::string& docImagePath = "");
    // A conversa com esta chave (sem o prefixo do andar) já está no log deste andar?
    bool IsDialogueLogged(const std::string& key) const {
        const std::string fullKey = std::to_string(currentLevelIndex) + ":" + key;
        for (const DialogueLogEntry& e : dialogueLog) {
            if (e.key == fullKey) return true;
        }
        return false;
    }
    void RenderVoiceSubtitle(SDL_Renderer* renderer);    // legenda da dublagem tocando

    float pendingWindowBreakDialogueTimer = -1.0f;       // janela quebrada: estrondo, depois a fala
    float pendingWindowBreakLineTimer = -1.0f;

private:
    DialogueBox dialogueBox;
    void UpdatePendingWindowBreak(float dt);

    // ═════════════════════════════════════════════════════════════════════════
    //  13. Documentos e pasta (JournalViewer.cpp)
    // ═════════════════════════════════════════════════════════════════════════
public:
    struct CollectedDocument {
        std::string imagePath;
        std::string title;
        std::string soundPath;
        int   order      = 0;
        float zoomFactor = 1.0f;
        bool  zoomable   = false;
        bool  unread     = true;                         // selo "novo" até abrir pela pasta
        std::vector<DialogueBox::Line> dialogueLines;    // tocam na 1ª leitura pela pasta
        int   level      = 0;                            // andar onde foi coletado
        std::string dialogueContext;
    };
    std::vector<CollectedDocument> collectedDocuments;   // sempre ordenada por order
    void CollectJornal(Jornal* jornal);
    void RemoveCollectedJornalsFromWorld();
    void RenderJournalViewer(SDL_Renderer* renderer);
    bool HasUnreadDocuments() const {                    // algum com selo "novo"
        for (const CollectedDocument& d : collectedDocuments) {
            if (d.unread) return true;
        }
        return false;
    }

    // Aba "Diálogos" da pasta.
    int   documentFolderTab = 0;                         // 0 = Documentos, 1 = Diálogos
    int   dialogueLogSelection = 0;
    bool  dialogueLogFocusPanel = false;                 // true = W/S rolam a conversa; false = escolhem na lista
    float dialogueLogScroll = 0.0f;
    float dialogueLogScrollTarget = 0.0f;
    float dialogueLogMaxScroll = 0.0f;                   // medido no Render
    static constexpr float kDialogueLogScrollSpeed = 900.0f;
    bool  dialogueReplayActive = false;                  // conversa na caixa é replay da pasta (ESC corta)
    void UpdateDialogueLogTab(float dt);
    void RenderFolderTabs(SDL_Renderer* renderer, float a);
    void RenderDialogueLogTab(SDL_Renderer* renderer, float a);
    const DialogueLogEntry* FindDialogueForDocument(const std::string& imagePath) const;

    static constexpr float kJournalOpenDuration = 0.35f;
    static constexpr float kJournalCloseDuration = 0.2f;

private:
    std::vector<Jornal*> jornals;
    Sound jornalInteractSound;                           // som ao interagir com objetos narrativos

    // Visualizador de documento.
    bool  journalViewerOpen = false;
    bool  journalViewerClosing = false;
    float journalAnimTimer = 0.0f;
    float journalCloseTimer = 0.0f;
    std::string journalViewImagePath;
    SDL_FRect journalSourceScreenRect{0.0f, 0.0f, 0.0f, 0.0f};
    SDL_FRect journalTargetScreenRect{0.0f, 0.0f, 0.0f, 0.0f};
    bool  journalViewZoomable = false;                   // só documentos "zoomable" no Tiled
    int   journalZoomLevel = 0;                          // 0 normal, 1 perto, 2 bem perto
    float journalZoomCurrent = 1.0f;
    float journalFocusX = 0.5f;                          // ponto do papel no centro (0..1)
    float journalFocusY = 0.5f;
    static constexpr float kJournalZoomLevels[3] = {1.0f, 2.0f, 3.0f};
    static constexpr float kJournalPanSpeedPx    = 900.0f;
    static constexpr float kJournalZoomSmoothing = 12.0f;

    Jornal* FindClosestReachableJornal() const;
    void TryOpenJournalOnKeyPress();
    void OpenJournalViewer(Jornal* jornal);
    void BeginJournalView(const std::string& imagePath, const std::string& soundPath,
                          float zoomFactor, bool zoomable, const SDL_FRect& sourceRect);
    void UpdateJournalViewer(float dt);
    SDL_FRect GetJournalZoomedRect() const;
    void ClampJournalFocus(int winW, int winH);

    // Pasta (Tab).
    struct FolderFrame {
        int level = 0;
        int order = 0;
        int docIndex = -1;                               // -1 = ainda não encontrado ("?")
    };
    bool  documentFolderOpen = false;
    float documentFolderAnim = 0.0f;                     // 0→1, fade de entrada
    int   documentFolderSelection = 0;
    float documentFolderScroll = 0.0f;                   // carrossel suavizado
    int   documentFolderTotal = 0;                       // total do jogo (contador)
    std::vector<FolderFrame> documentFolderFrames;
    std::vector<std::pair<int, int>> knownDocumentKeys;  // (andar, ordem) dos colecionáveis até aqui
    int   knownDocumentKeysLevel = -1;                   // andar da última varredura
    static constexpr float kDocumentFolderFadeTime = 0.18f;
    static constexpr float kDocumentFolderScrollSmoothing = 14.0f;

    void OpenDocumentFolder();
    void CloseDocumentFolder();
    void UpdateDocumentFolder(float dt);
    void RenderDocumentFolder(SDL_Renderer* renderer);
    void ScanKnownDocumentOrders();
    void RebuildDocumentFolderFrames();
    SDL_FRect GetDocumentFolderCardRect(int index) const;
    void OpenCollectedDocument(CollectedDocument& doc, const SDL_FRect& fromRect);

    // ═════════════════════════════════════════════════════════════════════════
    //  14. Monstro e sanidade
    // ═════════════════════════════════════════════════════════════════════════
public:
    void TriggerMonsterHitFeedback();                    // som + tremor + clarão vermelho (Monster)
    void SpawnMonsterEcho(const Vec2& worldFootPos, float strength01);   // onda de uma passada (Monster)
    bool AreAllWindowsOpen() const;                      // monstro dominou o andar

    float damageFlashTimer = 0.0f;                       // clarão vermelho de dano
    static constexpr float kDamageFlashDuration = 0.35f;
    // Morrer com este timer ativo = morte pelo monstro (senão, pela escuridão).
    float lastMonsterHitTimer = 0.0f;
    static constexpr float kMonsterHitDeathWindow = 2.0f;

    // Overlay de sanidade baixa (rabiscos na tela + aberração cromática).
    GameObject* sanityOverlayObj = nullptr;
    int   sanityOverlayFrameIndex = 0;
    float sanityOverlaySmoothedIntensity = 0.0f;
    static constexpr int   kSanityOverlayFrameCount = 56;
    static constexpr float kChromaticAberrationMaxOffsetPx = 14.0f;

private:
    struct MonsterEcho {                                 // uma passada ainda ecoando na tela
        Vec2  worldPos;
        float age = 0.0f;
        float strength = 1.0f;                           // 0..1 pela distância ao jogador
        bool  touched = false;                           // a onda já passou por um irmão (revela uma vez)
    };
    std::vector<MonsterEcho> monsterEchoes;
    mutable std::weak_ptr<GameObject> monsterCache;      // weak_ptr: não fica pendurado se o monstro sumir
    bool windowLockdownActive = false;                   // todas as janelas abertas: velas trancadas

    Monster* FindMonster() const;
    void UpdateMonsterEchoes(float dt);
    void RenderMonsterEchoes(SDL_Renderer* renderer) const;
    void UpdateWindowLockdown(float dt);
    void UpdateSanityFeedback(float dt);                 // overlay, vertigem, batimento
    void RenderSanityAberration(SDL_Renderer* renderer);
    void CheckDefeat();                                  // sanidade zerada / irmão perdido → EndState

    // ═════════════════════════════════════════════════════════════════════════
    //  15. Tutoriais (src/tutorial/) — dicas diegéticas, roteiro por andar
    // ═════════════════════════════════════════════════════════════════════════
public:
    HintSystem& Hints() { return hints; }                // HUD (anéis, teclas) e eventos de gameplay

private:
    HintSystem hints;
    HintContext BuildHintContext() const;                // src/tutorial/HintContext.cpp

    // ═════════════════════════════════════════════════════════════════════════
    //  16. Menus (pausa, configurações, sair)
    // ═════════════════════════════════════════════════════════════════════════
public:
    void RenderQuitConfirmModal(SDL_Renderer* renderer);
    SDL_Texture* pauseBlurTex = nullptr;                 // cena reduzida para o fundo borrado

private:
    bool pauseMenuOpen = false;                          // congela o mundo
    int  pauseMenuSelection = 0;
    static constexpr int kPauseMenuItemCount = 5;        // Continuar / Salvar / Config / Reiniciar / Sair
    SDL_Rect pauseMenuItemRects[kPauseMenuItemCount]{};
    SettingsMenu settingsMenu;
    bool quitConfirmOpen = false;
    int  quitConfirmSelection = 0;
    SDL_Rect quitConfirmSaveBtn{0, 0, 0, 0};
    SDL_Rect quitConfirmCancelBtn{0, 0, 0, 0};
    float saveToastTimer = 0.0f;                         // "Progresso salvo"
    static constexpr float kSaveToastDuration = 2.0f;

    bool HandleEscapeKey();                              // ESC: abre a pausa
    void HandlePauseMenuInput();
    void RenderPauseMenu(SDL_Renderer* renderer);
    bool HandleQuitConfirmInput();
    void ShowSaveToast() { saveToastTimer = kSaveToastDuration; }
    void RenderSaveToast(SDL_Renderer* renderer);
    bool BuildPauseBlurTexture(SDL_Renderer* renderer, int winW, int winH);
    void DrawSceneBlur(SDL_Renderer* renderer, int winW, int winH, float alpha);
    void DrawBlurBehindRect(SDL_Renderer* renderer, const SDL_Rect& rect, int winW, int winH);

    // ═════════════════════════════════════════════════════════════════════════
    //  17. Áudio
    // ═════════════════════════════════════════════════════════════════════════
private:
    Music music;
    bool  musicMuted = false;
    std::shared_ptr<Mix_Chunk> oceanWavesChunk;
    StageOceanAmbientController oceanAmbient_;
    int   oceanMixerChannel = -1;                        // canal dedicado das ondas
    float ambientResumeDelay = 0.0f;

    void UpdateStageMusic(float dt);                     // retoma a trilha se parou sozinha
    void UpdateOceanAmbient(float dt);

    // ═════════════════════════════════════════════════════════════════════════
    //  18. HUD
    // ═════════════════════════════════════════════════════════════════════════
private:
    FuelFlameHud fuelFlameHud;
    GameObject* hudLine1 = nullptr;                      // instruções (debug)
    GameObject* hudLine2 = nullptr;
    GameObject* hudLine3 = nullptr;
    GameObject* hudFps = nullptr;                        // FPS (debug)
    float fpsSmoothed = 60.0f;
    float fpsUiRefreshTimer = 0.0f;

    void TickOverlayTimers(float dt);                    // timers de tela que descem até zero
    void UpdateHudInstructions();
    void UpdateFpsHud(float dt);
    void RenderLittleBrotherPowerHud(SDL_Renderer* renderer, int winW, int winH);
    void RenderControlIndicator(SDL_Renderer* renderer);
    void RenderReloadProgress(SDL_Renderer* renderer);   // arco da recarga da luz sobre o irmãozão
    void RenderRepairOverlay(SDL_Renderer* renderer, int winW, int winH);

    // ═════════════════════════════════════════════════════════════════════════
    //  19. Debug (só com Game::debugMode)
    // ═════════════════════════════════════════════════════════════════════════
public:
    bool debugMonsterBlind = false;                      // [I] invisível para o monstro
    bool debugFreeCam = false;                           // [G] câmera livre no mouse
    bool IsMonsterBlindDebug() const { return debugMonsterBlind; }
    bool IsPhysicsDebugOn() const { return showMapPhysicsDebug; }

private:
    bool showMapPhysicsDebug = false;                    // [B] colisão, colliders, rota do parceiro

    void HandleDebugKeys(InputManager& input);
    void RenderLightDebugCircles(SDL_Renderer* renderer, const FrameLighting& fl);
    void RenderDebugStatus(SDL_Renderer* renderer, int winW);
    void RenderGameplayCollisionDebug(SDL_Renderer* renderer) const;
    void RenderCompanionFollowPathDebug(SDL_Renderer* renderer) const;

    // ═════════════════════════════════════════════════════════════════════════
    //  20. Telemetria de playtest
    // ═════════════════════════════════════════════════════════════════════════
    // Uma amostra por segundo + detecção de "preso", quedas de sanidade e jogador parado.
private:
    float telemetrySampleTimer = 0.0f;
    float telemetryLevelElapsed = 0.0f;                  // segundos no andar
    float telemetryWalkedPx = 0.0f;
    float telemetryLitSeconds = 0.0f;
    Vec2  telemetryLastPos{0.0f, 0.0f};
    bool  telemetryHasLastPos = false;
    float telemetryStuckAccum = 0.0f;                    // empurrando contra parede
    float telemetryStuckCooldown = 0.0f;
    float telemetryJournalOpenedAt = 0.0f;
    float telemetryPrevSanityBig = -1.0f;                // para pegar quedas entre amostras
    float telemetryPrevSanitySmall = -1.0f;
    float telemetryCliffAccum = 0.0f;
    float telemetryCliffCooldown = 0.0f;
    int   telemetrySanityTier = 0;                       // último patamar anunciado (75/50/25/0)
    Vec2  telemetryIdleAnchor{0.0f, 0.0f};               // parado = nem anda nem interage
    float telemetryIdleAccum = 0.0f;

    void UpdateTelemetry(float dt);
    const char* TelemetryMonsterState(float& outDistToPlayer) const;   // "" sem monstro
};

#endif

