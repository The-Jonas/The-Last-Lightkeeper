#include "core/CrashHandler.h"

// Hash do commit embutido pelo makefile (-DBUILD_GITHASH). Torna o relatorio
// AUTO-IDENTIFICAVEL: sabemos EXATAMENTE de qual fonte o build veio, para
// resolver os enderecos com o binario/commit certos.
#ifndef BUILD_GITHASH
#define BUILD_GITHASH "desconhecido"
#endif

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <mutex>
#include <string>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #ifdef __MINGW32__
    // dbghelp.h tem suporte limitado no MinGW — stack trace e minidump indisponiveis
    #define CRASH_HANDLER_NO_DBGHELP
  #else
    #include <dbghelp.h>
  #endif
#else
  #include <sys/stat.h>
  #include <unistd.h>
  #include <csignal>
  #if defined(__has_include)
    #if __has_include(<execinfo.h>)
      #include <execinfo.h>
      #define CH_HAVE_EXECINFO 1
    #endif
  #endif
#endif

namespace {

// ── Estado global do módulo ──────────────────────────────────────────────────
constexpr int   kMaxCrumbs   = 64;    // quantas migalhas guardar
constexpr int   kMaxFrames   = 64;    // profundidade máx. da pilha
const char*     kReportsDir  = "crash-reports";
const char*     kLogsDir     = "logs";

std::mutex   g_mutex;
std::string  g_crumbs[kMaxCrumbs];
int          g_crumbHead  = 0;        // próximo índice a escrever
int          g_crumbCount = 0;
std::FILE*   g_log        = nullptr;  // logs/latest.log
bool         g_installed  = false;

// ── Helpers de tempo / diretório ─────────────────────────────────────────────
void Timestamp(char* out, size_t n, bool forFilename) {
    std::time_t t = std::time(nullptr);
    std::tm* tmv = std::localtime(&t);   // buffer estatico; ok no contexto usado
    if (!tmv) { std::snprintf(out, n, "unknown"); return; }
    std::strftime(out, n, forFilename ? "%Y%m%d_%H%M%S" : "%Y-%m-%d %H:%M:%S", tmv);
}

void EnsureDir(const char* dir) {
#ifdef _WIN32
    CreateDirectoryA(dir, nullptr);   // no-op se já existe
#else
    mkdir(dir, 0755);
#endif
}

bool HasConsole() {
#ifdef _WIN32
    return GetConsoleWindow() != nullptr;
#else
    return isatty(fileno(stderr)) != 0;
#endif
}

// Escreve uma linha no log de sessão (com flush imediato — precisamos que
// sobreviva a um crash logo em seguida).
void WriteLog(const std::string& line) {
    if (!g_log) return;
    std::fputs(line.c_str(), g_log);
    std::fputc('\n', g_log);
    std::fflush(g_log);
}

// Adiciona uma migalha ao buffer circular. (Assume mutex já travado ou contexto
// single-thread de crash.)
void PushCrumb(const std::string& msg) {
    char ts[32];
    Timestamp(ts, sizeof(ts), false);
    std::string full = std::string(ts) + "  " + msg;
    g_crumbs[g_crumbHead] = full;
    g_crumbHead = (g_crumbHead + 1) % kMaxCrumbs;
    if (g_crumbCount < kMaxCrumbs) ++g_crumbCount;
    WriteLog(full);
}

// Despeja as migalhas (mais antiga → mais recente) num arquivo aberto.
// NÃO trava o mutex: é chamado do handler de crash, onde travar poderia causar
// deadlock caso o crash tenha ocorrido com o mutex já em posse.
void DumpCrumbs(std::FILE* f) {
    std::fputs("\n===== Ultimas acoes (breadcrumbs, mais antiga -> mais recente) =====\n", f);
    if (g_crumbCount == 0) {
        std::fputs("(nenhuma registrada)\n", f);
        return;
    }
    int start = (g_crumbCount < kMaxCrumbs) ? 0 : g_crumbHead;
    for (int i = 0; i < g_crumbCount; ++i) {
        int idx = (start + i) % kMaxCrumbs;
        std::fputs(g_crumbs[idx].c_str(), f);
        std::fputc('\n', f);
    }
}

// Cria e abre um arquivo de relatório novo; devolve o caminho por out-param.
std::FILE* OpenReport(std::string& pathOut, const char* ext) {
    EnsureDir(kReportsDir);
    char ts[32];
    Timestamp(ts, sizeof(ts), true);
    pathOut = std::string(kReportsDir) + "/crash_" + ts + ext;
    return std::fopen(pathOut.c_str(), "wb");
}

void WriteHeader(std::FILE* f) {
    char ts[32];
    Timestamp(ts, sizeof(ts), false);
    std::fprintf(f, "===== A Luz do Farol - RELATORIO DE CRASH =====\n");
    std::fprintf(f, "Data/hora : %s\n", ts);
#ifdef _WIN32
    char exePath[MAX_PATH] = {0};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::fprintf(f, "Executavel: %s\n", exePath);
    std::fprintf(f, "PID       : %lu\n", (unsigned long)GetCurrentProcessId());
#endif
    std::fprintf(f, "Build     : %s %s\n", __DATE__, __TIME__);
    std::fprintf(f, "Commit    : %s\n", BUILD_GITHASH);
}

#ifdef _WIN32
// ── Windows: nomes de exceção, pilha, minidump ───────────────────────────────
const char* ExceptionName(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:      return "ACCESS_VIOLATION (acesso invalido de memoria)";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_DATATYPE_MISALIGNMENT: return "DATATYPE_MISALIGNMENT";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:    return "FLT_DIVIDE_BY_ZERO";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:    return "INT_DIVIDE_BY_ZERO";
        case EXCEPTION_ILLEGAL_INSTRUCTION:   return "ILLEGAL_INSTRUCTION";
        case EXCEPTION_STACK_OVERFLOW:        return "STACK_OVERFLOW (recursao/estouro de pilha)";
        case EXCEPTION_PRIV_INSTRUCTION:      return "PRIV_INSTRUCTION";
        case EXCEPTION_IN_PAGE_ERROR:         return "IN_PAGE_ERROR";
        case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "NONCONTINUABLE_EXCEPTION";
        default:                              return "DESCONHECIDA";
    }
}

// Base e tamanho do modulo principal (JOGO.exe), lidos direto do cabeçalho PET
// carregado — sem depender de psapi.
bool GetMainModuleRange(DWORD64& base, DWORD64& size) {
    HMODULE h = GetModuleHandleW(nullptr);
    if (!h) return false;
    BYTE* b = reinterpret_cast<BYTE*>(h);
    IMAGE_DOS_HEADER* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(b);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    IMAGE_NT_HEADERS* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(b + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    base = reinterpret_cast<DWORD64>(b);
    size = nt->OptionalHeader.SizeOfImage;
    return true;
}

// Registradores + base do modulo. A base torna todos os "RVA = endereco - base"
// verificaveis e permite conferir se o build bate com o do desenvolvedor.
void WriteRegisters(std::FILE* f, CONTEXT* ctx) {
    if (!ctx) return;
    DWORD64 base = 0, size = 0;
    GetMainModuleRange(base, size);
    std::fputs("\n===== Registradores / modulo =====\n", f);
    std::fprintf(f, "JOGO base : 0x%llx  (SizeOfImage 0x%llx)  [RVA = endereco - base]\n",
                 (unsigned long long)base, (unsigned long long)size);
#ifdef _WIN64
    std::fprintf(f, "RIP=0x%016llx RSP=0x%016llx RBP=0x%016llx\n",
                 (unsigned long long)ctx->Rip, (unsigned long long)ctx->Rsp, (unsigned long long)ctx->Rbp);
    std::fprintf(f, "RAX=0x%016llx RBX=0x%016llx RCX=0x%016llx RDX=0x%016llx\n",
                 (unsigned long long)ctx->Rax, (unsigned long long)ctx->Rbx,
                 (unsigned long long)ctx->Rcx, (unsigned long long)ctx->Rdx);
    std::fprintf(f, "RSI=0x%016llx RDI=0x%016llx R8 =0x%016llx R9 =0x%016llx\n",
                 (unsigned long long)ctx->Rsi, (unsigned long long)ctx->Rdi,
                 (unsigned long long)ctx->R8, (unsigned long long)ctx->R9);
#else
    std::fprintf(f, "EIP=0x%08lx ESP=0x%08lx EBP=0x%08lx\n",
                 (unsigned long)ctx->Eip, (unsigned long)ctx->Esp, (unsigned long)ctx->Ebp);
    std::fprintf(f, "EAX=0x%08lx EBX=0x%08lx ECX=0x%08lx EDX=0x%08lx ESI=0x%08lx EDI=0x%08lx\n",
                 (unsigned long)ctx->Eax, (unsigned long)ctx->Ebx, (unsigned long)ctx->Ecx,
                 (unsigned long)ctx->Edx, (unsigned long)ctx->Esi, (unsigned long)ctx->Edi);
#endif
}

#ifndef CRASH_HANDLER_NO_DBGHELP
// Varredura BRUTA da pilha: em builds 32-bit -O3 (sem frame pointer), StackWalk64
// costuma parar no #0. Aqui lemos a pilha a partir de ESP/RSP e listamos todo
// valor que caia dentro do modulo JOGO — candidatos a endereço de retorno. Nem
// todos são chamadas reais, mas reconstroem grande parte da cadeia. Fica dentro
// da região committed (VirtualQuery) para não faltar em guard page.
void WriteRawStackScan(std::FILE* f, CONTEXT* ctx) {
    if (!ctx) return;
    DWORD64 base = 0, size = 0;
    if (!GetMainModuleRange(base, size)) return;
    const DWORD64 lo = base, hi = base + size;
#ifdef _WIN64
    uintptr_t sp = static_cast<uintptr_t>(ctx->Rsp);
    const size_t slot = 8;
#else
    uintptr_t sp = static_cast<uintptr_t>(ctx->Esp);
    const size_t slot = 4;
#endif
    MEMORY_BASIC_INFORMATION mbi;
    std::memset(&mbi, 0, sizeof(mbi));
    if (VirtualQuery(reinterpret_cast<void*>(sp), &mbi, sizeof(mbi)) == 0) return;

    BYTE* p   = reinterpret_cast<BYTE*>(sp);
    BYTE* end = reinterpret_cast<BYTE*>(mbi.BaseAddress) + mbi.RegionSize;
    BYTE* cap = p + 64 * 1024;                       // limita a 64 KB de pilha
    if (end > cap) end = cap;

    HANDLE proc = GetCurrentProcess();
    char symBuf[sizeof(SYMBOL_INFO) + 512];
    SYMBOL_INFO* sym = reinterpret_cast<SYMBOL_INFO*>(symBuf);

    std::fputs("\n===== Varredura da pilha (heuristica) =====\n"
               "Possiveis enderecos de retorno em JOGO (nem todos sao chamadas reais;\n"
               "uteis quando o call stack acima para cedo). Resolva com addr2line.\n", f);
    int hits = 0;
    for (; p + slot <= end && hits < 60; p += slot) {
        DWORD64 v = 0;
        std::memcpy(&v, p, slot);
        if (v < lo || v >= hi) continue;
        const DWORD64 rva = v - base;
        std::memset(sym, 0, sizeof(SYMBOL_INFO));
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 500;
        DWORD64 disp = 0;
        std::fprintf(f, "  [JOGO+0x%llx]", (unsigned long long)rva);
        if (SymFromAddr(proc, v, &disp, sym)) {
            std::fprintf(f, "  %s+0x%llx", sym->Name, (unsigned long long)disp);
        }
        std::fputc('\n', f);
        ++hits;
    }
    if (hits == 0) {
        std::fputs("  (nenhum candidato encontrado)\n", f);
    }
}

// Escreve a pilha de chamadas. Se 'ctx' for fornecido (crash real), usa
// StackWalk64 a partir do contexto do crash; senão captura a pilha atual.
void WriteStackTrace(std::FILE* f, CONTEXT* ctx) {
    HANDLE proc = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
    SymInitialize(proc, nullptr, TRUE);

    CONTEXT localCtx;
    if (!ctx) {
        RtlCaptureContext(&localCtx);
        ctx = &localCtx;
    }

    WriteRegisters(f, ctx);

    std::fputs("\n===== Pilha de chamadas (call stack) =====\n", f);

    STACKFRAME64 frame;
    std::memset(&frame, 0, sizeof(frame));
    DWORD machine;
#ifdef _WIN64
    machine = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset    = ctx->Rip;
    frame.AddrFrame.Offset = ctx->Rbp;
    frame.AddrStack.Offset = ctx->Rsp;
#else
    machine = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset    = ctx->Eip;
    frame.AddrFrame.Offset = ctx->Ebp;
    frame.AddrStack.Offset = ctx->Esp;
#endif
    frame.AddrPC.Mode    = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;

    // Buffer para SYMBOL_INFO + nome.
    char symBuf[sizeof(SYMBOL_INFO) + 512];
    SYMBOL_INFO* sym = reinterpret_cast<SYMBOL_INFO*>(symBuf);

    bool anyRVA = false;
    for (int i = 0; i < kMaxFrames; ++i) {
        if (!StackWalk64(machine, proc, thread, &frame, ctx, nullptr,
                         SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
            break;
        DWORD64 addr = frame.AddrPC.Offset;
        if (addr == 0) break;

        // Nome do modulo + RVA (endereco relativo ao modulo) — sempre util:
        // permite resolver depois com "addr2line -e JOGO.exe -f -C 0xRVA".
        IMAGEHLP_MODULE64 modInfo;
        std::memset(&modInfo, 0, sizeof(modInfo));
        modInfo.SizeOfStruct = sizeof(modInfo);
        const char* modName = "?";
        DWORD64 rva = 0;
        if (SymGetModuleInfo64(proc, addr, &modInfo)) {
            modName = modInfo.ModuleName;
            rva = addr - modInfo.BaseOfImage;
            anyRVA = true;
        }

        std::memset(sym, 0, sizeof(SYMBOL_INFO));
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 500;
        DWORD64 disp = 0;

        std::fprintf(f, "  #%-2d ", i);
        if (SymFromAddr(proc, addr, &disp, sym)) {
            std::fprintf(f, "%s+0x%llx", sym->Name, (unsigned long long)disp);
        } else {
            std::fprintf(f, "%s", modName);
        }

        IMAGEHLP_LINE64 line;
        std::memset(&line, 0, sizeof(line));
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisp = 0;
        if (SymGetLineFromAddr64(proc, addr, &lineDisp, &line)) {
            std::fprintf(f, "  (%s:%lu)", line.FileName, (unsigned long)line.LineNumber);
        }
        std::fprintf(f, "   [%s+0x%llx]\n", modName, (unsigned long long)rva);
    }

    if (anyRVA) {
        std::fputs("\nDica: para resolver arquivo:linha das entradas [modulo+0xRVA],\n"
                   "rode no PC de desenvolvimento (mesmo build):\n"
                   "  addr2line -e JOGO.exe -f -C -i 0xRVA1 0xRVA2 ...\n", f);
    }

    // Complemento heuristico: em 32-bit -O3 o StackWalk costuma parar cedo.
    WriteRawStackScan(f, ctx);

    SymCleanup(proc);
}

void WriteMiniDump(EXCEPTION_POINTERS* ep) {
    std::string path;
    char ts[32];
    Timestamp(ts, sizeof(ts), true);
    EnsureDir(kReportsDir);
    path = std::string(kReportsDir) + "/crash_" + ts + ".dmp";

    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

    MINIDUMP_EXCEPTION_INFORMATION mei;
    mei.ThreadId = GetCurrentThreadId();
    mei.ExceptionPointers = ep;
    mei.ClientPointers = FALSE;

    MINIDUMP_TYPE type = (MINIDUMP_TYPE)(MiniDumpWithThreadInfo | MiniDumpNormal);
    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
                      type, ep ? &mei : nullptr, nullptr, nullptr);
    CloseHandle(hFile);
}
#endif // CRASH_HANDLER_NO_DBGHELP

// Escreve o relatório completo (usado tanto pelo filtro SEH quanto pelo
// std::terminate, que passa ep = nullptr).
void WriteReport(EXCEPTION_POINTERS* ep, const char* reason) {
    std::string path;
    std::FILE* f = OpenReport(path, ".txt");
    if (!f) return;

    WriteHeader(f);
    if (ep && ep->ExceptionRecord) {
        DWORD code = ep->ExceptionRecord->ExceptionCode;
        std::fprintf(f, "Motivo    : %s (0x%08lx)\n", ExceptionName(code), (unsigned long)code);
        std::fprintf(f, "Endereco  : 0x%p\n", ep->ExceptionRecord->ExceptionAddress);
        if (code == EXCEPTION_ACCESS_VIOLATION &&
            ep->ExceptionRecord->NumberParameters >= 2) {
            ULONG_PTR op = ep->ExceptionRecord->ExceptionInformation[0];
            ULONG_PTR at = ep->ExceptionRecord->ExceptionInformation[1];
            std::fprintf(f, "Tentou %s o endereco 0x%p\n",
                         op == 0 ? "LER" : op == 1 ? "ESCREVER em" : "EXECUTAR",
                         (void*)at);
        }
    } else if (reason) {
        std::fprintf(f, "Motivo    : %s\n", reason);
    }

#ifndef CRASH_HANDLER_NO_DBGHELP
    WriteStackTrace(f, ep ? ep->ContextRecord : nullptr);
#else
    std::fputs("\n===== Pilha de chamadas =====\n", f);
    std::fputs("(stack trace indisponivel em build MinGW)\n", f);
#endif
    DumpCrumbs(f);
    std::fflush(f);
    std::fclose(f);

#ifndef CRASH_HANDLER_NO_DBGHELP
    WriteMiniDump(ep);
#endif

    // Avisa o jogador (best effort) — no build -mwindows nao ha console.
    char msg[512];
    std::snprintf(msg, sizeof(msg),
        "O jogo encontrou um erro e precisou fechar.\n\n"
        "Um relatorio foi salvo em:\n%s\n\n"
        "Por favor envie a pasta '%s' para os desenvolvedores.",
        path.c_str(), kReportsDir);
    MessageBoxA(nullptr, msg, "A Luz do Farol - Erro", MB_OK | MB_ICONERROR);
}

LONG WINAPI TopLevelFilter(EXCEPTION_POINTERS* ep) {
    WriteReport(ep, nullptr);
    return EXCEPTION_EXECUTE_HANDLER;   // encerra o processo
}

#else  // ── POSIX (Linux/Mac): signals + backtrace ─────────────────────────────

void WriteReportPosix(const char* reason) {
    std::string path;
    std::FILE* f = OpenReport(path, ".txt");
    if (!f) return;
    WriteHeader(f);
    std::fprintf(f, "Motivo    : %s\n", reason ? reason : "desconhecido");

    std::fputs("\n===== Pilha de chamadas (call stack) =====\n", f);
#ifdef CH_HAVE_EXECINFO
    void* frames[kMaxFrames];
    int n = backtrace(frames, kMaxFrames);
    char** syms = backtrace_symbols(frames, n);
    if (syms) {
        for (int i = 0; i < n; ++i) std::fprintf(f, "  #%-2d %s\n", i, syms[i]);
        std::free(syms);
    }
#else
    std::fputs("(backtrace indisponivel nesta plataforma)\n", f);
#endif
    DumpCrumbs(f);
    std::fflush(f);
    std::fclose(f);
}

void SignalHandler(int sig) {
    const char* name = "SIGNAL";
    switch (sig) {
        case SIGSEGV: name = "SIGSEGV (acesso invalido de memoria)"; break;
        case SIGABRT: name = "SIGABRT (abort)"; break;
        case SIGFPE:  name = "SIGFPE (erro aritmetico)"; break;
        case SIGILL:  name = "SIGILL (instrucao ilegal)"; break;
    }
    WriteReportPosix(name);
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}
#endif

// std::terminate: excecao C++ nao capturada.
void TerminateHandler() {
    std::string what = "std::terminate (excecao C++ nao capturada)";
    if (auto ex = std::current_exception()) {
        try {
            std::rethrow_exception(ex);
        } catch (const std::exception& e) {
            what += std::string(": ") + e.what();
        } catch (...) {
            what += ": (tipo desconhecido)";
        }
    }
#ifdef _WIN32
    WriteReport(nullptr, what.c_str());
#else
    WriteReportPosix(what.c_str());
#endif
    std::abort();
}

// Redireciona stdout/stderr para arquivos quando nao ha console (build do
// jogador), para capturar os std::cerr existentes.
void RedirectStdIO() {
    if (HasConsole()) return;   // build de debug (-mconsole): preserva o console
    EnsureDir(kLogsDir);
    std::freopen((std::string(kLogsDir) + "/stdout.log").c_str(), "w", stdout);
    std::freopen((std::string(kLogsDir) + "/stderr.log").c_str(), "w", stderr);
}

}  // namespace

// Precisa do SDL para o sink de log. Incluido depois do namespace anonimo para
// evitar poluir os helpers.
#define INCLUDE_SDL
#include "SDL_include.h"

namespace {
void SDLLogSinkImpl(const char* message) {
    CrashHandler::Log("[SDL] %s", message ? message : "");
}
}

extern "C" void CrashHandler_SDLLog(void*, int, SDL_LogPriority, const char* message) {
    SDLLogSinkImpl(message);
}

namespace CrashHandler {

void Install() {
    if (g_installed) return;
    g_installed = true;

    RedirectStdIO();

    EnsureDir(kLogsDir);
    g_log = std::fopen((std::string(kLogsDir) + "/latest.log").c_str(), "w");

    std::set_terminate(TerminateHandler);

#ifdef _WIN32
    SetUnhandledExceptionFilter(TopLevelFilter);
#else
    std::signal(SIGSEGV, SignalHandler);
    std::signal(SIGABRT, SignalHandler);
    std::signal(SIGFPE,  SignalHandler);
    std::signal(SIGILL,  SignalHandler);
#endif

    // Redireciona os logs do SDL para o nosso arquivo/breadcrumbs.
    SDL_LogSetOutputFunction(&CrashHandler_SDLLog, nullptr);

    Log("CrashHandler instalado.");
}

void Log(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    std::lock_guard<std::mutex> lk(g_mutex);
    PushCrumb(buf);
}

void Breadcrumb(const std::string& msg) {
    std::lock_guard<std::mutex> lk(g_mutex);
    PushCrumb(msg);
}

void Shutdown() {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_log) {
        WriteLog("Encerramento normal.");
        std::fclose(g_log);
        g_log = nullptr;
    }
}

}  // namespace CrashHandler