#include "../include/Sprite.h"
#include "../include/Game.h"
#include "../include/Resources.h"
#include "../include/Camera.h"
#include <iostream>

Sprite::Sprite(){                               // Inicializa como padrão
    texture = nullptr; 
    width = 0;
    height = 0;
    frameCountH = 1;
    frameCountW = 1;
    cameraFollower = false; 

    scale = Vec2(1.0f, 1.0f);
    flip = SDL_FLIP_NONE;
    tintR = 255;
    tintG = 255;
    tintB = 255;
    tintA = 255;
}

Sprite::Sprite(const std::string file, int frameCountW, int frameCountH) : frameCountW(frameCountW), frameCountH(frameCountH) {        // Construtor que carrega imagem
    texture = nullptr;
    scale = Vec2(1.0f, 1.0f);
    flip = SDL_FLIP_NONE;
    tintR = 255;
    tintG = 255;
    tintB = 255;
    tintA = 255;

    Open(file);
}

Sprite::~Sprite(){                              // Destrutor
    //if (texture) {
    //    SDL_DestroyTexture(texture);
    //}

    // Não vai mais destruir a textura
    // a classe Resources agora gerencia a memória
}

void Sprite::Open(std::string file){

    texture = Resources::GetImage(file);                                // Chamando Resources::GetImage em vez de IMG_LoadTexture

    if (!texture) {
        std::cerr << "Erro ao carregar imagem: " << file << " - " << SDL_GetError() << std::endl;
        return; 
    }

    if (!IsOpen()) return;
    SDL_QueryTexture(texture.get(), nullptr, nullptr, &width, &height); // Pega as dimensões da imagem
    SetFrameCount(frameCountW, frameCountH);                            // Chamando SetFrameCount para garantir que as dimensões do frame sejam atualizadas
}

void Sprite::SetClip(int x, int y, int w, int h) {
    clipRect = {x, y, w, h};                                            // Define a área de recorte da imagem
}

void Sprite::SetFlip(SDL_RendererFlip newFlip) {
    flip = newFlip;
}

void Sprite::SetScale(float scaleX, float scaleY) {
    // Mantenha a escala em dado eixo se o valor passado para ela for 0
    if (scaleX != 0.0f) {
        scale.x = scaleX;
    }
    if (scaleY != 0.0f) {
        scale.y = scaleY;
    }
}

void Sprite::SetTint(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    tintR = r;
    tintG = g;
    tintB = b;
    tintA = a;
}

void Sprite::Render(int x, int y, int w, int h, double angleDeg) {
    int finalX = x;
    int finalY = y;
    int finalW = w;
    int finalH = h;

    if(!cameraFollower){
        float zoom = Camera::GetZoom();
        finalX = static_cast<int>((x - Camera::pos.x) * zoom);
        finalY = static_cast<int>((y - Camera::pos.y) * zoom);
        finalW = static_cast<int>(w * zoom);
        finalH = static_cast<int>(h * zoom);
    }
    
    SDL_Rect dstRect = { finalX, finalY, finalW, finalH };                          // Área destino na tela

    SDL_SetTextureColorMod(texture.get(), tintR, tintG, tintB);
    SDL_SetTextureAlphaMod(texture.get(), tintA);

    //SDL_RenderCopy(Game::GetInstance().GetRenderer(), texture, &clipRect, &dstRect);  // Desenha
    SDL_RenderCopyEx(
        Game::GetInstance().GetRenderer(),
        texture.get(),
        &clipRect,
        &dstRect,
        angleDeg,                                                       // Ângulo de rotação em graus
        nullptr,                                                        // Determina o eixo a qual a rotação ocorre - null para a rotação acontecer em torno do centro do triângulo
        flip                                                            // Inverte a imagem verticalmente, horizontalmente ou não inverte
    );
}

void Sprite::SetCameraFollower(bool follow) {                           // Função para setar true ou false para cameraFollower
    cameraFollower = follow;
}

void Sprite::SetFrame(int frame) {                                      // Calcula as coordenadas x e y do frame na sprite sheet
    int frameWidth = width / frameCountW;
    int frameHeight = height / frameCountH;
    int y_offset = (frame / frameCountW) * frameHeight;                 // O PDF sugere dividir o índice do frame pela quantidade de frames em cada linha para obter o número de linhas a pular
    int x_offset = (frame % frameCountW) * frameWidth;                  // O resto dará o número de colunas a pular
    
    SetClip(x_offset, y_offset, frameWidth, frameHeight);               // Define o clip para o novo frame
}

void Sprite::SetFrameCount(int frameCountW, int frameCountH) {          // Seta os respectivos membros
    this->frameCountW = frameCountW;
    this->frameCountH = frameCountH;

    int frameWidth = width / this->frameCountW;
    int frameHeight = height / this->frameCountH;
    clipRect = { 0, 0, frameWidth, frameHeight };                       // Usa um frame base sem aplicar escala no recorte
}

Vec2 Sprite::GetScale() {
    return scale;
}

int Sprite::GetWidth() {
    return (width / frameCountW) * scale.x;                             // A largura de um frame é a largura total da imagem dividida pelo número de frames na horizontal
}

int Sprite::GetHeight() {
    return (height / frameCountH) * scale.y;                            // A altura de um frame é a altura total da imagem dividida pelo número de frames na vertical
}

bool Sprite::IsOpen() {
    return texture != nullptr;
}




