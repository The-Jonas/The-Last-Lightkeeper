#include "../include/Repairable.h"
#include "../include/InputManager.h"
#include "../include/SpriteRenderer.h"
#include "../include/StageState.h"
#include "../include/Game.h"
#include "../include/Camera.h"
#include "../include/Character.h"
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
        // Pega o componente Character do Irmãozão para podermos ler o crachá VIP dele
        Character* bigChar = bigBro->GetComponent<Character>();

        // Cria o ponto exato onde o jogador tem que ficar
        Vec2 exactInteractPoint(associated.box.x + interactionOffset.x, associated.box.y + interactionOffset.y);
        
        // Mede a distância entre o centro da escada e o centro do jogador
        float distance = exactInteractPoint.Distance(bigBro->box.Center());
        
        // A SUA TRAVA DE SEGURANÇA AQUI:
        // O jogador precisa estar perto O SUFICIENTE e PRECISA ESTAR NA ESCADA (isElevated)
        if (distance <= interactionDistance && bigChar != nullptr && bigChar->isElevated) {
            
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
                
                // Avisa o mapa que o buraco sumiu!
                stage->level.escadaConsertada = true; 

                std::cout << "Consertado usando: " << requiredItem << std::endl;
            }
        }
    }   
}

void Repairable::Render() {
// Se a escada já foi consertada, some
    if (isRepaired) return;

#ifdef DEBUG

    // AVISO: Removi o #ifdef DEBUG propositalmente para forçar a renderização aparecer!
    
    SDL_Renderer* renderer = Game::GetInstance().GetRenderer();

    // 1. Ponto exato no mundo
    Vec2 exactInteractPoint(associated.box.x + interactionOffset.x, associated.box.y + interactionOffset.y);

    // 2. Subtrai a câmera para desenhar no lugar certo da tela
    float renderX = exactInteractPoint.x - Camera::pos.x;
    float renderY = exactInteractPoint.y - Camera::pos.y;

    // 3. Define a cor como AMARELO
    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);

    // 4. Desenha a Cruz
    SDL_RenderDrawLine(renderer, renderX - 10, renderY, renderX + 10, renderY);
    SDL_RenderDrawLine(renderer, renderX, renderY - 10, renderX, renderY + 10);

    // 5. Desenha o Quadrado da Área
    SDL_Rect debugRect;
    debugRect.x = (int)(renderX - interactionDistance);
    debugRect.y = (int)(renderY - interactionDistance);
    debugRect.w = (int)(interactionDistance * 2);
    debugRect.h = (int)(interactionDistance * 2);
    
    SDL_RenderDrawRect(renderer, &debugRect);

#endif
}
