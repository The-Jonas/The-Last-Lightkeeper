#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H

#define INCLUDE_SDL

#include "engine/Component.h"
#include "math/Rect.h"
#include <vector>
#include <memory>

// Representa uma entidade do jogo (entidade genérica) 
class GameObject {
public:
    GameObject();                                   // Construtor padrão
    ~GameObject();                                  // Destrutor 

    void Start();                                   // Novo método para a fase de inicialização do GameObject.

    void Update(float dt);                          // Atualiza todos os componentes
    void Render();                                  // Chama Render() de todos os componentes
    bool IsDead();                                  // Retorna se o objeto deve ser destruído
    void RequestDelete();                           // Marca o objeto para remoção
    void AddComponent(Component* cpt);              // Adiciona um novo componente
    void RemoveComponent(Component* cpt);           // Remove componente
    void NotifyCollision(GameObject& other);        // Função para notificar a colisão 
    
    // Declaração e implementação do template de função aqui (Obs: Refatorei um pouco o código dado para deixa-ló de forma mais moderna)
    template<typename T>
    T* GetComponent() {                                                     
        for (Component* component : components) {                       // Percorre o vetor de componentes
            T* specific_component = dynamic_cast<T*>(component);        // Tenta fazer o cast do ponteiro para o tipo T
            if (specific_component != nullptr) {                        // Se o cast for bem-sucedido, retorna o ponteiro
                return specific_component;
            }
        }                                                               
        return nullptr;                                                 // Se nenhum componente do tipo T for encontrado, retorna nullptr
    }

    Rect box;                                       // Posição e tamanho
    double angleDeg;                                // Para os objetos terem o ângulo em graus

    // Vou fazer o extra do trabalho 5 (Z/Y Sorting)
    // Z-Sorting (Profundidade): Objetos com z menor (como o fundo) são desenhados primeiro.
    // Y-Sorting (Posição Vertical): Se dois objetos tiverem o mesmo z (como o Jogador e um Zumbi), aquele com o menor y (mais "alto" na tela) é desenhado primeiro.
    int z;
    // Adicionados especificamente pra arma ficar sempre na frente do nosso personagem
    int sub_z;                                      // Sub_camada (pra desempate)
    GameObject* owner;                              // Ponteiro para o "dono" (Para herdar o Y)

private:
    std::vector <Component*> components;            // Vetor de componentes do jogo
    bool isDead;                                    // Se verdadeiro, significa que o componente está morto
    bool started;                                   // Para verificar se Start já foi chamado
};

#endif 