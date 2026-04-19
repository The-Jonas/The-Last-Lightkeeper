#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H

#define INCLUDE_SDL
#include "SDL_include.h"
#include <unordered_map>

#define LEFT_ARROW_KEY SDLK_LEFT
#define RIGHT_ARROW_KEY SDLK_RIGHT
#define UP_ARROW_KEY SDLK_UP
#define DOWN_ARROW_KEY SDLK_DOWN
#define ESCAPE_KEY SDLK_ESCAPE
#define SPACE_KEY SDLK_SPACE
#define LEFT_MOUSE_BUTTON SDL_BUTTON_LEFT

class InputManager {
public:
 
    static InputManager& GetInstance();                             // Padrão de projeto Singleton (Meyers Singleton)

    void Update();                                                  // Atualiza a lógica a cada frame

    // Métodos para o teclado
    bool KeyPress(int key);
    bool KeyRelease(int key);
    bool IsKeyDown(int key);

    // Métodos para o mouse 
    bool MousePress(int button);
    bool MouseRelease(int button);
    bool IsMouseDown(int button);

    // Retorna as Coordenadas do Mouse
    int GetMouseX();
    int GetMouseY();

    bool QuitRequested();                                           // Retorna se o usuário pediu pra sair do jogo

private:
    
    // Construtor e Destrutor para o Singleton  
    InputManager();
    ~InputManager();

    // Membros para o estado do teclado (usando tabela de hash)
    std::unordered_map<int, bool> keyState;
    std::unordered_map<int, int> keyUpdate; 

    // Membros para o estado do mouse
    bool mouseState[6]; 
    int mouseUpdate[6];

    // Outros membros...
    bool quitRequested;
    int updateCounter;
    int mouseX, mouseY;

};


#endif