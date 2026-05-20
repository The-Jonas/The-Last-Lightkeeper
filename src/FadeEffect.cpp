#include "../include/FadeEffect.h"
#include "../include/SpriteRenderer.h"
#include "../include/StageState.h"
#include "../include/Game.h"
#include "../include/Character.h"

FadeEffect::FadeEffect(GameObject& associated, bool ignoreElevated) 
    : Component(associated), ignoreElevated(ignoreElevated) {
}

FadeEffect::~FadeEffect(){
}

void FadeEffect::Update(float dt) {
StageState* stage = (StageState*)&Game::GetInstance().GetCurrentState();
    
    GameObject* bigBro = stage->GetBigCharacter();
    GameObject* littleBro = stage->GetSmallCharacter();

    Rect fadeZone = associated.box;
    fadeZone.h = fadeZone.h * 0.75f; 

    bool playerIsBehind = false;

    // Criamos uma lambda limpa para testar os dois jogadores
    auto checkPlayer = [&](GameObject* playerObj) {
        if (!playerObj) return;
        
        Character* character = playerObj->GetComponent<Character>();
        if (!character) return;

        // Se este FadeEffect é da escada (ignoreElevated == true) 
        // e o jogador está no andar de cima, a gente ignora ele! Ele não deixa transparente.
        if (ignoreElevated && character->isElevated) return;

        // Para todo o resto (Pilares, Árvores, ou se estiver no chão normal), testa a caixa:
        if (fadeZone.Contains(playerObj->box.Center())) {
            playerIsBehind = true;
        }
    };

    checkPlayer(bigBro);
    checkPlayer(littleBro);

    // Aplica o efeito visual no Sprite
    SpriteRenderer* sprite = associated.GetComponent<SpriteRenderer>();
    if (sprite) {
        if (playerIsBehind) {
            sprite->SetTint(255, 255, 255, 100); // Transparente
        } else {
            sprite->SetTint(255, 255, 255, 255); // Sólido
        }
    }
}

void FadeEffect::Render() {
    // Vazio, a renderização real continua sendo do SpriteRenderer
}
