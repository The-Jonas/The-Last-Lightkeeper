#include "engine/Animator.h"
#include "engine/GameObject.h"
#include "engine/SpriteRenderer.h"
#include <iostream>

Animator::Animator(GameObject& associated) : Component(associated) {
    // Inicializa todos os membros com valores padrão zero.
    frameStart = 0;
    frameEnd = 0;
    frameTime = 0.0f;
    currentFrame = 0;
    timeElapsed = 0.0f;
    currentAnimation = "";
}

void Animator::Update(float dt){
    if(frameTime == 0.0f){                                                              // Se não houver tempo de quadro, a animação está parada.
        return;
    }

    timeElapsed += dt;                                                                  // Acumula o tempo que passou desde o último quadro.

    // Se o tempo acumulado for maior ou igual ao tempo de um quadro,
    // avança para o próximo quadro.
    if (timeElapsed >= frameTime) {
        currentFrame++;
        timeElapsed -= frameTime;

        // Se o quadro atual ultrapassou o quadro final, retorna ao inicial.
        if (currentFrame > frameEnd) {
            currentFrame = frameStart;
        }

        // Obtém o SpriteRenderer para atualizar o quadro exibido.
        SpriteRenderer* sprite = associated.GetComponent<SpriteRenderer>();
        if (sprite) {
            sprite->SetFrame(currentFrame);
        }
    }
}

void Animator::Render(){
    // Função vazia por enquanto
}

void Animator::SetAnimation(std::string name) {                                         // Seta a animação atual.

    // 1. Verifique se a animação atual é diferente da nova
    if (name != currentAnimation){                                                      // Verifica se a nova animação é diferente da atual
        auto it = animations.find(name);                                                // Busque no seu unordered_map por um par com chave = name

        if (it != animations.end()) {
            // 2. Se encontrar, defina frameStart, frameEnd e frameTime
            frameStart = it->second.frameStart;                                         // Se encontrar, defina frameStart, frameEnd e 
            frameEnd = it->second.frameEnd;                                             // frameTime de acordo com a animação encontrada 
            frameTime = it->second.frameTime;

            // 3. Resete currentFrame e timeElapsed
            currentFrame = frameStart;                                                  // Resete currentFrame para ser igual a frameStart 
            timeElapsed = 0.0f;                                                         // e timeElapsed para 0

            // 4. Salve o nome da animação atual
            currentAnimation = name;

            // 5. Chame SetFrame no SpriteRenderer
            SpriteRenderer* sprite = associated.GetComponent<SpriteRenderer>();         // Por fim, chame SetFrame no SpriteRenderer, passando
            if (sprite){                                                                // currentFrame como argumento.
                sprite->SetFrame(currentFrame, it->second.flip);
            }
        }
    }
}

void Animator::AddAnimation(std::string name, Animation anim) {                         // Adiciona uma nova animação ao mapa.
    auto it = animations.find(name);                                                    // Busque no seu unordered_map por um par com chave = name.
    
    if (it == animations.end()) {                                                       // Se não encontrar, insira o par (name, anim) no seu mapa.
        animations.insert({name, anim});
    }

}