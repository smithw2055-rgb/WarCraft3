// Single translation unit for sokol library implementations and swapchain Renderer lifecycle.
// Keep this file touched when diagnosing sokol internals so the implementation recompiles.
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "Dbghelp.lib")
#endif
#include <cstdint>
#include <cstdio>
#include <iostream>

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
#ifndef SOKOL_GLCORE
#define SOKOL_GLCORE
#endif
#include "third_party/sokol/sokol_app.h"
#include "third_party/sokol/sokol_gfx.h"
#include "third_party/sokol/sokol_glue.h"
#include "third_party/sokol/sokol_log.h"
#undef SOKOL_APP_IMPL
#undef SOKOL_GFX_IMPL
#undef SOKOL_GLUE_IMPL
#undef SOKOL_LOG_IMPL

#include "render/rhi_device.h"

// OpenGL function pointer for glPolygonMode (avoids GL header conflicts)
#ifdef _WIN32
typedef void (APIENTRY *GLPOLYGONMODEPROC)(unsigned int, unsigned int);
static GLPOLYGONMODEPROC glPolygonModePtr = nullptr;
static bool initPolyMode() {
    if (!glPolygonModePtr) {
        HMODULE lib = LoadLibraryA("opengl32.dll");
        if (lib) {
            glPolygonModePtr = reinterpret_cast<GLPOLYGONMODEPROC>(GetProcAddress(lib, "glPolygonMode"));
        }
    }
    return glPolygonModePtr != nullptr;
}
#else
#include <dlfcn.h>
typedef void (*GLPOLYGONMODEPROC)(unsigned int, unsigned int);
static GLPOLYGONMODEPROC glPolygonModePtr = nullptr;
static bool initPolyMode() {
    if (!glPolygonModePtr) {
        void* lib = dlopen("libGL.so.1", RTLD_LAZY | RTLD_LOCAL);
        if (lib) {
            glPolygonModePtr = reinterpret_cast<GLPOLYGONMODEPROC>(dlsym(lib, "glPolygonMode"));
        }
    }
    return glPolygonModePtr != nullptr;
}
#endif

#define GL_FRONT_AND_BACK 0x0408
#define GL_LINE 0x1B01
#define GL_FILL 0x1B02

namespace Renderer {

static bool g_active = false;
static sg_pass_action g_passAction = {};

bool Initialize() {
    sg_desc setup_desc = {};
    setup_desc.environment = sglue_environment();
    setup_desc.logger.func = slog_func;
    setup_desc.pipeline_pool_size = 512;
    setup_desc.shader_pool_size = 128;
    setup_desc.image_pool_size = 4096;
    setup_desc.buffer_pool_size = 4096;
    setup_desc.view_pool_size = 16384;
    setup_desc.sampler_pool_size = 4096;
    sg_setup(&setup_desc);
    if (!sg_isvalid()) {
        std::cerr << "Renderer: sg_setup failed!" << std::endl;
        return false;
    }
    g_passAction = {};
    g_passAction.colors[0].load_action = SG_LOADACTION_CLEAR;
    g_passAction.colors[0].clear_value.r = 0.35f;
    g_passAction.colors[0].clear_value.g = 0.05f;
    g_passAction.colors[0].clear_value.b = 0.45f;
    g_passAction.colors[0].clear_value.a = 1.0f;
    g_active = true;
    render::RhiDevice::Instance().ActivateSokolBackend();
    std::cout << "Renderer: sokol_gfx initialized (backend="
              << static_cast<int>(sg_query_backend()) << ")" << std::endl;
    return true;
}

void Shutdown() {
    render::RhiDevice::Instance().DeactivateSokolBackend();
    sg_shutdown();
    g_active = false;
}

bool IsActive() {
    return g_active;
}

void BeginFrame() {
    sg_reset_state_cache();
    sg_pass pass = {};
    pass.action = g_passAction;
    pass.swapchain = sglue_swapchain();
    sg_begin_pass(&pass);
}

void EndPass() {
    render::RhiDevice::Instance().FlushPendingUploads();
    sg_end_pass();
}

void CommitFrame() {
    sg_commit();
}

void EndFrame() {
    EndPass();
    CommitFrame();
}

void SetClearColor(const float color[4]) {
    g_passAction.colors[0].clear_value.r = color[0];
    g_passAction.colors[0].clear_value.g = color[1];
    g_passAction.colors[0].clear_value.b = color[2];
    g_passAction.colors[0].clear_value.a = color[3];
}

void SetWireframe(bool enabled) {
    if (initPolyMode()) {
        sg_reset_state_cache();
        glPolygonModePtr(GL_FRONT_AND_BACK, enabled ? GL_LINE : GL_FILL);
    }
}

sg_pass_action GetDefaultPassAction() {
    return g_passAction;
}

} // namespace Renderer
