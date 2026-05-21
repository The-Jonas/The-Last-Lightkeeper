#ifndef SOUND_H
#define SOUND_H

#define INCLUDE_SDL_MIXER
#include "SDL_include.h"
#include "core/Resources.h"
#include <string>
#include <memory>

// A classe Sound vai gerenciar a reprodução de efeitos sonoros
// Similar a Music, mas lida com sons de curta duração
// que podem ser reproduzidos em canais múltiplos
// por  isso temos que manter registrado em qual canal o chunk está tocando
// para porá-lo se necessário

class Sound {
public:
    Sound();                                            // Construtor Padrão
    Sound(const std::string file);                      // Construtor que já carrega o arquivo de som
    ~Sound();                                           // Destrutor - Desaloca o som da memória

    void Play(int times = 1);                           // Reproduz o som, o parâmetro "times" define quantas vezes vai ser repetido
    void Stop();                                        // Para a reprodução do som no canal em que ele está tocando
    void Open(const std::string file);                  // Carrega o arquivo de som a partir do caminho fornecido
    bool IsOpen();                                      // Verificar se o arquivo foi aberto

private:

    std::shared_ptr<Mix_Chunk> chunk;                   // Ponteiro para o bloco de som (arquivo.wav) carregado pelo SDL_Mixer
    int channel;                                        // O canal em que o som está sendo reproduzido (Vão ter 32 canais no total)

};

#endif 