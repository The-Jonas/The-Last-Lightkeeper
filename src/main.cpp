#include "core/CrashHandler.h"
#include "core/Telemetry.h"
#include "core/Game.h"
#include "states/TitleState.h"
#include <iostream>

int main(int argc, char** argv) {
    CrashHandler::Install();                // PRIMEIRA coisa: captura crashes/logs
    Telemetry::Begin();                     // abre o registo de playtest da sessao
    Game& game = Game::GetInstance();       // Pega a instância única do jogo
    game.Push(new TitleState());
    game.Run();                             // Executa o Game Loop
    Telemetry::End("normal");               // fecha o registo antes de sair
    CrashHandler::Shutdown();               // saída limpa: fecha o log de sessão
    return 0;                               // Fim do Programa
}
