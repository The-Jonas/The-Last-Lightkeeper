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
#include "../include/PlayerController.h"
#include "../include/Collider.h"
#include "../include/Collision.h"
#include "../include/GameData.h"
#include "../include/EndState.h"
#include <iostream>
#include <fstream> 
#include <algorithm>
#include <cmath>

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
}

StageState::StageState() {
    music.Open("Recursos/audio/BGM.wav");        // Carrega música de fundo
    music.Play();                                // Toca música
}

StageState::~StageState(){                                
    // O destrutor de State cuida de limpar o objectArray
    // A música é limpa pelo destrutor de Music
    SetMouseConfinedToWindow(false);
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
    
    // Criação do Personagem Jogável (Character)
    GameObject* playerObject = new GameObject();

    // O Character usa a imagem 'Player.png'
    Character* characterComp = new Character(*playerObject, "Recursos/img/Player.png");
    playerObject->AddComponent(characterComp);

    // Adiciona o PlayerController
    PlayerController* controller = new PlayerController(*playerObject);
    playerObject->AddComponent(controller);

    // Posição inicial
    playerObject->box.x = 1280.0f;
    playerObject->box.y = 1280.0f;
    playerObject->z = 2;                                                                // Z = 2 (Camada de Gameplay)
    AddObject(playerObject);

    Camera::Follow(playerObject);                                                       // Foca a câmera no personagem

}

void StageState::Update(float dt){
    Camera::Update(dt);                                                                 // Atualizando a camera cada iteração do gameloop

    InputManager& input = InputManager::GetInstance();

    // QuitRequested (Clicar no X da janela) -> Fecha o jogo
    if (input.QuitRequested()) {
        quitRequested = true;
    }

    // ESC -> Volta para o menu
    if (input.KeyPress(ESCAPE_KEY)) {
        popRequested = true;
    }

    UpdateArray(dt);                                                                    // Percorre o vetor de GameObjects chamando o Update de cada um

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
    if (Character::player == nullptr) {
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

    RenderArray();                                                   // Percorre o vetor de GameObjects chamando o Render de cada um 
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