#include "ui/DialogueBox.h"
#include "core/InputManager.h"
#include "core/Resources.h"
#include "core/Game.h"
#include "SDL_include.h"
#include <algorithm>
#include <cctype>
#include <sstream>

// Carrega as variações do blip uma única vez.
DialogueBox::DialogueBox() {
    martinBlip = std::make_shared<Sound>("Recursos/audio/SFX/UI/blip_martin.wav"); 
    lukeBlip   = std::make_shared<Sound>("Recursos/audio/SFX/UI/blip_luke.wav");   
}

// Nome de exibição na caixa
std::string DialogueBox::DisplayName(Speaker s) {
    switch (s) {
        case Speaker::BigBrother:    return "Martin";
        case Speaker::LittleBrother: return "Luke";
        default:                     return "";
    }
}

// Resolve qual som usar
Sound* DialogueBox::BlipFor(Speaker s) const {
    switch (s) {
        case Speaker::BigBrother:    return martinBlip.get();
        case Speaker::LittleBrother: return lukeBlip.get();
        default:                     return nullptr; 
    }
}

// Quebra o texto em páginas que cabem na caixa, sem cortar palavra no meio.
std::deque<std::string> DialogueBox::SplitIntoPages(const std::string& text) const {
    std::deque<std::string> pages;
    std::istringstream words(text);
    std::string word, current;

    while (words >> word) {
        const std::string candidate = current.empty() ? word : (current + " " + word);
        if (candidate.size() > DialogueTuning::maxCharsPerPage && !current.empty()) {
            pages.push_back(current);
            current = word;
        } else {
            current = candidate;
        }
    }
    if (!current.empty()) pages.push_back(current);
    if (pages.empty()) pages.push_back("");  

    return pages;
}

// Bota uma fala na fila. Se nada estiver tocando agora, começa na hora.
void DialogueBox::Queue(Speaker s, Speaker l, Emotion e, Emotion le, const std::string& text) {
    queue.push_back({s, l, e, le, text});
    if (!active) { Advance(); active = true; }
}

// Pega a próxima FALA (Line) da fila 
void DialogueBox::Advance() {
    if (queue.empty()) { active = false; return; }

    const Line next = queue.front();
    queue.pop_front();

    speaker  = next.speaker;
    listener = next.listener;
    emotion  = next.emotion;
    listenerEmotion = next.listenerEmotion;

    pendingPages = SplitIntoPages(next.text);
    fullText = pendingPages.front();
    pendingPages.pop_front();

    revealedChars = 0;
    typeTimer = 0.0f;
    autoAdvanceTimer = -1.0f;
    mouthFlapToggle = 0;
}

// Chamado quando o jogador pede pra avançar (Espaço) ou o timer de espera estoura.
void DialogueBox::AdvancePageOrLine() {
    if (!pendingPages.empty()) {
        fullText = pendingPages.front();
        pendingPages.pop_front();
        revealedChars = 0;
        typeTimer = 0.0f;
        autoAdvanceTimer = -1.0f;
        mouthFlapToggle = 0;
        return;
    }
    Advance();
}

// Toca 1 blip a cada 2 letras reveladas, pulando espaço 
void DialogueBox::PlayBlipIfDue(size_t newRevealedCount) {
    if (newRevealedCount == 0 || newRevealedCount > fullText.size()) return;
    const char c = fullText[newRevealedCount - 1];
    if (std::isspace(static_cast<unsigned char>(c))) return;
    if (newRevealedCount % DialogueTuning::blipEveryNChars != 0) return;

    if (Sound* blip = BlipFor(speaker)) blip->Play();
    mouthFlapToggle ^= 1;   
}

void DialogueBox::Update(float dt) {
    if (!active) return;

    InputManager& input = InputManager::GetInstance();
    const bool skipPressed = input.KeyPress(SDLK_SPACE);
    const bool fullyRevealed = revealedChars >= fullText.size();

    if (!fullyRevealed) {
        if (skipPressed) {
            revealedChars = fullText.size();   
        } else {
            typeTimer += dt * DialogueTuning::charsPerSecond;
            while (typeTimer >= 1.0f && revealedChars < fullText.size()) {
                revealedChars++;
                typeTimer -= 1.0f;
                PlayBlipIfDue(revealedChars);
            }
        }
        return;
    }

    if (skipPressed) { AdvancePageOrLine(); return; }

    if (autoAdvanceTimer < 0.0f) autoAdvanceTimer = DialogueTuning::autoAdvanceDelay;
    autoAdvanceTimer -= dt;
    if (autoAdvanceTimer <= 0.0f) AdvancePageOrLine();
}

std::string DialogueBox::PortraitPath(Speaker s, Emotion e, bool mouthOpen) const {
    if (s == Speaker::None) return "";
    const std::string who   = (s == Speaker::BigBrother) ? "irmaozao" : "irmaozinho";
    const std::string emo   = (e == Emotion::Normal) ? "NORMAL" : (e == Emotion::Fear) ? "MEDO" : "DUVIDA";
    const std::string mouth = mouthOpen ? "aberta" : "fechada";
    return "Recursos/img/ui/portraits/" + who + "/" + emo + "/" + mouth + ".png";
}

void DialogueBox::Render(SDL_Renderer* renderer, int windowW, int windowH) {
    if (!renderer || !active) return;

    const bool mouthOpen = (revealedChars < fullText.size()) && (mouthFlapToggle == 0);

    // ── Caixa de fundo ──────────
    auto boxTex = Resources::GetImage("Recursos/img/ui/caixa_dialogo.png");
    int boxTexW = 400, boxTexH = 140;   
    if (boxTex) SDL_QueryTexture(boxTex.get(), nullptr, nullptr, &boxTexW, &boxTexH);

    const float scale = Game::UiScale() * DialogueTuning::boxScaleMul; 
    const int boxW = static_cast<int>(boxTexW * scale);
    const int boxH = static_cast<int>(boxTexH * scale);
    const int margin = static_cast<int>(DialogueTuning::margin * scale);
    const int boxX = (windowW - boxW) / 2;    
    const int boxY = windowH - boxH - margin;

    const SDL_Rect boxDst{boxX, boxY, boxW, boxH};
    if (boxTex) {   
        SDL_RenderCopy(renderer, boxTex.get(), nullptr, &boxDst);
    } else {
        SDL_SetRenderDrawColor(renderer, 30, 30, 40, 220);
        SDL_RenderFillRect(renderer, &boxDst);
    }

    // ── Retrato de quem FALA — sempre à esquerda ────────────────────────────
    auto speakerTex = Resources::GetImage(PortraitPath(speaker, emotion, mouthOpen));
    if (speakerTex) {
        int pw = 0, ph = 0;
        SDL_QueryTexture(speakerTex.get(), nullptr, nullptr, &pw, &ph);
        const int portraitH = static_cast<int>(boxH * DialogueTuning::portraitHeightMul); // ex: 1.0 = mesma altura da caixa
        const int speakerW  = static_cast<int>(pw * (static_cast<float>(portraitH) / ph));
        const int overlapX  = static_cast<int>(speakerW * DialogueTuning::portraitOverlapMul); // quanto "invade" a caixa

        // Sobra de altura (se portraitHeightMul > 1) empurra o retrato pra cima, mas a
        // BASE dele fica alinhada com a base da caixa — nunca flutua acima sozinho.
        const int extraH = portraitH - boxH;
        const SDL_Rect pd{ boxX - speakerW + overlapX, boxY - extraH, speakerW, portraitH };
        SDL_RenderCopy(renderer, speakerTex.get(), nullptr, &pd);
    }

    // ── Retrato de quem OUVE — direita, só se houver ouvinte ────────────────
    if (listener != Speaker::None) {
        auto listenerTex = Resources::GetImage(PortraitPath(listener, listenerEmotion, false)); // ouvinte: boca sempre fechada
        if (listenerTex) {
            int pw = 0, ph = 0;
            SDL_QueryTexture(listenerTex.get(), nullptr, nullptr, &pw, &ph);
            const int portraitH = static_cast<int>(boxH * DialogueTuning::portraitHeightMul);
            const int listenerW = static_cast<int>(pw * (static_cast<float>(portraitH) / ph));
            const int overlapX  = static_cast<int>(listenerW * DialogueTuning::portraitOverlapMul);
            const int extraH = portraitH - boxH;

            const SDL_Rect pd{ boxX + boxW - overlapX, boxY - extraH, listenerW, portraitH };
            SDL_RenderCopyEx(renderer, listenerTex.get(), nullptr, &pd, 0.0, nullptr, SDL_FLIP_HORIZONTAL);
        }
    }

    // ── Nome do falante (área separada da caixa) ────────────────────────────
    auto nameFont = Resources::GetFont("Recursos/font/times.ttf", std::max(10, static_cast<int>(DialogueTuning::nameFontSize * scale)));
    if (nameFont) {
        const SDL_Color nameCol{200, 180, 110, 255};
        SDL_Surface* nsf = TTF_RenderUTF8_Blended(nameFont.get(), DisplayName(speaker).c_str(), nameCol);
        if (nsf) {
            SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, nsf);

            // Canto do quadrado do nome, na arte (offset a partir do canto da caixa):
            const int nameBoxX = boxX + static_cast<int>(DialogueTuning::namePadX * scale);
            const int nameBoxY = boxY + static_cast<int>(DialogueTuning::namePadY * scale);
            const int nameBoxW = static_cast<int>(DialogueTuning::nameBoxW * scale);
            const int nameBoxH = static_cast<int>(DialogueTuning::nameBoxH * scale);

            // Centraliza o texto dentro desse quadrado, não só ancora no canto.
            const SDL_Rect nd{
                nameBoxX + (nameBoxW - nsf->w) / 2,
                nameBoxY + (nameBoxH - nsf->h) / 2,
                nsf->w, nsf->h
            };
            SDL_FreeSurface(nsf);
            if (nt) { SDL_RenderCopy(renderer, nt, nullptr, &nd); SDL_DestroyTexture(nt); }
        }
    }

    // ── Texto da página atual ────────────────────────────────────────────────
    auto font = Resources::GetFont("Recursos/font/times.ttf", std::max(12, static_cast<int>(DialogueTuning::textFontSize * scale)));
    if (!font) return;

    const std::string shown = fullText.substr(0, revealedChars);
    const SDL_Color col{245, 232, 200, 255};

    const int textBoxX = boxX + static_cast<int>(DialogueTuning::textPadX * scale);
    const int textBoxY = boxY + static_cast<int>(DialogueTuning::textPadY * scale);
    const int textAreaW = static_cast<int>(DialogueTuning::textBoxW * scale);

    // Renderiza só o trecho já revelado, sempre ancorado no mesmo ponto fixo
    // (textBoxX, textBoxY) — sem centralizar por altura, então o início do
    // texto nunca muda de lugar entre uma fala curta e uma longa.
    SDL_Surface* sf = TTF_RenderUTF8_Blended_Wrapped(font.get(), shown.c_str(), col, textAreaW);
    if (!sf) return;
    SDL_Texture* textTex = SDL_CreateTextureFromSurface(renderer, sf);
    const int tw = sf->w, th = sf->h;
    SDL_FreeSurface(sf);
    if (!textTex) return;

    const SDL_Rect textDst{textBoxX, textBoxY, tw, th};
    SDL_RenderCopy(renderer, textTex, nullptr, &textDst);
    SDL_DestroyTexture(textTex);
}