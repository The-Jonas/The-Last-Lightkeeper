#ifndef BULLET_H
#define BULLET_H

#include "Component.h"
#include "Vec2.h"
#include <string>

class Bullet : public Component {

    // O projétil expirará uma hora para que ele não ande pelo mundo infinitamente

public:
    Bullet(GameObject& associated, float angle, float speed, int damage, float maxDistance, bool targetsPlayer);            // Recebe ângulo, segue em linha reta após a criação

    //Métodos de ciclo de vida
    void Update(float dt) override;
    void Render() override;
    void Start() override;
    void NotifyCollision(GameObject& other) override;
    
    int GetDamage();                                    // Retorna o dano
    bool targetsPlayer;

private:
    Vec2 speed;                                         // Módulo de velocidade
    float distanceLeft;                                 // Distância a ser percorrida
    int damage;                                         // Dano que a Bullet causará

};

#endif