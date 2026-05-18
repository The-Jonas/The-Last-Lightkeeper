#include "../include/FadeEffect.h"
#include "../include/SpriteRenderer.h"
#include "../include/StageState.h"
#include "../include/Game.h"

FadeEffect::FadeEffect(GameObject& associated) : Component(associated) {
}

FadeEffect::~FadeEffect(){
}

void FadeEffect::Update(float dt) {
    StageState* stage = (StageState*)&Game::GetInstance().GetCurrentState();
    
    // Pega os dois jogadores da fase
    GameObject* bigBro = stage->GetBigCharacter();
    GameObject* littleBro = stage->GetSmallCharacter();

    // Calcula a "Zona Mágica" de ficar transparente. 
    // Pegamos a box inteira da imagem, mas cortamos os 25% de baixo (que é a base sólida do objeto)
    Rect fadeZone = associated.box;
    fadeZone.h = fadeZone.h * 0.75f; 

    bool playerIsBehind = false;

    // Checa se o Irmãozão está atrás do objeto
    if (bigBro && fadeZone.Contains(bigBro->box.Center())) {
        playerIsBehind = true;
    }
    // Checa se o Irmãozinho está atrás do objeto
    if (littleBro && fadeZone.Contains(littleBro->box.Center())) {
        playerIsBehind = true;
    }

    // Aplica o efeito visual no Sprite
    SpriteRenderer* sprite = associated.GetComponent<SpriteRenderer>();
    if (sprite) {
        if (playerIsBehind) {
            // Fica transparente (Alpha 100)
            sprite->SetTint(255, 255, 255, 100);
        } else {
            // Fica opaco e normal (Alpha 255)
            sprite->SetTint(255, 255, 255, 255);
        }
    }
}

void FadeEffect::Render() {
    // Vazio, a renderização real continua sendo do SpriteRenderer
}