#include "gameplay/Candlestick.h"
#include "gameplay/Character.h"
#include "core/InputManager.h"
#include "states/stage/StageState.h"
#include "engine/SpriteRenderer.h"
#include "ui/Text.h"
#include "core/Game.h"
#include <iostream>

Candlestick::Candlestick(GameObject& associated, bool startsLit, const std::string& direction)
    : Component(associated), isLit(startsLit), direction(direction), myLightId(-1) {
    
    basePath = "Recursos/img/objetos/castical/castical_"; 

    std::string state = isLit ? "_aceso.png" : "_apagado.png";
    std::string fullPath = basePath + direction + state;
    
    // Já anexa o componente de Sprite na criação
    SpriteRenderer* sprite = new SpriteRenderer(associated, fullPath);
    associated.AddComponent(sprite);

  
    // CRIAÇÃO DO TEXTO DE INTERAÇÃO
    textObj = new GameObject();
    
    SDL_Color textColor = {255, 255, 255, 255}; 
    Text* promptText = new Text(*textObj, "Recursos/font/TradeWinds-Regular.ttf", 14, Text::SOLID, "[E] Acender", textColor);
    textObj->AddComponent(promptText);

}

Candlestick::~Candlestick() {
    // Como o textObj não foi pro StageState, nós deletamos ele aqui para não vazar memória
    delete textObj;
}

void Candlestick::Start() {
    // Registra o Castiçal na Engine de luzes no momento em que ele nasce!
    // Pegamos o centro da caixa (para a luz sair do meio da arte)
    StageState* stage = Game::TryGetStageState();
    if (stage) {
        myLightId = stage->CreateStaticLight(associated.box.Center(), isLit);
    }
} 

void Candlestick::Update(float dt) {
showPrompt = false; // Reseta a flag todo frame

    if (Character::player) {

        // Pede pro personagem gerar a mão EXATAMENTE na altura 1 (Hitbox das Mãos)
        SDL_Rect reachBox = Character::player->GetInteractionRect(1); 
        
        // Converte a caixa do Castiçal para SDL_Rect
        SDL_Rect candleRect = {
            static_cast<int>(associated.box.x),
            static_cast<int>(associated.box.y),
            static_cast<int>(associated.box.w),
            static_cast<int>(associated.box.h)
        };
        
        // Se a hitbox da mão encostar no castiçal...
        if (SDL_HasIntersection(&reachBox, &candleRect)) {
            
            // Só exibe a opção de acender se estiver apagado
            if (!isLit) {
                showPrompt = true;
                
                if (InputManager::GetInstance().KeyPress(SDLK_e)) {
                    StageState* stage = Game::TryGetStageState();
                    
                    if (stage && stage->GetInventory().IsUsableLightActive()) {
                        SetLit(true);
                        std::cout << "[CASTICAL] Acendido com sucesso!" << std::endl;
                    } else {
                        std::cout << "[CASTICAL] Preciso do isqueiro aceso na mao!" << std::endl;
                    }
                }
            } 
        }
    }
}

void Candlestick::Render() {
    // Se a flag estiver verdadeira, nós desenhamos o texto flutuando!
    if (showPrompt && textObj) {
        // Centraliza o texto no meio do castiçal e joga ele uns 30 pixels para cima
        textObj->box.x = associated.box.Center().x - (textObj->box.w / 2.0f);
        textObj->box.y = associated.box.y - 30; 
        
        textObj->Render();
    }
}

void Candlestick::SetLit(bool lit) {
    isLit = lit;

    SpriteRenderer* sprite = associated.GetComponent<SpriteRenderer>();
    if (sprite) {
        std::string estado = isLit ? "_aceso.png" : "_apagado.png";
        sprite->Open(basePath + direction + estado);
    }

    // Liga/Desliga a luz na Engine
    StageState* stage = Game::TryGetStageState();
    if (stage && myLightId != -1) {
        stage->SetLightEnabled(myLightId, isLit);
    }
}