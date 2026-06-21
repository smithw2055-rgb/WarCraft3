#pragma once

#include "third_party/sokol/sokol_gfx.h"

class Texture;

namespace Renderer {
    bool Initialize();
    void Shutdown();
    bool IsActive();
    void BeginFrame();
    void EndFrame();
    void SetWireframe(bool enabled);
    void SetClearColor(const float color[4]);

    sg_pass_action GetDefaultPassAction();
}
