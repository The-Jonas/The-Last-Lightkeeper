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

    // ── Zoom-base ───────────────────────────────────────────────────────────
    // Valor para onde a câmera SEMPRE volta. 1.0 = enquadramento antigo (1 pixel
    // de mundo = 1 pixel de tela). Abaixo de 1.0 a câmera afasta e mostra mais
    // mundo. Só a StageState mexe nisto; os outros estados desenham a interface
    // a 1:1 e por isso chamam `ResetView()` ao arrancar.
    static void SetBaseZoom(float newBaseZoom, bool snap = false);
    static float GetBaseZoom();

    // ── Limites do mundo ────────────────────────────────────────────────────
    // Retângulo da arte do nível. Enquanto estiver definido, a câmera nunca
    // mostra fora do mapa: sem isto, afastar a câmera deixa ver o vazio preto
    // para lá das bordas pintadas. Num eixo em que o mundo é menor que a tela,
    // a câmera centra em vez de encostar a uma borda.
    static void SetWorldBounds(float minX, float minY, float maxX, float maxY);
    static void ClearWorldBounds();

    // ── Mundo ↔ tela ────────────────────────────────────────────────────────
    // A MESMA conta que o `Sprite` faz: subtrai a câmera E multiplica pelo zoom.
    // Todo desenho direto no renderer (traços de debug, círculos, rótulos) tem
    // de passar por aqui. Só subtrair `pos` esquece o zoom, e aí a forma aparece
    // deslocada do sprite — o erro cresce com a distância ao canto da tela.
    // Tamanhos (largura, altura, raio) multiplicam-se por `GetZoom()`.
    static Vec2 WorldToScreen(const Vec2& world);
    static Vec2 ScreenToWorld(const Vec2& screen);

    // Devolve a câmera ao estado neutro: zoom 1.0 imediato, sem limites de mundo
    // e sem tremor. Os estados que desenham a 1:1 (título, loading, cutscene,
    // fim) chamam isto no `Start()`.
    static void ResetView();

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
    static float baseZoom;                          // Zoom para onde a câmera converge
    static bool hasWorldBounds;                     // true quando os limites abaixo valem
    static float worldMinX, worldMinY;              // Canto superior-esquerdo do mundo
    static float worldMaxX, worldMaxY;              // Canto inferior-direito do mundo

    static void ClampPosToWorldBounds();            // Prende `pos` dentro do mundo

    static float trauma;                            // Intensidade do tremor (0..1), decai com o tempo
    static float vertigo;                           // Intensidade da vertigem (0..1)
    static float fxTime;                            // Relógio interno para o ruído do tremor/balanço
    static Vec2 shakeOffset;                        // Offset atual somado em `pos` (tremor + balanço)
};

#endif