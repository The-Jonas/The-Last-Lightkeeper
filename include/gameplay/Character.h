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

class GameObject;
class Gun;

class Character : public Component {
public:
    class Command {
        public:
            enum CommandType { MOVE };
            Command(CommandType type, float x, float y);
            CommandType type;
            Vec2 pos;
        };

    enum class ActionState { NORMAL, INTERACTING, PUSHING_BOX };
    enum class Direction { UP, DOWN, LEFT, RIGHT };

    // ── Construtor e Instâncias Globais ───────────────────────────────────────
    Character(GameObject& associated, std::string spritePath, bool useIrmaozaoIdleStrips = false);
    ~Character();

    static Character* player;
    static Character* littleBrother;

    // ── Ciclo de Vida ─────────────────────────────────────────────────────────
    void Start() override;
    void Update(float dt) override;
    void Render() override;
    void NotifyCollision(GameObject& other) override;

    // ── Movimentação e Controles ──────────────────────────────────────────────
    void Issue(Command task);
    void ForceStop() {
        speed = Vec2(0.0f, 0.0f);
        targetSpeed = Vec2(0.0f, 0.0f);
        currentState = ActionState::INTERACTING;
    }

    void ReleaseInteract() {
        currentState = ActionState::NORMAL;
    }
    void ClearMovement();
    void SetSpeedMultiplier(float multiplier);
    void SetBaseSpeed(float speed);
    void PositionForCoop(Character* leader);
    
    Vec2 GetSpeed() const { return speed; }
    Vec2 GetCenter();
    Direction GetFacingDirection() const { return currentDirection; }
    Direction GetCurrentDirection() const { return currentDirection; }

    // ── Estados e Flags ───────────────────────────────────────────────────────
    ActionState currentState = ActionState::NORMAL;
    
    bool isElevated = false;
    bool hidePersonalLight = false;
    bool isHidden = false;
    
    float interactTimer = 0.0f;
    float stairAnchorY = 0.0f;

    // ── Caixas de Colisão e Interação (Hitboxes Constantes) ───────────────────
    SDL_Rect GetInteractionRect(int targetHeightLevel = 0) const;
    SDL_Rect GetFootRect() const;
    SDL_Rect GetFootCollisionRect() const;
    SDL_Rect GetHitRect() const;
    
    Vec2 GetFootCircleCenter() const;
    float GetFootCircleRadius() const;

    // ── Sistema de Sanidade ───────────────────────────────────────────────────
    float sanity = 100.0f;
    constexpr static float kMaxSanity = 100.0f;

    // ── Poderes (Irmãozinho) ──────────────────────────────────────────────────
    float visionPowerTimer = 0.0f;
    float visionCooldown = 0.0f;
    static constexpr float kVisionDuration = 4.5f;
    static constexpr float kVisionCooldown = 12.0f;

    // ── Animação e Renderização ───────────────────────────────────────────────
    void PreloadAnimationFrames();
    void NotifyInventoryLightChanged();
    void PlayPickLampAnimation();
    
    std::string GetShadowSpritePath() const;
    const std::string& GetCurrentStripSpritePath() const { return lastStripSpritePath; }

    GameObject& GetAssociated() { return associated; }

private:
    void UpdateVisionPower(float dt);

    std::queue<Command> taskQueue;

    Vec2 speed;
    Vec2 targetSpeed;
    float linearSpeed;
    float speedMultiplier;
    float acceleration;
    float deceleration;
    
    Direction currentDirection;
    std::string baseSpritePath;

    // ── Controle de Animação ──────────────────────────────────────────────────
    bool irmaozaoIdleStrips = false;
    float stripAnimTimer = 0.0f;
    int stripFrameIndex = 0;
    
    bool playingPickLampAnim = false;
    float pickLampAnimTimer = 0.0f;
    int pickLampFrameIndex = 0;
    
    bool stripWasMoving = false;
    std::string lastStripSpritePath;

    std::string GetAnimStripPath(Direction dir, int frameIndex, HeldPropVisual prop = HeldPropVisual::None, bool moving = false) const;
    std::string IrmaozaoPickLampStripPath(Direction dir, int frameIndex) const;
    
    void RefreshAnimSprite();
    void EnsureBaselineBox();
    void RestoreCollisionBox(float centerX, float footY);

    float baselineBoxW = 0.0f;
    float baselineBoxH = 0.0f;
};

#endif