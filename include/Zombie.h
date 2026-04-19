#ifndef ZOMBIE_H
#define ZOMBIE_H

#include "GameObject.h"
#include "Sprite.h"
#include "Component.h"
#include "Sound.h"
#include "Timer.h"

class Zombie : public Component {
public:
    Zombie(GameObject& associated);
    ~Zombie();

    void Damage(int damage);                
    void Update(float dt) override;             // Metodos virtuais
    void Start() override;
    void Render() override;                     // Herdados de Component
    void NotifyCollision(GameObject& other) override;

    static int zombieCount;

private:
    int hitpoints;                              // Zombie é um inimigo com determinada quantidade de HP
    bool zombieIsDead;                          // Uma flag criada por minha pessoa, pra saber se o zombie já tá morto
    bool hit;                                   // A flag de estado para hit
    Timer hitTimer;                             // O timer para a animação de hit
    Timer deathTimer;                           // O timer para a remoção do objeto zombie

    Sound deathSound;                           // Linha para fazer o som de morte do zumbi adicionada no trabalho 3
    Sound damageSound;                          // Som quando o zombie toma dano

};

#endif