#ifndef STAIRTRIGGER_H
#define STAIRTRIGGER_H

#include "Component.h"
#include "GameObject.h"

class StairTrigger : public Component {
public:
    StairTrigger(GameObject& associated);
    ~StairTrigger();

    void Update(float dt) override;
    void Render() override;
};

#endif