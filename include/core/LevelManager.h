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

// Ler as camadas das imagens
struct ImageLayer {
    SDL_Texture* texture;
    int x, y;
    int w, h;
};

// Guarda a "receita" de qualquer entidade do jogo
struct EntitySpawn {
    std::string type;
    std::string name;       
    float x, y;
    float w = 0.0f;
    float h = 0.0f;
    bool isStatic;
    int z;

    std::unordered_map<std::string, json> properties;
};

class LevelManager {
public:
    LevelManager();
    ~LevelManager();

    // Carrega o JSON exportado pelo Tiled
    void LoadLevel(std::string path, SDL_Renderer* renderer);

    // A função principal que o Player vai chamar
    bool CheckCollision(const SDL_Rect& entityBox, bool isElevated = false);

    // Permite testar a colisão enviando um Círculo ao invés de um Retângulo!
    bool CheckCollision(const Circle& entityCircle, bool isElevated = false);

    void RenderBackground(SDL_Renderer* renderer);
    void RenderDebug(SDL_Renderer* renderer);
    /// Map collision from Tiled (runtime; not tied to DEBUG preprocessor).
    void RenderCollisionOverlay(SDL_Renderer* renderer) const;

    // Getters para o sistema de colisão usar depois
    std::vector<SDL_Rect>& GetRectColliders();
    std::vector<Polygon>& GetPolyColliders();
    std::vector<Circle>& GetCircleColliders();

    // Lista de Spawns
    std::vector<EntitySpawn> entitySpawns;

    bool escadaConsertada = false;

private:
    // Vetor pra guardar todas as imagens na ordem certa
    std::vector<ImageLayer> imageLayers;              

    // Funções matemáticas auxiliares para resolver cada tipo de forma
    bool CheckRectVsCircle(const SDL_Rect& rect, const Circle& circle);
    bool CheckPolygonVsPolygon(const Polygon& p1, const Polygon& p2);

    // Ferramenta matemática para testar um polígono contra um círculo
    bool CheckPolygonVsCircle(const Polygon& poly, const Circle& circle);

    // Listas de colisores baseadas no que o Tiled exporta
    std::vector<SDL_Rect> rectColliders;
    std::vector<Polygon> chaoNormal;
    std::vector<Polygon> chaoEscada;
    std::vector<Polygon> chaoBuraco;
    std::vector<Circle> circleColliders;

    
};

#endif