#ifndef CAMERA_H
#define CAMERA_H

#include "GameObject.h"
#include "Vec2.h"

// Classe aparentemente não precisa ser Singleton já que é inteiramente estática

class Camera {
public:

    void static Follow (GameObject* newFocus);      // Seta um GameObject como foco da câmera
    void static Unfollow();                         // Deixa de usar um GameObject como foco da câmera (fica estático)
    void static Update(float dt);                   // Define a abordagem a ser usada para a câmera durante o jogo

    Vec2 static pos;                                // Posição da câmera 
    Vec2 static speed;                              // Velocidade da câmera

private:
    static GameObject* focus;                       // Ponteiro para o GameObject que a câmera está seguindo
};

#endif