// ─────────────────────────────────────────────────────────────────────────────
//  2º andar — por enquanto só as dicas que já existiam (troca de irmão e
//  habilidade do irmãozinho), no formato novo. A janela quebrando continua em
//  StageState::UpdatePendingWindowBreak. Próximo passo: vela, olho e armário.
// ─────────────────────────────────────────────────────────────────────────────
#include "tutorial/FloorScript.h"
#include "tutorial/HintSystem.h"
#include "states/stage/StageState.h"

namespace {

constexpr float kSwapFarDistance = 660.0f;               // irmãos longe: a troca resolve

class Floor2Script : public FloorScript {
public:
    void Update(StageState& stage, HintSystem& hints, const HintContext& ctx, float dt) override {
        (void)stage;
        (void)dt;
        const bool swapped = prevControllingBig >= 0 && (prevControllingBig == 1) != ctx.controllingBig;
        prevControllingBig = ctx.controllingBig ? 1 : 0;

        // Trocar: com os irmãos longe, tecla em cima de quem NÃO está sendo controlado.
        HintSpec swap;
        swap.id         = "swap";
        swap.pending    = ctx.partyReady && ctx.brothersDistance > kSwapFarDistance;
        swap.learned    = swapped;
        swap.glyphAfter = 1.5f;
        swap.keys       = HintSystem::Key(GameAction::SwapBrother);
        swap.label      = "Trocar de irmão";
        swap.anchor     = ctx.companionHead;
        swap.hasAnchor  = true;
        hints.Stuck(swap);

        // Habilidade: controlando o irmãozinho e ainda sem usar.
        HintSpec ability;
        ability.id         = "ability";
        ability.pending    = !ctx.controllingBig;
        ability.learned    = !ctx.controllingBig && ctx.useItemPressed;
        ability.glyphAfter = 1.0f;
        ability.keys       = HintSystem::Key(GameAction::UseItem);
        ability.label      = "Habilidade";
        ability.anchor     = ctx.smallHead;
        ability.hasAnchor  = true;
        hints.Stuck(ability);
    }

private:
    int prevControllingBig = -1;                         // -1 = 1º frame
};

}  // namespace

std::unique_ptr<FloorScript> MakeFloor2Script() {
    return std::make_unique<Floor2Script>();
}
