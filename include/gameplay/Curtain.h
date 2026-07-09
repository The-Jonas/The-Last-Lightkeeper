#ifndef CURTAIN_H
#define CURTAIN_H

#include "engine/Component.h"

class Collider;

class Curtain : public Component {
public:
    Curtain(GameObject& associated, int curtainId,
            bool startsOpen    = false,
            float animDuration = 2.0f);
    ~Curtain() = default;

    void Start()          override;
    void Update(float dt) override;
    void Render()         override;

    int  GetCurtainId()   const { return curtainId; }
    void Open();   // chamado pelo CurtainTrigger
    void Close();  // chamado pelo CurtainTrigger

    bool IsOpen()   const { return state == CurtainState::OPEN   || state == CurtainState::OPENING; }
    bool IsClosed() const { return state == CurtainState::CLOSED || state == CurtainState::CLOSING; }
    void NotifyCollision(GameObject& other) override;

    GameObject& GetAssociated() { return associated; }

private:
    int   curtainId;
    bool  startsOpen;
    float animDuration;

    float fullHeight    = 1369.0f;
    float minHeight     =  272.0f;
    float currentHeight = 1369.0f;

    enum class CurtainState { CLOSED, OPENING, OPEN, CLOSING };
    CurtainState state = CurtainState::CLOSED;

    Collider* col = nullptr;
};

#endif