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
    // creditsOnly=true: aberto pelo menu "Créditos" (rola os créditos sem a
    // intro de vitória e sem depender de GameData::playerVictory).
    explicit EndState(bool creditsOnly = false);
    ~EndState();

    void LoadAssets() override;
    void Update(float dt) override;
    void Render() override;

    void Start() override;
    void Pause() override;
    void Resume() override;

private:
    bool creditsOnly = false;   // aberto pelo menu (créditos sem intro de vitória)
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
        bool centered = false;   // imagens (e avisos de imagem ausente) centralizam; nomes vão à esquerda
    };
    bool creditsMode = false;
    bool creditsBuilt = false;
    bool creditsFinished = false;   // rolou tudo — mostra "Pressione Espaço"
    float creditsScrollY = 0.0f;
    float creditsContentHeight = 0.0f;
    std::vector<CreditsItem> creditsItems;

    // ── Intro de vitória ("Vocês escaparam" + agradecimento) ──────────────────
    // Antes do rolo, uma tela negra com o texto de escape aparece e some; só
    // então os créditos começam a subir. Espaço pula a intro.
    bool introDone = false;
    float introTimer = 0.0f;
    static constexpr float kIntroDuration = 6.0f;
    void RenderVictoryIntro(SDL_Renderer* renderer);

    void BuildCredits(SDL_Renderer* renderer);
    void RenderCredits(SDL_Renderer* renderer);

    // Sai do EndState e volta ao menu principal (empilha um TitleState novo).
    void ReturnToMainMenu();
};

#endif