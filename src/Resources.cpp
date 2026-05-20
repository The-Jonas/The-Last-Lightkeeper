#include "../include/Resources.h"
#include "../include/Game.h"
#include <iostream>

namespace {

void EnsureMixerDecodersForChunks() {
    static bool codecsWarmedUp = false;
    if (codecsWarmedUp) {
        return;
    }
    codecsWarmedUp = true;
    const int wanted = MIX_INIT_MP3 | MIX_INIT_OGG | MIX_INIT_FLAC | MIX_INIT_WAVPACK | MIX_INIT_MOD;
    Mix_Init(wanted);
}

} // namespace

// Definição e inicialização dos membros estáticos (se não dá erro por confundir o linker)

std::unordered_map<std::string, std::shared_ptr<SDL_Texture>> Resources::imageTable;
std::unordered_map<std::string, std::shared_ptr<Mix_Music>> Resources::musicTable;
std::unordered_map<std::string, std::shared_ptr<Mix_Chunk>> Resources::soundTable;
std::unordered_map<std::string, std::shared_ptr<TTF_Font>> Resources::fontTable;

//---------------------------------------------------------------------------//
//--------------------------------- SPRITE ----------------------------------//
//---------------------------------------------------------------------------//

std::shared_ptr<SDL_Texture> Resources::GetImage(const std::string file) {
    // Se já existe retorna
    if (imageTable.find(file) != imageTable.end()) {
        return imageTable[file];
    }

    // Carrega a textura
    SDL_Texture* texture = IMG_LoadTexture(Game::GetInstance().GetRenderer(), file.c_str());
    if (texture == nullptr) {
        std::cerr << "Erro ao carregar imagem: " << SDL_GetError() << std::endl;
        return nullptr;
    }

    // Cria o shared_ptr com o Deleter Customizado
    // Quando o contador chegar a zero ele chama o SDL_DestroyTexture
    std::shared_ptr<SDL_Texture> shared(texture, [](SDL_Texture* txt) {
        SDL_DestroyTexture(txt);
    });

    imageTable.emplace(file, shared);
    return shared;
}

void Resources::ClearImages() {
    // Percorre a tabela e remove quem só tem 1 referência (a própria tabela)
    for (auto it = imageTable.begin(); it != imageTable.end(); ) {
        // .unique vai retornar true se o use_count == 1
        if (it->second.unique()) {
            it = imageTable.erase(it);                                  // Remove e destroi a textura automaticamente
        } else {
            ++it;
        }
    }
}

//---------------------------------------------------------------------------//
//------------------------------- SOUND -------------------------------------//
//---------------------------------------------------------------------------//

std::shared_ptr<Mix_Chunk> Resources::GetDecodedChunk(const std::string file) {
    if (soundTable.find(file) != soundTable.end()) {
        return soundTable[file];
    }

    EnsureMixerDecodersForChunks();

    SDL_RWops* rw = SDL_RWFromFile(file.c_str(), "rb");
    if (!rw) {
        std::cerr << "Erro ao abrir som \"" << file << "\": " << SDL_GetError() << std::endl;
        return nullptr;
    }

    Mix_Chunk* chunk = Mix_LoadWAV_RW(rw, SDL_TRUE);
    if (chunk == nullptr) {
        std::cerr << "Erro ao decodificar som \"" << file << "\": " << Mix_GetError() << std::endl;
        return nullptr;
    }

    std::shared_ptr<Mix_Chunk> shared(chunk, [](Mix_Chunk* snd) {
        Mix_FreeChunk(snd);
    });

    soundTable.emplace(file, shared);
    return shared;
}

std::shared_ptr<Mix_Chunk> Resources::GetSound(const std::string file) {
    return GetDecodedChunk(file);
}

void Resources::ClearSounds() {
    for (auto it = soundTable.begin(); it != soundTable.end(); ) {
        if (it->second.unique()) {
            it = soundTable.erase(it);
        } else {
            ++it;
        }
    }
}

//---------------------------------------------------------------------------//
//----------------------------- MUSIC ---------------------------------------//
//---------------------------------------------------------------------------//

std::shared_ptr<Mix_Music> Resources::GetMusic(const std::string file) {
    if (musicTable.find(file) != musicTable.end()) {
        return musicTable[file];
    }

    EnsureMixerDecodersForChunks();

    Mix_Music* music = Mix_LoadMUS(file.c_str());
    if (music == nullptr) {
        std::cerr << "Erro ao carregar música: " << Mix_GetError() << std::endl;
        return nullptr;
    }

    // Deleter customizado para música
    std::shared_ptr<Mix_Music> shared(music, [](Mix_Music* mus) {
        Mix_FreeMusic(mus);
    });

    musicTable.emplace(file, shared);
    return shared;
}

void Resources::ClearMusics() {
    for (auto it = musicTable.begin(); it != musicTable.end(); ) {
        if (it->second.unique()) {
            it = musicTable.erase(it);
        } else {
            ++it;
        }
    }
}

//---------------------------------------------------------------------------//
//----------------------------- FONTES --------------------------------------//
//---------------------------------------------------------------------------//

std::shared_ptr<TTF_Font> Resources::GetFont(std::string file, int fontSize) {
    // Cria uma chave única combinando arquivo e tamanho
    std::string key = file + std::to_string(fontSize);

    if (fontTable.find(key) != fontTable.end()) {
        return fontTable[key];
    }

    // Carrega a fonte
    TTF_Font* font = TTF_OpenFont(file.c_str(), fontSize);
    if (font == nullptr) {
        std::cerr << "Erro ao carregar fonte: " << TTF_GetError() << std::endl;
        return nullptr;
    }

    // Shared Ptr com Deleter customizado
    std::shared_ptr<TTF_Font> shared(font, [](TTF_Font* val) {
        TTF_CloseFont(val);
    });

    fontTable.emplace(key, shared);
    return shared;
}

void Resources::ClearFonts() {
    for (auto it = fontTable.begin(); it != fontTable.end(); ) {
        if (it->second.unique()) {
            it = fontTable.erase(it);
        } else {
            ++it;
        }
    }
}