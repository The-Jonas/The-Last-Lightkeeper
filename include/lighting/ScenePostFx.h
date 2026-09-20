#ifndef SCENE_POST_FX_H
#define SCENE_POST_FX_H

#define INCLUDE_SDL
#include "SDL_include.h"

#include "lighting/LightMaskTypes.h"

/// Passagem de pos-processamento em ecra inteiro sobre a textura da cena ja
/// pronta (`renderTarget` do `StageState`).
///
/// O ecra inteiro fica A CORES — e esse o estado normal. O campo de visao
/// distingue-se por outras duas coisas:
///   • FOCO — so o cone e os circulos dos pes ficam nitidos; todo o resto do
///     ecra leva desfoque.
///   • REALCE — dentro do campo de visao o brilho e a saturacao sobem um
///     pouco, para apontar onde o personagem esta a olhar. E um multiplicador:
///     um canto sem luz continua escuro.
/// O antigo filtro preto-e-branco continua no shader mas VEM DESLIGADO
/// (`grayStrength` = 0). Quando se liga, a cor sobrevive dentro do campo de
/// visao e onde chega luz de uma fonte da cena.
/// A dessaturacao real (luma = mistura dos tres canais) e o desfoque sao
/// impossiveis com as chamadas normais do SDL_Renderer, por isso esta classe
/// usa um fragment shader de OpenGL.
///
/// Requisitos: o SDL tem de ter escolhido o backend "opengl" e o driver tem de
/// suportar shaders de GL 2.0. Quando isso falha, `IsAvailable()` devolve false
/// e quem chama mantem o `SDL_RenderCopy` normal — o jogo continua a correr,
/// apenas sem o filtro preto-e-branco.
class ScenePostFx {
public:
    ScenePostFx() = default;
    ~ScenePostFx();

    ScenePostFx(const ScenePostFx&) = delete;
    ScenePostFx& operator=(const ScenePostFx&) = delete;

    /// Compila o shader. Pode ser chamada varias vezes: so tenta uma vez.
    bool Init(SDL_Renderer* renderer);

    bool IsAvailable() const { return ready; }
    /// Texto curto com o motivo de estar (in)disponivel — util no HUD de debug.
    const char* GetStatus() const { return status; }

    /// Modo de mistura que MARCA no canal alfa da cena os pixeis que NAO devem
    /// ficar cinzentos (os dois irmaos). Deixa o RGB intacto e poe alfa a zero
    /// onde o sprite e opaco; o shader le esse alfa e mantem a cor ali.
    /// Devolve SDL_BLENDMODE_INVALID quando o driver nao suporta a mistura.
    static SDL_BlendMode NoGrayStampBlendMode();

    /// Desenha `sceneTex` no alvo atual com o filtro aplicado.
    /// Devolve false quando nada foi desenhado (quem chama tem de fazer o
    /// `SDL_RenderCopy` normal nesse caso).
    bool Render(SDL_Renderer* renderer, SDL_Texture* sceneTex, int windowW, int windowH,
                const PlayerVisionFrame& vision, const PlayerVisionParams& params);

private:
    enum Uniform {
        U_SCENE = 0,
        U_TEX_SCALE,
        U_RESOLUTION,
        U_GRAY_STRENGTH,
        U_MONO_GAIN,
        U_MONO_LIFT,
        U_MONO_TINT,
        U_MONO_TINT_STRENGTH,
        U_LIGHT_SHARPEN,
        U_BLUR_PX,
        U_VIGNETTE_STRENGTH,
        U_VIGNETTE_START,
        U_VISION_ENABLED,
        U_CONE_ORIGIN,
        U_CONE_DIR,
        U_CONE_HALF,
        U_CONE_FEATHER,
        U_CONE_LEN,
        U_CONE_LEN_FEATHER,
        U_FOOT0,
        U_FOOT1,
        U_FOOT_COUNT,
        U_FOOT_RADIUS,
        U_MASK_GAMMA,
        U_VISION_COLOR_GAIN,
        U_FLIP_V,
        U_LIGHTS,
        U_LIGHT_GAMMA,
        U_PLAYER_POS,
        U_UNLIT_FADE_R,
        U_REQUIRE_LIGHT,
        U_CONE_GAMMA,
        U_MONO_HIGHLIGHT,
        U_MONO_LIGHT_GLOW,
        U_LIGHT_COLOR,
        U_VISION_SAT,
        U_COUNT
    };

    bool ready = false;
    bool initTried = false;
    bool flipV = false;
    unsigned int program = 0;
    int uniforms[U_COUNT]{};
    char status[160] = "por iniciar";
};

#endif
