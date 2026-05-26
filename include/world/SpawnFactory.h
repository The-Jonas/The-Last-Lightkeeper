#pragma once
#include "core/LevelManager.h"
#include "states/stage/FirstLoadData.h"

class GameObject;
class StageState;

class SpawnFactory {
public:
    // Recebe o spawn do Tiled e o StageState (para ter acesso ao cfg e inventory)
    // Retorna nullptr se o tipo não for reconhecido
    static void SpawnEntity(const EntitySpawn& spawn, StageState& stage, const StageFirstLoadData& cfg);
};