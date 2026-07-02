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

    // Tremor de tela (trauma 0..1 que decai sozinho). Acumulativo: chame em
    // eventos de impacto (toque do monstro, trovão forte, etc.).
    static void AddTrauma(float amount);
    // Vertigem por baixa sanidade (0..1) — balanço lento e desorientador.
    // Definido a cada frame pela StageState a partir da sanidade mais baixa.
    static void SetVertigo(float amount);
    // Zera tremor/vertigem (ex.: ao carregar/transicionar de nível).
    static void ResetShake();

    Vec2 static pos;                                // Posição da câmera (já inclui o offset de tremor)
    Vec2 static speed;                              // Velocidade da câmera

private:
    static GameObject* focus;                       // Ponteiro para o GameObject que a câmera está seguindo
    static GameObject* pairA;                       // Primeiro alvo do modo de dupla
    static GameObject* pairB;                       // Segundo alvo do modo de dupla
    static GameObject* pairPrimary;                 // Alvo controlado (peso maior no enquadramento)
    static float zoom;                              // Fator de zoom aplicado no render

    static float trauma;                            // Intensidade do tremor (0..1), decai com o tempo
    static float vertigo;                           // Intensidade da vertigem (0..1)
    static float fxTime;                            // Relógio interno para o ruído do tremor/balanço
    static Vec2 shakeOffset;                        // Offset atual somado em `pos` (tremor + balanço)
};

#endif