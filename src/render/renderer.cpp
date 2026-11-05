#include "renderer.h"
#include "third_party/sokol/sokol_app.h"
#include "third_party/sokol/sokol_glue.h"
#include "third_party/sokol/sokol_log.h"
#include <iostream>

// OpenGL function pointer for glPolygonMode (avoids GL header conflicts)
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef void (APIENTRY *GLPOLYGONMODEPROC)(unsigned int, unsigned int);
static GLPOLYGONMODEPROC glPolygonModePtr = nullptr;
static bool initPolyMode() {
    if (!glPolygonModePtr) {
        HMODULE lib = LoadLibraryA("opengl32.dll");
        if (lib) glPolygonModePtr = (GLPOLYGONMODEPROC)GetProcAddress(lib, "glPolygonMode");
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
        if (lib) glPolygonModePtr = (GLPOLYGONMODEPROC)dlsym(lib, "glPolygonMode");
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
    std::cout << "Renderer: sokol_gfx initialized (backend="
              << (int)sg_query_backend() << ")" << std::endl;
    return true;
}

void Shutdown() {
    sg_shutdown();
    g_active = false;
}

bool IsActive() { return g_active; }

void BeginFrame() {
    sg_reset_state_cache();
    sg_pass pass = {};
    pass.action = g_passAction;
    pass.swapchain = sglue_swapchain();
    sg_begin_pass(&pass);
}

void EndPass() {
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
