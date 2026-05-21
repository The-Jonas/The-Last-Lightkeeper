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
bool StageState::IsBoxWalkableOnMapLayer(const Rect& box) const {
    if (!tileMapComp || !tileSet) {
        return true;
    }

    const int tileW = std::max(1, tileSet->GetTileWidth());
    const int tileH = std::max(1, tileSet->GetTileHeight());
    const int mapW = tileMapComp->GetWidth();
    const int mapH = tileMapComp->GetHeight();
    if (mapW <= 0 || mapH <= 0) {
        return true;
    }

    // Use a "footprint" near character feet for top-down walkability checks.
    const float inset = std::max(2.0f, std::min(box.w * 0.25f, 8.0f));
    const float left = box.x + inset;
    const float right = box.x + box.w - inset;
    const float footY = box.y + box.h - 2.0f;
    const float kneeY = box.y + box.h * 0.72f;

    const std::array<Vec2, 4> samplePoints = {
        Vec2(left, footY),
        Vec2(right, footY),
        Vec2(left, kneeY),
        Vec2(right, kneeY),
    };

    for (const Vec2& p : samplePoints) {
        const int tx = static_cast<int>((p.x - mapOrigin.x) / static_cast<float>(tileW));
        const int ty = static_cast<int>((p.y - mapOrigin.y) / static_cast<float>(tileH));
        if (tx < 0 || ty < 0 || tx >= mapW || ty >= mapH) {
            return false;
        }
        const int t = tileMapComp->At(tx, ty, 1);
        if (walkableTileIds.find(t) == walkableTileIds.end()) {
            return false;
        }
    }
    return true;
}

bool StageState::HasNavigationGrid() const {
    return (tileMapComp != nullptr && tileSet != nullptr)
        || (navGridWidthTiles > 0 && navGridHeightTiles > 0);
}

int StageState::NavTileWidthPx() const {
    if (tileMapComp != nullptr && tileSet != nullptr) {
        return tileSet->GetTileWidth();
    }
    return navTilePx;
}

int StageState::NavTileHeightPx() const {
    if (tileMapComp != nullptr && tileSet != nullptr) {
        return tileSet->GetTileHeight();
    }
    return navTilePx;
}

bool StageState::IsTileWalkable(int tx, int ty) const {
    if (tileMapComp != nullptr && tileSet != nullptr) {
        if (tx < 0 || ty < 0 || tx >= tileMapComp->GetWidth() || ty >= tileMapComp->GetHeight()) {
            return false;
        }
        const int tileId = tileMapComp->At(tx, ty, 1);
        return walkableTileIds.find(tileId) != walkableTileIds.end();
    }
    if (navGridWidthTiles > 0 && navGridHeightTiles > 0) {
        return tx >= 0 && ty >= 0 && tx < navGridWidthTiles && ty < navGridHeightTiles;
    }
    return true;
}

// Não conecta andares: usa o `isElevated` atual do agente (mesmo grafo que `Character` no chão/escada atual).
bool StageState::IsTileNavigableFor(const GameObject* agent, int tx, int ty) const {
    if (!agent || !IsTileWalkable(tx, ty)) {
        return false;
    }

    const Vec2 tileCenter = TileCenterToWorld(tx, ty);
    Rect probe;
    probe.w = agent->box.w;
    probe.h = agent->box.h;
    probe.x = tileCenter.x - probe.w * 0.5f;
    probe.y = tileCenter.y - probe.h * 0.5f;

    const int r = std::max(1, static_cast<int>(probe.w * 0.25f));
    Circle footCircle;
    footCircle.radius = r;
    footCircle.center.x = static_cast<int>(probe.x + probe.w * 0.5f);
    footCircle.center.y = static_cast<int>(probe.y + probe.h - r);

    GameObject* agentMut = const_cast<GameObject*>(agent);
    const Character* agentChar = agentMut->GetComponent<Character>();
    const bool elevated = agentChar ? agentChar->isElevated : false;
    if (const_cast<LevelManager&>(level).CheckCollision(footCircle, elevated)) {
        return false;
    }

    const float fr = static_cast<float>(r);
    Rect footRect(probe.x + probe.w * 0.5f - fr, probe.y + probe.h - 2.0f * fr, 2.0f * fr, 2.0f * fr);

    for (const auto& goPtr : objectArray) {
        GameObject* go = goPtr.get();
        if (!go || go == agent || go == bigCharacterObject || go == smallCharacterObject) {
            continue;
        }
        Collider* col = go->GetComponent<Collider>();
        if (!col) {
            continue;
        }
        Rect a = footRect;
        Rect b = col->box;
        const float angleRad = static_cast<float>(go->angleDeg) * (static_cast<float>(M_PI) / 180.0f);
        if (Collision::IsColliding(a, b, 0.0f, angleRad)) {
            return false;
        }
    }
    return true;
}

Vec2 StageState::TileCenterToWorld(int tx, int ty) const {
    const float tileW = static_cast<float>(std::max(1, NavTileWidthPx()));
    const float tileH = static_cast<float>(std::max(1, NavTileHeightPx()));
    return Vec2(mapOrigin.x + (static_cast<float>(tx) + 0.5f) * tileW,
                mapOrigin.y + (static_cast<float>(ty) + 0.5f) * tileH);
}

Vec2 StageState::ClampPickupTopLeft(Vec2 topLeft, float itemW, float itemH) const {
    if (levelWorldW <= 1.0f || levelWorldH <= 1.0f) {
        return topLeft;
    }
    const float minX = mapOrigin.x;
    const float minY = mapOrigin.y;
    const float maxX = mapOrigin.x + levelWorldW - itemW;
    const float maxY = mapOrigin.y + levelWorldH - itemH;
    return Vec2(std::clamp(topLeft.x, minX, maxX), std::clamp(topLeft.y, minY, maxY));
}

bool StageState::WorldToTile(const Vec2& worldPos, int& outTx, int& outTy) const {
    if (tileMapComp != nullptr && tileSet != nullptr) {
        const float tileW = static_cast<float>(std::max(1, tileSet->GetTileWidth()));
        const float tileH = static_cast<float>(std::max(1, tileSet->GetTileHeight()));
        outTx = static_cast<int>((worldPos.x - mapOrigin.x) / tileW);
        outTy = static_cast<int>((worldPos.y - mapOrigin.y) / tileH);
        return (outTx >= 0 && outTy >= 0 && outTx < tileMapComp->GetWidth() && outTy < tileMapComp->GetHeight());
    }
    if (navGridWidthTiles > 0 && navGridHeightTiles > 0) {
        const float tileW = static_cast<float>(std::max(1, NavTileWidthPx()));
        const float tileH = static_cast<float>(std::max(1, NavTileHeightPx()));
        outTx = static_cast<int>((worldPos.x - mapOrigin.x) / tileW);
        outTy = static_cast<int>((worldPos.y - mapOrigin.y) / tileH);
        return (outTx >= 0 && outTy >= 0 && outTx < navGridWidthTiles && outTy < navGridHeightTiles);
    }
    return false;
}

bool StageState::FindNearestWalkableTile(int startTx, int startTy, int& outTx, int& outTy, int maxRadius, const GameObject* agent) const {
    if (!HasNavigationGrid()) {
        return false;
    }
    auto passable = [&](int x, int y) {
        if (agent != nullptr) {
            return IsTileNavigableFor(agent, x, y);
        }
        return IsTileWalkable(x, y);
    };
    if (passable(startTx, startTy)) {
        outTx = startTx;
        outTy = startTy;
        return true;
    }

    const int w = tileMapComp ? tileMapComp->GetWidth() : navGridWidthTiles;
    const int h = tileMapComp ? tileMapComp->GetHeight() : navGridHeightTiles;
    for (int r = 1; r <= maxRadius; ++r) {
        const int minX = std::max(0, startTx - r);
        const int maxX = std::min(w - 1, startTx + r);
        const int minY = std::max(0, startTy - r);
        const int maxY = std::min(h - 1, startTy + r);

        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                if (std::abs(x - startTx) != r && std::abs(y - startTy) != r) {
                    continue;
                }
                if (passable(x, y)) {
                    outTx = x;
                    outTy = y;
                    return true;
                }
            }
        }
    }
    return false;
}

bool StageState::HasWalkableLine(const Vec2& fromWorld, const Vec2& toWorld) const {
    return HasWalkableLine(fromWorld, toWorld, nullptr);
}

bool StageState::HasWalkableLine(const Vec2& fromWorld, const Vec2& toWorld, const GameObject* agent) const {
    if (agent == nullptr) {
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        if (!WorldToTile(fromWorld, x0, y0) || !WorldToTile(toWorld, x1, y1)) {
            return false;
        }

        int dx = std::abs(x1 - x0);
        int sx = (x0 < x1) ? 1 : -1;
        int dy = -std::abs(y1 - y0);
        int sy = (y0 < y1) ? 1 : -1;
        int err = dx + dy;

        while (true) {
            if (!IsTileWalkable(x0, y0)) {
                return false;
            }
            if (x0 == x1 && y0 == y1) {
                break;
            }
            const int e2 = err * 2;
            if (e2 >= dy) {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y0 += sy;
            }
        }
        return true;
    }

    // Com agente: amostra o segmento em world — Bresenham em tiles pode ignorar paredes finas entre centros.
    const float dist = fromWorld.Distance(toWorld);
    const float halfTile = static_cast<float>(std::max(1, NavTileWidthPx())) * 0.5f;
    const int steps = std::max(1, static_cast<int>(std::ceil(dist / halfTile)));
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const Vec2 p = fromWorld + (toWorld - fromWorld) * t;
        int tx = 0;
        int ty = 0;
        if (!WorldToTile(p, tx, ty) || !IsTileNavigableFor(agent, tx, ty)) {
            return false;
        }
    }
    return true;
}

std::vector<Vec2> StageState::FindPathWorld(const Vec2& fromWorld, const Vec2& toWorld, const GameObject* agent, int nodeBudget) const {
    std::vector<Vec2> empty;
    if (!HasNavigationGrid()) {
        return empty;
    }

    const int w = tileMapComp ? tileMapComp->GetWidth() : navGridWidthTiles;
    const int h = tileMapComp ? tileMapComp->GetHeight() : navGridHeightTiles;
    if (w <= 0 || h <= 0) {
        return empty;
    }

    auto passable = [&](int tx, int ty) {
        if (agent != nullptr) {
            return IsTileNavigableFor(agent, tx, ty);
        }
        return IsTileWalkable(tx, ty);
    };

    int sx = 0;
    int sy = 0;
    int gx = 0;
    int gy = 0;
    if (!WorldToTile(fromWorld, sx, sy) || !WorldToTile(toWorld, gx, gy)) {
        return empty;
    }

    if (!passable(sx, sy)) {
        int nearestX = sx;
        int nearestY = sy;
        if (!FindNearestWalkableTile(sx, sy, nearestX, nearestY, 8, agent)) {
            return empty;
        }
        sx = nearestX;
        sy = nearestY;
    }
    if (!passable(gx, gy)) {
        int nearestX = gx;
        int nearestY = gy;
        if (!FindNearestWalkableTile(gx, gy, nearestX, nearestY, 8, agent)) {
            return empty;
        }
        gx = nearestX;
        gy = nearestY;
    }

    auto idxOf = [w](int x, int y) { return x + y * w; };
    auto heuristic = [gx, gy](int x, int y) { return std::abs(gx - x) + std::abs(gy - y); };

    struct Node {
        int f;
        int g;
        int x;
        int y;
    };
    struct NodeCompare {
        bool operator()(const Node& a, const Node& b) const {
            return a.f > b.f;
        }
    };

    const int total = w * h;
    const int startIdx = idxOf(sx, sy);
    const int goalIdx = idxOf(gx, gy);
    std::vector<int> gScore(static_cast<size_t>(total), std::numeric_limits<int>::max());
    std::vector<int> parent(static_cast<size_t>(total), -1);
    std::vector<std::uint8_t> closed(static_cast<size_t>(total), 0);
    std::priority_queue<Node, std::vector<Node>, NodeCompare> open;

    gScore[static_cast<size_t>(startIdx)] = 0;
    open.push({heuristic(sx, sy), 0, sx, sy});

    int expanded = 0;
    static const std::array<Vec2, 4> kDirs = {
        Vec2(1.0f, 0.0f), Vec2(-1.0f, 0.0f), Vec2(0.0f, 1.0f), Vec2(0.0f, -1.0f)};

    while (!open.empty() && expanded < nodeBudget) {
        const Node current = open.top();
        open.pop();

        const int currentIdx = idxOf(current.x, current.y);
        if (closed[static_cast<size_t>(currentIdx)] != 0) {
            continue;
        }
        closed[static_cast<size_t>(currentIdx)] = 1;
        ++expanded;

        if (currentIdx == goalIdx) {
            break;
        }

        for (const Vec2& dir : kDirs) {
            const int nx = current.x + static_cast<int>(dir.x);
            const int ny = current.y + static_cast<int>(dir.y);
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) {
                continue;
            }
            if (!passable(nx, ny)) {
                continue;
            }

            const int nextIdx = idxOf(nx, ny);
            if (closed[static_cast<size_t>(nextIdx)] != 0) {
                continue;
            }

            const int tentativeG = current.g + 1;
            if (tentativeG < gScore[static_cast<size_t>(nextIdx)]) {
                gScore[static_cast<size_t>(nextIdx)] = tentativeG;
                parent[static_cast<size_t>(nextIdx)] = currentIdx;
                open.push({tentativeG + heuristic(nx, ny), tentativeG, nx, ny});
            }
        }
    }

    if (goalIdx != startIdx && parent[static_cast<size_t>(goalIdx)] < 0) {
        return empty;
    }

    std::vector<Vec2> reversedPath;
    int idx = goalIdx;
    reversedPath.push_back(TileCenterToWorld(gx, gy));
    while (idx != startIdx) {
        idx = parent[static_cast<size_t>(idx)];
        if (idx < 0) {
            return empty;
        }
        const int x = idx % w;
        const int y = idx / w;
        reversedPath.push_back(TileCenterToWorld(x, y));
    }

    std::reverse(reversedPath.begin(), reversedPath.end());
    return reversedPath;
}

void StageState::ApplyMapBoundsAndWalkability(GameObject* characterObject, const Vec2& previousPos) {
    if (!characterObject || !tileMapComp || !tileSet) {
        return;
    }

    const int tileW = std::max(1, tileSet->GetTileWidth());
    const int tileH = std::max(1, tileSet->GetTileHeight());
    const float mapMinX = mapOrigin.x;
    const float mapMinY = mapOrigin.y;
    const float mapMaxX = mapOrigin.x + static_cast<float>(tileMapComp->GetWidth() * tileW);
    const float mapMaxY = mapOrigin.y + static_cast<float>(tileMapComp->GetHeight() * tileH);

    auto clampBox = [&](Rect& b) {
        b.x = std::max(mapMinX, std::min(b.x, mapMaxX - b.w));
        b.y = std::max(mapMinY, std::min(b.y, mapMaxY - b.h));
    };

    Rect candidate = characterObject->box;
    clampBox(candidate);

    if (IsBoxWalkableOnMapLayer(candidate)) {
        characterObject->box = candidate;
        return;
    }

    // Axis-separated resolution keeps movement responsive when sliding along walls.
    Rect tryX = candidate;
    tryX.y = previousPos.y;
    clampBox(tryX);
    const bool xOk = IsBoxWalkableOnMapLayer(tryX);

    Rect tryY = candidate;
    tryY.x = xOk ? tryX.x : previousPos.x;
    clampBox(tryY);
    const bool yOk = IsBoxWalkableOnMapLayer(tryY);

    if (yOk) {
        characterObject->box = tryY;
    } else if (xOk) {
        characterObject->box = tryX;
    } else {
        Rect fallback = characterObject->box;
        fallback.x = previousPos.x;
        fallback.y = previousPos.y;
        clampBox(fallback);
        characterObject->box = fallback;
    }
}
