#include "core/Game.h"
#include "states/TitleState.h"
#include <iostream>

int main(int argc, char** argv) {
    Game& game = Game::GetInstance();       // Pega a instância única do jogo
    game.Push(new TitleState());
    game.Run();                             // Executa o Game Loop
    return 0;                               // Fim do Programa
}