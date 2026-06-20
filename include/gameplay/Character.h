#ifndef CHARACTER_H
#define CHARACTER_H

#include "engine/Component.h"
#include "gameplay/Inventory.h"
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

    /// @param useIrmaozaoIdleStrips Só para o irmãozão: usa `irmaozao_idle/` parado e `irmaozao_walk/` em movimento (6 frames por direção).
    Character(GameObject& associated, std::string spritePath, bool useIrmaozaoIdleStrips = false);
    ~Character();                                                       // Destrutor 

    // Métodos do ciclo de vida
    void Start() override;                                              // Metodo para inicar o objeto
    void Update(float dt) override;  
    void Render() override;                                             // Deixando vazio
    void NotifyCollision(GameObject& other) override;                   // Para notificar colisão

    void Issue(Command task);                                           // Adiciona um comando a fila de tarefas      
    static Character* player;                                           // Ponteiro estático para a instância do jogador principal
    static Character* littleBrother;

    // Máquina de estados pra ações
    enum class ActionState {NORMAL, INTERACTING, PUSHING_BOX};
    ActionState currentState = ActionState::NORMAL;
    float interactTimer = 0.0f;

    void SetSpeedMultiplier(float multiplier);                          // Ajusta multiplicador de velocidade do personagem
    void SetBaseSpeed(float speed);                                     // Ajusta velocidade base de movimento
    void PositionForCoop(Character* leader);                            // Pra pegar a posição de onde os irmãos estão olhando
    SDL_Rect GetInteractionRect(int targetHeightLevel = 0) const;       // Retorna a caixa de iteração projetada na frente do personagem
    bool isElevated = false;                                            // Flag pra saber se o personagem está acima de algo Exemplo: Andando sob a escada.
    bool hidePersonalLight = false;                                     // Esconder a luz mesmo ela estando ativada
    float stairAnchorY = 0.0f;                                          // GUARDA A PROFUNDIDADE DA ESCADA DO ANDAR ATUAL

    void PreloadAnimationFrames();                                      // Carrega todos os frames no Start() para evitar stutter


    // Sistema de Sanidade (medo da escuridão)
    float sanity = 100.0f;          
    constexpr static float kMaxSanity = 100.0f;

    // Círculo dos pés (mesmo de colisão com chão / desenho DEBUG): raio = largura * 0.25, centro nos pés.
    Vec2 GetFootCircleCenter() const;
    float GetFootCircleRadius() const;

    GameObject& GetAssociated() { return associated; }
    Vec2 GetSpeed() const { return speed; }
    Vec2 GetCenter();                                                   // Para pegar o centro do personagem
    void ClearMovement();

    enum class Direction { UP, DOWN, LEFT, RIGHT };
    Direction GetFacingDirection() const { return currentDirection; }
    void NotifyInventoryLightChanged();
    void PlayPickLampAnimation();

    /// Sprite atual (sem props) para desenhar sombra — não inclui itens na mão.
    std::string GetShadowSpritePath() const;
    const std::string& GetCurrentStripSpritePath() const { return lastStripSpritePath; }

    // Poder do irmãozinho — visão do monstro
    float visionPowerTimer    = 0.0f;   // Quanto tempo ainda ativa
    float visionCooldown      = 0.0f;   // Tempo até poder usar de novo
    static constexpr float kVisionDuration  = 3.0f;   // Duração ativa
    static constexpr float kVisionCooldown  = 12.0f;  // Recarga

private:
    std::queue<Command> taskQueue;                                      // Fila de comando a serem executados

    Vec2 speed;                                                         // Velocidade do personagem
    Vec2 targetSpeed;                                                   // Velocidade-alvo usada na suavização
    float linearSpeed;                                                  // Velocidade linear base (módulo de velocidade)
    float speedMultiplier;                                              // Multiplicador aplicado sobre linearSpeed
    float acceleration;                                                 // Taxa de aceleração em movimento
    float deceleration;                                                 // Taxa de desaceleração ao soltar input
    
    Direction currentDirection;
    
    std::string baseSpritePath;                                         // guarda o caminho base (Ex: Recursos/img/personagens(irmãozao))

    bool irmaozaoIdleStrips = false;
    float stripAnimTimer = 0.0f;
    int stripFrameIndex = 0;
    bool playingPickLampAnim = false;
    float pickLampAnimTimer = 0.0f;
    int pickLampFrameIndex = 0;
    bool stripWasMoving = false;
    std::string lastStripSpritePath;

    std::string GetAnimStripPath(Direction dir,
                                      int frameIndex,
                                      HeldPropVisual prop = HeldPropVisual::None,
                                      bool moving = false) const;
    std::string IrmaozaoPickLampStripPath(Direction dir, int frameIndex) const;
    void RefreshAnimSprite();
    void EnsureBaselineBox();   
    void RestoreCollisionBox(float centerX, float footY);

    float baselineBoxW = 0.0f;
    float baselineBoxH = 0.0f;
};

#endif 