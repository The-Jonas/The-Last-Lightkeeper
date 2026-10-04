#ifndef HINT_CONTEXT_H
#define HINT_CONTEXT_H

#include "math/Vec2.h"

#include <string>

// ─────────────────────────────────────────────────────────────────────────────
//  HintContext — o que as dicas enxergam do jogo, montado uma vez por frame
//  (StageState::BuildHintContext, em src/tutorial/HintContext.cpp).
//
//  Os roteiros dos andares SÓ leem isto, nunca o StageState direto: se uma
//  dica nova precisar de uma informação, ela entra aqui primeiro.
// ─────────────────────────────────────────────────────────────────────────────
struct HintContext {
    // ── Andar e silêncio ─────────────────────────────────────────────────────
    int  level = 0;                          // 0 = 1º andar
    bool dialogueActive = false;             // caixa de diálogo ou dublagem tocando
    bool overlayOpen = false;                // pausa, documento ou pasta (input congelado)
    bool monsterHunting = false;             // perseguição: nenhuma dica
    bool controllingBig = true;

    // ── Irmãos (pontos no MUNDO, para ancorar as teclas) ─────────────────────
    bool  partyReady = false;
    bool  partyTogether = true;              // false = irmãozinho parado (Q)
    float brothersDistance = 0.0f;
    Vec2  bigHead, bigHands, bigFeet;
    Vec2  smallHead;
    Vec2  companionHead;                     // quem NÃO está sendo controlado
    bool  moveInput = false;                 // alguma tecla de andar apertada
    bool  useItemPressed = false;            // [UseItem] neste frame (habilidade do irmãozinho)

    // ── Luz ──────────────────────────────────────────────────────────────────
    bool  lightOn = false;                   // luz de mão acesa e com carga
    bool  hasLighter = false;
    bool  lighterInHand = false;             // isqueiro no centro da roda
    float lighterCharge = 0.0f;              // 0..1 (0 = vazio)
    float heldLightCharge = -1.0f;           // carga da luz na mão 0..1 (-1 = não é luz)
    std::string lighterIcon;                 // sprite do isqueiro

    // ── Roda de itens ────────────────────────────────────────────────────────
    int  itemCount = 0;                      // itens na bolsa (2+ = tem o que girar)
    bool cyclePressed = false;               // girou a roda neste frame

    // ── Combustível ──────────────────────────────────────────────────────────
    int  fuelInBag = 0;                      // unidades na bolsa
    bool canReload = false;                  // [R] faria algo agora (combustível + luz que cabe)
    bool reloading = false;                  // recarga em andamento (luz apagada)
    std::string reloadTargetIcon;            // sprite da luz que o [R] recarregaria agora
    std::string fuelIcon;                    // sprite do combustível (vazio se não tem)

    // ── Documentos ───────────────────────────────────────────────────────────
    int  documentsCollected = 0;
    bool folderOpen = false;

    // ── Escada ───────────────────────────────────────────────────────────────
    bool atHoleWithoutPlank = false;         // no vão da escada sem a tábua
    bool atHoleNotHolding = false;           // no vão com a tábua na bolsa, mas não na mão
    bool atHoleReady = false;                // no vão com a tábua na mão ([E] Consertar)
    std::string holeItemIcon;                // sprite do item que conserta (tábua)
};

#endif
