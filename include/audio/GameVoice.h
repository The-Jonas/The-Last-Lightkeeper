#ifndef GAME_VOICE_H
#define GAME_VOICE_H

#include <string>

// Falas dos irmãos (dublagem). Toca em UM único canal de voz por vez (nunca
// sobrepõe falas) com um cooldown global para não tagarelar. Cada gatilho
// abaixo é chamado pelos sistemas de gameplay nos momentos certos.
namespace GameVoice {

void NotifyLoadingBegin();   // silencia/para as falas durante carregamento
void NotifyLoadingEnd();
void StopAll();              // corta a fala atual (morte/troca de cena)

// Legenda (subtitle) da fala em reprodução. Como só existe UMA fala por vez e
// cada gatilho conhece seu texto, dá para legendar exatamente: retorna true e
// preenche `out` enquanto o canal de voz estiver tocando; false quando em
// silêncio. A UI (StageState) consulta isso a cada frame.
bool GetActiveSubtitle(std::string& out);

// Irmãozão (big brother)
void OnCallToFollow();       // "Aqui" — chama o irmãozinho para segui-lo
void OnItemPickup();         // ocasionalmente comenta ao pegar um item
void OnBagFull();            // "minha bolsa tá pesada" — bolsa encheu
void OnDragObject();         // às vezes, ao começar a arrastar um objeto
void OnActionBlocked();      // "não consigo" — apertou interagir e não pôde agir
void OnScoldFear();          // repreende o irmãozinho medroso ("para de ser medroso" / "quer que o monstro te coma?")

// Irmãozinho (little brother)
void OnAskToStay();          // protesta quando mandado ficar parado
void OnBrothersTooFar();     // com medo quando os irmãos se afastam demais
void OnHidingMonsterClose(); // escondido no armário e o monstro se aproxima
void OnHidingWhatIfMonster(); // #4 "E se o monstro estiver lá fora?" — sussurro aleatório no armário

// DEBUG: toca uma fala ALEATÓRIA do irmão indicado (true = irmãozão, false =
// irmãozinho), ignorando cooldown/mute e cortando a fala atual. Usado só pelo
// atalho de desenvolvedor (VOICE_TEST_KEY) em builds de debug.
void DebugPlayRandomForControlled(bool bigBrother);

} // namespace GameVoice

#endif
