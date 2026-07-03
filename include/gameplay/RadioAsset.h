#ifndef RADIOASSET_H
#define RADIOASSET_H
 
#include "engine/Component.h"
#include "engine/GameObject.h"
#include "audio/Sound.h"
#include <string>
 
class RadioAsset : public Component {
public:
    RadioAsset(GameObject& associated, const std::string& soundPath);
    ~RadioAsset() override = default;
 
    void Start()  override {}
    void Update(float dt) override;
    void Render() override {}
 
    // Chamado pelo StageState quando o jogador pressiona E
    void Toggle();
    bool IsPlaying() const { return isPlaying; }
    GameObject& GetAssociated() { return associated; }

        float playDuration = 8.0f;
 
private:
    Sound radioSound;
    bool  isPlaying = false;
 
    float playTimer   = 0.0f; 
};
 
#endif