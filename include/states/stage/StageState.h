#ifndef STAGESTATE_H
#define STAGESTATE_H

#define INCLUDE_SDL
#define INCLUDE_SDL_MIXER
#include "SDL_include.h"

#include "core/State.h"
#include "audio/Music.h"
#include "world/TileSet.h"
#include "lighting/LightMaskTypes.h"
#include "lighting/RadialLightOverlay.h"
#include "lighting/LightTweakPanel.h"
#include "lighting/TopDownLightShadows.h"
#include "gameplay/Inventory.h"
#include "core/LevelManager.h"
#include "core/SaveData.h"
#include "states/stage/FirstLoadData.h"
#include "states/stage/OceanAmbientController.h"
#include "math/Vec2.h"

#include <memory>
#include <unordered_set>
#include <vector>

class Character;
class GameObject;
class TileMap;
class Box;
class ItemPickup;
class Jornal;
class Candlestick;
class Window;

class StageState : public State {
friend class SpawnFactory;
public:
    enum class LoadMode {
        NewGame,
        Continue
    };

    struct LightInstance {
        Vec2 worldPos;
        LightMaskShape shape = LightMaskShape::Circle;
        LightMaskParams params;
        bool enabled = true;
        float animationSeed = 0.0f;
    };

    StageState(LoadMode mode = LoadMode::NewGame);                      // Construtor
    ~StageState();                                                      // Destrutor

    LevelManager level;
    void LoadAssets() override;                                         // Carrega assets do estado
    void Update(float dt) override ;                                    // Atualiza lógica de estado
    void Render() override;                                             // Desenha na tela

    void Start() override;                                              // Método para a fase de inicialização.
    void Pause() override;
    void Resume() override;

    GameObject* GetBigCharacter() { return bigCharacterObject; }
    GameObject* GetSmallCharacter() { return smallCharacterObject; }

    // Funções para controle de luzes dinâmicas de cenário
    int CreateStaticLight(Vec2 pos, bool startsLit);
    int CreateStaticLight(Vec2 pos, bool startsLit, LightMaskShape shape, const LightMaskParams& params); // Overload de CreateStaticLight com shape e params customizados

    void SetLightEnabled(int lightId, bool enabled);
    void UpdateInventoryLight();

    // Verifica se a luz está no range dos pés dos personagens
    float bigLightContact = 0.0f;
    float smallLightContact = 0.0f;
    
    // Verifica a intensidade da luz nos personagens
    float bigIlluminationLevel = 0.0f;
    float smallIlluminationLevel = 0.0f;

    Inventory& GetInventory() { return inventory; }
    const LightMaskParams& GetLightMaskParams() const { return lightMaskParams; }
    LoadMode GetLoadMode() const { return loadMode; }

    SaveGameState CaptureSaveState() const;
    void ApplySaveState(const SaveGameState& state);
    bool SaveCurrentProgress();
    bool SaveLevelCheckpoint();

    void SetInitialLevelIndex(int index);
    void BeginLevelTransition(int targetLevelIndex);
    void TransitionToLevel(int targetLevelIndex);
    int GetCurrentLevelIndex() const { return currentLevelIndex; }

    void RenderQuitConfirmModal(SDL_Renderer* renderer);
    void RenderLevelTitleBanner(SDL_Renderer* renderer);
    void RenderJournalViewer(SDL_Renderer* renderer);

    Box* GetReachablePushBox() const { return reachablePushBox; }
    Box* GetActivePushBox() const { return activePushBox; }
    Jornal* GetReachableJornal() const { return reachableJornal; }
    Candlestick* GetReachableCandle() const { return reachableCandle; }
    bool IsPushBoxCloserThanItem(ItemPickup* item, Box* box) const;
    bool IsJornalCloserThanItemAndBox(Jornal* jornal, ItemPickup* item, Box* box) const;
    bool IsCandleClosestForInteraction(Candlestick* candle) const;
    ItemPickup* GetReachablePickup() const { return reachablePickup; }
    void NotifyItemPickupCollected(ItemPickup* pickup);
    bool ShouldSkipPickupSpawn(int tiledId) const;
    bool IsPickupBlocked(ItemPickup* pickup) const;

    const std::vector<LightInstance>& GetLights() const { return lights; }
 
    // Posição em MUNDO da tocha/isqueiro do personagem, se estiver ativa.  
    // (smoothedTorchLightScreenPos é em coordenadas de TELA — não serve direto)
    bool GetActiveTorchWorldPos(Vec2& outPos, float& outFalloffRadiusPx) const {
        const bool playerWantsLightHidden = Character::player && Character::player->hidePersonalLight;
        const bool torchActive = inventory.IsActiveLightLighter() && !playerWantsLightHidden && bigCharacterObject;
        if (!torchActive) return false;
 
        outPos = bigCharacterObject->box.Center();
        outFalloffRadiusPx = lightMaskParams.falloffRadiusPx; // ou lighterLightParams se preferir o raio específico
        return true;
    }

    Window* GetReachableWindow() const { return reachableWindow; }
    Window* FindClosestReachableWindow() const;
    bool IsWindowClosestForInteraction(Window* window) const;
    void TryInteractWindowOnKeyPress();

    // Público pra ser reutilizado pelo monstro
    std::vector<Vec2> FindPathWorld(const Vec2& fromWorld, const Vec2& toWorld, const GameObject* agent = nullptr, int nodeBudget = 4096) const;
    const std::vector<std::shared_ptr<GameObject>>& GetObjectArray() const { return objectArray; }

    // Getter para saber quem está atualmente sendo controlado
    Character* GetControlledCharacter() const { return controlledCharacter; }

    // objetos estáticos que vão receber sombra de sprite real
    // Para reverter o teste, basta deixar esse vetor vazio (não chame Register)
    std::vector<GameObject*> testShadowObjects;
    void RegisterTestShadowObject(GameObject* go) { testShadowObjects.push_back(go); }
    float lastFrameDt = 0.016f;

    // Overlay de baixa sanidade (spritesheet de "linhas"/rabiscos na tela)
    GameObject* sanityOverlayObj = nullptr;   // GameObject dedicado para o overlay
    float sanityOverlayFrameTimer = 0.0f;
    int   sanityOverlayFrameIndex = 0;
    float sanityOverlaySmoothedIntensity = 0.0f;
    static constexpr int   kSanityOverlayFrameCount = 56;
    static constexpr float kSanityOverlayFrameSeconds = 0.05f; // ajuste a velocidade aqui
    static constexpr float kChromaticAberrationMaxOffsetPx = 14.0f;

private:

    enum class PartyMode {
        TOGETHER,      // Personagens andam juntos (seguidor ativo)
        INDEPENDENT    // Só o controlado anda; parceiro fica parado
    };

    void HandlePartyInput();                                             // Trata TAB/F para troca e modo
    void IssueMovementFromInput(Character* character, GameObject* object); // Aplica WASD no personagem ativo
    void UpdateCompanionBehavior();                                      // Decide lógica do parceiro no frame
    void IssueFollowCommand(Character* follower, GameObject* followerObject, GameObject* leaderObject, bool allowCatchup); // Comando de seguir com espaçamento
    void EnforceMaxDistance();                                           // Limita distância máxima entre os dois
    void SwapControlledCharacter();                                      // Troca personagem controlado
    void RefreshCameraTargets();                                         // Atualiza alvos da câmera (dupla + principal)
    void UpdateHudInstructions();                                        // Mantém HUD no canto superior esquerdo
    void UpdateControlledCharacterVisuals();                             // Destaca visualmente quem está sob controle
    void CreateLightAtCursor();
    Vec2 ScreenToWorld(const Vec2& screenPos) const;
    Vec2 WorldToScreen(const Vec2& worldPos) const;
    void ApplyMapBoundsAndWalkability(GameObject* characterObject, const Vec2& previousPos);
    bool IsBoxWalkableOnMapLayer(const Rect& box) const;
    bool IsTileWalkable(int tx, int ty) const;
    /// Tile walkability + cenário (`LevelManager`) + colliders dinâmicos; `agent nullptr` não é usado aqui (usar `IsTileWalkable`).
    bool IsTileNavigableFor(const GameObject* agent, int tx, int ty) const;
    bool HasWalkableLine(const Vec2& fromWorld, const Vec2& toWorld) const;
    bool HasWalkableLine(const Vec2& fromWorld, const Vec2& toWorld, const GameObject* agent) const;
    Vec2 TileCenterToWorld(int tx, int ty) const;
    /// Retângulo jogável em coordenadas de mundo (para itens não nascerem fora do mapa).
    Vec2 ClampPickupTopLeft(Vec2 topLeft, float itemW, float itemH) const;
    bool WorldToTile(const Vec2& worldPos, int& outTx, int& outTy) const;
    bool FindNearestWalkableTile(int startTx, int startTy, int& outTx, int& outTy, int maxRadius = 8,
                                   const GameObject* agent = nullptr) const;

    /// Grade A* disponível: matriz de tiles OU grade sintética (`LoadAssets` sem `TileMap` em cena).
    bool HasNavigationGrid() const;
    int NavTileWidthPx() const;
    int NavTileHeightPx() const;
    bool IsPartyReady() const;                                           // Confere se referências da dupla são válidas
    bool HandleQuitConfirmInput();                                       // Modal ESC: save and quit / cancel
    void ClearGameplayWorld();
    void BuildLevelWorld(const StageFirstLoadData& cfg, bool resetInventory);
    void ShowLevelTitleBanner();
    void RenderGameplayCollisionDebug(SDL_Renderer* renderer) const;     // Com showMapPhysicsDebug: colliders + foot circles
    void RenderCompanionFollowPathDebug(SDL_Renderer* renderer) const;   // Com showMapPhysicsDebug: polylinha do seguidor (modo junto)
    void UpdateBoxInteraction();
    void ApplyCoupledPushMovement(const Vec2& prevPlayerPos);
    void RenderInteractionGlowIfNeeded(GameObject& go);
    void RegisterAllCandleLights();
    void ApplyLitCandleIds(const std::vector<int>& litIds, bool extinguishOthers = true);
    void MarkMissedUniquePickupsOnLevelLeave();
    void MergeSkippedPickupIds(const std::vector<int>& removed, const std::vector<int>& missed);
    void TryOpenJournalOnKeyPress();
    void TryInteractCandleOnKeyPress();
    void OpenJournalViewer(Jornal* jornal);
    void UpdateJournalViewer(float dt);
    Box* FindClosestReachablePushBox() const;
    ItemPickup* FindClosestReachableItem() const;
    bool IsPickupStillTracked(ItemPickup* pickup) const;
    Jornal* FindClosestReachableJornal() const;
    Candlestick* FindClosestReachableCandle() const;
    bool IsPlayerNearLitCandle() const;
    float GetInteractableDistance(const GameObject& obj) const;

    Music music;                                                        // Música de Fundo
    TileSet* tileSet;                                                   // TileSet atualmente ativo no mapa
    std::unique_ptr<TileSet> dungeonTileSet;
    Vec2 mapOrigin{0.0f, 0.0f};
    float levelWorldW = 0.0f;
    float levelWorldH = 0.0f;
    GameObject* bigCharacterObject;                                      // GameObject do personagem grande (IRMÃOZÃO)
    GameObject* smallCharacterObject;                                    // GameObject do personagem pequeno (IRMÃOZINHO)
    Character* bigCharacter;                                             // Componente Character do grande (IRMÃOZÃO)
    Character* smallCharacter;                                           // Componente Character do pequeno (IRMÃOZINHO)
    GameObject* controlledCharacterObject;                               // GameObject atualmente controlado
    Character* controlledCharacter;                                      // Character atualmente controlado
    GameObject* companionCharacterObject;                                // GameObject do parceiro (não controlado)
    Character* companionCharacter;                                       // Character do parceiro (não controlado)
    PartyMode partyMode;                                                 // Estado atual da dupla (junto/independente)
    GameObject* hudLine1;                                                // Linha 1 de instruções
    GameObject* hudLine2;                                                // Linha 2 de instruções
    GameObject* hudLine3;                                                // Linha 3: atalhos luz / painel
    GameObject* hudFps;                                                  // Linha FPS (monitor de performance)
    float fpsSmoothed = 60.0f;                                           // FPS suavizado para leitura estável
    float fpsUiRefreshTimer = 0.0f;                                      // Timer de refresh do texto FPS
    RadialLightOverlay* radialGeometry;                                  // Vignette procedural (várias formas)
    LightMaskParams lightMaskParams;
    LightMaskShape lightMaskShape;
    std::unique_ptr<LightTweakPanel> lightTweakPanel;
    std::vector<LightInstance> lights;
    TileMap* tileMapComp = nullptr;
    /// Quando não há `TileMap`, A* usa esta grade (mundo do estágio ≈ spawn em `LoadAssets`).
    int navTilePx = 64;
    int navGridWidthTiles = 0;
    int navGridHeightTiles = 0;
    std::unordered_set<int> walkableTileIds{0, 1, 2, 7, 8, 9, 31, 37, 38};
    std::vector<TopDownShadowEdge> staticShadowEdges;
    bool staticShadowEdgesBuilt = false;
    bool renderStaticTileShadows = false;
    Vec2 smoothedDynamicLightScreenPos{0.0f, 0.0f};
    bool hasSmoothedDynamicLight = false;
    int inventoryLightId = -1;
    Vec2 smoothedTorchLightScreenPos{0.0f, 0.0f};
    bool hasSmoothedTorchLight = false;
    bool previewLightLockedToPlayer = false;
    GameObject* previewLightAnchorPlayer = nullptr;
    /// Luz de preview que segue o rato ou o jogador (ligada com botão direito). X alterna; luzes fixas no mapa e lanterna continuam.
    bool cursorPreviewLightEnabled = false;
    int maxActiveLights = 24;
    bool lightsEnabled = true;
    bool shadowsEnabled = true;
    bool musicMuted = false;
    
    // Tentar criar o shader olho de peixe
    SDL_Texture* renderTarget = nullptr;

    /// B: map collision / collider debug + caminho A* ou linha reta do parceiro que segue até o outro (modo dupla junto).
    bool showMapPhysicsDebug = false;

    /// Última rota planejada para o `companion` alcançar o alvo atrás do líder (só preenchido em `PartyMode::TOGETHER`).
    std::vector<Vec2> companionFollowPathWorld;

    Inventory inventory;
    GameObject* hotbarObject = nullptr;
    std::vector<class ItemPickup*> itemPickups;
    std::vector<Jornal*> jornals;

    Jornal* reachableJornal = nullptr;
    Candlestick* reachableCandle = nullptr;
    Window* reachableWindow = nullptr;
    bool journalViewerOpen = false;
    bool journalViewerClosing = false;
    float journalAnimTimer = 0.0f;
    float journalCloseTimer = 0.0f;
    std::string journalViewImagePath;
    SDL_FRect journalSourceScreenRect{0.0f, 0.0f, 0.0f, 0.0f};
    SDL_FRect journalTargetScreenRect{0.0f, 0.0f, 0.0f, 0.0f};
    static constexpr float kJournalOpenDuration = 0.35f;
    static constexpr float kJournalCloseDuration = 0.2f;

    ItemPickup* reachablePickup = nullptr;
    std::unordered_set<int> skippedPickupSpawnIds;
    std::vector<int> missedUniquePickupIdsAccum;
    Box* reachablePushBox = nullptr;
    Box* activePushBox = nullptr;
    Vec2 pushBoxOffset{0.0f, 0.0f};
    bool wasPushingLastFrame = false;

    std::shared_ptr<Mix_Chunk> oceanWavesChunk;
    StageOceanAmbientController oceanAmbient_;
    /// Canal das ondas (0 = ambiente dedicado, reservado para não colidir com Mix_PlayChannel(-1) dos SFX).
    int oceanMixerChannel = -1;
    float ambientResumeDelay = 0.0f;
    /// Set true at end of LoadAssets(); LoadingState may call LoadAssets before Start() — Start skips a second load.
    bool levelContentLoaded = false;
    LoadMode loadMode = LoadMode::NewGame;
    bool quitConfirmOpen = false;
    int quitConfirmSelection = 0;
    SDL_Rect quitConfirmSaveBtn{0, 0, 0, 0};
    SDL_Rect quitConfirmCancelBtn{0, 0, 0, 0};
    int companionStartDelay = 0;
    int currentLevelIndex = 0;
    float levelTitleTimer = 0.0f;
    int levelTitleNumber = 1;
    bool inventoryInitialized = false;
    static constexpr float kLevelTitleDuration = 3.0f;
};

#endif
