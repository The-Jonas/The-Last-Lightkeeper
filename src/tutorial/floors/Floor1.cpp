// ─────────────────────────────────────────────────────────────────────────────
//  1º andar — o loop básico, sem monstro.
//
//  Entra no escuro, irmãozinho paralisado na porta (modo separado).
//    1. Acender: fala do irmãozinho → [usar] + isqueiro + "Usar item".
//    2. Andar: logo depois da luz, até o jogador andar de verdade.
//    3. Medo: "Eu tô com medo…" / "Tá tudo bem, pode vir." → tecla de juntar
//       na cabeça do irmãozinho.
//  Enquanto a cena de abertura não termina, as dicas comuns (combustível,
//  documento) esperam, para não empilhar falas.
// ─────────────────────────────────────────────────────────────────────────────
#include "tutorial/FloorScript.h"
#include "tutorial/HintSystem.h"
#include "states/stage/StageState.h"

namespace {

using Emotion = DialogueBox::Emotion;

constexpr float kFearDistance = 260.0f;                  // irmãozão se afastou: o irmãozinho fala
constexpr float kFearAfterLit = 8.0f;                    // ou depois de tanto tempo com a luz acesa
constexpr float kMoveLearnTime = 2.5f;                   // s andando até a dica de andar sumir

class Floor1Script : public FloorScript {
public:
    // Abertura: o irmãozinho fica parado (um save carregado depois sobrescreve).
    void Enter(StageState& stage, HintSystem& hints) override {
        if (!hints.Learned("f1_join")) stage.SetPartyTogether(false);
    }

    void Update(StageState& stage, HintSystem& hints, const HintContext& ctx, float dt) override {
        if (!hints.Learned("f1_join")) hints.HoldCommonBeats();

        UpdateLight(hints, ctx);
        if (!hints.Learned("light")) return;             // o resto do andar espera a primeira luz

        if (ctx.lightOn) litTime += dt;
        if (ctx.moveInput && ctx.controllingBig) walkTime += dt;
        UpdateMove(hints, ctx);
        UpdateFearAndJoin(stage, hints, ctx);
    }

private:
    float litTime = 0.0f;
    float walkTime = 0.0f;                               // s com tecla de andar apertada

    void UpdateLight(HintSystem& hints, const HintContext& ctx);
    void UpdateMove(HintSystem& hints, const HintContext& ctx);
    void UpdateFearAndJoin(StageState& stage, HintSystem& hints, const HintContext& ctx);
};

// Isqueiro na mão, com carga e apagado.
void Floor1Script::UpdateLight(HintSystem& hints, const HintContext& ctx) {
    HintSpec light;
    light.id          = "light";
    light.pending     = ctx.controllingBig && ctx.lighterInHand && ctx.lighterCharge > 0.0f && !ctx.lightOn;
    light.learned     = ctx.lightOn;
    light.lineAfter   = 0.5f;
    light.line        = {HintSystem::Small("Tá muito escuro... você ainda tem o isqueiro?", Emotion::Fear)};
    light.lineContext = "No escuro, ao chegar no farol";
    light.glyphAfter  = 0.3f;                            // logo depois da fala
    light.keys        = HintSystem::Key(GameAction::UseItem);
    light.icon        = ctx.lighterIcon;
    light.label       = "Usar item";
    light.priority    = 2;
    light.anchor      = ctx.bigHead;
    light.hasAnchor   = true;
    hints.Stuck(light);
}

// Logo depois da primeira luz: as quatro teclas, até kMoveLearnTime s andando.
void Floor1Script::UpdateMove(HintSystem& hints, const HintContext& ctx) {
    HintSpec move;
    move.id         = "move";
    move.pending    = ctx.controllingBig;
    move.learned    = walkTime >= kMoveLearnTime;
    move.glyphAfter = 0.5f;
    move.label      = "Andar";
    move.keys       = HintSystem::Key(GameAction::MoveUp) + "\n" +                       // T invertido
                      HintSystem::Key(GameAction::MoveLeft) + " " + HintSystem::Key(GameAction::MoveDown) + " " +
                      HintSystem::Key(GameAction::MoveRight);
    move.anchor     = ctx.bigHead;
    move.hasAnchor  = true;
    hints.Stuck(move);
}

// Conversa do medo assim que tudo estiver calmo; depois a tecla de juntar
// fica na cabeça do irmãozinho até ele vir. Q antes da conversa pula tudo.
void Floor1Script::UpdateFearAndJoin(StageState& stage, HintSystem& hints, const HintContext& ctx) {
    if (!hints.Learned("f1_fear")) {
        if (ctx.partyTogether) {
            hints.MarkLearned("f1_fear");
            hints.MarkLearned("f1_join");
            return;
        }
        const bool ready = ctx.brothersDistance > kFearDistance || litTime > kFearAfterLit;
        if (!ready || !hints.IsIdle() || !hints.Once("f1_fear")) return;
        hints.Say(stage, "f1_fear", "Ao chegar no farol", {
            HintSystem::Small("Eu tô com medo...", Emotion::Fear, Emotion::Normal),
            HintSystem::Big("Tá tudo bem, pode vir.", Emotion::Normal, Emotion::Fear),
        });
        return;
    }

    HintSpec join;
    join.id         = "f1_join";
    join.pending    = !ctx.partyTogether;
    join.learned    = ctx.partyTogether;
    join.glyphAfter = 0.3f;                              // logo depois da conversa
    join.keys       = HintSystem::Key(GameAction::ToggleMode);
    join.label      = "Chamar";
    join.anchor     = ctx.smallHead;
    join.hasAnchor  = true;
    join.anchor2    = ctx.bigHead;                       // também no irmãozão: longe, ele não leria a do pequeno
    join.hasAnchor2 = true;
    hints.Stuck(join);
}

}  // namespace

std::unique_ptr<FloorScript> MakeFloor1Script() {
    return std::make_unique<Floor1Script>();
}
