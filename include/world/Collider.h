#ifndef COLLIDER_H
#define COLLIDER_H

#include "engine/Component.h"
#include "math/Rect.h"
#include "math/Vec2.h"

class Collider : public Component {
public:
    Collider(GameObject& associated, Vec2 scale = {1, 1}, Vec2 offset = {0, 0});                // Recebe o objeto associado, e opcionalmente escala e offset 
    
    void Update(float dt) override;                                                 // Atualiza a posição e rotação da box de colisão a cada frame
    void Render() override;                                                         // Desenha a box de colisão (útil para Debug)
    
    void SetScale(Vec2 scale);                                                      // Define a escala da box de colisão
    void SetOffset(Vec2 offset);                                                    // Define o deslocamento da box em relação ao centro do objeto

    Rect box;                                                                       // A caixa de colisão calculada final (pública para ser acessada pelo sistema de colisão)

private:

    Vec2 scale;                                                                     // Fator de escala da colisão em relação ao sprite original
    Vec2 offset;                                                                    // Distância do centro da colisão em relação ao centro do objeto

};

#endif