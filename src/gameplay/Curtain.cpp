#include "gameplay/Curtain.h"
#include "engine/GameObject.h"
#include "engine/SpriteRenderer.h"
#include "engine/Camera.h"
#include "world/Collider.h"
#include "core/Game.h"


Curtain::Curtain(GameObject& associated, int curtainId,
                 bool startsOpen, float animDuration)
    : Component(associated),
      curtainId(curtainId),
      startsOpen(startsOpen),
      animDuration(animDuration) {}

void Curtain::Start() {
    // Colisão na base do asset
    Vec2 colScale(0.9f, 0.04f);
    float offsetY = associated.box.h * 0.5f - associated.box.h * colScale.y * 0.5f;
    col = new Collider(associated, colScale, Vec2(0.0f, offsetY));
    associated.AddComponent(col);

    if (startsOpen) {
        currentHeight = minHeight;
        state         = CurtainState::OPEN;
        if (col) col->SetEnabled(false);
    } else {
        currentHeight = fullHeight;
        state         = CurtainState::CLOSED;
    }
    associated.box.h = currentHeight;
}

void Curtain::Open() {
    if (state == CurtainState::OPEN || state == CurtainState::OPENING) return;
    state = CurtainState::OPENING;
    // Colisão some imediatamente ao começar a abrir
    if (col) col->SetEnabled(false);
}

void Curtain::Close() {
    if (state == CurtainState::CLOSED || state == CurtainState::CLOSING) return;
    state = CurtainState::CLOSING;
    // Colisão só volta quando totalmente fechada (no Update)
}

void Curtain::Update(float dt) {
    float speed = (fullHeight - minHeight) / animDuration;

    switch (state) {
        case CurtainState::OPENING:
            currentHeight -= speed * dt;
            if (currentHeight <= minHeight) {
                currentHeight = minHeight;
                state = CurtainState::OPEN;
            }
            break;

        case CurtainState::CLOSING:
            currentHeight += speed * dt;
            if (currentHeight >= fullHeight) {
                currentHeight = fullHeight;
                state = CurtainState::CLOSED;
                if (col) col->SetEnabled(true);
            }
            break;

        default: break;
    }

    associated.box.h = currentHeight;
}

void Curtain::NotifyCollision(GameObject& other) {
    // Só bloqueia se o collider estiver ativo (cortina fechada)
    if (!col || !col->IsEnabled()) return;

    Collider* myCol    = associated.GetComponent<Collider>();
    Collider* otherCol = other.GetComponent<Collider>();
    if (!myCol || !otherCol) return;

    // Mesmo cálculo de separação AABB do Box
    float overLeft   = (otherCol->box.x + otherCol->box.w) - myCol->box.x;
    float overRight  = (myCol->box.x + myCol->box.w) - otherCol->box.x;
    float overTop    = (otherCol->box.y + otherCol->box.h) - myCol->box.y;
    float overBottom = (myCol->box.y + myCol->box.h) - otherCol->box.y;

    if (overLeft<=0||overRight<=0||overTop<=0||overBottom<=0) return;

    float minOverlap = overLeft;
    if (overRight  < minOverlap) minOverlap = overRight;
    if (overTop    < minOverlap) minOverlap = overTop;
    if (overBottom < minOverlap) minOverlap = overBottom;
    Vec2 push(0,0);
    if      (minOverlap == overLeft)   push.x =  overLeft;
    else if (minOverlap == overRight)  push.x = -overRight;
    else if (minOverlap == overTop)    push.y =  overTop;
    else if (minOverlap == overBottom) push.y = -overBottom;

    // Empurra o outro objeto (personagem) de volta
    other.box.x -= push.x;
    other.box.y -= push.y;
}

void Curtain::Render() {
#ifdef DEBUG
    SDL_Renderer* r = Game::GetInstance().GetRenderer();
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    // Sprite box — ciano
    SDL_Rect sb = {
        static_cast<int>(associated.box.x - Camera::pos.x),
        static_cast<int>(associated.box.y - Camera::pos.y),
        static_cast<int>(associated.box.w),
        static_cast<int>(associated.box.h)
    };
    SDL_SetRenderDrawColor(r, 0, 200, 255, 50);
    SDL_RenderFillRect(r, &sb);
    SDL_SetRenderDrawColor(r, 0, 200, 255, 200);
    SDL_RenderDrawRect(r, &sb);

    // Collider — verde=ativo, vermelho=inativo
    if (col) {
        SDL_Rect cb = {
            static_cast<int>(col->box.x - Camera::pos.x),
            static_cast<int>(col->box.y - Camera::pos.y),
            static_cast<int>(col->box.w),
            static_cast<int>(col->box.h)
        };
        bool alive = col->IsEnabled();
        SDL_SetRenderDrawColor(r, alive?0:255, alive?255:0, 0, 100);
        SDL_RenderFillRect(r, &cb);
        SDL_SetRenderDrawColor(r, alive?0:255, alive?255:0, 0, 255);
        SDL_RenderDrawRect(r, &cb);
    }

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
#endif
}