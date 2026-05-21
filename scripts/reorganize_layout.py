#!/usr/bin/env python3
"""One-time layout migration: flat src/include -> domain subdirectories."""

from __future__ import annotations

import re
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
INC = ROOT / "include"

# basename -> relative path inside src/ or include/
LAYOUT: dict[str, str] = {
    # entry
    "main.cpp": "main.cpp",
    # core
    "Game.cpp": "core/Game.cpp",
    "Game.h": "core/Game.h",
    "GameData.cpp": "core/GameData.cpp",
    "GameData.h": "core/GameData.h",
    "State.cpp": "core/State.cpp",
    "State.h": "core/State.h",
    "Resources.cpp": "core/Resources.cpp",
    "Resources.h": "core/Resources.h",
    "LevelManager.cpp": "core/LevelManager.cpp",
    "LevelManager.h": "core/LevelManager.h",
    "Timer.cpp": "core/Timer.cpp",
    "Timer.h": "core/Timer.h",
    "InputManager.cpp": "core/InputManager.cpp",
    "InputManager.h": "core/InputManager.h",
    # math
    "Vec2.cpp": "math/Vec2.cpp",
    "Vec2.h": "math/Vec2.h",
    "Rect.cpp": "math/Rect.cpp",
    "Rect.h": "math/Rect.h",
    # engine
    "GameObject.cpp": "engine/GameObject.cpp",
    "GameObject.h": "engine/GameObject.h",
    "Component.cpp": "engine/Component.cpp",
    "Component.h": "engine/Component.h",
    "Camera.cpp": "engine/Camera.cpp",
    "Camera.h": "engine/Camera.h",
    "SpriteRenderer.cpp": "engine/SpriteRenderer.cpp",
    "SpriteRenderer.h": "engine/SpriteRenderer.h",
    "Sprite.cpp": "engine/Sprite.cpp",
    "Sprite.h": "engine/Sprite.h",
    "Animation.cpp": "engine/Animation.cpp",
    "Animation.h": "engine/Animation.h",
    "Animator.cpp": "engine/Animator.cpp",
    "Animator.h": "engine/Animator.h",
    # audio
    "Music.cpp": "audio/Music.cpp",
    "Music.h": "audio/Music.h",
    "Sound.cpp": "audio/Sound.cpp",
    "Sound.h": "audio/Sound.h",
    # ui
    "Text.cpp": "ui/Text.cpp",
    "Text.h": "ui/Text.h",
    "FadeEffect.cpp": "ui/FadeEffect.cpp",
    "FadeEffect.h": "ui/FadeEffect.h",
    "VideoPlayer.cpp": "ui/VideoPlayer.cpp",
    "VideoPlayer.h": "ui/VideoPlayer.h",
    # world
    "TileMap.cpp": "world/TileMap.cpp",
    "TileMap.h": "world/TileMap.h",
    "TileSet.cpp": "world/TileSet.cpp",
    "TileSet.h": "world/TileSet.h",
    "Collider.cpp": "world/Collider.cpp",
    "Collider.h": "world/Collider.h",
    "Collision.h": "world/Collision.h",
    # lighting
    "TopDownLightShadows.cpp": "lighting/TopDownLightShadows.cpp",
    "TopDownLightShadows.h": "lighting/TopDownLightShadows.h",
    "LightShadowProfile.cpp": "lighting/LightShadowProfile.cpp",
    "LightShadowProfile.h": "lighting/LightShadowProfile.h",
    "RadialLightOverlay.cpp": "lighting/RadialLightOverlay.cpp",
    "RadialLightOverlay.h": "lighting/RadialLightOverlay.h",
    "LightTweakPanel.cpp": "lighting/LightTweakPanel.cpp",
    "LightTweakPanel.h": "lighting/LightTweakPanel.h",
    "LightMaskTypes.h": "lighting/LightMaskTypes.h",
    # gameplay
    "Character.cpp": "gameplay/Character.cpp",
    "Character.h": "gameplay/Character.h",
    "PlayerController.cpp": "gameplay/PlayerController.cpp",
    "PlayerController.h": "gameplay/PlayerController.h",
    "Box.cpp": "gameplay/Box.cpp",
    "Box.h": "gameplay/Box.h",
    "Repairable.cpp": "gameplay/Repairable.cpp",
    "Repairable.h": "gameplay/Repairable.h",
    "StairTrigger.cpp": "gameplay/StairTrigger.cpp",
    "StairTrigger.h": "gameplay/StairTrigger.h",
    "ItemPickup.cpp": "gameplay/ItemPickup.cpp",
    "ItemPickup.h": "gameplay/ItemPickup.h",
    "HotbarComponent.cpp": "gameplay/HotbarComponent.cpp",
    "HotbarComponent.h": "gameplay/HotbarComponent.h",
    "Inventory.cpp": "gameplay/Inventory.cpp",
    "Inventory.h": "gameplay/Inventory.h",
    "Item.h": "gameplay/Item.h",
    "Barrier.h": "gameplay/Barrier.h",
    # states
    "TitleState.cpp": "states/TitleState.cpp",
    "TitleState.h": "states/TitleState.h",
    "LoadingState.cpp": "states/LoadingState.cpp",
    "LoadingState.h": "states/LoadingState.h",
    "EndState.cpp": "states/EndState.cpp",
    "EndState.h": "states/EndState.h",
    "CutsceneState.cpp": "states/CutsceneState.cpp",
    "CutsceneState.h": "states/CutsceneState.h",
    # stage (renamed split files)
    "StageState.cpp": "states/stage/StageState.cpp",
    "StageState.h": "states/stage/StageState.h",
    "StageState_Internal.cpp": "states/stage/InternalHelpers.cpp",
    "StageState_Internal.h": "states/stage/InternalHelpers.h",
    "StageState_Load.cpp": "states/stage/Load.cpp",
    "StageState_Update.cpp": "states/stage/Update.cpp",
    "StageState_Render.cpp": "states/stage/Render.cpp",
    "StageState_Lifecycle.cpp": "states/stage/Lifecycle.cpp",
    "StageState_Lighting.cpp": "states/stage/Lighting.cpp",
    "StageState_Navigation.cpp": "states/stage/Navigation.cpp",
    "StageState_InputParty.cpp": "states/stage/PartyInput.cpp",
    "StageOceanAmbientController.cpp": "states/stage/OceanAmbientController.cpp",
    "StageOceanAmbientController.h": "states/stage/OceanAmbientController.h",
    "StageFirstLoadLoader.cpp": "states/stage/FirstLoadLoader.cpp",
    "StageFirstLoadData.h": "states/stage/FirstLoadData.h",
}

# Old include spellings -> new include path (without quotes)
LEGACY_INCLUDES: dict[str, str] = {
    "StageState_Internal.h": "states/stage/InternalHelpers.h",
    "StageFirstLoadData.h": "states/stage/FirstLoadData.h",
    "StageFirstLoadLoader.cpp": "states/stage/FirstLoadLoader.cpp",
}

INCLUDE_RE = re.compile(
    r'#include\s+"(?:\.\./(?:\.\./)?include/)?(?P<name>[^"/]+(?:/[^"]+)?)"'
)


def move_tree_files(base: Path, exts: tuple[str, ...]) -> None:
    skip_names = {"SDL_include.h", "pl_mpeg.h"}
    for path in sorted(base.iterdir()):
        if not path.is_file() or path.suffix not in exts:
            continue
        if path.name in skip_names:
            continue
        rel = LAYOUT.get(path.name)
        if rel is None:
            raise SystemExit(f"No layout mapping for {path}")
        dest = base / rel
        if path.resolve() == dest.resolve():
            continue
        dest.parent.mkdir(parents=True, exist_ok=True)
        if dest.exists():
            continue
        print(f"move {path.relative_to(ROOT)} -> {dest.relative_to(ROOT)}")
        shutil.move(str(path), str(dest))


def include_target(name: str) -> str | None:
    if name in LEGACY_INCLUDES:
        return LEGACY_INCLUDES[name]
    if name in LAYOUT:
        return LAYOUT[name]
    # already namespaced, e.g. nlohmann/json.hpp
    leaf = Path(name).name
    if leaf in LAYOUT:
        return LAYOUT[leaf]
    return None


def rewrite_includes(path: Path) -> bool:
    text = path.read_text(encoding="utf-8")
    changed = False

    def repl(match: re.Match[str]) -> str:
        nonlocal changed
        raw = match.group("name")
        leaf = Path(raw).name
        new = include_target(raw) or include_target(leaf)
        if new is None:
            return match.group(0)
        if new == raw:
            return match.group(0)
        changed = True
        return f'#include "{new}"'

    updated = INCLUDE_RE.sub(repl, text)
    if changed:
        path.write_text(updated, encoding="utf-8")
    return changed


def patch_internal_helpers_header() -> None:
    path = INC / "states/stage/InternalHelpers.h"
    text = path.read_text(encoding="utf-8")
    text = text.replace("STAGESTATE_INTERNAL_H", "STAGE_INTERNAL_HELPERS_H")
    text = text.replace('"LightMaskTypes.h"', '"lighting/LightMaskTypes.h"')
    text = text.replace('"Rect.h"', '"math/Rect.h"')
    text = text.replace('"Vec2.h"', '"math/Vec2.h"')
    text = text.replace('"SDL_include.h"', '"SDL_include.h"')
    path.write_text(text, encoding="utf-8")


def patch_stage_state_stub() -> None:
    path = SRC / "states/stage/StageState.cpp"
    text = path.read_text(encoding="utf-8")
    text = text.replace(
        "// StageState method definitions live in StageState_*.cpp",
        "// StageState method definitions live in states/stage/*.cpp",
    )
    path.write_text(text, encoding="utf-8")


def main() -> None:
    move_tree_files(SRC, (".cpp",))
    move_tree_files(INC, (".h",))

    targets: list[Path] = []
    for base in (SRC, INC):
        targets.extend(base.rglob("*.cpp"))
        targets.extend(base.rglob("*.h"))
    targets.append(ROOT / "scripts" / "_split_stage_state_once.py")

    touched = 0
    for path in targets:
        if path.exists() and rewrite_includes(path):
            print(f"includes: {path.relative_to(ROOT)}")
            touched += 1

    patch_internal_helpers_header()
    patch_stage_state_stub()
    print(f"Done. Updated includes in {touched} files.")


if __name__ == "__main__":
    main()
