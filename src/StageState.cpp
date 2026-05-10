#include "../include/StageState.h"
#include "../include/Game.h"
#include "../include/GameObject.h"
#include "../include/SpriteRenderer.h"
#include "../include/Rect.h"
#include "../include/TileSet.h"
#include "../include/TileMap.h"
#include "../include/InputManager.h"
#include "../include/Camera.h"
#include "../include/Component.h"
#include "../include/Character.h"
#include "../include/Collider.h"
#include "../include/Collision.h"
#include "../include/GameData.h"
#include "../include/EndState.h"
#include "../include/Text.h"
#include "../include/TopDownLightShadows.h"
#include "../include/Item.h"
#include "../include/ItemPickup.h"
#include "../include/HotbarComponent.h"
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

namespace {
void SetMouseConfinedToWindow(bool shouldConfine) {
    SDL_Window* window = Game::GetInstance().GetWindow();
    if (!window) {
        return;
    }

    SDL_SetWindowGrab(window, shouldConfine ? SDL_TRUE : SDL_FALSE);
    if (shouldConfine) {
        SDL_WarpMouseInWindow(window, Game::GetInstance().GetWindowsWidth() / 2, Game::GetInstance().GetWindowsHeight() / 2);
    }
}

float Clamp01(float v) {
    return std::max(0.0f, std::min(1.0f, v));
}

void DrawDebugCircle(SDL_Renderer* renderer, float cx, float cy, float r, Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha) {
    if (!renderer || r <= 1.0f) {
        return;
    }
    SDL_BlendMode oldBlend;
    SDL_GetRenderDrawBlendMode(renderer, &oldBlend);
    Uint8 dr, dg, db, da;
    SDL_GetRenderDrawColor(renderer, &dr, &dg, &db, &da);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, red, green, blue, alpha);
    constexpr int kSeg = 72;
    for (int i = 0; i < kSeg; i++) {
        const float a0 = (static_cast<float>(i) / static_cast<float>(kSeg)) * 2.0f * static_cast<float>(M_PI);
        const float a1 = (static_cast<float>(i + 1) / static_cast<float>(kSeg)) * 2.0f * static_cast<float>(M_PI);
        const int x0 = static_cast<int>(std::lround(cx + std::cos(a0) * r));
        const int y0 = static_cast<int>(std::lround(cy + std::sin(a0) * r));
        const int x1 = static_cast<int>(std::lround(cx + std::cos(a1) * r));
        const int y1 = static_cast<int>(std::lround(cy + std::sin(a1) * r));
        SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
    }
    SDL_SetRenderDrawBlendMode(renderer, oldBlend);
    SDL_SetRenderDrawColor(renderer, dr, dg, db, da);
}

void DrawDebugRect(SDL_Renderer* renderer, const Rect& rect, Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha) {
    if (!renderer || rect.w <= 0.0f || rect.h <= 0.0f) {
        return;
    }
    SDL_BlendMode oldBlend;
    SDL_GetRenderDrawBlendMode(renderer, &oldBlend);
    Uint8 dr, dg, db, da;
    SDL_GetRenderDrawColor(renderer, &dr, &dg, &db, &da);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, red, green, blue, alpha);
    SDL_FRect rf{rect.x, rect.y, rect.w, rect.h};
    SDL_RenderDrawRectF(renderer, &rf);
    SDL_SetRenderDrawBlendMode(renderer, oldBlend);
    SDL_SetRenderDrawColor(renderer, dr, dg, db, da);
}

void DrawDebugCross(SDL_Renderer* renderer, float x, float y, float halfSize, Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha) {
    if (!renderer || halfSize < 1.0f) {
        return;
    }
    SDL_BlendMode oldBlend;
    SDL_GetRenderDrawBlendMode(renderer, &oldBlend);
    Uint8 dr, dg, db, da;
    SDL_GetRenderDrawColor(renderer, &dr, &dg, &db, &da);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, red, green, blue, alpha);
    SDL_RenderDrawLine(renderer, static_cast<int>(std::lround(x - halfSize)), static_cast<int>(std::lround(y)),
                       static_cast<int>(std::lround(x + halfSize)), static_cast<int>(std::lround(y)));
    SDL_RenderDrawLine(renderer, static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y - halfSize)),
                       static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y + halfSize)));
    SDL_SetRenderDrawBlendMode(renderer, oldBlend);
    SDL_SetRenderDrawColor(renderer, dr, dg, db, da);
}

void DrawPlayerShadowTouchDebug(SDL_Renderer* renderer, GameObject* go, Uint8 red, Uint8 green, Uint8 blue) {
    if (!renderer || !go) {
        return;
    }
    const Rect& b = go->box;
    const float z = Camera::GetZoom();
    const Rect screenRect((b.x - Camera::pos.x) * z, (b.y - Camera::pos.y) * z, b.w * z, b.h * z);
    DrawDebugRect(renderer, screenRect, red, green, blue, 190);
    const float footX = (b.x + 0.5f * b.w - Camera::pos.x) * z;
    const float footY = (b.y + b.h - Camera::pos.y) * z;
    DrawDebugCross(renderer, footX, footY, std::max(3.0f, 5.0f * z), red, green, blue, 240);
}

float ComputeShadowDistanceRate(const Vec2& pointScreen, const Vec2& lightScreenPos, const LightMaskParams& params,
                                float* outDistancePx = nullptr, float* outMaxDistancePx = nullptr) {
    const float visualRadius = std::max(8.0f, params.falloffRadiusPx) * std::max(0.4f, params.fatorDicaDeRaio);
    const float maxShadowDist = std::max(1.0f, visualRadius);
    const float d = pointScreen.Distance(lightScreenPos);
    if (outDistancePx) {
        *outDistancePx = d;
    }
    if (outMaxDistancePx) {
        *outMaxDistancePx = maxShadowDist;
    }
    return Clamp01(1.0f - (d / maxShadowDist));
}

bool IsFootLit(GameObject* go, const Vec2& lightScreenPos, const LightMaskParams& params, float* outIntensity = nullptr) {
    if (!go) {
        return false;
    }
    const Rect& b = go->box;
    const Vec2 footWorld(b.x + 0.5f * b.w, b.y + b.h);
    const Vec2 footScreen((footWorld.x - Camera::pos.x) * Camera::GetZoom(), (footWorld.y - Camera::pos.y) * Camera::GetZoom());
    const float rate = ComputeShadowDistanceRate(footScreen, lightScreenPos, params);
    if (outIntensity) {
        *outIntensity = rate;
    }
    return rate > 0.0f;
}

void RenderProjectedSpriteShadow(GameObject* go, const Vec2& lightScreenPos, float lightTouch, float shadowLengthPx, Uint8 shadowAlpha,
                                 const LightMaskParams& params) {
    if (!go || lightTouch <= 0.01f || shadowLengthPx <= 1.0f) {
        return;
    }
    SpriteRenderer* sprite = go->GetComponent<SpriteRenderer>();
    if (!sprite) {
        return;
    }

    const Rect originalBox = go->box;
    const double originalAngle = go->angleDeg;

    const Vec2 foot(go->box.x + 0.5f * go->box.w, go->box.y + go->box.h);
    const Vec2 lightWorld(lightScreenPos.x / Camera::GetZoom() + Camera::pos.x, lightScreenPos.y / Camera::GetZoom() + Camera::pos.y);
    Vec2 dir = foot - lightWorld;
    if (dir.Magnitude() < 1e-3f) {
        return;
    }
    dir = dir.Normalized();

    const float distance01 = Clamp01(1.0f - lightTouch); // 0 = close light, 1 = far light
    const float fastStretch = std::pow(distance01, 0.60f);
    // Pure distance-rate sizing: no min/max user constraints.
    const float stretch = 0.82f + fastStretch * 2.35f;
    const float widen = 1.00f + fastStretch * 0.26f;

    Rect shadowBox = originalBox;
    shadowBox.w = std::max(2.0f, originalBox.w * widen);
    shadowBox.h = std::max(2.0f, originalBox.h * stretch);
    const double angDeg = std::atan2(dir.y, dir.x) * (180.0 / M_PI) + 90.0;
    const double angRad = angDeg * (M_PI / 180.0);
    // Lock shadow "feet" to character feet: rotate/stretch around anchored foot.
    const Vec2 footAnchor(originalBox.x + 0.5f * originalBox.w, originalBox.y + originalBox.h);
    const Vec2 localFootToCenter(0.0f, -shadowBox.h * 0.5f);
    const Vec2 rotatedFootToCenter(localFootToCenter.x * std::cos(angRad) - localFootToCenter.y * std::sin(angRad),
                                   localFootToCenter.x * std::sin(angRad) + localFootToCenter.y * std::cos(angRad));
    const Vec2 center = footAnchor + rotatedFootToCenter;
    shadowBox.x = center.x - shadowBox.w * 0.5f;
    shadowBox.y = center.y - shadowBox.h * 0.5f;

    go->box = shadowBox;
    go->angleDeg = angDeg;
    const Uint8 spriteShadowAlpha = static_cast<Uint8>(std::max(18, std::min(205, static_cast<int>(shadowAlpha))));
    sprite->SetTint(0, 0, 0, spriteShadowAlpha);
    go->Render();

    go->box = originalBox;
    go->angleDeg = originalAngle;
}

void DrawContactFootShadow(SDL_Renderer* renderer, const Rect& box, float contactRate) {
    if (!renderer || contactRate <= 0.0f) {
        return;
    }
    const float z = Camera::GetZoom();
    const float footX = (box.x + 0.5f * box.w - Camera::pos.x) * z;
    const float footY = (box.y + box.h - Camera::pos.y) * z;
    const float base = std::max(2.0f, std::min(box.w, box.h) * z);
    const float r = base * (0.14f + 0.16f * Clamp01(contactRate));
    if (r <= 0.5f) {
        return;
    }

    SDL_BlendMode oldBlend;
    SDL_GetRenderDrawBlendMode(renderer, &oldBlend);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    const int seg = 24;
    const int blurLayers = 4;
    const float baseAlpha = std::max(24.0f, std::min(220.0f, 255.0f * contactRate));
    for (int layer = 0; layer < blurLayers; layer++) {
        const float tLayer = static_cast<float>(layer) / static_cast<float>(std::max(1, blurLayers - 1));
        const float layerRadius = r * (1.0f + 0.45f * tLayer);
        const float layerAlphaF = baseAlpha * std::pow(1.0f - tLayer, 1.7f);
        if (layerAlphaF < 1.0f) {
            continue;
        }
        SDL_Color col{0, 0, 0, static_cast<Uint8>(layerAlphaF)};
        std::vector<SDL_Vertex> verts;
        std::vector<int> inds;
        verts.reserve(static_cast<size_t>(seg + 2));
        inds.reserve(static_cast<size_t>(seg * 3));
        verts.push_back({{footX, footY}, col, {0, 0}});
        for (int i = 0; i <= seg; i++) {
            const float t = (static_cast<float>(i) / static_cast<float>(seg)) * 2.0f * static_cast<float>(M_PI);
            verts.push_back({{footX + std::cos(t) * layerRadius, footY + std::sin(t) * layerRadius}, col, {0, 0}});
        }
        for (int i = 1; i <= seg; i++) {
            inds.push_back(0);
            inds.push_back(i);
            inds.push_back(i + 1);
        }
        SDL_RenderGeometry(renderer, nullptr, verts.data(), static_cast<int>(verts.size()), inds.data(),
                           static_cast<int>(inds.size()));
    }

    SDL_SetRenderDrawBlendMode(renderer, oldBlend);
}

float BottomYOf(const GameObject* go) {
    if (!go) {
        return 0.0f;
    }
    return go->box.y + go->box.h;
}
}

StageState::StageState() {
    levelTracks = {
        "Recursos/audio/soundtracks/Akane's Regret.mp3",
        "Recursos/audio/soundtracks/Last Hideout.mp3",
        "Recursos/audio/soundtracks/Unworn - When will this end_.mp3",
    };
    currentTrack = 0;
    music.Open(levelTracks[currentTrack]);
    music.Play();
    Mix_VolumeMusic((MIX_MAX_VOLUME * Game::masterVolumePercent) / 100); // Default: ON
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
}

StageState::~StageState(){                                
    // O destrutor de State cuida de limpar o objectArray
    // A música é limpa pelo destrutor de Music
    SetMouseConfinedToWindow(false);
    delete radialGeometry;
    radialGeometry = nullptr;
    lightTweakPanel.reset();
}

void StageState::LoadAssets() {

    // OBS: TOMAR CUIDADO NA ORDEM EM QUE CARREGAMOS OS COMPONENTES, POIS MUITO PROVAVELMENTE ISSO É A CAUSA DE ESTAREM SUMINDO, UM É DESENHADO POR CIMA DO OUTRO

    // // Criação do Background como Component e GameObject
    // GameObject* bgObject = new GameObject();            // Criação de um ponteiro de GameObject
    // SpriteRenderer* bgSprite = new SpriteRenderer(*bgObject, "Recursos/img/Background.png");        // Criando um spriteRenderer com os mesmos parâmetros de antes
    //
    // bgObject->AddComponent(bgSprite);                   // Adiciona o SpriteRenderer ao GameObject
    //
    // bgSprite->SetCameraFollower(true);                  // Por conta o setCameraFollower, agora a imagem não fica mais por baixo do TileSet sumida, agora o background segue a câmera
    //
    // bgObject->box.x = 0;                                // background colocado em (0, 0)
    // bgObject->box.y = 0;
    // bgObject->z = 0;                                    // Z = 0 (Camada mais ao fundo)
    // AddObject(bgObject);                                // Colocando o ponteiro para o GameObject no ObjectArray usando AddObject
    //
    //------------------------------------------

    // Criação do TileSet (64x64 tiles, 7 colunas)
    dungeonTileSet = std::make_unique<TileSet>(64, 64, "Recursos/img/Tileset.png");
    tileSet = dungeonTileSet.get();

    GameObject* mapObject = new GameObject();                                           // Criando o GameObject para o TileMap
    TileMap* tileMap = new TileMap(*mapObject, "Recursos/map/level_from_json.txt", tileSet); // Nível exportado do level.json (52x52)
    mapObject->AddComponent(tileMap);                                                   // Adicionando o TileMap ao GameObject
    tileMapComp = tileMap;

    // Define os multiplicador9s de parallax
    tileMap->SetParallax(0, -0.2f);                                                     // Camada 0 (ao fundo) se move mais devagar (na direção oposta?)
    tileMap->SetParallax(1, 0.0f);                                                      // Camada 1 (o chão) se move na velocidade normal
    
    mapObject->box.x = 0;                                                               // Definindo a posição
    mapObject->box.y = 0;                                                               // do mapa no jogo
    mapOrigin = Vec2(mapObject->box.x, mapObject->box.y);
    mapObject->z = 1;                                                                   // // Z = 1 (Camada do mapa, acima do fundo)
    AddObject(mapObject);                                                               // Adicionando o GameObject do mapa ao array de objetos

    if (tileMapComp) {
        tileMapComp->BuildLightOcclusionFromLayer(1, walkableTileIds);
        TopDownLightShadows::BuildShadowEdgesFromSolidGrid(
            tileMapComp->GetWidth(), tileMapComp->GetHeight(),
            static_cast<float>(tileSet->GetTileWidth()), static_cast<float>(tileSet->GetTileHeight()),
            mapObject->box.x, mapObject->box.y, tileMapComp->GetLightOcclusionSolid(), staticShadowEdges);
        staticShadowEdgesBuilt = !staticShadowEdges.empty();
    }

    //------------------------------------------
    
    // Criação dos dois personagens controláveis
    // Personagem grande (IRMÃOZÃO)
    GameObject* bigObject = new GameObject();
    Character* bigComp = new Character(*bigObject, "Recursos/img/Player.png");
    bigObject->AddComponent(bigComp);
    bigObject->z = 2;
    AddObject(bigObject);

    // Personagem pequeno (IRMÃOZINHO)
    GameObject* smallObject = new GameObject();
    Character* smallComp = new Character(*smallObject, "Recursos/img/Player.png");
    smallObject->AddComponent(smallComp);
    smallObject->z = 2;
    AddObject(smallObject);

    // Configurações do personagem pequeno (IRMÃOZINHO)
    SpriteRenderer* smallSprite = smallObject->GetComponent<SpriteRenderer>();
    if (smallSprite) {
        smallSprite->SetScale(0.72f, 0.72f);
        smallSprite->SetTint(150, 200, 255, 240);
    }
    smallComp->SetBaseSpeed(275.0f);

    // Spawn dinâmico: centro do mapa atual (horizontal + vertical).
    if (tileMapComp && tileSet) {
        const float mapWpx = static_cast<float>(tileMapComp->GetWidth() * tileSet->GetTileWidth());
        const float mapHpx = static_cast<float>(tileMapComp->GetHeight() * tileSet->GetTileHeight());
        const float mapLeft = mapOrigin.x;
        const float mapTop = mapOrigin.y;
        const float mapRight = mapLeft + mapWpx;
        const float mapBottom = mapTop + mapHpx;

        const float centerX = mapLeft + mapWpx * 0.5f;
        const float centerY = mapTop + mapHpx * 0.5f;

        bigObject->box.x = centerX - (bigObject->box.w * 0.5f);
        bigObject->box.y = centerY - (bigObject->box.h * 0.5f);

        // Companheiro nasce próximo ao jogador, sem sobrepor.
        smallObject->box.x = bigObject->box.x - std::max(18.0f, smallObject->box.w * 0.9f);
        smallObject->box.y = centerY - (smallObject->box.h * 0.5f);

        auto clampInsideMap = [&](GameObject* go) {
            if (!go) return;
            go->box.x = std::max(mapLeft, std::min(go->box.x, mapRight - go->box.w));
            go->box.y = std::max(mapTop, std::min(go->box.y, mapBottom - go->box.h));
        };
        clampInsideMap(bigObject);
        clampInsideMap(smallObject);
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
    hudLine1->AddComponent(new Text(*hudLine1, "Recursos/font/TradeWinds-Regular.ttf", 18, Text::BLENDED, "TAB: trocar | E: pegar | I: inventario", hudColor));
    AddObject(hudLine1);

    // Linha 2 de instruções
    hudLine2 = new GameObject();
    hudLine2->z = 100;
    hudLine2->AddComponent(new Text(*hudLine2, "Recursos/font/TradeWinds-Regular.ttf", 18, Text::BLENDED, "F: alternar entre junto e independente", hudColor));
    AddObject(hudLine2);

    hudLine3 = new GameObject();
    hudLine3->z = 100;
    hudLine3->AddComponent(new Text(*hudLine3, "Recursos/font/TradeWinds-Regular.ttf", 18, Text::BLENDED,
                                      "K forma | C criar luz | P painel | L luz | O sombras", hudColor));
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

    ItemDef kApple{"Apple", "Recursos/img/items/apple.png", -1, false, 1, {}};
    ItemDef kBrokenFlashlight{"Broken Flashlight", "Recursos/img/items/flashlight_broken.png", 0, false, 2, {}};
    ItemDef kOilGallon{"Oil Gallon", "Recursos/img/items/oil_gallon.png", 100, false, 3, {}};
    ItemDef itemDefs[3] = {kApple, kBrokenFlashlight, kOilGallon};

    std::vector<std::pair<int,int>> walkablePositions;
    if (tileMapComp) {
        for (int ty = 0; ty < tileMapComp->GetHeight(); ty++) {
            for (int tx = 0; tx < tileMapComp->GetWidth(); tx++) {
                int tid = tileMapComp->At(tx, ty, 1);
                if (walkableTileIds.count(tid)) {
                    walkablePositions.push_back({tx, ty});
                }
            }
        }
    }

    const int kItemCount = 35;
    if (static_cast<int>(walkablePositions.size()) >= kItemCount) {
        srand(static_cast<unsigned>(time(nullptr)));
        for (int i = static_cast<int>(walkablePositions.size()) - 1; i > 0; i--) {
            int j = rand() % (i + 1);
            std::swap(walkablePositions[i], walkablePositions[j]);
        }

        for (int i = 0; i < kItemCount && i < static_cast<int>(walkablePositions.size()); i++) {
            int tx = walkablePositions[i].first;
            int ty = walkablePositions[i].second;
            Vec2 worldPos = TileCenterToWorld(tx, ty);
            const ItemDef& def = itemDefs[i % 3];
            ItemPickup* pickup = ItemPickup::Spawn(worldPos.x - 32.0f, worldPos.y - 32.0f,
                                                     def, def.maxDurability, itemPickups);
            if (pickup) {
                AddObject(&pickup->GetAssociated());
            }
        }
    }

    GameObject* hotbarObj = new GameObject();
    hotbarObj->AddComponent(new HotbarComponent(*hotbarObj, inventory, bigCharacter,
                                                  &controlledCharacter, itemPickups,
                                                  [this](GameObject* obj) { AddObject(obj); }));
    hotbarObj->z = 200;
    AddObject(hotbarObj);
    hotbarObject = hotbarObj;

    RefreshCameraTargets(); // Atualiza alvos da câmera (dupla + principal)

}

void StageState::Update(float dt){
    InputManager& input = InputManager::GetInstance();
    Vec2 prevBigPos(0.0f, 0.0f);
    Vec2 prevSmallPos(0.0f, 0.0f);
    if (bigCharacterObject) {
        prevBigPos = Vec2(bigCharacterObject->box.x, bigCharacterObject->box.y);
    }
    if (smallCharacterObject) {
        prevSmallPos = Vec2(smallCharacterObject->box.x, smallCharacterObject->box.y);
    }

    // QuitRequested (Clicar no X da janela) -> Fecha o jogo
    if (input.QuitRequested()) {
        quitRequested = true;
    }

    // ESC -> Volta para o menu
    if (input.KeyPress(ESCAPE_KEY)) {
        popRequested = true;
    }

    if (input.KeyPress(LIGHTS_TOGGLE_KEY)) {
        lightsEnabled = !lightsEnabled;
    }
    if (input.KeyPress(SHADOWS_TOGGLE_KEY)) {
        shadowsEnabled = !shadowsEnabled;
    }
    if (input.KeyPress(MUSIC_MUTE_TOGGLE_KEY)) {
        musicMuted = !musicMuted;
        const int masterVolume = (MIX_MAX_VOLUME * Game::masterVolumePercent) / 100;
        Mix_VolumeMusic(musicMuted ? 0 : masterVolume);
    }

    if (!Mix_PlayingMusic() && !musicMuted && !levelTracks.empty()) {
        currentTrack = (currentTrack + 1) % static_cast<int>(levelTracks.size());
        music.Open(levelTracks[currentTrack]);
        music.Play();
    }

    if (input.KeyPress(CREATE_LIGHT_KEY) &&
        (lightMaskShape == LightMaskShape::Circle || lightMaskShape == LightMaskShape::Torch)) {
        CreateLightAtCursor();
    }

    if (IsPartyReady()) {
        HandlePartyInput();
        IssueMovementFromInput(controlledCharacter, controlledCharacterObject);
        UpdateCompanionBehavior();
    }

    UpdateArray(dt);                                                                    // Percorre o vetor de GameObjects chamando o Update de cada um
    ApplyMapBoundsAndWalkability(bigCharacterObject, prevBigPos);
    ApplyMapBoundsAndWalkability(smallCharacterObject, prevSmallPos);
    if (IsPartyReady()) {
        EnforceMaxDistance();
        ApplyMapBoundsAndWalkability(bigCharacterObject, prevBigPos);
        ApplyMapBoundsAndWalkability(smallCharacterObject, prevSmallPos);
        UpdateControlledCharacterVisuals();
        RefreshCameraTargets();
    }
    Camera::Update(dt);                                                                 // Atualizando a camera cada iteração do gameloop
    UpdateHudInstructions();

    if (lightTweakPanel) {
        lightTweakPanel->Update(input, dt, Game::GetInstance().GetWindowsWidth(), Game::GetInstance().GetWindowsHeight());
        if (lightTweakPanel->ConsumeCreateLightRequest()) {
            CreateLightAtCursor();
        }
    }

    // FPS monitor: suaviza leitura e atualiza HUD periodicamente para evitar custo de textura a cada frame.
    if (dt > 1e-6f) {
        const float instantFps = 1.0f / dt;
        const float smoothAlpha = 0.10f; // EMA simples para estabilidade visual
        fpsSmoothed += (instantFps - fpsSmoothed) * smoothAlpha;
    }
    fpsUiRefreshTimer += dt;
    if (hudFps && fpsUiRefreshTimer >= 0.10f) {
        fpsUiRefreshTimer = 0.0f;
        Text* fpsText = hudFps->GetComponent<Text>();
        if (fpsText) {
            const float instantFps = (dt > 1e-6f) ? (1.0f / dt) : 0.0f;
            const bool steady = std::fabs(instantFps - fpsSmoothed) <= 2.0f;
            SDL_Color fpsColor = {220, 80, 80, 240}; // vermelho: queda forte
            const char* quality = "LOW";
            if (fpsSmoothed >= 58.0f && steady) {
                fpsColor = {90, 235, 120, 240};      // verde: saudável + estável
                quality = "HEALTHY";
            } else if (fpsSmoothed >= 50.0f) {
                fpsColor = {245, 210, 90, 240};      // amarelo: aceitável, com quedas
                quality = steady ? "OK" : "UNSTEADY";
            }

            char fpsBuffer[64];
            std::snprintf(fpsBuffer, sizeof(fpsBuffer), "FPS: %.1f (%s)", fpsSmoothed, quality);
            fpsText->SetColor(fpsColor);
            fpsText->SetText(fpsBuffer);
        }
    }

    if (IsPartyReady() && input.MousePress(SDL_BUTTON_RIGHT)) {
        const int mx = input.GetMouseX();
        const int my = input.GetMouseY();
        const float z = Camera::GetZoom();
        auto screenRectOf = [&](const GameObject* go) -> Rect {
            const Rect& b = go->box;
            return Rect((b.x - Camera::pos.x) * z, (b.y - Camera::pos.y) * z, b.w * z, b.h * z);
        };
        const Vec2 mouse(static_cast<float>(mx), static_cast<float>(my));
        const bool onBig = bigCharacterObject && screenRectOf(bigCharacterObject).Contains(mouse);
        const bool onSmall = smallCharacterObject && screenRectOf(smallCharacterObject).Contains(mouse);
        if (onBig || onSmall) {
            if (onBig && onSmall) {
                const Vec2 mb = bigCharacterObject->box.Center();
                const Vec2 ms = smallCharacterObject->box.Center();
                const Vec2 mworld = ScreenToWorld(mouse);
                previewLightAnchorPlayer =
                    (mworld.Distance(mb) <= mworld.Distance(ms)) ? bigCharacterObject : smallCharacterObject;
            } else {
                previewLightAnchorPlayer = onBig ? bigCharacterObject : smallCharacterObject;
            }
            previewLightLockedToPlayer = true;
        } else {
            previewLightLockedToPlayer = false;
            previewLightAnchorPlayer = nullptr;
        }
    }

    Vec2 targetLightScreen(static_cast<float>(input.GetMouseX()), static_cast<float>(input.GetMouseY()));
    if (previewLightLockedToPlayer) {
        if (!IsPartyReady() || (previewLightAnchorPlayer != bigCharacterObject && previewLightAnchorPlayer != smallCharacterObject)) {
            previewLightLockedToPlayer = false;
            previewLightAnchorPlayer = nullptr;
        }
    }
    if (previewLightLockedToPlayer && previewLightAnchorPlayer) {
        // Keep the preview light at the feet (screen space): maximizes contact shadow, avoids long body shadow.
        const Rect& ab = previewLightAnchorPlayer->box;
        targetLightScreen = WorldToScreen(Vec2(ab.x + 0.5f * ab.w, ab.y + ab.h));
    }

    {
        Vec2 tw = ScreenToWorld(targetLightScreen);
        int tx, ty;
        if (WorldToTile(tw, tx, ty) && !IsTileWalkable(tx, ty)) {
            int ntx, nty;
            if (FindNearestWalkableTile(tx, ty, ntx, nty)) {
                Vec2 clampedWorld = TileCenterToWorld(ntx, nty);
                if (hasSmoothedDynamicLight) {
                    Vec2 currentWorld = ScreenToWorld(smoothedDynamicLightScreenPos);
                    if (!HasWalkableLine(currentWorld, clampedWorld)) {
                        targetLightScreen = smoothedDynamicLightScreenPos;
                    } else {
                        targetLightScreen = WorldToScreen(clampedWorld);
                    }
                } else {
                    targetLightScreen = WorldToScreen(clampedWorld);
                }
            }
        }
    }

    if (!hasSmoothedDynamicLight) {
        smoothedDynamicLightScreenPos = targetLightScreen;
        hasSmoothedDynamicLight = true;
    } else {
        const float s = std::max(0.01f, std::min(0.95f, lightMaskParams.lightTemporalSmoothing));
        const float lerpA = 1.0f - std::pow(1.0f - s, dt * 60.0f);
        smoothedDynamicLightScreenPos = smoothedDynamicLightScreenPos + (targetLightScreen - smoothedDynamicLightScreenPos) * lerpA;
    }

    // Loop duplo para testar pares de objetos
    for (size_t i = 0; i < objectArray.size(); i++) {
        // Para garantir que um par só é testado uma vez, usamos 'j' começando em 'i + 1'
        for (size_t j = i + 1; j < objectArray.size(); j++) {
            // Pega os GameObjects
            GameObject* goA = objectArray[i].get();
            GameObject* goB = objectArray[j].get();

            // Somente os que tem o componente collider
            Collider* colliderA = goA->GetComponent<Collider>();
            Collider* colliderB = goB->GetComponent<Collider>();

            if (colliderA && colliderB) { //..Se ambos existirem
                // "Use a box dos Colliders mas a rotação do GameObject"
                // Lembrando que isColliding espera radianos e meu angleDeg é em graus
                float angleDegA = goA->angleDeg * (M_PI / 180.0);
                float angleDegB = goB->angleDeg * (M_PI / 180.0);

                if (Collision::IsColliding(colliderA->box, colliderB->box, angleDegA, angleDegB)) {
                    // "Se houver colisão, notifique ambos os GameObjects"
                    goA->NotifyCollision(*goB);
                    goB->NotifyCollision(*goA);
                }
            }
        }    
    }

    for (size_t i = 0; i < objectArray.size();) {               
        if(objectArray[i]->IsDead()) {                          // Se o GameObject está morto 
            objectArray.erase(objectArray.begin() + i);         // Remova-o do array (Com iterador do ínicio somado á posição do elemento)
        } else {
            i++;                                                // Se não, avança para o próximo
        }
    }

    // VERIFICAÇÃO DE FIM DE JOGO

    // 1. Derrota: Jogador morreu
    if (!IsPartyReady()) {
        GameData::playerVictory = false;                        // Se o ponteiro é nulo, o jogador morreu

        // Empilha a tela de fim e pausa a fase atual
        popRequested = true;                                    // Remove o StageState atual
        Game::GetInstance().Push(new EndState());
    }
}

void StageState::Render(){
    // Implementação Z/Y sorting
    
    // 1. Defina a função de comparação (lambda)
    auto compareObjects = [](const std::shared_ptr<GameObject>& a, const std::shared_ptr<GameObject>& b) {
        
        // Regra 1: Z-Sorting (Profundidade)
        if (a->z != b->z) {
            return a->z < b->z;
        }

        // Regra 2: Y-Sorting (Usando o Y do "dono" se houver)
        // Se o objeto tem um 'owner', ele usa o Y-base do owner para a ordenação.
        float a_bottom_y = (a->owner != nullptr) ? (a->owner->box.y + a->owner->box.h) : (a->box.y + a->box.h);
        float b_bottom_y = (b->owner != nullptr) ? (b->owner->box.y + b->owner->box.h) : (b->box.y + b->box.h);
        
        // Tolerância (epsilon) para comparação de floats
        float epsilon = 0.01f; 

        // Se os Ys são diferentes, ordena pelo Y.
        if (std::abs(a_bottom_y - b_bottom_y) > epsilon) {
            return a_bottom_y < b_bottom_y;
        }

        // Regra 3: Sub-Z Sorting (Desempate)
        // Se os Ys são iguais (ex: Character e sua Gun), usa a sub-camada.
        // O sub_z menor (Character=0) é desenhado primeiro.
        return a->sub_z < b->sub_z;
    };

    // 2. Ordene o objectArray usando a função de comparação
    std::sort(objectArray.begin(), objectArray.end(), compareObjects);
    
    // Fim do sorting

    // Cenário: depois a luz escurece tudo — redesenha HUD (z>=100) por cima
    constexpr int kHudZ = 100;
    for (const auto& go : objectArray) {
        if (go->z < kHudZ && go.get() != bigCharacterObject && go.get() != smallCharacterObject) {
            go->Render();
        }
    }

    Game& g = Game::GetInstance();
    const bool showDebugTools = (lightTweakPanel && lightTweakPanel->visible);
    const bool bigCircleOnlyLight =
        previewLightLockedToPlayer && previewLightAnchorPlayer == bigCharacterObject;
    const bool smallCircleOnlyLight =
        previewLightLockedToPlayer && previewLightAnchorPlayer == smallCharacterObject;
    float bigMaxContact = 0.0f;
    float smallMaxContact = 0.0f;
    if (lightsEnabled && shadowsEnabled) {
        struct SpriteShadowCast {
            Vec2 lightScreen;
            float touch = 0.0f;
            float lengthPx = 0.0f;
            Uint8 alpha = 0;
            float contact = 0.0f;
        };
        std::vector<SpriteShadowCast> bigShadowCasts;
        std::vector<SpriteShadowCast> smallShadowCasts;
        bigShadowCasts.reserve(6);
        smallShadowCasts.reserve(6);

        auto renderShadowsForLight = [&](const Vec2& lightScreen, const LightMaskParams& params) {
            float touchBig = 0.0f;
            float touchSmall = 0.0f;
            IsFootLit(bigCharacterObject, lightScreen, params, &touchBig);
            IsFootLit(smallCharacterObject, lightScreen, params, &touchSmall);
            const float distBig = 1.0f - touchBig;
            const float distSmall = 1.0f - touchSmall;

            float dBigPx = 0.0f, maxBigPx = 1.0f;
            float dSmallPx = 0.0f, maxSmallPx = 1.0f;
            if (bigCharacterObject) {
                const Rect& b = bigCharacterObject->box;
                const Vec2 foot((b.x + 0.5f * b.w - Camera::pos.x) * Camera::GetZoom(),
                                (b.y + b.h - Camera::pos.y) * Camera::GetZoom());
                ComputeShadowDistanceRate(foot, lightScreen, params, &dBigPx, &maxBigPx);
            }
            if (smallCharacterObject) {
                const Rect& b = smallCharacterObject->box;
                const Vec2 foot((b.x + 0.5f * b.w - Camera::pos.x) * Camera::GetZoom(),
                                (b.y + b.h - Camera::pos.y) * Camera::GetZoom());
                ComputeShadowDistanceRate(foot, lightScreen, params, &dSmallPx, &maxSmallPx);
            }

            const float bigContactRadiusPx = std::max(6.0f, maxBigPx * 0.07f);
            const float smallContactRadiusPx = std::max(6.0f, maxSmallPx * 0.07f);
            const float bigContact = (dBigPx <= bigContactRadiusPx) ? Clamp01(1.0f - dBigPx / bigContactRadiusPx) : 0.0f;
            const float smallContact =
                (dSmallPx <= smallContactRadiusPx) ? Clamp01(1.0f - dSmallPx / smallContactRadiusPx) : 0.0f;
            bigMaxContact = std::max(bigMaxContact, bigContact);
            smallMaxContact = std::max(smallMaxContact, smallContact);

            if (touchBig > 0.0f) {
                const float shadowLengthPx = params.shadowMaxLengthPx * distBig;
                const Uint8 shadowAlpha = static_cast<Uint8>(std::max(0.0f, std::min(255.0f, params.darknessMax * touchBig)));
                bigShadowCasts.push_back({lightScreen, touchBig, shadowLengthPx, shadowAlpha, bigContact});
            }
            if (touchSmall > 0.0f) {
                const float shadowLengthPx = params.shadowMaxLengthPx * distSmall;
                const Uint8 shadowAlpha =
                    static_cast<Uint8>(std::max(0.0f, std::min(255.0f, params.darknessMax * touchSmall)));
                smallShadowCasts.push_back({lightScreen, touchSmall, shadowLengthPx, shadowAlpha, smallContact});
            }
        };

        // Preview light also casts shadows for immediate feedback.
        renderShadowsForLight(smoothedDynamicLightScreenPos, lightMaskParams);
        if (showDebugTools) {
            const float previewShadowRadius =
                std::max(24.0f,
                         std::max(8.0f, lightMaskParams.falloffRadiusPx) * std::max(0.4f, lightMaskParams.fatorDicaDeRaio));
            DrawDebugCircle(g.GetRenderer(), smoothedDynamicLightScreenPos.x, smoothedDynamicLightScreenPos.y, previewShadowRadius,
                            255, 210, 90, 130);
        }

        int renderedLights = 0;
        for (const LightInstance& light : lights) {
            if (!light.enabled) {
                continue;
            }
            if (renderedLights >= maxActiveLights) {
                break;
            }

            const Vec2 lightScreen = WorldToScreen(light.worldPos);
            const float cullRadius = std::max(32.0f, light.params.falloffRadiusPx * 1.6f);
            if (lightScreen.x < -cullRadius || lightScreen.y < -cullRadius ||
                lightScreen.x > static_cast<float>(g.GetWindowsWidth()) + cullRadius ||
                lightScreen.y > static_cast<float>(g.GetWindowsHeight()) + cullRadius) {
                continue;
            }

            renderShadowsForLight(lightScreen, light.params);
            if (showDebugTools) {
                const float placedShadowRadius =
                    std::max(24.0f,
                             std::max(8.0f, light.params.falloffRadiusPx) * std::max(0.4f, light.params.fatorDicaDeRaio));
                DrawDebugCircle(g.GetRenderer(), lightScreen.x, lightScreen.y, placedShadowRadius, 120, 220, 255, 95);
            }
            renderedLights++;
        }

        // Sprite-projected player shadows: each relevant nearby light casts its own shadow.
        constexpr size_t kMaxSpriteShadowsPerPlayer = 4;
        std::sort(bigShadowCasts.begin(), bigShadowCasts.end(),
                  [](const SpriteShadowCast& a, const SpriteShadowCast& b) { return a.touch > b.touch; });
        std::sort(smallShadowCasts.begin(), smallShadowCasts.end(),
                  [](const SpriteShadowCast& a, const SpriteShadowCast& b) { return a.touch > b.touch; });
        if (bigShadowCasts.size() > kMaxSpriteShadowsPerPlayer) {
            bigShadowCasts.resize(kMaxSpriteShadowsPerPlayer);
        }
        if (smallShadowCasts.size() > kMaxSpriteShadowsPerPlayer) {
            smallShadowCasts.resize(kMaxSpriteShadowsPerPlayer);
        }
        for (const SpriteShadowCast& c : bigShadowCasts) {
            // If light is right on the feet, keep only the contact circle.
            // While preview light is locked to this player, never draw the stretched sprite shadow for them.
            if (!bigCircleOnlyLight && c.contact < 0.50f) {
                RenderProjectedSpriteShadow(bigCharacterObject, c.lightScreen, c.touch, c.lengthPx, c.alpha, lightMaskParams);
            }
        }
        for (const SpriteShadowCast& c : smallShadowCasts) {
            if (!smallCircleOnlyLight && c.contact < 0.50f) {
                RenderProjectedSpriteShadow(smallCharacterObject, c.lightScreen, c.touch, c.lengthPx, c.alpha, lightMaskParams);
            }
        }
        UpdateControlledCharacterVisuals(); // restore player tint after black sprite-shadow rendering

        if (showDebugTools) {
            // Visualize the player collision box and foot point used by shadow-touch checks.
            DrawPlayerShadowTouchDebug(g.GetRenderer(), bigCharacterObject, 255, 120, 120);
            DrawPlayerShadowTouchDebug(g.GetRenderer(), smallCharacterObject, 130, 220, 255);
        }

        // Contact blob under the feet: draw before the sprite so it sits on the ground, not on top.
        if (bigCharacterObject && bigMaxContact > 0.0f) {
            DrawContactFootShadow(g.GetRenderer(), bigCharacterObject->box, bigMaxContact);
        }
        if (smallCharacterObject && smallMaxContact > 0.0f) {
            DrawContactFootShadow(g.GetRenderer(), smallCharacterObject->box, smallMaxContact);
        }
    }

    if (bigCharacterObject && smallCharacterObject) {
        if (BottomYOf(bigCharacterObject) <= BottomYOf(smallCharacterObject)) {
            bigCharacterObject->Render();
            smallCharacterObject->Render();
        } else {
            smallCharacterObject->Render();
            bigCharacterObject->Render();
        }
    } else {
        if (bigCharacterObject) {
            bigCharacterObject->Render();
        }
        if (smallCharacterObject) {
            smallCharacterObject->Render();
        }
    }

    if (lightsEnabled && radialGeometry != nullptr) {
        std::vector<RadialLightOverlay::ScreenLight> screenLights;
        screenLights.reserve(static_cast<size_t>(maxActiveLights + 1));
        // Dynamic preview light is always present.
        screenLights.push_back({smoothedDynamicLightScreenPos.x, smoothedDynamicLightScreenPos.y, lightMaskShape, lightMaskParams, 0.317f});
        int renderedLights = 0;
        for (LightInstance& light : lights) {
            if (!light.enabled) {
                continue;
            }
            if (renderedLights >= maxActiveLights) {
                break;
            }

            const Vec2 lightScreen = WorldToScreen(light.worldPos);
            const float cullRadius = std::max(32.0f, light.params.falloffRadiusPx * 1.4f);
            if (lightScreen.x < -cullRadius || lightScreen.y < -cullRadius ||
                lightScreen.x > static_cast<float>(g.GetWindowsWidth()) + cullRadius ||
                lightScreen.y > static_cast<float>(g.GetWindowsHeight()) + cullRadius) {
                continue;
            }
            screenLights.push_back({lightScreen.x, lightScreen.y, light.shape, light.params, light.animationSeed});
            renderedLights++;
        }
        LightOcclusionContext occCtx;
        if (tileMapComp && tileSet) {
            occCtx.solidGrid = &tileMapComp->GetLightOcclusionSolid();
            occCtx.mapWidth = tileMapComp->GetWidth();
            occCtx.mapHeight = tileMapComp->GetHeight();
            occCtx.tileWidth = static_cast<float>(tileSet->GetTileWidth());
            occCtx.tileHeight = static_cast<float>(tileSet->GetTileHeight());
            occCtx.mapOriginX = mapOrigin.x;
            occCtx.mapOriginY = mapOrigin.y;
            occCtx.cameraX = Camera::pos.x;
            occCtx.cameraY = Camera::pos.y;
            occCtx.zoom = Camera::GetZoom();
        }
        radialGeometry->RenderMany(g.GetRenderer(), g.GetWindowsWidth(), g.GetWindowsHeight(), screenLights, occCtx);

        if (shadowsEnabled && staticShadowEdgesBuilt && !staticShadowEdges.empty()) {
            const std::vector<TopDownShadowEdge> noDynamic;
            const int maxShadowVolumes = shadowsEnabled ? 4 : 0;
            for (int si = 0; si < static_cast<int>(screenLights.size()) && si < maxShadowVolumes; si++) {
                const RadialLightOverlay::ScreenLight& sl = screenLights[si];

                if (occCtx.IsEnabled()) {
                    const float lxWorld = sl.x / occCtx.zoom + occCtx.cameraX;
                    const float lyWorld = sl.y / occCtx.zoom + occCtx.cameraY;
                    const int ltx = static_cast<int>((lxWorld - occCtx.mapOriginX) / occCtx.tileWidth);
                    const int lty = static_cast<int>((lyWorld - occCtx.mapOriginY) / occCtx.tileHeight);
                    if (ltx >= 0 && ltx < occCtx.mapWidth && lty >= 0 && lty < occCtx.mapHeight) {
                        if ((*occCtx.solidGrid)[static_cast<size_t>(ltx + lty * occCtx.mapWidth)] != 0) {
                            continue;
                        }
                    }
                }

                TopDownLightShadows::RenderShadowVolumes(
                    g.GetRenderer(), sl.x, sl.y,
                    g.GetWindowsWidth(), g.GetWindowsHeight(),
                    staticShadowEdges, noDynamic,
                    90, sl.params.shadowMaxLengthPx,
                    sl.params.shadowSoftLayers, sl.params.shadowSoftness);
            }
        }
    }

    // Draw shadow-radius debug circles on top of dark overlay (toggle with P).
    if (lightsEnabled && shadowsEnabled && showDebugTools) {
        const float previewShadowRadius =
            std::max(24.0f,
                     std::max(8.0f, lightMaskParams.falloffRadiusPx) * std::max(0.4f, lightMaskParams.fatorDicaDeRaio));
        DrawDebugCircle(g.GetRenderer(), smoothedDynamicLightScreenPos.x, smoothedDynamicLightScreenPos.y, previewShadowRadius,
                        255, 210, 90, 130);

        int renderedLights = 0;
        for (const LightInstance& light : lights) {
            if (!light.enabled) {
                continue;
            }
            if (renderedLights >= maxActiveLights) {
                break;
            }
            const Vec2 lightScreen = WorldToScreen(light.worldPos);
            const float cullRadius = std::max(32.0f, light.params.falloffRadiusPx * 1.6f);
            if (lightScreen.x < -cullRadius || lightScreen.y < -cullRadius ||
                lightScreen.x > static_cast<float>(g.GetWindowsWidth()) + cullRadius ||
                lightScreen.y > static_cast<float>(g.GetWindowsHeight()) + cullRadius) {
                continue;
            }
            const float placedShadowRadius =
                std::max(24.0f,
                         std::max(8.0f, light.params.falloffRadiusPx) * std::max(0.4f, light.params.fatorDicaDeRaio));
            DrawDebugCircle(g.GetRenderer(), lightScreen.x, lightScreen.y, placedShadowRadius, 120, 220, 255, 95);
            renderedLights++;
        }
    }

    if (lightTweakPanel && lightTweakPanel->visible) {
        lightTweakPanel->Render(g.GetRenderer(), g.GetWindowsWidth(), g.GetWindowsHeight());
    }

    for (const auto& go : objectArray) {
        if (go->z >= kHudZ) {
            go->Render();
        }
    }
}

void StageState::Start() {
    LoadAssets();
    StartArray(); // Chama Start() de todos os objetos
    SetMouseConfinedToWindow(true);
    started = true;
}

void StageState::Pause() {
    SetMouseConfinedToWindow(false);
}

void StageState::Resume() {
    SetMouseConfinedToWindow(true);
}

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

bool StageState::IsPartyReady() const { // Verifica se a dupla está pronta (ambos os personagens estão presentes)
    return controlledCharacter && controlledCharacterObject && companionCharacter && companionCharacterObject;
}

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

bool StageState::IsTileWalkable(int tx, int ty) const {
    if (!tileMapComp || !tileSet) {
        return true;
    }
    if (tx < 0 || ty < 0 || tx >= tileMapComp->GetWidth() || ty >= tileMapComp->GetHeight()) {
        return false;
    }
    const int tileId = tileMapComp->At(tx, ty, 1);
    return walkableTileIds.find(tileId) != walkableTileIds.end();
}

Vec2 StageState::TileCenterToWorld(int tx, int ty) const {
    const float tileW = tileSet ? static_cast<float>(tileSet->GetTileWidth()) : 1.0f;
    const float tileH = tileSet ? static_cast<float>(tileSet->GetTileHeight()) : 1.0f;
    return Vec2(mapOrigin.x + (static_cast<float>(tx) + 0.5f) * tileW,
                mapOrigin.y + (static_cast<float>(ty) + 0.5f) * tileH);
}

bool StageState::WorldToTile(const Vec2& worldPos, int& outTx, int& outTy) const {
    if (!tileMapComp || !tileSet) {
        return false;
    }
    const float tileW = static_cast<float>(std::max(1, tileSet->GetTileWidth()));
    const float tileH = static_cast<float>(std::max(1, tileSet->GetTileHeight()));
    outTx = static_cast<int>((worldPos.x - mapOrigin.x) / tileW);
    outTy = static_cast<int>((worldPos.y - mapOrigin.y) / tileH);
    return (outTx >= 0 && outTy >= 0 && outTx < tileMapComp->GetWidth() && outTy < tileMapComp->GetHeight());
}

bool StageState::FindNearestWalkableTile(int startTx, int startTy, int& outTx, int& outTy, int maxRadius) const {
    if (!tileMapComp) {
        return false;
    }
    if (IsTileWalkable(startTx, startTy)) {
        outTx = startTx;
        outTy = startTy;
        return true;
    }

    const int w = tileMapComp->GetWidth();
    const int h = tileMapComp->GetHeight();
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
                if (IsTileWalkable(x, y)) {
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

std::vector<Vec2> StageState::FindPathWorld(const Vec2& fromWorld, const Vec2& toWorld, int nodeBudget) const {
    std::vector<Vec2> empty;
    if (!tileMapComp || !tileSet) {
        return empty;
    }

    const int w = tileMapComp->GetWidth();
    const int h = tileMapComp->GetHeight();
    if (w <= 0 || h <= 0) {
        return empty;
    }

    int sx = 0;
    int sy = 0;
    int gx = 0;
    int gy = 0;
    if (!WorldToTile(fromWorld, sx, sy) || !WorldToTile(toWorld, gx, gy)) {
        return empty;
    }

    if (!IsTileWalkable(sx, sy)) {
        int nearestX = sx;
        int nearestY = sy;
        if (!FindNearestWalkableTile(sx, sy, nearestX, nearestY)) {
            return empty;
        }
        sx = nearestX;
        sy = nearestY;
    }
    if (!IsTileWalkable(gx, gy)) {
        int nearestX = gx;
        int nearestY = gy;
        if (!FindNearestWalkableTile(gx, gy, nearestX, nearestY)) {
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

    while (!open.empty() && expanded < std::max(64, nodeBudget)) {
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
            if (!IsTileWalkable(nx, ny)) {
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

void StageState::SwapControlledCharacter() {
    if (!IsPartyReady()) {
        return;
    }

    std::swap(controlledCharacter, companionCharacter);
    std::swap(controlledCharacterObject, companionCharacterObject);
}

void StageState::HandlePartyInput() {
    InputManager& input = InputManager::GetInstance();

    if (input.KeyPress(TAB_KEY)) {
        SwapControlledCharacter();
    }

    if (input.KeyPress(TOGGLE_MODE_KEY)) {
        if (partyMode == PartyMode::TOGETHER) {
            partyMode = PartyMode::INDEPENDENT;
        } else {
            partyMode = PartyMode::TOGETHER;
        }
    }
}

void StageState::IssueMovementFromInput(Character* character, GameObject* object) {
    if (!character || !object) {
        return;
    }

    InputManager& input = InputManager::GetInstance();
    Vec2 direction(0.0f, 0.0f);

    if (input.IsKeyDown(SDLK_w)) {
        direction.y -= 1.0f;
    }
    if (input.IsKeyDown(SDLK_s)) {
        direction.y += 1.0f;
    }
    if (input.IsKeyDown(SDLK_a)) {
        direction.x -= 1.0f;
    }
    if (input.IsKeyDown(SDLK_d)) {
        direction.x += 1.0f;
    }

    if (direction.Magnitude() > 0.0f) {
        Vec2 normalized = direction.Normalized();
        Character::Command moveCommand(Character::Command::MOVE, object->box.Center().x + normalized.x, object->box.Center().y + normalized.y);
        character->Issue(moveCommand);
    }
}

void StageState::IssueFollowCommand(Character* follower, GameObject* followerObject, GameObject* leaderObject, bool allowCatchup) {
    if (!follower || !followerObject || !leaderObject) {
        return;
    }

    const float preferredDistance = 68.0f;
    const float overlapDistance = 40.0f;
    const float followStartDistance = 82.0f;
    const float catchupDistance = 420.0f;

    Vec2 leaderCenter = leaderObject->box.Center();
    Vec2 followerCenter = followerObject->box.Center();
    Vec2 toLeader = leaderCenter - followerCenter;
    float distance = toLeader.Magnitude();

    follower->SetSpeedMultiplier(1.0f);
    if (distance < overlapDistance) {
        if (distance > 0.01f) {
            Vec2 pushDir = (followerCenter - leaderCenter).Normalized();
            Character::Command moveAway(Character::Command::MOVE, followerCenter.x + pushDir.x, followerCenter.y + pushDir.y);
            follower->Issue(moveAway);
        }
        return;
    }

    if (distance < followStartDistance) {
        return;
    }

    Vec2 dir = toLeader.Normalized();
    Vec2 targetPos = leaderCenter - (dir * preferredDistance);
    if (allowCatchup && distance > catchupDistance) {
        follower->SetSpeedMultiplier(1.55f);
    }
    Vec2 followTarget = targetPos;
    if (tileMapComp && tileSet && !HasWalkableLine(followerCenter, targetPos)) {
        const std::vector<Vec2> path = FindPathWorld(followerCenter, targetPos);
        if (path.size() >= 2) {
            followTarget = path[1];
        } else if (!path.empty()) {
            followTarget = path.front();
        }
    }

    Character::Command followCommand(Character::Command::MOVE, followTarget.x, followTarget.y);
    follower->Issue(followCommand);
}

void StageState::UpdateCompanionBehavior() {
    if (!IsPartyReady()) {
        return;
    }

    if (partyMode == PartyMode::TOGETHER) {
        IssueFollowCommand(companionCharacter, companionCharacterObject, controlledCharacterObject, true);
        return;
    }

    companionCharacter->SetSpeedMultiplier(1.0f);
}

void StageState::EnforceMaxDistance() {
    if (!IsPartyReady()) {
        return;
    }
    if (partyMode != PartyMode::TOGETHER) {
        return;
    }
    // No hard snap/teleport when party members are far apart.
    // Companion catch-up is handled smoothly by IssueFollowCommand().
}

void StageState::RefreshCameraTargets() {
    if (bigCharacterObject && smallCharacterObject) {
        Camera::FollowPair(bigCharacterObject, smallCharacterObject, controlledCharacterObject);
    }
}

void StageState::UpdateControlledCharacterVisuals() {
    if (!bigCharacterObject || !smallCharacterObject || !controlledCharacterObject) {
        return;
    }

    SpriteRenderer* bigSprite = bigCharacterObject->GetComponent<SpriteRenderer>();
    SpriteRenderer* smallSprite = smallCharacterObject->GetComponent<SpriteRenderer>();
    if (!bigSprite || !smallSprite) {
        return;
    }

    if (controlledCharacterObject == bigCharacterObject) {
        bigSprite->SetTint(255, 255, 255, 255);
        smallSprite->SetTint(120, 170, 230, 215);
    } else {
        bigSprite->SetTint(190, 190, 190, 220);
        smallSprite->SetTint(170, 230, 255, 255);
    }
}

void StageState::UpdateHudInstructions() {
    const float startX = 16.0f;
    const float startY = 12.0f;
    const float lineGap = 22.0f;

    if (hudLine1) {
        hudLine1->box.x = Camera::pos.x + startX;
        hudLine1->box.y = Camera::pos.y + startY;
    }

    if (hudLine2) {
        hudLine2->box.x = Camera::pos.x + startX;
        hudLine2->box.y = Camera::pos.y + startY + lineGap;
    }

    if (hudLine3) {
        hudLine3->box.x = Camera::pos.x + startX;
        hudLine3->box.y = Camera::pos.y + startY + lineGap * 2.0f;
    }

    if (hudFps) {
        hudFps->box.x = Camera::pos.x + startX;
        hudFps->box.y = Camera::pos.y + startY + lineGap * 3.0f;
    }
}
