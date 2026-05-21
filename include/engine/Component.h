#ifndef COMPONENT_H
#define COMPONENT_H

class GameObject;           // Declaração antecipada para evitar dependência circular (e por isso foi removido o include do header de GameObject)
#include <string>

class Component {
public:
    Component(GameObject& associated);              // Construtor padrão  
    virtual ~Component();                           // Destrutor`

    virtual void Start();                           // Novo método virtual para a fase de inicialização. (Necessário para polimorfismo)

    virtual void Update(float dt);                  // Métodos virtuais puros
    virtual void Render();                          // (precisam ser implementadas nas classes derivadas)

    virtual void NotifyCollision(GameObject& other);    // Para notificar colisão

protected:
    GameObject& associated;                         // Referência ao objeto de jogo associado
};

#endif