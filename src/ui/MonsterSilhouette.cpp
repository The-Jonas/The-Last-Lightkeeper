#include "ui/MonsterSilhouette.h"
#include "gameplay/Monster.h"
#include "engine/SpriteRenderer.h"
 
void MonsterSilhouette::Render() {
    if (!monsterOwner) return;
    if (!monsterOwner->isVisibleToLittleBrother()) return;
 
    SpriteRenderer* sr = monsterOwner->GetAssociated().GetComponent<SpriteRenderer>();
    if (!sr) return;
 
    float ratio = monsterOwner->GetVisionRevealTimer() / Monster::kVisionRevealDuration;
    Uint8 alpha  = static_cast<Uint8>(180.0f * std::max(0.0f, std::min(1.0f, ratio)));
 
    sr->RenderHighlight(1.05f, 120, 0, 200, alpha);
}