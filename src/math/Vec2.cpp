#include "math/Vec2.h"
#include <cmath>

/* Vec2 expressa um
vetor no R2, que pode tanto representar uma posição no espaço como uma
grandeza.*/

Vec2::Vec2() : x(0), y(0) {}

Vec2::Vec2(float x, float y) : x(x), y(y) {}

Vec2 Vec2::operator+(const Vec2& other) const {
    return Vec2(x + other.x, y + other.y);
}

Vec2 Vec2::operator-(const Vec2& other) const {
    return Vec2(x - other.x, y - other.y);
}

Vec2 Vec2::operator*(float scalar) const {
    return Vec2(x * scalar, y * scalar);
}

Vec2& Vec2::operator+=(const Vec2& other){
    this->x += other.x;
    this->y += other.y;
    return *this;
}

float Vec2::Magnitude() const {
    // Comprimento do vetor
    return std::sqrt(x * x + y * y);
}

Vec2 Vec2::Normalized() const {
    // Retorna o vetor com mesma direção mas comprimento 1
    float mag = Magnitude();
    return (mag > 0) ? Vec2(x / mag, y / mag) : Vec2(0, 0);
}

float Vec2::Distance(const Vec2& other) const {
    // Distância entre este vetor e outro
    return (*this - other).Magnitude();
}

float Vec2::Angle() const {
    // Ângulo do vetor em relação ao eixo X (radianos)
    return std::atan2(y, x);
}

float Vec2::Angle(const Vec2& other) const {
    // Ângulo entre este vetor e outro
    return (other - *this).Angle();
}

Vec2 Vec2::Rotated(float angleRad) const {
    // Rotaciona o vetor por um ângulo (em radianos)
    float cosA = std::cos(angleRad);
    float sinA = std::sin(angleRad);
    return Vec2(x * cosA - y * sinA, x * sinA + y * cosA);
}

Vec2 Vec2::FromAngle(float angleRad) {                      
    // Usa cosseno para a componente X e seno para a componente Y.
    // Isso cria um vetor unitário (comprimento 1) na direção do ângulo.
    return Vec2(std::cos(angleRad), std::sin(angleRad));
}