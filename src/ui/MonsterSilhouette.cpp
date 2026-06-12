#include "ui/MonsterSilhouette.h"
#include "gameplay/Monster.h"
#include "engine/SpriteRenderer.h"
#include "core/Game.h"
 
void MonsterSilhouette::Render() {

if (!monsterOwner) return;

    float timer = monsterOwner->GetVisionRevealTimer();
    if (timer <= 0.0f) return;

    float maxDuration = Monster::kVisionRevealDuration; 
    float ratio = std::max(0.0f, std::min(1.0f, timer / maxDuration));

    SpriteRenderer* sr = monsterOwner->GetAssociated().GetComponent<SpriteRenderer>();
    if (!sr) return;

    // DIMINUINDO A OPACIDADE (ATUALMENTE 60.0) ────────────────────────────────────────────

    Uint8 glowAlpha = static_cast<Uint8>(60.0f * ratio);
    
    Uint8 interiorAlpha = (ratio > 0.2f) ? 255 : static_cast<Uint8>((ratio / 0.2f) * 255.0f);

    GameObject& go = monsterOwner->GetAssociated();
    float originalX = go.box.x;
    float originalY = go.box.y;

    static constexpr int NUM_STROKES = 5; 
    static float jitterTimer = 0.0f;
    
    static float s_offsetX[NUM_STROKES] = {0};
    static float s_offsetY[NUM_STROKES] = {0};
    static float s_scale[NUM_STROKES] = {1.0f};
    static float s_alphaMult[NUM_STROKES] = {1.0f};
    static Uint8 s_r[NUM_STROKES], s_g[NUM_STROKES], s_b[NUM_STROKES];

    float dt = Game::GetInstance().GetDeltaTime();
    jitterTimer += dt;

    if (jitterTimer > 0.08f) {
        jitterTimer = 0.0f;
        for (int i = 0; i < NUM_STROKES; i++) {

            // ── DIMINUINDO O DESLOCAMENTO (OFFSET) ──

            s_offsetX[i] = (rand() % 7) - 3; 
            s_offsetY[i] = (rand() % 7) - 3; 
            
            // ── DIMINUINDO A ESCALA (GROSSURA) ──

            s_scale[i] = 1.01f + static_cast<float>(rand() % 5) / 100.0f; 
            s_alphaMult[i] = 0.3f + static_cast<float>(rand() % 71) / 100.0f; 

            if (rand() % 2 == 0) {
                // ── MUDANDO AS CORES (ROSA AVERMELHADO NEON ATUALMENTE) ──────────────────────
                // 50% de chance de ser o Roxo Neon característico
                s_r[i] = 255; s_g[i] = 60; s_b[i] = 115;
            } else {
                // 50% de chance de ser um rabisco Branco de alta energia
                s_r[i] = 255; s_g[i] = 255; s_b[i] = 255;
            }
        }
    }

    // Desenhando os rabiscos
    for (int i = 0; i < NUM_STROKES; i++) {
        go.box.x = originalX + s_offsetX[i];
        go.box.y = originalY + s_offsetY[i];
        
        Uint8 strokeAlpha = static_cast<Uint8>(glowAlpha * s_alphaMult[i]);
        sr->RenderHighlight(s_scale[i], s_r[i], s_g[i], s_b[i], strokeAlpha);  
    }

    // Miolo preto
    go.box.x = originalX;
    go.box.y = originalY;
    sr->RenderHighlight(1.0f, 0, 0, 0, interiorAlpha);
}


