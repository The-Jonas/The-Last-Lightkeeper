#include "world/SpawnFactory.h"
#include "states/stage/StageState.h"
#include "states/stage/FirstLoadData.h"
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
StageState::StageState(LoadMode mode) : loadMode(mode) {
    // Música de fundo (Ambientacao) carregada em LoadAssets(); ondas opcionais via oceanChunkCandidates.
    tileSet = nullptr;                           // TileSet ativo
    dungeonTileSet.reset();
    bigCharacterObject = nullptr;                // GameObject do personagem grande (IRMÃOZÃO)
    smallCharacterObject = nullptr;              // GameObject do personagem pequeno (IRMÃOZINHO)
    bigCharacter = nullptr;                      // Componente Character do grande (IRMÃOZÃO)
    smallCharacter = nullptr;                    // Componente Character do pequeno (IRMÃOZINHO)
    controlledCharacterObject = nullptr;         // GameObject atualmente controlado
    controlledCharacter = nullptr;               // Character atualmente controlado
    companionCharacterObject = nullptr;          // GameObject do parceiro (não controlado)
    companionCharacter = nullptr;                // Character do parceiro (não controlado)
    partyMode = PartyMode::TOGETHER;             // Estado atual da dupla (junto/independente)
    hudLine1 = nullptr;                          // Linha 1 de instruções
    hudLine2 = nullptr;                          // Linha 2 de instruções
    hudLine3 = nullptr;                          // Linha 3: atalhos luz
    hudFps = nullptr;                            // Linha FPS
    fpsSmoothed = 60.0f;
    fpsUiRefreshTimer = 0.0f;
    radialGeometry = nullptr;
    lightMaskShape = LightMaskShape::Torch;
    lightTweakPanel.reset();
    tileMapComp = nullptr;
    staticShadowEdges.clear();
    staticShadowEdgesBuilt = false;
    hasSmoothedDynamicLight = false;
    previewLightLockedToPlayer = false;
    previewLightAnchorPlayer = nullptr;
    oceanAmbient_.Bind(&oceanWavesChunk, &oceanMixerChannel, &musicMuted);

    SDL_Renderer* renderer = Game::GetInstance().GetRenderer();
    int winW = Game::GetInstance().GetWindowsWidth();
    int winH = Game::GetInstance().GetWindowsHeight();
    
    // Cria uma textura vazia para ser a nossa "Tela de Cinema"
    renderTarget = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, winW, winH);
}

StageState::~StageState(){                                
    if (oceanMixerChannel >= 0) {
        Mix_HaltChannel(oceanMixerChannel);
        oceanMixerChannel = -1;
    }
    // O destrutor de State cuida de limpar o objectArray
    // A música é limpa pelo destrutor de Music
    SetMouseConfinedToWindow(false);
    delete radialGeometry;
    radialGeometry = nullptr;
    lightTweakPanel.reset();
}

void StageState::LoadAssets() {

    const StageFirstLoadData cfg = LoadStageFirstLoadData();

    if (!music.IsOpen()) {
        music.Open(cfg.ostPath.c_str());
        Mix_VolumeMusic(0);
    }

    const LevelDef& levelDef = GetLevelDef(cfg, currentLevelIndex);
    level.LoadLevel(levelDef.mapPath, Game::GetInstance().GetRenderer());
    mapOrigin = Vec2(0,0);

    navTilePx = cfg.navTilePx;
    navGridWidthTiles = static_cast<int>(std::ceil(cfg.navWorldW / static_cast<float>(navTilePx)));
    navGridHeightTiles = static_cast<int>(std::ceil(cfg.navWorldH / static_cast<float>(navTilePx)));

    const bool resetInventory = !inventoryInitialized && loadMode == LoadMode::NewGame;
    BuildLevelWorld(cfg, resetInventory);
    inventoryInitialized = true;

    if (radialGeometry == nullptr) {
        Game& gameRef = Game::GetInstance();
        radialGeometry = new RadialLightOverlay();
        if (!radialGeometry->Init(gameRef.GetRenderer())) {
            delete radialGeometry;
            radialGeometry = nullptr;
        }
    }

    if (!lightTweakPanel) {
        lightTweakPanel = std::make_unique<LightTweakPanel>(lightMaskParams, lightMaskShape);
    }

    levelWorldW = cfg.navWorldW;
    levelWorldH = cfg.navWorldH;
    if (tileMapComp != nullptr && tileSet != nullptr) {
        const int tileWPx = std::max(1, tileSet->GetTileWidth());
        const int tileHPx = std::max(1, tileSet->GetTileHeight());
        levelWorldW = static_cast<float>(tileMapComp->GetWidth() * tileWPx);
        levelWorldH = static_cast<float>(tileMapComp->GetHeight() * tileHPx);
    }

    if (oceanMixerChannel >= 0) {
        Mix_HaltChannel(oceanMixerChannel);
        oceanMixerChannel = -1;
    }
    // Preferir OGG/WAV curtos como chunk (MP3 longo decodificado inteiro na RAM pode falhar em Mix_LoadWAV_RW).
    oceanWavesChunk.reset();
    for (const std::string& path : cfg.oceanChunkCandidates) {
        oceanWavesChunk = Resources::GetDecodedChunk(path.c_str());
        if (oceanWavesChunk) {
            break;
        }
    }
    if (oceanWavesChunk) {
        oceanAmbient_.EnsurePlaying();
    } else if (!cfg.oceanChunkCandidates.empty()) {
        std::cerr << "Ocean waves: falha ao carregar qualquer formato (tentou .ogg, .wav, .mp3). "
                     "SDL_mixer só tem 1 faixa Mix_Music; o ambiente precisa de Mix_Chunk. "
                     "MP3 muito longo como chunk pode falhar (RAM); prefira um loop OGG/WAV curto."
                  << std::endl;
    }

    levelContentLoaded = true;
}
