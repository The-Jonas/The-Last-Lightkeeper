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
#include "states/stage/OceanAmbientController.h"
#include "math/Vec2.h"

#include <memory>
#include <unordered_set>
#include <vector>

class Character;
class GameObject;
class TileMap;

class StageState : public State {
public:
    StageState();                                                       // Construtor
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

private:

    struct LightInstance {
        Vec2 worldPos;
        LightMaskShape shape = LightMaskShape::Circle;
        LightMaskParams params;
        bool enabled = true;
        float animationSeed = 0.0f;
    };

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
    std::vector<Vec2> FindPathWorld(const Vec2& fromWorld, const Vec2& toWorld, const GameObject* agent = nullptr,
                                    int nodeBudget = 4096) const;
    /// Grade A* disponível: matriz de tiles OU grade sintética (`LoadAssets` sem `TileMap` em cena).
    bool HasNavigationGrid() const;
    int NavTileWidthPx() const;
    int NavTileHeightPx() const;
    bool IsPartyReady() const;                                           // Confere se referências da dupla são válidas
    void RenderGameplayCollisionDebug(SDL_Renderer* renderer) const;     // Com showMapPhysicsDebug: colliders + foot circles
    void RenderCompanionFollowPathDebug(SDL_Renderer* renderer) const;   // Com showMapPhysicsDebug: polylinha do seguidor (modo junto)

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
    Vec2 smoothedTorchLightScreenPos{0.0f, 0.0f};
    bool hasSmoothedTorchLight = false;
    bool previewLightLockedToPlayer = false;
    GameObject* previewLightAnchorPlayer = nullptr;
    /// Luz de preview que segue o rato ou o jogador (ligada com botão direito). X alterna; luzes fixas no mapa e lanterna continuam.
    bool cursorPreviewLightEnabled = true;
    int maxActiveLights = 24;
    bool lightsEnabled = true;
    bool shadowsEnabled = true;
    bool musicMuted = false;

    /// B: map collision / collider debug + caminho A* ou linha reta do parceiro que segue até o outro (modo dupla junto).
    bool showMapPhysicsDebug = false;

    /// Última rota planejada para o `companion` alcançar o alvo atrás do líder (só preenchido em `PartyMode::TOGETHER`).
    std::vector<Vec2> companionFollowPathWorld;

    Inventory inventory;
    GameObject* hotbarObject = nullptr;
    std::vector<class ItemPickup*> itemPickups;

    std::shared_ptr<Mix_Chunk> oceanWavesChunk;
    StageOceanAmbientController oceanAmbient_;
    /// Canal das ondas (0 = ambiente dedicado, reservado para não colidir com Mix_PlayChannel(-1) dos SFX).
    int oceanMixerChannel = -1;
    /// Set true at end of LoadAssets(); LoadingState may call LoadAssets before Start() — Start skips a second load.
    bool levelContentLoaded = false;
};

#endif
