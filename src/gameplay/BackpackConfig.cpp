#include "gameplay/BackpackConfig.h"

#include <algorithm>

int BackpackConfig::GroupIndexForItem(const std::string& itemName) const {
    for (size_t i = 0; i < groups.size(); ++i) {
        for (const std::string& name : groups[i].itemNames) {
            if (name == itemName) {
                return static_cast<int>(i);
            }
        }
    }
    return -1;
}

int BackpackConfig::GroupIndexForId(const std::string& groupId) const {
    for (size_t i = 0; i < groups.size(); ++i) {
        if (groups[i].id == groupId) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int BackpackConfig::MaxItemsInGroup(int groupIndex) const {
    if (groupIndex < 0 || groupIndex >= static_cast<int>(groups.size())) {
        return 0;
    }
    return std::max(0, groups[static_cast<size_t>(groupIndex)].maxItems);
}

int BackpackConfig::TotalCapacity() const {
    int total = 0;
    for (const BackpackGroupDef& g : groups) {
        total += std::max(0, g.maxItems);
    }
    return total;
}

int BackpackConfig::GroupIndexForSelectKey(int selectKey) const {
    for (size_t i = 0; i < groups.size(); ++i) {
        if (groups[i].selectKey == selectKey) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

const BackpackGroupDef* BackpackConfig::GetGroup(int groupIndex) const {
    if (groupIndex < 0 || groupIndex >= static_cast<int>(groups.size())) {
        return nullptr;
    }
    return &groups[static_cast<size_t>(groupIndex)];
}

BackpackConfig DefaultBackpackConfig() {
    BackpackConfig cfg;
    cfg.groups = {
        {"lighter", 3, 1, {"Flashlight", "Broken Flashlight"}},
        {"fuel", 2, 0, {"Fuel", "Lamp Fuel", "Lighter Fuel", "Light Fuel", "Oil Gallon"}},
        {"lamp", 1, 2, {"Lamp"}},
    };
    return cfg;
}
