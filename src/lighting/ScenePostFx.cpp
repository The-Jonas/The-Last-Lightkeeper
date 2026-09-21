#include "lighting/ScenePostFx.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace {

// ── Ligacao minima ao OpenGL ────────────────────────────────────────────────
// Nao incluimos <GL/gl.h> de proposito: os tipos, as constantes e os ponteiros
// de funcao ficam todos declarados aqui, e o SDL resolve os simbolos em tempo
// de execucao (SDL_GL_GetProcAddress). Assim o makefile nao precisa de -lopengl32
// nem de headers de GL em nenhuma das plataformas.

#if defined(_WIN32)
#define TLL_GLAPI __stdcall
#else
#define TLL_GLAPI
#endif

typedef unsigned int GLenum_;
typedef unsigned int GLuint_;
typedef int GLint_;
typedef int GLsizei_;
typedef char GLchar_;
typedef float GLfloat_;
typedef unsigned char GLboolean_;

constexpr GLenum_ kGL_FRAGMENT_SHADER = 0x8B30;
constexpr GLenum_ kGL_VERTEX_SHADER = 0x8B31;
constexpr GLenum_ kGL_COMPILE_STATUS = 0x8B81;
constexpr GLenum_ kGL_LINK_STATUS = 0x8B82;
constexpr GLenum_ kGL_CURRENT_PROGRAM = 0x8B8D;
constexpr GLenum_ kGL_BLEND = 0x0BE2;
constexpr GLenum_ kGL_TRIANGLE_FAN = 0x0006;

struct GLFns {
    GLuint_ (TLL_GLAPI* CreateShader)(GLenum_) = nullptr;
    void (TLL_GLAPI* ShaderSource)(GLuint_, GLsizei_, const GLchar_* const*, const GLint_*) = nullptr;
    void (TLL_GLAPI* CompileShader)(GLuint_) = nullptr;
    void (TLL_GLAPI* GetShaderiv)(GLuint_, GLenum_, GLint_*) = nullptr;
    void (TLL_GLAPI* GetShaderInfoLog)(GLuint_, GLsizei_, GLsizei_*, GLchar_*) = nullptr;
    GLuint_ (TLL_GLAPI* CreateProgram)() = nullptr;
    void (TLL_GLAPI* AttachShader)(GLuint_, GLuint_) = nullptr;
    void (TLL_GLAPI* LinkProgram)(GLuint_) = nullptr;
    void (TLL_GLAPI* GetProgramiv)(GLuint_, GLenum_, GLint_*) = nullptr;
    void (TLL_GLAPI* GetProgramInfoLog)(GLuint_, GLsizei_, GLsizei_*, GLchar_*) = nullptr;
    void (TLL_GLAPI* DeleteShader)(GLuint_) = nullptr;
    void (TLL_GLAPI* DeleteProgram)(GLuint_) = nullptr;
    void (TLL_GLAPI* UseProgram)(GLuint_) = nullptr;
    GLint_ (TLL_GLAPI* GetUniformLocation)(GLuint_, const GLchar_*) = nullptr;
    void (TLL_GLAPI* Uniform1i)(GLint_, GLint_) = nullptr;
    void (TLL_GLAPI* Uniform1f)(GLint_, GLfloat_) = nullptr;
    void (TLL_GLAPI* Uniform2f)(GLint_, GLfloat_, GLfloat_) = nullptr;
    void (TLL_GLAPI* Uniform3f)(GLint_, GLfloat_, GLfloat_, GLfloat_) = nullptr;
    void (TLL_GLAPI* Uniform4fv)(GLint_, GLsizei_, const GLfloat_*) = nullptr;
    void (TLL_GLAPI* GetIntegerv)(GLenum_, GLint_*) = nullptr;
    GLboolean_ (TLL_GLAPI* IsEnabled)(GLenum_) = nullptr;
    void (TLL_GLAPI* Enable)(GLenum_) = nullptr;
    void (TLL_GLAPI* Disable)(GLenum_) = nullptr;
    void (TLL_GLAPI* Begin)(GLenum_) = nullptr;
    void (TLL_GLAPI* End)() = nullptr;
    void (TLL_GLAPI* TexCoord2f)(GLfloat_, GLfloat_) = nullptr;
    void (TLL_GLAPI* Vertex2f)(GLfloat_, GLfloat_) = nullptr;

    bool loaded = false;
};

GLFns g_gl;

template <typename T>
bool LoadFn(T& slot, const char* name) {
    void* p = SDL_GL_GetProcAddress(name);
    slot = reinterpret_cast<T>(p);
    return p != nullptr;
}

/// Carrega tudo o que a passagem usa. Devolve o nome da PRIMEIRA funcao em
/// falta (nullptr quando esta tudo presente).
const char* LoadGLFunctions() {
    if (g_gl.loaded) {
        return nullptr;
    }
#define TLL_LOAD(field, name)                                                                                          \
    if (!LoadFn(g_gl.field, name)) {                                                                                   \
        return name;                                                                                                   \
    }
    TLL_LOAD(CreateShader, "glCreateShader")
    TLL_LOAD(ShaderSource, "glShaderSource")
    TLL_LOAD(CompileShader, "glCompileShader")
    TLL_LOAD(GetShaderiv, "glGetShaderiv")
    TLL_LOAD(GetShaderInfoLog, "glGetShaderInfoLog")
    TLL_LOAD(CreateProgram, "glCreateProgram")
    TLL_LOAD(AttachShader, "glAttachShader")
    TLL_LOAD(LinkProgram, "glLinkProgram")
    TLL_LOAD(GetProgramiv, "glGetProgramiv")
    TLL_LOAD(GetProgramInfoLog, "glGetProgramInfoLog")
    TLL_LOAD(DeleteShader, "glDeleteShader")
    TLL_LOAD(DeleteProgram, "glDeleteProgram")
    TLL_LOAD(UseProgram, "glUseProgram")
    TLL_LOAD(GetUniformLocation, "glGetUniformLocation")
    TLL_LOAD(Uniform1i, "glUniform1i")
    TLL_LOAD(Uniform1f, "glUniform1f")
    TLL_LOAD(Uniform2f, "glUniform2f")
    TLL_LOAD(Uniform3f, "glUniform3f")
    TLL_LOAD(Uniform4fv, "glUniform4fv")
    TLL_LOAD(GetIntegerv, "glGetIntegerv")
    TLL_LOAD(IsEnabled, "glIsEnabled")
    TLL_LOAD(Enable, "glEnable")
    TLL_LOAD(Disable, "glDisable")
    TLL_LOAD(Begin, "glBegin")
    TLL_LOAD(End, "glEnd")
    TLL_LOAD(TexCoord2f, "glTexCoord2f")
    TLL_LOAD(Vertex2f, "glVertex2f")
#undef TLL_LOAD
    g_gl.loaded = true;
    return nullptr;
}

// ── Shader ──────────────────────────────────────────────────────────────────
// GLSL 1.10 de proposito: o renderer de OpenGL do SDL cria um contexto de
// compatibilidade, por isso `gl_Vertex` / `gl_MultiTexCoord0` / `gl_TexCoord`
// existem e o quad pode ir em modo imediato (4 vertices, uma vez por frame).

const char* kVertexSrc =
    "#version 110\n"
    "void main() {\n"
    "    gl_Position = gl_Vertex;\n"
    "    gl_TexCoord[0] = gl_MultiTexCoord0;\n"
    "}\n";

const char* kFragmentSrc =
    "#version 110\n"
    "uniform sampler2D uScene;\n"
    "uniform vec2  uTexScale;\n"
    "uniform vec2  uResolution;\n"
    "uniform float uGrayStrength;\n"
    "uniform float uMonoGain;\n"
    "uniform float uMonoLift;\n"
    "uniform vec3  uMonoTint;\n"
    "uniform float uMonoTintStrength;\n"
    "uniform float uLightSharpen;\n"
    "uniform float uBlurPx;\n"
    "uniform float uVignetteStrength;\n"
    "uniform float uVignetteStart;\n"
    "uniform float uVisionEnabled;\n"
    "uniform vec2  uConeOrigin;\n"
    "uniform vec2  uConeDir;\n"
    "uniform float uConeHalf;\n"
    "uniform float uConeFeather;\n"
    "uniform float uConeLen;\n"
    "uniform float uConeLenFeather;\n"
    "uniform vec2  uFoot0;\n"
    "uniform vec2  uFoot1;\n"
    "uniform float uFootCount;\n"
    "uniform float uFootRadius;\n"
    "uniform float uMaskGamma;\n"
    "uniform float uConeGamma;\n"
    "uniform float uMonoHighlight;\n"
    "uniform float uMonoLightGlow;\n"
    "uniform float uLightColor;\n"
    /* Luzes REAIS da cena, reduzidas a circulos: xy = centro no ecra,
       z = alcance em px, w = intensidade 0..1. As ranhuras que sobram vem com
       w = 0 e nao pesam, por isso o ciclo nao precisa de contador nem de break
       (GLSL 1.10 e esquisito com ciclos de limite variavel). */
    "uniform vec4  uLights[8];\n"
    "uniform float uLightGamma;\n"
    "uniform vec2  uPlayerPos;\n"
    "uniform float uUnlitFadeR;\n"
    "uniform float uRequireLight;\n"
    "uniform float uVisionColorGain;\n"
    "uniform float uVisionSat;\n"
    "uniform float uFlipV;\n"
    "\n"
    "/* Converte uma posicao logica de ecra (y para baixo) de volta para UV da\n"
    "   textura da cena, respeitando a mesma inversao vertical do quad. */\n"
    "vec2 screenToUv(vec2 sp) {\n"
    "    vec2 n = clamp(sp / uResolution, 0.0, 1.0);\n"
    "    return vec2(n.x, uFlipV > 0.5 ? (1.0 - n.y) : n.y) * uTexScale;\n"
    "}\n"
    "\n"
    "/* DESFOQUE. 17 amostras em dois aneis a volta do ponto, com pesos de\n"
    "   gaussiana. Substitui o antigo mosaico de pixels grandes: a leitura fica\n"
    "   macia (fora de foco) em vez de grosseira. `r` e o raio em px de ecra. */\n"
    "vec3 blurAt(vec2 sp, float r) {\n"
    "    vec3 acc = texture2D(uScene, screenToUv(sp)).rgb * 0.1476;\n"
    "    for (int i = 0; i < 8; i++) {\n"
    "        float a = float(i) * 0.7853981634;\n"
    "        vec2 d = vec2(cos(a), sin(a));\n"
    "        acc += texture2D(uScene, screenToUv(sp + d * r * 0.55)).rgb * 0.0662;\n"
    "        acc += texture2D(uScene, screenToUv(sp + d * r)).rgb * 0.0403;\n"
    "    }\n"
    "    return acc;\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec2 uv = gl_TexCoord[0].xy;\n"
    "    vec4 src = texture2D(uScene, uv * uTexScale);\n"
    "\n"
    "    /* O quad cobre exatamente o viewport que o SDL montou, por isso a UV DA a\n"
    "       posicao logica no ecra (0..uResolution, y para baixo). Usar a UV em vez\n"
    "       de gl_FragCoord mantem o efeito certo com o letterbox do\n"
    "       SDL_RenderSetLogicalSize e com ecras HiDPI. */\n"
    "    vec2 p = vec2(uv.x, uFlipV > 0.5 ? (1.0 - uv.y) : uv.y) * uResolution;\n"
    "\n"
    "    /* `visGeo` repete PASSO A PASSO a conta que a malha de escuridao faz para\n"
    "       as mesmas formas (RadialLightOverlay::AlphaAt, curva Power, innerLift\n"
    "       zero). E o CAMPO DE VISAO puro: para onde o personagem esta virado.\n"
    "       `vis` e o mesmo depois de multiplicado pela luz disponivel. */\n"
    "    float visGeo = 0.0;\n"
    "    float vis = 0.0;\n"
    "    float lightAmt = 0.0;\n"
    "    float sceneLight = 0.0;\n"
    "    if (uVisionEnabled > 0.5) {\n"
    "        float foot = 1.0 - pow(clamp(length(p - uFoot0) / uFootRadius, 0.0, 1.0), uMaskGamma);\n"
    "        if (uFootCount > 1.5) {\n"
    "            float f1 = 1.0 - pow(clamp(length(p - uFoot1) / uFootRadius, 0.0, 1.0), uMaskGamma);\n"
    "            foot = max(foot, f1);\n"
    "        }\n"
    "\n"
    "        vec2 d = p - uConeOrigin;\n"
    "        float fwd = dot(d, uConeDir);\n"
    "        float perp = dot(d, vec2(-uConeDir.y, uConeDir.x));\n"
    "        float cone = 0.0;\n"
    "        if (fwd > 0.0 && fwd <= uConeLen) {\n"
    "            float angAbs = abs(atan(perp, fwd));\n"
    "            if (angAbs <= uConeHalf + uConeFeather) {\n"
    "                float t = max(min(1.0, fwd / uConeLen), min(1.0, angAbs / uConeHalf));\n"
    "                float angFade = clamp((uConeHalf + uConeFeather - angAbs) / uConeFeather, 0.0, 1.0);\n"
    "                float lenFade = clamp((uConeLen - fwd) / uConeLenFeather, 0.0, 1.0);\n"
    "                cone = (1.0 - pow(t, uConeGamma)) * angFade * lenFade;\n"
    "            }\n"
    "        }\n"
    "        visGeo = clamp(max(foot, cone), 0.0, 1.0);\n"
    "\n"
    "        /* O cone e VISAO, nao e lanterna: diz para onde o jogador olha, nao\n"
    "           acende nada. `lightAmt` e a claridade que chega a este pixel — os\n"
    "           circulos dos pes mais qualquer fonte da cena.\n"
    "           MESMA CONTA que StageState::LightAmountAtScreen. */\n"
    "        lightAmt = foot;\n"
    "        /* `sceneLight` conta SO as fontes de verdade (sem os pes). E ele que\n"
    "           devolve a COR fora do campo de visao: onde ha luz, ha cor. */\n"
    "        for (int i = 0; i < 8; i++) {\n"
    "            vec4 L = uLights[i];\n"
    "            float lt = clamp(length(p - L.xy) / max(1.0, L.z), 0.0, 1.0);\n"
    "            float w = L.w * (1.0 - pow(lt, uLightGamma));\n"
    "            lightAmt = max(lightAmt, w);\n"
    "            sceneLight = max(sceneLight, w);\n"
    "        }\n"
    "        vis = visGeo;\n"
    "        if (uRequireLight > 0.5) {\n"
    "            float prox = clamp(1.0 - length(p - uPlayerPos) / max(1.0, uUnlitFadeR), 0.0, 1.0);\n"
    "            vis = clamp(visGeo * clamp(max(lightAmt, prox), 0.0, 1.0), 0.0, 1.0);\n"
    "        }\n"
    "    }\n"
    "\n"
    "    /* Os dois irmaos ficam marcados com alfa ZERO no alvo da cena (ver\n"
    "       NoGrayStampBlendMode). Onde ha marca a imagem fica sempre nitida. */\n"
    "    float stamp = clamp(1.0 - src.a, 0.0, 1.0);\n"
    "\n"
    "    /* O carimbo dos IRMAOS carrega um numero, nao um sim/nao: e a LUZ que\n"
    "       lhes chega (0 = nenhuma, 1 = dentro de uma luz). O cenario chega aqui\n"
    "       com alfa 1.0 exacto, por isso qualquer alfa abaixo disso e um irmao —\n"
    "       inclusive o irmao SEM luz nenhuma, que e carimbado com 254/255.\n"
    "       `isChar` mantem-nos sempre NITIDOS (nunca desfocados, nunca\n"
    "       pixelizados) e `charLight` decide quanta COR lhes volta. */\n"
    "    float isChar = step(0.0005, stamp);\n"
    "    float charLight = clamp(stamp / 0.996, 0.0, 1.0);\n"
    "\n"
    "    /* FOCO: so o campo de visao fica nitido; todo o resto do ecra leva\n"
    "       desfoque, esteja iluminado ou nao. E este o sinal principal de para\n"
    "       onde o personagem esta a olhar. A luz devolve a nitidez apenas se\n"
    "       `uLightSharpen` estiver acima de zero (vem desligado). */\n"
    "    float sharpAmt = clamp(max(max(visGeo, sceneLight * uLightSharpen), isChar), 0.0, 1.0);\n"
    "\n"
    "    /* O ramo depende de um UNIFORME (fluxo uniforme), por isso e seguro ter\n"
    "       amostras de textura la dentro. */\n"
    "    vec3 base = src.rgb;\n"
    "    if (uBlurPx > 0.01) {\n"
    "        base = mix(blurAt(p, uBlurPx), src.rgb, sharpAmt);\n"
    "    }\n"
    "\n"
    "    /* CINZENTO — DESLIGADO POR OMISSAO (`uGrayStrength` = 0). O ecra inteiro\n"
    "       fica a cores, tal como o campo de visao. O que separa o dentro do fora\n"
    "       passou a ser o FOCO e o REALCE, nao a falta de cor. Suba a barra\n"
    "       (PB: forca do cinzento) para ter o preto-e-branco de volta: nessa\n"
    "       altura a cor sobrevive dentro do campo de visao e onde chega luz. */\n"
    "    vec3 col = base;\n"
    "    if (uGrayStrength > 0.001) {\n"
    "        /* COR: volta dentro do campo de visao E onde chega luz de uma fonte\n"
    "           da cena, para o cinzento querer dizer so uma coisa: nao ha luz. */\n"
    "        float keepColor = clamp(max(max(vis, sceneLight * uLightColor), charLight), 0.0, 1.0);\n"
    "\n"
    "        float luma = dot(base, vec3(0.299, 0.587, 0.114));\n"
    "        vec3 mono = mix(vec3(luma), uMonoTint * luma, uMonoTintStrength) * uMonoGain;\n"
    "        mono = clamp(mono + vec3(uMonoLift), 0.0, 1.0);\n"
    "\n"
    "        /* Vinheta: quanto mais perto da borda do ecra, mais escura a camada. */\n"
    "        vec2 c = (p / uResolution - 0.5) * 2.0;\n"
    "        float aspect = uResolution.x / max(1.0, uResolution.y);\n"
    "        c.x *= aspect;\n"
    "        float r = length(c) / length(vec2(aspect, 1.0));\n"
    "        mono *= 1.0 - uVignetteStrength * smoothstep(uVignetteStart, 1.0, r);\n"
    "\n"
    "        /* AS LUZES VISTAS ATRAVES DO CINZENTO. `uMonoGain` baixa a camada\n"
    "           toda por igual, e por isso uma vela ao longe quase desaparecia.\n"
    "           Estes dois termos SOMAM-SE depois da vinheta e so levantam o que ja\n"
    "           e claro — o escuro fica escuro. */\n"
    "        float hi = pow(clamp(luma, 0.0, 1.0), 2.2);\n"
    "        mono = clamp(mono + vec3(uMonoHighlight * hi + uMonoLightGlow * lightAmt * hi), 0.0, 1.0);\n"
    "\n"
    "        col = mix(mix(base, mono, clamp(uGrayStrength, 0.0, 1.0)), base, keepColor);\n"
    "    }\n"
    "\n"
    "    /* REALCE DO CAMPO DE VISAO. Nao e uma lanterna: e so um levantar de\n"
    "       brilho e de saturacao dentro do cone e dos circulos dos pes, para o\n"
    "       jogador saber para onde esta a olhar. MULTIPLICA o que ja la esta, por\n"
    "       isso um canto sem luz nenhuma continua escuro — o realce nao revela\n"
    "       nada, so aponta. Com `uVisionSat` acima de 1 as cores dentro da visao\n"
    "       ficam mais cheias (a mistura extrapola a partir do cinzento). */\n"
    "    float lum = dot(col, vec3(0.299, 0.587, 0.114));\n"
    "    vec3 hl = clamp(mix(vec3(lum), col, uVisionSat) * uVisionColorGain, 0.0, 1.0);\n"
    "    gl_FragColor = vec4(mix(col, hl, clamp(visGeo, 0.0, 1.0)), 1.0);\n"
    "}\n";

GLuint_ CompileStage(GLenum_ kind, const char* src, char* err, size_t errSize) {
    const GLuint_ sh = g_gl.CreateShader(kind);
    if (sh == 0) {
        std::snprintf(err, errSize, "glCreateShader falhou");
        return 0;
    }
    const GLint_ len = static_cast<GLint_>(std::strlen(src));
    g_gl.ShaderSource(sh, 1, &src, &len);
    g_gl.CompileShader(sh);
    GLint_ ok = 0;
    g_gl.GetShaderiv(sh, kGL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        log[0] = 0;
        g_gl.GetShaderInfoLog(sh, static_cast<GLsizei_>(sizeof(log)), nullptr, log);
        std::snprintf(err, errSize, "shader %s: %s", (kind == kGL_VERTEX_SHADER ? "vert" : "frag"), log);
        g_gl.DeleteShader(sh);
        return 0;
    }
    return sh;
}

bool EnvFlagOn(const char* name) {
    const char* v = std::getenv(name);
    return v != nullptr && v[0] != '\0' && v[0] != '0';
}

} // namespace

ScenePostFx::~ScenePostFx() {
    // O contexto de GL so morre com o renderer; o StageState e destruido antes
    // disso, portanto e seguro apagar o programa aqui.
    if (program != 0 && g_gl.loaded && g_gl.DeleteProgram) {
        g_gl.DeleteProgram(program);
    }
    program = 0;
    ready = false;
}

SDL_BlendMode ScenePostFx::NoGrayStampBlendMode() {
    // Cor: srcRGB*ZERO + dstRGB*ONE  -> o RGB da cena fica intacto.
    // Alfa: srcA*ZERO + dstA*(1-srcA) -> onde o sprite e OPACO o alfa cai a zero;
    //                                    onde e transparente nada muda.
    static SDL_BlendMode mode = SDL_ComposeCustomBlendMode(
        SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD,
        SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD);
    return mode;
}

bool ScenePostFx::Init(SDL_Renderer* renderer) {
    if (initTried) {
        return ready;
    }
    initTried = true;

    if (!renderer) {
        std::snprintf(status, sizeof(status), "sem renderer");
        return false;
    }

    if (EnvFlagOn("TLL_POSTFX_OFF")) {
        std::snprintf(status, sizeof(status), "desligado por TLL_POSTFX_OFF");
        return false;
    }
    flipV = EnvFlagOn("TLL_POSTFX_FLIPV");

    SDL_RendererInfo info;
    SDL_zero(info);
    if (SDL_GetRendererInfo(renderer, &info) != 0 || info.name == nullptr) {
        std::snprintf(status, sizeof(status), "SDL_GetRendererInfo falhou");
        return false;
    }
    if (std::strcmp(info.name, "opengl") != 0) {
        std::snprintf(status, sizeof(status), "backend '%s' (precisa de opengl)", info.name);
        std::cerr << "[ScenePostFx] " << status << " — filtro preto-e-branco desligado." << std::endl;
        return false;
    }

    const char* missing = LoadGLFunctions();
    if (missing != nullptr) {
        std::snprintf(status, sizeof(status), "falta %s", missing);
        std::cerr << "[ScenePostFx] " << status << std::endl;
        return false;
    }

    char err[256];
    err[0] = 0;
    const GLuint_ vs = CompileStage(kGL_VERTEX_SHADER, kVertexSrc, err, sizeof(err));
    if (vs == 0) {
        std::snprintf(status, sizeof(status), "%s", err);
        std::cerr << "[ScenePostFx] " << status << std::endl;
        return false;
    }
    const GLuint_ fs = CompileStage(kGL_FRAGMENT_SHADER, kFragmentSrc, err, sizeof(err));
    if (fs == 0) {
        g_gl.DeleteShader(vs);
        std::snprintf(status, sizeof(status), "%s", err);
        std::cerr << "[ScenePostFx] " << status << std::endl;
        return false;
    }

    const GLuint_ prog = g_gl.CreateProgram();
    g_gl.AttachShader(prog, vs);
    g_gl.AttachShader(prog, fs);
    g_gl.LinkProgram(prog);
    g_gl.DeleteShader(vs);
    g_gl.DeleteShader(fs);

    GLint_ linked = 0;
    g_gl.GetProgramiv(prog, kGL_LINK_STATUS, &linked);
    if (!linked) {
        char log[512];
        log[0] = 0;
        g_gl.GetProgramInfoLog(prog, static_cast<GLsizei_>(sizeof(log)), nullptr, log);
        g_gl.DeleteProgram(prog);
        std::snprintf(status, sizeof(status), "link: %s", log);
        std::cerr << "[ScenePostFx] " << status << std::endl;
        return false;
    }

    program = prog;

    static const char* kNames[U_COUNT] = {
        "uScene",           "uTexScale",       "uResolution",      "uGrayStrength",
        "uMonoGain",        "uMonoLift",       "uMonoTint",        "uMonoTintStrength",
        "uLightSharpen",    "uBlurPx",         "uVignetteStrength","uVignetteStart",
        "uVisionEnabled",   "uConeOrigin",     "uConeDir",         "uConeHalf",
        "uConeFeather",     "uConeLen",        "uConeLenFeather",  "uFoot0",
        "uFoot1",           "uFootCount",      "uFootRadius",      "uMaskGamma",
        "uVisionColorGain", "uFlipV",       "uLights[0]",       "uLightGamma",
        "uPlayerPos",       "uUnlitFadeR",     "uRequireLight",    "uConeGamma",
        "uMonoHighlight",   "uMonoLightGlow",  "uLightColor",      "uVisionSat"};
    for (int i = 0; i < U_COUNT; i++) {
        uniforms[i] = g_gl.GetUniformLocation(program, kNames[i]);
    }

    ready = true;
    std::snprintf(status, sizeof(status), "ok (opengl%s)", flipV ? ", flipV" : "");
    std::cout << "[ScenePostFx] " << status << std::endl;
    return true;
}

bool ScenePostFx::Render(SDL_Renderer* renderer, SDL_Texture* sceneTex, int windowW, int windowH,
                         const PlayerVisionFrame& vision, const PlayerVisionParams& params) {
    if (!ready || !renderer || !sceneTex || windowW < 1 || windowH < 1) {
        return false;
    }

    // Fecha a fila de comandos do SDL antes de tocar em GL cru (ver SDL_RenderFlush).
    if (SDL_RenderFlush(renderer) != 0) {
        return false;
    }

    // Guarda o estado que vamos mexer, para o SDL nao ficar com a cache errada.
    GLint_ prevProgram = 0;
    g_gl.GetIntegerv(kGL_CURRENT_PROGRAM, &prevProgram);
    const bool prevBlend = (g_gl.IsEnabled(kGL_BLEND) != 0);

    float texW = 1.0f;
    float texH = 1.0f;
    if (SDL_GL_BindTexture(sceneTex, &texW, &texH) != 0) {
        return false;
    }

    g_gl.Disable(kGL_BLEND);
    g_gl.UseProgram(program);

    auto set1i = [&](int u, int v) { if (uniforms[u] >= 0) g_gl.Uniform1i(uniforms[u], v); };
    auto set1f = [&](int u, float v) { if (uniforms[u] >= 0) g_gl.Uniform1f(uniforms[u], v); };
    auto set2f = [&](int u, float a, float b) { if (uniforms[u] >= 0) g_gl.Uniform2f(uniforms[u], a, b); };
    auto set3f = [&](int u, float a, float b, float c) { if (uniforms[u] >= 0) g_gl.Uniform3f(uniforms[u], a, b, c); };

    set1i(U_SCENE, 0);
    set2f(U_TEX_SCALE, texW, texH);
    set2f(U_RESOLUTION, static_cast<float>(windowW), static_cast<float>(windowH));

    // `visionOn` manda em TUDO o que a passagem faz. Sem um campo de visao
    // valido (cutscene, primeiro frame, jogador ainda por criar) o desfoque e o
    // cinzento tem de ficar a zero, senao o ecra inteiro borrava-se ou ficava
    // cinzento — as duas mascaras dariam zero em todo o lado.
    const bool visionOn = params.enabled && vision.valid;
    const float gray = visionOn ? std::max(0.0f, std::min(1.0f, params.grayStrength)) : 0.0f;
    set1f(U_GRAY_STRENGTH, gray);
    set1f(U_MONO_GAIN, std::max(0.0f, std::min(2.0f, params.monoGain)));
    set1f(U_MONO_LIFT, std::max(0.0f, std::min(0.35f, params.monoLift)));
    set3f(U_MONO_TINT, params.monoTintR, params.monoTintG, params.monoTintB);
    set1f(U_MONO_TINT_STRENGTH, std::max(0.0f, std::min(1.0f, params.monoTintStrength)));
    set1f(U_BLUR_PX, visionOn ? std::max(0.0f, std::min(48.0f, params.outsideBlurPx)) : 0.0f);
    set1f(U_VIGNETTE_STRENGTH, std::max(0.0f, std::min(1.0f, params.vignetteStrength)));
    set1f(U_VIGNETTE_START, std::max(0.0f, std::min(0.98f, params.vignetteStart)));
    set1f(U_VISION_COLOR_GAIN, visionOn ? std::max(0.2f, std::min(3.0f, params.visionColorGain)) : 1.0f);
    set1f(U_VISION_SAT, visionOn ? std::max(0.0f, std::min(3.0f, params.visionSaturation)) : 1.0f);
    set1f(U_MONO_HIGHLIGHT, std::max(0.0f, std::min(2.0f, params.monoHighlightGain)));
    set1f(U_MONO_LIGHT_GLOW, std::max(0.0f, std::min(2.0f, params.monoLightGlow)));
    set1f(U_LIGHT_COLOR, std::max(0.0f, std::min(1.0f, params.lightColorStrength)));
    set1f(U_LIGHT_SHARPEN, std::max(0.0f, std::min(1.0f, params.lightSharpenStrength)));
    set1f(U_FLIP_V, flipV ? 1.0f : 0.0f);

    set1f(U_VISION_ENABLED, visionOn ? 1.0f : 0.0f);
    if (visionOn) {
        set2f(U_CONE_ORIGIN, vision.coneX, vision.coneY);
        set2f(U_CONE_DIR, vision.dirX, vision.dirY);
        set1f(U_CONE_HALF, std::max(0.01f, vision.halfAngleRad));
        set1f(U_CONE_FEATHER, std::max(0.004f, vision.featherRad));
        set1f(U_CONE_LEN, std::max(1.0f, vision.lengthPx));
        set1f(U_CONE_LEN_FEATHER, std::max(1.0f, vision.lengthFeatherPx));
        const int feet = std::max(0, std::min(PlayerVisionFrame::kMaxFeet, vision.footCount));
        // Sem pes validos, empurra os circulos para fora do ecra em vez de os
        // desligar: assim o shader nao precisa de mais um ramo.
        const float offX = -1.0e6f;
        set2f(U_FOOT0, feet > 0 ? vision.footX[0] : offX, feet > 0 ? vision.footY[0] : offX);
        set2f(U_FOOT1, feet > 1 ? vision.footX[1] : offX, feet > 1 ? vision.footY[1] : offX);
        set1f(U_FOOT_COUNT, static_cast<float>(feet));
        set1f(U_FOOT_RADIUS, std::max(1.0f, vision.footRadiusPx));
        set1f(U_MASK_GAMMA, std::max(0.2f, std::min(12.0f, vision.maskGamma)));
        set1f(U_CONE_GAMMA, std::max(0.2f, std::min(16.0f, vision.coneGamma)));

        // Luzes reais: um vec4 por ranhura. As que sobram levam intensidade 0 e
        // um centro fora do ecra, para o ciclo do shader as ignorar sozinho.
        const bool requireLight = params.requireLightToSee && vision.requireLight;
        set1f(U_REQUIRE_LIGHT, requireLight ? 1.0f : 0.0f);
        // Sempre enviadas, mesmo com a regra desligada: o halo das luzes na
        // camada monocromatica tambem as usa.
        if (uniforms[U_LIGHTS] >= 0) {
            float packed[PlayerVisionFrame::kMaxLightSamples * 4];
            const int used = std::max(0, std::min(PlayerVisionFrame::kMaxLightSamples, vision.lightCount));
            for (int i = 0; i < PlayerVisionFrame::kMaxLightSamples; i++) {
                const bool on = i < used;
                packed[i * 4 + 0] = on ? vision.lightX[i] : -1.0e6f;
                packed[i * 4 + 1] = on ? vision.lightY[i] : -1.0e6f;
                packed[i * 4 + 2] = on ? std::max(1.0f, vision.lightR[i]) : 1.0f;
                packed[i * 4 + 3] = on ? std::max(0.0f, std::min(1.0f, vision.lightI[i])) : 0.0f;
            }
            g_gl.Uniform4fv(uniforms[U_LIGHTS], PlayerVisionFrame::kMaxLightSamples, packed);
        }
        set1f(U_LIGHT_GAMMA, std::max(0.5f, std::min(12.0f, vision.lightGamma)));
        set2f(U_PLAYER_POS, vision.playerX, vision.playerY);
        set1f(U_UNLIT_FADE_R, std::max(1.0f, vision.unlitFadeRadiusPx));
    }

    // Quad em espaco de clip: preenche exatamente o viewport que o SDL deixou
    // montado (respeita o letterbox do SDL_RenderSetLogicalSize).
    const float vTop = flipV ? 1.0f : 0.0f;
    const float vBot = flipV ? 0.0f : 1.0f;
    g_gl.Begin(kGL_TRIANGLE_FAN);
    g_gl.TexCoord2f(0.0f, vTop);
    g_gl.Vertex2f(-1.0f, 1.0f);
    g_gl.TexCoord2f(1.0f, vTop);
    g_gl.Vertex2f(1.0f, 1.0f);
    g_gl.TexCoord2f(1.0f, vBot);
    g_gl.Vertex2f(1.0f, -1.0f);
    g_gl.TexCoord2f(0.0f, vBot);
    g_gl.Vertex2f(-1.0f, -1.0f);
    g_gl.End();

    g_gl.UseProgram(static_cast<GLuint_>(prevProgram));
    SDL_GL_UnbindTexture(sceneTex);
    if (prevBlend) {
        g_gl.Enable(kGL_BLEND);
    }
    return true;
}
