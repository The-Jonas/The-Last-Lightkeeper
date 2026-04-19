#ifndef AICONTROLLER_H
#define AICONTROLLER_H

#include "Component.h"
#include "Timer.h"
#include "Vec2.h"

class AIController : public Component {
public:
    AIController(GameObject& associated);
    ~AIController();


    void Update(float dt) override;
    void Render() override;

    static int npcCounter;        

private:
    // Estado da máquina de estados [cite: 2225-2226]
    enum AIState { MOVING, RESTING };
    AIState state;

    Timer restTimer;      // Timer para controlar o cooldown 
    Vec2 destination;     // Destino do movimento 

};




#endif