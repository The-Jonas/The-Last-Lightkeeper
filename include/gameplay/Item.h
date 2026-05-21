#ifndef ITEM_H
#define ITEM_H

#include <string>
#include <utility>
#include <vector>

enum class ItemProperty { SPEED_BOOST, FUEL, LIGHT_SOURCE, HEALTH };

struct ItemDef {
    std::string name;
    std::string spritePath;
    int maxDurability;
    bool durabilityDecreases;
    int sortOrder;
    std::vector<std::pair<ItemProperty, float>> properties;

    bool HasProperty(ItemProperty p) const {
        for (const auto& pr : properties) {
            if (pr.first == p) {
                return true;
            }
        }
        return false;
    }
};

struct ItemInstance {
    ItemDef def;
    int durability;
};

#endif
