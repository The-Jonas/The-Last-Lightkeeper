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
#include <iostream>
#include <fstream> 
#include <algorithm>
#include <cmath>
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

float ComputeLightIntensityAtDistance(float distancePx, const LightMaskParams& params) {
    const float radius = std::max(8.0f, params.falloffRadiusPx);
    const float t = Clamp01(distancePx / radius);
    float curve = t * t * (3.0f - 2.0f * t);
    if (params.falloffCurve == LightFalloffCurve::Power) {
        curve = std::pow(t, std::max(0.05f, params.falloffGamma));
    }
    const float darkness = static_cast<float>(params.darknessMax) * (params.innerLift + (1.0f - params.innerLift) * curve);
    return Clamp01(1.0f - (darkness / 255.0f));
}

float ComputeShadowInfluence(const Vec2& pointScreen, const Vec2& lightScreenPos, const LightMaskParams& params) {
    // Hard distance cutoff for shadow casting; beyond this, no shadows.
    const float maxShadowDist = std::max(24.0f, params.falloffRadiusPx * params.shadowCastDistanceMul);
    const float d = pointScreen.Distance(lightScreenPos);
    if (d > maxShadowDist) {
        return 0.0f;
    }
    const float intensity = ComputeLightIntensityAtDistance(d, params);
    // Keep distance as dominant term so far lights get noticeably smaller/lighter shadows.
    const float distanceFactor = 1.0f - Clamp01(d / maxShadowDist);
    return Clamp01(distanceFactor * (0.15f + 0.85f * intensity));
}

bool IsPointLit(const Vec2& pointScreen, const Vec2& lightScreenPos, const LightMaskParams& params, float* outIntensity = nullptr) {
    const float intensity = ComputeShadowInfluence(pointScreen, lightScreenPos, params);
    if (outIntensity) {
        *outIntensity = intensity;
    }
    return intensity > 0.10f;
}

bool IsFootLit(GameObject* go, const Vec2& lightScreenPos, const LightMaskParams& params, float* outIntensity = nullptr) {
    if (!go) {
        return false;
    }
    const Rect& b = go->box;
    const Vec2 footWorld(b.x + 0.5f * b.w, b.y + b.h);
    const Vec2 footScreen((footWorld.x - Camera::pos.x) * Camera::GetZoom(), (footWorld.y - Camera::pos.y) * Camera::GetZoom());
    return IsPointLit(footScreen, lightScreenPos, params, outIntensity);
}

void AppendFootCircleShadows(GameObject* go, const Vec2& lightScreenPos, const LightMaskParams& params,
                             std::vector<TopDownShadowEdge>& outEdges, float* outLightTouch = nullptr) {
    if (!go) {
        return;
    }
    float touch = 0.0f;
    if (!IsFootLit(go, lightScreenPos, params, &touch)) {
        return;
    }
    if (outLightTouch) {
        *outLightTouch = std::max(*outLightTouch, touch);
    }
    const Rect& b = go->box;
    const Vec2 foot(b.x + 0.5f * b.w, b.y + b.h);
    const float m = (b.w < b.h) ? b.w : b.h;
    // Near lights generate broader contact shadows; far lights generate tighter ones.
    const float r = std::max(4.0f, std::min(36.0f, (0.10f + 0.28f * touch) * m));
    TopDownLightShadows::AppendCircleShadowEdges(foot, r, 16, outEdges);
}
}

StageState::StageState() {
    music.Open("Recursos/audio/BGM.wav");        // Carrega música de fundo
    music.Play();                                // Toca música
    tileSet = nullptr;                           // Caso precise guardar ponteiro
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
    radialGeometry = nullptr;
    lightMaskShape = LightMaskShape::Circle;
    lightTweakPanel.reset();
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

    // Criação do Background como Component e GameObject
    GameObject* bgObject = new GameObject();            // Criação de um ponteiro de GameObject
    SpriteRenderer* bgSprite = new SpriteRenderer(*bgObject, "Recursos/img/Background.png");        // Criando um spriteRenderer com os mesmos parâmetros de antes

    bgObject->AddComponent(bgSprite);                   // Adiciona o SpriteRenderer ao GameObject

    bgSprite->SetCameraFollower(true);                  // Por conta o setCameraFollower, agora a imagem não fica mais por baixo do TileSet sumida, agora o background segue a câmera

    bgObject->box.x = 0;                                // background colocado em (0, 0)
    bgObject->box.y = 0;
    bgObject->z = 0;                                    // Z = 0 (Camada mais ao fundo)
    AddObject(bgObject);                                // Colocando o ponteiro para o GameObject no ObjectArray usando AddObject
    
    //------------------------------------------

    //Criação do TileSet
    TileSet* tileSet = new TileSet(64, 64, "Recursos/img/Tileset.png");
    GameObject* mapObject = new GameObject();                                           // Criando o GameObject para o TileMap
    TileMap* tileMap = new TileMap(*mapObject, "Recursos/map/map.txt", tileSet);        // Criando o TileMap e associando o TileSet
    mapObject->AddComponent(tileMap);                                                   // Adicionando o TileMap ao GameObject

    // Define os multiplicador9s de parallax
    tileMap->SetParallax(0, -0.2f);                                                     // Camada 0 (ao fundo) se move mais devagar (na direção oposta?)
    tileMap->SetParallax(1, 0.0f);                                                      // Camada 1 (o chão) se move na velocidade normal
    
    mapObject->box.x = 0;                                                               // Definindo a posição
    mapObject->box.y = 0;                                                               // do mapa no jogo
    mapObject->z = 1;                                                                   // // Z = 1 (Camada do mapa, acima do fundo)
    AddObject(mapObject);                                                               // Adicionando o GameObject do mapa ao array de objetos

    //------------------------------------------
    
    // Criação dos dois personagens controláveis
    // Personagem grande (IRMÃOZÃO)
    GameObject* bigObject = new GameObject();
    Character* bigComp = new Character(*bigObject, "Recursos/img/Player.png");
    bigObject->AddComponent(bigComp);
    bigObject->box.x = 1280.0f;
    bigObject->box.y = 1280.0f;
    bigObject->z = 2;
    AddObject(bigObject);

    // Personagem pequeno (IRMÃOZINHO)
    GameObject* smallObject = new GameObject();
    Character* smallComp = new Character(*smallObject, "Recursos/img/Player.png");
    smallObject->AddComponent(smallComp);
    smallObject->box.x = 1220.0f;
    smallObject->box.y = 1320.0f;
    smallObject->z = 2;
    AddObject(smallObject);

    // Configurações do personagem pequeno (IRMÃOZINHO)
    SpriteRenderer* smallSprite = smallObject->GetComponent<SpriteRenderer>();
    if (smallSprite) {
        smallSprite->SetScale(0.72f, 0.72f);
        smallSprite->SetTint(150, 200, 255, 240);
    }
    smallComp->SetBaseSpeed(220.0f);

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
    hudLine1->AddComponent(new Text(*hudLine1, "Recursos/font/neodgm.ttf", 18, Text::BLENDED, "TAB: trocar personagem", hudColor));
    AddObject(hudLine1);

    // Linha 2 de instruções
    hudLine2 = new GameObject();
    hudLine2->z = 100;
    hudLine2->AddComponent(new Text(*hudLine2, "Recursos/font/neodgm.ttf", 18, Text::BLENDED, "F: alternar entre junto e independente", hudColor));
    AddObject(hudLine2);

    hudLine3 = new GameObject();
    hudLine3->z = 100;
    hudLine3->AddComponent(new Text(*hudLine3, "Recursos/font/neodgm.ttf", 18, Text::BLENDED,
                                      "K forma | C criar luz | P painel | L luz | O sombras", hudColor));
    AddObject(hudLine3);

    Game& gameRef = Game::GetInstance();
    radialGeometry = new RadialLightOverlay();
    if (!radialGeometry->Init(gameRef.GetRenderer())) {
        delete radialGeometry;
        radialGeometry = nullptr;
    }

    lightTweakPanel = std::make_unique<LightTweakPanel>(lightMaskParams, lightMaskShape);

    RefreshCameraTargets(); // Atualiza alvos da câmera (dupla + principal)

}

void StageState::Update(float dt){
    InputManager& input = InputManager::GetInstance();

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
    if (input.KeyPress(CREATE_LIGHT_KEY) && lightMaskShape == LightMaskShape::Circle) {
        CreateLightAtCursor();
    }

    if (IsPartyReady()) {
        HandlePartyInput();
        IssueMovementFromInput(controlledCharacter, controlledCharacterObject);
        UpdateCompanionBehavior();
    }

    UpdateArray(dt);                                                                    // Percorre o vetor de GameObjects chamando o Update de cada um
    if (IsPartyReady()) {
        EnforceMaxDistance();
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
        if (go->z < kHudZ) {
            go->Render();
        }
    }

    Game& g = Game::GetInstance();
    InputManager& in = InputManager::GetInstance();
    if (lightsEnabled && radialGeometry != nullptr) {
        std::vector<RadialLightOverlay::ScreenLight> screenLights;
        screenLights.reserve(static_cast<size_t>(maxActiveLights + 1));
        // Dynamic preview light is always present.
        screenLights.push_back({static_cast<float>(in.GetMouseX()), static_cast<float>(in.GetMouseY()), lightMaskShape, lightMaskParams});
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
            screenLights.push_back({lightScreen.x, lightScreen.y, light.shape, light.params});
            renderedLights++;
        }
        radialGeometry->RenderMany(g.GetRenderer(), g.GetWindowsWidth(), g.GetWindowsHeight(), screenLights);
    }

    if (lightsEnabled && shadowsEnabled) {
        static const std::vector<TopDownShadowEdge> kNoMapShadows;
        static thread_local std::vector<TopDownShadowEdge> playerEdges;

        auto renderShadowsForLight = [&](const Vec2& lightScreen, const LightMaskParams& params) {
            playerEdges.clear();
            float lightTouch = 0.0f;
            AppendFootCircleShadows(bigCharacterObject, lightScreen, params, playerEdges, &lightTouch);
            AppendFootCircleShadows(smallCharacterObject, lightScreen, params, playerEdges, &lightTouch);
            if (playerEdges.empty()) {
                return;
            }
            // Distance-sensitive response:
            // closer (high touch) => longer + darker
            // farther (low touch) => shorter + brighter
            const float shadowLengthPx = 12.0f + 140.0f * lightTouch;
            // Clamp so shadow stacking never dominates the base dark overlay.
            const float alphaCap = static_cast<float>(params.darknessMax) * 0.40f;
            const Uint8 shadowAlpha = static_cast<Uint8>(std::max(8.0f, std::min(alphaCap, 110.0f * lightTouch)));
            TopDownLightShadows::RenderShadowVolumes(g.GetRenderer(), lightScreen.x, lightScreen.y, g.GetWindowsWidth(),
                                                     g.GetWindowsHeight(), kNoMapShadows, playerEdges, shadowAlpha,
                                                     shadowLengthPx, 2);
        };

        // Preview light also casts shadows for immediate feedback.
        renderShadowsForLight(Vec2(static_cast<float>(in.GetMouseX()), static_cast<float>(in.GetMouseY())), lightMaskParams);

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
    if (lightMaskShape != LightMaskShape::Circle) {
        return;
    }
    InputManager& input = InputManager::GetInstance();
    LightInstance light;
    light.worldPos = ScreenToWorld(Vec2(static_cast<float>(input.GetMouseX()), static_cast<float>(input.GetMouseY())));
    light.shape = LightMaskShape::Circle;
    light.params = lightMaskParams;
    light.enabled = true;
    lights.push_back(light);
    if (static_cast<int>(lights.size()) > maxActiveLights * 2) {
        lights.erase(lights.begin(), lights.begin() + (lights.size() - static_cast<size_t>(maxActiveLights * 2)));
    }
}

bool StageState::IsPartyReady() const { // Verifica se a dupla está pronta (ambos os personagens estão presentes)
    return controlledCharacter && controlledCharacterObject && companionCharacter && companionCharacterObject;
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

    const float preferredDistance = 95.0f;
    const float overlapDistance = 55.0f;
    const float followStartDistance = 120.0f;
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
    Character::Command followCommand(Character::Command::MOVE, targetPos.x, targetPos.y);
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

    const float maxPartyDistance = 720.0f;
    Vec2 controlledCenter = controlledCharacterObject->box.Center();
    Vec2 companionCenter = companionCharacterObject->box.Center();
    Vec2 delta = controlledCenter - companionCenter;
    float distance = delta.Magnitude();

    if (distance <= maxPartyDistance || distance < 0.001f) {
        return;
    }

    Vec2 dir = delta.Normalized();
    Vec2 clampedCenter = companionCenter + (dir * maxPartyDistance);
    controlledCharacterObject->box.x = clampedCenter.x - (controlledCharacterObject->box.w / 2.0f);
    controlledCharacterObject->box.y = clampedCenter.y - (controlledCharacterObject->box.h / 2.0f);
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
}
