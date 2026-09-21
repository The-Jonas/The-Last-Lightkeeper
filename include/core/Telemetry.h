#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <string>

// ─────────────────────────────────────────────────────────────────────────────
//  Telemetry — registo do que o jogador FAZ, para as sessoes de playtest.
//
//  O CrashHandler responde a pergunta "porque e que o jogo rebentou?". Este
//  modulo responde a outra: "o que e que o jogador fez, onde se perdeu, onde
//  morreu, o que nunca chegou a encontrar?".
//
//  FORMATO: JSON Lines (.jsonl) — UM objecto JSON por linha, gravado e
//  descarregado no disco na hora. Nao e um array JSON de proposito: um array
//  so fica valido quando se fecha o "]", e uma sessao que rebenta (ou um
//  alt-F4) deixaria o ficheiro inteiro ilegivel. Em JSONL um fim abrupto custa,
//  no pior caso, a ultima linha.
//
//  FICHEIRO: logs/playtest/<id-da-sessao>.jsonl. Uma sessao = um ficheiro = uma
//  execucao do jogo. O tester envia a pasta inteira no fim.
//
//  TRES ESPECIES DE LINHA (campo "k"):
//    • "meta"   — uma por sessao, a primeira linha: quem, que build, que
//                 maquina, que definicoes.
//    • "event"  — algo aconteceu (apanhou item, morreu, acendeu vela...).
//    • "sample" — fotografia do estado, uma por segundo de jogo. E daqui que
//                 saem os mapas de calor, as curvas de sanidade e o FPS.
//
//  DESLIGAR: variavel de ambiente TLL_TELEMETRY=0. Identificar o tester:
//  TLL_TESTER=nome, ou {"tester":"nome"} em config/playtest.json.
//
//  NAO e seguro para varias threads — o jogo chama isto tudo da thread
//  principal, e o custo por evento tem de ficar perto de zero.
// ─────────────────────────────────────────────────────────────────────────────
namespace Telemetry {

/// Montador dos campos de uma linha. Escreve JSON directamente, sem construir
/// uma arvore pelo meio: um evento custa umas dezenas de bytes de string.
///
///     Telemetry::Event("item_pickup", Telemetry::Fields()
///         .Str("item", def.name).Int("level", idx).Pos("", pos.x, pos.y));
class Fields {
public:
    Fields& Str(const char* key, const std::string& value);
    Fields& Str(const char* key, const char* value);
    Fields& Num(const char* key, double value);      ///< 3 casas decimais
    Fields& Int(const char* key, long long value);
    Fields& Bool(const char* key, bool value);
    /// Par de coordenadas: `Pos("mob", x, y)` da "mobX" e "mobY". Com `key`
    /// vazio da "x" e "y" — o caso normal, a posicao do jogador.
    Fields& Pos(const char* key, float x, float y);

    const std::string& Json() const { return buf; }

private:
    std::string buf;   ///< ja no formato ,"chave":valor
};

/// Abre o ficheiro da sessao e grava a linha "meta". Chamar uma vez, no inicio
/// do main(), ANTES de o SDL arrancar. Idempotente.
void Begin();

/// Fecha a sessao com uma linha "session_end". `reason` diz como acabou
/// ("normal", "quit_menu", ...). Idempotente.
void End(const char* reason);

/// False quando TLL_TELEMETRY=0 ou o ficheiro nao abriu. Todas as funcoes
/// abaixo sao no-op nesse caso — quem chama nao precisa de verificar.
bool IsEnabled();

/// Identificador desta sessao (tambem o nome do ficheiro, sem extensao).
const std::string& SessionId();

/// Caminho do ficheiro desta sessao, para mostrar ao tester.
const std::string& FilePath();

void Event(const char* type);
void Event(const char* type, const Fields& fields);

/// Fotografia do estado. Alem dos campos dados, junta sozinha o FPS medio e o
/// pior frame desde a amostra anterior, e reinicia essa janela.
void Sample(const Fields& fields);

/// Uma vez por frame, no ciclo principal. Alimenta o FPS das amostras e deixa
/// um evento "hitch" quando um frame demora tempo a mais para passar
/// despercebido (engasgo visivel).
void FrameTick(float dt);

/// Segundos desde `Begin()`.
double Now();

// ── Momentos quentes ────────────────────────────────────────────────────────
// A amostra de um em um segundo perde o que importa quando o jogo aperta: uma
// morte inteira cabe entre duas amostras. Enquanto isto estiver ligado, a
// `StageState` amostra quatro vezes mais depressa. O monstro liga-o ao entrar
// em perseguicao e desliga-o ao sair.
void SetIntense(bool on);
bool IsIntense();

/// Motivo do fim da sessao, gravado por `End()` quando o main nao sabe melhor:
/// "window_close" (fechou a janela), "quit_menu" (saiu pelo menu)...
void SetEndReason(const char* reason);

/// Marca no ficheiro que a sessao acabou a rebentar, e fecha-o. Chamado pelo
/// CrashHandler, ja dentro do handler de excepcao: sem isto uma sessao que
/// rebenta fica sem ultima linha e parece apenas uma sessao abandonada.
void NoteCrash(const char* reason);

}  // namespace Telemetry

#endif  // TELEMETRY_H
