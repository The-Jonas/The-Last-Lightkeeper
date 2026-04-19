#ifndef WAVESPAWNER_H
#define WAVESPAWNER_H

#include "GameObject.h"
#include "Component.h"
#include "Timer.h"
#include <queue>
#include <vector>

// Tipos de comandos possíveis 
enum WaveCommandType {CMD_SPAWN_ZOMBIE, CMD_SPAWN_NPC, CMD_WAIT};

struct WaveCommand {
    WaveCommandType type;
    int count;                                      // Quantos inimigos spawnar
    float time;                                     // Tempo de espera (para WAIT) ou intervalo (para SPAWN)

    // Construtores auxiliares
    static WaveCommand SpawnZombie(int count, float interval) {return {CMD_SPAWN_ZOMBIE, count, interval};}
    static WaveCommand SpawnNPC(int count, float interval) {return {CMD_SPAWN_NPC, count, interval};}
    static WaveCommand Wait(float seconds) {return {CMD_WAIT, 0, seconds};}
};

class WaveSpawner: public Component {
public:

    WaveSpawner(GameObject& associated);
    // Métodos de Ciclo de Vida
    void Update(float dt) override;
    void Render() override;

    static bool isFinished;                 // Flag global de término

private:
    // Filas de comando (uma por Wave)
    std::vector<std::queue<WaveCommand>> waves;

    int spawnCountRemaining;               // Quantos zumbis já nasceram na onda atual
    int currentWave;                       // Índice da onda atual
    Timer spawnTimer;                      // Timer para controlar o intervalo de spawn
};


#endif