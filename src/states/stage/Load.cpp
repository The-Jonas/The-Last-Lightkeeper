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
StageState::StageState() {
    // Música de fundo (Last Hideout) e ondas são iniciadas em LoadAssets() para ficar em sync com o carregamento do nível.
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

    // ==================================
    // OBS: TOMAR CUIDADO NA ORDEM EM QUE CARREGAMOS OS COMPONENTES, POIS MUITO PROVAVELMENTE ISSO É A CAUSA DE ESTAREM SUMINDO, UM É DESENHADO POR CIMA DO OUTRO
    // ==================================

    // Carrega a OST; o Play fica em Start() para não tocar durante LoadingState::LoadAssets.
    music.Open(cfg.ostPath.c_str());
    Mix_VolumeMusic(0);

    // Carregamento do mapa livre
    level.LoadLevel(cfg.levelPath, Game::GetInstance().GetRenderer());
    mapOrigin = Vec2(0,0);

    // Mapa atual: imagens + colisão no JSON — não há componente TileMap; A* usa grade sintética no mesmo espaço do `LevelManager`.
    navTilePx = cfg.navTilePx;
    navGridWidthTiles = static_cast<int>(std::ceil(cfg.navWorldW / static_cast<float>(navTilePx)));
    navGridHeightTiles = static_cast<int>(std::ceil(cfg.navWorldH / static_cast<float>(navTilePx)));

    //------------------------------------------
    
    // Criação dos dois personagens controláveis
    // Personagem grande (IRMÃOZÃO)
    GameObject* bigObject = new GameObject();
    Character* bigComp = new Character(*bigObject, "Recursos/img/personagens/Irmãozão", true);
    bigObject->AddComponent(bigComp);
    bigObject->z = 2;
    AddObject(bigObject);

    // Personagem pequeno (IRMÃOZINHO)
    GameObject* smallObject = new GameObject();
    Character* smallComp = new Character(*smallObject, "Recursos/img/personagens/irmãozinho");
    smallObject->AddComponent(smallComp);
    smallObject->z = 2;
    AddObject(smallObject);

    // Configurações do personagem pequeno (IRMÃOZINHO)
    //SpriteRenderer* smallSprite = smallObject->GetComponent<SpriteRenderer>();
    //if (smallSprite) {
    //    smallSprite->SetScale(0.72f, 0.72f);
    //    smallSprite->SetTint(150, 200, 255, 240);
    //}
    //smallComp->SetBaseSpeed(275.0f);


    // ==========================================
    // SPAWN DOS IRMÃOS: posição vem do Tiled, com fallback pro centro
    // ==========================================
    const float centerX = cfg.navWorldW / 2.0f;
    const float centerY = cfg.navWorldH / 2.0f;

    float bigSpawnX = 0, bigSpawnY = 0;
    float smallSpawnX = 0, smallSpawnY = 0;
    bool bigFoundInTiled = false, smallFoundInTiled = false;

    // ==========================================
    // SPAWN AUTOMÁTICO DAS ENTIDADES (TILED)
    // ==========================================
    for (const auto& spawn : level.entitySpawns) {
        
        // 1. Spawns de jogador ficam aqui, pois alteram as variáveis locais do Load!
        if (spawn.type == "PlayerSpawn_Big") {
            bigSpawnX = spawn.x;
            bigSpawnY = spawn.y - bigObject->box.h;
            bigFoundInTiled = true;
        }
        else if (spawn.type == "PlayerSpawn_Small") {
            smallSpawnX = spawn.x;
            smallSpawnY = spawn.y - smallObject->box.h;
            smallFoundInTiled = true;
        }
        // 2. Todo o resto (Inimigos, Caixas, Escadas, Itens) vai para a Factory
        else {
            SpawnFactory::SpawnEntity(spawn, *this, cfg); 
        }
    }

    // Fallback só entra se o Tiled não tinha os pontos
    if (!bigFoundInTiled) {
        bigSpawnX = centerX - bigObject->box.w * 0.5f;
        bigSpawnY = centerY - bigObject->box.h;
    }
    if (!smallFoundInTiled) {
        smallSpawnX = bigSpawnX - std::max(40.0f, smallObject->box.w * 1.2f);
        smallSpawnY = bigSpawnY;
    }

    bigObject->box.x = bigSpawnX;
    bigObject->box.y = bigSpawnY;
    smallObject->box.x = smallSpawnX;
    smallObject->box.y = smallSpawnY;

    previewLightLockedToPlayer = true;
    previewLightAnchorPlayer = bigCharacterObject;

    bigCharacterObject = bigObject;
    smallCharacterObject = smallObject;
    bigCharacter = bigComp;
    smallCharacter = smallComp;
    controlledCharacterObject = bigCharacterObject;
    controlledCharacter = bigCharacter;
    companionCharacterObject = smallCharacterObject;
    companionCharacter = smallCharacter;

    // Modo default: dupla andando juntos
    partyMode = PartyMode::TOGETHER;

    SDL_Color hudColor = {230, 230, 230, 220}; // Cor do texto do HUD

    // Linha 1 de instruções
    hudLine1 = new GameObject();
    hudLine1->z = 100;
    hudLine1->AddComponent(new Text(*hudLine1, "Recursos/font/TradeWinds-Regular.ttf", 18, Text::BLENDED, "TAB: roda de inventario | Ctrl: trocar personagem | E: pegar", hudColor));
    AddObject(hudLine1);

    // Linha 2 de instruções
    hudLine2 = new GameObject();
    hudLine2->z = 100;
    hudLine2->AddComponent(new Text(*hudLine2, "Recursos/font/TradeWinds-Regular.ttf", 18, Text::BLENDED, "F: alternar entre junto e independente", hudColor));
    AddObject(hudLine2);

    hudLine3 = new GameObject();
    hudLine3->z = 100;
    hudLine3->AddComponent(new Text(*hudLine3, "Recursos/font/TradeWinds-Regular.ttf", 18, Text::BLENDED,
                                      "K forma | X luz cursor | C criar luz fixa | P painel | L luz | O sombras | B mapa", hudColor));
    AddObject(hudLine3);

    hudFps = new GameObject();
    hudFps->z = 100;
    hudFps->AddComponent(new Text(*hudFps, "Recursos/font/TradeWinds-Regular.ttf", 18, Text::BLENDED, "FPS: 60", hudColor));
    AddObject(hudFps);

    Game& gameRef = Game::GetInstance();
    radialGeometry = new RadialLightOverlay();
    if (!radialGeometry->Init(gameRef.GetRenderer())) {
        delete radialGeometry;
        radialGeometry = nullptr;
    }

    lightTweakPanel = std::make_unique<LightTweakPanel>(lightMaskParams, lightMaskShape);

    levelWorldW = cfg.navWorldW;
    levelWorldH = cfg.navWorldH;
    if (tileMapComp != nullptr && tileSet != nullptr) {
        const int tileWPx = std::max(1, tileSet->GetTileWidth());
        const int tileHPx = std::max(1, tileSet->GetTileHeight());
        levelWorldW = static_cast<float>(tileMapComp->GetWidth() * tileWPx);
        levelWorldH = static_cast<float>(tileMapComp->GetHeight() * tileHPx);
    }

    inventory.SetUsing(ItemInstance{cfg.startingFlashlight, cfg.startingFlashlightDurability});

    GameObject* hotbarObj = new GameObject();
    hotbarObj->AddComponent(new HotbarComponent(*hotbarObj, inventory, bigCharacter,
                                                  &controlledCharacter, itemPickups,
                                                  [this](GameObject* obj) { AddObject(obj); },
                                                  [this](Vec2 tl, float w, float h) {
                                                      return ClampPickupTopLeft(tl, w, h);
                                                  }));
    hotbarObj->z = 200;
    AddObject(hotbarObj);
    hotbarObject = hotbarObj;

    RefreshCameraTargets(); // Atualiza alvos da câmera (dupla + principal)

    // Ondas (fora do mapa): primeiro andar jogável em diante. Tenta WAV/OGG e depois MP3 (decoded chunk).
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
    if (!oceanWavesChunk) {
        std::cerr << "Ocean waves: falha ao carregar qualquer formato (tentou .ogg, .wav, .mp3). "
                     "SDL_mixer só tem 1 faixa Mix_Music; o ambiente precisa de Mix_Chunk. "
                     "MP3 muito longo como chunk pode falhar (RAM); prefira um loop OGG/WAV curto."
                  << std::endl;
    }
    oceanAmbient_.EnsurePlaying();

    levelContentLoaded = true;
}
