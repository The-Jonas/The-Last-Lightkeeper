#ifndef SAVE_MANAGER_H
#define SAVE_MANAGER_H

#include "core/SaveData.h"

class SaveManager {
public:
    static bool HasSave();
    static bool Load(SaveFile& out);
    static bool Save(const SaveFile& data);
    static bool DeleteSave();
    static bool RevertCurrentToCheckpoint();
    static std::string GetSavePath();
};

#endif
