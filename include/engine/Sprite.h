#ifndef SPRITE_H
#define SPRITE_H

#define INCLUDE_SDL
#define INCLUDE_SDL_IMAGE
#include "SDL_include.h"
#include "math/Vec2.h"

#include <string>
#include <memory>

class Sprite{
public:
    Sprite();                                               // Construtor Padrão
    Sprite(const std::string file, int frameCountW = 1, int frameCountH = 1);             // Construtor que já abre imagem
    ~Sprite();                                              // Destrutor

    void Open(std::string file);                            // Carrega a textura a partir do arquivo
    void SetClip(int x, int y, int w, int h);               // Define a área a ser desenhada
    void Render(int x, int y, int w, int h, double angleDeg = 0.0);                      // Desenha na tela (agora recebe w e h para a box)
    int GetWidth();                                         // Retorna Largura
    int GetHeight();                                        // Retorna Altura
    bool IsOpen();                                          // Verifica se a imagem foi carregada

    void SetFrame(int frame);                               // Seta o frame a ser mostrado
    void SetFrameCount(int frameCountW, int frameCountH);   // Conta quantos frames há na imagem

    void SetScale(float scaleX, float scaleY);              // Seta o tamanho do componente, valor 1 seria o padrão
    Vec2 GetScale();                                        // Retorna o valor de scale
    void SetFlip(SDL_RendererFlip flip);                    // Setar se o objeto vai ser espelhado
    void SetTint(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255); // Aplica modulação de cor/alpha na textura

    void SetCameraFollower(bool follow);                    // O CameraFollower vai escolher se o objeto fica fixo no mapa (false)
    bool cameraFollower;                                    // ou se ele segue a câmera (true), como por exemplo ser usado em elementos de fundo

private:
    std::shared_ptr<SDL_Texture> texture;                                   // Ponteiro para a textura carregada
    int width, height;                                      // Dimensões da Imagem
    SDL_Rect clipRect;                                      // Área a ser desenhada (recorte da imagem)
    int frameCountW, frameCountH;                           // Novos membros para animação            

    // Valores para modificar os objetos em termos de espelhamento e escala
    SDL_RendererFlip flip;
    Vec2 scale;
    Uint8 tintR;
    Uint8 tintG;
    Uint8 tintB;
    Uint8 tintA;

};

#endif //SPRITE_H