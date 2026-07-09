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
    channel = Mix_PlayChannel(-1, chunk.get(), times - 1);
    if (channel == -1) {
        std::cerr << "Erro ao reproduzir som: " << Mix_GetError() << std::endl;
    }
}

int Sound::PlayLooped() {
    if (!chunk) {
        return -1;
    }
    channel = Mix_PlayChannel(-1, chunk.get(), -1);
    if (channel == -1) {
        std::cerr << "Erro ao reproduzir som em loop: " << Mix_GetError() << std::endl;
    }
    return channel;
}

int Sound::PlayLoopedOnChannel(int ch) {
    // Toca em loop infinito num canal específico (reservado).
    // Se o canal já estiver tocando esse mesmo chunk, não reinicia.
    if (!chunk) return -1;
    if (Mix_Playing(ch) && channel == ch) return ch; // já tocando, não reinicia
    channel = Mix_PlayChannel(ch, chunk.get(), -1);
    if (channel == -1) {
        std::cerr << "Erro ao reproduzir som no canal " << ch << ": "
                  << Mix_GetError() << std::endl;
    }
    return channel;
}

void Sound::Stop() {
    if (channel != -1 && chunk && Mix_Playing(channel)) {
        Mix_HaltChannel(channel);
        channel = -1;  // reseta após parar
    }
}

void Sound::FadeOut(int ms) {
    // Desliga suavemente (fade) em vez de cortar seco. O canal se libera sozinho
    // ao fim do fade; soltamos a referência aqui.
    if (channel != -1 && chunk && Mix_Playing(channel)) {
        Mix_FadeOutChannel(channel, ms);
        channel = -1;
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