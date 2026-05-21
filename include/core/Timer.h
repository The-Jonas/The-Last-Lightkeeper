#ifndef TIMER_H
#define TIMER_H

class Timer {
public:
    Timer();                                // Construtor: Zera o contador de tempo.
    
    void Update(float dt);                  // Acumula o delta time.
    void Restart();                         // Reinicia o contador de tempo.
    float Get();                            // Retorna o tempo acumulado.

private:
    float time;                             // O tempo acumulado desde o início da contagem ou desde o último restart.
};

#endif