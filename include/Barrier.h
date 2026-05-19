#pragma once
#include "Component.h"
class Barrier : public Component {
public:
    Barrier(GameObject& associated) : Component(associated) {}
    void Update(float dt) override {}
    void Render() override {}

};