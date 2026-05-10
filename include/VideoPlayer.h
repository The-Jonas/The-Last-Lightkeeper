#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H

#define INCLUDE_SDL
#include "SDL_include.h"
#include <stdio.h>
#include "pl_mpeg.h"

#include "Component.h"
#include <string>

class VideoPlayer : public Component {
public:
    VideoPlayer(GameObject& associated, std::string file);  // Recebe o Game Object associado e o caminho do arquivo de vídeo.
    ~VideoPlayer();                                         // Destrutor 

    void Update(float dt) override;                         // Atualiza o estado do vídeo (avança o frame, verifica se terminou, etc.)
    void Render() override;                                 // Renderiza o frame atual do vídeo na tela, ajustando pela posição da câmera

    bool HasEnded();                                        // Retorna true se o vídeo terminou de tocar
    void UpdateTexture(plm_frame_t* frame);                 // Método para atualizar a textura SDL com o frame atual do vídeo
    
private:
    plm_t* plm;                                             // Ponteiro para a estrutura de vídeo do pl_mpeg
    SDL_Texture* videoTexture;                              // Textura SDL onde o frame atual do vídeo será renderizado    
}; 


#endif