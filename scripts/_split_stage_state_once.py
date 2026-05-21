# Run from repo root: python scripts/_split_stage_state_once.py
# Backs up src/StageState.cpp and splits into StageState_Internal.* + StageState_*.cpp

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src" / "StageState.cpp"
BACKUP = ROOT / "src" / "StageState.cpp.bak_before_split"

COMMON = r'''#include "states/stage/StageState.h"
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
'''

INTERNAL_HPP = ROOT / "include" / "StageState_Internal.h"
INTERNAL_CPP = ROOT / "src" / "StageState_Internal.cpp"


def slice_lines(lines: list[str], start_1: int, end_1: int) -> str:
    """1-based inclusive line numbers."""
    return "".join(lines[start_1 - 1 : end_1])


def main():
    text = SRC.read_text(encoding="utf-8")
    lines = text.splitlines(keepends=True)
    BACKUP.write_text(text, encoding="utf-8")

    # Anonymous helpers -> stage_internal namespace (lines 51-302)
    anon = slice_lines(lines, 51, 302)
    anon = anon.replace("namespace {", "namespace stage_internal {", 1)

    INTERNAL_CPP.write_text(
        '#include "states/stage/InternalHelpers.h"\n'
        '#include "core/Game.h"\n'
        '#include "engine/GameObject.h"\n'
        '#include "engine/SpriteRenderer.h"\n'
        '#include "engine/Camera.h"\n'
        '#include "lighting/LightMaskTypes.h"\n'
        '#define INCLUDE_SDL\n'
        '#include "../include/SDL_include.h"\n'
        "#include <algorithm>\n#include <cmath>\n#include <vector>\n\n"
        "#ifndef M_PI\n#define M_PI 3.14159265358979323846\n#endif\n\n"
        + anon
        + "\n",
        encoding="utf-8",
    )

    INTERNAL_HPP.write_text(
        """#ifndef STAGESTATE_INTERNAL_H
#define STAGESTATE_INTERNAL_H

#include \"LightMaskTypes.h\"
#include \"Rect.h\"
#include \"Vec2.h\"

#define INCLUDE_SDL
#include \"SDL_include.h\"

class GameObject;

namespace stage_internal {

extern float gStageOstSilenceRecover;

void SetMouseConfinedToWindow(bool shouldConfine);
float Clamp01(float v);
void DrawDebugCircle(SDL_Renderer* renderer, float cx, float cy, float r, Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha);
void DrawDebugRect(SDL_Renderer* renderer, const Rect& rect, Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha);
void DrawColliderDebugWire(SDL_Renderer* renderer, const Rect& box, float angleDeg, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca);
void DrawDebugCross(SDL_Renderer* renderer, float x, float y, float halfSize, Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha);
void DrawPlayerShadowTouchDebug(SDL_Renderer* renderer, GameObject* go, Uint8 red, Uint8 green, Uint8 blue);
float ComputeShadowDistanceRate(const Vec2& pointScreen, const Vec2& lightScreenPos, const LightMaskParams& params,
                                float* outDistancePx = nullptr, float* outMaxDistancePx = nullptr);
bool IsFootLit(GameObject* go, const Vec2& lightScreenPos, const LightMaskParams& params, float* outIntensity = nullptr);
void RenderProjectedSpriteShadow(GameObject* go, const Vec2& lightScreenPos, float lightTouch, float shadowLengthPx, Uint8 shadowAlpha,
                                 const LightMaskParams& params);
void DrawContactFootShadow(SDL_Renderer* renderer, const Rect& box, float contactRate);

} // namespace stage_internal

#endif
""",
        encoding="utf-8",
    )

    def write_tu(path: Path, start_1: int, end_1: int):
        path.write_text(COMMON + slice_lines(lines, start_1, end_1), encoding="utf-8")

    write_tu(ROOT / "src" / "StageState_Load.cpp", 304, 698)
    write_tu(ROOT / "src" / "StageState_Update.cpp", 700, 969)
    write_tu(ROOT / "src" / "StageState_Render.cpp", 971, 1386)
    write_tu(ROOT / "src" / "StageState_Lifecycle.cpp", 1388, 1411)

    lighting_path = ROOT / "src" / "StageState_Lighting.cpp"
    lighting_path.write_text(
        COMMON + slice_lines(lines, 1413, 1470),
        encoding="utf-8",
    )

    write_tu(ROOT / "src" / "StageState_Navigation.cpp", 1476, 1913)

    party_path = ROOT / "src" / "StageState_InputParty.cpp"
    party_path.write_text(
        COMMON + slice_lines(lines, 1472, 1474) + slice_lines(lines, 1915, 2107),
        encoding="utf-8",
    )

    stub = "".join(lines[0:44])  # includes through blank after #endif M_PI (lines 1-44)
    stub += '#include "states/stage/InternalHelpers.h"\n'
    stub += "\n// StageState method definitions live in StageState_*.cpp\n"
    SRC.write_text(stub, encoding="utf-8")

    print("OK:", BACKUP.name)


if __name__ == "__main__":
    main()
