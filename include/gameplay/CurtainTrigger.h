#ifndef CURTAINTRIGGER_H
#define CURTAINTRIGGER_H

#include "engine/Component.h"

// Tipo de trigger
// OPEN:  qualquer irmão entra -> abre a cortina
// CLOSE: AMBOS os irmãos entram -> fecha a cortina
enum class CurtainTriggerAction { OPEN, CLOSE };

class CurtainTrigger : public Component {
public:
    CurtainTrigger(GameObject& associated, int curtainId,
                   CurtainTriggerAction action);
    ~CurtainTrigger() = default;

    void Start()          override {}
    void Update(float dt) override;
    void Render()         override;

private:
    int                  curtainId;
    CurtainTriggerAction action;

    // Para trigger de fechar: rastreia quais irmãos já entraram
    bool bigEntered   = false;
    bool smallEntered = false;
};

#endif