#ifndef CLOSET_H
#define CLOSET_H

#include "engine/Component.h"
#include "engine/GameObject.h"
#include <string>

#define INCLUDE_SDL
#include "SDL_include.h"

class Closet : public Component {
public:
    Closet(GameObject& associated);
    ~Closet();

    void Start() override;
    void Update(float dt) override;
    void Render() override;

private:
    std::string direction;
    bool isOccupied;
    int frestaLightId;

    Vec2 playerEntryPos;
    Vec2 brotherEntryPos;

    // Latch p/ a fala de medo do irmãozinho: dispara UMA vez por aproximação do
    // monstro (rearma só quando ele se afasta), em vez de repetir a cada ~4s.
    bool hideVoiceArmed = true;

    // #4 Sussurro aleatório "E se o monstro estiver lá fora?" enquanto escondido.
    // Conta regressiva reiniciada com um intervalo aleatório a cada disparo.
    float whisperTimer = 0.0f;
    void ArmWhisperTimer();   // sorteia o próximo intervalo do sussurro

    void EnterCloset();
    void ExitCloset();

    // Funções auxiliares para lidar com a direção
    SDL_Rect GetInteractionRect() const;
    

};

#endif 