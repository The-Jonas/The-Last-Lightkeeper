#ifndef STAIRTRIGGER_H
#define STAIRTRIGGER_H

#include "engine/Component.h"
#include "engine/GameObject.h"

class StairTrigger : public Component {
public:
    StairTrigger(GameObject& associated, float anchorY);
    ~StairTrigger();

    void Update(float dt) override;
    void Render() override;

private:
    float anchorY; // Guarda o valor que veio do Tiled para verificar a base Y do asset que vamos subir
};

#endif  