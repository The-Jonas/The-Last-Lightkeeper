#ifndef LIGHT_MASK_TYPES_H
#define LIGHT_MASK_TYPES_H

#define INCLUDE_SDL
#include "SDL_include.h"

#include <cstdint>
#include <vector>

enum class LightFalloffCurve { Smoothstep, Power };

enum class LightMaskShape { Circle, Ellipse, Cone, SoftRect, Torch };

struct LightOcclusionContext {
    const std::vector<std::uint8_t>* solidGrid = nullptr;
    int mapWidth = 0;
    int mapHeight = 0;
    float tileWidth = 0.0f;
    float tileHeight = 0.0f;
    float mapOriginX = 0.0f;
    float mapOriginY = 0.0f;
    float cameraX = 0.0f;
    float cameraY = 0.0f;
    float zoom = 1.0f;

    bool IsEnabled() const {
        return solidGrid != nullptr && !solidGrid->empty() && mapWidth > 0 && mapHeight > 0;
    }
};

enum class LightQualityPreset { Custom, Quality, Balanced, Performance };

struct LightMaskParams {
    /// Balanced defaults; set Custom to use only the raw fields below without preset clamps.
    LightQualityPreset lightQualityPreset = LightQualityPreset::Balanced;

    Uint8 darknessMax = 250;

    /// Escuridao do fundo, onde NAO chega luz nenhuma. E o tecto da malha: nunca
    /// fica mais escuro do que isto. Mais BAIXO = camada escura mais clara, e o
    /// jogador continua a distinguir as formas fora do campo de visao — elas
    /// ficam em preto-e-branco por causa do `ScenePostFx`.
    /// (`darknessMax` acima continua a ser a escuridao na BORDA de cada luz.)
    Uint8 ambientDarknessMax = 200;
    float falloffRadiusPx = 400.0f;
    float fatorDicaDeRaio = 1.2f;
    LightFalloffCurve falloffCurve = LightFalloffCurve::Smoothstep;
    float falloffGamma = 2.0f;
    float innerLift = 0.0f;
    int numRings = 48;
    int numSeg = 48;
    float marginExtraAlemDoCanto = 4.0f;
    float rMaxMinimo = 8.0f;

    float ellipseAspect = 1.0f;

    float coneHalfAngleDeg = 35.0f;
    float coneLengthPx = 280.0f;
    float coneAxisDeg = -90.0f;
    bool coneFollowMouse = false;
    /// Largura da borda suave do cone. 0 = automatico (o valor antigo:
    /// 18% do meio-angulo / 14% do comprimento).
    float coneFeatherDeg = 0.0f;
    float coneLengthFeatherPx = 0.0f;

    float rectHalfWidthPx = 140.0f;
    float rectHalfHeightPx = 100.0f;
    float rectSoftBandPx = 72.0f;

    float shadowCastDistanceMul = 1.62f;
    float shadowMaxLengthPx = 600.0f;
    float shadowLengthByLightMul = 1.40f;
    float spriteShadowMinScale = 1.00f;
    float spriteShadowMaxScale = 2.40f;
    float shadowSoftness = 0.0f;
    int shadowSoftLayers = 1;
    float lightTemporalSmoothing = 0.20f;
    /// Screen-space sampling step for the analytical radial darkness mesh (larger = faster).
    float lightGridStepPx = 24.0f;
    /// When true, the overlay is rendered to a half-resolution texture and upscaled (much cheaper).
    bool radialMaskHalfResolution = true;

    float torchAnimSpeed = 1.0f;
    float torchMotionRangePx = 9.0f;
    float torchWarpStrength = 0.40f;
    float torchPulseStrength = 0.30f;
    float torchColorWarmth = 2.0f;
    float torchColorStrength = 1.0f;
};

/// Campo de visao do personagem controlado: um cone na direcao para onde ele
/// olha mais um circulo pequeno colado nos pes de CADA irmao. Dentro dele a
/// camada escura desaparece por completo e a cor volta; fora dele a cena fica
/// monocromatica (mais clara onde ha luz, mas nunca colorida).
struct PlayerVisionParams {
    bool enabled = true;

    // ── Cone (valores em pixels de MUNDO; a camera multiplica pelo zoom) ─────
    /// 80 graus de MEIO-angulo = 160 graus de abertura total.
    float coneHalfAngleDeg = 80.0f;
    float coneFeatherDeg = 11.0f;
    /// Longo o bastante para varrer a largura toda do ecra a zoom normal.
    float coneLengthPx = 1250.0f;
    float coneLengthFeatherPx = 280.0f;
    /// Empurra o apice do cone para a frente do corpo (evita ver "atras de si").
    float coneOriginForwardPx = 4.0f;
    /// Velocidade de rotacao do eixo do cone ao trocar de direcao (graus/s).
    float turnSpeedDegPerSec = 720.0f;

    // ── Circulo dos pes (visao periferica) ───────────────────────────────────
    // Existe nos DOIS irmaos, controlado ou nao, e produz LUZ A SERIO: clareia
    // por completo, ao contrario do cone (ver `unlitVisionDarkness`).
    // A suavidade da borda vem de `maskFalloffGamma`, igual a malha de escuridao.
    float footRadiusPx = 170.0f;

    // ── Mascara de escuridao ────────────────────────────────────────────────
    /// Curva do recorte na malha de escuridao. Alto = interior totalmente limpo
    /// e queda rapida so na borda.
    float maskFalloffGamma = 4.0f;

    /// Quanto da escuridao ambiente SOBRA dentro do cone quando nao ha nenhuma
    /// luz por perto. O cone e VISAO, nao e uma lanterna: sem uma fonte de luz o
    /// jogador distingue as formas mas o sitio continua escuro. Uma luz real
    /// (isqueiro, castiçal, candeeiro) clareia por cima disto, porque a malha
    /// fica sempre com o valor MAIS CLARO entre todas as fontes.
    /// 0 = o cone clareia tudo sozinho; 1 = o cone nao clareia nada.
    float unlitVisionDarkness = 0.45f;

    // ── Pos-processamento monocromatico (fora do campo de visao) ────────────
    /// 1 = preto-e-branco total fora do campo de visao.
    float grayStrength = 1.0f;
    /// Brilho da camada monocromatica. Abaixo de 1 escurece-a.
    float monoGain = 0.52f;
    /// Levanta o preto da camada monocromatica (0..0.25).
    float monoLift = 0.01f;
    /// Tom frio aplicado ao cinzento (luar). 0 = cinzento puro.
    float monoTintStrength = 0.30f;
    float monoTintR = 0.78f;
    float monoTintG = 0.86f;
    float monoTintB = 1.00f;
    /// Lado do "pixel" do mosaico aplicado SO a camada monocromatica (px de ecra).
    float monoPixelSizePx = 5.0f;
    /// Raio do desfoque da camada monocromatica (px de ecra). 0 = sem desfoque.
    float monoBlurPx = 3.0f;
    /// Vinheta: a camada monocromatica escurece na direcao das bordas do ecra.
    float vignetteStrength = 0.85f;
    /// Onde a vinheta comeca (0 = centro do ecra, 1 = canto).
    float vignetteStart = 0.22f;
    /// Ganho de brilho da cor DENTRO do campo de visao.
    float visionColorGain = 1.06f;

    // ── O que desaparece fora do campo de visao ─────────────────────────────
    // Tres categorias separadas: barris grandes a sumir podem atrapalhar o
    // movimento, por isso da para os deixar visiveis sem perder o efeito nos
    // objetos pequenos.
    /// Itens apanhaveis do chao (`ItemPickup`).
    bool hideItemsOutsideVision = true;
    /// Jornais, castiçais, radio, reparaveis, janelas e armarios.
    bool hideInteractablesOutsideVision = true;
    /// Barris e caixas que se empurram (`Box`).
    bool hidePushablesOutsideVision = true;
    /// A partir de que fraccao do campo de visao o objeto ja aparece por completo.
    float itemRevealThreshold = 0.35f;
};

/// Resultado por frame do campo de visao, ja em coordenadas de TELA (y para
/// baixo) e com o zoom da camera aplicado. Preenchido por
/// `StageState::UpdatePlayerVision`; consumido pela malha de escuridao, pelo
/// `ScenePostFx` e pelo corte dos itens do chao.
struct PlayerVisionFrame {
    bool valid = false;
    float coneX = 0.0f;
    float coneY = 0.0f;
    float dirX = 1.0f;
    float dirY = 0.0f;
    float halfAngleRad = 0.6f;
    float featherRad = 0.15f;
    float lengthPx = 1250.0f;
    float lengthFeatherPx = 280.0f;
    /// Ate dois circulos de pes: o irmao controlado e o companheiro.
    static constexpr int kMaxFeet = 2;
    float footX[kMaxFeet] = {0.0f, 0.0f};
    float footY[kMaxFeet] = {0.0f, 0.0f};
    int footCount = 0;
    float footRadiusPx = 110.0f;
    /// Copia de `PlayerVisionParams::maskFalloffGamma`, para o shader usar a
    /// MESMA curva que a malha de escuridao (senao aparece um anel na borda).
    float maskGamma = 4.0f;
};

#endif
