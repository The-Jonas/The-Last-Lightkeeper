#ifndef JORNAL_H
#define JORNAL_H

#include "engine/Component.h"
#include "math/Vec2.h"

#include <string>
#include <vector>

class Jornal : public Component {
public:
    Jornal(GameObject& associated, std::string imagePath, int heightLevel);

    const std::string& GetImagePath() const { return imagePath; }
    int GetHeightLevel() const { return heightLevel; }
    Vec2 GetCenter() const;
    GameObject& GetAssociated() { return associated; }
    const GameObject& GetAssociated() const { return associated; }

    static Jornal* Spawn(float worldX, float worldY, const std::string& imagePath, int heightLevel,
                         std::vector<Jornal*>& outList);

    void Start() override;
    void Update(float dt) override;
    void Render() override;

private:
    std::string imagePath;
    int heightLevel;
};

#endif
