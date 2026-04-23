#include "../include/TileMap.h"
#include "../include/GameObject.h"
#include "../include/TileSet.h"
#include "../include/Camera.h"
#include <fstream>
#include <iostream>
#include <sstream>

//Construtor do TileMap
TileMap::TileMap(GameObject& associated, std::string file, TileSet* tileSet) : Component(associated) {
    Load(file);                                                         // Chama Load com a file passada
    SetTileSet(tileSet);                                                // Seta o tileSet
}

void TileMap::Load(std::string file) {
    std::ifstream mapFile(file);

    if (!mapFile.is_open()) {
        std::cerr << "Erro: não foi possível abrir o arquivo de mapa " << file << std::endl;
        return;
    }

    // Lê as dimensões, ignorando as vírgulas.
    char comma;
    mapFile >> mapWidth >> comma >> mapHeight >> comma >> mapDepth >> comma;

    parallaxMultipliers.resize(mapDepth, 1.0f);                         // Aloca e inicializa os multiplicadores 

    // Lê os tiles e armazena na tileMatrix.
    int tileIndex;
    while (mapFile >> tileIndex) {
        tileMatrix.push_back(tileIndex);
        // Ignora a vírgula após cada número.
        if (mapFile.peek() == ',') {
            mapFile.ignore();
        }
    }
        mapFile.close();
}

void TileMap::SetTileSet(TileSet* tileSet) {
    this->tileSet = tileSet;                                            // Troca o tileSet em uso
}

int& TileMap::At(int x, int y, int z) {
    return tileMatrix[x + y * mapWidth + z * mapWidth * mapHeight];      // Ele retorna uma referência ao elemento [x][y][z] de tileMatrix
}

void TileMap::RenderLayer(int layer){                                   // Renderiza uma camada específica
    int tileW = tileSet->GetTileWidth();
    int tileH = tileSet->GetTileHeight();
    float multiplier = parallaxMultipliers[layer];

    for(int y = 0; y < mapHeight; y++){
        for(int x = 0; x < mapWidth; x++){
            // Renderiza o tile na posição correta, ajustando pela box do GameObject
            int tileIndex = At(x, y, layer);
            // Compensação da câmera com o multiplicador de parallax
            tileSet->RenderTile(tileIndex, (x * tileW) - static_cast<int>(Camera::pos.x * multiplier), (y * tileH) - static_cast<int>(Camera::pos.y * multiplier));
        }
    }
}

void TileMap::SetParallax(int layer, float multiplier) {
    if (layer >= 0 && layer < mapDepth) {
        parallaxMultipliers[layer] = multiplier;
    }
}

void TileMap::Render() {
    for (int i = 0; i < mapDepth; i++){                                 // Itera até renderizar todas as camadas
        RenderLayer(i);
    }
}

int TileMap::GetWidth() {                                               // Retorna a largura do mapa
    return mapWidth;
}

int TileMap::GetHeight() {                                              // Retorna a altura do mapa
    return mapHeight;
}

int TileMap::GetDepth() {                                               // Retorna a profundidade do mapa
    return mapDepth;
}

void TileMap::BuildLightOcclusionFromLayer(int layerZ, const std::unordered_set<int>& passableTileIds) {
    lightOcclusionSolid.clear();
    if (mapWidth < 1 || mapHeight < 1 || layerZ < 0 || layerZ >= mapDepth) {
        return;
    }
    const int cells = mapWidth * mapHeight;
    if (passableTileIds.empty()) {
        lightOcclusionSolid.assign(static_cast<size_t>(cells), 0);
        return;
    }
    lightOcclusionSolid.assign(static_cast<size_t>(cells), 0);
    for (int y = 0; y < mapHeight; y++) {
        for (int x = 0; x < mapWidth; x++) {
            const int t = At(x, y, layerZ);
            const bool pass = passableTileIds.find(t) != passableTileIds.end();
            lightOcclusionSolid[static_cast<size_t>(x + y * mapWidth)] = pass ? 0 : 1;
        }
    }
}

