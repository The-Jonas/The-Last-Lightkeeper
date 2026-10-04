#include "states/stage/StageState.h"
#include "core/Telemetry.h"
#include "core/Game.h"
#include "core/InputManager.h"
#include "core/Resources.h"
#include "engine/Camera.h"
#include "gameplay/Box.h"
#include "gameplay/Character.h"
#include "gameplay/ItemPickup.h"
#include "gameplay/Jornal.h"
#include "ui/Text.h"
#include "audio/GameSfx.h"
#include "states/stage/FirstLoadData.h"
#include "nlohmann/json.hpp"
#include "ui/KeyGlyphs.h"

#include <fstream>
#include <iostream>
#include <vector>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>

// Retângulo do papel na tela com o zoom e a rolagem atuais. Com zoom 1 e foco
// no meio, é exatamente o retângulo normal. Com zoom, o ponto (journalFocusX,
// journalFocusY) do papel fica no centro de onde o papel estaria.
SDL_FRect StageState::GetJournalZoomedRect() const {
    const SDL_FRect& base = journalTargetScreenRect;
    const float w  = base.w * journalZoomCurrent;
    const float h  = base.h * journalZoomCurrent;
    const float cx = base.x + base.w * 0.5f;
    const float cy = base.y + base.h * 0.5f;
    return SDL_FRect{ cx - journalFocusX * w, cy - journalFocusY * h, w, h };
}

// Mantém a rolagem dentro do papel. Com zoom, a borda do documento nunca entra
// na tela. Se num eixo o papel ainda cabe, esse eixo fica centralizado.
void StageState::ClampJournalFocus(int winW, int winH) {
    const SDL_FRect& base = journalTargetScreenRect;
    const float w  = base.w * journalZoomCurrent;
    const float h  = base.h * journalZoomCurrent;
    const float cx = base.x + base.w * 0.5f;
    const float cy = base.y + base.h * 0.5f;

    // Limita o foco de UM eixo: `center` é onde fica o meio do papel na tela,
    // `size` o tamanho do papel com zoom, `screen` o tamanho da tela.
    auto clampAxis = [](float focus, float center, float size, float screen) {
        if (size <= screen) return 0.5f;
        const float lo = center / size;                    
        const float hi = 1.0f - (screen - center) / size;  
        return std::max(lo, std::min(hi, focus));
    };
    journalFocusX = clampAxis(journalFocusX, cx, w, static_cast<float>(winW));
    journalFocusY = clampAxis(journalFocusY, cy, h, static_cast<float>(winH));
}

namespace {

float SmoothStep(float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    return t * t * (3.0f - 2.0f * t);
}

// Documento é "de papel" (ganha o farfalhar de folha) exceto os itens de olhar
// que claramente NÃO são papel: fotos, telefones, caixas, rádio. Classificamos
// pelo nome do arquivo da imagem aberta.
bool IsPaperDocumentImage(const std::string& imagePath) {
    std::string p = imagePath;
    for (char& c : p) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    static const char* kNonPaper[] = {"foto", "fotograf", "telefon", "caixa", "radio"};
    for (const char* k : kNonPaper) {
        if (p.find(k) != std::string::npos) return false;
    }
    return true;
}

// Variações do farfalhar de folha tocadas ao abrir um documento de papel.
const char* PickPaperRustleSfx() {
    static const char* kPaperSfx[] = {
        "Recursos/audio/SFX/INTERACTABLES/folha_1.mp3",
        "Recursos/audio/SFX/INTERACTABLES/folha_2.mp3",
        "Recursos/audio/SFX/INTERACTABLES/folha_3.mp3",
    };
    return kPaperSfx[std::rand() % 3];
}

SDL_FRect FitTextureInWindow(int winW, int winH, int texW, int texH, float marginFrac = 0.88f) {
    const float maxW = static_cast<float>(winW) * marginFrac;
    const float maxH = static_cast<float>(winH) * marginFrac;
    if (texW <= 0 || texH <= 0) {
        return {
            static_cast<float>(winW) * 0.06f,
            static_cast<float>(winH) * 0.06f,
            static_cast<float>(winW) * 0.88f,
            static_cast<float>(winH) * 0.88f,
        };
    }
    const float aspect = static_cast<float>(texW) / static_cast<float>(texH);
    float w = maxW;
    float h = w / aspect;
    if (h > maxH) {
        h = maxH;
        w = h * aspect;
    }
    return {
        (static_cast<float>(winW) - w) * 0.5f,
        (static_cast<float>(winH) - h) * 0.5f,
        w,
        h,
    };
}

SDL_FRect LerpRect(const SDL_FRect& a, const SDL_FRect& b, float t) {
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.w + (b.w - a.w) * t,
        a.h + (b.h - a.h) * t,
    };
}

} // namespace

bool StageState::IsJornalCloserThanItemAndBox(Jornal* jornal, ItemPickup* item, Box* box) const {
    if (!jornal) {
        return false;
    }

    float closestDist = GetInteractableDistance(jornal->GetAssociated());

    if (item && !item->GetAssociated().IsDead()) {
        const float dItem = GetInteractableDistance(item->GetAssociated());
        if (dItem < closestDist) {
            return false;
        }
    }

    if (box && box->IsPushable()) {
        const float dBox = GetInteractableDistance(box->GetAssociated());
        if (dBox < closestDist) {
            return false;
        }
    }

    return true;
}

Jornal* StageState::FindClosestReachableJornal() const {
    if (!bigCharacter || !Character::player || controlledCharacter != bigCharacter) {
        return nullptr;
    }

    Jornal* closest = nullptr;
    float closestDist = 1e30f;
    const Vec2 playerCenter = bigCharacter->GetCenter();

    for (Jornal* jornal : jornals) {
        if (!jornal || jornal->GetAssociated().IsDead()) {
            continue;
        }

        const int height = jornal->GetHeightLevel();
        const SDL_Rect reachBox = Character::player->GetInteractionRect(height);
        const GameObject& obj = jornal->GetAssociated();
        const SDL_Rect objRect = {
            static_cast<int>(obj.box.x),
            static_cast<int>(obj.box.y),
            static_cast<int>(obj.box.w),
            static_cast<int>(obj.box.h),
        };

        if (!SDL_HasIntersection(&reachBox, &objRect)) {
            continue;
        }

        const float d = playerCenter.Distance(jornal->GetCenter());
        if (d < closestDist) {
            closestDist = d;
            closest = jornal;
        }
    }

    return closest;
}

void StageState::BeginJournalView(const std::string& imagePath, const std::string& soundPath,
                                  float zoomFactor, bool zoomable, const SDL_FRect& sourceRect) {
    if (bigCharacter)   bigCharacter->ForceStop();
    if (smallCharacter) smallCharacter->ForceStop();

    // Som ao abrir: o do mapa tem prioridade; papel sem som ganha o farfalhar.
    std::string openSound = soundPath;
    if (openSound.empty() && IsPaperDocumentImage(imagePath)) {
        openSound = PickPaperRustleSfx();
    }
    if (!openSound.empty()) {
        jornalInteractSound.Open(openSound);
        jornalInteractSound.Play();
    }

    journalViewImagePath    = imagePath;
    journalSourceScreenRect = sourceRect;

    journalAnimTimer     = 0.0f;
    journalCloseTimer    = 0.0f;
    journalViewerClosing = false;
    journalViewerOpen    = true;
    journalViewZoomable  = zoomable;
    journalZoomLevel     = 0;
    journalZoomCurrent   = 1.0f;
    journalFocusX = journalFocusY = 0.5f;

    telemetryJournalOpenedAt = static_cast<float>(Telemetry::Now());
    Telemetry::Event("journal_open", Telemetry::Fields()
        .Int("level", currentLevelIndex)
        .Str("image", journalViewImagePath));

    const int winW = Game::GetInstance().GetWindowsWidth();
    const int winH = Game::GetInstance().GetWindowsHeight();
    int texW = 1, texH = 1;
    if (auto tex = Resources::GetImage(journalViewImagePath)) {
        SDL_QueryTexture(tex.get(), nullptr, nullptr, &texW, &texH);
    }
    journalTargetScreenRect = FitTextureInWindow(winW, winH, texW, texH);

    if (zoomFactor != 1.0f) {
        const float cx = journalTargetScreenRect.x + journalTargetScreenRect.w * 0.5f;
        const float cy = journalTargetScreenRect.y + journalTargetScreenRect.h * 0.5f;
        journalTargetScreenRect.w *= zoomFactor;
        journalTargetScreenRect.h *= zoomFactor;
        journalTargetScreenRect.x  = cx - journalTargetScreenRect.w * 0.5f;
        journalTargetScreenRect.y  = cy - journalTargetScreenRect.h * 0.5f;
    }
}

// Abre o visualizador de documento: som, animação saindo de sourceRect (na tela),
// zoom inicial e telemetria. Parte comum a papel do cenário e documento da pasta.
void StageState::OpenJournalViewer(Jornal* jornal) {
    if (!jornal) return;

    const GameObject& obj = jornal->GetAssociated();
    const float zoom = Camera::GetZoom();
    const SDL_FRect from{
        (obj.box.x - Camera::pos.x) * zoom,
        (obj.box.y - Camera::pos.y) * zoom,
        obj.box.w * zoom,
        obj.box.h * zoom,
    };
    BeginJournalView(jornal->GetImagePath(), jornal->GetSoundPath(),
                     jornal->GetZoomFactor(), jornal->IsZoomable(), from);

    if (jornal->HasPendingDialogue()) {
        const std::string& ctx = jornal->GetDialogueContext(); 
        PlayDialogue("doc:" + jornal->GetImagePath(),
                     ctx.empty() ? "Ao ler um documento" : ctx,
                     jornal->GetDialogueLines(), jornal->GetImagePath());
        jornal->MarkDialogueFired();
    }
}

void StageState::TryOpenJournalOnKeyPress() {
    InputManager& input = InputManager::GetInstance();
    if (!input.ActionPress(GameAction::Interact) || !reachableJornal) {
        return;
    }

    if (reachableCandle && IsCandleClosestForInteraction(reachableCandle)) {
        return;
    }

    ItemPickup* item = FindClosestReachableItem();
    if (!IsJornalCloserThanItemAndBox(reachableJornal, item, reachablePushBox)) {
        return;
    }

    if (reachableJornal->IsCollectible()) {
        CollectJornal(reachableJornal);
        return;
    }
    OpenJournalViewer(reachableJornal);
}

void StageState::CollectJornal(Jornal* jornal) {
    if (!jornal) return;

    CollectedDocument doc;
    doc.imagePath  = jornal->GetImagePath();
    doc.title      = jornal->GetDocTitle();
    doc.soundPath  = jornal->GetSoundPath();
    doc.order      = jornal->GetDocOrder(); 
    doc.zoomFactor = jornal->GetZoomFactor();
    doc.zoomable   = jornal->IsZoomable();
    doc.dialogueContext = jornal->GetDialogueContext();
    doc.level      = currentLevelIndex;
    if (jornal->HasPendingDialogue()) {
        doc.dialogueLines = jornal->GetDialogueLines();
    }

    // O mesmo documento não entra duas vezes (ex.: repetido em dois mapas).
    const bool already = std::any_of(collectedDocuments.begin(), collectedDocuments.end(),
        [&](const CollectedDocument& d) { return d.imagePath == doc.imagePath; });
    if (!already) {
        // Inserção ordenada por doc_order; empate mantém a ordem de coleta.
        auto it = std::upper_bound(collectedDocuments.begin(), collectedDocuments.end(), doc,
            [](const CollectedDocument& a, const CollectedDocument& b) {
                return a.level != b.level ? a.level < b.level : a.order < b.order;
            });
        collectedDocuments.insert(it, std::move(doc));
    }

    // Feedback sonoro: o mesmo farfalhar de abrir um papel.
    std::string sfx = jornal->GetSoundPath();
    if (sfx.empty() && IsPaperDocumentImage(jornal->GetImagePath())) {
        sfx = PickPaperRustleSfx();
    }
    if (!sfx.empty()) {
        jornalInteractSound.Open(sfx);
        jornalInteractSound.Play();
    }

    Telemetry::Event("document_collected", Telemetry::Fields()
        .Int("level", currentLevelIndex)
        .Str("image", jornal->GetImagePath()));

    // A fala do 1º documento sai do tutorial (CommonHints) ao notar a pasta crescer.
    jornals.erase(std::remove(jornals.begin(), jornals.end(), jornal), jornals.end());
    if (reachableJornal == jornal) reachableJornal = nullptr;
    jornal->GetAssociated().RequestDelete();
}

// CASO QUEIRAMOS FAZER A OPÇÃO DE VOLTAR ANDARES NO FUTURO
// Tira do mapa os papéis colecionáveis que já estão na pasta — chamada depois
// de montar um andar (voltar a um andar já visitado, carregar save).
void StageState::RemoveCollectedJornalsFromWorld() {
    for (auto it = jornals.begin(); it != jornals.end();) {
        Jornal* j = *it;
        const bool collected = j && j->IsCollectible() &&
            std::any_of(collectedDocuments.begin(), collectedDocuments.end(),
                [&](const CollectedDocument& d) { return d.imagePath == j->GetImagePath(); });
        if (collected) {
            j->GetAssociated().RequestDelete();
            it = jornals.erase(it);
        } else {
            ++it;
        }
    }
    reachableJornal = nullptr;
}

void StageState::UpdateJournalViewer(float dt) {
    InputManager& input = InputManager::GetInstance();

    if (bigCharacter)   bigCharacter->ReleaseInteract();
    if (smallCharacter) smallCharacter->ReleaseInteract();

    if (journalViewerClosing) {
        journalCloseTimer += dt;
        if (journalCloseTimer >= kJournalCloseDuration) {
            journalViewerOpen = false;
            journalViewerClosing = false;
            journalViewImagePath.clear();
        }
        return;
    }

    journalAnimTimer += dt;

    // ── Zoom (F) e rolagem (WASD/setas) — só documento marcado no Tiled ──────
    if (journalViewZoomable) {
        const int winW = Game::GetInstance().GetWindowsWidth();
        const int winH = Game::GetInstance().GetWindowsHeight();

        if (input.ActionPress(GameAction::UseItem)) {
            journalZoomLevel = (journalZoomLevel + 1) % 3;   // normal → perto → bem perto → normal
            if (journalZoomLevel == 0) {
                journalFocusX = journalFocusY = 0.5f;        // saiu do zoom: volta ao meio
            }
        }

        const float target = kJournalZoomLevels[journalZoomLevel];
        journalZoomCurrent += (target - journalZoomCurrent) * std::min(1.0f, kJournalZoomSmoothing * dt);

        if (journalZoomLevel > 0) {
            float dx = 0.0f, dy = 0.0f;
            if (input.ActionDown(GameAction::MoveLeft)  || input.IsKeyDown(SDLK_LEFT))  dx -= 1.0f;
            if (input.ActionDown(GameAction::MoveRight) || input.IsKeyDown(SDLK_RIGHT)) dx += 1.0f;
            if (input.ActionDown(GameAction::MoveUp)    || input.IsKeyDown(SDLK_UP))    dy -= 1.0f;
            if (input.ActionDown(GameAction::MoveDown)  || input.IsKeyDown(SDLK_DOWN))  dy += 1.0f;

            // Velocidade fixa na TELA: com mais zoom o papel é maior, então o
            // passo em fração do papel diminui (sensação igual nos dois níveis).
            const float w = journalTargetScreenRect.w * journalZoomCurrent;
            const float h = journalTargetScreenRect.h * journalZoomCurrent;
            if (w > 1.0f) journalFocusX += dx * kJournalPanSpeedPx * dt / w;
            if (h > 1.0f) journalFocusY += dy * kJournalPanSpeedPx * dt / h;
        }
        ClampJournalFocus(winW, winH);
    }

    if (input.ActionPress(GameAction::Interact) || input.KeyPress(SDLK_ESCAPE)) {
        Telemetry::Event("journal_close", Telemetry::Fields()
            .Str("image", journalViewImagePath)
            .Num("readSeconds", Telemetry::Now() - telemetryJournalOpenedAt));
        journalViewerClosing = true;
        journalCloseTimer = 0.0f;
        jornalInteractSound.FadeOut(600);           // fade suave (não corta seco) ao fechar o documento
    }
}

void StageState::RenderJournalViewer(SDL_Renderer* renderer) {
    if (!renderer || (!journalViewerOpen && !journalViewerClosing) || journalViewImagePath.empty()) {
        return;
    }

    const int winW = Game::GetInstance().GetWindowsWidth();
    const int winH = Game::GetInstance().GetWindowsHeight();

    float openT = SmoothStep(journalAnimTimer / kJournalOpenDuration);
    float alphaMul = 1.0f;
    if (journalViewerClosing) {
        const float closeT = journalCloseTimer / kJournalCloseDuration;
        alphaMul = 1.0f - std::max(0.0f, std::min(1.0f, closeT));
        openT = 1.0f;
    }

    const Uint8 backdropAlpha = static_cast<Uint8>(180.0f * openT * alphaMul);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, backdropAlpha);
    SDL_FRect full{0.0f, 0.0f, static_cast<float>(winW), static_cast<float>(winH)};
    SDL_RenderFillRectF(renderer, &full);

    auto tex = Resources::GetImage(journalViewImagePath);
    if (tex) {
        const SDL_FRect dst = LerpRect(journalSourceScreenRect, GetJournalZoomedRect(), openT);
        SDL_SetTextureAlphaMod(tex.get(), static_cast<Uint8>(255.0f * alphaMul));
        SDL_SetTextureColorMod(tex.get(), 255, 255, 255);
        SDL_SetTextureBlendMode(tex.get(), SDL_BLENDMODE_BLEND);
        SDL_RenderCopyF(renderer, tex.get(), nullptr, &dst);
    }

    if (openT > 0.85f && alphaMul > 0.2f) {
        auto hintFont = Resources::GetFont("Recursos/font/times.ttf", 16);
        if (hintFont) {
            std::string keyName = SDL_GetKeyName(InputManager::GetInstance().GetBinding(GameAction::Interact));
            if (keyName.empty()) {
                keyName = "E";
            }
            std::string hintStr = keyName + " / ESC — fechar";
            if (journalViewZoomable) {
                std::string zoomKey = SDL_GetKeyName(InputManager::GetInstance().GetBinding(GameAction::UseItem));
                if (zoomKey.empty()) zoomKey = "F";
                hintStr += "    " + zoomKey + " — zoom";
                if (journalZoomLevel > 0) hintStr += "    WASD / setas — mover";
            }
            SDL_Color hc{230, 225, 200, static_cast<Uint8>(240.0f * alphaMul)};
            SDL_Surface* surf = TTF_RenderUTF8_Blended(hintFont.get(), hintStr.c_str(), hc);
            if (surf) {
                SDL_Texture* hintTex = SDL_CreateTextureFromSurface(renderer, surf);
                const int tw = surf->w;
                const int th = surf->h;
                SDL_FreeSurface(surf);
                if (hintTex) {
                    SDL_FRect dst = {
                        (static_cast<float>(winW) - static_cast<float>(tw)) * 0.5f,
                        static_cast<float>(winH) - static_cast<float>(th) - 24.0f,
                        static_cast<float>(tw),
                        static_cast<float>(th),
                    };
                    SDL_RenderCopyF(renderer, hintTex, nullptr, &dst);
                    SDL_DestroyTexture(hintTex);
                }
            }
        }
    }
}

void StageState::PlayDialogue(const std::string& key, const std::string& context,
                              const std::vector<DialogueBox::Line>& lines,
                              const std::string& docImagePath) {
    if (lines.empty()) return;

    for (const DialogueBox::Line& l : lines) {
        dialogueBox.Queue(l.speaker, l.listener, l.emotion, l.listenerEmotion, l.text);
    }

    const std::string fullKey = std::to_string(currentLevelIndex) + ":" + key;
    for (const DialogueLogEntry& e : dialogueLog) {
        if (e.key == fullKey) return;   // já registrada (gatilho repetível, replay etc.)
    }

    DialogueLogEntry entry;
    entry.key          = fullKey;
    entry.context      = context.empty() ? "Conversa" : context;
    entry.level        = currentLevelIndex;
    entry.docImagePath = docImagePath;
    entry.lines        = lines;
    dialogueLog.push_back(std::move(entry));

    std::cerr << "[dialogos] +" << fullKey << " (" << dialogueLog.back().context
              << ") total=" << dialogueLog.size() << std::endl;
}

// ========================================================= ##
//  Desenha a pasta: fundo escurecido, carrossel de molduras ##
// ========================================================= ##
namespace {

// Encaixa uma textura dentro de um retângulo mantendo a proporção, com margem.
SDL_FRect FitInsideRect(const SDL_FRect& box, int texW, int texH, float pad) {
    const float bw = box.w - 2.0f * pad;
    const float bh = box.h - 2.0f * pad;
    if (texW <= 0 || texH <= 0 || bw <= 0.0f || bh <= 0.0f) return box;
    const float aspect = static_cast<float>(texW) / static_cast<float>(texH);
    float w = bw;
    float h = w / aspect;
    if (h > bh) { h = bh; w = h * aspect; }
    return { box.x + (box.w - w) * 0.5f, box.y + (box.h - h) * 0.5f, w, h };
}

// Desenha um texto centrado em cx com o topo em y (alpha vem de color.a).
void DrawFolderText(SDL_Renderer* r, TTF_Font* font, const std::string& text,
                    float cx, float y, SDL_Color color) {
    if (!font || text.empty()) return;
    SDL_Surface* s = TTF_RenderUTF8_Blended(font, text.c_str(), SDL_Color{color.r, color.g, color.b, 255});
    if (!s) return;
    if (SDL_Texture* t = SDL_CreateTextureFromSurface(r, s)) {
        SDL_SetTextureAlphaMod(t, color.a);
        const SDL_FRect d{ cx - s->w * 0.5f, y, static_cast<float>(s->w), static_cast<float>(s->h) };
        SDL_RenderCopyF(r, t, nullptr, &d);
        SDL_DestroyTexture(t);
    }
    SDL_FreeSurface(s);
}

// Desenha um texto com o canto superior-esquerdo em (x, y). Devolve a largura.
float DrawFolderTextLeft(SDL_Renderer* r, TTF_Font* font, const std::string& text,
                         float x, float y, SDL_Color color) {
    if (!font || text.empty()) return 0.0f;
    SDL_Surface* s = TTF_RenderUTF8_Blended(font, text.c_str(), SDL_Color{color.r, color.g, color.b, 255});
    if (!s) return 0.0f;
    const float w = static_cast<float>(s->w);
    if (SDL_Texture* t = SDL_CreateTextureFromSurface(r, s)) {
        SDL_SetTextureAlphaMod(t, color.a);
        const SDL_FRect d{x, y, w, static_cast<float>(s->h)};
        SDL_RenderCopyF(r, t, nullptr, &d);
        SDL_DestroyTexture(t);
    }
    SDL_FreeSurface(s);
    return w;
}

// Texto com quebra de linha automática em wrapW px. Quem chama destrói a textura.
SDL_Texture* MakeWrappedText(SDL_Renderer* r, TTF_Font* font, const std::string& text,
                             SDL_Color color, int wrapW, int& outW, int& outH) {
    outW = outH = 0;
    if (!font || text.empty()) return nullptr;
    SDL_Surface* s = TTF_RenderUTF8_Blended_Wrapped(font, text.c_str(), color, static_cast<Uint32>(std::max(1, wrapW)));
    if (!s) return nullptr;
    outW = s->w;
    outH = s->h;
    SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
    SDL_FreeSurface(s);
    return t;
}

// Triângulo (ponta para cima ou para baixo) com borda suave de ~1,5 px, feito
// com SDL_RenderGeometry: bordas retas, sem o serrilhado do desenho por linhas.
void DrawFolderArrow(SDL_Renderer* r, float cx, float cy, float halfW, float h, bool up, SDL_Color c) {
    const float tipY  = up ? cy - h * 0.5f : cy + h * 0.5f;
    const float baseY = up ? cy + h * 0.5f : cy - h * 0.5f;
    const SDL_FPoint p[3] = { {cx, tipY}, {cx - halfW, baseY}, {cx + halfW, baseY} };

    // Contorno externo: cada vértice empurrado para fora a partir do centro do
    // triângulo. Com alpha 0 lá fora, a cor se desfaz aos poucos na borda.
    const float feather = 1.5f;
    const SDL_FPoint center{cx, (tipY + 2.0f * baseY) / 3.0f};
    SDL_Vertex v[6];
    for (int i = 0; i < 3; ++i) {
        const float dx = p[i].x - center.x;
        const float dy = p[i].y - center.y;
        const float len = std::max(0.001f, std::sqrt(dx * dx + dy * dy));
        const float push = feather * 2.0f;   
        v[i]     = SDL_Vertex{ p[i], c, {0.0f, 0.0f} };
        v[i + 3] = SDL_Vertex{ {p[i].x + dx / len * push, p[i].y + dy / len * push},
                               SDL_Color{c.r, c.g, c.b, 0}, {0.0f, 0.0f} };
    }

    // Miolo + uma faixa (2 triângulos) em cada um dos 3 lados.
    const int idx[21] = {
        0, 1, 2,
        0, 1, 4,   0, 4, 3,
        1, 2, 5,   1, 5, 4,
        2, 0, 3,   2, 3, 5,
    };
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(r, nullptr, v, 6, idx, 21);
}

// Faixa que escurece em degradê: forte na borda do painel, some para dentro.
void DrawEdgeFade(SDL_Renderer* r, const SDL_FRect& band, bool strongAtTop, Uint8 maxAlpha) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    const int steps = std::max(1, static_cast<int>(band.h));
    for (int i = 0; i < steps; ++i) {
        const float t = static_cast<float>(i) / steps;         
        const float k = strongAtTop ? (1.0f - t) : t;
        SDL_SetRenderDrawColor(r, 8, 8, 10, static_cast<Uint8>(maxAlpha * k * k));
        const SDL_FRect row{band.x, band.y + i, band.w, 1.0f};
        SDL_RenderFillRectF(r, &row);
    }
}

} // namespace

// Varre os mapas do 1º andar até o atual e guarda (andar, ordem) de cada
// JornalSpawn colecionável — total do contador e posições das páginas vazias.
void StageState::ScanKnownDocumentOrders() {
    knownDocumentKeysLevel = currentLevelIndex;
    knownDocumentKeys.clear();

    const StageFirstLoadData cfg = LoadStageFirstLoadData();
    const int last = std::min(currentLevelIndex, GetLevelCount(cfg) - 1);
    for (int lv = 0; lv <= last; ++lv) {
        const std::string& mapPath = GetLevelDef(cfg, lv).mapPath;
        std::ifstream f(mapPath);
        if (!f.is_open()) {
            std::cerr << "[pasta] mapa nao encontrado: " << mapPath << std::endl;
            continue;
        }
        try {
            nlohmann::json j;
            f >> j;
            for (const auto& layer : j.value("layers", nlohmann::json::array())) {
                if (layer.value("name", "") != "Entidades") continue;
                for (const auto& obj : layer.value("objects", nlohmann::json::array())) {
                    const std::string type = obj.value("class", obj.value("type", ""));
                    if (type != "JornalSpawn" || !obj.contains("properties")) continue;

                    bool collectible = false;
                    int  order = 1000 + obj.value("id", -1);   // mesma regra do SpawnFactory
                    for (const auto& p : obj["properties"]) {
                        const std::string name = p.value("name", "");
                        if (!p.contains("value")) continue;
                        if (name == "collectible" && p["value"].is_boolean()) collectible = p["value"].get<bool>();
                        if (name == "doc_order" && p["value"].is_number_integer()) order = p["value"].get<int>();
                    }
                    if (collectible) knownDocumentKeys.push_back({lv, order});
                }
            }
        } catch (const std::exception& ex) {
            std::cerr << "[pasta] falha ao varrer " << mapPath << ": " << ex.what() << std::endl;
        }
    }
    std::sort(knownDocumentKeys.begin(), knownDocumentKeys.end());
    knownDocumentKeys.erase(std::unique(knownDocumentKeys.begin(), knownDocumentKeys.end()),
                            knownDocumentKeys.end());

    std::cerr << "[pasta] " << knownDocumentKeys.size() << " colecionavel(is) ate o andar "
              << (currentLevelIndex + 1) << std::endl;
}

// Monta as páginas em ordem (andar, ordem): uma por documento coletado e uma
// vazia ("?") para cada colecionável conhecido que ficou para trás.
void StageState::RebuildDocumentFolderFrames() {
    documentFolderFrames.clear();

    std::vector<std::pair<int, int>> keys = knownDocumentKeys;
    for (const CollectedDocument& d : collectedDocuments) keys.push_back({d.level, d.order});
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    documentFolderTotal = static_cast<int>(keys.size());

    for (const auto& k : keys) {
        bool found = false;
        for (int i = 0; i < static_cast<int>(collectedDocuments.size()); ++i) {
            const CollectedDocument& d = collectedDocuments[i];
            if (d.level == k.first && d.order == k.second) {
                documentFolderFrames.push_back({k.first, k.second, i});
                found = true;
            }
        }
        if (!found) documentFolderFrames.push_back({k.first, k.second, -1});
    }
}

// Retângulo na tela da moldura `index`, considerando a rolagem suavizada:
// a selecionada fica no centro em tamanho cheio, as vizinhas 25% menores.
SDL_FRect StageState::GetDocumentFolderCardRect(int index) const {
    const float winW = static_cast<float>(Game::GetInstance().GetWindowsWidth());
    const float winH = static_cast<float>(Game::GetInstance().GetWindowsHeight());

    const float cardH   = winH * 0.52f;
    const float cardW   = cardH * 0.75f;          // moldura retrato padronizada (3:4)
    const float spacing = cardW * 1.18f;

    const float offset = static_cast<float>(index) - documentFolderScroll;
    const float scale  = 1.0f - 0.25f * std::min(1.0f, std::fabs(offset));   // vizinhos menores
    const float cx = winW * 0.5f + offset * spacing;
    const float cy = winH * 0.46f;
    return { cx - cardW * scale * 0.5f, cy - cardH * scale * 0.5f, cardW * scale, cardH * scale };
}



// Abre a pasta (Tab): varre os mapas na 1ª vez, monta as molduras, pausa o
// mundo e posiciona no primeiro documento não lido.
void StageState::OpenDocumentFolder() {
     if (knownDocumentKeysLevel != currentLevelIndex) ScanKnownDocumentOrders();
    RebuildDocumentFolderFrames();

    if (bigCharacter)   bigCharacter->ForceStop();
    if (smallCharacter) smallCharacter->ForceStop();
    GameSfx::StopAllGameplayAudio();   // mesmo motivo do menu de pausa: o mundo para

    // Começa no documento "novo" mais antigo; sem novos, mantém a última posição.
    const int n = static_cast<int>(documentFolderFrames.size());
    for (int i = 0; i < n; ++i) {
        const int di = documentFolderFrames[i].docIndex;
        if (di >= 0 && collectedDocuments[di].unread) { documentFolderSelection = i; break; }
    }
    documentFolderSelection = (n > 0) ? std::max(0, std::min(documentFolderSelection, n - 1)) : 0;
    documentFolderScroll    = static_cast<float>(documentFolderSelection);
    
    dialogueLogSelection = std::max(0, std::min(dialogueLogSelection,
                                                 static_cast<int>(dialogueLog.size()) - 1));
    documentFolderOpen = true;
    documentFolderAnim = 0.0f;
    Telemetry::Event("document_folder_open", Telemetry::Fields()
        .Int("level", currentLevelIndex)
        .Int("collected", static_cast<int>(collectedDocuments.size())));
}



// Fecha a pasta (interrompendo um replay em andamento) e devolve o controle ao jogo.
void StageState::CloseDocumentFolder() {
    if (dialogueReplayActive) {   
        dialogueBox.Stop();
        dialogueReplayActive = false;
    }
    documentFolderOpen = false;
}



// Abre um documento da pasta no visualizador normal (zoom etc.), crescendo a
// partir da moldura; tira o "novo" e toca o diálogo pendente.
void StageState::OpenCollectedDocument(CollectedDocument& doc, const SDL_FRect& fromRect) {
    BeginJournalView(doc.imagePath, doc.soundPath, doc.zoomFactor, doc.zoomable, fromRect);
    doc.unread = false;

    // Diálogo do Tiled: toca e registra no log só na primeira leitura.
    // Depois disso, ouvir de novo é pelo replay do log.
    if (!doc.dialogueLines.empty()) {
        PlayDialogue("doc:" + doc.imagePath,
                     doc.dialogueContext.empty() ? "Ao ler: " + doc.title : doc.dialogueContext,
                     doc.dialogueLines, doc.imagePath);
        doc.dialogueLines.clear();
    }
}



// Entrada da pasta: Tab/ESC fecham, esquerda/direita navegam, E/Enter abrem o
// documento selecionado (molduras vazias são ignoradas). Suaviza a rolagem.
void StageState::UpdateDocumentFolder(float dt) {
    InputManager& input = InputManager::GetInstance();
    if (bigCharacter)   bigCharacter->ReleaseInteract();
    if (smallCharacter) smallCharacter->ReleaseInteract();

    documentFolderAnim = std::min(1.0f, documentFolderAnim + dt / kDocumentFolderFadeTime);

    // Replay tocando: o ESC só interrompe a conversa, sem fechar a pasta.
    if (dialogueReplayActive && !dialogueBox.isActive()) {
        dialogueReplayActive = false;   // terminou sozinho
    }
    if (dialogueReplayActive && input.KeyPress(ESCAPE_KEY)) {
        dialogueBox.Stop();
        dialogueReplayActive = false;
        return;
    }

    if (input.KeyPress(TAB_KEY) || input.KeyPress(ESCAPE_KEY)) {
        CloseDocumentFolder();
        return;
    }

    // [Q] alterna entre as abas Documentos e Diálogos.
    if (input.ActionPress(GameAction::ToggleMode)) {
        documentFolderTab = 1 - documentFolderTab;
        if (documentFolderTab == 1) {
            dialogueLogSelection  = std::max(0, static_cast<int>(dialogueLog.size()) - 1);   // a mais recente
            dialogueLogFocusPanel = false;
            dialogueLogScroll = dialogueLogScrollTarget = 0.0f;
        }
    }
    if (documentFolderTab == 1) {
        UpdateDialogueLogTab(dt);
        return;
    }

    const int n = static_cast<int>(documentFolderFrames.size());
    if (n > 0 && !collectedDocuments.empty()) {
        const bool left  = input.ActionPress(GameAction::MoveLeft)  || input.ActionPress(GameAction::CyclePrev) || input.KeyPress(SDLK_LEFT);
        const bool right = input.ActionPress(GameAction::MoveRight) || input.ActionPress(GameAction::CycleNext) || input.KeyPress(SDLK_RIGHT);
        if (left)  documentFolderSelection = std::max(0, documentFolderSelection - 1);
        if (right) documentFolderSelection = std::min(n - 1, documentFolderSelection + 1);

        if (input.ActionPress(GameAction::Interact) || input.KeyPress(SDLK_RETURN)) {
            const FolderFrame& f = documentFolderFrames[documentFolderSelection];
            if (f.docIndex >= 0) {
                OpenCollectedDocument(collectedDocuments[f.docIndex],
                                      GetDocumentFolderCardRect(documentFolderSelection));
            }
        }

        // [F] Ouvir de novo a conversa deste documento.
        if (input.ActionPress(GameAction::UseItem) && !dialogueBox.isActive()) {
            const FolderFrame& f = documentFolderFrames[documentFolderSelection];
            if (f.docIndex >= 0) {
                if (const DialogueLogEntry* e = FindDialogueForDocument(collectedDocuments[f.docIndex].imagePath)) {
                    for (const DialogueBox::Line& l : e->lines) {
                        dialogueBox.Queue(l.speaker, l.listener, l.emotion, l.listenerEmotion, l.text);
                    }
                    dialogueReplayActive = true;
                }
            }
        }
    }

    const float target = static_cast<float>(documentFolderSelection);
    documentFolderScroll += (target - documentFolderScroll) * std::min(1.0f, kDocumentFolderScrollSmoothing * dt);
}

const StageState::DialogueLogEntry* StageState::FindDialogueForDocument(const std::string& imagePath) const {
    for (const DialogueLogEntry& e : dialogueLog) {
        if (!e.docImagePath.empty() && e.docImagePath == imagePath) return &e;
    }
    return nullptr;
}
// Conversa já registrada que veio deste documento (nullptr se ele não tem ou
// se ainda não foi ouvida).

// Desenha a pasta: páginas flutuando sem moldura (sombra + balanço), vizinhas
// menores e escurecidas, "?" para as que ficaram para trás, selo "novo",
// título, contador "coletados / total até este andar" e dicas.
void StageState::RenderDocumentFolder(SDL_Renderer* renderer) {
    if (!renderer || !documentFolderOpen) return;

    const int   winW = Game::GetInstance().GetWindowsWidth();
    const int   winH = Game::GetInstance().GetWindowsHeight();
    const float a    = SmoothStep(documentFolderAnim);
    const Uint8 ta   = static_cast<Uint8>(255.0f * a);
    const float time = SDL_GetTicks() / 1000.0f;

    DrawSceneBlur(renderer, winW, winH, a);   
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, static_cast<Uint8>(150.0f * a));
    const SDL_FRect full{0.0f, 0.0f, static_cast<float>(winW), static_cast<float>(winH)};
    SDL_RenderFillRectF(renderer, &full);

    RenderFolderTabs(renderer, a);
    if (documentFolderTab == 1) {
        RenderDialogueLogTab(renderer, a);
        return;
    }

    auto titleFont = Resources::GetFont("Recursos/font/times.ttf", 44);
    auto bigFont   = Resources::GetFont("Recursos/font/times.ttf", 120);
    auto smallFont = Resources::GetFont("Recursos/font/times.ttf", 28);
    auto badgeFont = Resources::GetFont("Recursos/font/times.ttf", 24);
    const SDL_Color textCol{230, 225, 200, ta};
    const SDL_Color dimCol {200, 195, 175, ta};

    if (collectedDocuments.empty()) {
        DrawFolderText(renderer, titleFont.get(), "Não há nada aqui", winW * 0.5f, winH * 0.5f - 22.0f, textCol);
        DrawFolderText(renderer, smallFont.get(), "Tab / ESC — fechar", winW * 0.5f, winH * 0.90f, dimCol);
        return;
    }

    const int n = static_cast<int>(documentFolderFrames.size());

    std::vector<int> drawList;
    for (int i = 0; i < n; ++i) {
        if (std::fabs(i - documentFolderScroll) < 3.5f) drawList.push_back(i);
    }
    std::sort(drawList.begin(), drawList.end(), [&](int x, int y) {
        return std::fabs(x - documentFolderScroll) > std::fabs(y - documentFolderScroll);
    });

    for (int i : drawList) {
        const FolderFrame& f = documentFolderFrames[i];
        SDL_FRect slot = GetDocumentFolderCardRect(i);
        slot.y += std::sin(time * 1.6f + i * 1.3f) * 6.0f;   // balanço leve, fora de fase entre páginas

        const float dist  = std::fabs(i - documentFolderScroll);
        const float fade  = a * (1.0f - 0.3f * std::min(dist, 2.0f));
        const Uint8 fa    = static_cast<Uint8>(255.0f * fade);
        const Uint8 shade = static_cast<Uint8>(255.0f - 105.0f * std::min(dist, 1.0f));   // vizinhas mais escuras

        if (f.docIndex >= 0) {
            const CollectedDocument& doc = collectedDocuments[f.docIndex];
            auto tex = Resources::GetImage(doc.imagePath);
            if (!tex) continue;
            int tw = 0, th = 0;
            SDL_QueryTexture(tex.get(), nullptr, nullptr, &tw, &th);
            const SDL_FRect page = FitInsideRect(slot, tw, th, 0.0f);

            // Sombra suave: a própria página em preto, deslocada.
            SDL_SetTextureColorMod(tex.get(), 0, 0, 0);
            SDL_SetTextureAlphaMod(tex.get(), static_cast<Uint8>(110.0f * fade));
            const SDL_FRect shadow{page.x + 12.0f, page.y + 16.0f, page.w, page.h};
            SDL_RenderCopyF(renderer, tex.get(), nullptr, &shadow);

            SDL_SetTextureColorMod(tex.get(), shade, shade, shade);
            SDL_SetTextureAlphaMod(tex.get(), fa);
            SDL_RenderCopyF(renderer, tex.get(), nullptr, &page);

            SDL_SetTextureColorMod(tex.get(), 255, 255, 255);   // textura é compartilhada pelo Resources
            SDL_SetTextureAlphaMod(tex.get(), 255);

            if (doc.unread) {
                const SDL_FRect badge{page.x + page.w - 70.0f, page.y - 14.0f, 76.0f, 32.0f};
                SDL_SetRenderDrawColor(renderer, 170, 45, 35, fa);
                SDL_RenderFillRectF(renderer, &badge);
                DrawFolderText(renderer, badgeFont.get(), "novo", badge.x + badge.w * 0.5f, badge.y + 2.0f,
                               SDL_Color{245, 235, 215, fa});
            }
        } else {
            // Documento que ficou para trás: só um "?" flutuando no lugar da página.
            DrawFolderText(renderer, bigFont.get(), "?", slot.x + slot.w * 0.5f,
                           slot.y + slot.h * 0.5f - 70.0f, SDL_Color{150, 140, 125, static_cast<Uint8>(fa * 0.7f)});
        }
    }

    // Título só para documento encontrado.
    const FolderFrame& sel = documentFolderFrames[documentFolderSelection];
    if (sel.docIndex >= 0) {
        const SDL_FRect selSlot = GetDocumentFolderCardRect(documentFolderSelection);
        DrawFolderText(renderer, titleFont.get(), collectedDocuments[sel.docIndex].title,
                       winW * 0.5f, selSlot.y + selSlot.h + 30.0f, textCol);
    }

    const std::string counter = std::to_string(collectedDocuments.size()) + " / " + std::to_string(documentFolderTotal);
    DrawFolderText(renderer, smallFont.get(), counter, winW * 0.5f, winH * 0.11f, dimCol);

    std::string interactKey = SDL_GetKeyName(InputManager::GetInstance().GetBinding(GameAction::Interact));
    if (interactKey.empty()) interactKey = "E";
    std::string hint = "Setas — navegar";
    if (sel.docIndex >= 0) {
        hint += "    " + interactKey + " — ler";
        if (FindDialogueForDocument(collectedDocuments[sel.docIndex].imagePath)) {
            std::string useKey = SDL_GetKeyName(InputManager::GetInstance().GetBinding(GameAction::UseItem));
            if (useKey.empty()) useKey = "F";
            hint += "    " + useKey + " — rever conversa";
        }
    }
    hint += "    Q — Diálogos    Tab / ESC — fechar";

    if (dialogueReplayActive) {
        DrawFolderText(renderer, smallFont.get(), "ESC — parar o replay", winW * 0.5f, winH * 0.125f, dimCol);
    }
    if (!dialogueBox.isActive()) {
        DrawFolderText(renderer, smallFont.get(), hint, winW * 0.5f, winH * 0.90f, dimCol);
    }
}

void StageState::UpdateDialogueLogTab(float dt) {
    InputManager& input = InputManager::GetInstance();
    const int n = static_cast<int>(dialogueLog.size());
    if (n == 0) return;

    // D/→ entra na conversa, A/← volta para a lista.
    if (input.ActionPress(GameAction::MoveRight) || input.KeyPress(SDLK_RIGHT)) dialogueLogFocusPanel = true;
    if (input.ActionPress(GameAction::MoveLeft)  || input.KeyPress(SDLK_LEFT))  dialogueLogFocusPanel = false;

    if (dialogueLogFocusPanel) {
        // Segurando W/S: rolagem contínua dos balões.
        float dir = 0.0f;
        if (input.ActionDown(GameAction::MoveUp)   || input.IsKeyDown(SDLK_UP))   dir -= 1.0f;
        if (input.ActionDown(GameAction::MoveDown) || input.IsKeyDown(SDLK_DOWN)) dir += 1.0f;
        dialogueLogScrollTarget += dir * kDialogueLogScrollSpeed * dt;
    } else {
        const int before = dialogueLogSelection;
        if (input.ActionPress(GameAction::MoveUp)   || input.KeyPress(SDLK_UP))   dialogueLogSelection = std::max(0, dialogueLogSelection - 1);
        if (input.ActionPress(GameAction::MoveDown) || input.KeyPress(SDLK_DOWN)) dialogueLogSelection = std::min(n - 1, dialogueLogSelection + 1);
        if (dialogueLogSelection != before) {
            dialogueLogScroll = dialogueLogScrollTarget = 0.0f;   // conversa nova começa do topo
        }
    }

    dialogueLogScrollTarget = std::max(0.0f, std::min(dialogueLogScrollTarget, dialogueLogMaxScroll));
    dialogueLogScroll += (dialogueLogScrollTarget - dialogueLogScroll) * std::min(1.0f, 14.0f * dt);

    // Replay: toca de novo na caixa de diálogo (sem registrar outra vez).
    const bool replay = input.ActionPress(GameAction::Interact) || input.KeyPress(SDLK_RETURN);
    if (replay && !dialogueBox.isActive()) {
        for (const DialogueBox::Line& l : dialogueLog[dialogueLogSelection].lines) {
            dialogueBox.Queue(l.speaker, l.listener, l.emotion, l.listenerEmotion, l.text);
        }
        dialogueReplayActive = true;
    }
}
// Entrada da aba Diálogos: foco na lista (W/S escolhem) ou na conversa (W/S
// rolam, D/A trocam o foco); E/Enter tocam o replay se nada estiver tocando.

void StageState::RenderFolderTabs(SDL_Renderer* renderer, float a) {
    const float winW = static_cast<float>(Game::GetInstance().GetWindowsWidth());
    const float winH = static_cast<float>(Game::GetInstance().GetWindowsHeight());
    const Uint8 ta = static_cast<Uint8>(255.0f * a);
    auto font = Resources::GetFont("Recursos/font/times.ttf", 36);
    if (!font) return;

    const char* labels[2] = {"Documentos", "Diálogos"};
    int w[2] = {0, 0}, h[2] = {0, 0};
    for (int i = 0; i < 2; ++i) TTF_SizeUTF8(font.get(), labels[i], &w[i], &h[i]);

    const float gap = 70.0f;
    const float total = w[0] + gap + w[1];
    float x = winW * 0.5f - total * 0.5f;
    const float y = winH * 0.03f;

    // Tecla [Q] à esquerda das abas.
    int qw = 0, qh = 0;
    KeyGlyphs::Measure(font.get(), "[Q]", 1.6f, qw, qh);
    KeyGlyphs::Draw(renderer, font.get(), "[Q]", static_cast<int>(x - 24.0f - qw),
                    static_cast<int>(y + h[0] * 0.5f - qh * 0.5f), SDL_Color{235, 225, 195, 255}, ta, 1.6f);

    for (int i = 0; i < 2; ++i) {
        const bool sel = (i == documentFolderTab);
        const SDL_Color c = sel ? SDL_Color{235, 215, 160, ta} : SDL_Color{150, 140, 125, ta};
        DrawFolderTextLeft(renderer, font.get(), labels[i], x, y, c);
        if (sel) {
            SDL_SetRenderDrawColor(renderer, 235, 215, 160, ta);
            const SDL_FRect underline{x, y + h[i] + 4.0f, static_cast<float>(w[i]), 3.0f};
            SDL_RenderFillRectF(renderer, &underline);
        }
        x += w[i] + gap;
    }
}
// Cabeçalho da pasta: abas "Documentos | Diálogos" (ativa sublinhada) e a
// tecla [Q] que alterna entre elas.

void StageState::RenderDialogueLogTab(SDL_Renderer* renderer, float a) {
    const float winW = static_cast<float>(Game::GetInstance().GetWindowsWidth());
    const float winH = static_cast<float>(Game::GetInstance().GetWindowsHeight());
    const Uint8 ta = static_cast<Uint8>(255.0f * a);
    const SDL_Color textCol{230, 225, 200, ta};
    const SDL_Color dimCol {200, 195, 175, ta};
    const SDL_Color fadeCol{150, 140, 125, ta};

    auto titleFont = Resources::GetFont("Recursos/font/times.ttf", 40);
    auto rowFont   = Resources::GetFont("Recursos/font/times.ttf", 30);
    auto smallFont = Resources::GetFont("Recursos/font/times.ttf", 22);
    auto nameFont  = Resources::GetFont("Recursos/font/times.ttf", 24);
    auto lineFont  = Resources::GetFont("Recursos/font/times.ttf", 28);
    auto hintFont  = Resources::GetFont("Recursos/font/times.ttf", 28);

    const bool showHints = !dialogueBox.isActive();   // durante o replay a caixa ocupa a parte de baixo
    const std::string hintTail = "Q — Documentos    Tab / ESC — fechar";

    if (dialogueLog.empty()) {
        DrawFolderText(renderer, titleFont.get(), "Nenhuma conversa ainda", winW * 0.5f, winH * 0.5f - 22.0f, textCol);
        if (showHints) DrawFolderText(renderer, hintFont.get(), hintTail, winW * 0.5f, winH * 0.90f, dimCol);
        return;
    }

    const int n = static_cast<int>(dialogueLog.size());
    dialogueLogSelection = std::max(0, std::min(dialogueLogSelection, n - 1));

    // ── Lista (esquerda) ────────────────────────────────────────────────
    const SDL_FRect list{winW * 0.05f, winH * 0.16f, winW * 0.30f, winH * 0.70f};
    const float rowH = 76.0f;
    const int maxRows = std::max(1, static_cast<int>(list.h / rowH));
    const int first = std::max(0, std::min(dialogueLogSelection - maxRows / 2, n - maxRows));

    for (int i = first; i < std::min(n, first + maxRows); ++i) {
        const DialogueLogEntry& e = dialogueLog[i];
        const float y = list.y + (i - first) * rowH;
        const bool sel = (i == dialogueLogSelection);

        if (sel) {
            // Com o foco na conversa, a seleção da lista fica mais apagada.
            const float k = dialogueLogFocusPanel ? 0.5f : 1.0f;
            SDL_SetRenderDrawColor(renderer, 235, 215, 160, static_cast<Uint8>(35.0f * a * k));
            const SDL_FRect bg{list.x, y, list.w, rowH - 6.0f};
            SDL_RenderFillRectF(renderer, &bg);
            SDL_SetRenderDrawColor(renderer, 235, 215, 160, static_cast<Uint8>(255.0f * a * k));
            const SDL_FRect accent{list.x, y, 4.0f, rowH - 6.0f};
            SDL_RenderFillRectF(renderer, &accent);
        }

        DrawFolderTextLeft(renderer, rowFont.get(), e.context, list.x + 18.0f, y + 6.0f, sel ? textCol : dimCol);
        const std::string sub = "Andar " + std::to_string(e.level + 1) + " · " +
                                std::to_string(e.lines.size()) + (e.lines.size() == 1 ? " fala" : " falas");
        DrawFolderTextLeft(renderer, smallFont.get(), sub, list.x + 18.0f, y + 42.0f, fadeCol);
    }

    // ── Conversa em balões (direita) ────────────────────────────────────
    const DialogueLogEntry& cur = dialogueLog[dialogueLogSelection];
    const SDL_FRect panel{winW * 0.39f, winH * 0.16f, winW * 0.55f, winH * 0.70f};
    DrawFolderText(renderer, titleFont.get(), cur.context, panel.x + panel.w * 0.5f, winH * 0.09f, textCol);

    if (dialogueLogFocusPanel) {
        SDL_SetRenderDrawColor(renderer, 235, 215, 160, static_cast<Uint8>(90.0f * a));
        const SDL_FRect border{panel.x - 10.0f, panel.y - 10.0f, panel.w + 20.0f, panel.h + 20.0f};
        SDL_RenderDrawRectF(renderer, &border);
    }

    const float portrait = 84.0f, gap = 14.0f, padX = 18.0f, padY = 12.0f, rowGap = 16.0f;
    const int wrapW = static_cast<int>(panel.w - portrait - gap - 2.0f * padX - 80.0f);

    // 1ª passada: prepara os textos e mede a altura total (para a rolagem).
    struct Bubble {
        SDL_Texture* txt = nullptr;
        SDL_Texture* name = nullptr;
        int tw = 0, th = 0, nw = 0, nh = 0;
        bool isLuke = false;
        std::string portraitPath;
        float bw = 0.0f, bh = 0.0f, rowH = 0.0f;
    };
    std::vector<Bubble> bubbles;
    float contentH = 0.0f;
    for (const DialogueBox::Line& l : cur.lines) {
        Bubble b;
        b.isLuke = (l.speaker == DialogueBox::Speaker::LittleBrother);
        const SDL_Color nameCol = b.isLuke ? SDL_Color{170, 200, 235, 255} : SDL_Color{235, 200, 140, 255};
        b.txt  = MakeWrappedText(renderer, lineFont.get(), l.text, SDL_Color{235, 228, 210, 255}, wrapW, b.tw, b.th);
        b.name = MakeWrappedText(renderer, nameFont.get(), DialogueBox::DisplayName(l.speaker), nameCol, 1000, b.nw, b.nh);
        b.portraitPath = DialogueBox::PortraitPath(l.speaker, l.emotion, false);
        b.bw   = std::max(b.tw, b.nw) + 2.0f * padX;
        b.bh   = (b.nh > 0 ? b.nh + 4.0f : 0.0f) + b.th + 2.0f * padY;
        b.rowH = std::max(b.bh, portrait) + rowGap;
        contentH += b.rowH;
        bubbles.push_back(b);
    }
    if (!bubbles.empty()) contentH -= rowGap;
    dialogueLogMaxScroll = std::max(0.0f, contentH - panel.h);

    // 2ª passada: desenha com o deslocamento da rolagem, recortado no painel.
    const SDL_Rect clip{static_cast<int>(panel.x), static_cast<int>(panel.y),
                        static_cast<int>(panel.w), static_cast<int>(panel.h)};
    SDL_RenderSetClipRect(renderer, &clip);

    float y = panel.y - dialogueLogScroll;
    for (const Bubble& b : bubbles) {
        if (y + b.rowH >= panel.y && y <= panel.y + panel.h) {
            const float px = b.isLuke ? panel.x + panel.w - portrait : panel.x;
            const float bx = b.isLuke ? px - gap - b.bw : px + portrait + gap;

            if (!b.portraitPath.empty()) {
                if (auto pt = Resources::GetImage(b.portraitPath)) {
                    int pw = 0, ph = 0;
                    SDL_QueryTexture(pt.get(), nullptr, nullptr, &pw, &ph);
                    const SDL_FRect pr = FitInsideRect(SDL_FRect{px, y, portrait, portrait}, pw, ph, 0.0f);
                    SDL_SetTextureAlphaMod(pt.get(), ta);
                    SDL_RenderCopyF(renderer, pt.get(), nullptr, &pr);
                    SDL_SetTextureAlphaMod(pt.get(), 255);
                }
            }

            const SDL_FRect bubble{bx, y, b.bw, b.bh};
            SDL_SetRenderDrawColor(renderer, 30, 26, 22, static_cast<Uint8>(225.0f * a));
            SDL_RenderFillRectF(renderer, &bubble);
            SDL_SetRenderDrawColor(renderer, 140, 125, 100, ta);
            SDL_RenderDrawRectF(renderer, &bubble);

            float ty = y + padY;
            if (b.name) {
                SDL_SetTextureAlphaMod(b.name, ta);
                const SDL_FRect nd{bx + padX, ty, static_cast<float>(b.nw), static_cast<float>(b.nh)};
                SDL_RenderCopyF(renderer, b.name, nullptr, &nd);
                ty += b.nh + 4.0f;
            }
            if (b.txt) {
                SDL_SetTextureAlphaMod(b.txt, ta);
                const SDL_FRect td{bx + padX, ty, static_cast<float>(b.tw), static_cast<float>(b.th)};
                SDL_RenderCopyF(renderer, b.txt, nullptr, &td);
            }
        }
        y += b.rowH;
    }
    SDL_RenderSetClipRect(renderer, nullptr);

    for (Bubble& b : bubbles) {
        if (b.txt)  SDL_DestroyTexture(b.txt);
        if (b.name) SDL_DestroyTexture(b.name);
    }

    // Indicadores de conversa fora da área visível: esfumado na borda + seta.
    // Cada um aparece com fade conforme a distância até o topo/fim (em px).
    const float moreAbove = std::min(1.0f, dialogueLogScroll / 40.0f);
    const float moreBelow = std::min(1.0f, (dialogueLogMaxScroll - dialogueLogScroll) / 40.0f);
    const float bob = std::sin(SDL_GetTicks() / 1000.0f * 3.0f) * 2.0f;
    const float arrowCX = panel.x + panel.w * 0.5f;
    const float fadeH = 70.0f;
    const float arrowHalfW = 12.0f;   
    const float arrowH     = 10.0f;   

    if (moreAbove > 0.01f) {
        DrawEdgeFade(renderer, SDL_FRect{panel.x, panel.y, panel.w, fadeH}, true,
                     static_cast<Uint8>(200.0f * a * moreAbove));
        const Uint8 aa = static_cast<Uint8>(255.0f * a * moreAbove);
        const float cy = panel.y - 22.0f - bob;
        DrawFolderArrow(renderer, arrowCX + 2.0f, cy + 2.0f, arrowHalfW, arrowH, true, SDL_Color{0, 0, 0, static_cast<Uint8>(aa * 0.6f)});
        DrawFolderArrow(renderer, arrowCX, cy, arrowHalfW, arrowH, true, SDL_Color{235, 215, 160, aa});
    }
    if (moreBelow > 0.01f) {
        DrawEdgeFade(renderer, SDL_FRect{panel.x, panel.y + panel.h - fadeH, panel.w, fadeH}, false,
                     static_cast<Uint8>(200.0f * a * moreBelow));
        const Uint8 aa = static_cast<Uint8>(255.0f * a * moreBelow);
        const float cy = panel.y + panel.h + 22.0f + bob;
        DrawFolderArrow(renderer, arrowCX + 2.0f, cy + 2.0f, arrowHalfW, arrowH, false, SDL_Color{0, 0, 0, static_cast<Uint8>(aa * 0.6f)});
        DrawFolderArrow(renderer, arrowCX, cy, arrowHalfW, arrowH, false, SDL_Color{235, 215, 160, aa});
    }

    if (dialogueReplayActive) {
        DrawFolderText(renderer, smallFont.get(), "ESC — parar o replay",
                       winW * 0.5f, winH * 0.125f, dimCol);
    }

    if (showHints) {
        std::string interactKey = SDL_GetKeyName(InputManager::GetInstance().GetBinding(GameAction::Interact));
        if (interactKey.empty()) interactKey = "E";
        const std::string nav = dialogueLogFocusPanel ? "W / S — rolar    A — voltar à lista"
                                                      : "W / S — escolher    D — ler conversa";
        const std::string hint = nav + "    " + interactKey + " — Replay    " + hintTail;
        DrawFolderText(renderer, hintFont.get(), hint, winW * 0.5f, winH * 0.90f, dimCol);
    }
}
// Aba Diálogos: lista das conversas à esquerda e a selecionada em balões à
// direita (Martin à esquerda, Luke à direita, com retrato da emoção). O painel
// rola quando o foco está nele; as dicas somem enquanto um replay toca.


