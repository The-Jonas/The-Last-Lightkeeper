#ifndef GUN_H
#define GUN_H

#include "GameObject.h"
#include "Sprite.h"
#include "Component.h"
#include "Sound.h"
#include "Timer.h"
#include "Vec2.h"

class Gun: public Component {
public:
    Gun(GameObject& associated, std::weak_ptr<GameObject> character);       // Construtor que recebe o GameObject associado e uma referência fraca ao Character
    
    // Métodos do ciclo de vida
    void Update(float dt) override;                                  
    void Render() override;

    void Shoot(Vec2 target);                                                // Método para atirar em direção a um alvo

private:
    std::weak_ptr<GameObject> character;                                    // Referência fraca ao GameObject do Character

    //Sons da arma
    Sound shotSound;                                
    Sound reloadSound;

    float angle;                                                            // Ângulo de rotação da arma
    int cooldownState;                                                      // Estado de cooldown (0 = pronto, 1 = atirou, 2 = recarregando, 3 = recarregou)
    Timer cdTimer;                                                          // Timer para controlar o cooldown

};



#endif 