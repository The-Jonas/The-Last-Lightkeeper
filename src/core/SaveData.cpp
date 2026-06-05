#include "core/SaveData.h"

using json = nlohmann::json;

void to_json(json& j, const SavedItemSlot& s) {
    j = json{{"name", s.name}, {"durability", s.durability}};
}

void from_json(const json& j, SavedItemSlot& s) {
    s.name = j.value("name", "");
    s.durability = j.value("durability", 0);
}

void to_json(json& j, const SavedCharacter& c) {
    j = json{{"x", c.x},
             {"y", c.y},
             {"sanity", c.sanity},
             {"isElevated", c.isElevated},
             {"stairAnchorY", c.stairAnchorY}};
}

void from_json(const json& j, SavedCharacter& c) {
    c.x = j.value("x", 0.0f);
    c.y = j.value("y", 0.0f);
    c.sanity = j.value("sanity", 100.0f);
    c.isElevated = j.value("isElevated", false);
    c.stairAnchorY = j.value("stairAnchorY", 0.0f);
}

void to_json(json& j, const SavedDroppedItem& d) {
    j = json{{"name", d.name},
             {"durability", d.durability},
             {"x", d.x},
             {"y", d.y},
             {"heightLevel", d.heightLevel}};
}

void from_json(const json& j, SavedDroppedItem& d) {
    d.name = j.value("name", "");
    d.durability = j.value("durability", 0);
    d.x = j.value("x", 0.0f);
    d.y = j.value("y", 0.0f);
    d.heightLevel = j.value("heightLevel", 0);
}

void to_json(json& j, const SavedBoxPos& b) {
    j = json{{"tiledId", b.tiledId}, {"x", b.x}, {"y", b.y}};
}

void from_json(const json& j, SavedBoxPos& b) {
    b.tiledId = j.value("tiledId", -1);
    b.x = j.value("x", 0.0f);
    b.y = j.value("y", 0.0f);
}

void to_json(json& j, const SavedBackpackGroup& g) {
    j = json{{"groupId", g.groupId}, {"items", g.items}};
}

void from_json(const json& j, SavedBackpackGroup& g) {
    g.groupId = j.value("groupId", "");
    g.items = j.value("items", std::vector<SavedItemSlot>{});
}

static json SerializeOptionalSlot(const std::optional<SavedItemSlot>& slot) {
    if (!slot.has_value()) {
        return nullptr;
    }
    return json(*slot);
}

static std::optional<SavedItemSlot> DeserializeOptionalSlot(const json& j) {
    if (j.is_null()) {
        return std::nullopt;
    }
    SavedItemSlot s;
    from_json(j, s);
    return s;
}

void to_json(json& j, const SaveGameState& s) {
    json slots = json::array();
    for (const auto& slot : s.slots) {
        slots.push_back(SerializeOptionalSlot(slot));
    }

    j = json{{"big", s.big},
             {"small", s.small},
             {"controlled", s.controlled},
             {"partyMode", s.partyMode},
             {"lightOn", s.lightOn},
             {"selectedBackpackGroup", s.selectedBackpackGroup},
             {"backpackGroups", s.backpackGroups},
             {"using", SerializeOptionalSlot(s.usingItem)},
             {"slots", slots},
             {"escadaConsertada", s.escadaConsertada},
             {"removedPickupIds", s.removedPickupIds},
             {"missedUniquePickupIds", s.missedUniquePickupIds},
             {"droppedItems", s.droppedItems},
             {"litCandleIds", s.litCandleIds},
             {"repairedIds", s.repairedIds},
             {"boxPositions", s.boxPositions}};
}

void from_json(const json& j, SaveGameState& s) {
    if (j.contains("big")) {
        from_json(j["big"], s.big);
    }
    if (j.contains("small")) {
        from_json(j["small"], s.small);
    }
    s.controlled = j.value("controlled", "big");
    s.partyMode = j.value("partyMode", "TOGETHER");
    s.lightOn = j.value("lightOn", false);
    s.selectedBackpackGroup = j.value("selectedBackpackGroup", 0);
    s.backpackGroups = j.value("backpackGroups", std::vector<SavedBackpackGroup>{});
    s.usingItem = DeserializeOptionalSlot(j.value("using", json()));
    s.slots.clear();
    if (j.contains("slots") && j["slots"].is_array()) {
        for (const auto& slotJson : j["slots"]) {
            s.slots.push_back(DeserializeOptionalSlot(slotJson));
        }
    }
    s.escadaConsertada = j.value("escadaConsertada", false);
    s.removedPickupIds = j.value("removedPickupIds", std::vector<int>{});
    s.missedUniquePickupIds = j.value("missedUniquePickupIds", std::vector<int>{});
    s.droppedItems = j.value("droppedItems", std::vector<SavedDroppedItem>{});
    s.litCandleIds = j.value("litCandleIds", std::vector<int>{});
    s.repairedIds = j.value("repairedIds", std::vector<int>{});
    s.boxPositions = j.value("boxPositions", std::vector<SavedBoxPos>{});
}

void to_json(json& j, const SaveFile& f) {
    j = json{{"version", f.version},
             {"levelIndex", f.levelIndex},
             {"levelPath", f.levelPath},
             {"levelCheckpoint", f.levelCheckpoint},
             {"current", f.current}};
}

void from_json(const json& j, SaveFile& f) {
    f.version = j.value("version", SaveFile::kVersion);
    f.levelIndex = j.value("levelIndex", 0);
    f.levelPath = j.value("levelPath", "");
    if (j.contains("levelCheckpoint")) {
        from_json(j["levelCheckpoint"], f.levelCheckpoint);
    }
    if (j.contains("current")) {
        from_json(j["current"], f.current);
    }
}
