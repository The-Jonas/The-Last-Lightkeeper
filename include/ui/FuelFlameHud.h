#ifndef FUEL_FLAME_HUD_H
#define FUEL_FLAME_HUD_H
#include <string>

class Inventory;
struct SDL_Renderer;

class FuelFlameHud {
public:
    void Update(float dt);                                                                      // Avança a animação
    void Render(SDL_Renderer* renderer, Inventory& inventory, int windowW, int windowH);        // Só desenha se o item ativo tiver combustível

private:
    static int LevelForRatio(float ratio);                                                      // 1 (cheio) a 4 (quase apagando)
    static std::string FramePath(int level, int frame);                 

    float frameTimer = 0.0f;
    int currentFrame = 0;                                                                       // 0, 1 ou 2
};
#endif