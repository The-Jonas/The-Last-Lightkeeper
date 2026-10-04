#ifndef FUEL_FLAME_HUD_H
#define FUEL_FLAME_HUD_H
#define INCLUDE_SDL
#include "SDL_include.h"
#include <string>

class Inventory;

class FuelFlameHud {
public:
    void Update(float dt);                                                                      // Avança a animação
    void Render(SDL_Renderer* renderer, Inventory& inventory, int windowW, int windowH);        // Só desenha se o item ativo tiver combustível
    bool GetLastRect(SDL_FRect& out) const;                                                     // onde a chama foi desenhada neste frame (aura do tutorial)
    const std::string& GetLastFramePath() const { return lastFramePath; }                       // PNG do quadro desenhado (silhueta da aura)
    float GetLastGlow() const { return lastGlow; }                                              // força da aura pelo nível da chama (1 cheia … 0.45 quase apagando)

private:
    static int LevelForRatio(float ratio);                                                      // 1 (cheio) a 4 (quase apagando)
    static std::string FramePath(int level, int frame);                 

    float frameTimer = 0.0f;
    int currentFrame = 0;                                                                       // 0, 1 ou 2
    SDL_FRect lastRect{0.0f, 0.0f, 0.0f, 0.0f};
    bool lastRectValid = false;                                                                 // false quando a chama não apareceu
    std::string lastFramePath;
    float lastGlow = 1.0f;
};
#endif

