#ifndef BOX_H
#define BOX_H

#include "engine/Component.h"
#include <string>

class Box : public Component {
public:
    // O construtor recebe se a imagem é estática (true = parede) ou dinâmica (false = empurrável)
    Box(GameObject& associated, bool isStatic);
    ~Box();
    
    void Start() override;
    void Update(float dt) override;
    void Render() override;

    // Aqui que a mágica vai acontecer quando alguém tocar em um objeto dinâmico
    void NotifyCollision(GameObject& other) override;

private:
    bool isStatic;
};

#endif 