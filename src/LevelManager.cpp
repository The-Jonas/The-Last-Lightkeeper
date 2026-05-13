#include "../include/LevelManager.h"
#include "../include/Camera.h"
#include <SDL2/SDL_image.h>
#include <cmath>
#include <algorithm>
#include <limits>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <fstream>
#include <iostream>

LevelManager::LevelManager() {
    // Inicializamos os ponteiros como nulos por segurança
}

LevelManager::~LevelManager() {
    // O destrutor limpa todas as texturas do vetor automaticamente
    for (auto& layer : imageLayers) {
        if (layer.texture != nullptr) {
            SDL_DestroyTexture(layer.texture);
        }
    }
    imageLayers.clear();
    rectColliders.clear();
    polyColliders.clear();
    circleColliders.clear();
}

void LevelManager::LoadLevel(std::string path, SDL_Renderer* renderer) {
    std::ifstream file(path);
    if (!file.is_open()) return;

    json j;
    file >> j;

    // Limpa a fase anterior
    for (auto& layer : imageLayers) {
        SDL_DestroyTexture(layer.texture);
    }
    imageLayers.clear();
    rectColliders.clear();
    polyColliders.clear();
    circleColliders.clear();

    // Lemos as camadas na ordem em que o Tiled exportou!
    for (auto& layer : j["layers"]) {
        
        // ==========================================
        // SE FOR UMA CAMADA DE IMAGEM (Chão, Parede)
        // ==========================================
        if (layer["type"] == "imagelayer") {
            ImageLayer imgLayer;
            
            // Pega o offset se existir (senão é 0)
            imgLayer.x = layer.contains("offsetx") ? (int)layer["offsetx"] : 0;
            imgLayer.y = layer.contains("offsety") ? (int)layer["offsety"] : 0;
            imgLayer.w = layer.contains("imagewidth") ? (int)layer["imagewidth"] : 0;
            imgLayer.h = layer.contains("imageheight") ? (int)layer["imageheight"] : 0;

            // O Tiled salva como "../img/...". Nossa engine precisa de "Recursos/img/..."
            std::string imagePath = layer["image"];
            size_t pos = imagePath.find("../");
            if (pos != std::string::npos) {
                imagePath.replace(pos, 3, "Recursos/");
            }

            // Carrega a textura e guarda no nosso vetor
            imgLayer.texture = IMG_LoadTexture(renderer, imagePath.c_str());
            if (imgLayer.texture == nullptr) {
                std::cout << "Erro ao carregar textura do Tiled: " << imagePath << std::endl;
            } else {
                imageLayers.push_back(imgLayer);
            }
        }
        
        // ==========================================
        // SE FOR A CAMADA DE COLISÃO
        // ==========================================
        else if (layer["type"] == "objectgroup") {
            float layerOffsetX = layer.contains("offsetx") ? (float)layer["offsetx"] : 0.0f;
            float layerOffsetY = layer.contains("offsety") ? (float)layer["offsety"] : 0.0f;

            for (auto& obj : layer["objects"]) {
                float finalX = (float)obj["x"] + layerOffsetX;
                float finalY = (float)obj["y"] + layerOffsetY;
                
                if (obj.contains("polygon")) {
                    Polygon poly;
                    for (auto& p : obj["polygon"]) {
                        poly.vertices.push_back({ (int)(finalX + (float)p["x"]), 
                                                  (int)(finalY + (float)p["y"]) });
                    }
                    // Só adiciona se for um polígono válido (pelo menos um triângulo)
                    if (poly.vertices.size() >= 3) {
                        polyColliders.push_back(poly);
                    }
                } else if (obj.contains("ellipse")) {
                    Circle c;
                    // Usa .value() para evitar crashes se a propriedade não existir
                    c.radius = (int)obj.value("width", 0.0f) / 2;
                    c.center.x = (int)finalX + c.radius;
                    c.center.y = (int)finalY + c.radius;
                    if (c.radius > 0) circleColliders.push_back(c);
                } else {
                    SDL_Rect r;
                    r.x = (int)finalX;
                    r.y = (int)finalY;
                    r.w = obj.value("width", 0.0f);
                    r.h = obj.value("height", 0.0f);
                    if (r.w > 0 && r.h > 0) rectColliders.push_back(r);
                }
            }
        }
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
    // Trava de segurança: Um poligono precisa de no mínimo 3 pontos para ter volume
    if (p1.vertices.size() < 3 || p2.vertices.size() < 3) return false;

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
            float minA = std::numeric_limits<float>::max();
            float maxA = std::numeric_limits<float>::lowest();
            for (const auto& p : p1.vertices) {
                // Produto escalar para projetar
                float projection = (p.x * normalX) + (p.y * normalY);
                minA = std::min(minA, projection);
                maxA = std::max(maxA, projection);
            }

            float minB = std::numeric_limits<float>::max();
            float maxB = std::numeric_limits<float>::lowest();
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

void LevelManager::RenderBackground(SDL_Renderer* renderer) {
    // Como o vetor guardou as imagens na ordem do JSON (Parede -> Chão),
    // ele vai desenhar automaticamente na ordem certa!
    for (const auto& imgLayer : imageLayers) {
        if (imgLayer.texture != nullptr) {
            SDL_Rect destRect = { 
                (int)(-Camera::pos.x + imgLayer.x), 
                (int)(-Camera::pos.y + imgLayer.y), 
                imgLayer.w, imgLayer.h
            };
            SDL_RenderCopy(renderer, imgLayer.texture, nullptr, &destRect);
        }
    }
}

bool LevelManager::CheckCollision(const Circle& entityCircle) {
    // Checagem contra Retângulos do Cenário 
    for (const auto& rectCol : rectColliders) {
        if (CheckRectVsCircle(rectCol, entityCircle)) {
            return true;
        }
    }

    // Checagem contra outros Círculos do Cenário
    for (const auto& circleCol : circleColliders) {
        int dx = entityCircle.center.x - circleCol.center.x;
        int dy = entityCircle.center.y - circleCol.center.y;
        int distSq = (dx * dx) + (dy * dy);
        int rSum = entityCircle.radius + circleCol.radius;
        if (distSq < (rSum * rSum)) return true;
    }

    // Checagem contra Polígonos (As paredes diagonais do Tiled)
    for (const auto& polyCol : polyColliders) {
        if (CheckPolygonVsCircle(polyCol, entityCircle)) {
            return true;
        }
    }

    return false; // Caminho livre!
}

bool LevelManager::CheckPolygonVsCircle(const Polygon& poly, const Circle& circle) {
    if (poly.vertices.empty()) return false;

    float cx = (float)circle.center.x;
    float cy = (float)circle.center.y;
    float radiusSq = (float)(circle.radius * circle.radius);

    // Teste 1: O centro do círculo está DE DENTRO do polígono? (Ray-casting algorithm)
    bool inside = false;
    for (size_t i = 0, j = poly.vertices.size() - 1; i < poly.vertices.size(); j = i++) {
        float p1x = (float)poly.vertices[i].x, p1y = (float)poly.vertices[i].y;
        float p2x = (float)poly.vertices[j].x, p2y = (float)poly.vertices[j].y;

        if (((p1y > cy) != (p2y > cy)) &&
            (cx < (p2x - p1x) * (cy - p1y) / (p2y - p1y) + p1x)) {
            inside = !inside;
        }
    }
    if (inside) return true;

    // Teste 2: O círculo está esbarrando (raspando) em alguma das paredes do polígono?
    for (size_t i = 0; i < poly.vertices.size(); i++) {
        float p1x = (float)poly.vertices[i].x, p1y = (float)poly.vertices[i].y;
        float p2x = (float)poly.vertices[(i + 1) % poly.vertices.size()].x, p2y = (float)poly.vertices[(i + 1) % poly.vertices.size()].y;

        float lineLenSq = (p2x - p1x) * (p2x - p1x) + (p2y - p1y) * (p2y - p1y);
        if (lineLenSq == 0) continue; // Previne divisão por zero se a linha for um ponto
        
        // Acha o ponto exato da parede que está mais perto do jogador
        float dot = (((cx - p1x) * (p2x - p1x)) + ((cy - p1y) * (p2y - p1y))) / lineLenSq;
        
        float closestX, closestY;
        if (dot < 0) {
            closestX = p1x; closestY = p1y;
        } else if (dot > 1) {
            closestX = p2x; closestY = p2y;
        } else {
            closestX = p1x + (dot * (p2x - p1x));
            closestY = p1y + (dot * (p2y - p1y));
        }

        float distX = cx - closestX;
        float distY = cy - closestY;
        float distSq = (distX * distX) + (distY * distY);

        if (distSq < radiusSq) return true; // Bateu na parede!
    }

    return false;
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