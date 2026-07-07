#ifndef ENDSTATE_H
#define ENDSTATE_H

#include "core/State.h"
#include "audio/Music.h"

#include <memory>
#include <string>
#include <vector>

class GameObject;
struct SDL_Texture;
struct SDL_Renderer;

class EndState : public State {
public:
    EndState();
    ~EndState();

    void LoadAssets() override;
    void Update(float dt) override;
    void Render() override;

    void Start() override;
    void Pause() override;
    void Resume() override;

private:
    Music backgroundMusic;
    GameObject* gameOverTitle = nullptr;
    GameObject* causeSubtitle = nullptr;
    GameObject* continuePrompt = nullptr;

    // ── Rolo de créditos finais (só na vitória) ───────────────────────────────
    // Um item já rasterizado do rolo: texto OU imagem, com o espaço a deixar
    // depois dele. As texturas ficam num shared_ptr para limpeza automática.
    struct CreditsItem {
        std::shared_ptr<SDL_Texture> tex;
        int w = 0;
        int h = 0;
        float gapAfter = 24.0f;
    };
    bool creditsMode = false;
    bool creditsBuilt = false;
    bool creditsFinished = false;   // rolou tudo — mostra "Pressione Espaço"
    float creditsScrollY = 0.0f;
    float creditsContentHeight = 0.0f;
    std::vector<CreditsItem> creditsItems;

    void BuildCredits(SDL_Renderer* renderer);
    void RenderCredits(SDL_Renderer* renderer);
};

#endif