#include "core/Timer.h"

Timer::Timer() {                    // Inicializa o contador de tempo como 0
    time = 0.0f;
}

void Timer::Update(float dt) {      // Acumula o delta time no contador
    time += dt;
}

void Timer::Restart() {             // Reinicia o contador do tempo, zerando-o
    time = 0.0f;
}

float Timer::Get() {                // Retorna o tempo acumulado
    return time;
}