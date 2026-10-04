#ifndef HINT_SYSTEM_H
#define HINT_SYSTEM_H

#define INCLUDE_SDL
#include "SDL_include.h"
#include "core/InputManager.h"
#include "math/Vec2.h"
#include "tutorial/HintContext.h"
#include "ui/DialogueBox.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class StageState;
class FloorScript;

// Elementos da HUD que uma dica pode destacar.
enum class HudSlot { None, Fuel, Wheel, Folder, Count };

// ─────────────────────────────────────────────────────────────────────────────
//  Uma dica de "travado": os roteiros a descrevem A CADA FRAME com o estado
//  atual (pending/learned/âncora) e o HintSystem cuida do resto.
//
//  Escalada, contada só enquanto `pending` e sem silêncio:
//    nada → (lineAfter s) fala do irmão → (+glyphAfter s) tecla + ícone + rótulo.
//  `learned` encerra a dica para sempre (vai no save).
// ─────────────────────────────────────────────────────────────────────────────
struct HintSpec {
    std::string id;                          // chave salva quando aprendida
    bool  pending = false;                   // a situação pede a mecânica agora
    bool  learned = false;                   // o jogador fez: some e não volta
    float lineAfter  = -1.0f;                // s travado até a fala (< 0 = sem fala)
    float glyphAfter = 6.0f;                 // s depois da fala (ou do começo) até a tecla
    std::vector<DialogueBox::Line> line;     // a fala (vai para o histórico)
    std::string lineContext = "Conversa";    // título no histórico da pasta
    std::string keys;                        // "[F]" (HintSystem::Key) — vazio = sem tecla
    std::string icon;                        // sprite do item ao lado da tecla
    std::string icon2;                       // 2º item: vira "icon + icon2 + tecla"
    std::string label;                       // ação curta ao lado da tecla ("Usar item")
    Vec2  anchor;                            // ponto no MUNDO; a tecla fica logo acima
    bool  hasAnchor = false;
    HudSlot anchorHud = HudSlot::None;       // ou ancorada à DIREITA de um elemento da HUD
    HudSlot ring    = HudSlot::None;         // anel de atenção num elemento da HUD
    HudSlot hudKeys = HudSlot::None;         // pede à HUD as próprias teclas (roda, pasta)
    int   priority = 1;                      // maior toma a vez de uma menor (2 = sobrevivência: luz)
    float linger = 0.0f;                     // segundos que a tecla continua na tela depois de aprendida
};

// ─────────────────────────────────────────────────────────────────────────────
//  HintSystem — tutorial diegético. Dono do roteiro do andar atual e das dicas
//  comuns a todos; aplica as regras de silêncio e desenha as teclas no mundo.
//
//  Ordem do frame: roteiro do andar primeiro, depois as dicas comuns.
//  Silêncio (nenhuma escalada, teclas apagam): diálogo ou dublagem, pausa,
//  pasta ou documento abertos, perseguição e os primeiros segundos do andar.
//  Uma coisa por vez: enquanto uma tecla está na tela, as outras não contam
//  tempo (a não ser que tenham prioridade maior, aí tomam a vez); quando ela
//  sai, kHintGap s de respiro antes da próxima.
// ─────────────────────────────────────────────────────────────────────────────
class HintSystem {
public:
    HintSystem();
    ~HintSystem();

    void EnterLevel(int levelIndex, StageState& stage);  // troca o roteiro e zera os tempos do andar
    void Update(StageState& stage, const HintContext& ctx, float dt);
    // worldToScreen converte as âncoras; desenha teclas, ícones e anéis da HUD.
    void Render(SDL_Renderer* renderer, const std::function<Vec2(const Vec2&)>& worldToScreen);

    // ── Usado pelos roteiros ─────────────────────────────────────────────────
    void Stuck(const HintSpec& spec);                    // descreve uma dica neste frame
    bool Learned(const std::string& id) const { return learned.count(id) > 0; }
    void MarkLearned(const std::string& id);
    bool Once(const std::string& id);                    // true só na 1ª vez (e marca)
    void Say(StageState& stage, const std::string& id, const std::string& context,
             const std::vector<DialogueBox::Line>& lines);   // fala na caixa + histórico
    void FocusHud(HudSlot slot, float seconds);          // anel de atenção por um tempo
    bool IsSilent() const { return silent; }
    void HoldCommonBeats() { commonHeld = true; }        // o andar está no meio de uma cena: comuns esperam
    bool CommonBeatsHeld() const { return commonHeld; }
    bool IsIdle() const;                                 // sem silêncio, sem tecla na tela e fora do respiro
    float TimeInLevel() const { return timeInLevel; }
    bool ConsumeFuelLimitHit();                          // evento "bolsa cheia de combustível"

    static std::string Key(GameAction action);           // "[F]" com a tecla configurada
    static DialogueBox::Line Big(const std::string& text,
                                 DialogueBox::Emotion e = DialogueBox::Emotion::Normal,
                                 DialogueBox::Emotion listener = DialogueBox::Emotion::Normal);
    static DialogueBox::Line Small(const std::string& text,
                                   DialogueBox::Emotion e = DialogueBox::Emotion::Normal,
                                   DialogueBox::Emotion listener = DialogueBox::Emotion::Normal);

    // ── Usado pela HUD ───────────────────────────────────────────────────────
    void ReportHudRect(HudSlot slot, const SDL_FRect& rect);   // onde o elemento está neste frame
    bool WantsHudKeys(HudSlot slot) const;               // a HUD deve mostrar as próprias teclas
    bool HasGlyphAt(HudSlot slot) const;                 // uma dica está desenhada ao lado do elemento
    void NotifyFuelLimitHit() { fuelLimitHit = true; }   // HotbarComponent: 3º combustível recusado

    // ── Save ─────────────────────────────────────────────────────────────────
    std::vector<std::string> GetLearnedList() const;
    void SetLearnedList(const std::vector<std::string>& ids);

    static constexpr float kQuietStart = 2.0f;           // s de silêncio ao entrar no andar
    static constexpr float kHintGap    = 2.0f;           // s de respiro entre uma dica sair e outra entrar

private:
    struct Runtime {                                     // estado de uma dica entre frames
        HintSpec spec;                                   // última descrição recebida
        float stuck = 0.0f;                              // s travado (sem silêncio)
        bool  lineSaid = false;
        float alpha = 0.0f;                              // fade da tecla 0..1
        bool  touched = false;                           // descrita neste frame
        bool  showing = false;                           // tecla na tela (dona da vez)
        float lingerLeft = -1.0f;                        // >= 0: aprendida, ainda na tela (spec.linger)
    };

    void TickFades(float dt);
    void ReleaseOwner();                                 // a dica da vez saiu: abre o respiro
    void RenderHint(SDL_Renderer* renderer, const Runtime& rt,
                    const std::function<Vec2(const Vec2&)>& worldToScreen) const;

    std::unique_ptr<FloorScript> common;                 // dicas de todos os andares
    std::unique_ptr<FloorScript> floor;                  // roteiro do andar (pode ser nulo)
    std::unordered_set<std::string> learned;
    std::unordered_map<std::string, Runtime> hints;
    std::string owner;                                   // id da dica com a tecla na tela

    StageState* stageInUpdate = nullptr;                 // válido só durante o Update (falas do Stuck)
    int   levelIndex = 0;
    float dtThisFrame = 0.0f;
    float timeInLevel = 0.0f;
    float clock = 0.0f;                                  // anima tremor e pulsação
    bool  silent = true;
    bool  overlayOpen = false;                           // pausa/pasta: até os anéis somem
    bool  fuelLimitHit = false;
    bool  commonHeld = false;                            // HoldCommonBeats neste frame
    float gapTimer = 0.0f;                               // > 0 = respiro: nenhuma dica nova aparece

    float hudFocus[static_cast<int>(HudSlot::Count)] = {};       // s restantes de anel
    float hudRingAlpha[static_cast<int>(HudSlot::Count)] = {};
    SDL_FRect hudRect[static_cast<int>(HudSlot::Count)] = {};
    bool  hudRectValid[static_cast<int>(HudSlot::Count)] = {};
};

#endif
