#ifndef RESOURCES_H
#define RESOURCES_H

#define INCLUDE_SDL
#define INCLUDE_SDL_IMAGE
#define INCLUDE_SDL_MIXER
#define INCLUDE_SDL_TTF

#include "SDL_include.h"

#include <unordered_map>
#include <string>
#include <memory>

class Resources {
public:
    // Obtém uma textura. Se ela já estiver em memória, retorna o ponteiro existente.
    // Se não, carrega, armazena e retorna o ponteiro.
    static std::shared_ptr<SDL_Texture> GetImage(const std::string file);
    static void ClearImages();                                                          // Libera todas as texturas da memória

    // Obtém uma música. Se ela já estiver em memória, retorna o ponteiro existente.
    // Se não, carrega, armazena e retorna o ponteiro.
    static std::shared_ptr<Mix_Music> GetMusic (const std::string file);
    static void ClearMusics();                                                          // Libera todas as músicas da memória
    
    // Obtém um som. Se ele já estiver em memória, retorna o ponteiro existente.
    // Se não, carrega, armazena e retorna o ponteiro.
    static std::shared_ptr<Mix_Chunk> GetSound (const std::string file);    
    static void ClearSounds();                                                          // Libera todos os sons da memória

    // Obtém uma fonte. Se ela já estiver na memória, retorna o ponteiro existente
    // Se não, carrega, armazena e retorna o ponteiro
    static std::shared_ptr<TTF_Font> GetFont(std::string file, int fontSize);
    static void ClearFonts();

private:
    // Tabelas agora armazenam shared_ptr
    static std::unordered_map<std::string, std::shared_ptr<SDL_Texture>> imageTable;        // Mapeamento de strings (caminho do arquivo) para ponteiros dos assets
    static std::unordered_map<std::string, std::shared_ptr<Mix_Music>> musicTable;          // Eles armazenam os ativos para
    static std::unordered_map<std::string, std::shared_ptr<Mix_Chunk>> soundTable;          // evitar carregamentos duplicados
    static std::unordered_map<std::string, std::shared_ptr<TTF_Font>> fontTable;            // Tabela de fontes

    //Construtor privado para evitar a instanciação

    Resources() {}

};

#endif