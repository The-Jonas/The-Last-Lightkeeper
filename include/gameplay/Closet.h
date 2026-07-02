#ifndef CLOSET_H
#define CLOSET_H

#include "engine/Component.h"
#include "engine/GameObject.h"
#include <string>

#define INCLUDE_SDL
#include "SDL_include.h"

class Closet : public Component {
public:
    Closet(GameObject& associated, const std::string& direction);
    ~Closet();

    void Start() override;
    void Update(float dt) override;
    void Render() override;

private:
    std::string direction;
    GameObject* textObj;
    bool showPrompt;
    bool isOccupied;
    int frestaLightId;

    Vec2 playerEntryPos;
    Vec2 brotherEntryPos;

    void EnterCloset();
    void ExitCloset();

    // Funções auxiliares para lidar com a direção
    SDL_Rect GetInteractionRect() const;
    

};

#endif 