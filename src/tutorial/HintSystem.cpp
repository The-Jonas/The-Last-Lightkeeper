#include "tutorial/HintSystem.h"

#include "core/Game.h"
#include "core/Telemetry.h"
#include "states/stage/StageState.h"
#include "tutorial/FloorScript.h"
#include "ui/HorrorFx.h"

#include <algorithm>

namespace {

constexpr float kHintFadeIn  = 1.2f;                     // s até a tecla aparecer inteira (surge devagar)
constexpr float kHintFadeOut = 0.6f;
constexpr float kRingFade    = 0.35f;
constexpr float kAnchorGapPx = 8.0f;                     // folga entre a âncora e a base da tecla

int SlotIndex(HudSlot slot) { return static_cast<int>(slot); }

// Aproxima `value` de `target` numa velocidade de 1/seconds por segundo.
void Approach(float& value, float target, float seconds, float dt) {
    const float step = dt / std::max(0.01f, seconds);
    value = (value < target) ? std::min(target, value + step) : std::max(target, value - step);
}

void LogHint(int level, const std::string& id, const char* step) {
    Telemetry::Event("hint", Telemetry::Fields().Int("level", level).Str("id", id).Str("step", step));
}

}  // namespace

HintSystem::HintSystem() : common(MakeCommonHints()) {}

HintSystem::~HintSystem() = default;

// Andar novo: roteiro novo, tempos e teclas zerados. O que foi aprendido continua.
void HintSystem::EnterLevel(int newLevelIndex, StageState& stage) {
    levelIndex = newLevelIndex;
    hints.clear();
    owner.clear();
    gapTimer = 0.0f;
    timeInLevel = 0.0f;
    fuelLimitHit = false;
    for (int i = 0; i < SlotIndex(HudSlot::Count); ++i) {
        hudFocus[i] = 0.0f;
        hudRingAlpha[i] = 0.0f;
        hudRectValid[i] = false;
    }
    common = MakeCommonHints();
    floor  = MakeFloorScript(newLevelIndex);
    if (common) common->Enter(stage, *this);
    if (floor)  floor->Enter(stage, *this);
}

// Decide o silêncio, roda os roteiros (que chamam Stuck) e anima fades e anéis.
void HintSystem::Update(StageState& stage, const HintContext& ctx, float dt) {
    dtThisFrame = dt;
    clock += dt;
    overlayOpen = ctx.overlayOpen;
    if (!ctx.overlayOpen) timeInLevel += dt;
    silent = ctx.dialogueActive || ctx.overlayOpen || ctx.monsterHunting || timeInLevel < kQuietStart;

    for (auto& kv : hints) kv.second.touched = false;
    if (!silent) gapTimer = std::max(0.0f, gapTimer - dt);

    stageInUpdate = &stage;
    commonHeld = false;
    if (floor)  floor->Update(stage, *this, ctx, dt);   // o andar primeiro: pode segurar as comuns
    if (common) common->Update(stage, *this, ctx, dt);
    stageInUpdate = nullptr;
    fuelLimitHit = false;                                // evento vale só para este frame

    // Dica que o roteiro parou de descrever = situação passou.
    for (auto& kv : hints) {
        if (!kv.second.touched && kv.second.showing) {
            kv.second.showing = false;
            if (owner == kv.first) ReleaseOwner();
        }
    }
    TickFades(dt);
}

// Escalada de uma dica: conta o tempo travado, fala, depois mostra a tecla.
void HintSystem::Stuck(const HintSpec& spec) {
    Runtime& rt = hints[spec.id];
    rt.spec = spec;
    rt.touched = true;

    if (Learned(spec.id)) return;

    // Aprendida com a tecla na tela e `linger`: fica mais um pouco, depois sai.
    if (rt.lingerLeft >= 0.0f) {
        rt.lingerLeft -= dtThisFrame;
        if (rt.lingerLeft <= 0.0f) MarkLearned(spec.id);
        return;
    }
    if (spec.learned) {
        if (rt.showing && spec.linger > 0.0f) {
            rt.lingerLeft = spec.linger;
            return;
        }
        MarkLearned(spec.id);
        return;
    }

    const bool hasLine = spec.lineAfter >= 0.0f && !spec.line.empty();
    const float lineAt = hasLine ? spec.lineAfter : 0.0f;

    if (!spec.pending) {
        // Saiu da situação sem aprender: a tecla some e o relógio volta para
        // depois da fala (ela não se repete).
        if (rt.showing) {
            rt.showing = false;
            if (owner == spec.id) ReleaseOwner();
        }
        rt.stuck = std::min(rt.stuck, lineAt);
        return;
    }
    if (silent) return;

    // Outra tecla na vez: espera — a menos que esta seja mais importante.
    Runtime* current = nullptr;
    if (!owner.empty() && owner != spec.id) {
        auto it = hints.find(owner);
        if (it != hints.end()) current = &it->second;
        if (!current || current->spec.priority >= spec.priority) return;
    }

    rt.stuck += dtThisFrame;

    if (hasLine && !rt.lineSaid && rt.stuck >= lineAt) {
        rt.lineSaid = true;
        if (stageInUpdate) Say(*stageInUpdate, spec.id, spec.lineContext, spec.line);
        LogHint(levelIndex, spec.id, "line");
        return;                                          // a fala liga o silêncio; a tecla espera ela acabar
    }

    const bool hasVisual = !spec.keys.empty() || !spec.icon.empty() ||
                           spec.ring != HudSlot::None || spec.hudKeys != HudSlot::None;
    if (hasVisual && !rt.showing && (!hasLine || rt.lineSaid) && rt.stuck >= lineAt + spec.glyphAfter) {
        if (current) {
            // Toma a vez: a outra some e volta a contar do começo depois.
            const bool curHasLine = current->spec.lineAfter >= 0.0f && !current->spec.line.empty();
            current->showing = false;
            current->stuck = curHasLine ? current->spec.lineAfter : 0.0f;
        } else if (gapTimer > 0.0f) {
            return;                                      // respiro depois da dica anterior
        }
        rt.showing = true;
        owner = spec.id;
        LogHint(levelIndex, spec.id, "glyph");
    }
}

// A dica da vez saiu da tela: ninguém é dono e começa o respiro.
void HintSystem::ReleaseOwner() {
    owner.clear();
    gapTimer = kHintGap;
}

// Aprendida: some na hora e entra no save.
void HintSystem::MarkLearned(const std::string& id) {
    if (!learned.insert(id).second) return;
    auto it = hints.find(id);
    if (it != hints.end()) it->second.showing = false;
    if (owner == id) ReleaseOwner();
    LogHint(levelIndex, id, "learned");
}

// Para falas e momentos roteirizados que acontecem uma única vez por jogo.
bool HintSystem::Once(const std::string& id) {
    if (Learned(id)) return false;
    MarkLearned(id);
    return true;
}

// Fala na caixa de diálogo; também entra no histórico da pasta.
void HintSystem::Say(StageState& stage, const std::string& id, const std::string& context,
                     const std::vector<DialogueBox::Line>& lines) {
    stage.PlayDialogue("hint:" + id, context, lines);
}

// Anel de atenção num elemento da HUD por `seconds` (aparece mesmo com fala tocando).
void HintSystem::FocusHud(HudSlot slot, float seconds) {
    if (slot == HudSlot::None) return;
    hudFocus[SlotIndex(slot)] = std::max(hudFocus[SlotIndex(slot)], seconds);
}

bool HintSystem::IsIdle() const {
    return !silent && owner.empty() && gapTimer <= 0.0f;
}

bool HintSystem::ConsumeFuelLimitHit() {
    const bool hit = fuelLimitHit;
    fuelLimitHit = false;
    return hit;
}

// "[F]" com a tecla que o jogador configurou (KeyGlyphs troca pela arte).
std::string HintSystem::Key(GameAction action) {
    const char* name = SDL_GetKeyName(InputManager::GetInstance().GetBinding(action));
    return (name && name[0] != '\0') ? "[" + std::string(name) + "]" : std::string();
}

// Fala do irmãozão para o irmãozinho.
DialogueBox::Line HintSystem::Big(const std::string& text, DialogueBox::Emotion e, DialogueBox::Emotion listener) {
    return {DialogueBox::Speaker::BigBrother, DialogueBox::Speaker::LittleBrother, e, listener, text};
}

// Fala do irmãozinho para o irmãozão.
DialogueBox::Line HintSystem::Small(const std::string& text, DialogueBox::Emotion e, DialogueBox::Emotion listener) {
    return {DialogueBox::Speaker::LittleBrother, DialogueBox::Speaker::BigBrother, e, listener, text};
}

// A HUD avisa onde está neste frame (vale até o próximo Render).
void HintSystem::ReportHudRect(HudSlot slot, const SDL_FRect& rect) {
    if (slot == HudSlot::None) return;
    hudRect[SlotIndex(slot)] = rect;
    hudRectValid[SlotIndex(slot)] = true;
}

// True enquanto a dica da vez pede as teclas desse elemento (ex.: as da roda).
bool HintSystem::WantsHudKeys(HudSlot slot) const {
    if (owner.empty() || silent) return false;
    auto it = hints.find(owner);
    return it != hints.end() && it->second.showing && it->second.spec.hudKeys == slot;
}

// True com a dica da vez desenhada ao lado desse elemento (a HUD esconde a tecla dela).
bool HintSystem::HasGlyphAt(HudSlot slot) const {
    if (owner.empty() || silent) return false;
    auto it = hints.find(owner);
    return it != hints.end() && it->second.showing && it->second.spec.anchorHud == slot;
}

std::vector<std::string> HintSystem::GetLearnedList() const {
    std::vector<std::string> out(learned.begin(), learned.end());
    std::sort(out.begin(), out.end());
    return out;
}

void HintSystem::SetLearnedList(const std::vector<std::string>& ids) {
    learned.clear();
    learned.insert(ids.begin(), ids.end());
    owner.clear();
    for (auto& kv : hints) kv.second.showing = false;
}

// Fade das teclas (some no silêncio) e dos anéis da HUD.
void HintSystem::TickFades(float dt) {
    for (auto& kv : hints) {
        Runtime& rt = kv.second;
        const bool visible = rt.showing && !silent;
        Approach(rt.alpha, visible ? 1.0f : 0.0f, visible ? kHintFadeIn : kHintFadeOut, dt);
    }

    const Runtime* own = nullptr;
    if (!owner.empty()) {
        auto it = hints.find(owner);
        if (it != hints.end() && it->second.showing && !silent) own = &it->second;
    }
    for (int i = 1; i < SlotIndex(HudSlot::Count); ++i) {
        hudFocus[i] = std::max(0.0f, hudFocus[i] - dt);
        const bool on = !overlayOpen &&
                        (hudFocus[i] > 0.0f || (own && SlotIndex(own->spec.ring) == i));
        Approach(hudRingAlpha[i], on ? 1.0f : 0.0f, kRingFade, dt);
    }
}

// Tecla + ícone + rótulo: acima da âncora no mundo, ou à direita do elemento da HUD.
void HintSystem::RenderHint(SDL_Renderer* renderer, const Runtime& rt,
                            const std::function<Vec2(const Vec2&)>& worldToScreen) const {
    const HintSpec& s = rt.spec;
    if (rt.alpha <= 0.01f) return;
    if (s.keys.empty() && s.icon.empty() && s.label.empty()) return;
    std::vector<std::string> icons;
    if (!s.icon.empty())  icons.push_back(s.icon);
    if (!s.icon2.empty()) icons.push_back(s.icon2);

    const float u = Game::UiScale();
    const int hud = SlotIndex(s.anchorHud);
    if (s.anchorHud != HudSlot::None) {
        if (!hudRectValid[hud]) return;                  // elemento escondido neste frame
        const SDL_FRect& r = hudRect[hud];
        HorrorFx::DrawKeyHint(renderer, s.keys, icons, s.label, r.x + r.w + 18.0f * u, r.y + r.h * 0.5f,
                              HorrorFx::HintAnchor::LeftMiddle, rt.alpha, clock);
    } else if (s.hasAnchor) {
        const Vec2 screen = worldToScreen(s.anchor);
        HorrorFx::DrawKeyHint(renderer, s.keys, icons, s.label, screen.x, screen.y - kAnchorGapPx * u,
                              HorrorFx::HintAnchor::BottomCenter, rt.alpha, clock);
    }
}

// Todas as teclas ainda visíveis (a que sai faz fade) e os anéis da HUD.
void HintSystem::Render(SDL_Renderer* renderer, const std::function<Vec2(const Vec2&)>& worldToScreen) {
    if (!renderer) return;
    for (const auto& kv : hints) RenderHint(renderer, kv.second, worldToScreen);

    for (int i = 1; i < SlotIndex(HudSlot::Count); ++i) {
        if (hudRectValid[i] && hudRingAlpha[i] > 0.01f) {
            HorrorFx::DrawAttentionRing(renderer, hudRect[i], hudRingAlpha[i], clock);
        }
        hudRectValid[i] = false;                         // a HUD informa de novo no próximo frame
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Roteiro de cada andar (índice 0 = 1º andar)
// ═════════════════════════════════════════════════════════════════════════════
std::unique_ptr<FloorScript> MakeFloorScript(int levelIndex) {
    switch (levelIndex) {
    case 0:  return MakeFloor1Script();
    case 1:  return MakeFloor2Script();
    default: return nullptr;                             // 3º andar: próximo passo (lamparina)
    }
}
