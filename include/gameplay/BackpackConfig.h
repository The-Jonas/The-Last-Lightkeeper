#ifndef BACKPACK_CONFIG_H
#define BACKPACK_CONFIG_H

#include <string>
#include <vector>

struct BackpackGroupDef {
    std::string id;
    int maxItems = 1;
    int selectKey = 0;
    std::vector<std::string> itemNames;
};

struct BackpackConfig {
    std::vector<BackpackGroupDef> groups;

    int GroupIndexForItem(const std::string& itemName) const;
    int GroupIndexForId(const std::string& groupId) const;
    int MaxItemsInGroup(int groupIndex) const;
    int TotalCapacity() const;
    int GroupIndexForSelectKey(int selectKey) const;
    const BackpackGroupDef* GetGroup(int groupIndex) const;
};

BackpackConfig DefaultBackpackConfig();

#endif
