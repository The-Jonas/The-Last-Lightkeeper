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
        
        if (spawn.type == "Caixa") {
            GameObject* boxObj = new GameObject();
            boxObj->z = spawn.z; 
            // Instancia a classe Box passando a flag isStatic lida do Tiled!
            // o SpriteRenderer carrega a imagem e ajusta a largura/altura do boxObj.
            boxObj->AddComponent(new Box(*boxObj, spawn.isStatic));
            // Subtrai metade da largura e metade da altura.
            // Agora o ponto do Tiled fica exatamente no centro da caixa!
            boxObj->box.x = spawn.x;
            boxObj->box.y = spawn.y - (boxObj->box.h);
            AddObject(boxObj);
        }
        // Exemplo de objeto que só faz parte do cenário e por isso não tem classe própria
        else if (spawn.type == "Pilar") {
            GameObject* pilarObj = new GameObject();
            pilarObj->z = spawn.z;

            // 1. Adiciona a arte do pilar
            SpriteRenderer* sprite = new SpriteRenderer(*pilarObj, "Recursos/img/cenario/pilares.png");
            pilarObj->AddComponent(sprite);

            // 2. Adiciona o Efeito Transparente
            pilarObj->AddComponent(new FadeEffect(*pilarObj));

            // A colisão foi feita diretamente no Tiled por ser algo mais complexo (3 pernas)

            // 3. Posicionamento do Tiled
            pilarObj->box.x = spawn.x;
            pilarObj->box.y = spawn.y - pilarObj->box.h;

            AddObject(pilarObj);
        }
        else if (spawn.type == "Escada_Quebrada") {
            GameObject* ladderObj = new GameObject();
            ladderObj->z = spawn.z;
            ladderObj->AddComponent(new SpriteRenderer(*ladderObj, "Recursos/img/cenario/escada_quebrada.png"));
            
            ladderObj->AddComponent(new FadeEffect(*ladderObj, true));

            ladderObj->AddComponent(new Repairable(*ladderObj,
            "Recursos/img/cenario/escada_inteira.png",
            "Tabua de Madeira",
            "Recursos/audio/Hit0.wav",
            120.0f,
            Vec2(1780, 1050)
            ));

            ladderObj->box.x = spawn.x;
            ladderObj->box.y = spawn.y - ladderObj->box.h;

            AddObject(ladderObj);
        }
        else if (spawn.type == "StairTrigger") {
            GameObject* triggerObj = new GameObject();
            
            // Como é um objeto invisível desenhado no Tiled, pegamos as medidas dele
            triggerObj->box.x = spawn.x;
            triggerObj->box.y = spawn.y;
            
            triggerObj->box.w = spawn.w; 
            triggerObj->box.h = spawn.h;

            triggerObj->AddComponent(new StairTrigger(*triggerObj));
            
            AddObject(triggerObj);
        }
        else if (spawn.type == "ItemSpawn") {
            // A classe chama ItemSpawn
            // Porém precisa também da propriedade string "itemName" = "Oil Gallon" (ou qualquer outro item do jogo)
            const ItemDef* foundDef = nullptr;
            for (const auto& def : cfg.pickupCycle) {
                if (def.name == spawn.customString) {
                    foundDef = &def;
                    break;
                }   
            }

            // Também aceita a lanterna inicial pelo nome
            if (!foundDef && cfg.startingFlashlight.name == spawn.customString) {
                foundDef = &cfg.startingFlashlight;
            }

            if (foundDef) {
                int spawnDurability = foundDef->maxDurability;
                if (foundDef->HasProperty(ItemProperty::LIGHT_SOURCE)) {
                    spawnDurability = 1 + (rand() % 100);
                }
                const float itemSize = 48.0f;
                Vec2 tl(spawn.x - itemSize * 0.5f, spawn.y - itemSize);
                tl = ClampPickupTopLeft(tl, itemSize, itemSize);
                ItemPickup* pickup = ItemPickup::Spawn(tl.x, tl.y, *foundDef, spawnDurability, itemPickups);
                if (pickup) {
                    AddObject(&pickup->GetAssociated());
                } 
            }
            else {
                std::cerr << "[ItemSpawn] Item nao encontrado no pickupCycle: '" 
                << spawn.customString << "'. Verifique a propriedade itemName no Tiled." << std::endl;
                }
        }
        else if (spawn.type == "PlayerSpawn_Big") {
            bigSpawnX = spawn.x;
            bigSpawnY = spawn.y - bigObject->box.h;
            bigFoundInTiled = true;
        }
        else if (spawn.type == "PlayerSpawn_Small") {
            smallSpawnX = spawn.x;
            smallSpawnY = spawn.y - smallObject->box.h;
            smallFoundInTiled = true;
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
