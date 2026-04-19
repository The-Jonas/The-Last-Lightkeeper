#ifndef STAGESTATE_H
#define STAGESTATE_H

#define INCLUDE_SDL
#include "SDL_include.h"

#include "State.h"
#include "Music.h"
#include "TileSet.h"

class StageState : public State {
public:
    StageState();                                                       // Construtor
    ~StageState();                                                      // Destrutor

    void LoadAssets() override;                                         // Carrega assets do estado
    void Update(float dt) override ;                                    // Atualiza lógica de estado
    void Render() override;                                             // Desenha na tela

    void Start() override;                                              // Método para a fase de inicialização.
    void Pause() override;
    void Resume() override;

private:
    Music music;                                                        // Música de Fundo
    TileSet* tileSet;                                                   // Caso precise guardar ponteiro
};

#endif   