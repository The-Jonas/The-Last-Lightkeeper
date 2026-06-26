#ifndef SAVE_DATA_H
#define SAVE_DATA_H

#include "nlohmann/json.hpp"

#include <optional>
#include <string>
#include <vector>

struct SavedItemSlot {
    std::string name;
    int durability = 0;
};

struct SavedBackpackGroup {
    std::string groupId;
    std::vector<SavedItemSlot> items;
};

struct SavedCharacter {
    float x = 0.0f;
    float y = 0.0f;
    float sanity = 100.0f;
    bool isElevated = false;
    float stairAnchorY = 0.0f;
};

struct SavedDroppedItem {
    std::string name;
    int durability = 0;
    float x = 0.0f;
    float y = 0.0f;
    int heightLevel = 0;
};

struct SavedBoxPos {
    int tiledId = -1;
    float x = 0.0f;
    float y = 0.0f;
};

struct SaveGameState {
    SavedCharacter big;
    SavedCharacter small;
    std::string controlled = "big";
    std::string partyMode = "TOGETHER";
    bool lightOn = false;
    int selectedSlot = -1;
    std::vector<std::optional<SavedItemSlot>> inventorySlots;
    int selectedBackpackGroup = 0;
    std::vector<SavedBackpackGroup> backpackGroups;
    std::optional<SavedItemSlot> usingItem;
    std::vector<std::optional<SavedItemSlot>> slots;
    bool escadaConsertada = false;
    std::vector<int> removedPickupIds;
    std::vector<int> missedUniquePickupIds;
    std::vector<SavedDroppedItem> droppedItems;
    std::vector<int> litCandleIds;
    std::vector<int> repairedIds;
    std::vector<SavedBoxPos> boxPositions;
};

struct SaveFile {
    static constexpr int kVersion = 1;

    int version = kVersion;
    int levelIndex = 0;
    std::string levelPath;
    SaveGameState levelCheckpoint;
    SaveGameState current;
};

void to_json(nlohmann::json& j, const SavedItemSlot& s);
void from_json(const nlohmann::json& j, SavedItemSlot& s);
void to_json(nlohmann::json& j, const SavedCharacter& c);
void from_json(const nlohmann::json& j, SavedCharacter& c);
void to_json(nlohmann::json& j, const SavedDroppedItem& d);
void from_json(const nlohmann::json& j, SavedDroppedItem& d);
void to_json(nlohmann::json& j, const SavedBoxPos& b);
void from_json(const nlohmann::json& j, SavedBoxPos& b);
void to_json(nlohmann::json& j, const SaveGameState& s);
void from_json(const nlohmann::json& j, SaveGameState& s);
void to_json(nlohmann::json& j, const SaveFile& f);
void from_json(const nlohmann::json& j, SaveFile& f);

#endif
