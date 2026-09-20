#include "lighting/LightTweakStore.h"

#include "nlohmann/json.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

namespace {

const char* kPrimaryPath = "config/lighting.json";
const char* kFallbackPath = "lighting.json";

char g_lastPath[64] = "config/lighting.json";

void RememberPath(const char* p) {
    std::snprintf(g_lastPath, sizeof(g_lastPath), "%s", p);
}

// ── Listas de campos ────────────────────────────────────────────────────────
// Uma linha por valor gravado. Para acrescentar um parametro novo ao painel,
// basta juntar o nome a lista certa: a leitura, a escrita e o nome da chave no
// JSON saem todos daqui. Esquecer-se disto nao parte nada — o valor volta ao
// default na proxima execucao, que e o sintoma a procurar.

#define TLL_MASK_FLOATS(X)                                                                                             \
    X(falloffRadiusPx) X(fatorDicaDeRaio) X(falloffGamma) X(innerLift) X(marginExtraAlemDoCanto) X(rMaxMinimo)         \
    X(ellipseAspect) X(coneHalfAngleDeg) X(coneLengthPx) X(coneAxisDeg) X(coneFeatherDeg) X(coneLengthFeatherPx)       \
    X(rectHalfWidthPx) X(rectHalfHeightPx) X(rectSoftBandPx) X(shadowCastDistanceMul) X(shadowMaxLengthPx)             \
    X(shadowLengthByLightMul) X(spriteShadowMinScale) X(spriteShadowMaxScale) X(shadowSoftness)                         \
    X(lightTemporalSmoothing) X(lightGridStepPx) X(torchAnimSpeed) X(torchMotionRangePx) X(torchWarpStrength)          \
    X(torchPulseStrength) X(torchColorWarmth) X(torchColorStrength) X(torchGlowStrength) X(torchGlowRadiusScale)

#define TLL_MASK_INTS(X) X(numRings) X(numSeg) X(shadowSoftLayers)

#define TLL_MASK_BOOLS(X) X(coneFollowMouse) X(radialMaskHalfResolution)

#define TLL_MASK_U8(X) X(darknessMax) X(ambientDarknessMax)

#define TLL_VISION_FLOATS(X)                                                                                           \
    X(coneHalfAngleDeg) X(coneFeatherDeg) X(coneLengthPx) X(coneLengthFeatherPx) X(coneEdgeGamma)                      \
    X(coneOriginForwardPx) X(turnSpeedDegPerSec) X(footRadiusPx) X(maskFalloffGamma) X(unlitVisionDarkness)            \
    X(lightReachScale) X(lightPerceptionGamma) X(unlitFadeDistancePx) X(grayStrength) X(monoGain) X(monoLift)          \
    X(monoTintStrength) X(monoTintR) X(monoTintG) X(monoTintB) X(outsideBlurPx) X(lightSharpenStrength)               \
    X(monoHighlightGain) X(monoLightGlow) X(vignetteStrength) X(vignetteStart) X(visionColorGain)                     \
    X(visionSaturation)                                                                                            \
    X(lightColorStrength)                                                                                          \
    X(itemRevealThreshold) X(cameraZoom)

#define TLL_VISION_BOOLS(X)                                                                                            \
    X(enabled) X(requireLightToSee) X(hideItemsOutsideVision) X(hideInteractablesOutsideVision)                        \
    X(hidePushablesOutsideVision)

// ── Leitura defensiva ───────────────────────────────────────────────────────
// Um valor so entra se existir, for do tipo certo e for FINITO. Um NaN gravado
// por engano espalha-se por toda a conta da luz e da um ecra preto sem qualquer
// mensagem de erro, por isso apanha-se aqui.

void ReadFloat(const nlohmann::json& j, const char* key, float& out) {
    if (!j.contains(key) || !j[key].is_number()) {
        return;
    }
    const double v = j[key].get<double>();
    if (std::isfinite(v)) {
        out = static_cast<float>(v);
    }
}

void ReadInt(const nlohmann::json& j, const char* key, int& out) {
    if (j.contains(key) && j[key].is_number_integer()) {
        out = j[key].get<int>();
    }
}

void ReadBool(const nlohmann::json& j, const char* key, bool& out) {
    if (j.contains(key) && j[key].is_boolean()) {
        out = j[key].get<bool>();
    }
}

void ReadU8(const nlohmann::json& j, const char* key, Uint8& out) {
    if (!j.contains(key) || !j[key].is_number_integer()) {
        return;
    }
    const int v = j[key].get<int>();
    if (v >= 0 && v <= 255) {
        out = static_cast<Uint8>(v);
    }
}

/// Enums vao como inteiro. Um valor fora do intervalo fica com o que la estava.
template <typename E>
void ReadEnum(const nlohmann::json& j, const char* key, E& out, int lo, int hi) {
    if (!j.contains(key) || !j[key].is_number_integer()) {
        return;
    }
    const int v = j[key].get<int>();
    if (v >= lo && v <= hi) {
        out = static_cast<E>(v);
    }
}

bool ReadFileInto(const char* path, nlohmann::json& out) {
    std::ifstream f(path);
    if (!f.is_open()) {
        return false;
    }
    try {
        f >> out;
    } catch (const std::exception& ex) {
        std::cerr << "[LightTweakStore] " << path << " ignorado (parse): " << ex.what() << std::endl;
        return false;
    }
    return out.is_object();
}

} // namespace

const char* LightTweakStore::LastPath() {
    return g_lastPath;
}

bool LightTweakStore::Load(LightMaskParams& params, LightMaskShape& shape, PlayerVisionParams& vision,
                           bool& durability) {
    nlohmann::json j;
    const char* path = kPrimaryPath;
    if (!ReadFileInto(path, j)) {
        path = kFallbackPath;
        if (!ReadFileInto(path, j)) {
            return false;   // primeira execucao: ficam os defaults compilados
        }
    }
    RememberPath(path);

    if (j.contains("mask") && j["mask"].is_object()) {
        const nlohmann::json& m = j["mask"];
#define X(f) ReadFloat(m, #f, params.f);
        TLL_MASK_FLOATS(X)
#undef X
#define X(f) ReadInt(m, #f, params.f);
        TLL_MASK_INTS(X)
#undef X
#define X(f) ReadBool(m, #f, params.f);
        TLL_MASK_BOOLS(X)
#undef X
#define X(f) ReadU8(m, #f, params.f);
        TLL_MASK_U8(X)
#undef X
        ReadEnum(m, "falloffCurve", params.falloffCurve, 0, 1);
        ReadEnum(m, "lightQualityPreset", params.lightQualityPreset, 0, 3);
    }

    if (j.contains("vision") && j["vision"].is_object()) {
        const nlohmann::json& v = j["vision"];
#define X(f) ReadFloat(v, #f, vision.f);
        TLL_VISION_FLOATS(X)
#undef X
#define X(f) ReadBool(v, #f, vision.f);
        TLL_VISION_BOOLS(X)
#undef X
    }

    ReadEnum(j, "shape", shape, 0, 4);
    ReadBool(j, "durability", durability);

    std::cout << "[LightTweakStore] valores lidos de " << path << std::endl;
    return true;
}

bool LightTweakStore::Save(const LightMaskParams& params, LightMaskShape shape, const PlayerVisionParams& vision,
                           bool durability) {
    nlohmann::json j = nlohmann::json::object();
    j["_leia_me"] = "Valores do painel de afinacao (tecla \\). Apague este ficheiro para voltar aos defaults do jogo.";

    nlohmann::json m = nlohmann::json::object();
#define X(f) m[#f] = params.f;
    TLL_MASK_FLOATS(X)
    TLL_MASK_INTS(X)
    TLL_MASK_BOOLS(X)
#undef X
#define X(f) m[#f] = static_cast<int>(params.f);
    TLL_MASK_U8(X)
#undef X
    m["falloffCurve"] = static_cast<int>(params.falloffCurve);
    m["lightQualityPreset"] = static_cast<int>(params.lightQualityPreset);
    j["mask"] = m;

    nlohmann::json v = nlohmann::json::object();
#define X(f) v[#f] = vision.f;
    TLL_VISION_FLOATS(X)
    TLL_VISION_BOOLS(X)
#undef X
    j["vision"] = v;

    j["shape"] = static_cast<int>(shape);
    j["durability"] = durability;

    const char* path = kPrimaryPath;
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) {
        // Sem pasta `config`: grava ao lado do executavel em vez de perder tudo.
        path = kFallbackPath;
        out.open(path, std::ios::trunc);
        if (!out.is_open()) {
            std::cerr << "[LightTweakStore] nao consegui gravar em " << kPrimaryPath << " nem em " << kFallbackPath
                      << std::endl;
            return false;
        }
    }
    out << j.dump(2) << std::endl;
    RememberPath(path);
    return true;
}
