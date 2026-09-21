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

    Uint8 darknessMax = 236;

    /// Escuridao do fundo, onde NAO chega luz nenhuma. E o tecto da malha: nunca
    /// fica mais escuro do que isto. Mais BAIXO = camada escura mais clara, e o
    /// jogador continua a distinguir as formas fora do campo de visao — elas
    /// aparecem a cores mas DESFOCADAS por causa do `ScenePostFx`.
    /// (`darknessMax` acima continua a ser a escuridao na BORDA de cada luz.)
    /// 228 de 255: onde nao chega luz nenhuma fica quase preto. A equipa pediu
    /// escuro depois de jogar (ninguem percebia que era a ESCURIDAO que estava
    /// a matar), e depois pediu um pouco de volta — 238 era escuro de mais.
    Uint8 ambientDarknessMax = 228;
    float falloffRadiusPx = 430.0f;   // alcance de cada luz (subiu de 400)
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
    /// Comprimento maximo da sombra projectada. Encurtado de 600: as sombras
    /// esticavam-se meio ecra e liam-se como manchas, nao como sombras.
    float shadowMaxLengthPx = 460.0f;
    float shadowLengthByLightMul = 1.40f;
    float spriteShadowMinScale = 1.00f;
    /// Quanto a silhueta cresce no extremo do alcance. Baixado de 2.40 pela
    /// mesma razao: uma sombra do dobro do objecto e um borrao.
    float spriteShadowMaxScale = 1.90f;
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

    // ── BRILHO ADITIVO DA CHAMA ─────────────────────────────────────────────
    // E isto que faz uma luz LER-SE COMO AMARELA. A cor da malha de escuridao
    // nao chega: ela vai no vertice, multiplicada pelo proprio alfa da
    // escuridao, portanto apaga-se exatamente no MEIO da luz — onde a escuridao
    // ja abriu e o alfa e quase zero. O disco aditivo por cima nao tem esse
    // problema: soma cor onde a luz e mais forte.
    /// Multiplica o brilho e a opacidade desse disco. Subir = amarelo mais
    /// obvio e diferenca maior entre aceso e apagado.
    float torchGlowStrength = 1.8f;
    /// Raio do disco, em fraccao do raio da luz.
    float torchGlowRadiusScale = 0.62f;
};

/// Campo de visao do personagem controlado: um cone na direcao para onde ele
/// olha mais um circulo pequeno colado nos pes de CADA irmao. Dentro dele a
/// camada escura desaparece, a cor volta e a imagem fica NITIDA.
/// Fora dele a imagem fica DESFOCADA, e so fica a cores onde chega luz de uma
/// fonte da cena (ver `lightColorStrength`); sem luz nenhuma fica cinzenta.
struct PlayerVisionParams {
    bool enabled = true;

    // ── Camera ──────────────────────────────────────────────────────────────
    /// Zoom-base da camera dentro da fase. 1.0 = enquadramento antigo; 0.75
    /// afasta a camera e mostra 33% mais mundo em cada eixo. O campo de visao
    /// mantem o TAMANHO NO ECRA quando isto muda (ver `BuildVisionLights`), por
    /// isso afastar a camera revela mesmo mais mapa em vez de encolher tudo.
    float cameraZoom = 0.75f;

    // ── Cone (valores em pixels de MUNDO; a camera multiplica pelo zoom) ─────
    /// 45 graus de MEIO-angulo = 90 graus de abertura total. Comecou em 160 e
    /// era largo de mais: via-se quase o andar inteiro de uma vez.
    float coneHalfAngleDeg = 45.0f;
    /// Largura da borda difusa nos LADOS do cone. Baixo = risco recto e seco.
    /// Nao poe a zero: 1 a 2 graus servem de anti-serrilhado.
    float coneFeatherDeg = 1.5f;
    /// Alcance do cone. Encurtado de 1250: com o cone estreito, um alcance
    /// enorme dava um corredor de luz ate ao outro lado do mapa.
    float coneLengthPx = 850.0f;
    /// Largura da borda difusa na PONTA do cone.
    float coneLengthFeatherPx = 60.0f;
    /// Dureza do cone POR DENTRO. O peso de um ponto e 1 - t^gamma, com t a
    /// crescer do apice ate a borda. Gamma alto = o interior fica todo com o
    /// mesmo valor e so corta mesmo na borda (aspeto de recorte); gamma baixo
    /// = o cone escurece aos poucos desde o meio (aspeto de facho difuso).
    float coneEdgeGamma = 12.0f;
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

    // ── PRECISA DE LUZ PARA VER ─────────────────────────────────────────────
    // O cone diz ONDE o jogador olha; a luz diz o QUE ele consegue distinguir
    // ali. As duas coisas multiplicam-se: sem luz, o fundo do cone fica em
    // preto-e-branco e os objetos apagam-se.
    /// Liga a regra. Desligado = comportamento antigo (cone todo colorido, e os
    /// objetos aparecem so por estarem dentro do cone).
    bool requireLightToSee = true;
    /// Multiplica o alcance de cada fonte quando se pergunta "chega luz aqui?".
    /// Abaixo de 1 encolhe a bolha de cor a volta de cada luz.
    float lightReachScale = 1.0f;
    /// Curva da luz percebida: com t = distancia/alcance, o peso e 1 - t^gamma.
    /// Alto = a luz vale quase toda ate perto da borda e cai de repente.
    float lightPerceptionGamma = 2.6f;
    /// Sem nenhuma luz, um objeto ainda aparece por estar PERTO. A opacidade
    /// cai LINEARMENTE de 1 (colado ao personagem) ate 0 a esta distancia
    /// (px de MUNDO). E a mesma rampa que devolve a cor no inicio do cone.
    float unlitFadeDistancePx = 300.0f;

    // ── Pos-processamento monocromatico (fora do campo de visao) ────────────
    /// DESLIGADO POR OMISSAO. O ecra inteiro fica a cores; o que aponta o campo
    /// de visao e o FOCO (`outsideBlurPx`) mais o REALCE (`visionColorGain` e
    /// `visionSaturation`). Suba para 1 para ter o preto-e-branco de volta fora
    /// do campo de visao e longe de qualquer luz.
    float grayStrength = 0.0f;
    /// Brilho da camada monocromatica. Abaixo de 1 escurece-a.
    float monoGain = 0.52f;
    /// Levanta o preto da camada monocromatica (0..0.25).
    float monoLift = 0.01f;
    /// Tom frio aplicado ao cinzento (luar). 0 = cinzento puro.
    float monoTintStrength = 0.30f;
    float monoTintR = 0.78f;
    float monoTintG = 0.86f;
    float monoTintB = 1.00f;
    /// DESFOQUE FORA DO CAMPO DE VISAO. Raio, em px de ecra, do borrao aplicado
    /// a tudo o que fica fora do cone e dos circulos dos pes. Dentro do campo de
    /// visao a imagem fica sempre nitida — e essa a unica zona em foco.
    /// Substitui o antigo mosaico de "pixels grandes". 0 = sem desfoque.
    float outsideBlurPx = 4.0f;
    /// As FONTES DE LUZ vistas atraves da camada monocromatica. Sem isto uma
    /// vela ao longe fica quase invisivel, porque `monoGain` baixa a camada
    /// toda por igual. Estes dois so levantam o que ja e claro:
    /// realce = quanto sobem os pixeis JA claros da imagem (chama, halo);
    /// halo   = quanto sobe a camada a volta de cada fonte de luz da cena.
    /// Ambos entram DEPOIS da vinheta, para uma luz junto a borda do ecra nao
    /// ser apagada por ela.
    float monoHighlightGain = 0.55f;
    float monoLightGlow = 0.35f;
    /// Vinheta: a camada monocromatica escurece na direcao das bordas do ecra.
    float vignetteStrength = 0.85f;
    /// Onde a vinheta comeca (0 = centro do ecra, 1 = canto).
    float vignetteStart = 0.22f;
    // ── REALCE DO CAMPO DE VISAO ────────────────────────────────────────────
    // Com o cinzento desligado, e isto que diz ao jogador para onde o
    // personagem esta a olhar, junto com o foco. Os dois valores MULTIPLICAM o
    // que ja esta no ecra, por isso um canto sem luz continua escuro: o realce
    // aponta, nao revela.
    /// Brilho dentro do campo de visao. 1.00 = sem realce.
    float visionColorGain = 1.18f;
    /// Saturacao dentro do campo de visao. 1.00 = sem mudanca; acima de 1 as
    /// cores ficam mais cheias do que no resto do ecra.
    float visionSaturation = 1.15f;

    // ── A LUZ DEVOLVE A COR ─────────────────────────────────────────────────
    // Onde CHEGA LUZ a imagem fica a cores, mesmo fora do campo de visao: uma
    // sala com um castiçal aceso le-se a cores mesmo quando o personagem olha
    // para outro lado. O cinzento passa a ser so o sinal de "aqui nao ha luz".
    // A bolha de cor usa o MESMO alcance e a MESMA curva da luz percebida
    // (`lightReachScale`, `lightPerceptionGamma`).
    /// Quanto da cor volta onde ha luz. 0 = tudo cinzento fora do campo de
    /// visao (comportamento antigo); 1 = a cor volta a par da intensidade.
    float lightColorStrength = 1.0f;
    /// A luz tambem FOCA. Por omissao a luz devolve so a COR: uma zona
    /// iluminada fora do campo de visao fica a cores mas desfocada, porque o
    /// personagem nao esta a olhar para la. 1 = a luz devolve tambem a nitidez.
    float lightSharpenStrength = 0.0f;

    // ── O que desaparece fora do campo de visao ─────────────────────────────
    // Tres categorias separadas: barris grandes a sumir podem atrapalhar o
    // movimento, por isso da para os deixar visiveis sem perder o efeito nos
    // objetos pequenos.
    /// Itens apanhaveis do chao (`ItemPickup`).
    bool hideItemsOutsideVision = true;
    /// Jornais, castiçais, radio, reparaveis, janelas e armarios.
    bool hideInteractablesOutsideVision = true;
    /// Barris e caixas que se empurram (`Box`).
    /// Os BARRIS ficam sempre a vista, como o resto do cenario. Sumirem e
    /// voltarem conforme o cone passava fazia o andar parecer instavel — e um
    /// barril e mobilia, nao uma pista escondida.
    bool hidePushablesOutsideVision = false;
    /// O MONSTRO. Ao contrario de tudo o resto, ele nao aparece so por estar
    /// perto: tem de estar ao mesmo tempo DENTRO do campo de visao e DENTRO de
    /// luz a serio (uma vela, a lanterna na mao). A rampa de proximidade nao
    /// conta — na escuridao ele pode estar colado ao jogador e continuar
    /// invisivel. Desligar isto devolve o comportamento antigo (sempre a vista).
    bool hideMonsterOutsideLight = true;
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
    /// So os CIRCULOS DOS PES a usam — eles sao luz e querem borda macia.
    float maskGamma = 4.0f;
    /// Copia de `PlayerVisionParams::coneEdgeGamma`. So o CONE a usa, para
    /// poder ter borda seca sem endurecer tambem os circulos dos pes.
    float coneGamma = 12.0f;

    // ── LUZ REAL DISPONIVEL NESTE FRAME ─────────────────────────────────────
    // Cada fonte de luz da cena reduzida a um CIRCULO (centro de tela + raio +
    // intensidade). Serve para responder "chega luz a este pixel?" tanto no C++
    // (opacidade dos objetos) como no shader (filtro preto-e-branco), com a
    // mesma conta nos dois lados. Preenchido por `StageState::BuildVisionLights`.
    static constexpr int kMaxLightSamples = 8;
    float lightX[kMaxLightSamples] = {0.0f};
    float lightY[kMaxLightSamples] = {0.0f};
    float lightR[kMaxLightSamples] = {1.0f};
    float lightI[kMaxLightSamples] = {0.0f};
    int lightCount = 0;
    float lightGamma = 2.6f;

    /// Pes do personagem controlado, em coordenadas de tela. Origem da rampa
    /// linear de proximidade.
    float playerX = 0.0f;
    float playerY = 0.0f;
    /// Raio dessa rampa, ja em px de TELA.
    float unlitFadeRadiusPx = 300.0f;
    /// Copia de `PlayerVisionParams::requireLightToSee`.
    bool requireLight = true;
};

#endif
