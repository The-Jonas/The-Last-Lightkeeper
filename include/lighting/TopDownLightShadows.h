#ifndef TOP_DOWN_LIGHT_SHADOWS_H
#define TOP_DOWN_LIGHT_SHADOWS_H

#define INCLUDE_SDL
#include "SDL_include.h"

#include "math/Rect.h"
#include "math/Vec2.h"

#include <cstdint>
#include <vector>

struct TopDownShadowEdge {
    Vec2 a;
    Vec2 b;
};

namespace TopDownLightShadows {

void BuildShadowEdgesFromSolidGrid(int mapW, int mapH, float tileW, float tileH, float mapOriginX, float mapOriginY,
                                   const std::vector<std::uint8_t>& solid, std::vector<TopDownShadowEdge>& outEdges);

void AppendAabbShadowEdges(const Rect& worldAabb, std::vector<TopDownShadowEdge>& outEdges);

void AppendCircleShadowEdges(const Vec2& centerWorld, float radiusWorld, int segments, std::vector<TopDownShadowEdge>& outEdges);

void RenderShadowVolumes(SDL_Renderer* renderer, float lightScreenX, float lightScreenY, int windowW, int windowH,
                         const std::vector<TopDownShadowEdge>& staticEdgesWorld,
                         const std::vector<TopDownShadowEdge>& dynamicEdgesWorld, Uint8 shadowAlpha = 80,
                         float shadowLengthPx = 160.0f, int softnessLayers = 1, float softness = 0.5f);

} // namespace TopDownLightShadows

#endif
