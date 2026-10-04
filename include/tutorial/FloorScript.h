#ifndef FLOOR_SCRIPT_H
#define FLOOR_SCRIPT_H

#include "tutorial/HintContext.h"

#include <memory>

class StageState;
class HintSystem;

// ─────────────────────────────────────────────────────────────────────────────
//  Roteiro de tutorial de um andar (src/tutorial/floors/FloorN.cpp).
//
//  Enter: o andar acabou de ser montado (antes de um save ser aplicado por
//  cima — o que o save tiver vence). Update: a cada frame, descreve as dicas
//  com HintSystem::Stuck e dispara as falas roteirizadas com Once + Say.
//
//  Andar novo = arquivo novo + uma linha em MakeFloorScript. Mecânica que vale
//  em qualquer andar vai em CommonHints.cpp.
// ─────────────────────────────────────────────────────────────────────────────
class FloorScript {
public:
    virtual ~FloorScript() = default;
    virtual void Enter(StageState& stage, HintSystem& hints) { (void)stage; (void)hints; }
    virtual void Update(StageState& stage, HintSystem& hints, const HintContext& ctx, float dt) = 0;
};

std::unique_ptr<FloorScript> MakeCommonHints();          // combustível, pasta, escada…
std::unique_ptr<FloorScript> MakeFloor1Script();         // escuro, isqueiro, andar, juntar
std::unique_ptr<FloorScript> MakeFloor2Script();         // troca de irmão e habilidade
std::unique_ptr<FloorScript> MakeFloorScript(int levelIndex);   // nullptr se o andar não tem roteiro

#endif
