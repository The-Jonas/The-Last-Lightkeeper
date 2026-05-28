#include "states/stage/FirstLoadData.h"
#include "gameplay/Item.h"

#include "nlohmann/json.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unordered_map>

namespace {

using json = nlohmann::json;

ItemProperty ParseItemProperty(const std::string& name) {
    static const std::unordered_map<std::string, ItemProperty> kMap = {
        {"SPEED_BOOST", ItemProperty::SPEED_BOOST},
        {"FUEL", ItemProperty::FUEL},
        {"LIGHT_SOURCE", ItemProperty::LIGHT_SOURCE},
        {"HEALTH", ItemProperty::HEALTH},
    };
    const auto it = kMap.find(name);
    if (it == kMap.end()) {
        throw std::runtime_error("unknown ItemProperty: " + name);
    }
    return it->second;
}

ItemDef ParseItemDef(const json& j) {
    ItemDef def;
    def.name = j.at("name").get<std::string>();
    def.spritePath = j.at("spritePath").get<std::string>();
    def.maxDurability = j.at("maxDurability").get<int>();
    def.durabilityDecreases = j.at("durabilityDecreases").get<bool>();
    def.sortOrder = j.at("sortOrder").get<int>();
    def.properties.clear();
    if (j.contains("properties") && j["properties"].is_array()) {
        for (const auto& pr : j["properties"]) {
            const std::string propName = pr.at("prop").get<std::string>();
            const float val = pr.at("value").get<float>();
            def.properties.emplace_back(ParseItemProperty(propName), val);
        }
    }
    return def;
}

StageFirstLoadData EmbeddedDefaults() {
    StageFirstLoadData d;
    d.ostPath = "Recursos/audio/soundtracks/Last Hideout.mp3";
    d.levelPath = "Recursos/map/mapa_1_andar.json";
    d.navWorldW = 4358.0f;
    d.navWorldH = 3276.0f;
    d.navTilePx = 64;
    d.itemPickupCount = 35;
    d.startingFlashlightDurability = 50;

    ItemDef apple{"Apple", "Recursos/img/items/apple.png", -1, false, 1, {}};
    ItemDef brokenFlashlight{"Broken Flashlight",
                             "Recursos/img/items/Isqueiro.png",
                             100,
                             true,
                             2,
                             {{ItemProperty::LIGHT_SOURCE, 1.0f}}};
    ItemDef oil{"Oil Gallon", "Recursos/img/items/oil_gallon.png", 100, false, 3, {}};
    d.pickupCycle = {apple, brokenFlashlight, oil};

    d.startingFlashlight =
        ItemDef{"Flashlight",
                "Recursos/img/items/Isqueiro.png",
                100,
                true,
                0,
                {{ItemProperty::LIGHT_SOURCE, 1.0f}}};

    d.oceanChunkCandidates = {"Recursos/audio/waves.ogg", "Recursos/audio/waves.wav", "Recursos/audio/waves.mp3"};
    return d;
}

bool TryReadJsonFile(const char* path, json& out) {
    std::ifstream f(path);
    if (!f.is_open()) {
        return false;
    }
    try {
        f >> out;
        return true;
    } catch (const std::exception& ex) {
        std::cerr << "stage_first_load: falha ao ler JSON (" << path << "): " << ex.what() << std::endl;
        return false;
    }
}

static const char kEmbeddedStageFirstLoadJson[] = R"json({
  "ostPath": "Recursos/audio/soundtracks/Last Hideout.mp3",
  "levelPath": "Recursos/map/mapa_1_andar.json",
  "navWorld": { "w": 4358, "h": 3276 },
  "navTilePx": 64,
  "itemPickupCount": 35,
  "startingFlashlightDurability": 50,
  "startingFlashlight": {
    "name": "Flashlight",
    "spritePath": "Recursos/img/items/Isqueiro.png",
    "maxDurability": 100,
    "durabilityDecreases": true,
    "sortOrder": 0,
    "properties": [{ "prop": "LIGHT_SOURCE", "value": 1.0 }]
  },
  "pickupCycle": [
    {
      "name": "Apple",
      "spritePath": "Recursos/img/items/apple.png",
      "maxDurability": -1,
      "durabilityDecreases": false,
      "sortOrder": 1,
      "properties": []
    },
    {
      "name": "Broken Flashlight",
      "spritePath": "Recursos/img/items/Isqueiro.png",
      "maxDurability": 100,
      "durabilityDecreases": true,
      "sortOrder": 2,
      "properties": [{ "prop": "LIGHT_SOURCE", "value": 1.0 }]
    },
    {
      "name": "Oil Gallon",
      "spritePath": "Recursos/img/items/oil_gallon.png",
      "maxDurability": 100,
      "durabilityDecreases": false,
      "sortOrder": 3,
      "properties": []
    }
  ],
  "oceanChunkCandidates": [
    "Recursos/audio/waves.ogg",
    "Recursos/audio/waves.wav",
    "Recursos/audio/waves.mp3"
  ]
})json";

StageFirstLoadData ParseFromJsonRoot(const json& j) {
    StageFirstLoadData d = EmbeddedDefaults();

    if (j.contains("ostPath")) {
        d.ostPath = j.at("ostPath").get<std::string>();
    }
    if (j.contains("levelPath")) {
        d.levelPath = j.at("levelPath").get<std::string>();
    }
    if (j.contains("navWorld") && j["navWorld"].is_object()) {
        const auto& nw = j["navWorld"];
        if (nw.contains("w")) {
            d.navWorldW = nw.at("w").get<float>();
        }
        if (nw.contains("h")) {
            d.navWorldH = nw.at("h").get<float>();
        }
    }
    if (j.contains("navTilePx")) {
        d.navTilePx = j.at("navTilePx").get<int>();
    }
    if (j.contains("itemPickupCount")) {
        d.itemPickupCount = j.at("itemPickupCount").get<int>();
    }
    if (j.contains("startingFlashlightDurability")) {
        d.startingFlashlightDurability = j.at("startingFlashlightDurability").get<int>();
    }
    if (j.contains("startingFlashlight")) {
        d.startingFlashlight = ParseItemDef(j.at("startingFlashlight"));
    }
    if (j.contains("pickupCycle") && j["pickupCycle"].is_array()) {
        std::vector<ItemDef> cycle;
        for (const auto& it : j["pickupCycle"]) {
            cycle.push_back(ParseItemDef(it));
        }
        if (!cycle.empty()) {
            d.pickupCycle = std::move(cycle);
        }
    }
    if (j.contains("oceanChunkCandidates") && j["oceanChunkCandidates"].is_array()) {
        std::vector<std::string> paths;
        for (const auto& p : j["oceanChunkCandidates"]) {
            paths.push_back(p.get<std::string>());
        }
        if (!paths.empty()) {
            d.oceanChunkCandidates = std::move(paths);
        }
    }

    return d;
}

StageFirstLoadData SanitizeLists(StageFirstLoadData d) {
    if (d.pickupCycle.empty()) {
        d.pickupCycle = EmbeddedDefaults().pickupCycle;
    }
    if (d.oceanChunkCandidates.empty()) {
        d.oceanChunkCandidates = EmbeddedDefaults().oceanChunkCandidates;
    }
    return d;
}

StageFirstLoadData LoadStageFirstLoadDataDispatch() {
    json j;
    const char* path = "Recursos/data/stage_first_load.json";
    if (!TryReadJsonFile(path, j)) {
        try {
            j = json::parse(kEmbeddedStageFirstLoadJson);
        } catch (const std::exception& ex) {
            std::cerr << "stage_first_load: fallback embutido invalido (!): " << ex.what() << std::endl;
            return SanitizeLists(EmbeddedDefaults());
        }
        return SanitizeLists(ParseFromJsonRoot(j));
    }

    try {
        return SanitizeLists(ParseFromJsonRoot(j));
    } catch (const std::exception& ex) {
        std::cerr << "stage_first_load: erro ao aplicar \"" << path << "\" — usando fallback embutido: " << ex.what()
                  << std::endl;
        try {
            json fb = json::parse(kEmbeddedStageFirstLoadJson);
            return SanitizeLists(ParseFromJsonRoot(fb));
        } catch (const std::exception& ex2) {
            std::cerr << "stage_first_load: fallback embutido falhou: " << ex2.what() << std::endl;
            return SanitizeLists(EmbeddedDefaults());
        }
    }
}

} // namespace

StageFirstLoadData LoadStageFirstLoadData() {
    return LoadStageFirstLoadDataDispatch();
}
