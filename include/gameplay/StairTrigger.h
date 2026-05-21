#ifndef STAIRTRIGGER_H
#define STAIRTRIGGER_H

#include "engine/Component.h"
#include "engine/GameObject.h"

class StairTrigger : public Component {
public:
    StairTrigger(GameObject& associated);
    ~StairTrigger();

    void Update(float dt) override;
    void Render() override;
};

#endif