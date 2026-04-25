#define PL_MPEG_IMPLEMENTATION
#include "../include/VideoPlayer.h"
#include "../include/Game.h"
#include "../include/GameObject.h"
#include "../include/Camera.h"
#include <iostream>

// Callback que em C puro que o pl_mpeg chama para entregar um frame de vídeo.
void OnVideoFrame(plm_t *plm, plm_frame_t * frame, void *user) {
    VideoPlayer* player = static_cast<VideoPlayer*>(user);              // Transformamos o ponteiro genérico de volta na classe C++
    player->UpdateTexture(frame);                                       // Atualiza a textura do vídeo com o frame recebido                 
}

VideoPlayer::VideoPlayer(GameObject& associated, std::string file) : Component(associated) {
    // Abre o arquivo de vídeo 
    plm = plm_create_with_filename(file.c_str());

    if (!plm) {
        std::cerr << "Erro ao carregar vídeo: " << file << std::endl;
        videoTexture = nullptr;
        return;
    }

    // Desliga o áudio do vídeo, já que não vamos usar
    plm_set_audio_enabled(plm, FALSE);

    // Informa qual função deve ser chamada quando houver frame novo
    plm_set_video_decode_callback(plm, OnVideoFrame, this);

    // Cria a Textura de Streaming (preparada para mudar a cada frame)
    videoTexture = SDL_CreateTexture(
        Game::GetInstance().GetRenderer(),
        SDL_PIXELFORMAT_YV12,                                            // Formato de pixel usado pelo pl_mpeg
        SDL_TEXTUREACCESS_STREAMING,                                     // A textura será atualizada frequentemente
        plm_get_width(plm),
        plm_get_height(plm)
    );
    
    // Estica o GameObject para preencher a tela inteira (FULL HD)
    associated.box.w = Game::GetInstance().GetWindowsWidth();
    associated.box.h = Game::GetInstance().GetWindowsHeight();
}

VideoPlayer::~VideoPlayer() {
    if (videoTexture) {
        SDL_DestroyTexture(videoTexture);
    }
    if (plm) {
        plm_destroy(plm);     
    }
}

void VideoPlayer::UpdateTexture(plm_frame_t* frame) {
    if (!videoTexture) {
        return; 
    }

    // Joga os dados crus do decodificador direto para a Placa de vídeo
    SDL_UpdateYUVTexture(
        videoTexture,
        NULL,
        frame->y.data, frame->y.width,
        frame->cb.data, frame->cb.width,
        frame->cr.data, frame->cr.width              
    );
}

void VideoPlayer::Update(float dt) {
    if (!plm) {
        return;
    }

    // Avança o relógio interno da biblioteca
    // Se o vídeo perceber que já passou tempo suficiente, ele chama a função OnVideoFrame para entregar o próximo frame
    plm_decode(plm, dt);
}

void VideoPlayer::Render() {
    if (!videoTexture) {
        return;
    }

    // Calculamos a posição igual a classe Sprite
    SDL_Rect dstRect;
    dstRect.x = associated.box.x - Camera::pos.x;
    dstRect.y = associated.box.y - Camera::pos.y;
    dstRect.w = associated.box.w;
    dstRect.h = associated.box.h;

    SDL_RenderCopyEx(Game::GetInstance().GetRenderer(),
    videoTexture, 
    nullptr, 
    &dstRect,
    associated.angleDeg,
    nullptr,
    SDL_FLIP_NONE);
}

bool VideoPlayer::HasEnded() {
    if (!plm) {
        return true;
    }
    return plm_has_ended(plm);                                  // Verifica se o vídeo terminou de tocar
}
