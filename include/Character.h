#ifndef CHARACTER_H
#define CHARACTER_H

#include "Component.h"
#include "Timer.h"
#include "Vec2.h"
#include "Sound.h"
#include <queue>
#include <memory>
#include <string>

class GameObject;                                                       // Forward declaration
class Gun;

class Character : public Component {
public:
    enum CharacterRole {PLAYER, NPC};

    // Classe Command
    class Command {
    public:
        enum CommandType{MOVE, SHOOT};                                  // Enum para definir os tipos de comando possíveis
        Command(CommandType type, float x, float y);                    // Construtor do Command
        CommandType type;                                               // O tipo de comando (MOVE ou SHOOT)
        Vec2 pos;                                                       // A posição do alvo (para movimento ou tiro)
    };

    Character(GameObject& associated, std::string spritePath, CharacterRole role = PLAYER);              
    ~Character();                                                       // Destrutor 

    // Métodos do ciclo de vida
    void Start() override;                                              // Metodo para inicar o objeto
    void Update(float dt) override;  
    void Render() override;                                             // Deixando vazio
    void NotifyCollision(GameObject& other) override;                   // Para notificar colisão

    void Issue(Command task);                                           // Adiciona um comando a fila de tarefas      
    static Character* player;                                           // Ponteiro estático para a instância do jogador principal

    void Damage(int damage);                                            // Novo método para colisão

    Vec2 GetCenter();                                                   // Para pegar o centro do personagem
    int GetDamage();                                                    // Para Gun saber quanto dano esse personagem causa
    

private:
    std::weak_ptr<GameObject> gun;                                      // Referência fraca para o GameObject da arma
    std::queue<Command> taskQueue;                                      // Fila de comando a serem executados

    Vec2 speed;                                                         // Velocidade do personagem
    float linearSpeed;                                                  // Velocidade linear base (módulo de velocidade)
    int hp, damage;                                                     // Pontos de vida e dano do character
    Timer deathTimer;                                                   // Timer para a remoção da morte

    Timer damageCooldownTimer;
    Sound hitSound;
    Sound deathSound;

    CharacterRole role;                                                 // Para saber quem character é
    bool facingLeft;                                                    // Lembrar pra onde deve olhar conforme a ultima tecla apertada

};

#endif 