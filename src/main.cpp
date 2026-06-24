#include "third_party/sokol/sokol_app.h"
#include "third_party/sokol/sokol_gfx.h"
#include "third_party/sokol/sokol_glue.h"
#include "third_party/sokol/sokol_log.h"
#include "core/game_controller.h"

static GameController g;

static void init_cb() {
    if (!g.Initialize()) return;
    g.RunSyncInit();
}

static void frame_cb() {
    int w = sapp_width(), h = sapp_height();
    float dt = (float)sapp_frame_duration();
    g.Update(w, h, dt);
    g.Render(w, h);
}

static void event_cb(const sapp_event* ev) {
    g.OnEvent(ev);
}

static void cleanup_cb() {
    g.Shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    g.ParseArgs(argc, argv);

    sapp_desc desc = {};
    desc.init_cb = init_cb;
    desc.frame_cb = frame_cb;
    desc.cleanup_cb = cleanup_cb;
    desc.event_cb = event_cb;
    desc.width = 1280;
    desc.height = 720;
    desc.sample_count = 4;
    desc.window_title = "Warsmash C++ Recreation (sokol)";
    desc.icon.sokol_default = true;
    desc.logger.func = slog_func;
    return desc;
}