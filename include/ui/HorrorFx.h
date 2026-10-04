#ifndef HORROR_FX_H
#define HORROR_FX_H

#define INCLUDE_SDL
#include "SDL_include.h"

#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  HorrorFx — o "traço" visual das dicas: nada de balão, tudo meio apagado,
//  tremendo como chama fraca, sobre manchas de carvão. Tudo procedural
//  (SDL_RenderGeometry), em coordenadas de TELA.
//
//  As teclas continuam sendo a arte de KeyGlyphs; aqui só muda o jeito de
//  aparecer (tremor, falha de luz, mancha atrás).
// ─────────────────────────────────────────────────────────────────────────────
namespace HorrorFx {

float Flicker(float time, float seed);                   // 0.7..1: chama oscilando, com quedas leves
float Jitter(float time, float seed);                    // -1..1, troca de valor ~8x por segundo

// Mancha escura de bordas irregulares (fundo de leitura das teclas).
void DrawSmudge(SDL_Renderer* renderer, float cx, float cy, float rx, float ry, Uint8 alpha, float seed);

enum class HintAnchor {
    BottomCenter,                                        // (x, y) = meio da base (acima de um personagem)
    LeftMiddle,                                          // (x, y) = meio da borda esquerda (ao lado da HUD)
    TopCenter                                            // (x, y) = meio do topo (embaixo da HUD)
};

// [ícones] [teclas] rótulo — ex.: isqueiro + [F] "Usar item". Ícones e teclas
// se juntam com "+": isqueiro + combustível + [R]. Qualquer parte pode ser
// vazia. alpha 0..1 (fade da dica).
void DrawKeyHint(SDL_Renderer* renderer, const std::string& keys, const std::vector<std::string>& iconPaths,
                 const std::string& label, float x, float y, HintAnchor anchor, float alpha, float time);

// Risco horizontal irregular (sublinhado de título), de x1 a x2 na altura y.
void DrawScratchLine(SDL_Renderer* renderer, float x1, float x2, float y, float thickness,
                     SDL_Color color, float time);

// Arco de progresso (0..1, começa no topo, sentido horário) sobre um anel
// apagado, com o ícone no meio. Usado na recarga da luz.
void DrawProgressArc(SDL_Renderer* renderer, float cx, float cy, float radius, float progress,
                     const std::string& iconPath, float alpha, float time);

// Balão de pensamento de carvão: três bolinhas subindo da cabeça (headX, headY)
// até uma nuvem escura com contorno claro, com o ícone dentro. Tudo "fervendo".
void DrawThoughtBubble(SDL_Renderer* renderer, float headX, float headY, const std::string& iconPath,
                       float alpha, float time);

enum class AuraShape {
    Round,                                               // halo redondo + silhueta (slots da roda, pasta)
    Silhouette                                           // brilho com a forma do próprio PNG (a chama): degradê amarelo→laranja→vermelho subindo
};

// Aura de brasa: destaque de um elemento da HUD. Brilho quente e macio em volta
// (vermelho por fora, amarelo por dentro, "respirando"), a silhueta do próprio
// elemento (máscara branca do PNG em maskPath) e fagulhas subindo. intensity 0..1
// escala o brilho (ex.: a chama da HUD mais fraca brilha menos). Desenhar ANTES
// do elemento: só o que vaza em volta aparece.
void DrawEmberAura(SDL_Renderer* renderer, const SDL_FRect& target, const std::string& maskPath,
                   AuraShape shape, float intensity, float alpha, float time);

}  // namespace HorrorFx

#endif
