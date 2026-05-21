#ifndef STAGE_FIRST_LOAD_DATA_H
#define STAGE_FIRST_LOAD_DATA_H

#include "gameplay/Item.h"

#include <string>
#include <vector>

/// Runtime configuration for `StageState::LoadAssets` (JSON under `Recursos/data/` + embedded fallback).
struct StageFirstLoadData {
    std::string ostPath;
    std::string levelPath;
    float navWorldW = 4358.0f;
    float navWorldH = 3276.0f;
    int navTilePx = 64;
    int itemPickupCount = 35;
    int startingFlashlightDurability = 50;
    ItemDef startingFlashlight;
    std::vector<ItemDef> pickupCycle;
    std::vector<std::string> oceanChunkCandidates;
};

/// Loads `Recursos/data/stage_first_load.json` when valid; otherwise logs and returns embedded defaults (parity).
StageFirstLoadData LoadStageFirstLoadData();

#endif
