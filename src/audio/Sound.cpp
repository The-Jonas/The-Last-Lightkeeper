#include "audio/Sound.h"
#include <string>
#include <iostream>

Sound::Sound(){
    chunk = nullptr;
}

Sound::Sound(const std::string file) : Sound() {
    Open(file);
}

Sound::~Sound() {                                                       
    //if (chunk) {                                                       // Se o  som foi carregado
    //   Stop();                                                         // para a reprodução    
    //    Mix_FreeChunk(chunk);                                          // e desaloca a memória
    //}

    // O destrutor não deve mais desalocar o som, pois a classe Resources agora gerencia a memória.
}

void Sound::Play(int times) {
    // Com o parâmetro "-1" a Mixer vai escolher um canal livre e retorna-lo
    channel = Mix_PlayChannel( -1, chunk.get(), times - 1);              // o parâmetro loops indica a quantidade de repetições e não de reproduções, por isso "times - 1"

    if (channel == -1) {
        std::cerr << "Erro ao reproduzir som: " << Mix_GetError() << std::endl;
    }
}

void Sound::Stop() {
    if (chunk && Mix_Playing(channel)) {                                // Verifica se o som existe e se está tocando em algum canal antes de pará-lo
        Mix_HaltChannel(channel);
    }
}

void Sound::Open(const std::string file) {

    chunk = Resources::GetSound(file);                                  // Usa Resources para carregar o som.
    if (!chunk) {
        std::cerr << "Erro ao carregar som: " << file << " - " << Mix_GetError() << std::endl;
    }

}

bool Sound::IsOpen(){
    return chunk != nullptr;                                            // Verifica se o som está aberto
}