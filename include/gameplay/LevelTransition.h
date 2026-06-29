#ifndef LEVEL_TRANSITION_H
#define LEVEL_TRANSITION_H

#include "engine/Component.h"

class LevelTransition : public Component {
public:
    LevelTransition(GameObject& associated, int targetLevelIndex);

    void Update(float dt) override;
    void Render() override;

private:
    int targetLevelIndex;
    float cooldownTimer = 0.0f;
};

#endif
