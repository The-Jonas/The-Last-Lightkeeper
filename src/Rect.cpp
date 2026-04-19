#include "../include/Rect.h"
#include <cmath>

/* Rect expressa uma posição (canto superior esquerdo do
retângulo) e dimensões. */

Rect::Rect() : x(0), y(0), w(0), h(0) {}

Rect::Rect(float x, float y, float w, float h) : x(x), y(y), w(w), h(h) {}

Vec2 Rect::Center() const {
    // Retorna o ponto central do retângulo
    return Vec2(x + w / 2, y + h / 2);
}

bool Rect::Contains(const Vec2& point) const {
    // Verifica se o ponto está dentro do retângulo
    return (point.x >= x && point.x <= x + w && point.y >= y && point.y <= y + h);
}

float Rect::Distance(const Rect& other) const {
    // Distância entre os centros de dois retângulos
    return Center().Distance(other.Center());
}

Rect Rect::operator+(const Vec2& v) const {
    // Retorna um novo retângulo deslocado pelo vetor v
    return Rect(x + v.x, y + v.y, w, h);
}