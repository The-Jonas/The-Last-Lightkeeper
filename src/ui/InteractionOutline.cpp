#include "ui/InteractionOutline.h"
#include "engine/GameObject.h"
#include "engine/SpriteRenderer.h"

void DrawRectOutline(SDL_Renderer* renderer, const SDL_FRect& rect, Uint8 r, Uint8 g, Uint8 b,
                     Uint8 a, float pad, int thickness) {
    if (!renderer) {
        return;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    SDL_FRect outline = {
        rect.x - pad,
        rect.y - pad,
        rect.w + pad * 2.0f,
        rect.h + pad * 2.0f,
    };
    for (int i = 0; i < thickness; ++i) {
        const float offset = static_cast<float>(i);
        SDL_FRect expanded = {
            outline.x - offset,
            outline.y - offset,
            outline.w + offset * 2.0f,
            outline.h + offset * 2.0f,
        };
        SDL_RenderDrawRectF(renderer, &expanded);
    }
}

void DrawSpriteInteractionGlow(GameObject& obj, Uint8 r, Uint8 g, Uint8 b,
                               float scaleBoost, Uint8 a) {
    SpriteRenderer* sprite = obj.GetComponent<SpriteRenderer>();
    if (!sprite) {
        return;
    }

    sprite->RenderHighlight(scaleBoost, r, g, b, a);
}
