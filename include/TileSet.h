#ifndef TILESET_H
#define TILESET_H

#include "Sprite.h"
#include <string>
#include <memory>

class TileSet {
public:

    TileSet(int tileWidth, int tileHeight, const std::string file); // Seta as dimensões dos tiles e carrega a imagem do tileset
    TileSet(int tileWidth, int tileHeight, const std::string file, int frameCountW, int frameCountH);
    void RenderTile(unsigned index, float x, float y);              // Renderiza um tile específico na tela, a partir do índice 

    int GetTileWidth();                                             // Retorna largura de um tile
    int GetTileHeight();                                            // Retorna altura de um tile

private:

    Sprite tileSet;                                                 //O sprite que contém a imagem completa do tileset
    int tileWidth;                                                  //A largura de um tile
    int tileHeight;                                                 //A altura de um tile
    int frameCountW = 0;
    int frameCountH = 0;
    unsigned int tileCount;                                         //O número total de tiles do tileset (unsigned porque não pode ser negativo e pode ser usado para ser comparado ao index)

};

#endif