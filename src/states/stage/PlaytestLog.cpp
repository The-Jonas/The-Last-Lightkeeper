// ─────────────────────────────────────────────────────────────────────────────
//  StageState — parte de TELEMETRIA DE PLAYTEST.
//
//  Os eventos soltos (apanhou, morreu, acendeu) ficam onde a accao acontece.
//  Aqui fica o que so faz sentido ao ritmo do frame:
//    • a AMOSTRA de um em um segundo, que da os mapas de calor e as curvas;
//    • a deteccao de PRESO, que aponta os sitios onde o mapa agarra o jogador.
//
//  Ver include/core/Telemetry.h para o formato do ficheiro.
// ─────────────────────────────────────────────────────────────────────────────
#include "states/stage/StageState.h"

#include "core/Telemetry.h"
#include "engine/GameObject.h"
#include "gameplay/Character.h"
#include "gameplay/Monster.h"

#include <algorithm>
#include <cmath>

namespace {
/// O personagem tenta andar (o jogador esta a carregar numa tecla de direccao).
constexpr float kWantsToMoveSpeed = 8.0f;
/// Velocidade real abaixo da qual se considera que nao saiu do sitio (px/s).
/// O personagem a andar a serio faz varias centenas de px/s.
constexpr float kStuckSpeedPxPerSec = 20.0f;
/// Tempo a empurrar sem sair do sitio ate valer a pena registar.
constexpr float kStuckSeconds = 2.0f;
/// Um registo de "preso" por cada 15 s: preso contra a mesma parede durante
/// meio minuto e UM problema, nao trinta.
constexpr float kStuckCooldown = 15.0f;
/// Periodo da amostra em jogo normal. Um segundo chega para o mapa de calor e
/// mantem o ficheiro pequeno (cerca de 250 bytes por amostra).
constexpr float kSamplePeriod = 1.0f;
/// Periodo nos momentos quentes (perseguicao ou sanidade em baixo). A primeira
/// sessao gravada perdeu a morte inteira entre duas amostras: 100 de sanidade
/// desapareceram no intervalo. Quatro vezes mais depressa custa uns kB e mostra
/// a curva toda.
constexpr float kSamplePeriodHot = 0.25f;
/// Sanidade abaixo da qual o momento conta como quente.
constexpr float kHotSanity = 40.0f;

/// Queda de sanidade, somada em cerca de um segundo, a partir da qual vale a
/// pena uma linha propria. Um toque do monstro no escuro tira 80.
constexpr float kSanityCliff = 20.0f;
constexpr float kCliffCooldown = 3.0f;
/// Patamares anunciados uma unica vez por descida.
constexpr float kSanityTiers[] = {75.0f, 50.0f, 25.0f};

/// Quieto no mesmo sitio durante isto = o jogador nao sabe o que fazer.
constexpr float kIdleSeconds = 20.0f;
/// Raio dentro do qual ainda conta como "no mesmo sitio".
constexpr float kIdleRadiusPx = 48.0f;
}  // namespace

const char* StageState::TelemetryMonsterState(float& outDistToPlayer) const {
    outDistToPlayer = -1.0f;
    for (const auto& goPtr : objectArray) {
        GameObject* go = goPtr.get();
        if (!go || go->IsDead()) continue;
        Monster* monster = go->GetComponent<Monster>();
        if (!monster) continue;
        if (controlledCharacterObject) {
            outDistToPlayer = go->box.Center().Distance(controlledCharacterObject->box.Center());
        }
        return Monster::StateName(monster->GetState());
    }
    return "";
}

void StageState::UpdateTelemetry(float dt) {
    if (!Telemetry::IsEnabled()) {
        return;
    }

    telemetryLevelElapsed += dt;
    if (telemetryStuckCooldown > 0.0f) {
        telemetryStuckCooldown -= dt;
    }

    const bool lightOn = inventory.IsUsableLightActive();
    if (lightOn) {
        telemetryLitSeconds += dt;
    }

    // ── Distancia andada + preso ────────────────────────────────────────────
    if (controlledCharacterObject && controlledCharacter) {
        const Vec2 pos = controlledCharacterObject->box.Center();
        if (telemetryHasLastPos) {
            const float moved = pos.Distance(telemetryLastPos);
            telemetryWalkedPx += moved;

            // "Preso" = o personagem QUER andar (o jogador tem a tecla em
            // baixo, a velocidade interna nao e zero) mas o corpo nao sai do
            // sitio. A comparacao e em px/s para nao depender do frame rate.
            const float speedPxPerSec = moved / std::max(0.0001f, dt);
            const bool wantsToMove = controlledCharacter->GetSpeed().Magnitude() > kWantsToMoveSpeed;
            const bool freeToMove = !IsPlayerInputFrozen() && !sceneTransitionActive;
            if (wantsToMove && freeToMove && speedPxPerSec < kStuckSpeedPxPerSec) {
                telemetryStuckAccum += dt;
            } else {
                telemetryStuckAccum = 0.0f;
            }

            if (telemetryStuckAccum >= kStuckSeconds && telemetryStuckCooldown <= 0.0f) {
                telemetryStuckCooldown = kStuckCooldown;
                telemetryStuckAccum = 0.0f;
                Telemetry::Event("stuck", Telemetry::Fields()
                    .Int("level", currentLevelIndex)
                    .Pos("", pos.x, pos.y)
                    .Str("who", controlledCharacter == bigCharacter ? "big" : "small")
                    .Num("seconds", kStuckSeconds));
            }
        }
        telemetryLastPos = pos;
        telemetryHasLastPos = true;
    }

    // ── Sanidade: quedas e patamares ────────────────────────────────────────
    // Corre a cada frame, nao a cada amostra: o que interessa e exactamente o
    // que acontece DENTRO de um segundo.
    const float sanBig = bigCharacter ? bigCharacter->sanity : -1.0f;
    const float sanSmall = smallCharacter ? smallCharacter->sanity : -1.0f;
    if (telemetryCliffCooldown > 0.0f) {
        telemetryCliffCooldown -= dt;
    }
    if (sanBig >= 0.0f && telemetryPrevSanityBig >= 0.0f) {
        telemetryCliffAccum += std::max(0.0f, telemetryPrevSanityBig - sanBig);
        if (sanSmall >= 0.0f && telemetryPrevSanitySmall >= 0.0f) {
            telemetryCliffAccum += std::max(0.0f, telemetryPrevSanitySmall - sanSmall);
        }
        // Esquece a queda ao longo de ~1 s: o que fica somado e o que caiu de
        // repente, nao o desgaste lento de estar no escuro.
        telemetryCliffAccum = std::max(0.0f, telemetryCliffAccum - telemetryCliffAccum * dt);

        if (telemetryCliffAccum >= kSanityCliff && telemetryCliffCooldown <= 0.0f) {
            telemetryCliffCooldown = kCliffCooldown;
            float monsterDist = -1.0f;
            const char* monsterState = TelemetryMonsterState(monsterDist);
            Telemetry::Fields f;
            f.Int("level", currentLevelIndex)
             .Num("dropped", telemetryCliffAccum)
             .Num("sanityBig", sanBig)
             .Num("sanitySmall", sanSmall)
             .Bool("lightOn", lightOn)
             .Str("monster", monsterState);
            if (controlledCharacterObject) {
                const Vec2 c = controlledCharacterObject->box.Center();
                f.Pos("", c.x, c.y);
            }
            Telemetry::Event("sanity_drop", f);
            telemetryCliffAccum = 0.0f;
        }

        // Patamares: 75, 50, 25 e 0, uma vez cada um enquanto a sanidade desce.
        const float lowest = std::min(sanBig, sanSmall >= 0.0f ? sanSmall : sanBig);
        const int tierCount = static_cast<int>(sizeof(kSanityTiers) / sizeof(kSanityTiers[0]));
        while (telemetrySanityTier < tierCount && lowest <= kSanityTiers[telemetrySanityTier]) {
            Telemetry::Event("sanity_tier", Telemetry::Fields()
                .Int("level", currentLevelIndex)
                .Num("below", kSanityTiers[telemetrySanityTier])
                .Num("levelTime", telemetryLevelElapsed));
            telemetrySanityTier++;
        }
        // Recuperou com folga: volta a armar os patamares ja passados.
        while (telemetrySanityTier > 0 && lowest > kSanityTiers[telemetrySanityTier - 1] + 10.0f) {
            telemetrySanityTier--;
        }
    }
    telemetryPrevSanityBig = sanBig;
    telemetryPrevSanitySmall = sanSmall;

    // ── Quieto no mesmo sitio ───────────────────────────────────────────────
    if (controlledCharacterObject && !IsPlayerInputFrozen() && !sceneTransitionActive) {
        const Vec2 here = controlledCharacterObject->box.Center();
        if (here.Distance(telemetryIdleAnchor) > kIdleRadiusPx) {
            telemetryIdleAnchor = here;
            telemetryIdleAccum = 0.0f;
        } else {
            telemetryIdleAccum += dt;
            if (telemetryIdleAccum >= kIdleSeconds) {
                telemetryIdleAccum = 0.0f;   // volta a contar: 40 s dao 2 linhas
                Telemetry::Event("idle", Telemetry::Fields()
                    .Int("level", currentLevelIndex)
                    .Pos("", here.x, here.y)
                    .Num("seconds", kIdleSeconds));
            }
        }
    } else {
        telemetryIdleAccum = 0.0f;
    }

    // ── Amostra ─────────────────────────────────────────────────────────────
    // O ritmo sobe quando o jogo aperta (perseguicao, ou sanidade em baixo).
    const float lowestSanity = std::min(sanBig >= 0.0f ? sanBig : 100.0f,
                                        sanSmall >= 0.0f ? sanSmall : 100.0f);
    const bool hot = Telemetry::IsIntense() || lowestSanity < kHotSanity;
    telemetrySampleTimer += dt;
    if (telemetrySampleTimer < (hot ? kSamplePeriodHot : kSamplePeriod)) {
        return;
    }
    telemetrySampleTimer = 0.0f;

    Telemetry::Fields f;
    f.Int("level", currentLevelIndex).Num("levelTime", telemetryLevelElapsed);

    if (bigCharacterObject && bigCharacter) {
        const Vec2 c = bigCharacterObject->box.Center();
        f.Pos("big", c.x, c.y).Num("sanityBig", bigCharacter->sanity);
    }
    if (smallCharacterObject && smallCharacter) {
        const Vec2 c = smallCharacterObject->box.Center();
        f.Pos("small", c.x, c.y).Num("sanitySmall", smallCharacter->sanity);
    }

    f.Str("controlled", controlledCharacter == smallCharacter ? "small" : "big")
     .Str("party", partyMode == PartyMode::TOGETHER ? "together" : "independent")
     .Bool("lightOn", lightOn)
     .Num("fuel", inventory.GetSelectedLightFuelRatio())
     .Int("items", inventory.GetStackCount())
     .Num("walkedPx", telemetryWalkedPx)
     .Num("litSeconds", telemetryLitSeconds)
     .Bool("hidden", Character::player && Character::player->isHidden)
     .Bool("paused", IsPlayerInputFrozen())
     .Bool("hot", hot);

    float monsterDist = -1.0f;
    const char* monsterState = TelemetryMonsterState(monsterDist);
    if (monsterState[0] != '\0') {
        f.Str("monster", monsterState).Num("monsterDist", monsterDist);
    }

    Telemetry::Sample(f);
}
