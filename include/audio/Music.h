#ifndef MUSIC_H
#define MUSIC_H

#define INCLUDE_SDL_MIXER
#include "SDL_include.h"

#include <string>
#include <memory>

class Music {
public:
    Music();                                // Construtor Padrão
    Music(const std::string& file);         // Construtor que já abre música
    ~Music();                               // Destrutor

    void Play(int times = -1);              // Toca a música (padrão: Infinito)
    void Stop(int msToStop = 1500);         // Para a música (fade-out)
    void Open(const std::string& file);     // Carrega a música
    bool IsOpen();                          // Verifica se foi carregada

private:
    std::shared_ptr<Mix_Music> music; 
};

#endif //MUSIC_H