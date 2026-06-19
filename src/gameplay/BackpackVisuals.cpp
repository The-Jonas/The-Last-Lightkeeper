#include "gameplay/BackpackVisuals.h"
#include "core/Game.h"
#include "core/Resources.h"
#include "engine/Camera.h"
#include "gameplay/Character.h"
#include "gameplay/Inventory.h"

namespace {

struct BodyAttachOffset {
    float x;
    float y;
    float sizeMul;
};

BodyAttachOffset OffsetForItemGroup(const std::string& groupId,
                                    Character::Direction dir,
                                    int stackIndex,
                                    float boxW,
                                    float boxH) {
    BodyAttachOffset off{0.0f, 0.0f, 1.0f};

    if (groupId == "lighter") {
        switch (dir) {
        case Character::Direction::DOWN:
            off = {boxW * 0.22f, boxH * 0.72f, 0.55f};
            break;
        case Character::Direction::UP:
            off = {-boxW * 0.18f, boxH * 0.68f, 0.55f};
            break;
        case Character::Direction::LEFT:
            off = {-boxW * 0.24f, boxH * 0.74f, 0.55f};
            break;
        case Character::Direction::RIGHT:
            off = {boxW * 0.24f, boxH * 0.74f, 0.55f};
            break;
        }
    } else if (groupId == "lamp") {
        switch (dir) {
        case Character::Direction::DOWN:
            off = {-boxW * 0.30f, boxH * 0.48f, 0.85f};
            break;
        case Character::Direction::UP:
            off = {boxW * 0.26f, boxH * 0.42f, 0.85f};
            break;
        case Character::Direction::LEFT:
            off = {-boxW * 0.08f, boxH * 0.46f, 0.85f};
            break;
        case Character::Direction::RIGHT:
            off = {boxW * 0.08f, boxH * 0.46f, 0.85f};
            break;
        }
    } else if (groupId == "oil") {
        switch (dir) {
        case Character::Direction::DOWN:
            off = {boxW * 0.06f, boxH * 0.34f, 0.95f};
            break;
        case Character::Direction::UP:
            off = {0.0f, boxH * 0.38f, 1.0f};
            break;
        case Character::Direction::LEFT:
            off = {boxW * 0.22f, boxH * 0.36f, 0.95f};
            break;
        case Character::Direction::RIGHT:
            off = {-boxW * 0.22f, boxH * 0.36f, 0.95f};
            break;
        }
    } else {
        switch (dir) {
        case Character::Direction::DOWN:
            off = {-boxW * 0.18f, boxH * 0.50f, 0.7f};
            break;
        case Character::Direction::UP:
            off = {boxW * 0.16f, boxH * 0.44f, 0.7f};
            break;
        case Character::Direction::LEFT:
            off = {boxW * 0.10f, boxH * 0.48f, 0.7f};
            break;
        case Character::Direction::RIGHT:
            off = {-boxW * 0.10f, boxH * 0.48f, 0.7f};
            break;
        }
    }

    off.x += static_cast<float>(stackIndex) * 3.0f;
    off.y -= static_cast<float>(stackIndex) * 2.0f;
    return off;
}

} // namespace

BackpackVisuals::BackpackVisuals(GameObject& associated, Inventory& inventory, Character* bigChar)
    : Component(associated), inventory(inventory), bigCharacter(bigChar) {}

void BackpackVisuals::Start() {}

void BackpackVisuals::Update(float dt) {
    (void)dt;
}

void BackpackVisuals::Render() {
    if (!bigCharacter) {
        return;
    }

    SDL_Renderer* renderer = Game::GetInstance().GetRenderer();
    if (!renderer) {
        return;
    }

    const GameObject& ch = bigCharacter->GetAssociated();
    const Character::Direction facing = bigCharacter->GetFacingDirection();
    const float zoom = Camera::GetZoom();
    const float baseDrawSize = 22.0f * zoom;
    const int selectedGroup = inventory.GetSelectedGroup();
    const BackpackConfig& cfg = inventory.GetBackpackConfig();

    for (int gi = 0; gi < static_cast<int>(cfg.groups.size()); ++gi) {
        const int count = inventory.CountInGroup(gi);
        if (count <= 0) {
            continue;
        }

        const BackpackGroupDef* group = cfg.GetGroup(gi);
        const std::string groupId = group ? group->id : "";

        for (int si = 0; si < count; ++si) {
            if (gi == selectedGroup) {
                continue;
            }

            const ItemInstance* item = inventory.GetItemInGroup(gi, si);
            if (!item) {
                continue;
            }

            const BodyAttachOffset attach =
                OffsetForItemGroup(groupId, facing, si, ch.box.w, ch.box.h);
            const float cx = ch.box.x + ch.box.w * 0.5f + attach.x;
            const float cy = ch.box.y + attach.y;
            const float drawSize = baseDrawSize * attach.sizeMul;
            const float screenX = (cx - Camera::pos.x) * zoom - drawSize * 0.5f;
            const float screenY = (cy - Camera::pos.y) * zoom - drawSize * 0.5f;

            auto tex = Resources::GetImage(item->def.spritePath);
            if (!tex) {
                continue;
            }

            SDL_FRect dst{screenX, screenY, drawSize, drawSize};
            SDL_SetTextureAlphaMod(tex.get(), 235);
            SDL_SetTextureColorMod(tex.get(), 255, 255, 255);
            SDL_RenderCopyExF(renderer, tex.get(), nullptr, &dst, 0.0, nullptr,
                              (facing == Character::Direction::LEFT) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
        }
    }
}
