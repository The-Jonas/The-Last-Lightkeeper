#include "core/InputManager.h"
#include <iostream>

InputManager::InputManager(){

    // Inicializa o array do mouse
    for(int i = 0; i < 6; i++){
        mouseState[i] = false;
        mouseUpdate[i] = 0;
    }

    // Inicializando as variáveis
    quitRequested = false;
    updateCounter = 0;
    mouseX = 0;
    mouseY = 0;
}

InputManager::~InputManager(){
    // Nada aqui
}

InputManager& InputManager::GetInstance(){              // Ao invés de dar new InputManager...
    static InputManager instance;                       // Declarando variável estática
    return instance;                                    // e retornando ela (Meyers' Singleton)
}

void InputManager::Update(){
    updateCounter ++;                                   // Incrementa o contador de frame
    SDL_GetMouseState(&mouseX, &mouseY);                // Obtém as coordenadas do mouse
    quitRequested = false;                              // Reinicia a flag de quit

    SDL_Event event;
    while (SDL_PollEvent(&event)){
        switch (event.type)
        {
        case SDL_KEYDOWN:
            // Se a tecla não for uma repetição, atualiza estado
            if (event.key.repeat == 0){
                keyState[event.key.keysym.sym] = true;
                keyUpdate[event.key.keysym.sym] = updateCounter;
            }
            break;
        
        case SDL_KEYUP:
            // Uma tecla foi solta
            keyState[event.key.keysym.sym] = false;
            keyUpdate[event.key.keysym.sym] = updateCounter;
            break;

        case SDL_MOUSEBUTTONDOWN:
            // Pressionamento de botão do mouse
            mouseState[event.button.button] = true;
            mouseUpdate[event.button.button] = updateCounter;
            break;

        case SDL_MOUSEBUTTONUP: 
            // Botão do mouse foi solto
            mouseState[event.button.button] = false;
            mouseUpdate[event.button.button] = updateCounter;
            break;

        case SDL_QUIT:
            // Clicar no X, Alt + F4, etc.
            quitRequested = true;
            break;
        }
    }
}

/*--------------------------------------------------------------------------------------
* _Press e _Release estão interessados no pressionamento ocorrido
naquele frame, e só devem retornar true nesse caso.
* Is_Down retorna se o botão/tecla está pressionado, independente de quando isso ocorreu
--------------------------------------------------------------------------------------*/


//Metodos de Consulta de estado (Teclado)                               
bool InputManager::KeyPress(int key){
    // Retorna true se a chave existe, está pressionada, e a última atualização
    // ocorreu neste frame. Ou seja, retorna true quando a tecla foi pressionada                           
    return keyState.find(key) != keyState.end() && keyState[key] && keyUpdate[key] == updateCounter;                                                                    
}                                                                       

bool InputManager::KeyRelease(int key){
    // Retorna true se a chave existe, não está pressionada, e a última atualização
    // ocorreu neste frame. Ou seja, retorna true quando a tecla foi solta
    return keyState.find(key) != keyState.end() && !keyState[key] && keyUpdate[key] == updateCounter;
}

bool InputManager::IsKeyDown(int key){      
    // Retorna true se a chave existe na tabela e o estado é 'true'
    // Ou seja, retorna true se a tecla estiver pressionada, não se importa em qual frame foi
    // pressionada, apenas com o estado atual                           
    return keyState.find(key) != keyState.end() && keyState[key];                                                           
}

//Metodos de Consulta de estado (Mouse) -- mesma lógica do teclado, porém utilizando arrays ao invés de tabelas hash
bool InputManager::MousePress(int button){
    return mouseState[button] && mouseUpdate[button] == updateCounter;
}

bool InputManager::MouseRelease(int button){
    return !mouseState[button] && mouseUpdate[button] == updateCounter;
}

bool InputManager::IsMouseDown(int button){
    return mouseState[button];
}

//----------------------------------------------------------------------------------

int InputManager::GetMouseX(){
    return mouseX;
}

int InputManager::GetMouseY(){
    return mouseY;
}

bool InputManager::QuitRequested(){
    return quitRequested;
}