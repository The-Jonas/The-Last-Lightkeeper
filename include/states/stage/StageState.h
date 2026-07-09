#ifndef STAGESTATE_H
#define STAGESTATE_H

#define INCLUDE_SDL
#define INCLUDE_SDL_MIXER
#include "SDL_include.h"

#include "core/State.h"
#include "core/InputManager.h"
#include "audio/Music.h"
#include "world/TileSet.h"
#include "lighting/LightMaskTypes.h"
#include "lighting/RadialLightOverlay.h"
#include "lighting/LightTweakPanel.h"
#include "lighting/TopDownLightShadows.h"
#include "gameplay/Inventory.h"
#include "gameplay/Character.h"
#include "core/LevelManager.h"
#include "core/SaveData.h"
#include "gameplay/Character.h"
#include "states/stage/FirstLoadData.h"
#include "states/stage/OceanAmbientController.h"
#include "math/Vec2.h"
#include "gameplay/RadioAsset.h"

#include <memory>
#include <unordered_set>
#include <vector>

class GameObject;
class TileMap;
class Box;
class ItemPickup;
class Jornal;
class Candlestick;
class Window;
class Closet;
class Repairable;

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
    // Legenda da fala (dublagem) que estiver tocando — barra inferior central.
    void RenderVoiceSubtitle(SDL_Renderer* renderer);
    void RenderJournalViewer(SDL_Renderer* renderer);
    // Prompt contextual "[E] ..." quando há um objeto interagível ao alcance
    // (apenas controlando o irmão maior). Substitui a antiga legenda fixa (3.5).
    void RenderInteractionPrompt(SDL_Renderer* renderer);

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
        const float fuelRatio = inventory.GetSelectedLightFuelRatio();
        outFalloffRadiusPx = lightMaskParams.falloffRadiusPx * fuelRatio;
        return true;
    }

    // True quando existe ≥1 janela no andar e TODAS estão abertas — o monstro
    // dominou o andar. Consultado pelo Monster (fim da campanha estratégica).
    bool AreAllWindowsOpen() const;

    // Empurra o personagem para fora de qualquer colisão em que tenha ficado
    // preso (ex.: ao sair do armário). Testa AS DUAS colisões do movimento:
    // level.CheckCollision (geometria estática) E IsBoxWalkableOnMapLayer (tiles).
    void UnstickCharacter(Character* c);

    Window* GetReachableWindow() const { return reachableWindow; }
    Window* FindClosestReachableWindow() const;
    bool IsWindowClosestForInteraction(Window* window) const;
    void TryInteractWindowOnKeyPress();

    // Armário alcançável neste frame (preenchido por Closet::Update); alimenta
    // o prompt central do rodapé. Resetado a cada frame antes do UpdateArray.
    Closet* GetReachableCloset() const { return reachableCloset; }
    void SetReachableCloset(Closet* c) { reachableCloset = c; }

    // Público pra ser reutilizado pelo monstro
    // footRadius > 0 usa um footprint CIRCULAR centrado no tile (agentes grandes,
    // ex.: monstro); <= 0 usa o footprint padrão "nos pés" da box do agente (1.4).
    std::vector<Vec2> FindPathWorld(const Vec2& fromWorld, const Vec2& toWorld, const GameObject* agent = nullptr,
                                    int nodeBudget = 4096, float footRadius = -1.0f) const;
    bool IsWorldPosNavigableFor(const Vec2& worldPos, const GameObject* agent, float footRadius = -1.0f) const;
    // Linha livre (navegável) entre dois pontos. Público: usado pelo Monster (LOS de visão).
    bool HasWalkableLine(const Vec2& fromWorld, const Vec2& toWorld) const;
    bool HasWalkableLine(const Vec2& fromWorld, const Vec2& toWorld, const GameObject* agent, float footRadius = -1.0f) const;
    const std::vector<std::shared_ptr<GameObject>>& GetObjectArray() const { return objectArray; }

    // Getter para saber quem está atualmente sendo controlado
    Character* GetControlledCharacter() const { return controlledCharacter; }

    // True quando algum overlay deve impedir o input de gameplay (menu/modal/jornal).
    // Público: consultado por HotbarComponent e Character.
    bool IsPlayerInputFrozen() const { return pauseMenuOpen || quitConfirmOpen || journalViewerOpen; }

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

    // Flash vermelho de dano (toque do monstro). Decai ao longo do tempo.
    float damageFlashTimer = 0.0f;
    static constexpr float kDamageFlashDuration = 0.35f;
    // Janela após um toque do monstro: se a morte ocorre com esse timer ativo,
    // a derrota é atribuída ao monstro (6.5); senão, à escuridão.
    float lastMonsterHitTimer = 0.0f;
    static constexpr float kMonsterHitDeathWindow = 2.0f;

    // Showcase inicial: em jogo novo o jogador começa com a luz APAGADA e, alguns
    // segundos após o 1º nível carregar, ela é acesa automaticamente — mostrando
    // ao jogador o contraste e a mecânica da luz.
    bool  autoLightShowcasePending = false;
    float autoLightShowcaseTimer = 0.0f;
    static constexpr float kAutoLightShowcaseDelay = 2.0f;

    // Indicador "quem estou controlando": aparece no início e a cada troca,
    // quica + pisca branco por alguns segundos e some (eased-out).
    float controlIndicatorTimer = 0.0f;
    static constexpr float kControlIndicatorDuration = 2.5f;
    void TriggerControlIndicator() { controlIndicatorTimer = kControlIndicatorDuration; }

    // Tutoriais (máx. 3x por sessão cada — contadores estáticos no .cpp).
    float lighterTutTimer = 0.0f;
    bool lighterTutArmed = true;
    float swapTutTimer = 0.0f;
    bool swapTutArmed = true;
    float abilityTutTimer = 0.0f;   // habilidade do irmãozinho (E)
    bool abilityTutArmed = true;
    // Tutorial de movimento (WASD): aparece no começo se o jogador ficar parado
    // sem nunca ter andado.
    float moveTutTimer = 0.0f;
    bool moveTutDone = false;        // vira true ao primeiro movimento; não repete
    float noMoveAccum = 0.0f;
    // Tutorial de pegar item (E): aparece ao ficar perto de um item sem pegar.
    float pickupTutTimer = 0.0f;
    bool pickupTutArmed = true;
    float pickupNearAccum = 0.0f;
    // Tutorial de reabastecer (F): só aparece quando o jogador está segurando o
    // combustível (item selecionado na roda).
    float refuelTutTimer = 0.0f;
    bool refuelTutArmed = true;
    // Tutorial de trocar item na roda (1/3): dispara ao pegar o 1º item novo,
    // quando a roda passa a ter mais de um item para alternar.
    float cycleTutTimer = 0.0f;
    int prevStackCount = -1;
    // Aviso "luz apagou": quando a fonte de luz chega a zero pela 1ª vez.
    float lighterEmptyTutTimer = 0.0f;
    static constexpr float kTutorialDisplayDuration = 4.5f;
    static constexpr int   kMaxTutorialShows = 3;
    static constexpr float kSwapTutFarDist = 660.0f;     // dispara o tutorial de troca
    static constexpr float kSwapTutNearDist = 340.0f;    // re-arma quando se aproximam
    void UpdateTutorials(float dt);
    void RenderTutorials(SDL_Renderer* renderer);

    // Dispara o feedback de dano do monstro: SFX + tremor de tela + flash vermelho.
    // Chamado pelo Monster ao tocar um irmão.
    void TriggerMonsterHitFeedback();

    // Sequência final: ao alcançar a escada no último andar, uma sucessão rápida
    // de clarões (a luz do farol se aproximando) satura a tela até o branco total,
    // e então os créditos/pós-créditos entram (EndState).
    bool  finalEscapeActive = false;
    float finalEscapeTimer = 0.0f;
    int   finalEscapeStrikeCount = 0;   // clarões de trovão já disparados
    static constexpr float kFinalEscapeDuration = 3.4f;
    void UpdateFinalEscape(float dt);
    void RenderFinalEscape(SDL_Renderer* renderer);

    // Transição de cena estilo RE4: congela o quadro atual e faz um zoom central
    // borrado escurecendo, antes de carregar o próximo andar (ou o EndState no
    // último). Usada em TODAS as escadas.
    bool  sceneTransitionActive = false;
    bool  sceneTransitionToEnd  = false;   // último andar → EndState (vitória)
    int   sceneTransitionTargetLevel = 0;
    float sceneTransitionTimer  = 0.0f;
    SDL_Texture* sceneTransitionFrame = nullptr;   // quadro congelado

    // #19 Backdrop borrado do menu de pausa: alvo pequeno onde a cena é reduzida
    // (downscale) e depois ampliada com filtragem linear = desfoque barato na GPU.
    SDL_Texture* pauseBlurTex = nullptr;
    static constexpr float kSceneTransitionDuration    = 1.5f;   // escadas normais
    static constexpr float kSceneTransitionEndDuration = 2.2f;   // último andar (com clarão + trovão)
    float SceneTransitionDuration() const {
        return sceneTransitionToEnd ? kSceneTransitionEndDuration : kSceneTransitionDuration;
    }
    void BeginSceneTransition(int targetLevelIndex, bool toEnd);
    void UpdateSceneTransition(float dt);
    void CaptureSceneFrame(SDL_Renderer* renderer);
    void RenderSceneTransition(SDL_Renderer* renderer);

    RadioAsset* GetReachableRadio() const { return reachableRadio; }
    void SetReachableRepairable(Repairable* r) { reachableRepairable = r; }

    Character* GetBigCharacterComponent()   const { return bigCharacter; }
    Character* GetSmallCharacterComponent() const { return smallCharacter; }

    // Debug (só em Game::debugMode): jogador invisível para o monstro (observar
    // comportamento) e câmera livre seguindo o mouse.
    bool debugMonsterBlind = false;
    bool debugFreeCam = false;
    bool IsMonsterBlindDebug() const { return debugMonsterBlind; }
    bool IsPhysicsDebugOn() const { return showMapPhysicsDebug; }   // tecla [B]: overlays de colisão (inclui boxes do monstro)

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
    bool IsTileNavigableFor(const GameObject* agent, int tx, int ty, float footRadius = -1.0f) const;
    Vec2 TileCenterToWorld(int tx, int ty) const;
    /// Retângulo jogável em coordenadas de mundo (para itens não nascerem fora do mapa).
    Vec2 ClampPickupTopLeft(Vec2 topLeft, float itemW, float itemH) const;
    bool WorldToTile(const Vec2& worldPos, int& outTx, int& outTy) const;
    bool FindNearestWalkableTile(int startTx, int startTy, int& outTx, int& outTy, int maxRadius = 8,
                                   const GameObject* agent = nullptr, float footRadius = -1.0f) const;

    /// Grade A* disponível: matriz de tiles OU grade sintética (`LoadAssets` sem `TileMap` em cena).
    bool HasNavigationGrid() const;
    int NavTileWidthPx() const;
    int NavTileHeightPx() const;
    bool IsPartyReady() const;                                           // Confere se referências da dupla são válidas
    bool HandleQuitConfirmInput();                                       // Modal ESC: save and quit / cancel

    // Menu de pausa (overlay): o mundo continua simulando, mas o input do
    // jogador é congelado enquanto o menu está aberto (decisão 1.1).
    void HandlePauseMenuInput();
    void RenderPauseMenu(SDL_Renderer* renderer);
    bool BuildPauseBlurTexture(SDL_Renderer* renderer, int winW, int winH);       // #19 gera o desfoque do cenário (GPU)
    void DrawBlurBehindRect(SDL_Renderer* renderer, const SDL_Rect& rect, int winW, int winH);  // desfoque só atrás de um box
    void RenderSaveToast(SDL_Renderer* renderer);
    void HandleSettingsPanelInput();
    void RenderSettingsPanel(SDL_Renderer* renderer);
    void HandleControlsPanelInput();
    void RenderControlsPanel(SDL_Renderer* renderer);
    void RestartLevelFromCheckpoint();
    void ShowSaveToast() { saveToastTimer = kSaveToastDuration; }
    void ClearGameplayWorld();
    void BuildLevelWorld(const StageFirstLoadData& cfg, bool resetInventory);
    void ShowLevelTitleBanner();
    // Trancamento do andar: quando todas as janelas abrem, apaga todas as velas
    // e mantém as reacesas apagando (2s) até uma janela fechar de novo.
    bool windowLockdownActive = false;
    void UpdateWindowLockdown(float dt);
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
    GameObject* hudLine1 = nullptr;                                      // Linha 1 de instruções (debug)
    GameObject* hudLine2 = nullptr;                                      // Linha 2 de instruções (debug)
    GameObject* hudLine3 = nullptr;                                      // Linha 3: atalhos luz / painel (debug)
    GameObject* hudFps = nullptr;                                        // Linha FPS (monitor, debug)
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
    GameObject* inventoryWheelObject = nullptr;
    std::vector<class ItemPickup*> itemPickups;
    std::vector<Jornal*> jornals;

    Jornal* reachableJornal = nullptr;
    Candlestick* reachableCandle = nullptr;
    Window* reachableWindow = nullptr;
    Closet* reachableCloset = nullptr;
    Repairable* reachableRepairable = nullptr;
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

    // Menu de pausa (overlay)
    bool pauseMenuOpen = false;
    int pauseMenuSelection = 0;
    static constexpr int kPauseMenuItemCount = 5;        // Continuar / Salvar / Config / Reiniciar / Sair
    SDL_Rect pauseMenuItemRects[kPauseMenuItemCount]{};
    float saveToastTimer = 0.0f;
    static constexpr float kSaveToastDuration = 2.0f;

    // Painel de configurações (overlay sobre o menu de pausa)
    bool settingsPanelOpen = false;
    int settingsSelection = 0;
    bool settingsDragging = false;                       // arrastando um slider com o mouse
    static constexpr int kSettingsRowCount = 9;          // Master/Ambiente/Efeitos/Dublagem/Brilho/TelaCheia/ReduzirFlashes/Controles/Voltar
    static constexpr int kSettingsSliderCount = 5;       // as 5 primeiras linhas são sliders
    SDL_Rect settingsRowRects[kSettingsRowCount]{};
    SDL_Rect settingsSliderRects[kSettingsSliderCount]{};

    // Tela de Controles (remapeamento de teclas — overlay sobre Configurações)
    bool controlsPanelOpen = false;
    int controlsSelection = 0;
    bool awaitingRebind = false;                         // capturando a próxima tecla
    GameAction rebindAction = GameAction::MoveUp;        // ação sendo remapeada
    static constexpr int kControlsRowCount = InputManager::ActionCount + 2; // ações + Restaurar + Voltar
    SDL_Rect controlsRowRects[kControlsRowCount]{};
    int companionStartDelay = 0;
    int currentLevelIndex = 0;
    float levelTitleTimer = 0.0f;
    int levelTitleNumber = 1;
    bool inventoryInitialized = false;
    static constexpr float kLevelTitleDuration = 3.0f;

    // Tentando arrumar o pathfinding
    float companionPathRefreshTimer = 0.0f;
    static constexpr float kCompanionPathRefreshInterval = 0.35f;
    std::vector<Vec2> cachedCompanionPath;
    int companionPathIndex = 0;                                          // waypoint atual no caminho em cache (1.1)
    bool companionHolding = false;                                       // histerese de chegada: seguidor parado no ponto atrás do líder
    mutable std::vector<GameObject*> dynamicColliderCache;
    mutable bool dynamicColliderCacheDirty = true;
    mutable GameObject* monsterNavObstacle = nullptr;   // monstro como obstáculo de nav p/ os irmãos (1.3)
    void RefreshDynamicColliderCache() const;

    RadioAsset* reachableRadio = nullptr;
    RadioAsset* FindClosestReachableRadio() const;
    void        TryInteractRadioOnKeyPress();
    
    //Som que toca quando se interage com objetos narrativos
    Sound jornalInteractSound;
};

#endif
