#include "core/Telemetry.h"

#include "core/CrashHandler.h"

#include "nlohmann/json.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <random>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

#ifndef BUILD_GITHASH
#define BUILD_GITHASH "desconhecido"
#endif

namespace {

const char* kLogsDir = "logs";
const char* kPlaytestDir = "logs/playtest";
const char* kTesterConfigPath = "config/playtest.json";

std::FILE* g_file = nullptr;
bool g_started = false;
bool g_ended = false;
bool g_enabled = true;
std::string g_sessionId;
std::string g_filePath;
std::string g_endReason;
bool g_intense = false;
std::chrono::steady_clock::time_point g_start;

// ── Janela de FPS ───────────────────────────────────────────────────────────
// Reiniciada a cada amostra: o FPS de uma amostra descreve o segundo que ela
// fecha, nao a sessao inteira.
int g_frames = 0;
double g_frameSeconds = 0.0;
double g_worstFrame = 0.0;
double g_lastHitchAt = -100.0;

/// Frame a partir do qual o engasgo se ve. 200 ms = 5 FPS instantaneos.
constexpr double kHitchSeconds = 0.20;
/// Nao mais do que um "hitch" por este intervalo: um carregamento de nivel
/// consegue encadear varios frames longos e encheria o ficheiro sozinho.
constexpr double kHitchCooldown = 1.5;

void EnsureDir(const char* dir) {
#ifdef _WIN32
    CreateDirectoryA(dir, nullptr);   // no-op se ja existe
#else
    mkdir(dir, 0755);
#endif
}

/// `forFileName` = formato para nome de ficheiro; senao, data legivel.
/// Mesmo idioma do CrashHandler: `std::localtime` devolve um buffer estatico,
/// o que so seria um problema com varias threads — e aqui so ha uma.
std::string Timestamp(bool forFileName) {
    const std::time_t now = std::time(nullptr);
    const std::tm* tmv = std::localtime(&now);
    if (!tmv) {
        return "desconhecido";
    }
    char buf[32];
    std::strftime(buf, sizeof(buf), forFileName ? "%Y%m%d_%H%M%S" : "%Y-%m-%d %H:%M:%S", tmv);
    return std::string(buf);
}

/// Quem esta a jogar. Ordem: variavel de ambiente (o que o script de playtest
/// define), depois config/playtest.json, depois "anon". Nunca vai buscar nada
/// ao sistema — o nome do utilizador do Windows nao e nosso para gravar.
std::string ReadTesterId() {
    if (const char* env = std::getenv("TLL_TESTER")) {
        if (env[0] != '\0') {
            return std::string(env);
        }
    }
    std::ifstream in(kTesterConfigPath);
    if (in.good()) {
        try {
            nlohmann::json j;
            in >> j;
            if (j.is_object() && j.contains("tester") && j["tester"].is_string()) {
                const std::string name = j["tester"].get<std::string>();
                if (!name.empty()) {
                    return name;
                }
            }
        } catch (...) {
            // Ficheiro estragado: nao vale um crash no arranque.
        }
    }
    return "anon";
}

std::string MakeSessionId() {
    std::mt19937 rng(static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<int> dist(0, 0xFFFF);
    char suffix[8];
    std::snprintf(suffix, sizeof(suffix), "%04x", dist(rng));
    return Timestamp(true) + "_" + suffix;
}

const char* PlatformName() {
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#else
    return "outro";
#endif
}

void AppendEscaped(std::string& out, const char* s) {
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p != '\0'; ++p) {
        switch (*p) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if (*p < 0x20) {
                char esc[8];
                std::snprintf(esc, sizeof(esc), "\\u%04x", *p);
                out += esc;
            } else {
                // Bytes >= 0x80 passam intactos: o ficheiro e UTF-8, como as
                // fontes do jogo, e os nomes dos itens tem acentos.
                out += static_cast<char>(*p);
            }
            break;
        }
    }
}

/// JSON nao tem NaN nem infinito: um valor destes tornaria a linha inteira
/// ilegivel para o visualizador. Trocamos por 0 e seguimos.
double Finite(double v) {
    return std::isfinite(v) ? v : 0.0;
}

void WriteLine(const char* kind, const char* type, const std::string& fieldsJson) {
    if (!g_enabled || !g_file) {
        return;
    }
    std::string line = "{\"t\":";
    char num[32];
    std::snprintf(num, sizeof(num), "%.3f", Finite(Telemetry::Now()));
    line += num;
    line += ",\"k\":\"";
    line += kind;
    line += "\"";
    if (type != nullptr) {
        line += ",\"type\":\"";
        AppendEscaped(line, type);
        line += "\"";
    }
    line += fieldsJson;
    line += "}\n";

    std::fputs(line.c_str(), g_file);
    // Descarrega SEMPRE. Uma sessao de playtest que rebenta e exactamente a
    // sessao que mais interessa ler, e o buffer do C perderia os ultimos
    // segundos — que sao os que explicam o rebentamento.
    std::fflush(g_file);
}

}  // namespace

namespace Telemetry {

// ── Fields ──────────────────────────────────────────────────────────────────

Fields& Fields::Str(const char* key, const std::string& value) {
    return Str(key, value.c_str());
}

Fields& Fields::Str(const char* key, const char* value) {
    buf += ",\"";
    AppendEscaped(buf, key);
    buf += "\":\"";
    AppendEscaped(buf, value ? value : "");
    buf += "\"";
    return *this;
}

Fields& Fields::Num(const char* key, double value) {
    char num[32];
    std::snprintf(num, sizeof(num), "%.3f", Finite(value));
    buf += ",\"";
    AppendEscaped(buf, key);
    buf += "\":";
    buf += num;
    return *this;
}

Fields& Fields::Int(const char* key, long long value) {
    char num[32];
    std::snprintf(num, sizeof(num), "%lld", value);
    buf += ",\"";
    AppendEscaped(buf, key);
    buf += "\":";
    buf += num;
    return *this;
}

Fields& Fields::Bool(const char* key, bool value) {
    buf += ",\"";
    AppendEscaped(buf, key);
    buf += "\":";
    buf += value ? "true" : "false";
    return *this;
}

Fields& Fields::Pos(const char* key, float x, float y) {
    const std::string prefix(key ? key : "");
    Num((prefix + (prefix.empty() ? "x" : "X")).c_str(), x);
    Num((prefix + (prefix.empty() ? "y" : "Y")).c_str(), y);
    return *this;
}

// ── Ciclo de vida ───────────────────────────────────────────────────────────

void Begin() {
    if (g_started) {
        return;
    }
    g_started = true;
    g_start = std::chrono::steady_clock::now();

    if (const char* off = std::getenv("TLL_TELEMETRY")) {
        if (std::strcmp(off, "0") == 0) {
            g_enabled = false;
            CrashHandler::Log("Telemetria desligada (TLL_TELEMETRY=0).");
            return;
        }
    }

    EnsureDir(kLogsDir);
    EnsureDir(kPlaytestDir);

    g_sessionId = MakeSessionId();
    g_filePath = std::string(kPlaytestDir) + "/" + g_sessionId + ".jsonl";
    g_file = std::fopen(g_filePath.c_str(), "w");
    if (!g_file) {
        g_enabled = false;
        CrashHandler::Log("Telemetria: nao consegui abrir %s", g_filePath.c_str());
        return;
    }

    Fields meta;
    meta.Str("session", g_sessionId)
        .Str("tester", ReadTesterId())
        .Str("build", BUILD_GITHASH)
        .Str("startedAt", Timestamp(false))
        .Str("platform", PlatformName())
        .Int("schema", 1);
#ifdef DEBUG
    meta.Bool("debugBuild", true);
#else
    meta.Bool("debugBuild", false);
#endif
    WriteLine("meta", nullptr, meta.Json());
    CrashHandler::Log("Telemetria: a gravar em %s", g_filePath.c_str());
}

void SetIntense(bool on) { g_intense = on; }

bool IsIntense() { return g_intense; }

void SetEndReason(const char* reason) {
    if (reason && reason[0] != '\0') {
        g_endReason = reason;
    }
}

void NoteCrash(const char* reason) {
    if (!g_enabled || !g_file || g_ended) {
        return;
    }
    g_ended = true;   // o `End` que vier a seguir ja nao escreve nada
    Event("crash", Fields().Str("reason", reason ? reason : "?").Num("duration", Now()));
    std::fclose(g_file);
    g_file = nullptr;
}

void End(const char* reason) {
    if (!g_enabled || !g_file || g_ended) {
        return;
    }
    g_ended = true;
    // O motivo que o jogo registou (janela fechada, menu) manda sobre o
    // generico que vem do `main`.
    const std::string why = !g_endReason.empty() ? g_endReason : std::string(reason ? reason : "normal");
    Event("session_end", Fields().Str("reason", why).Num("duration", Now()));
    std::fclose(g_file);
    g_file = nullptr;
}

bool IsEnabled() { return g_enabled && g_file != nullptr; }

const std::string& SessionId() { return g_sessionId; }

const std::string& FilePath() { return g_filePath; }

double Now() {
    const auto delta = std::chrono::steady_clock::now() - g_start;
    return std::chrono::duration<double>(delta).count();
}

// ── Escrita ─────────────────────────────────────────────────────────────────

void Event(const char* type) {
    WriteLine("event", type, std::string());
}

void Event(const char* type, const Fields& fields) {
    WriteLine("event", type, fields.Json());
}

void Sample(const Fields& fields) {
    if (!IsEnabled()) {
        return;
    }
    const double fps = (g_frameSeconds > 0.0001) ? (g_frames / g_frameSeconds) : 0.0;
    std::string json = fields.Json();
    Fields perf;
    perf.Num("fps", fps).Num("worstMs", g_worstFrame * 1000.0);
    json += perf.Json();
    WriteLine("sample", nullptr, json);

    g_frames = 0;
    g_frameSeconds = 0.0;
    g_worstFrame = 0.0;
}

void FrameTick(float dt) {
    if (!IsEnabled()) {
        return;
    }
    const double d = Finite(dt);
    g_frames++;
    g_frameSeconds += d;
    if (d > g_worstFrame) {
        g_worstFrame = d;
    }
    if (d >= kHitchSeconds) {
        const double now = Now();
        if (now - g_lastHitchAt >= kHitchCooldown) {
            g_lastHitchAt = now;
            Event("hitch", Fields().Num("ms", d * 1000.0));
        }
    }
}

}  // namespace Telemetry
