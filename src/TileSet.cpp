#include "TileSet.h"
#include "Sprite.h"
#include <iostream>


TileSet::TileSet(int tileWidth, int tileHeight, const std::string file) {  
    this->tileWidth = tileWidth;                                                                    // Seta as dimensões 
    this->tileHeight = tileHeight;                                                                  // do tile

    tileSet.Open(file);

    if (tileSet.IsOpen()){                                                                          // Se a abertura do sprite for bem-sucedida
        int frameCountW = tileSet.GetWidth() / tileWidth;                                           
        int frameCountH = tileSet.GetHeight() / tileHeight;                                         
    
        tileCount = frameCountW * frameCountH;                                                      // calcula o número total de tiles
        tileSet.SetFrameCount(frameCountW, frameCountH);                                            // Seta o frameCount no sprite para que GetWidth/GetHeight retornem o valor correto
    
    } else {
        tileCount = 0;                                                                              // Se não carregar, trata o caso de falha no carregamento
    }
}

void TileSet::RenderTile(unsigned int index, float x, float y) {
    if (tileSet.IsOpen() && index < tileCount) {                                                    // Checa se o índice é valido, ou seja, se está entre 0 e o número de tiles - 1
        tileSet.SetFrame(index);                                                                    // Seta o frame desejado no sprite
        tileSet.Render(x, y, tileWidth, tileHeight);                                                // Renderiza o tile na posição e com as dimensões corretas
    }
}

int TileSet::GetTileWidth() {                                                                       // Retorna a largura de um tile
    return tileWidth;
}

int TileSet:: GetTileHeight() {                                                                     // Retorna a altura de um tile
    return tileHeight;
}