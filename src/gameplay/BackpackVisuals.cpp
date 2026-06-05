#include "gameplay/BackpackVisuals.h"
#include "core/Game.h"
#include "core/Resources.h"
#include "engine/Camera.h"
#include "gameplay/Character.h"
#include "gameplay/Inventory.h"

namespace {

struct SatchelOffset {
    float x;
    float y;
};

SatchelOffset OffsetForFacing(Character::Direction dir, int groupIndex, int stackIndex) {
    SatchelOffset base{};
    switch (dir) {
    case Character::Direction::DOWN:
        base = {-18.0f, 8.0f};
        break;
    case Character::Direction::UP:
        base = {16.0f, 10.0f};
        break;
    case Character::Direction::LEFT:
        base = {14.0f, 4.0f};
        break;
    case Character::Direction::RIGHT:
        base = {-20.0f, 4.0f};
        break;
    }
    base.x += static_cast<float>(groupIndex) * 4.0f;
    base.y += static_cast<float>(stackIndex) * 3.0f;
    return base;
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
    const float drawSize = 22.0f * zoom;
    const int selectedGroup = inventory.GetSelectedGroup();
    const BackpackConfig& cfg = inventory.GetBackpackConfig();

    for (int gi = 0; gi < static_cast<int>(cfg.groups.size()); ++gi) {
        const int count = inventory.CountInGroup(gi);
        if (count <= 0) {
            continue;
        }

        for (int si = 0; si < count; ++si) {
            const ItemInstance* item = inventory.GetItemInGroup(gi, si);
            if (!item) {
                continue;
            }

            const SatchelOffset off = OffsetForFacing(facing, gi, si);
            const float cx = ch.box.x + ch.box.w * 0.5f + off.x;
            const float cy = ch.box.y + ch.box.h * 0.55f + off.y;
            const float screenX = (cx - Camera::pos.x) * zoom - drawSize * 0.5f;
            const float screenY = (cy - Camera::pos.y) * zoom - drawSize * 0.5f;

            auto tex = Resources::GetImage(item->def.spritePath);
            if (!tex) {
                continue;
            }

            const bool selected = (gi == selectedGroup);
            const float size = selected ? drawSize * 1.12f : drawSize;
            const float adjX = screenX - (size - drawSize) * 0.5f;
            const float adjY = screenY - (size - drawSize) * 0.5f;
            SDL_FRect dst{adjX, adjY, size, size};
            SDL_SetTextureAlphaMod(tex.get(), selected ? 255 : 210);
            SDL_SetTextureColorMod(tex.get(), 255, 255, 255);
            SDL_RenderCopyExF(renderer, tex.get(), nullptr, &dst, 0.0, nullptr,
                              (facing == Character::Direction::LEFT) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
        }
    }
}
