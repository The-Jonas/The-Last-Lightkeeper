#ifndef MONSTERSILHOUETTE_H
#define MONSTERSILHOUETTE_H
 
#include "engine/Component.h"
#include "engine/GameObject.h"
#include "gameplay/Monster.h"
 
class Monster;
 
class MonsterSilhouette : public Component {
public:
    MonsterSilhouette(GameObject& associated, Monster* owner)
        : Component(associated), monsterOwner(owner) {}
 
    void Start()  override {}
    void Update(float dt) override {
        // Mantém a box sincronizada com o monstro
        if (monsterOwner) {
            associated.box = monsterOwner->GetAssociated().box;
        }
    }
    void Render() override; // Implementado no .cpp
 
private:
    Monster* monsterOwner = nullptr;
};
 
#endif