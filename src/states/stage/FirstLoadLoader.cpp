#include "states/stage/FirstLoadData.h"
#include "gameplay/BackpackConfig.h"
#include "gameplay/Item.h"

#include "nlohmann/json.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <algorithm>

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
    d.ostPath = "Recursos/audio/soundtracks/Ambientacao.mp3";
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
    ItemDef oil{"Oil Gallon", "Recursos/img/items/oil_gallon.png", 1, false, 3, {{ItemProperty::FUEL, 100.0f}}};
    ItemDef lamp{"Lamp",
                 "Recursos/img/items/0106_ASSET_LAMPARINA_PERSPECTIVA_APAGADA_FLV.png.png",
                 250,
                 true,
                 4,
                 {{ItemProperty::LIGHT_SOURCE, 1.0f}}};
    d.pickupCycle = {apple, brokenFlashlight, oil, lamp};

    d.startingFlashlight =
        ItemDef{"Flashlight",
                "Recursos/img/items/Isqueiro.png",
                100,
                true,
                0,
                {{ItemProperty::LIGHT_SOURCE, 1.0f}}};

    d.oceanChunkCandidates = {};
    d.levels = {
        {"level_1", "Recursos/map/mapa_1_andar.json", 1},
        {"level_2", "Recursos/map/mapa_2_andar.json", 2},
    };
    d.backpackConfig = DefaultBackpackConfig();
    return d;
}

BackpackConfig ParseBackpackConfig(const json& j) {
    BackpackConfig cfg = DefaultBackpackConfig();
    if (!j.is_array()) {
        return cfg;
    }
    std::vector<BackpackGroupDef> groups;
    for (const auto& g : j) {
        BackpackGroupDef def;
        def.id = g.value("id", "");
        def.maxItems = g.value("maxItems", 1);
        def.selectKey = g.value("selectKey", 0);
        if (g.contains("itemNames") && g["itemNames"].is_array()) {
            for (const auto& name : g["itemNames"]) {
                def.itemNames.push_back(name.get<std::string>());
            }
        }
        if (!def.id.empty()) {
            groups.push_back(std::move(def));
        }
    }
    if (!groups.empty()) {
        cfg.groups = std::move(groups);
    }
    return cfg;
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
    if (j.contains("backpackGroups") && j["backpackGroups"].is_array()) {
        d.backpackConfig = ParseBackpackConfig(j["backpackGroups"]);
    }
    if (j.contains("levels") && j["levels"].is_array()) {
        std::vector<LevelDef> levels;
        for (const auto& lv : j["levels"]) {
            LevelDef def;
            def.label = lv.value("label", "");
            def.mapPath = lv.value("mapPath", "");
            def.displayNumber = lv.value("displayNumber", 1);
            if (!def.mapPath.empty()) {
                levels.push_back(std::move(def));
            }
        }
        if (!levels.empty()) {
            d.levels = std::move(levels);
            d.levelPath = d.levels.front().mapPath;
        }
    }

    return d;
}

StageFirstLoadData SanitizeLists(StageFirstLoadData d) {
    if (d.pickupCycle.empty()) {
        d.pickupCycle = EmbeddedDefaults().pickupCycle;
    }
    if (d.levels.empty()) {
        d.levels = EmbeddedDefaults().levels;
    }
    if (d.levelPath.empty() && !d.levels.empty()) {
        d.levelPath = d.levels.front().mapPath;
    }
    if (d.backpackConfig.groups.empty()) {
        d.backpackConfig = DefaultBackpackConfig();
    }
    return d;
}

StageFirstLoadData LoadStageFirstLoadDataDispatch() {
    json j;
    const char* path = "Recursos/data/stage_first_load.json";

    // TENTA LER O ARQUIVO EXTERNO

    if (TryReadJsonFile(path, j)) {
        try {
            return SanitizeLists(ParseFromJsonRoot(j));
        } catch (const std::exception& ex) {
            std::cerr << "stage_first_load: Erro grave ao interpretar as chaves do JSON externo: " << ex.what() << std::endl;
            std::cerr << ">> Usando o esqueleto padrão do C++ como salva-vidas." << std::endl;
        }
        return SanitizeLists(ParseFromJsonRoot(j));
    }
    else {
        std::cerr << "stage_first_load: Arquivo '" << path << "' nao encontrado." << std::endl;
        std::cerr << ">> Usando o esqueleto padrão do C++ como salva-vidas." << std::endl;
    }

    // SE NÃO ACHOU O ARQUIVO OU DEU ERRO GRAVE, USA OS DEFAULTS
    return SanitizeLists(EmbeddedDefaults());

    
}

} // namespace

StageFirstLoadData LoadStageFirstLoadData() {
    return LoadStageFirstLoadDataDispatch();
}

int GetLevelCount(const StageFirstLoadData& cfg) {
    if (!cfg.levels.empty()) {
        return static_cast<int>(cfg.levels.size());
    }
    return 1;
}

const LevelDef& GetLevelDef(const StageFirstLoadData& cfg, int index) {
    static LevelDef fallback;
    if (!cfg.levels.empty()) {
        const int clamped = std::max(0, std::min(index, static_cast<int>(cfg.levels.size()) - 1));
        return cfg.levels[static_cast<size_t>(clamped)];
    }
    fallback.label = "level_1";
    fallback.mapPath = cfg.levelPath;
    fallback.displayNumber = index + 1;
    return fallback;
}
