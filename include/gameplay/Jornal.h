#ifndef JORNAL_H
#define JORNAL_H

#include "engine/Component.h"
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

    static Jornal* Spawn(float worldX, float worldY,
                         const std::string& spritePath,
                         const std::string& imagePath,
                         int heightLevel,
                         std::vector<Jornal*>& outList,
                         const std::string& soundPath = "",  
                         float zoomFactor = 1.0f);

    void Start() override;
    void Update(float dt) override;
    void Render() override;

private:
    std::string imagePath;
    std::string soundPath;   
    float       zoomFactor;  
    int         heightLevel;
};

#endif
