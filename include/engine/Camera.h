#ifndef CAMERA_H
#define CAMERA_H

#include "engine/GameObject.h"
#include "math/Vec2.h"

// Classe aparentemente não precisa ser Singleton já que é inteiramente estática

class Camera {
public:

    void static Follow (GameObject* newFocus);      // Seta um GameObject como foco da câmera
    void static Unfollow();                         // Deixa de usar um GameObject como foco da câmera (fica estático)
    void static FollowPair(GameObject* first, GameObject* second, GameObject* primary = nullptr); // Enquadra dois objetos com prioridade visual no principal
    void static ClearPairFollow();                  // Limpa o modo de enquadramento de dupla
    void static Update(float dt);                   // Define a abordagem a ser usada para a câmera durante o jogo
    static float GetZoom();                         // Retorna zoom atual da câmera
    static void SetZoomOffset(float offset);        // Permite injetar o efeito de vertigem

    Vec2 static pos;                                // Posição da câmera 
    Vec2 static speed;                              // Velocidade da câmera

private:
    static GameObject* focus;                       // Ponteiro para o GameObject que a câmera está seguindo
    static GameObject* pairA;                       // Primeiro alvo do modo de dupla
    static GameObject* pairB;                       // Segundo alvo do modo de dupla
    static GameObject* pairPrimary;                 // Alvo controlado (peso maior no enquadramento)
    static float zoom;                              // Fator de zoom aplicado no render
    static float zoomOffset;                        // Camada de distorção visual
};

#endif