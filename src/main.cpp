#include "core/CrashHandler.h"
#include "core/Game.h"
#include "states/TitleState.h"
#include <iostream>

int main(int argc, char** argv) {
    CrashHandler::Install();                // PRIMEIRA coisa: captura crashes/logs
    Game& game = Game::GetInstance();       // Pega a instância única do jogo
    game.Push(new TitleState());
    game.Run();                             // Executa o Game Loop
    CrashHandler::Shutdown();               // saída limpa: fecha o log de sessão
    return 0;                               // Fim do Programa
}