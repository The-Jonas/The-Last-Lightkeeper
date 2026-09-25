#ifndef JORNAL_H
#define JORNAL_H

#include "engine/Component.h"
#include "ui/DialogueBox.h"
#include "math/Vec2.h"

#include <string>
#include <vector>

class Jornal : public Component {
public:
    Jornal(GameObject& associated, std::string imagePath, int heightLevel,
           std::string soundPath = "", float zoomFactor = 1.0f);

    const std::string& GetImagePath()  const { return imagePath; }
    float              GetZoomFactor() const { return zoomFactor; }
    const std::string& GetSoundPath()  const { return soundPath; }

    int GetHeightLevel() const { return heightLevel; }
    Vec2 GetCenter() const;
    GameObject& GetAssociated() { return associated; }
    const GameObject& GetAssociated() const { return associated; }

    void SetDialogueLines(std::vector<DialogueBox::Line> lines, bool once) {
        dialogueLines = std::move(lines);
        dialogueOnce  = once;
    }
    bool HasPendingDialogue() const {
        return !dialogueLines.empty() && !(dialogueOnce && dialogueFired);
    }
    const std::vector<DialogueBox::Line>& GetDialogueLines() const { return dialogueLines; }
    void MarkDialogueFired() { dialogueFired = true; }

    static Jornal* Spawn(float worldX, float worldY,
                         const std::string& spritePath,
                         const std::string& imagePath,
                         int heightLevel,
                         std::vector<Jornal*>& outList,
                         const std::string& soundPath = "",  
                         float zoomFactor = 1.0f);

    void SetZoomable(bool z) { zoomable = z; }   // marcado no Tiled: aceita zoom com F
    bool IsZoomable() const { return zoomable; }

    void Start() override;
    void Update(float dt) override;
    void Render() override;

private:
    std::string imagePath;
    std::string soundPath;   
    float       zoomFactor;  
    int         heightLevel;

    std::vector<DialogueBox::Line> dialogueLines;
    bool dialogueOnce  = true;
    bool dialogueFired = false;
    bool zoomable = false;
};

#endif
