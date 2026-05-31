#include "core/SaveManager.h"

#include <SDL2/SDL_filesystem.h>
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <iostream>
#ifdef _WIN32
#include <direct.h>
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

} // namespace

std::string SaveManager::GetSavePath() {
    return ResolveSavePath();
}

bool SaveManager::HasSave() {
    const std::string path = ResolveSavePath();
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    try {
        nlohmann::json j;
        file >> j;
        return j.contains("current") && j.contains("levelCheckpoint");
    } catch (...) {
        return false;
    }
}

bool SaveManager::Load(SaveFile& out) {
    const std::string path = ResolveSavePath();
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    try {
        nlohmann::json j;
        file >> j;
        from_json(j, out);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[SaveManager] Failed to load save: " << e.what() << std::endl;
        return false;
    }
}

bool SaveManager::Save(const SaveFile& data) {
    const std::string path = ResolveSavePath();
    if (!EnsureParentDir(path)) {
        std::cerr << "[SaveManager] Could not create save directory for " << path << std::endl;
    }
    try {
        nlohmann::json j;
        to_json(j, data);
        std::ofstream file(path, std::ios::trunc);
        if (!file.is_open()) {
            std::cerr << "[SaveManager] Could not open save file for writing: " << path << std::endl;
            return false;
        }
        file << j.dump(2);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[SaveManager] Failed to write save: " << e.what() << std::endl;
        return false;
    }
}

bool SaveManager::DeleteSave() {
    const std::string path = ResolveSavePath();
    if (std::remove(path.c_str()) != 0) {
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
