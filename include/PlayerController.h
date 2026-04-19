#ifndef PLAYER_CONTROLLER_H
#define PLAYER_CONTROLLER_H

#include "Component.h"
#include "Character.h"
#include "Vec2.h"

class PlayerController : public Component {
public:
    PlayerController(GameObject& associated);                           // Construtor apenas pra fazer a ligação entre o GameObject e o controller

    //Métodos de ciclo de vida
    void Start() override;                                              // Vazio
    void Update(float dt) override;                                     // Verifica teclas apertadas (WASD) e movimenta o Character
    void Render() override;                                             // Vazio
};

#endif