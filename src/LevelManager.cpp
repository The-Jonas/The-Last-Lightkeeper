#include "../include/LevelManager.h"
#include "../include/Camera.h"
#include <SDL2/SDL_image.h>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <fstream>
#include <iostream>

LevelManager::LevelManager() {
    // Inicializamos os ponteiros como nulos por segurança
    floorTexture = nullptr;
    wallTexture = nullptr;
}

LevelManager::~LevelManager() {
    // Quando o andar for destruído, limpamos as texturas da placa de vídeo 
    // para não causar vazamento de memória (memory leak)
    if (floorTexture != nullptr) {
        SDL_DestroyTexture(floorTexture);
    }
    if (wallTexture != nullptr) {
        SDL_DestroyTexture(wallTexture);
    }

    // Limpamos também as listas de colisores
    rectColliders.clear();
    polyColliders.clear();
    circleColliders.clear();
}

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

            float layerOffsetX = layer.contains("offsetx") ? (float)layer["offsetx"] : 0.0f;
            float layerOffsetY = layer.contains("offsety") ? (float)layer["offsety"] : 0.0f;

            // ESTES SÃO OS MESMOS VALORES QUE VOCÊ USOU NO RENDER FLOOR!
            // Se você mudar lá no Render, tem que mudar aqui também.
            float mapGlobalOffsetX = 350.0f; 
            float mapGlobalOffsetY = 1100.0f;

            for (auto& obj : layer["objects"]) {

                // Soma o X/Y do objeto com o deslocamento da camada APENAS UMA VEZ
                float finalX = (float)obj["x"] + layerOffsetX + mapGlobalOffsetX;
                float finalY = (float)obj["y"] + layerOffsetY + mapGlobalOffsetY;
                
                // CASO 1: Polígono
                if (obj.contains("polygon")) {
                    Polygon poly;
                    for (auto& p : obj["polygon"]) {
                        poly.vertices.push_back({ (int)(finalX + (float)p["x"]), 
                                                  (int)(finalY + (float)p["y"]) });
                    }
                    polyColliders.push_back(poly);
                }
                // CASO 2: Círculo (No Tiled, elipses com W=H)
                else if (obj.contains("ellipse")) {
                    Circle c;
                    c.radius = (int)obj["width"] / 2;
                    c.center.x = (int)finalX + c.radius;
                    c.center.y = (int)finalY + c.radius;
                    circleColliders.push_back(c);
                }
                // CASO 3: Retângulo Simples
                else {
                    SDL_Rect r;
                    r.x = (int)finalX;
                    r.y = (int)finalY;
                    r.w = obj["width"];
                    r.h = obj["height"];
                    rectColliders.push_back(r);
                }
            }
        }
    }
    
    floorTexture = IMG_LoadTexture(renderer, "Recursos/img/cenario/mapa1_chao.png");
    wallTexture = IMG_LoadTexture(renderer, "Recursos/img/cenario/mapa1_parede.png");

    if (floorTexture == nullptr || wallTexture == nullptr) {
        std::cout << "Erro ao carregar texturas do mapa no LevelManager!" << std::endl;
    }
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

void LevelManager::RenderFloor(SDL_Renderer* renderer) {
    if (floorTexture == nullptr) return;

    int offsetX = 350; // Exemplo: empurra o chão 350 pixels pra direita
    int offsetY = 1100; // Exemplo: empurra o chão 500 pixels pra baixo

    // Criamos o retângulo de destino baseado no tamanho real do seu mapa
    // Subtraímos a posição da câmera para o mapa "correr" conforme o player anda
    SDL_Rect destRect = { 
        (int)(-Camera::pos.x + offsetX), 
        (int)(-Camera::pos.y + offsetY), 
        4357, 3276  
    };

    SDL_RenderCopy(renderer, floorTexture, nullptr, &destRect);
}

void LevelManager::RenderWalls(SDL_Renderer* renderer) {
    if (wallTexture == nullptr) return;

    // A parede usa a mesma lógica do chão para ficarem alinhados
    SDL_Rect destRect = { 
        (int)(-Camera::pos.x), 
        (int)(-Camera::pos.y), 
        5066, 4399
    };

    SDL_RenderCopy(renderer, wallTexture, nullptr, &destRect);
}

void LevelManager::RenderDebug(SDL_Renderer* renderer){
#ifdef DEBUG
    // Cor vermelha para as hitbox do cenário
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);

    // 1. Desenha Retângulos
    for (auto& r : rectColliders) {
        SDL_Rect screenRect = { 
            (int)(r.x - Camera::pos.x), 
            (int)(r.y - Camera::pos.y), 
            r.w, r.h 
        };
        SDL_RenderDrawRect(renderer, &screenRect);
    }

    // 2. Desenha Polígonos (Linha por linha)
    for (auto& poly : polyColliders) {
        for (size_t i = 0; i < poly.vertices.size(); i++) {
            SDL_Point p1 = poly.vertices[i];
            SDL_Point p2 = poly.vertices[(i + 1) % poly.vertices.size()];
            SDL_RenderDrawLine(renderer, 
                p1.x - (int)Camera::pos.x, p1.y - (int)Camera::pos.y, 
                p2.x - (int)Camera::pos.x, p2.y - (int)Camera::pos.y);
        }
    }

    // 3. Desenha Círculos de verdade
    for (auto& c : circleColliders) {
        int cx = c.center.x - (int)Camera::pos.x;
        int cy = c.center.y - (int)Camera::pos.y;
        int r = c.radius;

        // Desenhamos 36 linhas curtas para formar um círculo suave
        const int kSeg = 36; 
        for (int i = 0; i < kSeg; i++) {
            float a0 = ((float)i / kSeg) * 2.0f * M_PI;
            float a1 = ((float)(i + 1) / kSeg) * 2.0f * M_PI;
            
            SDL_RenderDrawLine(renderer, 
                cx + (int)(cos(a0) * r), cy + (int)(sin(a0) * r), 
                cx + (int)(cos(a1) * r), cy + (int)(sin(a1) * r));
        }        
    }
#endif
}