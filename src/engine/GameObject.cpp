#include "engine/GameObject.h"
#include "engine/Component.h"
#include "math/Rect.h"

#include <iostream>
#include <memory>               // Para std::unique_ptr
#include <algorithm>            // Para a função std::find_if

GameObject::GameObject() {                                                          // O construtor vai inicializar com isDead sendo falso como garantia
    isDead = false;
    started = false;
    angleDeg = 0.0f;
    depthOffset = 0.0f;
    z = 0;                                                                          // Lembrar de definir para os objetos quando forem criados (ordem de quem fica na frente)
    sub_z = 0;                                                                      // Padrão é a camada base
    owner = nullptr;                                                                // Padrão é não ter dono
}
 
GameObject::~GameObject(){                                                          // Percorre o vetor de componentes, deleta e limpa o vetor
    for(auto it = components.rbegin(); it != components.rend(); ++it){              // Iterando de trás pra frente
        delete *it;
    }
    components.clear();
}

void GameObject::Update(float dt) {                                                 // Percorre o vetor de componentes chamando o update de cada um
    for (Component* cpt : components){                                              
        cpt->Update(dt);
    }
}

void GameObject::Render() {                                                         // Percorre o vetor de componentes chamando o render dos mesmos
    for (Component* cpt : components){
        cpt->Render();
    }
}

void GameObject::Start() {
    if (started) {                                                                  // Se já startou, não faz nada
        return;
    }

    for (Component* cpt : components) {                                             // Percorre os componentes chamando o Start deles.
        cpt -> Start();
    }

    started = true;                                                                 // Define a flag como true, já que foi chamado o start
}

void GameObject::AddComponent(Component* cpt) {                                     // Adiciona o componente ao vetor 'components'
    components.push_back(cpt);
    
    if (started) {
        cpt -> Start();                                                             // Chamando o Start do novo componente se o GameObject já foi iniciado.
    }
}

void GameObject::RemoveComponent(Component* cpt) {                                  // Remove o componente do vetor, se ele estiver lá 
    auto it = std::find_if(components.begin(), components.end(),                    // Usa-se um lambda e a função std::find_if para encontrar o ponteiro
        [cpt](Component* existing_cpt) {
            return existing_cpt == cpt;
        });

    if (it != components.end()) {
        components.erase(it);                                                       // e depois o removemos com erase.
    }
}

void GameObject::NotifyCollision(GameObject& other){
    for (Component* cpt : components) {
        cpt->NotifyCollision(other);
    }
}

bool GameObject::IsDead() {                                                         // Retorna o valor do membro "isDead"
    return isDead;
}

void GameObject::RequestDelete() {                                                  // Atribui "true" ao membro "isDead"
    isDead = true;
}


// A implementação de GetComponent<T>() é complexa e foi fornecida no drive.
// A assinatura do template de função, no entanto, pode ser declarada no .h.
// A implementação precisa ser feita no arquivo de cabeçalho para templates.