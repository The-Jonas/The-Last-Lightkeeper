#ifndef INTERACTION_OUTLINE_H
#define INTERACTION_OUTLINE_H

#define INCLUDE_SDL
#include "SDL_include.h"

class GameObject;

void DrawRectOutline(SDL_Renderer* renderer, const SDL_FRect& rect, Uint8 r, Uint8 g, Uint8 b,
                     Uint8 a = 240, float pad = 2.0f, int thickness = 2);
void DrawSpriteInteractionGlow(GameObject& obj, Uint8 r, Uint8 g, Uint8 b,
                               float scaleBoost = 1.08f, Uint8 a = 220);

#endif
