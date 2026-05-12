#include "../include/LevelManager.h"
#include <fstream>
#include <iostream>

// Lembre-se de usar seu TextureManager para carregar as imagens!

void LevelManager::LoadLevel(std::string path, SDL_Renderer* renderer) {
    std::ifstream file(path);
    if (!file.is_open()) return;

    json j;
    file >> j;

    // 1. Limpa dados anteriores caso esteja trocando de andar
    rectColliders.clear();
    polyColliders.clear();
    circleColliders.clear();

    // 2. No Tiled, você criará camadas de objetos. Vamos iterar por elas.
    for (auto& layer : j["layers"]) {
        if (layer["type"] == "objectgroup") {
            for (auto& obj : layer["objects"]) {
                
                // CASO 1: Polígono
                if (obj.contains("polygon")) {
                    Polygon poly;
                    float objX = obj["x"];
                    float objY = obj["y"];
                    for (auto& p : obj["polygon"]) {
                        poly.vertices.push_back({ (int)(objX + (float)p["x"]), 
                                                  (int)(objY + (float)p["y"]) });
                    }
                    polyColliders.push_back(poly);
                }
                // CASO 2: Círculo (No Tiled, elipses com W=H)
                else if (obj.contains("ellipse")) {
                    Circle c;
                    c.radius = (int)obj["width"] / 2;
                    c.center.x = (int)obj["x"] + c.radius;
                    c.center.y = (int)obj["y"] + c.radius;
                    circleColliders.push_back(c);
                }
                // CASO 3: Retângulo Simples
                else {
                    SDL_Rect r;
                    r.x = obj["x"];
                    r.y = obj["y"];
                    r.w = obj["width"];
                    r.h = obj["height"];
                    rectColliders.push_back(r);
                }
            }
        }
    }
    
    // 3. Aqui é pra carregar as texturas de fundo e paredes
    // floorTexture = IMG_LoadTexture(renderer, "assets/mapa1_chao.jpg");
    // wallTexture = IMG_LoadTexture(renderer, "assets/mapa1_parede.jpg");
}

bool LevelManager::CheckCollision(const SDL_Rect& entityBox) {
    // Checagem contra retângulos (Mais fácil porque é nativa da SDL)
    for (const auto& rectCol : rectColliders) {
        if (SDL_HasIntersection(&entityBox, &rectCol)) {
            return true;
        }
    }

    // Checagem contra Círculos
    for (const auto& circleCol : circleColliders) {
        if (CheckRectVsCircle(entityBox, circleCol)) {
            return true;
        }
    }

    // Checagem contra Polígonos (SAT)
    // Transforma-se temporariamente o SDL_React do jogador em um Polígono

    Polygon entityPoly;
    entityPoly.vertices = {
        {entityBox.x, entityBox.y},
        {entityBox.x + entityBox.w, entityBox.y},
        {entityBox.x + entityBox.w, entityBox.y + entityBox.h},
        {entityBox.x, entityBox.y + entityBox.h}
    };

    for (const auto& polyCol : polyColliders) {
        if (CheckPolygonVsPolygon(entityPoly, polyCol)) {
            return true;
        }
    }

    return false; // Se não bateu em nada, o caminho está livre!
}

bool LevelManager::CheckRectVsCircle(const SDL_Rect& rect, const Circle& circle) {
    // Acha o ponto X e Y mais próximo ao centro do círculo dentro do retângulo
    int closestX = std::clamp(circle.center.x, rect.x, rect.x + rect.w);
    int closestY = std::clamp(circle.center.y, rect.y, rect.y + rect.h);

    // Calcula a distância entre o centro do círculo e esse ponto mais próximo
    int distanceX = circle.center.x - closestX;
    int distanceY = circle.center.y - closestY;

    // Teorema de Pitágoras (Não vou usar raiz quadrada pra não sobrecarregar o processador)
    int distanceSquared = (distanceX * distanceX) + (distanceY * distanceY);
    int radiusSquared = circle.radius * circle.radius;

    return distanceSquared < radiusSquared;
}

bool LevelManager::CheckPolygonVsPolygon(const Polygon& p1, const Polygon& p2) {
    // Primeiro testar os eixos (normais) de ambos os polígonos
    const Polygon* polys[2] = {&p1, &p2};

    for (int i = 0; i < 2; i++) {
        const Polygon& poly = *polys[i];
        for (size_t j = 0; j < poly.vertices.size(); j++) {
            // Pega o ponto atual e o próximo pra formar aresta
            SDL_Point pA = poly.vertices[j];
            SDL_Point pB = poly.vertices[(j + 1) % poly.vertices.size()];

            // Vetor de aresta
            float edgeX = pB.x - pA.x;
            float edgeY = pB.y - pA.y;

            // A Normal é o vetor perpendicular à aresta (-Y, X)
            float normalX = -edgeY;
            float normalY = edgeX;
            
            // Projeta os dois polígonos nesse eixo Normal
            float minA = INFINITY, maxA = -INFINITY;
            for (const auto& p : p1.vertices) {
                // Produto escalar para projetar
                float projection = (p.x * normalX) + (p.y * normalY);
                minA = std::min(minA, projection);
                maxA = std::max(maxA, projection);
            }

            float minB = INFINITY, maxB = -INFINITY;
            for (const auto& p : p2.vertices) {
                float projection = (p.x * normalX) + (p.y * normalY);
                minB = std::min(minB, projection);
                maxB = std::max(maxB, projection);
            }

            // Se as projeções não se sobrepõem, existe um vão entre eles!
            // Logo, não estão colidindo.
            if (maxA < minB || maxB < minA) {
                return false; 
            }
        }
    }
    
    // Se testou todos os eixos e nenhuma teve vão livre, estão colidindo.
    return true;
}