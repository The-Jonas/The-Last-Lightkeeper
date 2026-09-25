#include "ui/DialogueTuning.h"
#include "nlohmann/json.hpp"
#include <fstream>
#include <iostream>

void DialogueTuning::Load(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return;

    try {
        nlohmann::json j;
        f >> j;

        auto readFloat = [&](const char* key, float& out) {
            if (j.contains(key) && j[key].is_number()) out = j[key].get<float>();
        };
        auto readInt = [&](const char* key, int& out) {
            if (j.contains(key) && j[key].is_number_integer()) out = j[key].get<int>();
        };

        readFloat("chars_per_second", charsPerSecond);
        readFloat("auto_advance_delay", autoAdvanceDelay);
        if (j.contains("max_chars_per_page") && j["max_chars_per_page"].is_number_integer())
            maxCharsPerPage = static_cast<size_t>(j["max_chars_per_page"].get<int>());

        readFloat("box_scale_mul", boxScaleMul);
        readInt("margin", margin);
        readFloat("portrait_height_mul", portraitHeightMul);
        readFloat("portrait_overlap_mul", portraitOverlapMul);
        
        readInt("name_font_size", nameFontSize);
        readInt("text_font_size", textFontSize);

        readInt("name_pad_x", namePadX);
        readInt("name_pad_y", namePadY);
        readInt("text_pad_x", textPadX);
        readInt("text_pad_y", textPadY);

        readInt("name_box_w", nameBoxW);
        readInt("name_box_h", nameBoxH);
        readInt("text_box_w", textBoxW);
        readInt("text_box_h", textBoxH);

        readInt("hint_font_size", hintFontSize);
        readInt("hint_pad_x", hintPadX);
        readInt("hint_pad_y", hintPadY);

        readInt("blip_every_n_chars", blipEveryNChars);
    } catch (const std::exception& ex) {
        std::cerr << path << " ignorado (parse): " << ex.what() << std::endl;
    }
}