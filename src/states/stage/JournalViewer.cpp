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

void StageState::OpenJournalViewer(Jornal* jornal) {
    if (!jornal) return;

    if (bigCharacter)   bigCharacter->ForceStop();
    if (smallCharacter) smallCharacter->ForceStop();

    // ── Som ao abrir ──────────────────────────────────────────────────────
    // Prioridade ao som explícito do mapa; senão, se for um documento de PAPEL,
    // toca um farfalhar de folha (não vale para fotos/telefones/caixas).
    std::string openSound = jornal->GetSoundPath();
    if (openSound.empty() && IsPaperDocumentImage(jornal->GetImagePath())) {
        openSound = PickPaperRustleSfx();
    }
    if (!openSound.empty()) {
        jornalInteractSound.Open(openSound);
        jornalInteractSound.Play();
    }

    const GameObject& obj = jornal->GetAssociated();
    journalViewImagePath = jornal->GetImagePath();
    const float zoom = Camera::GetZoom();
    journalSourceScreenRect = {
        (obj.box.x - Camera::pos.x) * zoom,
        (obj.box.y - Camera::pos.y) * zoom,
        obj.box.w * zoom,
        obj.box.h * zoom,
    };

    journalAnimTimer   = 0.0f;
    journalCloseTimer  = 0.0f;
    journalViewerClosing = false;
    journalViewerOpen    = true;
    journalViewZoomable = jornal->IsZoomable();
    journalZoomLevel    = 0;
    journalZoomCurrent  = 1.0f;
    journalFocusX = journalFocusY = 0.5f;

    // Telemetria: que documentos o jogador abriu — e, com o par de eventos,
    // quanto tempo os leu de facto (ou se fechou logo a seguir).
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

    // Calcula o rect base (cabe na tela) e aplica o zoom do item
    journalTargetScreenRect = FitTextureInWindow(winW, winH, texW, texH);

    // ── Zoom opcional (1.0 = sem zoom, 1.5 = 50% maior, etc.) ────────────
    float zf = jornal->GetZoomFactor();
    if (zf != 1.0f) {
        float cx = journalTargetScreenRect.x + journalTargetScreenRect.w * 0.5f;
        float cy = journalTargetScreenRect.y + journalTargetScreenRect.h * 0.5f;
        journalTargetScreenRect.w *= zf;
        journalTargetScreenRect.h *= zf;
        journalTargetScreenRect.x  = cx - journalTargetScreenRect.w * 0.5f;
        journalTargetScreenRect.y  = cy - journalTargetScreenRect.h * 0.5f;
    }
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

    OpenJournalViewer(reachableJornal);
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
