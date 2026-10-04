#ifndef STAIRTRIGGER_H
#define STAIRTRIGGER_H

#include "engine/Component.h"
#include "engine/GameObject.h"
#include "math/Rect.h"
#include "math/Vec2.h"

// ─────────────────────────────────────────────────────────────────────────────
//  "Tapete" na base da escada (Tiled: StairTrigger, propriedade anchorY).
//  Quem passa por ele SUBINDO fica "em cima da escada" (isElevated); quem passa
//  DESCENDO volta ao chão. Vale para os irmãos (aqui) e para o monstro (que
//  chama ApplyCrossing no próprio Update).
// ─────────────────────────────────────────────────────────────────────────────
class StairTrigger : public Component {
public:
    StairTrigger(GameObject& associated, float anchorY);
    ~StairTrigger();

    void Update(float dt) override;
    void Render() override;

    const Rect& GetZone() const { return associated.box; }   // área do tapete (mundo)
    float GetAnchorY() const { return anchorY; }             // base da escada para o Y-sort

    // Regra da catraca: pé dentro do tapete subindo → em cima (anota a base);
    // descendo → no chão. velocityY em px/s (negativo = subindo na tela).
    static void ApplyCrossing(const Rect& zone, float zoneAnchorY, const Vec2& foot, float velocityY,
                              bool& isElevated, float& stairAnchorY);

private:
    float anchorY;                                           // base Y da escada (vem do Tiled)
};

#endif