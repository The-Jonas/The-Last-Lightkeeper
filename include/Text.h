#ifndef TEXT_H
#define TEXT_H

#define INCLUDE_SDL_TTF
#include "Component.h"
#include "SDL_include.h"
#include <SDL2/SDL_pixels.h>
#include <string>
#include <memory>

class Text : public Component {
public:
    // Enumeração para os estilos de renderização suportados pela SDL_ttf
    // 1. SOLID: Renderização rápida, sem anti-aliasing (bordas serrilhadas)
    // 2. SHADED: Renderiza o texto com uma caixa de fundo (background) de cor sólida
    // 3. BLENDED: Renderização de alta qualidade, com anti-aliasing (bordas suaves) e transparência
    enum TextStyle { SOLID, SHADED, BLENDED};

    Text(GameObject& associated, std::string fontFile, int fontSize,        // Construtor: Recebe todos os parâmetros iniciais (arquivo da fonte, tamanho, estilo, texto e cor)
    TextStyle style, std::string text, SDL_Color color);                    
    ~Text();                                                                // Destrutor: Se houver uma textura alocada, destrói ela para liberar memória

    void Update(float dt) override;                                         // Pode ser usada para efeitos (ex: texto piscando)
    void Render() override;                                                 // Desenha a textura na tela, ajustando pela posição da câmera


    // Métodos Setters:
    // Todos estes métodos alteram um atributo interno e chamam imediatamente RemakeTexture()
    void SetText(std::string text);
    void SetColor(SDL_Color color);
    void SetStyle(TextStyle style);
    void SetFontFile(std::string fontFile);
    void SetFontSize(int fontSize);

private:
    void RemakeTexture();                                                   // Método interno crucial

    std::shared_ptr<TTF_Font> font;                                         // Fonte gerenciada via shared_ptr pelo Resources
    SDL_Texture* texture;                                                   // A textura final gerada que será desenhada na tela

    std::string text;                                                       // Conteúdo da String
    TextStyle style;                                                        // Estilo de renderização
    std::string fontFile;                                                   // Caminho do arquivo
    int fontSize;                                                           // Tamanho da fonte
    SDL_Color color;                                                        // Cor do texto (R, G, B, A)
};

#endif