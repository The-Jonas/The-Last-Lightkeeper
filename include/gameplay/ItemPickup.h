#ifndef ITEM_PICKUP_H
#define ITEM_PICKUP_H

#include "engine/Component.h"
#include "gameplay/Item.h"
#include "math/Vec2.h"
#include "ui/DialogueBox.h"

#include <vector>
#include <string>

class ItemPickup : public Component {
public:
    ItemPickup(GameObject& associated, const ItemDef& def, int durability);

    const ItemDef* GetDef() const { return &def; }
    int GetDurability() const { return durability; }
    Vec2 GetCenter() const;
    GameObject& GetAssociated() { return associated; }
    void Destroy();

    static ItemPickup* Spawn(float worldX, float worldY, const ItemDef& def, int durability,
                      std::vector<ItemPickup*>& outList);
    
    // Conversa do Tiled ao pegar este item (ItemSpawn: dialogue / dialogue_context).
    void SetDialogue(std::vector<DialogueBox::Line> lines, std::string context) {
        dialogueLines   = std::move(lines);
        dialogueContext = std::move(context);
    }
    const std::vector<DialogueBox::Line>& GetDialogueLines() const { return dialogueLines; }
    const std::string& GetDialogueContext() const { return dialogueContext; }

    void SetHeightLevel(int heightlevel);
    int GetHeightLevel() const {return HeightLevel;}

private:
    ItemDef def;
    int durability;
    int HeightLevel;

    std::vector<DialogueBox::Line> dialogueLines;
    std::string dialogueContext;
};

#endif
