#ifndef LEVEL_MANAGER_H
#define LEVEL_MANAGER_H

#define INCLUDE_SDL
#include "SDL_include.h"
#include "nlohmann/json.hpp"
#include "math/Vec2.h"
#include "audio/GameSfx.h"

#include <string>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;

struct Polygon {                                         // paredes diagonais / formas do Tiled
    std::vector<SDL_Point> vertices;
};

struct Circle {                                          // pilares redondos; também o pé de quem anda
    Vec2 center;
    float radius;
};

struct ImageLayer {                                      // camada de imagem do Tiled (parede, chão)
    SDL_Texture* texture;
    int x, y;
    int w, h;
};

// "Receita" de uma entidade do mapa (camada Entidades), montada pela SpawnFactory.
struct EntitySpawn {
    std::string type;                                    // class/type do Tiled
    std::string name;
    int   tiledId = -1;
    float x, y;
    float w = 0.0f;
    float h = 0.0f;
    bool  isStatic;
    int   z;
    bool  flipH = false;                                 // flips decodificados do gid
    bool  flipV = false;
    float rotation = 0.0f;                               // graus, horário
    int   gid = 0;                                       // gid sem os bits de flip (imagem do tileset)
    std::unordered_map<std::string, json> properties;    // propriedades customizadas
};

// ─────────────────────────────────────────────────────────────────────────────
//  Mapa de um andar: lê o JSON do Tiled (imagens, colisão, zonas de passos,
//  entidades, gatilhos) e responde às perguntas de colisão.
//
//  Camadas lidas: imagelayer (arte) · Collision (paredes, escada, buraco,
//  move_levels, stone_floor, repairable_trigger) · Collision_Obj (móveis) ·
//  FootstepZones (piso de pedra) · Entidades (spawns).
// ─────────────────────────────────────────────────────────────────────────────
class LevelManager {
public:
    LevelManager() = default;
    ~LevelManager();

    void LoadLevel(const std::string& path, SDL_Renderer* renderer);   // troca o andar inteiro

    // ── Colisão ──────────────────────────────────────────────────────────────
    // isElevated = na escada: só o corrimão (e o buraco, se a escada estiver quebrada) colidem.
    bool CheckCollision(const SDL_Rect& entityBox, bool isElevated = false);
    bool CheckCollision(const Circle& entityCircle, bool isElevated = false);
    Vec2 GetCirclePushVector(const Circle& entityCircle, bool isElevated = false);   // empurrão para sair de dentro
    bool CheckRepairableTrigger(const SDL_Rect& entityBox);                           // encosta num "repairable_trigger"

    // ── Consultas ────────────────────────────────────────────────────────────
    FootstepSurface QueryFootstepSurface(int x, int y, bool isElevated) const;       // escada / pedra / madeira
    bool GetWorldBounds(float& outMinX, float& outMinY, float& outMaxX, float& outMaxY) const;   // área da arte (limite da câmera)
    const std::string* GetTileImagePath(int gid) const;                               // imagem do tileset p/ um gid

    // ── Desenho ──────────────────────────────────────────────────────────────
    void RenderBackground(SDL_Renderer* renderer);
    void RenderCollisionOverlay(SDL_Renderer* renderer) const;                        // debug [B]

    std::vector<EntitySpawn> entitySpawns;               // camada Entidades
    std::vector<EntitySpawn> levelTransitionZones;       // retângulos "move_levels"
    bool escadaConsertada = false;                       // buraco da escada consertado

private:
    void ClearLevel();                                   // libera texturas e esvazia tudo
    void LoadTilesets(const json& j, const std::string& mapPath);   // gid → imagem (inclui .tsx)
    void ParseTsx(const std::string& tsxPath, int firstgid);
    void LoadImageLayer(const json& layer, SDL_Renderer* renderer);
    void LoadCollisionLayer(const json& layer, float offX, float offY);
    void LoadStaticObjectLayer(const json& layer, float offX, float offY);
    void LoadFootstepZones(const json& layer, float offX, float offY);
    void LoadEntities(const json& layer, float offX, float offY);

    bool CheckRectVsCircle(const SDL_Rect& rect, const Circle& circle) const;
    bool CheckPolygonVsRect(const Polygon& poly, const SDL_Rect& rect) const;     // serve para côncavos
    bool CheckPolygonVsCircle(const Polygon& poly, const Circle& circle) const;

    std::unordered_map<int, std::string> gidToImagePath;
    std::vector<ImageLayer> imageLayers;                 // na ordem do Tiled (fundo primeiro)

    std::vector<SDL_Rect> rectColliders;                 // paredes retangulares
    std::vector<Circle>   circleColliders;               // pilares redondos
    std::vector<Polygon>  chaoNormal;                    // paredes em polígono
    std::vector<Polygon>  chaoEscada;                    // corrimão da escada (valem só em cima dela)
    std::vector<Polygon>  chaoBuraco;                    // buraco da escada quebrada
    std::vector<Polygon>  objPolyColliders;              // móveis em polígono (Collision_Obj)
    std::vector<SDL_Rect> objRectColliders;              // móveis retangulares (Collision_Obj)
    std::vector<Polygon>  floorStoneZones;               // piso de pedra (só som de passos)
    std::vector<Polygon>  repairableTriggers;            // zonas de conserto
};

#endif