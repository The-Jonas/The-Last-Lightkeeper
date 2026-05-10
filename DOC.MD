# DOC.MD -- A Luz do Farol (The Lighthouse Light)

## 1. Project Overview

**A Luz do Farol** is a 2D top-down cooperative adventure game developed in **C++17** using the **SDL2** multimedia library suite. It was created as a group project for the *Introducao a Desenvolvimento de Jogos* (Introduction to Game Development) course at the **Universidade de Brasilia (UnB)**.

**Author:** Joao Victor Pereira Vieira (211036114)

## 2. Technology Stack

| Layer | Library | Purpose |
|-------|---------|---------|
| Windowing & Rendering | SDL2 | Window creation, GPU-accelerated 2D rendering |
| Image Loading | SDL2_image | Load PNG, JPG, TIF textures |
| Audio | SDL2_mixer | Background music (WAV/OGG/MP3) and sound effects (WAV) |
| Font Rendering | SDL2_ttf | TrueType font rendering for HUD and UI text |
| Math | `<cmath>` (libm) | Trigonometry, sqrt, etc. |
| Build System | GNU Make / MinGW-make | Cross-platform compilation |
| Compiler | g++ (TDM-GCC-32 on Windows) | C++17 with pedantic warnings |

## 3. Project Structure

```
A-Luz-do-Farol---Jogo/
├── .git/
├── .gitignore
├── .vscode/
│   ├── c_cpp_properties.json      # IntelliSense config for VS Code (Windows/MinGW)
│   └── settings.json              # VS Code file associations
├── bin/                           # Compiled .o object files (30 files)
├── dep/                           # Auto-generated .d dependency files (30 files)
├── include/                       # Header files (32 .h files)
├── JOGO.exe                       # Compiled Windows executable
├── makefile                       # GNU Make build system
├── README.md                      # Brief project description (Portuguese)
├── Recursos/                      # Game assets (Resources)
│   ├── audio/                     # 8 audio files (BGM, SFX, end state music)
│   ├── font/
│   │   └── neodgm.ttf             # Korean pixel font used for UI text
│   ├── img/                       # 12 image files (sprites, backgrounds, tilesets)
│   │   └── dungeon/               # Dungeon tileset images
│   └── map/                       # 8 map/tilemap files
│       ├── level.json             # Tilemap editor source (JSON)
│       ├── level_from_json.txt    # Converted game-level tilemap
│       └── stone-bridge.png       # Classic tileset atlas
├── SDL2/                          # SDL2 runtime libraries (Windows)
├── SDL2.dll, SDL2_image.dll,      # SDL2 shared libraries (Windows)
│   SDL2_mixer.dll, SDL2_ttf.dll
├── src/                           # Source files (30 .cpp files)
└── tools/
    └── tilemap_json_to_txt.py     # Python script to convert JSON tilemaps to game format
```

## 4. Architecture & Design Patterns

### 4.1 Component-Based Entity System
The game uses an **ECS-like** architecture with `GameObject` as entities and `Component` as attachable behaviors:

```
GameObject (entity)
 ├── SpriteRenderer (visual)
 ├── Animator (sprite animation)
 ├── Character (movement logic, command queue)
 ├── Collider (collision detection)
 ├── TileMap (tile-based map rendering)
 └── Text (UI text rendering)
```

**`GameObject`** (`include/GameObject.h`) has:
- `Rect box` - position and size in world space
- `double angleDeg` - rotation in degrees
- `int z` - depth sorting layer
- `int sub_z` - sub-layer for tiebreaking
- `GameObject* owner` - parent reference for Y-sorting

### 4.2 State Machine Pattern
Each game screen is a `State` subclass managed in a **stack**:

```
Game
 ├── TitleState        (Start screen: "Pressione Espaco")
 ├── StageState         (Main gameplay)
 └── EndState           (Win/Lose screen)
```

**`State`** (`include/State.h`):
- `LoadAssets()` - load resources
- `Start()` - initialize state
- `Update(float dt)` - per-frame logic
- `Render()` - draw to screen
- `Pause()` / `Resume()` - stack management
- `PopRequested()` / `QuitRequested()` - flow control
- `objectArray` - vector of `shared_ptr<GameObject>` owned by the state

### 4.3 Singletons
- **`Game`** - Window, renderer, delta time, state stack
- **`InputManager`** - Keyboard and mouse input (Meyers' Singleton)
- **`Camera`** - Fully static class (no instance needed)

### 4.4 Resource Management
**`Resources`** (`include/Resources.h`) is a fully static class that caches loaded assets using `shared_ptr` with custom deleters:

| Method | Returns | Deletion |
|--------|---------|----------|
| `GetImage(file)` | `shared_ptr<SDL_Texture>` | `SDL_DestroyTexture` |
| `GetMusic(file)` | `shared_ptr<Mix_Music>` | `Mix_FreeMusic` |
| `GetSound(file)` | `shared_ptr<Mix_Chunk>` | `Mix_FreeChunk` |
| `GetFont(file, size)` | `shared_ptr<TTF_Font>` | `TTF_CloseFont` |

## 5. Game States in Detail

### 5.1 TitleState (`src/TitleState.cpp`)
- Displays `Title.png` as full-screen background
- Shows blinking "Pressione Espaco" (Press Space) text
- **Space** → pushes `StageState` to start the game
- **ESC** → quits the game

### 5.2 StageState (`src/StageState.cpp`) - Main Gameplay
The core of the game. Features:

#### Two-Player Party System
- **Big character** (IRMAOZAO) and **Small character** (IRMAOZINHO)
- **Party modes:**
  - `TOGETHER` - Companion follows the controlled character with A* pathfinding
  - `INDEPENDENT` - Only the controlled character moves
- **TAB** - Switch controlled character
- **F** - Toggle party mode (together/independent)
- **WASD** - Movement (command queue system with acceleration/deceleration)
- **Right-click on character** - Lock preview light to that character

#### Dynamic Lighting System
One of the most sophisticated features. The mouse cursor acts as a **light source** that illuminates the dark map:

- **5 light shapes:** Circle, Ellipse, Cone, SoftRect, Torch (animated flame)
- **K** - Cycle light shape
- **C** - Place a permanent light at cursor position (up to 24 active)
- **L** - Toggle lights on/off
- **O** - Toggle shadows on/off

**Light Parameters** (configurable via panel):
| Parameter | Default | Range |
|-----------|---------|-------|
| `darknessMax` | 250 | 0-255 (overlay alpha) |
| `falloffRadiusPx` | 240 | 40-600 |
| `falloffCurve` | Smoothstep | Smoothstep / Power |
| `falloffGamma` | 2.0 | 0.15-4.0 |
| `innerLift` | 0.0 | 0-0.85 |
| `numRings` | 48 | 8-48 |
| `numSeg` | 48 | 8-48 |
| Torch animation speed, motion range, warp, pulse, color | various | 0-1 range |

#### Procedural Shadows
- Static shadows cast from solid map tiles
- Dynamic sprite-projected shadows cast from light onto characters
- Contact foot shadows when a character is near a light source
- Multi-layered soft shadow volumes

#### TileMap System
- 52x52 tile grid loaded from `level_from_json.txt`
- 2 map layers with parallax scrolling
- Walkable tile validation (tile IDs: 0,1,2,7,8,9,30,31,37,38)
- A* pathfinding (`FindPathWorld`) for companion AI
- Solid tiles generate light occlusion edges for shadow casting

#### Camera (`Camera.cpp`)
- `FollowPair()` mode tracks both characters simultaneously
- Dynamic zoom (0.65x-1.0x) based on distance between characters
- Mouse look-ahead (biased toward controlled character)
- Smooth exponential interpolation

#### HUD
- Top-left overlay with keyboard shortcuts
- FPS counter with color-coded health indicator (green/yellow/red)
- Korean pixel font (neodgm.ttf) at 14-18pt

#### Audio
- Background music: `BGM.wav` (muted by default, **M** to toggle)
- Volume: 5% of SDL max volume (`MASTER_VOLUME_PERCENT = 5`)

### 5.3 EndState (`src/EndState.cpp`)
- Shows `Win.png` or `Lose.png` depending on `GameData::playerVictory`
- Plays corresponding end music (`endStateWin.ogg` / `endStateLose.ogg`)
- **ESC** → quit game
- **SPACE** → return to TitleState

## 6. Component Details

### 6.1 Character (`Character.cpp`)
- Command queue system with acceleration/deceleration
- Speed: 250 px/s (big), 275 px/s (small)
- Speed multiplier for catch-up (1.55x)
- 4-directional sprite animations (idle, walking + horizontal flip)
- Death animation (frames 10-11)

### 6.2 PlayerController (`PlayerController.cpp`)
- Standalone WASD input component (separate from Character)
- Normalizes direction before issuing MOVE commands

### 6.3 SpriteRenderer (`SpriteRenderer.cpp`)
- Wraps `Sprite` as a `Component`
- Supports scale, tint (color mod + alpha), flip (horizontal/vertical)
- Camera-aware rendering with zoom

### 6.4 Animator (`Animator.cpp`)
- Frame-based animation with configurable frame time
- Animation transitions reset elapsed time
- Supports per-animation flip (e.g., mirrored walking frames)

### 6.5 Collider (`Collider.cpp`)
- Scaled collision box with offset from GameObject center
- Rotation-aware update (uses `Vec2::Rotated`)
- Debug rendering (red outline) in debug builds

### 6.6 TileMap (`TileMap.cpp`)
- 3D tile matrix [x][y][layer] stored as 1D vector
- Parallax multipliers per layer
- Light occlusion grid generation (`BuildLightOcclusionFromLayer`)
- Map file format: `width,height,depth,` followed by comma-separated tile indices

### 6.7 Text (`Text.cpp`)
- Renders TrueType text as SDL texture
- Three styles: SOLID, SHADED, BLENDED
- Dynamic text rebuild on property changes

## 7. Utility Classes

| Class | File | Purpose |
|-------|------|---------|
| `Vec2` | `Vec2.h` | 2D vector (add, sub, scale, normalize, rotate, magnitude, distance, angle) |
| `Rect` | `Rect.h` | Axis-aligned rectangle (center, contains, distance, translate) |
| `Collision` | `Collision.h` | SAT-based rotated rectangle collision (Separating Axis Theorem) |
| `Sprite` | `Sprite.h` | Low-level SDL texture wrapper with frames, scale, tint, flip |
| `Timer` | `Timer.h` | Accumulates delta time, supports restart |
| `Music` | `Music.h` | Background music playback (SDLE_Mixer) |
| `Sound` | `Sound.h` | Sound effect playback with channel tracking |
| `Animation` | `Animation.h` | Frame range + timing data |
| `RadialLightOverlay` | `RadialLightOverlay.h` | GPU-based radial light rendering via SDL_RenderGeometry |
| `LightTweakPanel` | `LightTweakPanel.h` | In-game light parameter tweaking UI (31 configurable rows) |
| `TopDownLightShadows` | `TopDownLightShadows.h` | Static/dynamic shadow volume generation and rendering |
| `GameData` | `GameData.h` | Shared game state (`playerVictory` flag) |

## 8. Input Key Bindings

| Key | Action |
|-----|--------|
| WASD | Move controlled character |
| TAB | Switch controlled character |
| F | Toggle party mode (together/independent) |
| K | Cycle light shape |
| C | Create permanent light at cursor |
| P | Toggle light tweaking panel |
| L | Toggle lights on/off |
| O | Toggle shadows on/off |
| M | Toggle music mute |
| T | Switch tileset (classic / extended) |
| ESC | Return to menu / Quit |
| SPACE | Start game (title screen) |
| Right-click on character | Lock preview light to that character |
| +/- | Adjust selected panel parameter |
| [ / ] | Navigate panel rows |
| Middle mouse (right) | Contextual (light anchor selection) |

## 9. Build System

### 9.1 Make Targets
```
make          - Debug build (ggdb -O0 -DDEBUG)
make release  - Optimized build (-O3 -mtune=native)
make debug    - Debug build
make clean    - Remove bin/, dep/, and executable
make folders  - Create required directories
make help     - Show available targets
```

### 9.2 Compiler Flags
```
C++17 standard (-std=c++17)
All warnings (-Wall -Wextra -pedantic)
No unused parameter warnings (-Wno-unused-parameter)
Init-self error (-Werror=init-self)
```

### 9.3 Platform Support
- **Windows:** MinGW-w64 with SDL2 DLLs, `-mwindows` (release) / `-mconsole` (debug)
- **Linux:** System SDL2 packages via pkg-config equivalent
- **macOS:** SDL2 frameworks

## 10. Game Assets (Recursos/)

### 10.1 Audio
| File | Type | Usage |
|------|------|-------|
| BGM.wav | Background music | StageState looping music |
| Dead.wav | Sound effect | Character death (unused in current code) |
| endStateLose.ogg | Music | EndState loss screen |
| endStateWin.ogg | Music | EndState win screen |
| Hit0.wav, Hit1.wav | Sound effects | Hit sounds (unused) |
| PumpAction.mp3 | Music | Action music (unused) |
| Range.wav | Sound effect | Range attack sound (unused) |

### 10.2 Images
| File | Usage |
|------|-------|
| Background.png | StageState background (camera-following) |
| Player.png | Character sprite sheet (3x4 frames) |
| NPC.png, Enemy.png, Gun.png, Bullet.png | Unused assets |
| Title.png | TitleState background |
| Win.png, Lose.png | EndState screens |
| Tileset.png | Extended tileset atlas (24x16 grid) |
| stone-bridge.png | Classic tileset (stone bridge) |
| radial-light.jpg | Reference texture (unused at runtime) |
| dungeon/*.png | Additional tileset images |

### 10.3 Maps
| File | Description |
|------|-------------|
| level.json | Tilemap editor source (JSON format) |
| level_from_json.txt | Converted 52x52x2 tilemap for the game |
| lighthouse_floor1.txt, lighthouse_floor_52.txt | Alternate test levels |
| map.txt, map_old.txt | Legacy test maps |
| stone-bridge.txt | Stone bridge tileset reference |

### 10.4 Font
| File | Description |
|------|-------------|
| neodgm.ttf | Korean pixel font (NeoDunggeunmo), used for all UI text |

## 11. Key Constants & Configuration

```cpp
// Game (Game.h)
WINDOW_WIDTH = 1920          // Window resolution
WINDOW_HEIGHT = 1080
MASTER_VOLUME_PERCENT = 5    // Volume level (0-100)

// StageState
maxActiveLights = 24         // Max concurrent placed lights
walkableTileIds = {0,1,2,7,8,9,30,31,37,38}  // Passable tiles

// Character
linearSpeed = 250.0f         // Big character base speed
charBaseSpeed = 275.0f       // Small character base speed
acceleration = 1000.0f
deceleration = 1400.0f
speedMultiplier = 1.0f       // Normal; 1.55x for catch-up
preferredDistance = 68.0f    // Ideal follow distance
catchupDistance = 420.0f     // When companion starts sprinting

// LightTweakPanel
kLogicalRows = 31            // Number of tweakable parameters
```

## 12. Tools

### tilemap_json_to_txt.py
Python script that converts tilemap editor JSON exports to the game's comma-separated text format.

**Usage:**
```bash
python3 tools/tilemap_json_to_txt.py level.json -o output.txt --cols 7 --depth 2
```

**Parameters:**
- `--cols` - Tileset column count (default 7)
- `--depth` - Number of layers to export (default 2)

## 13. Key Code Patterns

### Z/Y Sorting (Render Order)
Objects are sorted before rendering using a three-level comparison:
1. **Z-Sorting:** Lower `z` rendered first (background layers)
2. **Y-Sorting:** Objects with same `z` are ordered by their bottom Y coordinate (feet)
3. **Sub-Z:** Tiebreaker using `sub_z` for same-foot objects (e.g., character vs. weapon)

### Movement System
Characters use a **command queue** with smooth acceleration:
```
Input → Issue(Command::MOVE) → taskQueue → Update processes commands → targetSpeed
→ acceleration/deceleration lerp → speed → box position
```

### Lighting Pipeline
1. Compute light attenuation per grid vertex (multi-light composition)
2. Render radial light overlay as screen-space geometry triangles (additive blend)
3. Compute shadow volumes from static edges + dynamic edges
4. Render shadow volumes as dark quads extending from light source
5. Render projected sprite shadows (character shadows on the ground)
6. Draw contact foot shadow circles

## 14. SDL Initialization

```cpp
// SDL subsystems initialized:
SDL_INIT_VIDEO     // Window and rendering
SDL_INIT_AUDIO     // Audio output
SDL_INIT_TIMER     // Timer for delta time

IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_TIF)

Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 1024)
Mix_AllocateChannels(32)  // 32 simultaneous sound channels

TTF_Init()
```

## 15. Implementation Notes

- The game uses **C++17** features including `std::optional`, structured bindings, `if constexpr`, and lambda improvements.
- Smart pointers (`shared_ptr`, `unique_ptr`) are used throughout for memory management.
- The `Sprite` class no longer destroys its own texture; `Resources` manages texture lifetime via `shared_ptr` with custom deleters.
- The camera is fully static, allowing any part of the code to access it without passing references.
- `SDL_RenderGeometry()` is used extensively for the lighting system's GPU-based rendering.
- The lighting tweaking panel (`LightTweakPanel`) provides real-time parameter adjustment with sliders.

## 16. Potential Game Progression

1. **Title Screen** → Press Space
2. **Gameplay** → Navigate the tile-based map with two characters using WASD
3. Use the mouse cursor as a dynamic light source to reveal the dark map
4. Switch between characters (TAB) to solve cooperative puzzles
5. Place permanent lights (C) at strategic positions
6. **Game Over** → `GameData::playerVictory` determines Win/Lose screen
7. Return to menu or quit
