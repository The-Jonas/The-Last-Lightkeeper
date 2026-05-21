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
    // SPAWN FIXO DOS IRMÃOS NO CENTRO DO MAPA
    // ==========================================
    // Sabendo que o mapa tem 4500x4000, o centro é a metade disso.

    const float centerX = cfg.navWorldW / 2.0f;
    const float centerY = cfg.navWorldH / 2.0f;

    if (bigObject) {
        bigObject->box.x = centerX - (bigObject->box.w * 0.5f);
        bigObject->box.y = centerY - (bigObject->box.h * 0.5f);
    }

    if (smallObject) {
        // O irmãozinho nasce um pouco pro lado para não nascerem grudados
        smallObject->box.x = bigObject->box.x - std::max(40.0f, smallObject->box.w * 1.2f);
        smallObject->box.y = centerY - (smallObject->box.h * 0.5f);
    }

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
    }

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

    constexpr float kItemPickupSize = 48.0f;
    constexpr float kItemPickupHalf = kItemPickupSize * 0.5f;

    std::vector<std::pair<int, int>> walkablePositions;
    if (tileMapComp) {
        for (int ty = 0; ty < tileMapComp->GetHeight(); ty++) {
            for (int tx = 0; tx < tileMapComp->GetWidth(); tx++) {
                int tid = tileMapComp->At(tx, ty, 1);
                if (walkableTileIds.count(tid)) {
                    Vec2 c = TileCenterToWorld(tx, ty);
                    if (c.x >= mapOrigin.x + kItemPickupHalf && c.x <= mapOrigin.x + levelWorldW - kItemPickupHalf &&
                        c.y >= mapOrigin.y + kItemPickupHalf && c.y <= mapOrigin.y + levelWorldH - kItemPickupHalf) {
                        walkablePositions.push_back({tx, ty});
                    }
                }
            }
        }
    } else if (navGridWidthTiles > 0 && navGridHeightTiles > 0 && bigCharacterObject) {
        // Mapa só com imagens + JSON: usa a grade de navegação e o mesmo teste de passagem que o personagem.
        for (int ty = 0; ty < navGridHeightTiles; ty++) {
            for (int tx = 0; tx < navGridWidthTiles; tx++) {
                Vec2 c = TileCenterToWorld(tx, ty);
                if (c.x < mapOrigin.x + kItemPickupHalf || c.x > mapOrigin.x + levelWorldW - kItemPickupHalf ||
                    c.y < mapOrigin.y + kItemPickupHalf || c.y > mapOrigin.y + levelWorldH - kItemPickupHalf) {
                    continue;
                }
                if (IsTileNavigableFor(bigCharacterObject, tx, ty)) {
                    walkablePositions.push_back({tx, ty});
                }
            }
        }
    }

    const int itemPickupTargetCount = cfg.itemPickupCount;
    if (static_cast<int>(walkablePositions.size()) >= itemPickupTargetCount) {
        srand(static_cast<unsigned>(time(nullptr)));
        for (int i = static_cast<int>(walkablePositions.size()) - 1; i > 0; i--) {
            int j = rand() % (i + 1);
            std::swap(walkablePositions[i], walkablePositions[j]);
        }

        for (int i = 0; i < itemPickupTargetCount && i < static_cast<int>(walkablePositions.size()); i++) {
            int tx = walkablePositions[i].first;
            int ty = walkablePositions[i].second;
            Vec2 worldPos = TileCenterToWorld(tx, ty);
            Vec2 tl(worldPos.x - kItemPickupHalf, worldPos.y - kItemPickupHalf);
            tl = ClampPickupTopLeft(tl, kItemPickupSize, kItemPickupSize);
            const ItemDef& def = cfg.pickupCycle[static_cast<size_t>(i) % cfg.pickupCycle.size()];
            int spawnDurability = def.maxDurability;
            if (def.HasProperty(ItemProperty::LIGHT_SOURCE)) {
                spawnDurability = 1 + (rand() % 100);
            }
            ItemPickup* pickup = ItemPickup::Spawn(tl.x, tl.y, def, spawnDurability, itemPickups);
            if (pickup) {
                AddObject(&pickup->GetAssociated());
            }
        }
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
