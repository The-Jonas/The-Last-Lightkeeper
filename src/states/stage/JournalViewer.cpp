#include "states/stage/StageState.h"
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
#include <cmath>

namespace {

float SmoothStep(float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    return t * t * (3.0f - 2.0f * t);
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
    if (!jornal) {
        return;
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

    journalAnimTimer = 0.0f;
    journalCloseTimer = 0.0f;
    journalViewerClosing = false;
    journalViewerOpen = true;

    const int winW = Game::GetInstance().GetWindowsWidth();
    const int winH = Game::GetInstance().GetWindowsHeight();
    int texW = 1;
    int texH = 1;
    if (auto tex = Resources::GetImage(journalViewImagePath)) {
        SDL_QueryTexture(tex.get(), nullptr, nullptr, &texW, &texH);
    }
    journalTargetScreenRect = FitTextureInWindow(winW, winH, texW, texH);
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

    if (input.ActionPress(GameAction::Interact) || input.KeyPress(SDLK_ESCAPE)) {
        journalViewerClosing = true;
        journalCloseTimer = 0.0f;
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
        const SDL_FRect dst = LerpRect(journalSourceScreenRect, journalTargetScreenRect, openT);
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
            const std::string hintStr = keyName + " / ESC — fechar";
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
