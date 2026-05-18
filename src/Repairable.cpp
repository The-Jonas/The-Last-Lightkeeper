#include "../include/Repairable.h"
#include "../include/InputManager.h"
#include "../include/SpriteRenderer.h"
#include "../include/StageState.h"
#include "../include/Game.h"
#include "../include/Camera.h"
#include <iostream>

Repairable::Repairable(GameObject& associated, std::string fixedSpritePath, std::string requiredItem, std::string soundPath, float interactionDistance, Vec2 interactionOffset) 
    : Component(associated), fixedSpritePath(fixedSpritePath), requiredItem(requiredItem), soundPath(soundPath), interactionDistance(interactionDistance), interactionOffset(interactionOffset) {
    isRepaired = false;
}

Repairable::~Repairable() {
}

void Repairable::Update(float dt) {
    // Se já consertou, não precisa fazer mais nada
    if (isRepaired) return;

    StageState* stage = (StageState*)&Game::GetInstance().GetCurrentState();
    GameObject* bigBro = stage->GetBigCharacter();
    
    if (bigBro) {

        // Cria o ponto exato onde o jogador tem que ficar
        Vec2 exactInteractPoint(associated.box.x + interactionOffset.x, associated.box.y + interactionOffset.y);
        // Mede a distância entre o centro da escada e o centro do jogador

        float distance = exactInteractPoint.Distance(bigBro->box.Center());
        
        // Se o jogador estiver perto o suficiente
        if (distance <= interactionDistance) {
            // Aperta a tecla E
            if (InputManager::GetInstance().KeyPress(SDLK_e)) {

                // Checa o item pelo nome que foi passado (ex: "Madeira" ou "Engrenagem")
                // if (requiredItem != "" && !Inventory::HasItem(requiredItem)) return;

                if (soundPath != "") {
                    //Sound* repairSound = new Sound(associated, soundPath);
                    //repairSound->Play();
                }

                // Troca a imagem para a versão consertada!
                SpriteRenderer* sprite = associated.GetComponent<SpriteRenderer>();
                if (sprite) {
                    sprite->Open(fixedSpritePath);
                }

                // Marca como consertado para não ativar de novo
                isRepaired = true;
                std::cout << "Consertado usando: " << requiredItem << std::endl;
            }
        }
    }   
}

void Repairable::Render() {
    // Se a escada já foi consertada, não precisa fazer mais nada
    if (isRepaired) return;

// Toda essa parte de baixo só vai existir se o jogo for compilado em modo Debug!
#ifdef DEBUG

    // Pega o renderizador da engine
    SDL_Renderer* renderer = Game::GetInstance().GetRenderer();

    // Calcula o ponto exato no mundo
    Vec2 exactInteractPoint(associated.box.x + interactionOffset.x, associated.box.y + interactionOffset.y);

    // [IMPORTANTE]: Se o seu jogo tiver uma Câmera, aplique a subtração aqui:
    float renderX = exactInteractPoint.x - Camera::pos.x;
    float renderY = exactInteractPoint.y - Camera::pos.y;
    //float renderX = exactInteractPoint.x; 
    //float renderY = exactInteractPoint.y;

    // Define a cor do pincel para AMARELO (R: 255, G: 255, B: 0, Alpha: 255)
    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);

    // Desenha uma CRUZ (+) exatamente no pixel do Offset
    SDL_RenderDrawLine(renderer, renderX - 10, renderY, renderX + 10, renderY);
    SDL_RenderDrawLine(renderer, renderX, renderY - 10, renderX, renderY + 10);

    // Desenha a "Área de Interação"
    SDL_Rect debugRect;
    debugRect.x = (int)(renderX - interactionDistance);
    debugRect.y = (int)(renderY - interactionDistance);
    debugRect.w = (int)(interactionDistance * 2);
    debugRect.h = (int)(interactionDistance * 2);

    SDL_RenderDrawRect(renderer, &debugRect);

#endif
}
