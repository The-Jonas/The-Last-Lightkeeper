#ifndef INVENTORY_WHEEL_H
#define INVENTORY_WHEEL_H

#define INCLUDE_SDL
#include "SDL_include.h"
#include "engine/Component.h"
#include "gameplay/Inventory.h"

class InventoryWheel : public Component {
public:
    InventoryWheel(GameObject& associated, Inventory& inventory);

    void Start() override;
    void Update(float dt) override;
    void Render() override;

private:
    Inventory& inventory;
    float displayActiveIndex = 0.0f;
    int lastKnownActive = -1;
    float bobTimer = 0.0f;

    // Roda VERTICAL (itens sobem/descem): ativo no centro, anterior acima,
    // próximo abaixo; o arco abre para a esquerda. Slots circulares + anel de
    // durabilidade radial.
    static constexpr float kSlotSize = 88.0f;
    static constexpr float kIconSize = 62.0f;
    static constexpr float kArcRadius = 150.0f;   // raio do arco vertical
    static constexpr float kAnchorXFraction = 0.085f;  // X do slot ATIVO (borda esquerda)
    static constexpr float kAnchorYFraction = 0.76f;   // Y do slot ATIVO (mais para baixo do canto)
    static constexpr float kSlideSpeed = 10.0f;
    static constexpr float kRingThickness = 7.0f;   // espessura do anel de durabilidade

    float GetAnchorX() const;
    float GetAnchorY() const;
    void GetSlotScreenPos(int visibleOffset, int visibleCount, float scrollOffset,
                          float& outX, float& outY) const;
    float GetSlotAlpha(int distanceFromCenter, int visibleCount) const;
    float GetSlotScale(int distanceFromCenter, int visibleCount) const;
    void DrawSlot(SDL_Renderer* renderer, int stackIndex, float x, float y,
                  float alpha, float scale, bool isActive) const;
    void DrawRefuelSelector(SDL_Renderer* renderer);
    void DrawUseHint(SDL_Renderer* renderer, float activeX, float activeY);
    // Desenha as teclas de trocar item (1/3) nos lados corretos da roda:
    // "anterior" à esquerda, "próximo" à direita.
    void DrawCycleKeyHints(SDL_Renderer* renderer);
};

#endif
