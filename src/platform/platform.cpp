#include "platform.h"
#include "third_party/sokol/sokol_app.h"
#include <cstring>
#include <iostream>

namespace {
    bool g_mouseDown[3] = {false, false, false};
    float g_mouseDX = 0.0f;
    float g_mouseDY = 0.0f;
    float g_mouseScroll = 0.0f;
    bool g_quitRequested = false;

    uint8_t g_keys[SAPP_MAX_KEYCODES] = {};
    uint8_t g_prevKeys[SAPP_MAX_KEYCODES] = {};
    bool g_keysInitialized = false;

    int g_winWidth = 1280;
    int g_winHeight = 720;
}

namespace Platform {
    void HandleEvent(const sapp_event* ev) {
        switch (ev->type) {
        case SAPP_EVENTTYPE_QUIT_REQUESTED:
            g_quitRequested = true;
            break;
        case SAPP_EVENTTYPE_RESIZED:
            g_winWidth = ev->framebuffer_width;
            g_winHeight = ev->framebuffer_height;
            break;
        case SAPP_EVENTTYPE_MOUSE_DOWN:
            if (ev->mouse_button >= 1 && ev->mouse_button <= 3)
                g_mouseDown[ev->mouse_button - 1] = true;
            break;
        case SAPP_EVENTTYPE_MOUSE_UP:
            if (ev->mouse_button >= 1 && ev->mouse_button <= 3)
                g_mouseDown[ev->mouse_button - 1] = false;
            break;
        case SAPP_EVENTTYPE_MOUSE_MOVE:
            g_mouseDX += ev->mouse_dx;
            g_mouseDY += ev->mouse_dy;
            break;
        case SAPP_EVENTTYPE_MOUSE_SCROLL:
            g_mouseScroll += ev->scroll_y;
            break;
        case SAPP_EVENTTYPE_KEY_DOWN:
            if (ev->key_code >= 0 && ev->key_code < SAPP_MAX_KEYCODES)
                g_keys[ev->key_code] = 1;
            break;
        case SAPP_EVENTTYPE_KEY_UP:
            if (ev->key_code >= 0 && ev->key_code < SAPP_MAX_KEYCODES)
                g_keys[ev->key_code] = 0;
            break;
        default:
            break;
        }
    }

    bool Initialize() {
        if (!g_keysInitialized) {
            memset(g_keys, 0, sizeof(g_keys));
            memset(g_prevKeys, 0, sizeof(g_prevKeys));
            g_keysInitialized = true;
        }
        return true;
    }

    void Shutdown() {}

    bool ProcessInput() {
        memcpy(g_prevKeys, g_keys, SAPP_MAX_KEYCODES);
        g_mouseDX = 0.0f;
        g_mouseDY = 0.0f;
        g_mouseScroll = 0.0f;
        return !g_quitRequested;
    }

    void* GetWindow() { return nullptr; }

    void GetWindowSize(int& width, int& height) {
        width = g_winWidth;
        height = g_winHeight;
    }

    bool IsMouseDown(int button) {
        if (button < 0 || button > 2) return false;
        return g_mouseDown[button];
    }

    float GetMouseDX() { return g_mouseDX; }
    float GetMouseDY() { return g_mouseDY; }
    float GetMouseScroll() { return g_mouseScroll; }

    void WarpMouseToCenter() {}

    bool IsKeyDown(int scancode) {
        return scancode >= 0 && scancode < SAPP_MAX_KEYCODES && g_keys[scancode];
    }

    bool WasKeyPressed(int scancode) {
        if (scancode < 0 || scancode >= SAPP_MAX_KEYCODES) return false;
        return g_keys[scancode] && !g_prevKeys[scancode];
    }

    void UpdateKeys() {}
}
