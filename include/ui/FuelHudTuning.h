#ifndef FUEL_HUD_TUNING_H
#define FUEL_HUD_TUNING_H
#include <string>

struct FuelHudTuning {
    static void Load(const std::string& path = "config/fuel_hud.json");

    static inline float frameDuration = 0.15f;  // ~6-7 fps de tremulação — ajuste pra parecer mais real
    static inline float iconScaleMul  = 0.4f;   // multiplicador extra sobre Game::UiScale()

    // Iguais a config/fuel_hud.json: sem o ficheiro o HUD tem de ficar no mesmo
    // sitio, nao encostado ao canto.
    static inline int   marginTop   = 150;     // menor = mais pra cima
    static inline int   marginRight = 150;     // menor = mais pra direita (mais perto da borda)
};
#endif