// Single translation unit for sokol library implementations.
// Keep this file touched when diagnosing sokol internals so the implementation recompiles.
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "Dbghelp.lib")
#endif
#include <cstdint>
#include <cstdio>

#if defined(_WIN32)
static void codex_log_invalid_sokol_view(uint32_t view_id) {
    static bool symbolsReady = false;
    HANDLE process = GetCurrentProcess();
    if (!symbolsReady) {
        SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
        symbolsReady = SymInitialize(process, nullptr, TRUE) == TRUE;
    }

    void* frames[16] = {};
    USHORT frameCount = CaptureStackBackTrace(0, (ULONG)(sizeof(frames) / sizeof(frames[0])), frames, nullptr);
    std::fprintf(stderr, "sokol invalid view stack for id=0x%08x frames=%u\n", view_id, (unsigned)frameCount);

    for (USHORT i = 0; i < frameCount; ++i) {
        DWORD64 address = (DWORD64)(uintptr_t)frames[i];
        char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;

        DWORD64 displacement = 0;
        IMAGEHLP_LINE64 line = {};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisplacement = 0;

        const BOOL hasSymbol = symbolsReady && SymFromAddr(process, address, &displacement, symbol);
        const BOOL hasLine = symbolsReady && SymGetLineFromAddr64(process, address, &lineDisplacement, &line);
        if (hasSymbol && hasLine) {
            std::fprintf(stderr, "  [%u] %s + 0x%llx (%s:%lu)\n",
                (unsigned)i, symbol->Name, (unsigned long long)displacement, line.FileName, line.LineNumber);
        } else if (hasSymbol) {
            std::fprintf(stderr, "  [%u] %s + 0x%llx\n",
                (unsigned)i, symbol->Name, (unsigned long long)displacement);
        } else {
            std::fprintf(stderr, "  [%u] 0x%p\n", (unsigned)i, frames[i]);
        }
    }
}
#else
static void codex_log_invalid_sokol_view(uint32_t) {
}
#endif

#define SOKOL_APP_IMPL
#define SOKOL_GFX_IMPL
#define SOKOL_GLUE_IMPL
#define SOKOL_LOG_IMPL
#define SOKOL_GLCORE
#include "third_party/sokol/sokol_app.h"
#include "third_party/sokol/sokol_gfx.h"
#include "third_party/sokol/sokol_glue.h"
#include "third_party/sokol/sokol_log.h"
