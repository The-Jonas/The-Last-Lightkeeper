#ifndef FADEEFFECT_H
#define FADEEFFECT_H

#include "Component.h"
#include "GameObject.h"
#include "Rect.h"

class FadeEffect : public Component {
public:
    FadeEffect(GameObject& associated, bool ignoreElevated = false);
    ~FadeEffect();

    void Update(float dt) override;
    void Render() override;

private:
    bool ignoreElevated;                // Função criada com o intuito de só deixar a escada não ter o efeito Fade
};

#endif  