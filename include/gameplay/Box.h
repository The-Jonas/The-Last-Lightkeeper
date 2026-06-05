#ifndef BOX_H
#define BOX_H

#include "engine/Component.h"
#include <string>

class Box : public Component {
public:
    // O construtor recebe se a imagem é estática (true = parede) ou dinâmica (false = empurrável)
    Box(GameObject& associated, bool isStatic);
    ~Box();
    
    void Start() override;
    void Update(float dt) override;
    void Render() override;

    // Aqui que a mágica vai acontecer quando alguém tocar em um objeto dinâmico
    void NotifyCollision(GameObject& other) override;

    bool IsPushable() const { return !isStatic; }
    GameObject& GetAssociated() { return associated; }
    const GameObject& GetAssociated() const { return associated; }

    static void SetActivePushTarget(Box* box);
    static bool IsActivePushTarget(const Box* box);
    bool TryMoveBy(float dx, float dy);

private:
    bool IsPositionBlocked() const;
    bool isStatic;
};

#endif 