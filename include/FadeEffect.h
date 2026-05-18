#ifndef FADEEFFECT_H
#define FADEEFFECT_H

#include "Component.h"
#include "GameObject.h"
#include "Rect.h"

class FadeEffect : public Component {
public:
    FadeEffect(GameObject& associated);
    ~FadeEffect();

    void Update(float dt) override;
    void Render() override;
};

#endif