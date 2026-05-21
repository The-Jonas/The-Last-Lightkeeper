#ifndef STATE_H
#define STATE_H

#include "engine/GameObject.h"
#include <vector>
#include <memory>

class State {
public:
    State();
    virtual ~State();

    // --- Interface de Ciclo de Vida do Estado ---

    virtual void LoadAssets() = 0;                                      // Carrega recursos (sprites, sons) específicos deste estado
    virtual void Update(float dt) = 0;                                  // Atualiza a lógica do estado (input, física, etc.)
    virtual void Render() = 0;                                          // Renderiza o estado na tela

    virtual void Start() = 0;                                           // Chamado quando o estado é empilhado ou iniciado
    virtual void Pause() = 0;                                           // Chamado quando outro estado é empilhado por cima deste
    virtual void Resume() = 0;                                          // Chamado quando este estado volta a ser o topo da pilha'

    // --- Gerenciamento de Objetos ---

    virtual std::weak_ptr<GameObject> AddObject(GameObject* go);        // Adiciona um objeto ao array do estado e o inicializa
    virtual std::weak_ptr<GameObject> GetObjectPtr(GameObject* go);     // Retorna um ponteiro fraco para um objeto existente

    // --- Controle de Fluxo ---

    bool PopRequested();                                                // Retorna true se este estado deve ser desempilhado
    bool QuitRequested();                                               // Retorna true se o jogo deve ser encerrado completamente

protected:
    // Métodos auxiliares para as classes filhas usarem
    void StartArray();                                                  // Percorre o array chamando Start() nos objetos novos
    virtual void UpdateArray(float dt);                                 // Percorre o array chamando Update() nos objetos e removendo os mortos
    virtual void RenderArray();                                         // Percorre o array chamando Render() nos objetos
    
    bool popRequested;                                                  // Flag para pedir remoção da pilha
    bool quitRequested;                                                 // Flag para pedir encerramento do jogo
    bool started;                                                       // Indica se o estado já foi inicializado (Start chamado)

    std::vector<std::shared_ptr<GameObject>> objectArray;               // Vetor que contém todos os GameObjects deste estado
};

#endif