#ifndef KEY_GLYPHS_H
#define KEY_GLYPHS_H

#define INCLUDE_SDL
#define INCLUDE_SDL_TTF
#include "SDL_include.h"

#include <string>

// ─────────────────────────────────────────────────────────────────────────────
//  KeyGlyphs — desenha uma linha de texto com as TECLAS em imagem.
//
//  As dicas do jogo sao escritas assim:
//
//      "Pressione [F] para ligar seu isqueiro"
//      "Use [W] [A] [S] [D] para se mover"
//
//  Tudo o que esta entre [ ] e o nome de uma tecla, tal como o SDL a chama
//  (`SDL_GetKeyName`), porque as dicas sao montadas a partir das TECLAS QUE O
//  JOGADOR TEM configuradas. Este modulo troca cada um desses pedacos pela arte
//  da tecla correspondente (Recursos/img/ui/teclas), e deixa o resto como
//  texto. Uma tecla sem arte volta a aparecer escrita entre parenteses rectos,
//  por isso uma dica nova nunca fica por desenhar.
//
//  O desenho e a medicao passam pela MESMA funcao, para a caixa de fundo nunca
//  discordar do conteudo.
// ─────────────────────────────────────────────────────────────────────────────
namespace KeyGlyphs {

/// Largura e altura que a linha vai ocupar, com as teclas ja em conta.
void Measure(TTF_Font* font, const std::string& text, float keyScale, int& outW, int& outH);

/// Desenha a linha com o canto superior-esquerdo em (x, y).
void Draw(SDL_Renderer* renderer, TTF_Font* font, const std::string& text,
          int x, int y, SDL_Color color, Uint8 alpha, float keyScale);

/// Altura de uma tecla em relacao a altura da fonte. 1.0 = do tamanho da
/// linha; acima disso a tecla sobressai, que e o que a faz ler como tecla.
/// A 1.45 a moldura lia-se mas a LETRA la dentro ficava pequena de mais — a
/// arte tem muita margem a volta do caractere.
constexpr float kDefaultKeyScale = 2.15f;

}  // namespace KeyGlyphs

#endif  // KEY_GLYPHS_H
