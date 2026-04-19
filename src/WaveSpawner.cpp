#include "../include/WaveSpawner.h"
#include "../include/GameObject.h"
#include "../include/Zombie.h"
#include "../include/Game.h"
#include "../include/State.h"
#include "../include/Camera.h"
#include "../include/Character.h"
#include "../include/AIController.h"
#include <cstdlib>
#include <iostream>

bool WaveSpawner::isFinished = false;

WaveSpawner::WaveSpawner(GameObject& associated) : Component(associated) {
    spawnCountRemaining = 0;
    currentWave = 0;
    isFinished = false;

    // -- DEFINIÇÃO DE WAVES --

    // Wave 1 - Introdução
    std::queue<WaveCommand> wave1;
    wave1.push(WaveCommand::Wait(1.0f));                                // Espera 1 segundo
    wave1.push(WaveCommand::SpawnZombie(5, 1.5f));                      // Spawna 5 zumbis (1 a cada 1.5s)
    wave1.push(WaveCommand::Wait(3.0f));                                // Espera 3s
    wave1.push(WaveCommand::SpawnZombie(3, 0.5f));                      // Spawna 3 zumbis rapidamente
    waves.push_back(wave1);
    std::cout << "Wave 1 size: " << waves[0].size() << std::endl;

    // WAVE 2: Os Primeiro NPCs
    std::queue<WaveCommand> wave2;
    wave2.push(WaveCommand::Wait(2.0f));
    wave2.push(WaveCommand::SpawnNPC(2, 0.0f));                         // 2 NPC
    wave2.push(WaveCommand::Wait(2.0f));
    wave2.push(WaveCommand::SpawnZombie(10, 1.0f));                     // Horda de cobertura
    waves.push_back(wave2);
    std::cout << "Wave 2 size: " << waves[1].size() << std::endl;

    // WAVE 3: O Caos
    std::queue<WaveCommand> wave3;
    wave3.push(WaveCommand::SpawnZombie(15, 0.2f));                     // Rush de zumbis
    wave3.push(WaveCommand::SpawnNPC(5, 2.0f));                         // 5 NPCs
    waves.push_back(wave3);
    std::cout << "Wave 3 size: " << waves[2].size() << std::endl;

    std::cout << "Iniciando Wave" << currentWave + 1 << std::endl;
}

void WaveSpawner::Update(float dt) {
    spawnTimer.Update(dt);

    // Verifica se ainda existem ondas para processar
    if (currentWave < (int)waves.size()) {

        // Verifica se a fila de comandos da wave atual ainda tem itens
        // OU se ainda estamos processando um comando de spawn múltiplo
        if (!waves[currentWave].empty() || spawnCountRemaining > 0) {

            // Se não estamos no meio de um spawn múltiplo, pegamos o próximo comando
            if (spawnCountRemaining == 0) {
                WaveCommand& cmd = waves[currentWave].front();

                // Processa WAIT
                if (cmd.type == CMD_WAIT) {
                    if (spawnTimer.Get() > cmd.time) {
                        waves[currentWave].pop();                                           // Comando concluído
                        spawnTimer.Restart();
                    }
                }
                // Processa o INÍCIO DE SPAWN
                else {
                    spawnCountRemaining = cmd.count;                                       // Inicia contagem
                }
            }
            
            // Lógica de processamento de Spawn (zumbi ou npc)
            if (spawnCountRemaining > 0) {
                WaveCommand& cmd = waves[currentWave].front();

                if (spawnTimer.Get() > cmd.time) {
                    // LÓGICA DE CRIAÇÃO
                    Vec2 cameraCenter(Camera::pos.x + 600, Camera::pos.y + 450);
                    float angle = (rand() % 360) * (M_PI / 180.f);
                    float distance = 800 + (rand() % 400);
                    Vec2 spawnPos = cameraCenter + Vec2::FromAngle(angle) * distance;

                    if (cmd.type == CMD_SPAWN_ZOMBIE) {
                        // -- Criando zumbi
                        GameObject* newZombie = new GameObject();
                        newZombie->AddComponent(new Zombie(*newZombie));
                        newZombie->z = 2;

                        // Posicionamento
                        newZombie->box.x = spawnPos.x - newZombie->box.w / 2;
                        newZombie->box.y = spawnPos.y - newZombie->box.h / 2;
                        Game::GetInstance().GetCurrentState().AddObject(newZombie);
                    }

                    else if (cmd.type == CMD_SPAWN_NPC) {
                        // -- Criando NPC
                        GameObject* npcObj = new GameObject();
                        npcObj->z = 2;
                        Character* charComp = new Character(*npcObj, "Recursos/img/NPC.png", Character::NPC);
                        npcObj->AddComponent(charComp);
                        npcObj->AddComponent(new AIController(*npcObj));

                        // Posicionamento
                        npcObj->box.x = spawnPos.x - npcObj->box.w / 2;
                        npcObj->box.y = spawnPos.y - npcObj->box.h / 2;
                        Game::GetInstance().GetCurrentState().AddObject(npcObj);                        
                    }

                    spawnCountRemaining--;
                    spawnTimer.Restart();

                    // Se acabou a contagem deste comando, remove eles da fila
                    if (spawnCountRemaining <= 0) {
                        waves[currentWave].pop();
                    }
                }
            }
        }
        // Se a fila de comandos acabou, espera todos morrerem para passar de Wave
        else {
            if (Zombie::zombieCount == 0 && AIController::npcCounter == 0) {
                currentWave++;
                std::cout << "Iniciando Wave" << currentWave + 1 << std::endl;
            }
        }
    }
    else {
        std::cout << "Fim do jogo, parabéns!" << currentWave + 1 << std::endl;
        // Fim de jogo
        isFinished = true;                                      // Sinaliza vitória
    }
}

void WaveSpawner::Render() {
    // Nada aqui
}