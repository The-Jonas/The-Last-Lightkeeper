#ifndef ANIMATION_H
#define ANIMATION_H

#define INCLUDE_SDL
#include "SDL_include.h"


class Animation {
public:
    // Conjunto de números que representa o primeiro e último frames da animação, e
    // quanto tempo deve se passar em cada frame.
    Animation(int frameStart, int frameEnd, float frameTime);
    Animation(int frameStart, int frameEnd, float frameTime, SDL_RendererFlip flip);           
                                                                        
    int frameStart;
    int frameEnd;
    float frameTime;
    SDL_RendererFlip flip;
};

#endif 