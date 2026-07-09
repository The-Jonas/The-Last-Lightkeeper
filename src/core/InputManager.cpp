#include "core/InputManager.h"
#include "core/Game.h"
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
    mouseWheel = 0;

    InitDefaultBindings();
}

void InputManager::InitDefaultBindings() {
    bindings[static_cast<int>(GameAction::MoveUp)]      = SDLK_w;
    bindings[static_cast<int>(GameAction::MoveDown)]    = SDLK_s;
    bindings[static_cast<int>(GameAction::MoveLeft)]    = SDLK_a;
    bindings[static_cast<int>(GameAction::MoveRight)]   = SDLK_d;
    bindings[static_cast<int>(GameAction::Interact)]    = SDLK_e;
    bindings[static_cast<int>(GameAction::UseItem)]     = SDLK_f;
    bindings[static_cast<int>(GameAction::CyclePrev)]   = SDLK_LEFT;   // item anterior
    bindings[static_cast<int>(GameAction::CycleNext)]   = SDLK_RIGHT;  // próximo item
    bindings[static_cast<int>(GameAction::SwapBrother)] = SDLK_LCTRL;
    bindings[static_cast<int>(GameAction::ToggleMode)]  = SDLK_q;
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
    SDL_GetMouseState(&mouseX, &mouseY);                // Coordenadas do mouse (pixels da tela)
    // Converte para o ESPAÇO LÓGICO de render (a resolução escolhida), pois o
    // jogo usa SDL_RenderSetLogicalSize — assim mira da lanterna, HUD e cliques
    // batem com o que é desenhado, mesmo com escala/letterbox do fullscreen.
    if (SDL_Renderer* r = Game::GetInstance().GetRenderer()) {
        float lx = 0.0f, ly = 0.0f;
        SDL_RenderWindowToLogical(r, mouseX, mouseY, &lx, &ly);
        mouseX = static_cast<int>(lx);
        mouseY = static_cast<int>(ly);
    }
    mouseWheel = 0;
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

        case SDL_MOUSEWHEEL:
            mouseWheel += event.wheel.y;
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

int InputManager::GetMouseWheel() {
    return mouseWheel;
}

// ---- Ações remapeáveis (4.1) -------------------------------------------------

bool InputManager::ActionPress(GameAction action) {
    return KeyPress(bindings[static_cast<int>(action)]);
}

bool InputManager::ActionDown(GameAction action) {
    return IsKeyDown(bindings[static_cast<int>(action)]);
}

int InputManager::GetBinding(GameAction action) const {
    return bindings[static_cast<int>(action)];
}

void InputManager::SetBinding(GameAction action, int keycode) {
    bindings[static_cast<int>(action)] = keycode;
}

void InputManager::ResetBindingsToDefault() {
    InitDefaultBindings();
}

int InputManager::PollAnyKeyPressed() {
    for (const auto& kv : keyState) {
        if (kv.second && keyUpdate[kv.first] == updateCounter) {
            return kv.first;
        }
    }
    return 0;
}

const char* InputManager::ActionName(GameAction action) {
    switch (action) {
    case GameAction::MoveUp:      return "move_up";
    case GameAction::MoveDown:    return "move_down";
    case GameAction::MoveLeft:    return "move_left";
    case GameAction::MoveRight:   return "move_right";
    case GameAction::Interact:    return "interact";
    case GameAction::UseItem:     return "use_item";
    case GameAction::CyclePrev:   return "cycle_prev";
    case GameAction::CycleNext:   return "cycle_next";
    case GameAction::SwapBrother: return "swap_brother";
    case GameAction::ToggleMode:  return "toggle_mode";
    default:                      return "";
    }
}

const char* InputManager::ActionLabel(GameAction action) {
    switch (action) {
    case GameAction::MoveUp:      return "Mover cima";
    case GameAction::MoveDown:    return "Mover baixo";
    case GameAction::MoveLeft:    return "Mover esquerda";
    case GameAction::MoveRight:   return "Mover direita";
    case GameAction::Interact:    return "Interagir";
    case GameAction::UseItem:     return "Usar item";
    case GameAction::CyclePrev:   return "Item anterior";
    case GameAction::CycleNext:   return "Proximo item";
    case GameAction::SwapBrother: return "Trocar irmao";
    case GameAction::ToggleMode:  return "Modo dupla";
    default:                      return "";
    }
}