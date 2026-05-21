#pragma once
#include "engine/Component.h"
class Barrier : public Component {
public:
    Barrier(GameObject& associated) : Component(associated) {}
    void Update(float dt) override {}
    void Render() override {}

};