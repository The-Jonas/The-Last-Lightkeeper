#include "../include/Music.h"
#include "../include/Resources.h"
#include <iostream> 

Music::Music() {
    music = nullptr;
}

Music::Music(const std::string& file) { 
    music = nullptr;                
    Open(file);
}

Music::~Music(){
    Stop();                                                                         // Para a música
    //if (music) {
    //Mix_FreeMusic(music);                                                         // Libera Música da Memória
    //}
    // O destrutor não deve mais desalocar a música, pois a classe Resources agora gerencia a memória.
}

void Music::Play(int times) {
    if (music) {                                                    
        if (Mix_PlayMusic(music.get(), times) == -1) {                              // Toca música se == 0, se não, retorna erro
            std::cerr << "Erro ao tocar música: " << Mix_GetError() << std::endl;
        }
    }
}

void Music::Open(const std::string& file){

    music = Resources::GetMusic(file);                                              // Usa Resources para carregar a música

    if (!music) {
        std::cerr << "Erro ao carregar música: " << Mix_GetError() << std::endl;
    }
}

void Music::Stop(int msToStop) {                                                    // Fade out na música, abaixa ela gradualmente até acabar (1,5 segundos passado)
    Mix_FadeOutMusic(msToStop);
}

bool Music::IsOpen(){
    return music != nullptr;                                                        // Retorna se a música foi carregada
}