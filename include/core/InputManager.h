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
#define TAB_KEY SDLK_TAB
#define TOGGLE_MODE_KEY SDLK_q
#define LIGHT_SHAPE_CYCLE_KEY SDLK_k
#define LIGHT_PANEL_TOGGLE_KEY SDLK_p
#define LIGHTS_TOGGLE_KEY SDLK_l
#define SHADOWS_TOGGLE_KEY SDLK_o
#define MUSIC_MUTE_TOGGLE_KEY SDLK_m
#define MAP_PHYSICS_DEBUG_KEY SDLK_b
#define THUNDER_TEST_KEY SDLK_t
#define CURSOR_PREVIEW_LIGHT_TOGGLE_KEY SDLK_x
#define CREATE_LIGHT_KEY SDLK_c
#define VOICE_TEST_KEY SDLK_v          // debug: toca uma fala aleatoria do irmao controlado
#define MONSTER_BLIND_TOGGLE_KEY SDLK_i  // debug: fica invisivel para o monstro (observar comportamento)
#define FREE_CAMERA_TOGGLE_KEY SDLK_g    // debug: camera livre seguindo o mouse
#define PANEL_ROW_PREV_KEY SDLK_LEFTBRACKET
#define PANEL_ROW_NEXT_KEY SDLK_RIGHTBRACKET
#define LEFT_MOUSE_BUTTON SDL_BUTTON_LEFT

// Ações de gameplay remapeáveis (4.1). A navegação de menus e as teclas de
// debug NÃO passam por aqui — continuam fixas.
enum class GameAction {
    MoveUp, MoveDown, MoveLeft, MoveRight,
    Interact, UseItem, CyclePrev, CycleNext,
    SwapBrother, ToggleMode,
    Count
};

class InputManager {
public:
 
    static InputManager& GetInstance();                             // Padrão de projeto Singleton (Meyers Singleton)

    void Update();                                                  // Atualiza a lógica a cada frame

    // Métodos para o teclado
    bool KeyPress(int key);
    bool KeyRelease(int key);
    bool IsKeyDown(int key);

    // Consulta por AÇÃO (resolve a tecla atualmente vinculada — 4.1)
    bool ActionPress(GameAction action);
    bool ActionDown(GameAction action);
    int GetBinding(GameAction action) const;
    void SetBinding(GameAction action, int keycode);
    void ResetBindingsToDefault();
    static constexpr int ActionCount = static_cast<int>(GameAction::Count);
    static const char* ActionName(GameAction action);   // chave estável p/ settings.json
    static const char* ActionLabel(GameAction action);  // rótulo p/ a UI

    // Captura de remapeamento: retorna a primeira tecla pressionada neste frame
    // (ou 0 se nenhuma). Usada pela tela de Controles.
    int PollAnyKeyPressed();

    // Métodos para o mouse 
    bool MousePress(int button);
    bool MouseRelease(int button);
    bool IsMouseDown(int button);

    // Retorna as Coordenadas do Mouse
    int GetMouseX();
    int GetMouseY();

    int GetMouseWheel();

    bool QuitRequested();                                           // Retorna se o usuário pediu pra sair do jogo

private:
    
    // Construtor e Destrutor para o Singleton  
    InputManager();
    ~InputManager();

    // Membros para o estado do teclado (usando tabela de hash)
    std::unordered_map<int, bool> keyState;
    std::unordered_map<int, int> keyUpdate;

    int bindings[static_cast<int>(GameAction::Count)];   // tecla vinculada a cada ação
    void InitDefaultBindings();

    // Membros para o estado do mouse
    bool mouseState[6]; 
    int mouseUpdate[6];

    // Outros membros...
    bool quitRequested;
    int updateCounter;
    int mouseX, mouseY;
    int mouseWheel;

};


#endif