#include "../include/Game.h"
#include "../include/TitleState.h"
#include <iostream>
#include <filesystem>

int main(int argc, char** argv) {
    Game& game = Game::GetInstance();       // Pega a instância única do jogo
    game.Push(new TitleState());
    game.Run();                             // Executa o Game Loop
    return 0;                               // Fim do Programa
}