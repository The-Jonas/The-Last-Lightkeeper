// ─────────────────────────────────────────────────────────────────────────────
//  Dicas que valem em qualquer andar: documentos e pasta, combustível
//  (acabando, vazio, reabastecer, limite) e o vão da escada sem tábua.
// ─────────────────────────────────────────────────────────────────────────────
#include "tutorial/FloorScript.h"
#include "tutorial/HintSystem.h"
#include "states/stage/StageState.h"

namespace {

using Emotion = DialogueBox::Emotion;

constexpr float kLowFuelRatio    = 0.25f;                // mesmo patamar da chama "nível 4" da HUD
constexpr float kLowFuelDelay    = 1.5f;                 // s com a carga baixa e tudo calmo até falar
constexpr float kLowFuelRingTime = 5.0f;                 // s do anel em volta da chama
constexpr float kFuelLimitRing   = 3.5f;                 // s do anel na roda ao recusar o 3º combustível
constexpr float kReloadRingTime  = 4.5f;                 // s do anel na chama depois da 1ª recarga
constexpr float kFolderDelay     = 1.0f;                 // s depois da fala do 1º documento até a dica da pasta
constexpr float kFuelLimitLineGap = 4.0f;                // s mínimos entre duas falas de "não cabe mais"
constexpr int   kMaxHoleLines    = 2;                    // "preciso de algo…" por andar

class CommonHints : public FloorScript {
public:
    void Update(StageState& stage, HintSystem& hints, const HintContext& ctx, float dt) override {
        UpdateDocuments(stage, hints, ctx);
        UpdateWheel(hints, ctx);
        UpdateFuel(stage, hints, ctx, dt);
        UpdateStairHole(stage, hints, ctx);
    }

private:
    int   prevDocuments = -1;                            // -1 = ainda não leu (1º frame do andar)
    bool  prevReloading = false;                         // para notar a recarga terminando
    float lowFuelTimer = 0.0f;
    float lastFuelLimitLine = -100.0f;                   // TimeInLevel da última fala de limite
    bool  holeArmed = true;
    int   holeLines = 0;

    void UpdateDocuments(StageState& stage, HintSystem& hints, const HintContext& ctx);
    void UpdateWheel(HintSystem& hints, const HintContext& ctx);
    void UpdateFuel(StageState& stage, HintSystem& hints, const HintContext& ctx, float dt);
    void UpdateStairHole(StageState& stage, HintSystem& hints, const HintContext& ctx);
};

// Primeiro documento (qualquer um): o irmãozão comenta que vai guardar; logo
// depois o slot da pasta ganha o anel e "[Tab] Documentos" até ela ser aberta.
void CommonHints::UpdateDocuments(StageState& stage, HintSystem& hints, const HintContext& ctx) {
    if (prevDocuments < 0) prevDocuments = ctx.documentsCollected;   // save carregado não conta como "pegou agora"
    if (ctx.documentsCollected > prevDocuments && hints.Once("doc_first")) {
        hints.Say(stage, "doc_first", "Ao guardar o primeiro documento",
                  {HintSystem::Big("Vou guardar isso na pasta. Pode ser útil depois.")});
    }
    prevDocuments = ctx.documentsCollected;

    HintSpec folder;
    folder.id         = "folder";
    folder.pending    = hints.Learned("doc_first") && ctx.documentsCollected > 0;
    folder.learned    = ctx.folderOpen;
    folder.glyphAfter = kFolderDelay;
    folder.keys       = "[Tab]";                         // tecla fixa (TAB_KEY), não é uma ação remapeável
    folder.label      = "Documentos";
    folder.anchorHud  = HudSlot::Folder;
    folder.belowHud   = true;
    folder.ring       = HudSlot::Folder;
    hints.Stuck(folder);
    
}

// Roda: com o 2º item (agora tem o que girar), as setas e o [F] Usar aparecem
// na própria roda, com o anel, até o jogador girar uma vez.
void CommonHints::UpdateWheel(HintSystem& hints, const HintContext& ctx) {
    HintSpec wheel;
    wheel.id         = "wheel";
    wheel.pending    = ctx.itemCount >= 2 && ctx.controllingBig;
    wheel.learned    = ctx.itemCount >= 2 && ctx.cyclePressed;
    wheel.glyphAfter = 2.5f;                                            // deixa o "pegou" acontecer antes
    wheel.ring       = HudSlot::Wheel;
    wheel.hudKeys    = HudSlot::Wheel;
    wheel.linger     = 3.0f;                                            // continua um pouco depois do 1º giro
    hints.Stuck(wheel);
}

// Carga baixa (fala + anel na chama), recarga com [R] (luz baixa ou vazia e
// combustível na bolsa), "acabou" sem combustível e o 3º combustível recusado
// (fala do irmãozão + anel na roda).
void CommonHints::UpdateFuel(StageState& stage, HintSystem& hints, const HintContext& ctx, float dt) {
    const bool reloaded = prevReloading && !ctx.reloading;
    prevReloading = ctx.reloading;

    const bool beatsFree = hints.IsIdle() && !hints.CommonBeatsHeld();

    // Acabando: uma vez por jogo. É aviso de sobrevivência — fala mesmo com outra
    // dica na tela (a fala só espera diálogo/silêncio e a cena de abertura).
    const bool low = ctx.lightOn && ctx.lighterCharge > 0.0f && ctx.lighterCharge < kLowFuelRatio;
    const bool canWarn = !hints.IsSilent() && !hints.CommonBeatsHeld();
    lowFuelTimer = (low && canWarn) ? lowFuelTimer + dt : 0.0f;
    if (lowFuelTimer > kLowFuelDelay && hints.Once("fuel_low")) {
        hints.Say(stage, "fuel_low", "Com o isqueiro fraco",
                  {HintSystem::Big("O fluido tá acabando...", Emotion::Doubt, Emotion::Fear)});
        hints.FocusHud(HudSlot::Fuel, kLowFuelRingTime);
    }

    // Vazio sem combustível na bolsa: uma vez, para o jogador ir procurar.
    const bool empty = ctx.hasLighter && ctx.lighterCharge <= 0.0f;
    if (empty && ctx.fuelInBag == 0 && beatsFree && hints.Once("fuel_none")) {
        hints.Say(stage, "fuel_none", "Com o isqueiro vazio",
                  {HintSystem::Big("Acabou o fluido... preciso achar mais.", Emotion::Fear, Emotion::Fear)});
    }

    // Recarregar: luz na mão baixa (só depois do aviso "tá acabando") ou o isqueiro
    // vazio, com combustível na bolsa → isqueiro + combustível + [R] "Recarregar".
    // Vazio, o irmãozão fala antes.
    const bool heldLow = ctx.heldLightCharge >= 0.0f && ctx.heldLightCharge < kLowFuelRatio &&
                         hints.Learned("fuel_low");
    HintSpec reload;
    reload.id          = "reload";
    reload.pending     = ctx.canReload && !ctx.reloading && ctx.controllingBig && (empty || heldLow);
    reload.learned     = reloaded;
    reload.lineAfter   = empty ? 1.0f : -1.0f;
    reload.line        = {HintSystem::Big("Preciso pôr mais fluido na luz.", Emotion::Doubt)};
    reload.lineContext = "Com a luz fraca";
    reload.glyphAfter  = empty ? 0.5f : 1.5f;
    reload.keys        = HintSystem::Key(GameAction::Reload);
    reload.icon        = ctx.reloadTargetIcon;           // a luz que vai receber (isqueiro ou lamparina)
    reload.icon2       = ctx.fuelIcon;
    reload.label       = "Recarregar";
    reload.anchor      = ctx.bigHead;
    reload.hasAnchor   = true;
    reload.ring        = HudSlot::Fuel;                  // só aparece se a chama estiver na tela
    reload.priority    = 2;                              // sobrevivência: passa na frente da roda/pasta
    hints.Stuck(reload);

    // 1ª recarga: o anel volta na chama da HUD para mostrar que ela encheu.
    if (reloaded && hints.Once("reload_hud")) {
        hints.FocusHud(HudSlot::Fuel, kReloadRingTime);
    }

    // 3º combustível recusado: sempre o anel; a fala, com um intervalo mínimo.
    if (hints.ConsumeFuelLimitHit()) {
        hints.FocusHud(HudSlot::Wheel, kFuelLimitRing);
        if (!ctx.dialogueActive && hints.TimeInLevel() - lastFuelLimitLine > kFuelLimitLineGap) {
            lastFuelLimitLine = hints.TimeInLevel();
            hints.Say(stage, "fuel_limit", "Ao tentar levar mais combustível",
                      {HintSystem::Big("Não consigo carregar mais combustível... a bolsa tá pesada.",
                                       Emotion::Doubt)});
        }
    }
}

// No vão da escada: com a tábua na bolsa e não na mão, ensina a segurá-la; sem
// a tábua: comenta (no máximo kMaxHoleLines por andar),
// de novo só depois de sair de perto.
void CommonHints::UpdateStairHole(StageState& stage, HintSystem& hints, const HintContext& ctx) {

    // Com a tábua na bolsa mas não na mão: ícone da tábua + "Segure na mão",
    // com as setas e o anel na roda, até ela ir para a mão.
    HintSpec hold;
    hold.id          = "hold_plank";
    hold.pending     = ctx.atHoleNotHolding;
    hold.learned     = ctx.atHoleReady;
    hold.lineAfter   = 0.5f;
    hold.line        = {HintSystem::Big("Devo conseguir usar a tábua aqui.", Emotion::Doubt)};
    hold.lineContext = "No vão da escada";
    hold.glyphAfter  = 0.5f;
    hold.icon        = ctx.holeItemIcon;
    hold.label       = "Segure na mão";
    hold.anchor      = ctx.bigHead;
    hold.hasAnchor   = true;
    hold.ring        = HudSlot::Wheel;
    hold.hudKeys     = HudSlot::Wheel;
    hold.priority    = 2;                                // está parado no vão: passa na frente
    hints.Stuck(hold);  

    if (!ctx.atHoleWithoutPlank) {
        holeArmed = true;
        return;
    }
    if (holeArmed && holeLines < kMaxHoleLines && !hints.IsSilent()) {
        holeArmed = false;
        ++holeLines;
        hints.Say(stage, "stair_hole", "No vão da escada",
                  {HintSystem::Big("Preciso de algo para consertar esse buraco...", Emotion::Doubt)});
    }
}

}  // namespace

std::unique_ptr<FloorScript> MakeCommonHints() {
    return std::make_unique<CommonHints>();
}
