#ifndef STAGESTATE_H
#define STAGESTATE_H

#define INCLUDE_SDL
#include "SDL_include.h"

#include "State.h"
#include "Music.h"
#include "TileSet.h"
#include "LightMaskTypes.h"
#include "RadialLightOverlay.h"
#include "LightTweakPanel.h"
#include "TopDownLightShadows.h"
#include "Vec2.h"

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

    void LoadAssets() override;                                         // Carrega assets do estado
    void Update(float dt) override ;                                    // Atualiza lógica de estado
    void Render() override;                                             // Desenha na tela

    void Start() override;                                              // Método para a fase de inicialização.
    void Pause() override;
    void Resume() override;

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
    bool IsPartyReady() const;                                           // Confere se referências da dupla são válidas

    Music music;                                                        // Música de Fundo
    TileSet* tileSet;                                                   // Caso precise guardar ponteiro
    Vec2 mapOrigin{0.0f, 0.0f};
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
    RadialLightOverlay* radialGeometry;                                  // Vignette procedural (várias formas)
    LightMaskParams lightMaskParams;
    LightMaskShape lightMaskShape;
    std::unique_ptr<LightTweakPanel> lightTweakPanel;
    std::vector<LightInstance> lights;
    TileMap* tileMapComp = nullptr;
    std::vector<TopDownShadowEdge> staticShadowEdges;
    bool staticShadowEdgesBuilt = false;
    bool renderStaticTileShadows = false;
    Vec2 smoothedDynamicLightScreenPos{0.0f, 0.0f};
    bool hasSmoothedDynamicLight = false;
    int maxActiveLights = 24;
    bool lightsEnabled = true;
    bool shadowsEnabled = true;
    bool musicMuted = true;
};

#endif
