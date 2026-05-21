#ifndef TILEMAP_H
#define TILEMAP_H

#include "engine/GameObject.h"
#include "world/TileSet.h"
#include "engine/Sprite.h"

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

// A classe TileMap simula uma matriz tridimensional para representar um mapa
// e suas diversas camadas, contendo em cada posição um índice de tile no TileSet.

class TileMap : public Component {
public:

    TileMap(GameObject& associated, const std::string file, TileSet* tileset);          // Construtor: Chama o método Load() para carregar o mapa e seta o TileSet.

    void Load(const std::string file);                                                  // Carrega o arquivo do mapa, lendo as dimensões e os índices dos tiles.
    void SetTileSet(TileSet* tileset);                                                  // Troca o TileSet em uso.
    int& At(int x, int y, int z = 0);                                                   // Retorna uma referência ao índice do tile na posição (x, y, z).
    void Render() override;                                                             // Renderiza o mapa completo, camada por camada.
    void RenderLayer(int layer);                                                        // Renderiza uma camada específica do mapa.

    int GetWidth();                                                                     // Retorna a largura do mapa
    int GetHeight();                                                                    // Retorna a altura do mapa
    int GetDepth();                                                                     // Retorna a profundidade do mapa (número de camadas)

    void SetParallax(int layer, float multiplier);                                      // Método para definir o multiplicador de uma camada.

    void BuildLightOcclusionFromLayer(int layerZ, const std::unordered_set<int>& passableTileIds);
    const std::vector<std::uint8_t>& GetLightOcclusionSolid() const { return lightOcclusionSolid; }

private:

    std::vector<float> parallaxMultipliers;                                             // Multiplicadores para cada camada
    std::vector<int> tileMatrix;                                                        // Matriz de tiles simulada por um vetor 1D
    TileSet* tileSet;                                                                   // Ponteiro para o TileSet em uso. (fiz diferente do que o PDF dizia, parece mais sólido assim)

    //Dimensões do mapa
    int mapWidth, mapHeight, mapDepth;

    std::vector<std::uint8_t> lightOcclusionSolid;
};


#endif