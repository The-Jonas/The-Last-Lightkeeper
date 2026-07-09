#ifndef BOX_H
#define BOX_H

#include "engine/Component.h"
#include <string>

class   Box : public Component {
public:
    // O construtor recebe se a imagem é estática (true = parede) ou dinâmica (false = empurrável)
    Box(GameObject& associated, bool isStatic, std::string spritePath = "Recursos/img/objetos/Caixa.png", float weightMulti = 1.0f);
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

    float GetWeightMultiplier() const { 
        return weightMultiplier; 
    }

private:
    bool IsPositionBlocked() const;
    bool isStatic;

    float weightMultiplier; // Guarda o quão pesada é
};

#endif 