#ifndef RECT_H
#define RECT_H

#include "math/Vec2.h"

// Representa um retângulo com posição (x, y) e tamanho (w, h)
class Rect {
public:
    float x, y, w, h;

    Rect();                                       // Construtor padrão
    Rect(float x, float y, float w, float h);     // Construtor com posição e tamanho

    Vec2 Center() const;                          // Retorna o centro do retângulo
    bool Contains(const Vec2& point) const;       // Verifica se um ponto está dentro
    float Distance(const Rect& other) const;      // Distância entre os centros de dois retângulos
    Rect operator+(const Vec2& v) const;          // Desloca o retângulo com um vetor
};

#endif