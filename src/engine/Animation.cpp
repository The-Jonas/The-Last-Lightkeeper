#include "engine/Animation.h"


// Construtor original (3 args)
// Ele DELEGA para o construtor de 4 args, passando SDL_FLIP_NONE
// Isso resolveu o bug de o valor de flip pegar o lixo na memória (e acabar sendo o VERTICAL), fazendo o personagem ficar de cabeça pra baixo
// Já que não é definido esse valor pra maior parte das animações
Animation::Animation(int frameStart, int frameEnd, float frameTime)
: Animation(frameStart, frameEnd, frameTime, SDL_FLIP_NONE) {
}

// Novo construtor (4 args)
// Este faz a inicialização real, inicializando todos os membros
Animation::Animation(int frameStart, int frameEnd, float frameTime, SDL_RendererFlip flip)
: frameStart(frameStart), frameEnd(frameEnd), frameTime(frameTime), flip(flip) {
}