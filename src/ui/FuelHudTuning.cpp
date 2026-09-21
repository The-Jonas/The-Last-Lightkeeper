#include "ui/FuelHudTuning.h"
#include "nlohmann/json.hpp"
#include <fstream>
#include <iostream>

void FuelHudTuning::Load(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return;
    try {
        nlohmann::json j;
        f >> j;
        if (j.contains("frame_duration") && j["frame_duration"].is_number()) frameDuration = j["frame_duration"].get<float>();
        if (j.contains("icon_scale_mul") && j["icon_scale_mul"].is_number()) iconScaleMul = j["icon_scale_mul"].get<float>();
        if (j.contains("margin_top") && j["margin_top"].is_number_integer()) marginTop = j["margin_top"].get<int>();
        if (j.contains("margin_right") && j["margin_right"].is_number_integer()) marginRight = j["margin_right"].get<int>();
    } catch (const std::exception& ex) {
        std::cerr << path << " ignorado (parse): " << ex.what() << std::endl;
    }
}