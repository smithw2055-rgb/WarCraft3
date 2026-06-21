#pragma once

struct sapp_event;

namespace Platform {
    bool Initialize();
    void Shutdown();
    bool ProcessInput();
    void* GetWindow();
    void GetWindowSize(int& width, int& height);

    bool IsMouseDown(int button);
    float GetMouseDX();
    float GetMouseDY();
    float GetMouseScroll();
    void WarpMouseToCenter();

    bool IsKeyDown(int scancode);
    bool WasKeyPressed(int scancode);
    void UpdateKeys();

    // Called by event callback (sokol_app)
    void HandleEvent(const sapp_event* ev);
}