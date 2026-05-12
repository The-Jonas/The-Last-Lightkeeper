#ifndef LEVEL_MANAGER_H
#define LEVEL_MANAGER_H

#define INCLUDE_SDL
#include <vector>
#include <string>
#include "SDL_include.h" 
#include "nlohmann/json.hpp"

using json = nlohmann::json;

// Estrutura para Polígonos (diagonais e formas complexas)
struct Polygon {
    std::vector<SDL_Point> vertices;
};

// Estrutura para Círculos (paineis, pilastras redondas)
struct Circle {
    SDL_Point center;
    int radius;
};

class LevelManager {
public:
    LevelManager();
    ~LevelManager();

    // Carrega o JSON exportado pelo Tiled
    void LoadLevel(std::string path, SDL_Renderer* renderer);
    // A função principal que o Player vai chamar
    bool CheckCollision(const SDL_Rect& entityBox);
    
    void RenderFloor(SDL_Renderer* renderer);
    void RenderWalls(SDL_Renderer* renderer);

    // Getters para o sistema de colisão usar depois
    std::vector<SDL_Rect>& GetRectColliders();
    std::vector<Polygon>& GetPolyColliders();
    std::vector<Circle>& GetCircleColliders();

private:
    SDL_Texture* floorTexture;
    SDL_Texture* wallTexture;

    // Funções matemáticas auxiliares para resolver cada tipo de forma
    bool CheckRectVsCircle(const SDL_Rect& rect, const Circle& circle);
    bool CheckPolygonVsPolygon(const Polygon& p1, const Polygon& p2);

    // Listas de colisores baseadas no que o Tiled exporta
    std::vector<SDL_Rect> rectColliders;
    std::vector<Polygon> polyColliders;
    std::vector<Circle> circleColliders;
};

#endif