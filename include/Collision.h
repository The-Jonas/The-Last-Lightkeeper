#ifndef COLLISION_H
#define COLLISION_H

#include "Rect.h"
#include "Vec2.h"
#include <algorithm>
#include <cmath>

class Collision {
public:
    // Observação: IsColliding espera ângulos em radianos!
    static inline bool IsColliding(Rect& a, Rect& b, float angleOfA, float angleOfB) {
        Vec2 A[] = { Vec2( a.x, a.y + a.h ),
                      Vec2( a.x + a.w, a.y + a.h ),
                      Vec2( a.x + a.w, a.y ),
                      Vec2( a.x, a.y )
                    };
        Vec2 B[] = { Vec2( b.x, b.y + b.h ),
                      Vec2( b.x + b.w, b.y + b.h ),
                      Vec2( b.x + b.w, b.y ),
                      Vec2( b.x, b.y )
                    };

        for (auto& v : A) {
            // Adaptado para usar o meu método de classe Vec2 'Rotated' 
            v = (v - a.Center()).Rotated(angleOfA) + a.Center();
        }

        for (auto& v : B) {
            // Adaptado para usar o meu método de classe Vec2 'Rotated' 
            v = (v - b.Center()).Rotated(angleOfB) + b.Center();
        }

        Vec2 axes[] = { Norm(A[0] - A[1]), Norm(A[1] - A[2]), Norm(B[0] - B[1]), Norm(B[1] - B[2]) };

        for (auto& axis : axes) {
            float P[4];

            for (int i = 0; i < 4; ++i) P[i] = Dot(A[i], axis);

            float minA = *std::min_element(P, P + 4);
            float maxA = *std::max_element(P, P + 4);

            for (int i = 0; i < 4; ++i) P[i] = Dot(B[i], axis);

            float minB = *std::min_element(P, P + 4);
            float maxB = *std::max_element(P, P + 4);

            if (maxA < minB || minA > maxB)
                return false;
        }

        return true;
    }

private:
    // Funções auxiliares (adaptadas para usar seus métodos Vec2)
    static inline float Mag(const Vec2& p) {
        return p.Magnitude(); 
    }

    static inline Vec2 Norm(const Vec2& p) {
        return p.Normalized(); 
    }

    static inline float Dot(const Vec2& a, const Vec2& b) {
        return a.x * b.x + a.y * b.y; // Produto escalar padrão
    }

    // Nota: A função 'Rotate' do arquivo foi removida
    // pois adaptei o IsColliding para usar o meu Vec2::Rotated
};

#endif // COLLISION_H