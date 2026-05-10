#ifndef SPRITERENDERER_H
#define SPRITERENDERER_H

#include "Component.h"
#include "Sprite.h"
#include "GameObject.h"

#include <string>

class SpriteRenderer : public Component {
public: 
    SpriteRenderer(GameObject& associated);                                                                         // Construtor 1
    SpriteRenderer(GameObject& associated, const std::string file, int frameCountW = 1, int frameCountH = 1);       // Construtor 2

    void Update(float dt) override;                                                                                 // Metodos virtuais
    void Render() override;                                                                                         // herdados de Component

    // Métodos "wrapper"
    void Open(const std::string file);                                                                              
    void SetFrameCount(int frameCountW, int frameCountH);
    void SetFrame(int frame);
    // -----------------

    void SetFrame(int frame, SDL_RendererFlip flip);                                                                // Variante do SetFrame
    void SetScale(float scaleX, float scaleY);  
    void SetFlip(SDL_RendererFlip flip);                                                                            // Precisei pra manter a animação da arma ao espelhar ela (não tá no PDF)
    void SetTint(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255);
    
    void SetCameraFollower(bool follow);

private: 
    Sprite sprite;
};

#endif