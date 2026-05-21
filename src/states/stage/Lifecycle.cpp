#include "states/stage/StageState.h"
#include "states/stage/InternalHelpers.h"
#include "core/Game.h"
#include "engine/GameObject.h"
#include "engine/SpriteRenderer.h"
#include "math/Rect.h"
#include "world/TileSet.h"
#include "world/TileMap.h"
#include "core/InputManager.h"
#include "engine/Camera.h"
#include "engine/Component.h"
#include "gameplay/Character.h"
#include "world/Collider.h"
#include "world/Collision.h"
#include "core/GameData.h"
#include "states/EndState.h"
#include "ui/Text.h"
#include "lighting/TopDownLightShadows.h"
#include "lighting/LightShadowProfile.h"
#include "gameplay/Item.h"
#include "gameplay/ItemPickup.h"
#include "gameplay/HotbarComponent.h"
#include "gameplay/Box.h"
#include "ui/FadeEffect.h"
#include "gameplay/Repairable.h"
#include "gameplay/StairTrigger.h"
#include "core/Resources.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <array>
#include <queue>
#include <limits>
#include <unordered_map>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace stage_internal;
void StageState::Start() {
    if (!levelContentLoaded) {
        LoadAssets();
    }
    StartArray();                                                   // Chama Start() de todos os objetos
    SetMouseConfinedToWindow(true);
    // OST só após o nível estar carregado e empilhado (não durante o loading screen).
    if (music.IsOpen()) {
        const int ostVol = (MIX_MAX_VOLUME * Game::masterVolumePercent) / 100;
        Mix_VolumeMusic(musicMuted ? 0 : ostVol);
        if (!musicMuted) {
            music.Play(-1);
        }
    }
    started = true;
}

void StageState::Pause() {
    SetMouseConfinedToWindow(false);
}

void StageState::Resume() {
    SetMouseConfinedToWindow(true);
}
