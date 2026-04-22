#ifndef STAGESTATE_H
#define STAGESTATE_H

#define INCLUDE_SDL
#include "SDL_include.h"

#include "State.h"
#include "Music.h"
#include "TileSet.h"

class Character;
class GameObject;

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
    bool IsPartyReady() const;                                           // Confere se referências da dupla são válidas

    Music music;                                                        // Música de Fundo
    TileSet* tileSet;                                                   // Caso precise guardar ponteiro
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
};

#endif   