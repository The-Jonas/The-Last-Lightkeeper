#include "core/LevelManager.h"
#include "core/CrashHandler.h"
#include "engine/Camera.h"

#include <SDL2/SDL_image.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>

namespace {

constexpr float kPi = 3.14159265f;

// Retângulo do Tiled como polígono de 4 vértices.
Polygon RectPolygon(float x, float y, float w, float h) {
    Polygon p;
    p.vertices = {{static_cast<int>(x), static_cast<int>(y)},
                  {static_cast<int>(x + w), static_cast<int>(y)},
                  {static_cast<int>(x + w), static_cast<int>(y + h)},
                  {static_cast<int>(x), static_cast<int>(y + h)}};
    return p;
}

// Pontos de um objeto "polygon" do Tiled, somados à posição (x, y) do objeto.
Polygon ReadPolygon(const json& obj, float x, float y) {
    Polygon p;
    for (const auto& pt : obj["polygon"]) {
        p.vertices.push_back({static_cast<int>(x + pt.value("x", 0.0f)), static_cast<int>(y + pt.value("y", 0.0f))});
    }
    return p;
}

// Forma de uma zona: o polígono, ou o retângulo (se tiver tamanho). Vazia se nenhum.
Polygon ReadZoneShape(const json& obj, float x, float y) {
    if (obj.contains("polygon")) return ReadPolygon(obj, x, y);
    const float w = obj.value("width", 0.0f), h = obj.value("height", 0.0f);
    return (w > 0.0f && h > 0.0f) ? RectPolygon(x, y, w, h) : Polygon{};
}

// class/type do Tiled (o editor grava um ou outro dependendo da versão).
std::string TiledType(const json& obj) {
    return obj.value("class", obj.value("type", ""));
}

// Ponto dentro do polígono (ray casting; serve para côncavos).
bool PointInPolygon(const Polygon& poly, float px, float py) {
    const size_t n = poly.vertices.size();
    if (n < 3) return false;
    bool inside = false;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const float xi = poly.vertices[i].x, yi = poly.vertices[i].y;
        const float xj = poly.vertices[j].x, yj = poly.vertices[j].y;
        if (((yi > py) != (yj > py)) && (px < (xj - xi) * (py - yi) / (yj - yi) + xi)) inside = !inside;
    }
    return inside;
}

// Caixa envolvente do polígono.
SDL_Rect PolygonBounds(const Polygon& poly) {
    int minX = poly.vertices[0].x, maxX = minX, minY = poly.vertices[0].y, maxY = minY;
    for (const auto& v : poly.vertices) {
        minX = std::min(minX, v.x); maxX = std::max(maxX, v.x);
        minY = std::min(minY, v.y); maxY = std::max(maxY, v.y);
    }
    return SDL_Rect{minX, minY, maxX - minX, maxY - minY};
}

// Caixas (com borda inclusiva) que não se tocam — descarte rápido.
bool BoundsApart(const SDL_Rect& a, int bMinX, int bMinY, int bMaxX, int bMaxY) {
    return a.x + a.w < bMinX || a.x > bMaxX || a.y + a.h < bMinY || a.y > bMaxY;
}

// Ponto de um segmento mais perto de (px, py). False se o segmento for um ponto.
bool ClosestOnSegment(float ax, float ay, float bx, float by, float px, float py, float& outX, float& outY) {
    const float lenSq = (bx - ax) * (bx - ax) + (by - ay) * (by - ay);
    if (lenSq == 0.0f) return false;
    const float t = std::clamp(((px - ax) * (bx - ax) + (py - ay) * (by - ay)) / lenSq, 0.0f, 1.0f);
    outX = ax + t * (bx - ax);
    outY = ay + t * (by - ay);
    return true;
}

// Contornos dos polígonos em coordenadas de tela (debug).
void DrawPolygons(SDL_Renderer* r, const std::vector<Polygon>& polys, float zoom) {
    for (const Polygon& poly : polys) {
        const size_t n = poly.vertices.size();
        for (size_t i = 0; i < n; i++) {
            const SDL_Point a = poly.vertices[i], b = poly.vertices[(i + 1) % n];
            SDL_RenderDrawLineF(r, (a.x - Camera::pos.x) * zoom, (a.y - Camera::pos.y) * zoom,
                                   (b.x - Camera::pos.x) * zoom, (b.y - Camera::pos.y) * zoom);
        }
    }
}

// Retângulos em coordenadas de tela (debug).
void DrawRects(SDL_Renderer* r, const std::vector<SDL_Rect>& rects, float zoom) {
    for (const SDL_Rect& rc : rects) {
        const SDL_FRect s{(rc.x - Camera::pos.x) * zoom, (rc.y - Camera::pos.y) * zoom, rc.w * zoom, rc.h * zoom};
        SDL_RenderDrawRectF(r, &s);
    }
}

// Diretório de um caminho ("." se não houver).
std::string DirNameOf(const std::string& p) {
    const auto s = p.find_last_of("/\\");
    return (s == std::string::npos) ? std::string(".") : p.substr(0, s);
}

// Normaliza barras e colapsa "." e ".." (resolve os "../img/..." dos .tsx).
std::string NormalizePath(const std::string& raw) {
    std::string p = raw;
    std::replace(p.begin(), p.end(), '\\', '/');
    const bool absolute = !p.empty() && p[0] == '/';
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= p.size()) {
        size_t slash = p.find('/', start);
        if (slash == std::string::npos) slash = p.size();
        const std::string seg = p.substr(start, slash - start);
        if (seg == "..") {
            if (!parts.empty() && parts.back() != "..") parts.pop_back();
            else if (!absolute) parts.push_back(seg);
        } else if (!seg.empty() && seg != ".") {
            parts.push_back(seg);
        }
        start = slash + 1;
    }
    std::string out = absolute ? "/" : "";
    for (size_t i = 0; i < parts.size(); ++i) {
        out += parts[i];
        if (i + 1 < parts.size()) out += "/";
    }
    return out;
}

// Valor de attr="..." numa linha de XML; "" se não houver.
std::string ExtractAttr(const std::string& line, const std::string& attr) {
    const std::string key = attr + "=\"";
    const auto pos = line.find(key);
    if (pos == std::string::npos) return "";
    const auto begin = pos + key.size();
    const auto end = line.find('"', begin);
    return (end == std::string::npos) ? "" : line.substr(begin, end - begin);
}

}  // namespace

// ═════════════════════════════════════════════════════════════════════════════
//  Carregamento
// ═════════════════════════════════════════════════════════════════════════════

LevelManager::~LevelManager() {
    ClearLevel();
}

// Destrói as texturas das camadas e esvazia colisores, zonas e spawns.
void LevelManager::ClearLevel() {
    for (auto& layer : imageLayers) {
        if (layer.texture) SDL_DestroyTexture(layer.texture);
    }
    imageLayers.clear();
    rectColliders.clear();
    circleColliders.clear();
    chaoNormal.clear();
    chaoEscada.clear();
    chaoBuraco.clear();
    objPolyColliders.clear();
    objRectColliders.clear();
    floorStoneZones.clear();
    repairableTriggers.clear();
    entitySpawns.clear();
    levelTransitionZones.clear();
    gidToImagePath.clear();
}

// Lê o JSON do Tiled e reconstrói o andar, camada por camada na ordem do arquivo.
void LevelManager::LoadLevel(const std::string& path, SDL_Renderer* renderer) {
    CrashHandler::Log("LoadLevel: %s", path.c_str());
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cout << "Erro: Arquivo do mapa nao encontrado -> " << path << std::endl;
        return;
    }
    try {
        json j;
        file >> j;
        ClearLevel();
        LoadTilesets(j, path);
        if (!j.contains("layers")) return;

        for (const auto& layer : j["layers"]) {
            const std::string type = layer.value("type", "");
            if (type == "imagelayer") {
                LoadImageLayer(layer, renderer);
                continue;
            }
            if (type != "objectgroup" || !layer.contains("objects")) continue;

            const std::string name = layer.value("name", "");
            const float offX = layer.value("offsetx", 0.0f);
            const float offY = layer.value("offsety", 0.0f);
            if (name == "Collision")          LoadCollisionLayer(layer, offX, offY);
            else if (name == "Collision_Obj") LoadStaticObjectLayer(layer, offX, offY);
            else if (name == "FootstepZones") LoadFootstepZones(layer, offX, offY);
            else if (name == "Entidades")     LoadEntities(layer, offX, offY);
        }
    } catch (const std::exception& e) {
        std::cout << "Erro Fatal ao processar o JSON: " << e.what() << std::endl;
    }
}

// Camada de imagem: carrega a textura (o "../" do Tiled vira "Recursos/") com o offset.
void LevelManager::LoadImageLayer(const json& layer, SDL_Renderer* renderer) {
    std::string imagePath = layer.value("image", "");
    if (imagePath.empty()) return;
    const size_t pos = imagePath.find("../");
    if (pos != std::string::npos) imagePath.replace(pos, 3, "Recursos/");

    ImageLayer img;
    img.x = layer.value("offsetx", 0);
    img.y = layer.value("offsety", 0);
    img.w = layer.value("imagewidth", 0);
    img.h = layer.value("imageheight", 0);
    img.texture = IMG_LoadTexture(renderer, imagePath.c_str());
    if (img.texture) imageLayers.push_back(img);
    else std::cout << "Erro ao carregar textura: " << imagePath << std::endl;
}

// Camada "Collision". Polígonos: escada, buraco, madeira (ignorado — madeira é o
// piso padrão) ou parede. Elipses: pilar redondo. Retângulos: "move_levels"
// (troca de andar), "stone_floor" (piso de pedra, só som) ou parede.
// "repairable_trigger" (polígono ou retângulo) é só zona de conserto.
void LevelManager::LoadCollisionLayer(const json& layer, float offX, float offY) {
    for (const auto& obj : layer["objects"]) {
        const std::string type = TiledType(obj);
        const std::string name = obj.value("name", std::string());
        const float x = obj.value("x", 0.0f) + offX;
        const float y = obj.value("y", 0.0f) + offY;
        const float w = obj.value("width", 0.0f);
        const float h = obj.value("height", 0.0f);

        if (type == "repairable_trigger" || name == "repairable_trigger") {
            Polygon trig = ReadZoneShape(obj, x, y);
            if (trig.vertices.size() >= 3) repairableTriggers.push_back(trig);
            continue;
        }

        if (obj.contains("polygon")) {
            Polygon poly = ReadPolygon(obj, x, y);
            if (poly.vertices.size() < 3) continue;
            if (type == "Escada")       chaoEscada.push_back(poly);
            else if (type == "Buraco")  chaoBuraco.push_back(poly);
            else if (type != "Madeira") chaoNormal.push_back(poly);
            continue;
        }

        if (obj.contains("ellipse")) {
            const float radius = w / 2.0f;
            if (radius > 0.0f) circleColliders.push_back(Circle{Vec2(x + radius, y + radius), radius});
            continue;
        }

        if (name == "move_levels") {
            EntitySpawn zone;
            zone.name = name;
            zone.tiledId = obj.value("id", -1);
            zone.x = x;
            zone.y = y;
            zone.w = w;
            zone.h = h;
            if (obj.contains("properties")) {
                for (const auto& prop : obj["properties"]) {
                    if (prop.contains("value")) zone.properties[prop.value("name", "")] = prop["value"];
                }
            }
            if (w > 0.0f && h > 0.0f) levelTransitionZones.push_back(zone);
        } else if (name == "stone_floor") {
            if (static_cast<int>(w) > 0 && static_cast<int>(h) > 0) floorStoneZones.push_back(RectPolygon(x, y, w, h));
        } else {
            const SDL_Rect r{static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h)};
            if (r.w > 0 && r.h > 0) rectColliders.push_back(r);
        }
    }
}

// Camada "Collision_Obj": móveis — polígono (diagonal/irregular) ou retângulo.
void LevelManager::LoadStaticObjectLayer(const json& layer, float offX, float offY) {
    for (const auto& obj : layer["objects"]) {
        const float x = obj.value("x", 0.0f) + offX;
        const float y = obj.value("y", 0.0f) + offY;
        if (obj.contains("polygon")) {
            Polygon poly = ReadPolygon(obj, x, y);
            if (poly.vertices.size() >= 3) objPolyColliders.push_back(poly);
        } else {
            const SDL_Rect r{static_cast<int>(x), static_cast<int>(y),
                             static_cast<int>(obj.value("width", 0.0f)), static_cast<int>(obj.value("height", 0.0f))};
            if (r.w > 0 && r.h > 0) objRectColliders.push_back(r);
        }
    }
}

// Camada "FootstepZones": zonas de pedra ("Pedra"/"Stone"). Madeira é o padrão, não precisa de zona.
void LevelManager::LoadFootstepZones(const json& layer, float offX, float offY) {
    for (const auto& obj : layer["objects"]) {
        const std::string type = TiledType(obj);
        if (type != "Pedra" && type != "Stone") continue;
        Polygon zone = ReadZoneShape(obj, obj.value("x", 0.0f) + offX, obj.value("y", 0.0f) + offY);
        if (zone.vertices.size() >= 3) floorStoneZones.push_back(zone);
    }
}

// Camada "Entidades": um EntitySpawn por objeto. "isStatic" e "z" têm campo
// próprio (padrão false e 2); as demais propriedades vão para `properties`.
void LevelManager::LoadEntities(const json& layer, float offX, float offY) {
    for (const auto& obj : layer["objects"]) {
        EntitySpawn spawn;
        spawn.type     = TiledType(obj);
        spawn.name     = obj.value("name", "");
        spawn.tiledId  = obj.value("id", -1);
        spawn.x        = obj.value("x", 0.0f) + offX;
        spawn.y        = obj.value("y", 0.0f) + offY;
        spawn.w        = obj.value("width", 0.0f);
        spawn.h        = obj.value("height", 0.0f);
        spawn.rotation = obj.value("rotation", 0.0f);
        spawn.isStatic = false;
        spawn.z        = 2;

        // O Tiled guarda os flips nos 3 bits altos do gid.
        const long long rawGid = obj.value("gid", 0LL);
        spawn.flipH = (rawGid & 0x80000000LL) != 0;
        spawn.flipV = (rawGid & 0x40000000LL) != 0;
        spawn.gid   = static_cast<int>(rawGid & 0x1FFFFFFFLL);

        if (obj.contains("properties")) {
            for (const auto& prop : obj["properties"]) {
                if (!prop.contains("value")) continue;
                const std::string pName = prop.value("name", "");
                const json& value = prop["value"];
                if (pName == "isStatic") {
                    if (value.is_boolean()) spawn.isStatic = value.get<bool>();
                } else if (pName == "z") {
                    if (value.is_number()) spawn.z = value.get<int>();
                } else {
                    spawn.properties[pName] = value;
                }
            }
        }
        entitySpawns.push_back(spawn);
    }
}

// gid → imagem para os tilesets do mapa: .tsx externo (relativo ao mapa) ou
// coleção de imagens embutida.
void LevelManager::LoadTilesets(const json& j, const std::string& mapPath) {
    if (!j.contains("tilesets") || !j["tilesets"].is_array()) return;
    const std::string mapDir = DirNameOf(mapPath);
    for (const auto& ts : j["tilesets"]) {
        const int firstgid = ts.value("firstgid", 1);
        if (ts.contains("source")) {
            ParseTsx(NormalizePath(mapDir + "/" + ts["source"].get<std::string>()), firstgid);
        } else if (ts.contains("tiles") && ts["tiles"].is_array()) {
            for (const auto& tile : ts["tiles"]) {
                if (tile.contains("image")) {
                    gidToImagePath[firstgid + tile.value("id", 0)] =
                        NormalizePath(mapDir + "/" + tile["image"].get<std::string>());
                }
            }
        }
    }
}

// Lê um .tsx linha a linha: cada <tile id> seguido de <image source> vira uma entrada.
void LevelManager::ParseTsx(const std::string& tsxPath, int firstgid) {
    std::ifstream f(tsxPath);
    if (!f.is_open()) {
        std::cout << "aviso: tileset .tsx nao encontrado -> " << tsxPath << std::endl;
        return;
    }
    const std::string tsxDir = DirNameOf(tsxPath);
    std::string line;
    int curId = -1;
    while (std::getline(f, line)) {
        if (line.find("<tile ") != std::string::npos) {
            const std::string idStr = ExtractAttr(line, "id");
            if (!idStr.empty()) {
                try { curId = std::stoi(idStr); } catch (...) { curId = -1; }
            }
        }
        if (line.find("<image ") != std::string::npos && curId >= 0) {
            const std::string src = ExtractAttr(line, "source");
            if (!src.empty()) gidToImagePath[firstgid + curId] = NormalizePath(tsxDir + "/" + src);
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Consultas
// ═════════════════════════════════════════════════════════════════════════════

const std::string* LevelManager::GetTileImagePath(int gid) const {
    const auto it = gidToImagePath.find(gid);
    return (it == gidToImagePath.end()) ? nullptr : &it->second;
}

// União das camadas de imagem. False se o andar não tiver nenhuma (câmera sem limite).
bool LevelManager::GetWorldBounds(float& outMinX, float& outMinY, float& outMaxX, float& outMaxY) const {
    bool any = false;
    for (const auto& img : imageLayers) {
        if (!img.texture || img.w <= 0 || img.h <= 0) continue;
        const float x0 = static_cast<float>(img.x), y0 = static_cast<float>(img.y);
        const float x1 = x0 + img.w, y1 = y0 + img.h;
        if (!any) {
            outMinX = x0; outMinY = y0; outMaxX = x1; outMaxY = y1;
            any = true;
        } else {
            outMinX = std::min(outMinX, x0); outMinY = std::min(outMinY, y0);
            outMaxX = std::max(outMaxX, x1); outMaxY = std::max(outMaxY, y1);
        }
    }
    return any;
}

// Escada tem prioridade; depois pedra (zonas); o resto é madeira.
FootstepSurface LevelManager::QueryFootstepSurface(int x, int y, bool isElevated) const {
    if (isElevated) return FootstepSurface::Stairs;
    for (const Polygon& poly : floorStoneZones) {
        if (PointInPolygon(poly, static_cast<float>(x), static_cast<float>(y))) return FootstepSurface::Stone;
    }
    return FootstepSurface::Wood;
}

bool LevelManager::CheckRepairableTrigger(const SDL_Rect& entityBox) {
    return std::any_of(repairableTriggers.begin(), repairableTriggers.end(),
                       [&](const Polygon& p) { return CheckPolygonVsRect(p, entityBox); });
}

// ═════════════════════════════════════════════════════════════════════════════
//  Colisão
// ═════════════════════════════════════════════════════════════════════════════

// Caixa: no chão bate em paredes, pilares e móveis; sempre nos polígonos do
// piso atual (chão ou escada); na escada quebrada, também no buraco.
bool LevelManager::CheckCollision(const SDL_Rect& box, bool isElevated) {
    if (!isElevated) {
        for (const auto& r : rectColliders)    if (SDL_HasIntersection(&box, &r)) return true;
        for (const auto& c : circleColliders)  if (CheckRectVsCircle(box, c)) return true;
        for (const auto& r : objRectColliders) if (SDL_HasIntersection(&box, &r)) return true;
        for (const auto& p : objPolyColliders) if (CheckPolygonVsRect(p, box)) return true;
    }
    for (const auto& p : (isElevated ? chaoEscada : chaoNormal)) {
        if (CheckPolygonVsRect(p, box)) return true;
    }
    if (isElevated && !escadaConsertada) {
        for (const auto& p : chaoBuraco) if (CheckPolygonVsRect(p, box)) return true;
    }
    return false;
}

// Círculo: na escada só o corrimão (e o buraco, se quebrada); no chão paredes,
// pilares, polígonos e móveis.
bool LevelManager::CheckCollision(const Circle& circle, bool isElevated) {
    if (isElevated) {
        for (const auto& p : chaoEscada) if (CheckPolygonVsCircle(p, circle)) return true;
        if (!escadaConsertada) {
            for (const auto& p : chaoBuraco) if (CheckPolygonVsCircle(p, circle)) return true;
        }
        return false;
    }
    for (const auto& r : rectColliders) if (CheckRectVsCircle(r, circle)) return true;
    for (const auto& c : circleColliders) {
        const float dx = circle.center.x - c.center.x, dy = circle.center.y - c.center.y;
        const float rSum = circle.radius + c.radius;
        if (dx * dx + dy * dy < rSum * rSum) return true;
    }
    for (const auto& p : chaoNormal)       if (CheckPolygonVsCircle(p, circle)) return true;
    for (const auto& r : objRectColliders) if (CheckRectVsCircle(r, circle)) return true;
    for (const auto& p : objPolyColliders) if (CheckPolygonVsCircle(p, circle)) return true;
    return false;
}

bool LevelManager::CheckRectVsCircle(const SDL_Rect& rect, const Circle& circle) const {
    const float cx = std::clamp(circle.center.x, static_cast<float>(rect.x), static_cast<float>(rect.x + rect.w));
    const float cy = std::clamp(circle.center.y, static_cast<float>(rect.y), static_cast<float>(rect.y + rect.h));
    const float dx = circle.center.x - cx, dy = circle.center.y - cy;
    return dx * dx + dy * dy < circle.radius * circle.radius;
}

// Retângulo vs polígono, servindo para côncavos: um canto do retângulo dentro
// do polígono, um vértice do polígono dentro do retângulo, ou arestas cruzando.
bool LevelManager::CheckPolygonVsRect(const Polygon& poly, const SDL_Rect& rect) const {
    const size_t n = poly.vertices.size();
    if (n < 3) return false;
    if (BoundsApart(PolygonBounds(poly), rect.x, rect.y, rect.x + rect.w, rect.y + rect.h)) return false;

    const float rx = rect.x, ry = rect.y, rw = rect.w, rh = rect.h;
    const float corners[4][2] = {{rx, ry}, {rx + rw, ry}, {rx + rw, ry + rh}, {rx, ry + rh}};
    for (const auto& c : corners) {
        if (PointInPolygon(poly, c[0], c[1])) return true;
    }
    for (const auto& v : poly.vertices) {
        if (SDL_PointInRect(&v, &rect)) return true;
    }

    auto cross = [](float x1, float y1, float x2, float y2) { return x1 * y2 - y1 * x2; };
    auto segmentsCross = [&](float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy) {
        const float d1 = cross(dx - cx, dy - cy, ax - cx, ay - cy);
        const float d2 = cross(dx - cx, dy - cy, bx - cx, by - cy);
        const float d3 = cross(bx - ax, by - ay, cx - ax, cy - ay);
        const float d4 = cross(bx - ax, by - ay, dx - ax, dy - ay);
        return ((d1 > 0) != (d2 > 0)) && ((d3 > 0) != (d4 > 0));
    };
    for (size_t i = 0; i < n; i++) {
        const SDL_Point a = poly.vertices[i], b = poly.vertices[(i + 1) % n];
        for (int k = 0; k < 4; k++) {
            if (segmentsCross(a.x, a.y, b.x, b.y, corners[k][0], corners[k][1],
                              corners[(k + 1) % 4][0], corners[(k + 1) % 4][1])) return true;
        }
    }
    return false;
}

// Círculo vs polígono: centro dentro do polígono ou alguma aresta a menos de um raio.
bool LevelManager::CheckPolygonVsCircle(const Polygon& poly, const Circle& circle) const {
    const size_t n = poly.vertices.size();
    if (n == 0) return false;
    if (BoundsApart(PolygonBounds(poly),
                    static_cast<int>(circle.center.x - circle.radius), static_cast<int>(circle.center.y - circle.radius),
                    static_cast<int>(circle.center.x + circle.radius), static_cast<int>(circle.center.y + circle.radius))) {
        return false;
    }
    const float cx = circle.center.x, cy = circle.center.y;
    if (PointInPolygon(poly, cx, cy)) return true;

    const float rSq = circle.radius * circle.radius;
    for (size_t i = 0; i < n; i++) {
        const SDL_Point a = poly.vertices[i], b = poly.vertices[(i + 1) % n];
        float qx, qy;
        if (!ClosestOnSegment(a.x, a.y, b.x, b.y, cx, cy, qx, qy)) continue;
        if ((cx - qx) * (cx - qx) + (cy - qy) * (cy - qy) < rSq) return true;
    }
    return false;
}

// Até 4 rodadas resolvendo só a sobreposição MAIS FUNDA de cada vez (evita o
// empurrão duplo nas quinas e o "ganho de velocidade" nas escadinhas de pixel).
// Devolve o deslocamento total.
Vec2 LevelManager::GetCirclePushVector(const Circle& circle, bool isElevated) {
    float cx = circle.center.x, cy = circle.center.y;
    const float r = circle.radius;

    for (int iter = 0; iter < 4; iter++) {
        float maxOverlap = 0.0f;
        Vec2 bestPush(0.0f, 0.0f);

        // Candidato a empurrão: afastar do ponto (qx, qy), com o alcance `reach`.
        auto consider = [&](float qx, float qy, float reach) {
            const float dx = cx - qx, dy = cy - qy;
            const float distSq = dx * dx + dy * dy;
            if (distSq <= 0.0001f || distSq >= reach * reach) return;
            const float dist = std::sqrt(distSq);
            const float overlap = reach - dist;
            if (overlap > maxOverlap) {
                maxOverlap = overlap;
                bestPush = Vec2(dx / dist * overlap, dy / dist * overlap);
            }
        };
        auto rect = [&](const SDL_Rect& rc) {
            consider(std::clamp(cx, static_cast<float>(rc.x), static_cast<float>(rc.x + rc.w)),
                     std::clamp(cy, static_cast<float>(rc.y), static_cast<float>(rc.y + rc.h)), r);
        };
        auto poly = [&](const Polygon& p) {
            const size_t n = p.vertices.size();
            for (size_t i = 0; i < n; i++) {
                const SDL_Point a = p.vertices[i], b = p.vertices[(i + 1) % n];
                float qx, qy;
                if (ClosestOnSegment(a.x, a.y, b.x, b.y, cx, cy, qx, qy)) consider(qx, qy, r);
            }
        };

        if (!isElevated) {
            for (const auto& rc : rectColliders)    rect(rc);
            for (const auto& rc : objRectColliders) rect(rc);
            for (const auto& c : circleColliders)   consider(c.center.x, c.center.y, r + c.radius);
            for (const auto& p : objPolyColliders)  poly(p);
        }
        for (const auto& p : (isElevated ? chaoEscada : chaoNormal)) poly(p);
        if (isElevated && !escadaConsertada) {
            for (const auto& p : chaoBuraco) poly(p);
        }

        if (maxOverlap <= 0.0f) break;
        cx += bestPush.x;
        cy += bestPush.y;
    }
    return Vec2(cx - circle.center.x, cy - circle.center.y);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Desenho
// ═════════════════════════════════════════════════════════════════════════════

// Camadas de imagem na ordem do Tiled (parede → chão), com câmera e zoom.
void LevelManager::RenderBackground(SDL_Renderer* renderer) {
    const float zoom = Camera::GetZoom();
    for (const auto& img : imageLayers) {
        if (!img.texture) continue;
        const SDL_Rect dst{static_cast<int>((img.x - Camera::pos.x) * zoom), static_cast<int>((img.y - Camera::pos.y) * zoom),
                           static_cast<int>(img.w * zoom), static_cast<int>(img.h * zoom)};
        SDL_RenderCopy(renderer, img.texture, nullptr, &dst);
    }
}

// Debug [B]: cada tipo de colisor/zona numa cor; restaura o estado de desenho do renderer.
void LevelManager::RenderCollisionOverlay(SDL_Renderer* renderer) const {
    if (!renderer) return;
    const float zm = Camera::GetZoom();
    SDL_BlendMode oldBlend;
    SDL_GetRenderDrawBlendMode(renderer, &oldBlend);
    Uint8 dr, dg, db, da;
    SDL_GetRenderDrawColor(renderer, &dr, &dg, &db, &da);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(renderer, 255, 90, 90, 230);   DrawRects(renderer, rectColliders, zm);
    SDL_SetRenderDrawColor(renderer, 255, 60, 60, 235);   DrawPolygons(renderer, chaoNormal, zm);
    SDL_SetRenderDrawColor(renderer, 0, 220, 255, 230);   DrawPolygons(renderer, chaoEscada, zm);
    if (!escadaConsertada) {
        SDL_SetRenderDrawColor(renderer, 255, 80, 255, 220); DrawPolygons(renderer, chaoBuraco, zm);
    }
    SDL_SetRenderDrawColor(renderer, 180, 180, 200, 210); DrawPolygons(renderer, floorStoneZones, zm);
    SDL_SetRenderDrawColor(renderer, 90, 240, 120, 235);  DrawPolygons(renderer, repairableTriggers, zm);
    SDL_SetRenderDrawColor(renderer, 80, 255, 120, 230);  DrawRects(renderer, objRectColliders, zm);
    SDL_SetRenderDrawColor(renderer, 40, 200, 80, 235);   DrawPolygons(renderer, objPolyColliders, zm);

    SDL_SetRenderDrawColor(renderer, 90, 240, 120, 235);
    constexpr int kSeg = 36;
    for (const auto& c : circleColliders) {
        const float cx = (c.center.x - Camera::pos.x) * zm, cy = (c.center.y - Camera::pos.y) * zm, rad = c.radius * zm;
        for (int i = 0; i < kSeg; i++) {
            const float a0 = (static_cast<float>(i) / kSeg) * 2.0f * kPi;
            const float a1 = (static_cast<float>(i + 1) / kSeg) * 2.0f * kPi;
            SDL_RenderDrawLineF(renderer, cx + std::cos(a0) * rad, cy + std::sin(a0) * rad,
                                          cx + std::cos(a1) * rad, cy + std::sin(a1) * rad);
        }
    }

    SDL_SetRenderDrawBlendMode(renderer, oldBlend);
    SDL_SetRenderDrawColor(renderer, dr, dg, db, da);
}