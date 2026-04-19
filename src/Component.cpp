#include "Component.h"
#include "GameObject.h"

Component::Component(GameObject& associated) : associated(associated) {}

Component::~Component() {
}

void Component::Render() {
}

void Component::Update(float dt) {
}

void Component::NotifyCollision(GameObject& other){
}

void Component::Start() {
    // Vazio pra ser usado com polimorfismo, igual Update e Render
}