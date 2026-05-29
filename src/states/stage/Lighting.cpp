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
Vec2 StageState::ScreenToWorld(const Vec2& screenPos) const {
    const float z = Camera::GetZoom();
    if (z <= 1e-5f) {
        return Vec2(Camera::pos.x, Camera::pos.y);
    }
    return Vec2(screenPos.x / z + Camera::pos.x, screenPos.y / z + Camera::pos.y);
}

Vec2 StageState::WorldToScreen(const Vec2& worldPos) const {
    const float z = Camera::GetZoom();
    return Vec2((worldPos.x - Camera::pos.x) * z, (worldPos.y - Camera::pos.y) * z);
}

void StageState::CreateLightAtCursor() {
    if (lightMaskShape != LightMaskShape::Circle && lightMaskShape != LightMaskShape::Torch) {
        return;
    }
    InputManager& input = InputManager::GetInstance();
    LightInstance light;
    light.worldPos = ScreenToWorld(Vec2(static_cast<float>(input.GetMouseX()), static_cast<float>(input.GetMouseY())));
    if (tileMapComp && tileSet) {
        const int tileW = std::max(1, tileSet->GetTileWidth());
        const int tileH = std::max(1, tileSet->GetTileHeight());
        const int tx = static_cast<int>((light.worldPos.x - mapOrigin.x) / static_cast<float>(tileW));
        const int ty = static_cast<int>((light.worldPos.y - mapOrigin.y) / static_cast<float>(tileH));
        const auto& solid = tileMapComp->GetLightOcclusionSolid();
        if (!solid.empty() && tx >= 0 && ty >= 0 && tx < tileMapComp->GetWidth() && ty < tileMapComp->GetHeight()) {
            const size_t idx = static_cast<size_t>(tx + ty * tileMapComp->GetWidth());
            if (idx < solid.size() && solid[idx] != 0) {
                return; // do not place lights inside occluding cells
            }
        }
    }
    light.shape = lightMaskShape;
    light.params = lightMaskParams;
    light.enabled = true;

    // Prevent infinite oversaturation from stacking many lights in the same place:
    // if there is already a nearby light, refresh that one instead of adding another.
    const float overlapLimitWorld = std::max(24.0f, lightMaskParams.falloffRadiusPx * 0.18f);
    for (LightInstance& existing : lights) {
        if (existing.worldPos.Distance(light.worldPos) <= overlapLimitWorld) {
            existing.shape = light.shape;
            existing.params = light.params;
            existing.enabled = true;
            existing.worldPos = light.worldPos;
            return;
        }
    }

    static std::uint32_t sSeedCounter = 1u;
    sSeedCounter = sSeedCounter * 1664525u + 1013904223u;
    light.animationSeed = static_cast<float>(sSeedCounter & 0xFFFFu) / 65535.0f;
    lights.push_back(light);
    if (static_cast<int>(lights.size()) > maxActiveLights * 2) {
        lights.erase(lights.begin(), lights.begin() + (lights.size() - static_cast<size_t>(maxActiveLights * 2)));
    }
}

int StageState::CreateStaticLight(Vec2 pos, bool startsLit) {
    LightInstance light;
    light.worldPos = pos;
    light.shape = lightMaskShape;   // Usa o formato base da fase
    light.params = lightMaskParams; // Usa as cores/sombras base
    
    // Opcional: Se quiser que a luz do castiçal seja um pouco menor que a do poste:
    // light.params.falloffRadiusPx = 300.0f; 
    
    light.enabled = startsLit;
    
    // Semente aleatória para o fogo "tremer" de forma independente
    static std::uint32_t sSeed = 100;
    sSeed = sSeed * 1664525u + 1013904223u;
    light.animationSeed = static_cast<float>(sSeed & 0xFFFFu) / 65535.0f;
    
    lights.push_back(light);
    return lights.size() - 1; // Retorna o ID (posição no vetor)
}

void StageState::SetLightEnabled(int lightId, bool enabled) {
    if (lightId >= 0 && lightId < lights.size()) {
        lights[lightId].enabled = enabled;
    }
}
