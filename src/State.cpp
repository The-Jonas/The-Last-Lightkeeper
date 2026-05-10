#include "../include/State.h"
#include <algorithm>

State::State() {
    popRequested = false;
    quitRequested = false;
    started = false;
}

State::~State() {
    objectArray.clear();
}

std::weak_ptr<GameObject> State::AddObject(GameObject* go) {
    std::shared_ptr<GameObject> sharedPtr(go);                  // 1. Cria um std::shared_ptr a partir do ponteiro bruto.
    objectArray.push_back(sharedPtr);                           // 2. Faz um push_back desse shared_ptr em objectArray. (Adiciona ele ao vetor)
    if (started) {                                              // 3. Se started já tiver sido chamado, chame o Start desse GameObject.
        go->Start();
    }
    return std::weak_ptr<GameObject>(sharedPtr);                // 4. Retorna um std::weak_ptr construído a partir do shared_ptr. (referência segura)
}

std::weak_ptr<GameObject> State::GetObjectPtr(GameObject* go) {
    for (auto& gameObjectPtr : objectArray) {                   // Percorre o vetor de objetos
        if (gameObjectPtr.get() == go) {                        // Compara o endereço armazenado no shared_ptr (ponteiro puro) com o ponteiro 'go'
            return std::weak_ptr<GameObject>(gameObjectPtr);    // Retorna um weak_ptr criado a partir do shared_ptr encontrado
        }
    }
    return std::weak_ptr<GameObject>();                         // Retorna um weak_ptr vazio caso não encontre
}

bool State::PopRequested() { return popRequested; }
bool State::QuitRequested() { return quitRequested; }

void State::StartArray() {
    for (size_t i = 0; i < objectArray.size(); i++) {
        objectArray[i]->Start();
    }
}

void State::UpdateArray(float dt) {
    for (size_t i = 0; i < objectArray.size(); i++) {
        objectArray[i]->Update(dt);
    }
    for (size_t i = 0; i < objectArray.size();) {
        if (objectArray[i]->IsDead()) {
            objectArray.erase(objectArray.begin() + i);
        } else {
            i++;
        }
    }
}

void State::RenderArray() {
    std::stable_sort(objectArray.begin(), objectArray.end(), [](const auto& a, const auto& b) {
        if (std::abs(a->z - b->z) > 0.01f) return a->z < b->z;
        return a->box.Center().y < b->box.Center().y;
    });
    for (auto& go : objectArray) {
        go->Render();
    }
}