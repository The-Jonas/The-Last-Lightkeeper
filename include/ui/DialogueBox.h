#ifndef DIALOGUE_BOX_H
#define DIALOGUE_BOX_H

#include "audio/Sound.h"
#include "ui/DialogueTuning.h"
#include <deque>
#include <string>
#include <vector>
#include <memory>

struct SDL_Renderer;
struct _TTF_Font;

class DialogueBox {
public:

    enum class Speaker { BigBrother, LittleBrother, None };
    enum class Emotion { Normal, Fear, Doubt };

    struct Line {
        Speaker speaker;            // Quem fala essa linha (vai sempre à esquerda)
        Speaker listener;           // Quem escuta (Speaker::None = lado direito fica vazio)
        Emotion emotion;            // humor de quem FALA
        Emotion listenerEmotion;    // humor de quem OUVE 
        std::string text;
    };

    DialogueBox();

    void Queue(Speaker speaker, Speaker listener, Emotion emotion, Emotion listenerEmotion, const std::string& text);         
    void Update(float dt);                                                                          // Digitação, Espaço (pular/avançar) e avanço automático
    void Render(SDL_Renderer* renderer, int windowW, int windowH);                                  // Mostra a caixa conforme a resolução da tela
    bool isActive() const { return active; }

    static std::string DisplayName(Speaker s);                                                      // Speaker::BigBrother -> "Martin", LittleBrother -> "Luke"

private:
    void Advance();                                                                                 // Pega a próxima fala da fila, ou encerra se vazia
    void AdvancePageOrLine();                                                                       // Vai pra próxima página da mesma fala, ou pra próxima fala da fila se acabou
    void PlayBlipIfDue(size_t newRevealCount);                                                      // Toca o "tu tu tu" a cada letra nova (pulando espaço)
    Sound* BlipFor(Speaker s) const;                                                                // Retorna o blip do personagem que está falando (nullptr p/ None)
    std::string PortraitPath(Speaker s, Emotion e, bool mouthOpen) const;                           // Monta o path do retrato certo
    std::deque<std::string> SplitIntoPages(const std::string& text) const;                          // Quebra texto grande em pedaços que cabem na caixa (por palavra, não corta no meio)

    std::deque<Line> queue;
    std::deque<std::string> pendingPages;                                                           // Páginas restantes da fala ATUAL
    Speaker speaker  = Speaker::BigBrother;                                                         // quem fala a linha atual
    Speaker listener = Speaker::None;                                                               // quem ouve a linha atual (None = vazio)
    Emotion emotion  = Emotion::Normal;
    Emotion listenerEmotion = Emotion::Normal;
    std::string fullText;

    size_t revealedChars = 0;
    float typeTimer = 0.0f;
    float autoAdvanceTimer = -1.0f;
    bool active = false; 

    std::shared_ptr<Sound> martinBlip;   
    std::shared_ptr<Sound> lukeBlip;

    size_t mouthFlapToggle = 0;          

};


#endif