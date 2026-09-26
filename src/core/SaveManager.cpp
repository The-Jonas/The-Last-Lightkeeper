#include "core/SaveManager.h"

#include <SDL2/SDL_filesystem.h>
#include <SDL2/SDL_rwops.h>
#include <SDL2/SDL_stdinc.h>
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <iostream>
#ifdef _WIN32
#include <direct.h>
#define WIN32_LEAN_AND_MEAN   // corta o que não usamos do windows.h
#ifndef NOMINMAX
#define NOMINMAX              // impede o windows.h de criar macros min/max (quebram std::min/std::max)
#endif
#include <windows.h>          // DeleteFileW
#else
#include <sys/stat.h>
#endif

namespace {

std::string ResolveSavePath() {
    char* prefPath = SDL_GetPrefPath("UnB", "A-Luz-do-Farol");
    if (prefPath) {
        std::string path = std::string(prefPath) + "save.json";
        SDL_free(prefPath);
        return path;
    }
    return "Recursos/saves/save.json";
}

// Só para o caminho reserva (Recursos/saves), que é ASCII. A pasta do
// SDL_GetPrefPath o próprio SDL já cria, com suporte a acento.
bool EnsureParentDir(const std::string& filePath) {
    const size_t slash = filePath.find_last_of("/\\");
    if (slash == std::string::npos) {
        return true;
    }
    std::string dir = filePath.substr(0, slash);
#ifdef _WIN32
    return _mkdir(dir.c_str()) == 0 || errno == EEXIST;
#else
    return mkdir(dir.c_str(), 0755) == 0 || errno == EEXIST;
#endif
}

// Lê o arquivo inteiro. Usa SDL_RWops porque ele entende caminho UTF-8 no
// Windows (usuário com acento); std::ifstream com char* não entende.
bool ReadWholeFileUtf8(const std::string& path, std::string& out) {
    SDL_RWops* rw = SDL_RWFromFile(path.c_str(), "rb");
    if (!rw) return false;
    const Sint64 size = SDL_RWsize(rw);
    if (size < 0) { SDL_RWclose(rw); return false; }
    out.resize(static_cast<size_t>(size));
    const size_t got = size > 0 ? SDL_RWread(rw, &out[0], 1, static_cast<size_t>(size)) : 0;
    SDL_RWclose(rw);
    return got == static_cast<size_t>(size);
}

// Grava o texto inteiro (substitui o arquivo). Mesmo motivo do Read acima.
bool WriteWholeFileUtf8(const std::string& path, const std::string& text) {
    SDL_RWops* rw = SDL_RWFromFile(path.c_str(), "wb");
    if (!rw) return false;
    const size_t wrote = SDL_RWwrite(rw, text.data(), 1, text.size());
    SDL_RWclose(rw);
    return wrote == text.size();
}

// Apaga um arquivo com caminho UTF-8. No Windows converte para UTF-16 e usa a
// API do próprio Windows (DeleteFileW), que aceita acento.
bool RemoveFileUtf8(const std::string& path) {
#ifdef _WIN32
    wchar_t* wide = reinterpret_cast<wchar_t*>(
        SDL_iconv_string("UTF-16LE", "UTF-8", path.c_str(), path.size() + 1));
    if (!wide) return false;
    const bool ok = DeleteFileW(wide) != 0;   // DeleteFileW devolve diferente de 0 em caso de sucesso
    SDL_free(wide);
    return ok;
#else
    return std::remove(path.c_str()) == 0;
#endif
}

} // namespace

std::string SaveManager::GetSavePath() {
    return ResolveSavePath();
}

bool SaveManager::HasSave() {
    std::string text;
    if (!ReadWholeFileUtf8(ResolveSavePath(), text)) {
        return false;
    }
    try {
        const nlohmann::json j = nlohmann::json::parse(text);
        return j.contains("current") && j.contains("levelCheckpoint");
    } catch (...) {
        return false;
    }
}

bool SaveManager::Load(SaveFile& out) {
    std::string text;
    if (!ReadWholeFileUtf8(ResolveSavePath(), text)) {
        return false;
    }
    try {
        const nlohmann::json j = nlohmann::json::parse(text);
        from_json(j, out);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[SaveManager] Failed to load save: " << e.what() << std::endl;
        return false;
    }
}

bool SaveManager::Save(const SaveFile& data) {
    const std::string path = ResolveSavePath();
    // Só o caminho reserva precisa criar pasta; a do SDL_GetPrefPath já existe.
    if (path.rfind("Recursos/", 0) == 0 && !EnsureParentDir(path)) {
        std::cerr << "[SaveManager] Could not create save directory for " << path << std::endl;
    }
    try {
        nlohmann::json j;
        to_json(j, data);
        if (!WriteWholeFileUtf8(path, j.dump(2))) {
            std::cerr << "[SaveManager] Could not open save file for writing: " << path
                      << " (" << SDL_GetError() << ")" << std::endl;
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[SaveManager] Failed to write save: " << e.what() << std::endl;
        return false;
    }
}

bool SaveManager::DeleteSave() {
    if (!RemoveFileUtf8(ResolveSavePath())) {
        return !HasSave();
    }
    return true;
}

bool SaveManager::RevertCurrentToCheckpoint() {
    SaveFile file;
    if (!Load(file)) {
        return false;
    }
    file.current = file.levelCheckpoint;
    return Save(file);
}
