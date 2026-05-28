#ifndef CHARACTER_H
#define CHARACTER_H

#include "engine/Component.h"
#include "core/Timer.h"
#include "math/Vec2.h"
#include "audio/Sound.h"
#include <queue>
#include <memory>
#include <string>

class GameObject;                                                       // Forward declaration
class Gun;

class Character : public Component {
public:
    // Classe Command
    class Command {
    public:
        enum CommandType{MOVE};                                         // Enum para definir os tipos de comando possíveis
        Command(CommandType type, float x, float y);                    // Construtor do Command
        CommandType type;                                               // O tipo de comando (MOVE ou SHOOT)
        Vec2 pos;                                                       // A posição do alvo (para movimento ou tiro)
    };

    /// @param useIrmaozaoIdleStrips Só para o irmãozão: usa `irmaozao_idle/` (6 frames por direção) para idle e movimento.
    Character(GameObject& associated, std::string spritePath, bool useIrmaozaoIdleStrips = false);
    ~Character();                                                       // Destrutor 

    // Métodos do ciclo de vida
    void Start() override;                                              // Metodo para inicar o objeto
    void Update(float dt) override;  
    void Render() override;                                             // Deixando vazio
    void NotifyCollision(GameObject& other) override;                   // Para notificar colisão

    void Issue(Command task);                                           // Adiciona um comando a fila de tarefas      
    static Character* player;                                           // Ponteiro estático para a instância do jogador principal
    Vec2 GetCenter();                                                   // Para pegar o centro do personagem
    /// Círculo dos pés (mesmo de colisão com chão / desenho DEBUG): raio = largura * 0.25, centro nos pés.
    Vec2 GetFootCircleCenter() const;
    float GetFootCircleRadius() const;
    void SetSpeedMultiplier(float multiplier);                          // Ajusta multiplicador de velocidade do personagem
    void SetBaseSpeed(float speed);                                     // Ajusta velocidade base de movimento
    SDL_Rect GetInteractionRect(int targetHeightLevel = 0) const;                                 // Retorna a caixa de iteração projetada na frente do personagem

    bool isElevated = false;

    Vec2 GetSpeed() const { return speed; }

private:
    std::queue<Command> taskQueue;                                      // Fila de comando a serem executados

    Vec2 speed;                                                         // Velocidade do personagem
    Vec2 targetSpeed;                                                   // Velocidade-alvo usada na suavização
    float linearSpeed;                                                  // Velocidade linear base (módulo de velocidade)
    float speedMultiplier;                                              // Multiplicador aplicado sobre linearSpeed
    float acceleration;                                                 // Taxa de aceleração em movimento
    float deceleration;                                                 // Taxa de desaceleração ao soltar input
    
    enum class Direction {UP, DOWN, LEFT, RIGHT};                       // enum saberá onde o character está andando
    Direction currentDirection;
    
    std::string baseSpritePath;                                         // guarda o caminho base (Ex: Recursos/img/personagens(irmãozao))

    bool irmaozaoIdleStrips = false;
    float stripAnimTimer = 0.0f;
    int stripFrameIndex = 0;

    std::string IrmaozaoIdleStripPath(Direction dir, int frameIndex) const;
    void RefreshIrmaozaoStripSprite();
};

#endif 