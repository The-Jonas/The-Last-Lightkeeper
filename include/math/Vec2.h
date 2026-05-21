#ifndef VEC2_H
#define VEC2_H

#include <cmath>

// Representa um vetor 2D (x, y)
class Vec2 {
public:
    float x, y;

    Vec2();                       // Construtor padrão (x = 0, y = 0)
    Vec2(float x, float y);       // Construtor com valores

    Vec2 operator+(const Vec2& other) const;  // Soma dois vetores
    Vec2 operator-(const Vec2& other) const;  // Subtrai dois vetores
    Vec2 operator*(float scalar) const;       // Multiplica por escalar

    Vec2& operator+=(const Vec2& other);      // Operador de atribuição de soma

    float Magnitude() const;                  // Retorna o comprimento do vetor
    Vec2 Normalized() const;                  // Retorna o vetor normalizado (direção)
    float Distance(const Vec2& other) const;  // Retorna a distância até outro vetor
    float Angle() const;                      // Retorna o ângulo em relação ao eixo x
    float Angle(const Vec2& other) const;     // Retorna o ângulo até outro vetor
    static Vec2 FromAngle(float angleRad);    // Método estático para criar um vetor unitário a partir de um ângulo (em radianos).
    Vec2 Rotated(float angleRad) const;       // Retorna o vetor rotacionado por um ângulo
};

#endif