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
        for (const DialogueBox::Line& l : jornal->GetDialogueLines()) {
            dialogueBox.Queue(l.speaker, l.listener, l.emotion, l.listenerEmotion, l.text);
        }
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

    if (!documentTutorialShown) {
        documentTutorialShown = true;
        RequestTutorial(kDocumentTutorialText);
    }
    
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

    documentFolderOpen = true;
    documentFolderAnim = 0.0f;
    Telemetry::Event("document_folder_open", Telemetry::Fields()
        .Int("level", currentLevelIndex)
        .Int("collected", static_cast<int>(collectedDocuments.size())));
}



// Fecha a pasta e devolve o controle ao jogo.
void StageState::CloseDocumentFolder() {
    documentFolderOpen = false;
}



// Abre um documento da pasta no visualizador normal (zoom etc.), crescendo a
// partir da moldura; tira o "novo" e toca o diálogo pendente.
void StageState::OpenCollectedDocument(CollectedDocument& doc, const SDL_FRect& fromRect) {
    BeginJournalView(doc.imagePath, doc.soundPath, doc.zoomFactor, doc.zoomable, fromRect);
    doc.unread = false;

    // Diálogo do Tiled: só na primeira leitura.
    for (const DialogueBox::Line& l : doc.dialogueLines) {
        dialogueBox.Queue(l.speaker, l.listener, l.emotion, l.listenerEmotion, l.text);
    }
    doc.dialogueLines.clear();
}



// Entrada da pasta: Tab/ESC fecham, esquerda/direita navegam, E/Enter abrem o
// documento selecionado (molduras vazias são ignoradas). Suaviza a rolagem.
void StageState::UpdateDocumentFolder(float dt) {
    InputManager& input = InputManager::GetInstance();
    if (bigCharacter)   bigCharacter->ReleaseInteract();
    if (smallCharacter) smallCharacter->ReleaseInteract();

    documentFolderAnim = std::min(1.0f, documentFolderAnim + dt / kDocumentFolderFadeTime);

    if (input.KeyPress(TAB_KEY) || input.KeyPress(ESCAPE_KEY)) {
        CloseDocumentFolder();
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
    }

    const float target = static_cast<float>(documentFolderSelection);
    documentFolderScroll += (target - documentFolderScroll) * std::min(1.0f, kDocumentFolderScrollSmoothing * dt);
}



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
    DrawFolderText(renderer, smallFont.get(), counter, winW * 0.5f, winH * 0.07f, dimCol);

    std::string interactKey = SDL_GetKeyName(InputManager::GetInstance().GetBinding(GameAction::Interact));
    if (interactKey.empty()) interactKey = "E";
    std::string hint = "Setas — navegar";
    if (sel.docIndex >= 0) hint += "    " + interactKey + " — ler";
    hint += "    Tab / ESC — fechar";
    DrawFolderText(renderer, smallFont.get(), hint, winW * 0.5f, winH * 0.90f, dimCol);
}

