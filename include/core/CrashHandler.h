#ifndef CRASH_HANDLER_H
#define CRASH_HANDLER_H

#include <string>

// ─────────────────────────────────────────────────────────────────────────────
//  CrashHandler — diagnóstico de travamentos "aleatórios" no build do jogador.
//
//  O build de distribuição usa -mwindows (SEM console), então qualquer
//  std::cerr/printf normalmente se perde e um crash (ex.: acesso inválido de
//  memória) simplesmente fecha a janela sem deixar rastro. Este módulo:
//
//    1. Instala um handler de crash (SEH no Windows, signals no resto) que,
//       ao travar, grava "crash-reports/crash_<data>.txt" com o tipo de erro,
//       o endereço que falhou, a PILHA DE CHAMADAS e as últimas "migalhas"
//       (breadcrumbs) — além de um minidump ".dmp" para depuração profunda.
//    2. Redireciona stdout/stderr para "logs/latest.log" quando não há console
//       (build do jogador), capturando de graça todos os std::cerr existentes.
//    3. Mantém um rastro de breadcrumbs (CrashHandler::Log) mostrando o que o
//       jogo estava fazendo pouco antes de travar.
//
//  Uso: chame CrashHandler::Install() como PRIMEIRA linha de main(). Depois use
//  CrashHandler::Log("...") em pontos importantes (troca de estado, load de
//  fase, etc.). Nada mais é obrigatório.
// ─────────────────────────────────────────────────────────────────────────────
namespace CrashHandler {

// Instala handlers de crash e redireciona logs. Chamar uma única vez, o mais
// cedo possível em main(). Idempotente.
void Install();

// Registra uma "migalha" (breadcrumb): vai para o log de sessão e para um buffer
// circular que é despejado no relatório de crash. Estilo printf.
void Log(const char* fmt, ...);

// Igual a Log, mas recebe uma string pronta.
void Breadcrumb(const std::string& msg);

// Fecha/descarrega os logs de forma limpa numa saída normal (opcional).
void Shutdown();

}  // namespace CrashHandler

#endif  // CRASH_HANDLER_H
