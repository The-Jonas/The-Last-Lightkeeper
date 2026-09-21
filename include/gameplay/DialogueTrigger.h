#ifndef DIALOGUE_TRIGGER_H
#define DIALOGUE_TRIGGER_H

#include "engine/Component.h"
#include "ui/DialogueBox.h"
#include <vector>
#include <string>

class GameObject;

// Dispara uma fala/conversa quando qualquer um dos irmãos pisa na área
// (um retângulo desenhado no Tiled). 100% configurado por lá — sem código
// novo pra cada diálogo de lugar.
class DialogueTrigger : public Component {
public:

    DialogueTrigger(GameObject& associated, std::vector<DialogueBox::Line> lines, bool once);

    void Start()          override {}
    void Update(float dt) override;             // Checa se algum irmão pisou dentro; dispara a conversa
    void Render()          override {}

private:
    std::vector<DialogueBox::Line> lines;
    bool once;                                  // true = só dispara 1 vez na vida do nível
    bool fired = false;
};

#endif