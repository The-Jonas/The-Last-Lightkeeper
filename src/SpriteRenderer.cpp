#include "../include/SpriteRenderer.h"
#include "../include/Game.h"
#include "../include/GameObject.h"

// Construtor 1: Não faz nada com o sprite, apenas inicializa a classe base
SpriteRenderer::SpriteRenderer(GameObject& associated) : Component(associated) {
    // Apenas constrói a classe base
}

// Construtor 2: Lida com o sprite e o GameObject associado
SpriteRenderer::SpriteRenderer(GameObject& associated, std::string file, int frameCountW, int frameCountH)
: Component(associated), sprite(file, frameCountW, frameCountH) {                           // Chamando construtor de Component e Sprite

    associated.box.w = sprite.GetWidth();                                   // O construtor deve definir a altura e largura da box do GameObject
    associated.box.h = sprite.GetHeight();                                  // com base no tamanho do sprite aberto.
    
    SetFrame(0);                                                            // Inicializa o primeiro frame com 0
}

void SpriteRenderer::SetFrame(int frame) {
    sprite.SetFrame(frame);                                                 // Chama o método homônimo da classe Sprite
}

void SpriteRenderer::SetFrame(int frame, SDL_RendererFlip flip) {
    sprite.SetFrame(frame);
    sprite.SetFlip(flip);
}

void SpriteRenderer::SetFlip(SDL_RendererFlip flip) {
    sprite.SetFlip(flip);
}

void SpriteRenderer::SetTint(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    sprite.SetTint(r, g, b, a);
}

void SpriteRenderer::SetFrameCount(int frameCountW, int frameCountH) {
    sprite.SetFrameCount(frameCountW, frameCountH);                         // Chama o método homônimo da classe Sprite
}

void SpriteRenderer::SetScale(float scaleX, float scaleY) {
    //1. Obter o centro atual antes de mudar a escala
    Vec2 oldCenter = associated.box.Center();

    //2. Aplicar a nova escala no sprite
    sprite.SetScale(scaleX, scaleY);

    //3. Atualizar a box do GameObject com as novas dimensões
    associated.box.w = sprite.GetWidth();
    associated.box.h = sprite.GetHeight();

    //4. Mover a box para manter o centro no mesmo lugar
    associated.box.x = oldCenter.x - (associated.box.w / 2.0f);
    associated.box.y = oldCenter.y - (associated.box.h / 2.0f);
}

void SpriteRenderer::Open(const std::string file) {
    sprite.Open(file);                                                      // Chama o método homônimo da classe Sprite      

    associated.box.w = sprite.GetWidth();                                   // Novamente declarando a largura e altura
    associated.box.h = sprite.GetHeight();                                  // do box do GameObject associado
}

void SpriteRenderer::SetCameraFollower(bool follow) {                       // Permite o acesso para mudar o estado de CameraFollower
    sprite.SetCameraFollower(follow);
}

void SpriteRenderer::Update(float dt) {
    // O documento pede para deixar essa função vazia
}

void SpriteRenderer::Render() {
    sprite.Render(
        associated.box.x,
        associated.box.y, 
        associated.box.w, 
        associated.box.h,
        associated.angleDeg);   // Chama o Render do sprite, passando os valores da box do GameObject associado
}
