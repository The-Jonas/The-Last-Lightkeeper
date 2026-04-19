#ifndef ANIMATOR_H
#define ANIMATOR_H

#include "Component.h"
#include "Animation.h"
#include <string>
#include <unordered_map>


class Animator : public Component {
public:
    Animator(GameObject& associated);

    void Update(float dt) override;                             // Metodos virtuais
    void Render() override;                                     // Herdados de Component

    void SetAnimation(std::string name);                        // Define a animação atual para ser reproduzida, buscando-a no mapa
    void AddAnimation(std::string name, Animation anim);        // Adiciona uma nova animação ao mapa.

private:

    std::unordered_map<std::string, Animation> animations;      // Mapa que armazena as animações, usando o nome como chave.

    // Atributos da animação atual em execução.
    int frameStart, frameEnd;                                   // O primeiro e último quadro da animação
    float frameTime;                                            // O tempo em segundos que cada quadro deve ser exibido
    int currentFrame;                                               
    float timeElapsed;

    std::string currentAnimation;                               // Guarda o nome da animação atual

};

#endif