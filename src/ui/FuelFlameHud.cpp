#include "ui/FuelFlameHud.h"
#include "ui/FuelHudTuning.h"
#include "gameplay/Inventory.h"
#include "core/Resources.h"
#include "core/Game.h"
#include "SDL_include.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

void FuelFlameHud::Update(float dt) {
    frameTimer += dt;
    if (frameTimer >= FuelHudTuning::frameDuration) {
        frameTimer -= FuelHudTuning::frameDuration;
        currentFrame = (currentFrame + 1) % 3;
    }
}

// 100–75% nível 1, 75–50% nível 2, 50–25% nível 3, 25–0% nível 4.
int FuelFlameHud::LevelForRatio(float ratio) {
    if (ratio > 0.75f) return 1;
    if (ratio > 0.50f) return 2;
    if (ratio > 0.25f) return 3;
    return 4;
}

std::string FuelFlameHud::FramePath(int level, int frame) {
    std::ostringstream oss;
    oss << "Recursos/img/ui/fogo_combustivel/nivel" << level << "/"
        << std::setfill('0') << std::setw(4) << (frame + 1) << ".png";
    return oss.str();   
}

void FuelFlameHud::Render(SDL_Renderer* renderer, Inventory& inventory, int windowW, int windowH) {
    if (!renderer) return;
    if (!inventory.IsUsableLightActive()) return;   // Só mostra com a luz ligada

    const Inventory::ItemStack* active = inventory.GetActiveStack();
    if (!active) return;
    if (!active->def.HasProperty(ItemProperty::LIGHT_SOURCE) || active->def.maxDurability <= 0) return;

    // Zerou o combustível: sem chama nenhuma, some a HUD inteira.
    const int durability = active->durabilities.empty() ? 0 : active->durabilities.front();
    if (durability <= 0) return;

    const float ratio = inventory.GetSelectedLightFuelRatio();   

    auto tex = Resources::GetImage(FramePath(LevelForRatio(ratio), currentFrame));
    if (!tex) return;

    const float scale = Game::UiScale() * FuelHudTuning::iconScaleMul;
    int texW = 0, texH = 0;
    SDL_QueryTexture(tex.get(), nullptr, nullptr, &texW, &texH);
    const int iconW = static_cast<int>(texW * scale);
    const int iconH = static_cast<int>(texH * scale);
    const int marginTop = static_cast<int>(FuelHudTuning::marginTop * scale);
    const int marginRight = static_cast<int>(FuelHudTuning::marginRight * scale);

    const SDL_Rect dst{ windowW - iconW - marginRight, marginTop, iconW, iconH };
    SDL_RenderCopy(renderer, tex.get(), nullptr, &dst);
}