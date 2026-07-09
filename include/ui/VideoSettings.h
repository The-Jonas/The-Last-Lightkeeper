#ifndef VIDEO_SETTINGS_H
#define VIDEO_SETTINGS_H

#define INCLUDE_SDL
#include "SDL_include.h"

class InputManager;

// Overlay de vídeo compartilhado pelo menu principal e pelo menu de pausa.
// Mostra "Modo de tela" (ciclo), "Resolução" (dropdown) e um botão "Aplicar"
// que salva e pergunta se o jogador quer reiniciar agora ou depois. As mudanças
// de vídeo só valem após reiniciar (fluxo de "Aplicar").
namespace VideoSettings {
void Open();
void Close();
bool IsOpen();
void HandleInput(InputManager& input);
void Render(SDL_Renderer* renderer);
}  // namespace VideoSettings

#endif  // VIDEO_SETTINGS_H
