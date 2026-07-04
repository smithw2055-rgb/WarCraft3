#include "NoesisHudHost.h"

#include "GluePanels.h"
#include "MdxSceneControl.h"

#include "third_party/sokol/sokol_app.h"
#include "third_party/sokol/sokol_glue.h"
#include <NsCore/DynamicCast.h>
#include <NsCore/Aero.h>
#include <NsCore/RegisterComponent.h>
#include <NsGui/BitmapImage.h>
#include <NsGui/Border.h>
#include <NsGui/Canvas.h>
#include <NsGui/Enums.h>
#include <NsGui/FrameworkElement.h>
#include <NsGui/Image.h>
#include <NsGui/IRenderer.h>
#include <NsGui/IntegrationAPI.h>
#include <NsGui/SolidColorBrush.h>
#include <NsGui/TextBlock.h>
#include <NsGui/TextBox.h>
#include <NsGui/Uri.h>
#include <NsGui/UserControl.h>
#include <NsGui/Visual.h>
#include <NsDrawing/Color.h>
#include <NsApp/Launcher.h>
#include <NsApp/LocalFontProvider.h>
#include <NsApp/LocalTextureProvider.h>
#include <NsApp/LocalXamlProvider.h>
#include <NsApp/ThemeProviders.h>
#include <NsRender/GLFactory.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifndef NS_LICENSE_NAME
#define NS_LICENSE_NAME ""
#endif

#ifndef NS_LICENSE_KEY
#define NS_LICENSE_KEY ""
#endif

namespace {

#ifdef _WIN32
using GLBindFramebufferProc = void (APIENTRY*)(unsigned int target, unsigned int framebuffer);
using GLViewportProc = void (APIENTRY*)(int x, int y, int width, int height);
using GLDisableProc = void (APIENTRY*)(unsigned int cap);
using GLColorMaskProc = void (APIENTRY*)(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha);
using GLGetIntegervProc = void (APIENTRY*)(unsigned int pname, int* data);
using GLClearStencilProc = void (APIENTRY*)(int s);
using GLClearProc = void (APIENTRY*)(unsigned int mask);
using GLGetErrorProc = unsigned int (APIENTRY*)();
using GLStencilMaskProc = void (APIENTRY*)(unsigned int mask);
using GLDepthMaskProc = void (APIENTRY*)(unsigned char flag);
using GLEnableProc = void (APIENTRY*)(unsigned int cap);
using GLScissorProc = void (APIENTRY*)(int x, int y, int width, int height);
using GLClearColorProc = void (APIENTRY*)(float red, float green, float blue, float alpha);

constexpr unsigned int GL_FRAMEBUFFER_EXT = 0x8D40;
constexpr unsigned int GL_FRAMEBUFFER_BINDING_EXT = 0x8CA6;
constexpr unsigned int GL_SCISSOR_TEST_EXT = 0x0C11;
constexpr unsigned int GL_DEPTH_TEST_EXT = 0x0B71;
constexpr unsigned int GL_CULL_FACE_EXT = 0x0B44;
constexpr unsigned int GL_DEPTH_BITS_EXT = 0x0D56;
constexpr unsigned int GL_STENCIL_BITS_EXT = 0x0D57;
constexpr unsigned int GL_COLOR_BUFFER_BIT_EXT = 0x00004000;
constexpr unsigned int GL_STENCIL_BUFFER_BIT_EXT = 0x00000400;
constexpr unsigned int GL_NO_ERROR_EXT = 0;

void* LoadGLProc(const char* name) {
    void* proc = reinterpret_cast<void*>(wglGetProcAddress(name));
    if (proc) return proc;

    HMODULE lib = GetModuleHandleA("opengl32.dll");
    return lib ? reinterpret_cast<void*>(GetProcAddress(lib, name)) : nullptr;
}

void RestoreDefaultFramebufferForNoesis(int width, int height) {
    static GLBindFramebufferProc bindFramebuffer =
        reinterpret_cast<GLBindFramebufferProc>(LoadGLProc("glBindFramebuffer"));
    static GLViewportProc viewport =
        reinterpret_cast<GLViewportProc>(LoadGLProc("glViewport"));
    static GLDisableProc disable =
        reinterpret_cast<GLDisableProc>(LoadGLProc("glDisable"));
    static GLColorMaskProc colorMask =
        reinterpret_cast<GLColorMaskProc>(LoadGLProc("glColorMask"));
    static GLStencilMaskProc stencilMask =
        reinterpret_cast<GLStencilMaskProc>(LoadGLProc("glStencilMask"));
    static GLDepthMaskProc depthMask =
        reinterpret_cast<GLDepthMaskProc>(LoadGLProc("glDepthMask"));
    static GLClearStencilProc clearStencil =
        reinterpret_cast<GLClearStencilProc>(LoadGLProc("glClearStencil"));
    static GLClearProc clear =
        reinterpret_cast<GLClearProc>(LoadGLProc("glClear"));

    if (bindFramebuffer) bindFramebuffer(GL_FRAMEBUFFER_EXT, sglue_swapchain().gl.framebuffer);
    if (viewport) viewport(0, 0, width, height);
    if (disable) {
        disable(GL_SCISSOR_TEST_EXT);
        disable(GL_DEPTH_TEST_EXT);
        disable(GL_CULL_FACE_EXT);
    }
    if (colorMask) colorMask(1, 1, 1, 1);
    if (stencilMask) stencilMask(0xff);
    if (depthMask) depthMask(0);
    if (clearStencil && clear) {
        clearStencil(0);
        clear(GL_STENCIL_BUFFER_BIT_EXT);
    }
}

int CurrentFramebufferBinding() {
    static GLGetIntegervProc getIntegerv =
        reinterpret_cast<GLGetIntegervProc>(LoadGLProc("glGetIntegerv"));
    int value = -1;
    if (getIntegerv) getIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &value);
    return value;
}

int GLInteger(unsigned int pname) {
    static GLGetIntegervProc getIntegerv =
        reinterpret_cast<GLGetIntegervProc>(LoadGLProc("glGetIntegerv"));
    int value = -1;
    if (getIntegerv) getIntegerv(pname, &value);
    return value;
}

unsigned int DrainGLError() {
    static GLGetErrorProc getError =
        reinterpret_cast<GLGetErrorProc>(LoadGLProc("glGetError"));
    if (!getError) return GL_NO_ERROR_EXT;

    unsigned int last = GL_NO_ERROR_EXT;
    for (;;) {
        unsigned int error = getError();
        if (error == GL_NO_ERROR_EXT) return last;
        last = error;
    }
}

void DrawGLProbeMarker(int width, int height) {
    static GLEnableProc enable = reinterpret_cast<GLEnableProc>(LoadGLProc("glEnable"));
    static GLDisableProc disable = reinterpret_cast<GLDisableProc>(LoadGLProc("glDisable"));
    static GLScissorProc scissor = reinterpret_cast<GLScissorProc>(LoadGLProc("glScissor"));
    static GLClearColorProc clearColor =
        reinterpret_cast<GLClearColorProc>(LoadGLProc("glClearColor"));
    static GLClearProc clear = reinterpret_cast<GLClearProc>(LoadGLProc("glClear"));
    static GLColorMaskProc colorMask =
        reinterpret_cast<GLColorMaskProc>(LoadGLProc("glColorMask"));

    if (!enable || !disable || !scissor || !clearColor || !clear) return;

    const int markerW = std::max(80, width / 8);
    const int markerH = std::max(40, height / 14);
    const int x = 24;
    const int y = std::max(0, height - markerH - 24);

    if (colorMask) colorMask(1, 1, 1, 1);
    enable(GL_SCISSOR_TEST_EXT);
    scissor(x, y, markerW, markerH);
    clearColor(0.0f, 1.0f, 0.0f, 1.0f);
    clear(GL_COLOR_BUFFER_BIT_EXT);
    disable(GL_SCISSOR_TEST_EXT);
}
#else
void RestoreDefaultFramebufferForNoesis(int, int) {}
int CurrentFramebufferBinding() { return -1; }
int GLInteger(unsigned int) { return -1; }
unsigned int DrainGLError() { return 0; }
void DrawGLProbeMarker(int, int) {}
#endif

void HudDiagLog(const char* fmt, ...) {
    char buffer[4096];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    std::fprintf(stderr, "%s\n", buffer);

    std::error_code ec;
    std::filesystem::path logPath = std::filesystem::path(PROJECT_SOURCE_DIR) / "Logs" / "aero-hud.log";
    std::filesystem::create_directories(logPath.parent_path(), ec);
    std::ofstream log(logPath, std::ios::app);
    if (log) {
        log << buffer << '\n';
    }
}

int ToLogicalPixels(int physicalPixels, float dpiScale) {
    if (physicalPixels <= 0) return physicalPixels;
    if (dpiScale <= 0.0f) return physicalPixels;
    return std::max(1, (int)(physicalPixels / dpiScale + 0.5f));
}

constexpr int kHudLayoutWidth = 1536;
constexpr int kHudLayoutHeight = 1152;

enum GameMenuActionIndex {
    kGameMenuPause = 0,
    kGameMenuSaveGame,
    kGameMenuLoadGame,
    kGameMenuOptions,
    kGameMenuHelp,
    kGameMenuTips,
    kGameMenuEndGame,
    kGameMenuReturnToGame,
    kGameMenuRestart,
    kGameMenuQuitMission,
    kGameMenuExitProgram,
    kGameMenuPrevious,
    kGameMenuConfirmExit,
    kGameMenuCancelConfirm,
    kGameMenuGameplay,
    kGameMenuVideo,
    kGameMenuSound,
    kGameMenuNetwork,
    kGameMenuOptionsBack,
    kGameMenuOptionsOK,
    kGameMenuOptionsCancel,
    kGameMenuSaveGameSave,
    kGameMenuSaveGameDelete,
    kGameMenuSaveGameCancel,
    kGameMenuLoadGameLoad,
    kGameMenuLoadGameCancel,
    kGameMenuOverwriteConfirm,
    kGameMenuOverwriteCancel,
    kGameMenuDeleteConfirm,
    kGameMenuDeleteCancel,
    kGameMenuHelpOK,
    kGameMenuTipsBack,
    kGameMenuTipsNext,
    kGameMenuTipsOK,
    kGameMenuOptionGameSpeedSlider,
    kGameMenuOptionMouseScrollSlider,
    kGameMenuOptionMouseScrollDisable,
    kGameMenuOptionKeyScrollSlider,
    kGameMenuOptionEnhancedTooltips,
    kGameMenuOptionSubgroupModifier,
    kGameMenuOptionFormationToggle,
    kGameMenuOptionHealthBars,
    kGameMenuOptionGammaSlider,
    kGameMenuOptionParticles,
    kGameMenuOptionLights,
    kGameMenuOptionShadows,
    kGameMenuOptionOcclusion,
    kGameMenuOptionSoundVolume,
    kGameMenuOptionSoundVolumeSlider,
    kGameMenuOptionMusicVolume,
    kGameMenuOptionMusicVolumeSlider,
    kGameMenuOptionAmbientSounds,
    kGameMenuOptionMovementSounds,
    kGameMenuOptionUnitSounds,
    kGameMenuOptionSubtitles,
    kGameMenuOptionEnvironmentalEffects,
    kGameMenuOptionPositionalAudio,
    kGameMenuOptionLowLatency,
    kGameMenuOptionHighLatency,
    kGameMenuOptionExtraHighLatency,
    kGameMenuOptionParticlesLow,
    kGameMenuOptionParticlesMedium,
    kGameMenuOptionParticlesHigh,
    kGameMenuOptionLightsLow,
    kGameMenuOptionLightsMedium,
    kGameMenuOptionLightsHigh,
    kGameMenuOptionShadowsOff,
    kGameMenuOptionShadowsOn,
    kGameMenuOptionOcclusionOff,
    kGameMenuOptionOcclusionOn,
};

constexpr const char* kGameMenuActionElementNames[] = {
    "GameMenuButtonPause",
    "GameMenuButtonSaveGame",
    "GameMenuButtonLoadGame",
    "GameMenuButtonOptions",
    "GameMenuButtonHelp",
    "GameMenuButtonTips",
    "GameMenuButtonEndGame",
    "GameMenuButtonReturnToGame",
    "GameMenuButtonRestart",
    "GameMenuButtonQuitMission",
    "GameMenuButtonExitProgram",
    "GameMenuButtonPrevious",
    "GameMenuButtonConfirmExit",
    "GameMenuButtonCancelConfirm",
    "GameMenuButtonGameplay",
    "GameMenuButtonVideo",
    "GameMenuButtonSound",
    "GameMenuButtonNetwork",
    "GameMenuButtonOptionsBack",
    "GameMenuButtonOptionsOK",
    "GameMenuButtonOptionsCancel",
    "GameMenuButtonSaveGameSave",
    "GameMenuButtonSaveGameDelete",
    "GameMenuButtonSaveGameCancel",
    "GameMenuButtonLoadGameLoad",
    "GameMenuButtonLoadGameCancel",
    "GameMenuButtonOverwriteConfirm",
    "GameMenuButtonOverwriteCancel",
    "GameMenuButtonDeleteConfirm",
    "GameMenuButtonDeleteCancel",
    "GameMenuButtonHelpOK",
    "GameMenuButtonTipsBack",
    "GameMenuButtonTipsNext",
    "GameMenuButtonTipsOK",
    "GameMenuOptionGameSpeedSlider",
    "GameMenuOptionMouseScrollSlider",
    "GameMenuOptionMouseScrollDisable",
    "GameMenuOptionKeyScrollSlider",
    "GameMenuOptionEnhancedTooltips",
    "GameMenuOptionSubgroupModifier",
    "GameMenuOptionFormationToggle",
    "GameMenuOptionHealthBars",
    "GameMenuOptionGammaSlider",
    "GameMenuOptionParticles",
    "GameMenuOptionLights",
    "GameMenuOptionShadows",
    "GameMenuOptionOcclusion",
    "GameMenuOptionSoundVolume",
    "GameMenuOptionSoundVolumeSlider",
    "GameMenuOptionMusicVolume",
    "GameMenuOptionMusicVolumeSlider",
    "GameMenuOptionAmbientSounds",
    "GameMenuOptionMovementSounds",
    "GameMenuOptionUnitSounds",
    "GameMenuOptionSubtitles",
    "GameMenuOptionEnvironmentalEffects",
    "GameMenuOptionPositionalAudio",
    "GameMenuOptionLowLatency",
    "GameMenuOptionHighLatency",
    "GameMenuOptionExtraHighLatency",
    "GameMenuOptionParticlesLow",
    "GameMenuOptionParticlesMedium",
    "GameMenuOptionParticlesHigh",
    "GameMenuOptionLightsLow",
    "GameMenuOptionLightsMedium",
    "GameMenuOptionLightsHigh",
    "GameMenuOptionShadowsOff",
    "GameMenuOptionShadowsOn",
    "GameMenuOptionOcclusionOff",
    "GameMenuOptionOcclusionOn",
};

constexpr int kGameMenuActionElementNameCount =
    (int)(sizeof(kGameMenuActionElementNames) / sizeof(kGameMenuActionElementNames[0]));

constexpr const char* kGameMenuButtonSuffixes[] = {
    "Pause",
    "SaveGame",
    "LoadGame",
    "Options",
    "Help",
    "Tips",
    "EndGame",
    "ReturnToGame",
    "Restart",
    "QuitMission",
    "ExitProgram",
    "Previous",
    "ConfirmExit",
    "CancelConfirm",
    "Gameplay",
    "Video",
    "Sound",
    "Network",
    "OptionsBack",
    "OptionsOK",
    "OptionsCancel",
    "SaveGameSave",
    "SaveGameDelete",
    "SaveGameCancel",
    "LoadGameLoad",
    "LoadGameCancel",
    "OverwriteConfirm",
    "OverwriteCancel",
    "DeleteConfirm",
    "DeleteCancel",
    "HelpOK",
    "TipsBack",
    "TipsNext",
    "TipsOK",
};

bool IsGameMenuOptionCheckboxAction(int action) {
    switch (action) {
    case kGameMenuOptionMouseScrollDisable:
    case kGameMenuOptionEnhancedTooltips:
    case kGameMenuOptionSubgroupModifier:
    case kGameMenuOptionFormationToggle:
    case kGameMenuOptionHealthBars:
    case kGameMenuOptionSoundVolume:
    case kGameMenuOptionMusicVolume:
    case kGameMenuOptionAmbientSounds:
    case kGameMenuOptionMovementSounds:
    case kGameMenuOptionUnitSounds:
    case kGameMenuOptionSubtitles:
    case kGameMenuOptionEnvironmentalEffects:
    case kGameMenuOptionPositionalAudio:
        return true;
    default:
        return false;
    }
}

bool IsGameMenuOptionRadioAction(int action) {
    switch (action) {
    case kGameMenuOptionLowLatency:
    case kGameMenuOptionHighLatency:
    case kGameMenuOptionExtraHighLatency:
        return true;
    default:
        return false;
    }
}

bool IsGameMenuOptionSliderAction(int action) {
    switch (action) {
    case kGameMenuOptionGameSpeedSlider:
    case kGameMenuOptionMouseScrollSlider:
    case kGameMenuOptionKeyScrollSlider:
    case kGameMenuOptionGammaSlider:
    case kGameMenuOptionSoundVolumeSlider:
    case kGameMenuOptionMusicVolumeSlider:
        return true;
    default:
        return false;
    }
}

bool IsGameMenuOptionPopupAction(int action) {
    switch (action) {
    case kGameMenuOptionParticles:
    case kGameMenuOptionLights:
    case kGameMenuOptionShadows:
    case kGameMenuOptionOcclusion:
        return true;
    default:
        return false;
    }
}

int GameMenuPopupParentAction(int action) {
    switch (action) {
    case kGameMenuOptionParticlesLow:
    case kGameMenuOptionParticlesMedium:
    case kGameMenuOptionParticlesHigh:
        return kGameMenuOptionParticles;
    case kGameMenuOptionLightsLow:
    case kGameMenuOptionLightsMedium:
    case kGameMenuOptionLightsHigh:
        return kGameMenuOptionLights;
    case kGameMenuOptionShadowsOff:
    case kGameMenuOptionShadowsOn:
        return kGameMenuOptionShadows;
    case kGameMenuOptionOcclusionOff:
    case kGameMenuOptionOcclusionOn:
        return kGameMenuOptionOcclusion;
    default:
        return -1;
    }
}

int GameMenuPopupItemValue(int action) {
    switch (action) {
    case kGameMenuOptionParticlesLow:
    case kGameMenuOptionLightsLow:
    case kGameMenuOptionShadowsOff:
    case kGameMenuOptionOcclusionOff:
        return 0;
    case kGameMenuOptionParticlesMedium:
    case kGameMenuOptionLightsMedium:
    case kGameMenuOptionShadowsOn:
    case kGameMenuOptionOcclusionOn:
        return 1;
    case kGameMenuOptionParticlesHigh:
    case kGameMenuOptionLightsHigh:
        return 2;
    default:
        return 0;
    }
}

const char* GameMenuActionElementName(int action) {
    if (action < 0 || action >= kGameMenuActionElementNameCount) return "";
    return kGameMenuActionElementNames[action];
}

const char* GameMenuOptionCycleText(int action, int index) {
    static constexpr const char* kLowMediumHigh[] = { "LOW", "MEDIUM", "HIGH" };
    static constexpr const char* kOffOn[] = { "OFF", "ON" };
    switch (action) {
    case kGameMenuOptionParticles:
    case kGameMenuOptionLights:
        return kLowMediumHigh[std::clamp(index, 0, 2)];
    case kGameMenuOptionShadows:
    case kGameMenuOptionOcclusion:
        return kOffOn[std::clamp(index, 0, 1)];
    default:
        return "";
    }
}

float JavaFdfToX(int logicalWidth, float fdfX) {
    return ((float)logicalWidth * fdfX) / 0.8f;
}

float JavaFdfToY(int logicalHeight, float fdfY) {
    return ((float)logicalHeight * fdfY) / 0.6f;
}

float FdfSquareToPixels(int logicalWidth, int logicalHeight, float fdfSize) {
    return std::min(JavaFdfToX(logicalWidth, fdfSize), JavaFdfToY(logicalHeight, fdfSize));
}

ScreenRect BuildFallbackMinimapRect(int logicalWidth, int logicalHeight, float dpiScale) {
    if (logicalWidth <= 0 || logicalHeight <= 0) return {};

    const float scale = dpiScale > 0.0f ? dpiScale : 1.0f;
    const float consoleHeight = std::clamp(JavaFdfToY(logicalHeight, 0.176f), 170.0f, 330.0f);

    const float consoleX = 8.0f;
    const float consoleY = (float)logicalHeight - 6.0f - consoleHeight;
    const float contentX = consoleX + 3.0f + 12.0f + 3.0f + 6.0f;
    const float contentY = consoleY + 3.0f + 10.0f + 38.0f + 8.0f + 3.0f + 6.0f;
    const float contentW = 224.0f - 10.0f - 6.0f - 12.0f;
    const float contentH = consoleHeight - 10.0f - 12.0f - 38.0f - 8.0f - 6.0f - 12.0f;

    return ScreenRect{
        contentX * scale,
        contentY * scale,
        std::max(1.0f, contentW) * scale,
        std::max(1.0f, contentH) * scale
    };
}

void InstallLogHandler() {
    static bool installed = false;
    if (installed) return;
    installed = true;

    Aero::GUI::SetLogHandler([](const char*, uint32_t, uint32_t level, const char*, const char* msg) {
        const char* prefixes[] = {"T", "D", "I", "W", "E"};
        HudDiagLog("[AERO/%s] %s", prefixes[level], msg);
    });
}

Aero::Key MapKey(int sokolKey) {
    switch (sokolKey) {
    case SAPP_KEYCODE_BACKSPACE: return Aero::Key_Back;
    case SAPP_KEYCODE_TAB: return Aero::Key_Tab;
    case SAPP_KEYCODE_ENTER: return Aero::Key_Return;
    case SAPP_KEYCODE_ESCAPE: return Aero::Key_Escape;
    case SAPP_KEYCODE_SPACE: return Aero::Key_Space;
    case SAPP_KEYCODE_DELETE: return Aero::Key_Delete;
    case SAPP_KEYCODE_LEFT: return Aero::Key_Left;
    case SAPP_KEYCODE_RIGHT: return Aero::Key_Right;
    case SAPP_KEYCODE_UP: return Aero::Key_Up;
    case SAPP_KEYCODE_DOWN: return Aero::Key_Down;
    case SAPP_KEYCODE_HOME: return Aero::Key_Home;
    case SAPP_KEYCODE_END: return Aero::Key_End;
    case SAPP_KEYCODE_PAGE_UP: return Aero::Key_PageUp;
    case SAPP_KEYCODE_PAGE_DOWN: return Aero::Key_PageDown;
    case SAPP_KEYCODE_INSERT: return Aero::Key_Insert;
    case SAPP_KEYCODE_F1: return Aero::Key_F1;
    case SAPP_KEYCODE_F2: return Aero::Key_F2;
    case SAPP_KEYCODE_F3: return Aero::Key_F3;
    case SAPP_KEYCODE_F4: return Aero::Key_F4;
    case SAPP_KEYCODE_F5: return Aero::Key_F5;
    case SAPP_KEYCODE_F6: return Aero::Key_F6;
    case SAPP_KEYCODE_F7: return Aero::Key_F7;
    case SAPP_KEYCODE_F8: return Aero::Key_F8;
    case SAPP_KEYCODE_F9: return Aero::Key_F9;
    case SAPP_KEYCODE_F10: return Aero::Key_F10;
    case SAPP_KEYCODE_F11: return Aero::Key_F11;
    case SAPP_KEYCODE_F12: return Aero::Key_F12;
    default: break;
    }

    if (sokolKey >= SAPP_KEYCODE_A && sokolKey <= SAPP_KEYCODE_Z) {
        return static_cast<Aero::Key>(Aero::Key_A + (sokolKey - SAPP_KEYCODE_A));
    }
    if (sokolKey >= SAPP_KEYCODE_0 && sokolKey <= SAPP_KEYCODE_9) {
        return static_cast<Aero::Key>(Aero::Key_D0 + (sokolKey - SAPP_KEYCODE_0));
    }
    return Aero::Key_None;
}

template<class T>
T* FindAs(Aero::FrameworkElement* root, const char* name) {
    return root ? Aero::DynamicCast<T*>(root->FindName(name)) : nullptr;
}

template<class T>
T* FindFirstAs(Aero::FrameworkElement* root, const char* primaryName, const char* fallbackName) {
    if (T* primary = FindAs<T>(root, primaryName)) return primary;
    return FindAs<T>(root, fallbackName);
}

std::string JoinLines(const std::vector<std::string>& lines) {
    std::ostringstream ss;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) ss << '\n';
        ss << lines[i];
    }
    return ss.str();
}

std::string TrimAscii(std::string s) {
    const auto first = std::find_if_not(s.begin(), s.end(), [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    });
    const auto last = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    }).base();
    if (first >= last) {
        return {};
    }
    return std::string(first, last);
}

bool ExpandXamlIncludes(const std::filesystem::path& path, int depth, std::string& out) {
    if (depth > 16) {
        HudDiagLog("AeroHudHost: XAML include depth exceeded at '%s'", path.string().c_str());
        return false;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        HudDiagLog("AeroHudHost: unable to read XAML include '%s'", path.string().c_str());
        return false;
    }

    std::string line;
    while (std::getline(in, line)) {
        const std::string marker = "<!-- @include ";
        const size_t markerPos = line.find(marker);
        if (markerPos == std::string::npos) {
            out += line;
            out += '\n';
            continue;
        }

        const size_t pathStart = markerPos + marker.size();
        const size_t pathEnd = line.find("-->", pathStart);
        if (pathEnd == std::string::npos) {
            HudDiagLog("AeroHudHost: malformed XAML include in '%s': %s", path.string().c_str(), line.c_str());
            return false;
        }

        const std::string includeName = TrimAscii(line.substr(pathStart, pathEnd - pathStart));
        const std::filesystem::path includePath = path.parent_path() / std::filesystem::path(includeName);
        out += "<!-- begin include ";
        out += includeName;
        out += " -->\n";
        if (!ExpandXamlIncludes(includePath, depth + 1, out)) {
            return false;
        }
        out += "<!-- end include ";
        out += includeName;
        out += " -->\n";
    }

    return true;
}

bool XamlHasIncludeMarker(const std::filesystem::path& path, bool& hasIncludes) {
    hasIncludes = false;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        HudDiagLog("AeroHudHost: unable to read XAML '%s'", path.string().c_str());
        return false;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.find("<!-- @include ") != std::string::npos) {
            hasIncludes = true;
            return true;
        }
    }
    return true;
}

bool ComposeXamlForNoesis(const std::string& assetRoot,
                          const std::string& sourceRelativePath,
                          std::string& composedRelativePath) {
    const std::filesystem::path sourcePath = std::filesystem::path(assetRoot) / sourceRelativePath;
    bool hasIncludes = false;
    if (!XamlHasIncludeMarker(sourcePath, hasIncludes)) {
        return false;
    }
    if (!hasIncludes) {
        composedRelativePath = sourceRelativePath;
        return true;
    }

    std::string composed;
    if (!ExpandXamlIncludes(sourcePath, 0, composed)) {
        return false;
    }

    const std::filesystem::path composedPath =
        sourcePath.parent_path() / (sourcePath.stem().string() + ".composed" + sourcePath.extension().string());
    std::ofstream out(composedPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        HudDiagLog("AeroHudHost: unable to write composed XAML '%s'", composedPath.string().c_str());
        return false;
    }
    out << composed;
    out.close();
    if (!out) {
        HudDiagLog("AeroHudHost: failed while writing composed XAML '%s'", composedPath.string().c_str());
        return false;
    }

    std::error_code ec;
    std::filesystem::path relative = std::filesystem::relative(composedPath, assetRoot, ec);
    if (ec) {
        relative = composedPath;
    }
    composedRelativePath = relative.generic_string();
    return true;
}

std::array<int, 5> TooltipCostValues(const HudResourceCosts& costs) {
    return { costs.gold, costs.lumber, costs.stone, costs.mana, costs.food };
}

Aero::Ptr<Aero::SolidColorBrush> MakeBrushFromArgb(uint32_t argb) {
    const int a = (argb >> 24) & 0xFF;
    const int r = (argb >> 16) & 0xFF;
    const int g = (argb >> 8) & 0xFF;
    const int b = argb & 0xFF;
    return Aero::MakePtr<Aero::SolidColorBrush>(Aero::Color(r, g, b, a));
}

uint32_t HealthBarTintArgb(float ratio) {
    ratio = std::clamp(ratio, 0.0f, 1.0f);
    const int r = (int)std::round(std::min(1.0f, 2.0f - (ratio * 2.0f)) * 255.0f);
    const int g = (int)std::round(std::min(1.0f, ratio * 2.0f) * 255.0f);
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8);
}

bool IsUpperButtonEnabled(int index) {
    return index == 1;
}

const char* UpperButtonDebugName(int index) {
    static constexpr const char* kNames[] = { "quests", "menu", "allies", "chat" };
    if (index < 0 || index >= (int)(sizeof(kNames) / sizeof(kNames[0]))) return "";
    return kNames[index];
}

int UpperButtonIndexForDebugName(const std::string& name) {
    std::string key = name;
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    if (key.rfind("upperbuttonbar", 0) == 0) {
        key.erase(0, 14);
    }
    if (key.size() >= 6 && key.compare(key.size() - 6, 6, "button") == 0) {
        key.erase(key.size() - 6);
    }
    if (key == "quest") key = "quests";
    for (int i = 0; i < 4; ++i) {
        if (key == UpperButtonDebugName(i)) return i;
    }
    return -1;
}

} // namespace

NoesisHudHost::~NoesisHudHost() {
    Shutdown();
}

bool NoesisHudHost::Initialize(const std::string& assetRoot, int width, int height,
                               const std::string& xamlPath, MpqManager* mpq, int gameVersion) {
    if (initialized_) return true;

    assetRoot_ = assetRoot;
    std::string resolvedXamlPath = xamlPath.empty() ? "assets/ui/Hud.xaml" : xamlPath;
    std::replace(resolvedXamlPath.begin(), resolvedXamlPath.end(), '\\', '/');
    if (resolvedXamlPath == "assets/noesis/Hud.xaml" || resolvedXamlPath == "./assets/noesis/Hud.xaml") {
        HudDiagLog("AeroHudHost: remapping legacy HUD XAML '%s' to 'assets/ui/Hud.xaml'",
                   resolvedXamlPath.c_str());
        resolvedXamlPath = "assets/ui/Hud.xaml";
    }
    dpiScale_ = sapp_dpi_scale();
    physicalWidth_ = std::max(1, width);
    physicalHeight_ = std::max(1, height);
    width_ = ToLogicalPixels(width, dpiScale_);
    height_ = ToLogicalPixels(height, dpiScale_);
    HudDiagLog(
        "AeroHudHost: initializing xaml='%s' assetRoot='%s' physical=%dx%d logical=%dx%d dpi=%.2f",
        resolvedXamlPath.c_str(), assetRoot_.c_str(), physicalWidth_, physicalHeight_, width_, height_, dpiScale_);

    InstallLogHandler();
    Aero::GUI::SetLicense(NS_LICENSE_NAME, NS_LICENSE_KEY);
    Aero::GUI::Init();
    guiInitialized_ = true;
    War3UI::MdxSceneControl::SetSharedContext(mpq, gameVersion);
    Aero::RegisterComponent<War3UI::GlueMenuControl>();

    AeroApp::Launcher::RegisterAppComponents();
    Aero::RegisterComponent<War3UI::MdxSceneControl>();
    Aero::RegisterComponent<War3UI::MainMenuBackdropControl>();
    Aero::RegisterComponent<War3UI::MainMenuPanelControl>();
    Aero::RegisterComponent<War3UI::SinglePlayerPanelControl>();
    Aero::RegisterComponent<War3UI::CampaignPanelControl>();
    Aero::RegisterComponent<War3UI::CustomGamePanelControl>();
    Aero::RegisterComponent<War3UI::GlueOptionsPanelControl>();
    Aero::RegisterComponent<War3UI::GlueBattleNetPanelControl>();
    Aero::RegisterComponent<War3UI::GlueLocalAreaNetworkPanelControl>();
    Aero::GUI::SetXamlProvider(Aero::MakePtr<AeroApp::LocalXamlProvider>(assetRoot_.c_str()));
    Aero::GUI::SetTextureProvider(Aero::MakePtr<AeroApp::LocalTextureProvider>(assetRoot_.c_str()));
    Aero::GUI::SetFontProvider(Aero::MakePtr<AeroApp::LocalFontProvider>(assetRoot_.c_str()));
    AeroApp::SetThemeProviders();
    Aero::GUI::LoadApplicationResources(AeroApp::Theme::DarkBlue());

    std::string loadXamlPath = resolvedXamlPath;
    if (!ComposeXamlForNoesis(assetRoot_, resolvedXamlPath, loadXamlPath)) {
        HudDiagLog("AeroHudHost: failed to compose '%s'", resolvedXamlPath.c_str());
        return false;
    }
    if (loadXamlPath == resolvedXamlPath) {
        HudDiagLog("AeroHudHost: loading '%s' without composition", resolvedXamlPath.c_str());
    } else {
        HudDiagLog("AeroHudHost: composed '%s' -> '%s'", resolvedXamlPath.c_str(), loadXamlPath.c_str());
    }

    root_ = Aero::GUI::LoadXaml<Aero::UserControl>(loadXamlPath.c_str());
    if (!root_) {
        HudDiagLog("AeroHudHost: failed to load '%s'", loadXamlPath.c_str());
        return false;
    }
    HudDiagLog("AeroHudHost: loaded '%s'", loadXamlPath.c_str());
    if (Aero::BaseComponent* mdxBackground = root_->FindName("GlueMdxBackground")) {
        glueMdxBackground_ = Aero::DynamicCast<War3UI::MdxSceneControl*>(mdxBackground);
        HudDiagLog("AeroHudHost: GlueMdxBackground found type='%s' mdx=%s",
                   mdxBackground->GetClassType()->GetName(),
                   glueMdxBackground_ ? "yes" : "no");
    } else {
        glueMdxBackground_ = nullptr;
        HudDiagLog("AeroHudHost: GlueMdxBackground not found");
    }

    view_ = Aero::GUI::CreateView(root_);
    if (!view_) {
        HudDiagLog("AeroHudHost: failed to create view");
        return false;
    }

    view_->SetFlags(Aero::RenderFlags_PPAA | Aero::RenderFlags_LCD);
    view_->SetScale(dpiScale_);
    view_->SetSize((uint32_t)physicalWidth_, (uint32_t)physicalHeight_);

    device_ = AeroApp::GLFactory::CreateDevice(false);
    if (!device_) {
        HudDiagLog("AeroHudHost: failed to create GL render device");
        return false;
    }

    view_->GetRenderer()->Init(device_.GetPtr());
    CacheElements();
    LogMissingElements();
    initialized_ = true;
    ApplyState();
    view_->Update(0.0);
    CacheRects();
    HudDiagLog(
        "AeroHudHost: ready root=%.1fx%.1f view=%dx%d",
        (float)root_->GetActualWidth(), (float)root_->GetActualHeight(), physicalWidth_, physicalHeight_);
    return true;
}

bool NoesisHudHost::ReloadXaml(const std::string& xamlPath, MpqManager* mpq, int gameVersion) {
    if (!guiInitialized_) {
        return Initialize(assetRoot_, physicalWidth_, physicalHeight_, xamlPath, mpq, gameVersion);
    }

    std::string resolvedXamlPath = xamlPath.empty() ? "assets/ui/Hud.xaml" : xamlPath;
    std::replace(resolvedXamlPath.begin(), resolvedXamlPath.end(), '\\', '/');
    if (resolvedXamlPath == "assets/noesis/Hud.xaml" || resolvedXamlPath == "./assets/noesis/Hud.xaml") {
        resolvedXamlPath = "assets/ui/Hud.xaml";
    }

    HudDiagLog("AeroHudHost: reloading xaml='%s'", resolvedXamlPath.c_str());

    if (initialized_ && view_) {
        view_->GetRenderer()->Shutdown();
    }
    initialized_ = false;
    glueMenuControl_ = nullptr;
    glueMdxBackground_ = nullptr;
    hasLastUpdateTime_ = false;
    lastUpdateTime_ = 0.0;
    device_.Reset();
    root_.Reset();
    view_.Reset();

    War3UI::MdxSceneControl::SetSharedContext(mpq, gameVersion);

    std::string loadXamlPath = resolvedXamlPath;
    if (!ComposeXamlForNoesis(assetRoot_, resolvedXamlPath, loadXamlPath)) {
        HudDiagLog("AeroHudHost: failed to compose reload '%s'", resolvedXamlPath.c_str());
        return false;
    }
    if (loadXamlPath == resolvedXamlPath) {
        HudDiagLog("AeroHudHost: reloading '%s' without composition", resolvedXamlPath.c_str());
    } else {
        HudDiagLog("AeroHudHost: recomposed '%s' -> '%s'", resolvedXamlPath.c_str(), loadXamlPath.c_str());
    }

    root_ = Aero::GUI::LoadXaml<Aero::UserControl>(loadXamlPath.c_str());
    if (!root_) {
        HudDiagLog("AeroHudHost: failed to reload '%s'", loadXamlPath.c_str());
        return false;
    }
    HudDiagLog("AeroHudHost: reloaded '%s'", loadXamlPath.c_str());
    if (Aero::BaseComponent* mdxBackground = root_->FindName("GlueMdxBackground")) {
        glueMdxBackground_ = Aero::DynamicCast<War3UI::MdxSceneControl*>(mdxBackground);
        HudDiagLog("AeroHudHost: GlueMdxBackground found after reload type='%s' mdx=%s",
                   mdxBackground->GetClassType()->GetName(),
                   glueMdxBackground_ ? "yes" : "no");
    } else {
        glueMdxBackground_ = nullptr;
        HudDiagLog("AeroHudHost: GlueMdxBackground not found after reload");
    }

    view_ = Aero::GUI::CreateView(root_);
    if (!view_) {
        HudDiagLog("AeroHudHost: failed to create reload view");
        return false;
    }
    view_->SetFlags(Aero::RenderFlags_PPAA | Aero::RenderFlags_LCD);
    view_->SetScale(dpiScale_);
    view_->SetSize((uint32_t)physicalWidth_, (uint32_t)physicalHeight_);

    device_ = AeroApp::GLFactory::CreateDevice(false);
    if (!device_) {
        HudDiagLog("AeroHudHost: failed to create reload GL render device");
        return false;
    }

    view_->GetRenderer()->Init(device_.GetPtr());
    CacheElements();
    LogMissingElements();
    initialized_ = true;
    ApplyState();
    view_->Update(0.0);
    CacheRects();
    HudDiagLog("AeroHudHost: reload ready root=%.1fx%.1f view=%dx%d",
               (float)root_->GetActualWidth(), (float)root_->GetActualHeight(), physicalWidth_, physicalHeight_);
    return true;
}

void NoesisHudHost::Shutdown() {
    if (initialized_ && view_) {
        view_->GetRenderer()->Shutdown();
    }
    initialized_ = false;
    glueMenuControl_ = nullptr;
    glueMdxBackground_ = nullptr;
    hasLastUpdateTime_ = false;
    lastUpdateTime_ = 0.0;
    device_.Reset();
    root_.Reset();
    view_.Reset();

    if (guiInitialized_) {
        War3UI::MdxSceneControl::SetSharedContext(nullptr, 0);
        Aero::GUI::Shutdown();
        guiInitialized_ = false;
    }
}

void NoesisHudHost::Resize(int width, int height) {
    if (width <= 0 || height <= 0 || !view_) return;

    float scale = sapp_dpi_scale();
    const int logicalWidth = ToLogicalPixels(width, scale);
    const int logicalHeight = ToLogicalPixels(height, scale);
    const int physicalWidth = std::max(1, width);
    const int physicalHeight = std::max(1, height);
    if (physicalWidth_ == physicalWidth && physicalHeight_ == physicalHeight &&
        width_ == logicalWidth && height_ == logicalHeight && dpiScale_ == scale) {
        return;
    }

    physicalWidth_ = physicalWidth;
    physicalHeight_ = physicalHeight;
    width_ = logicalWidth;
    height_ = logicalHeight;
    dpiScale_ = scale;
    view_->SetScale(dpiScale_);
    view_->SetSize((uint32_t)physicalWidth_, (uint32_t)physicalHeight_);
    HudDiagLog("AeroHudHost: SetSize view=%dx%d logical=%dx%d scale=%.2f",
               physicalWidth_, physicalHeight_, width_, height_, dpiScale_);
}

void NoesisHudHost::Update(double time) {
    if (!initialized_ || !view_) return;
    double dt = hasLastUpdateTime_ ? (time - lastUpdateTime_) : (1.0 / 60.0);
    if (!std::isfinite(dt) || dt < 0.0 || dt > 0.25) {
        dt = 1.0 / 60.0;
    }
    lastUpdateTime_ = time;
    hasLastUpdateTime_ = true;

    if (chatInput_ && chatOpen_) {
        chatInputText_ = chatInput_->GetText();
    }
    ApplyState();
    view_->Update(time);
    if (glueMdxBackground_) {
        glueMdxBackground_->RenderFrame((float)dt);
    }
    CacheRects();
}

void NoesisHudHost::RenderOffscreen() {
    if (!initialized_ || !view_) return;

    sg_reset_state_cache();
    auto* renderer = view_->GetRenderer();
    renderTreeReady_ = renderer->UpdateRenderTree();
    renderer->RenderOffscreen();
    sg_reset_state_cache();
}

void NoesisHudHost::Render() {
    if (!initialized_ || !view_) return;

    sg_reset_state_cache();
    static uint64_t renderProbeFrame = 0;
    const bool logProbe = (renderProbeFrame++ % 120) == 0;
    auto* renderer = view_->GetRenderer();

    renderTreeReady_ = renderer->UpdateRenderTree();
    renderer->RenderOffscreen();
    RestoreDefaultFramebufferForNoesis(physicalWidth_, physicalHeight_);

    if (logProbe) {
        HudDiagLog("AeroHudHost render probe: currentFb=%d swapFb=%u physical=%dx%d depthBits=%d stencilBits=%d preError=0x%x",
                   CurrentFramebufferBinding(), sglue_swapchain().gl.framebuffer,
                   physicalWidth_, physicalHeight_,
                   GLInteger(GL_DEPTH_BITS_EXT), GLInteger(GL_STENCIL_BITS_EXT),
                   DrainGLError());
    }
    renderer->Render();
    if (logProbe) {
        HudDiagLog("AeroHudHost render probe: postError=0x%x", DrainGLError());
    }
    renderTreeReady_ = false;
    sg_reset_state_cache();
}

bool NoesisHudHost::HandleEvent(const sapp_event* e) {
    if (!initialized_ || !view_ || !e) return false;

    float x = e->mouse_x;
    float y = e->mouse_y;

    switch (e->type) {
    case SAPP_EVENTTYPE_MOUSE_DOWN: {
        Aero::MouseButton btn = Aero::MouseButton_Left;
        if (e->mouse_button == SAPP_MOUSEBUTTON_RIGHT) btn = Aero::MouseButton_Right;
        else if (e->mouse_button == SAPP_MOUSEBUTTON_MIDDLE) btn = Aero::MouseButton_Middle;
        return view_->MouseButtonDown(x, y, btn);
    }
    case SAPP_EVENTTYPE_MOUSE_UP: {
        Aero::MouseButton btn = Aero::MouseButton_Left;
        if (e->mouse_button == SAPP_MOUSEBUTTON_RIGHT) btn = Aero::MouseButton_Right;
        else if (e->mouse_button == SAPP_MOUSEBUTTON_MIDDLE) btn = Aero::MouseButton_Middle;
        return view_->MouseButtonUp(x, y, btn);
    }
    case SAPP_EVENTTYPE_MOUSE_MOVE:
        return view_->MouseMove(x, y);
    case SAPP_EVENTTYPE_MOUSE_SCROLL:
        return view_->MouseWheel(x, y, e->scroll_y * 3.5f);
    case SAPP_EVENTTYPE_KEY_DOWN:
        return view_->KeyDown(MapKey(e->key_code));
    case SAPP_EVENTTYPE_KEY_UP:
        return view_->KeyUp(MapKey(e->key_code));
    case SAPP_EVENTTYPE_CHAR:
        return view_->Char(e->char_code);
    case SAPP_EVENTTYPE_RESIZED:
        Resize(e->window_width, e->window_height);
        return true;
    default:
        return false;
    }
}

void NoesisHudHost::SetResources(int gold, int lumber, int foodUsed, int foodCap, const std::string& timeText) {
    const bool changed = gold_ != gold || lumber_ != lumber || foodUsed_ != foodUsed ||
        foodCap_ != foodCap || timeValue_ != timeText;
    gold_ = gold;
    lumber_ = lumber;
    foodUsed_ = foodUsed;
    foodCap_ = foodCap;
    upkeepValue_ = "No Upkeep";
    timeValue_ = timeText;
    if (changed) {
        HudDiagLog("AeroHudHost: resources gold=%d lumber=%d food=%d/%d time=%s",
                   gold_, lumber_, foodUsed_, foodCap_, timeValue_.c_str());
    }
}

void NoesisHudHost::SetSelection(const std::string& name, int level, float hp, float maxHp, float mp, float maxMp,
                                 bool heroVisible, int xp, int maxXp,
                                 int strBase, int strBonus, int agiBase, int agiBonus, int intBase, int intBonus,
                                 const HudInfoStats& infoStats) {
    unitName_ = name;
    unitLevel_ = level;
    hp_ = hp;
    maxHp_ = maxHp;
    mp_ = mp;
    maxMp_ = maxMp;
    heroVisible_ = heroVisible;
    heroXp_ = xp;
    heroMaxXp_ = maxXp;
    heroStrBase_ = strBase;
    heroStrBonus_ = strBonus;
    heroAgiBase_ = agiBase;
    heroAgiBonus_ = agiBonus;
    heroIntBase_ = intBase;
    heroIntBonus_ = intBonus;
    infoStats_ = infoStats;
}

void NoesisHudHost::SetTooltip(const std::string& text, float screenX, float screenY, bool visible,
                               const HudResourceCosts& costs) {
    tooltipValue_ = text;
    tooltipCosts_ = costs;
    tooltipX_ = screenX;
    tooltipY_ = screenY;
    tooltipVisible_ = visible && !text.empty();
}

void NoesisHudHost::SetChatOpen(bool open) {
    chatOpen_ = open;
}

void NoesisHudHost::SetChatInput(const std::string& text) {
    chatInputText_ = text;
    if (chatInput_) {
        chatInput_->SetText(chatInputText_.c_str());
        if (chatOpen_) chatInput_->Focus();
    }
}

std::string NoesisHudHost::GetChatInput() const {
    return chatInput_ ? std::string(chatInput_->GetText()) : chatInputText_;
}

void NoesisHudHost::SetChatMessages(const std::vector<std::string>& messages) {
    chatJoinedText_ = JoinLines(messages);
}

void NoesisHudHost::SetCommandSlot(int index, const HudCommandSlot& slot) {
    if (index < 0 || index >= (int)commandStates_.size()) return;
    commandStates_[index].label = slot.label;
    commandStates_[index].tooltip = slot.tooltip;
    commandStates_[index].iconUri = slot.iconUri;
    commandStates_[index].resourceCosts = slot.resourceCosts;
    commandStates_[index].cooldownRemaining = slot.cooldownRemaining;
    commandStates_[index].cooldownMax = slot.cooldownMax;
    commandStates_[index].autocastOrderId = slot.autocastOrderId;
    commandStates_[index].numberOverlay = slot.numberOverlay;
    commandStates_[index].enabled = slot.enabled;
    commandStates_[index].visible = slot.visible;
    commandStates_[index].active = slot.active;
    commandStates_[index].autocastActive = slot.autocastActive;
}

void NoesisHudHost::SetHoveredCommand(int index) {
    hoveredCommandIndex_ = (index >= 0 && index < (int)commandStates_.size()) ? index : -1;
}

void NoesisHudHost::SetPressedCommand(int index) {
    pressedCommandIndex_ = (index >= 0 && index < (int)commandStates_.size()) ? index : -1;
}

void NoesisHudHost::SetHoveredUpperButton(int index) {
    hoveredUpperButtonIndex_ = (index >= 0 && index < (int)upperButtons_.size() && IsUpperButtonEnabled(index))
        ? index
        : -1;
}

void NoesisHudHost::SetPressedUpperButton(int index) {
    pressedUpperButtonIndex_ = (index >= 0 && index < (int)upperButtons_.size() && IsUpperButtonEnabled(index))
        ? index
        : -1;
}

void NoesisHudHost::SetPressedMinimapButton(int index) {
    pressedMinimapButtonIndex_ = (index >= 0 && index < (int)minimapButtons_.size()) ? index : -1;
}

void NoesisHudHost::ActivateMinimapButton(int index) {
    if (index < 0 || index >= (int)minimapButtonModes_.size()) {
        return;
    }

    switch (index) {
    case 1: // terrain active/inactive
    case 3: // creep active/inactive
    case 4: // formation on/off
        minimapButtonModes_[index] = minimapButtonModes_[index] == 0 ? 1 : 0;
        break;
    case 2: // ally active/inactive/off
        minimapButtonModes_[index] = (minimapButtonModes_[index] + 1) % 3;
        break;
    default:
        break;
    }
}

bool NoesisHudHost::IsGameMenuActionEnabled(int action) const {
    switch (action) {
    case kGameMenuPause:
    case kGameMenuSaveGame:
    case kGameMenuLoadGame:
    case kGameMenuOptions:
    case kGameMenuHelp:
    case kGameMenuTips:
    case kGameMenuEndGame:
    case kGameMenuReturnToGame:
    case kGameMenuQuitMission:
    case kGameMenuExitProgram:
    case kGameMenuPrevious:
    case kGameMenuConfirmExit:
    case kGameMenuCancelConfirm:
    case kGameMenuGameplay:
    case kGameMenuVideo:
    case kGameMenuSound:
    case kGameMenuNetwork:
    case kGameMenuOptionsBack:
    case kGameMenuOptionsOK:
    case kGameMenuOptionsCancel:
    case kGameMenuSaveGameSave:
    case kGameMenuSaveGameDelete:
    case kGameMenuSaveGameCancel:
    case kGameMenuLoadGameLoad:
    case kGameMenuLoadGameCancel:
    case kGameMenuOverwriteConfirm:
    case kGameMenuOverwriteCancel:
    case kGameMenuDeleteConfirm:
    case kGameMenuDeleteCancel:
    case kGameMenuHelpOK:
    case kGameMenuTipsBack:
    case kGameMenuTipsNext:
    case kGameMenuTipsOK:
    case kGameMenuOptionGameSpeedSlider:
    case kGameMenuOptionMouseScrollSlider:
    case kGameMenuOptionMouseScrollDisable:
    case kGameMenuOptionKeyScrollSlider:
    case kGameMenuOptionEnhancedTooltips:
    case kGameMenuOptionSubgroupModifier:
    case kGameMenuOptionFormationToggle:
    case kGameMenuOptionHealthBars:
    case kGameMenuOptionGammaSlider:
    case kGameMenuOptionParticles:
    case kGameMenuOptionLights:
    case kGameMenuOptionShadows:
    case kGameMenuOptionOcclusion:
    case kGameMenuOptionSoundVolume:
    case kGameMenuOptionMusicVolume:
    case kGameMenuOptionAmbientSounds:
    case kGameMenuOptionMovementSounds:
    case kGameMenuOptionUnitSounds:
    case kGameMenuOptionSubtitles:
    case kGameMenuOptionEnvironmentalEffects:
    case kGameMenuOptionPositionalAudio:
    case kGameMenuOptionLowLatency:
    case kGameMenuOptionHighLatency:
    case kGameMenuOptionExtraHighLatency:
        return true;
    case kGameMenuOptionSoundVolumeSlider:
        return gameMenuOptionCheckedState_[kGameMenuOptionSoundVolume];
    case kGameMenuOptionMusicVolumeSlider:
        return gameMenuOptionCheckedState_[kGameMenuOptionMusicVolume];
    default:
        return GameMenuPopupParentAction(action) == gameMenuOptionPopupOpenAction_;
    }
}

void NoesisHudHost::SetGameMenuPanel(GameMenuPanel panel) {
    gameMenuPanel_ = panel;
    hoveredGameMenuAction_ = -1;
    pressedGameMenuAction_ = -1;
    gameMenuOptionPopupOpenAction_ = -1;
}

void NoesisHudHost::SetMultiSelection(const std::vector<MultiSelectUnitInfo>& units, int primaryHandle) {
    multiSelectPrimaryHandle_ = primaryHandle;
    multiSelectVisible_.fill(false);
    for (size_t i = 0; i < units.size() && i < multiSelectStates_.size(); ++i) {
        multiSelectStates_[i] = units[i];
        multiSelectVisible_[i] = true;
    }
}

void NoesisHudHost::SetQueue(const std::vector<HudQueueItem>& items, float progressRatio) {
    for (auto& state : queueStates_) {
        state = HudQueueItem{};
    }
    for (size_t i = 0; i < items.size() && i < queueStates_.size(); ++i) {
        queueStates_[i] = items[i];
    }
    queueProgressRatio_ = std::clamp(progressRatio, 0.0f, 1.0f);
}

void NoesisHudHost::SetInventory(const std::vector<HudInventoryItem>& items, int capacity, bool available) {
    inventoryAvailable_ = available;
    inventoryCapacity_ = std::clamp(capacity, 0, (int)inventoryStates_.size());
    for (auto& state : inventoryStates_) {
        state = HudInventoryItem{};
    }
    for (const HudInventoryItem& item : items) {
        if (item.slot >= 0 && item.slot < (int)inventoryStates_.size()) {
            inventoryStates_[item.slot] = item;
        }
    }
}

void NoesisHudHost::SetPortraitIcon(const std::string& iconUri) {
    portraitIconUri_ = iconUri;
}

void NoesisHudHost::SetBuffs(const std::vector<HudBuffItem>& buffs) {
    buffCount_ = std::min((int)buffStates_.size(), (int)buffs.size());
    for (int i = 0; i < (int)buffStates_.size(); ++i) {
        buffStates_[i] = (i < buffCount_) ? buffs[i] : HudBuffItem{};
    }
}

void NoesisHudHost::OpenGameMenu() {
    gameMenuOpen_ = true;
    SetGameMenuPanel(GameMenuPanel::Main);
}

void NoesisHudHost::OpenGameMenuDebugPanel(const std::string& panel) {
    gameMenuOpen_ = true;
    if (panel == "endgame") {
        SetGameMenuPanel(GameMenuPanel::EndGame);
    } else if (panel == "confirm") {
        SetGameMenuPanel(GameMenuPanel::ConfirmQuit);
    } else if (panel == "save") {
        SetGameMenuPanel(GameMenuPanel::SaveGame);
    } else if (panel == "load") {
        SetGameMenuPanel(GameMenuPanel::LoadGame);
    } else if (panel == "overwrite") {
        SetGameMenuPanel(GameMenuPanel::SaveOverwriteConfirm);
    } else if (panel == "delete") {
        SetGameMenuPanel(GameMenuPanel::SaveDeleteConfirm);
    } else if (panel == "help") {
        SetGameMenuPanel(GameMenuPanel::Help);
    } else if (panel == "tips") {
        SetGameMenuPanel(GameMenuPanel::Tips);
    } else if (panel == "options") {
        SetGameMenuPanel(GameMenuPanel::Options);
    } else if (panel == "gameplay") {
        SetGameMenuPanel(GameMenuPanel::OptionsGameplay);
    } else if (panel == "video") {
        SetGameMenuPanel(GameMenuPanel::OptionsVideo);
    } else if (panel == "sound") {
        SetGameMenuPanel(GameMenuPanel::OptionsSound);
    } else if (panel == "network") {
        SetGameMenuPanel(GameMenuPanel::OptionsNetwork);
    } else {
        SetGameMenuPanel(GameMenuPanel::Main);
    }
}

void NoesisHudHost::CloseGameMenu() {
    gameMenuOpen_ = false;
    hoveredGameMenuAction_ = -1;
    pressedGameMenuAction_ = -1;
    gameMenuOptionPopupOpenAction_ = -1;
}

void NoesisHudHost::ToggleGameMenu() {
    if (gameMenuOpen_) {
        CloseGameMenu();
    } else {
        OpenGameMenu();
    }
}

int NoesisHudHost::HitGameMenuButton(float x, float y) const {
    if (!gameMenuOpen_) return -1;
    for (int i = 0; i < (int)gameMenuButtonRects_.size(); ++i) {
        if (IsGameMenuActionEnabled(i) && gameMenuButtonRects_[i].Contains(x, y)) {
            return i;
        }
    }
    return -1;
}

void NoesisHudHost::SetHoveredGameMenuButton(int action) {
    hoveredGameMenuAction_ = (action >= 0 && action < kGameMenuActionCount && IsGameMenuActionEnabled(action))
        ? action
        : -1;
}

void NoesisHudHost::SetPressedGameMenuButton(int action) {
    pressedGameMenuAction_ = (action >= 0 && action < kGameMenuActionCount && IsGameMenuActionEnabled(action))
        ? action
        : -1;
}

bool NoesisHudHost::InvokeGameMenuActionForDebug(const std::string& actionName) {
    int action = -1;
    auto findActionByName = [](const std::string& name) {
        for (int i = 0; i < kGameMenuActionCount; ++i) {
            if (name == GameMenuActionElementName(i)) {
                return i;
            }
        }
        return -1;
    };

    action = findActionByName(actionName);
    if (action < 0 && actionName.rfind("GameMenu", 0) == 0 &&
        actionName.rfind("GameMenuButton", 0) != 0) {
        action = findActionByName("GameMenuButton" + actionName.substr(8));
    }
    if (action < 0) {
        HudDiagLog("AeroHudHost: debug game menu action '%s' not found", actionName.c_str());
        return false;
    }
    if (!IsGameMenuActionEnabled(action)) {
        HudDiagLog("AeroHudHost: debug game menu action '%s' is disabled", actionName.c_str());
        return false;
    }

    SetPressedGameMenuButton(action);
    ApplyState();
    HandleGameMenuAction(action);
    SetPressedGameMenuButton(-1);
    ApplyState();

    const int parent = GameMenuPopupParentAction(action);
    if (parent >= 0) {
        HudDiagLog("AeroHudHost: debug game menu action '%s' selected %s=%s",
                   actionName.c_str(),
                   GameMenuActionElementName(parent),
                   GameMenuOptionCycleText(parent, gameMenuOptionCycleIndex_[parent]));
    } else if (IsGameMenuOptionPopupAction(action)) {
        HudDiagLog("AeroHudHost: debug game menu action '%s' popup %s",
                   actionName.c_str(),
                   gameMenuOptionPopupOpenAction_ == action ? "opened" : "closed");
    } else {
        HudDiagLog("AeroHudHost: debug game menu action '%s' invoked", actionName.c_str());
    }
    return true;
}

bool NoesisHudHost::InvokeUpperButtonForDebug(const std::string& buttonName) {
    const int index = UpperButtonIndexForDebugName(buttonName);
    if (index < 0) {
        HudDiagLog("AeroHudHost: debug upper button '%s' not found", buttonName.c_str());
        return false;
    }
    if (!IsUpperButtonEnabled(index)) {
        HudDiagLog("AeroHudHost: debug upper button '%s' is disabled", UpperButtonDebugName(index));
        return false;
    }

    SetHoveredUpperButton(index);
    SetPressedUpperButton(index);
    ApplyState();

    if (index == 1) {
        OpenGameMenu();
    }

    SetPressedUpperButton(-1);
    ApplyState();

    HudDiagLog("AeroHudHost: debug upper button '%s' invoked%s",
               UpperButtonDebugName(index),
               gameMenuOpen_ ? ", game menu open" : "");
    return true;
}

int NoesisHudHost::HitGlueMenuButton(float x, float y) const {
    return glueMenuControl_ ? glueMenuControl_->HitButton(x, y) : -1;
}

void NoesisHudHost::SetHoveredGlueMenuButton(int action) {
    if (glueMenuControl_) {
        glueMenuControl_->SetHoveredButton(action);
    }
}

void NoesisHudHost::SetPressedGlueMenuButton(int action) {
    if (glueMenuControl_) {
        glueMenuControl_->SetPressedButton(action);
    }
}

void NoesisHudHost::HandleGlueMenuAction(int action) {
    if (glueMenuControl_) {
        glueMenuControl_->HandleAction(action);
    } else {
        HudDiagLog("AeroHudHost: glue menu action %d ignored; GlueMenuControl is not loaded", action);
    }
}

void NoesisHudHost::OpenGlueMenuDebugPage(const std::string& pageName) {
    if (glueMenuControl_) {
        glueMenuControl_->OpenDebugPage(pageName);
    } else {
        HudDiagLog("AeroHudHost: debug glue menu page '%s' ignored; GlueMenuControl is not loaded",
                   pageName.c_str());
    }
}

bool NoesisHudHost::InvokeGlueMenuActionForDebug(const std::string& actionName) {
    if (glueMenuControl_) {
        return glueMenuControl_->InvokeActionForDebug(actionName);
    }

    HudDiagLog("AeroHudHost: debug glue menu action '%s' ignored; GlueMenuControl is not loaded",
               actionName.c_str());
    return false;
}

void NoesisHudHost::SetGlueMenuMaps(const std::vector<War3UI::GlueMapListItem>& maps) {
    if (glueMenuControl_) {
        glueMenuControl_->SetMaps(maps);
    }
}

void NoesisHudHost::SetGlueMenuCampaigns(const std::vector<War3UI::GlueCampaignListItem>& campaigns) {
    if (glueMenuControl_) {
        glueMenuControl_->SetCampaigns(campaigns);
    }
}

void NoesisHudHost::SetGlueMapLaunchCallback(War3UI::GlueMapLaunchCallback cb) {
    glueMapLaunchCallback_ = std::move(cb);
    if (glueMenuControl_) {
        glueMenuControl_->SetMapLaunchCallback(glueMapLaunchCallback_);
    }
}

void NoesisHudHost::HandleGameMenuAction(int action) {
    switch (action) {
    case kGameMenuPause:
        gameMenuPaused_ = !gameMenuPaused_;
        HudDiagLog("AeroHudHost: game menu Pause toggled %s (UI wired, simulation pause pending)",
                   gameMenuPaused_ ? "on" : "off");
        break;
    case kGameMenuSaveGame:
        SetGameMenuPanel(GameMenuPanel::SaveGame);
        break;
    case kGameMenuLoadGame:
        SetGameMenuPanel(GameMenuPanel::LoadGame);
        break;
    case kGameMenuOptions:
        SetGameMenuPanel(GameMenuPanel::Options);
        break;
    case kGameMenuHelp:
        SetGameMenuPanel(GameMenuPanel::Help);
        break;
    case kGameMenuTips:
        SetGameMenuPanel(GameMenuPanel::Tips);
        break;
    case kGameMenuGameplay:
        SetGameMenuPanel(GameMenuPanel::OptionsGameplay);
        break;
    case kGameMenuVideo:
        SetGameMenuPanel(GameMenuPanel::OptionsVideo);
        break;
    case kGameMenuSound:
        SetGameMenuPanel(GameMenuPanel::OptionsSound);
        break;
    case kGameMenuNetwork:
        SetGameMenuPanel(GameMenuPanel::OptionsNetwork);
        break;
    case kGameMenuEndGame:
        SetGameMenuPanel(GameMenuPanel::EndGame);
        break;
    case kGameMenuReturnToGame:
        CloseGameMenu();
        break;
    case kGameMenuPrevious:
    case kGameMenuOptionsBack:
        SetGameMenuPanel(GameMenuPanel::Main);
        break;
    case kGameMenuOptionsOK:
    case kGameMenuOptionsCancel:
        SetGameMenuPanel(GameMenuPanel::Options);
        break;
    case kGameMenuSaveGameSave:
        SetGameMenuPanel(GameMenuPanel::SaveOverwriteConfirm);
        break;
    case kGameMenuSaveGameDelete:
        SetGameMenuPanel(GameMenuPanel::SaveDeleteConfirm);
        break;
    case kGameMenuSaveGameCancel:
    case kGameMenuLoadGameCancel:
        SetGameMenuPanel(GameMenuPanel::Main);
        break;
    case kGameMenuLoadGameLoad:
        HudDiagLog("AeroHudHost: game menu Load requested (UI wired, savegame loading pending)");
        SetGameMenuPanel(GameMenuPanel::Main);
        break;
    case kGameMenuOverwriteConfirm:
        HudDiagLog("AeroHudHost: game menu Save overwrite requested (UI wired, savegame writing pending)");
        SetGameMenuPanel(GameMenuPanel::SaveGame);
        break;
    case kGameMenuOverwriteCancel:
    case kGameMenuDeleteConfirm:
    case kGameMenuDeleteCancel:
        if (action == kGameMenuDeleteConfirm) {
            HudDiagLog("AeroHudHost: game menu Delete save requested (UI wired, savegame deletion pending)");
        }
        SetGameMenuPanel(GameMenuPanel::SaveGame);
        break;
    case kGameMenuHelpOK:
    case kGameMenuTipsOK:
        SetGameMenuPanel(GameMenuPanel::Main);
        break;
    case kGameMenuTipsBack:
    case kGameMenuTipsNext:
        HudDiagLog("AeroHudHost: game menu Tips %s requested (UI wired, tip paging pending)",
                   action == kGameMenuTipsBack ? "Back" : "Next");
        SetGameMenuPanel(GameMenuPanel::Tips);
        break;
    case kGameMenuExitProgram:
        SetGameMenuPanel(GameMenuPanel::ConfirmQuit);
        break;
    case kGameMenuCancelConfirm:
        SetGameMenuPanel(GameMenuPanel::EndGame);
        break;
    case kGameMenuQuitMission:
        HudDiagLog("AeroHudHost: game menu Quit Mission requested (UI wired, gameplay action pending)");
        CloseGameMenu();
        break;
    case kGameMenuConfirmExit:
        HudDiagLog("AeroHudHost: game menu Exit Program confirmed (UI wired, application exit suppressed)");
        CloseGameMenu();
        break;
    case kGameMenuOptionMouseScrollDisable:
    case kGameMenuOptionEnhancedTooltips:
    case kGameMenuOptionSubgroupModifier:
    case kGameMenuOptionFormationToggle:
    case kGameMenuOptionHealthBars:
    case kGameMenuOptionSoundVolume:
    case kGameMenuOptionMusicVolume:
    case kGameMenuOptionAmbientSounds:
    case kGameMenuOptionMovementSounds:
    case kGameMenuOptionUnitSounds:
    case kGameMenuOptionSubtitles:
    case kGameMenuOptionEnvironmentalEffects:
    case kGameMenuOptionPositionalAudio:
        gameMenuOptionCheckedState_[action] = !gameMenuOptionCheckedState_[action];
        gameMenuOptionPopupOpenAction_ = -1;
        break;
    case kGameMenuOptionLowLatency:
    case kGameMenuOptionHighLatency:
    case kGameMenuOptionExtraHighLatency:
        gameMenuOptionCheckedState_[kGameMenuOptionLowLatency] = action == kGameMenuOptionLowLatency;
        gameMenuOptionCheckedState_[kGameMenuOptionHighLatency] = action == kGameMenuOptionHighLatency;
        gameMenuOptionCheckedState_[kGameMenuOptionExtraHighLatency] = action == kGameMenuOptionExtraHighLatency;
        gameMenuOptionPopupOpenAction_ = -1;
        break;
    case kGameMenuOptionParticles:
    case kGameMenuOptionLights:
    case kGameMenuOptionShadows:
    case kGameMenuOptionOcclusion:
        gameMenuOptionPopupOpenAction_ = gameMenuOptionPopupOpenAction_ == action ? -1 : action;
        break;
    case kGameMenuOptionParticlesLow:
    case kGameMenuOptionParticlesMedium:
    case kGameMenuOptionParticlesHigh:
    case kGameMenuOptionLightsLow:
    case kGameMenuOptionLightsMedium:
    case kGameMenuOptionLightsHigh:
    case kGameMenuOptionShadowsOff:
    case kGameMenuOptionShadowsOn:
    case kGameMenuOptionOcclusionOff:
    case kGameMenuOptionOcclusionOn: {
        const int parent = GameMenuPopupParentAction(action);
        if (parent >= 0) {
            gameMenuOptionCycleIndex_[parent] = GameMenuPopupItemValue(action);
        }
        gameMenuOptionPopupOpenAction_ = -1;
        break;
    }
    case kGameMenuOptionGameSpeedSlider:
    case kGameMenuOptionMouseScrollSlider:
    case kGameMenuOptionKeyScrollSlider:
    case kGameMenuOptionGammaSlider:
    case kGameMenuOptionSoundVolumeSlider:
    case kGameMenuOptionMusicVolumeSlider:
        HudDiagLog("AeroHudHost: game menu option slider action %d clicked (drag/value binding pending)", action);
        gameMenuOptionPopupOpenAction_ = -1;
        break;
    default:
        if (action >= 0) {
            HudDiagLog("AeroHudHost: game menu action %d is present but not enabled yet", action);
        }
        break;
    }
}

int NoesisHudHost::HitCommandButton(float x, float y) const {
    for (int i = 0; i < (int)commandRects_.size(); ++i) {
        if (commandStates_[i].visible && commandStates_[i].enabled && commandRects_[i].Contains(x, y)) {
            return i;
        }
    }
    return -1;
}

int NoesisHudHost::HitUpperButton(float x, float y) const {
    for (int i = 0; i < (int)upperButtonRects_.size(); ++i) {
        if (IsUpperButtonEnabled(i) && upperButtonRects_[i].Contains(x, y)) {
            return i;
        }
    }
    return -1;
}

int NoesisHudHost::HitMinimapButton(float x, float y) const {
    for (int i = 0; i < (int)minimapButtonRects_.size(); ++i) {
        if (minimapButtonRects_[i].Contains(x, y)) {
            return i;
        }
    }
    return -1;
}

int NoesisHudHost::HitMultiSelection(float x, float y) const {
    for (int i = 0; i < (int)multiSelectRects_.size(); ++i) {
        if (multiSelectVisible_[i] && multiSelectRects_[i].Contains(x, y)) {
            return multiSelectStates_[i].handleId;
        }
    }
    return -1;
}

int NoesisHudHost::HitQueueSlot(float x, float y) const {
    for (int i = 0; i < (int)queueRects_.size(); ++i) {
        if (queueStates_[i].visible && queueRects_[i].Contains(x, y)) {
            return queueStates_[i].slot;
        }
    }
    return -1;
}

int NoesisHudHost::HitInventorySlot(float x, float y) const {
    for (int i = 0; i < (int)inventoryRects_.size(); ++i) {
        if (inventoryStates_[i].visible && inventoryStates_[i].enabled && inventoryStates_[i].occupied &&
            inventoryRects_[i].Contains(x, y)) {
            return inventoryStates_[i].slot;
        }
    }
    return -1;
}

ScreenRect NoesisHudHost::GetCommandRect(int index) const {
    if (index < 0 || index >= (int)commandRects_.size()) return {};
    return commandRects_[index];
}

void NoesisHudHost::CacheElements() {
    auto* rootElement = root_.GetPtr();
    glueMenuControl_ = Aero::DynamicCast<War3UI::GlueMenuControl*>(rootElement);
    if (!glueMenuControl_) {
        glueMenuControl_ = FindAs<War3UI::GlueMenuControl>(rootElement, "GlueMenuControl");
    }
    if (glueMenuControl_) {
        glueMenuControl_->CacheElements();
        glueMenuControl_->SetMapLaunchCallback(glueMapLaunchCallback_);
    }
    goldText_ = FindFirstAs<Aero::TextBlock>(rootElement, "ResourceBarGoldText", "GoldText");
    lumberText_ = FindFirstAs<Aero::TextBlock>(rootElement, "ResourceBarLumberText", "LumberText");
    foodText_ = FindFirstAs<Aero::TextBlock>(rootElement, "ResourceBarSupplyText", "FoodText");
    upkeepText_ = FindFirstAs<Aero::TextBlock>(rootElement, "ResourceBarUpkeepText", "UpkeepText");
    timeText_ = FindAs<Aero::TextBlock>(rootElement, "TimeText");
    timeOfDayIndicator_ = FindAs<Aero::FrameworkElement>(rootElement, "TimeOfDayIndicator");
    unitNameText_ = FindAs<Aero::TextBlock>(rootElement, "UnitNameText");
    unitLevelText_ = FindAs<Aero::TextBlock>(rootElement, "UnitLevelText");
    hpText_ = FindAs<Aero::TextBlock>(rootElement, "HpText");
    mpText_ = FindAs<Aero::TextBlock>(rootElement, "MpText");
    heroStatsText_ = FindAs<Aero::TextBlock>(rootElement, "HeroStatsText");
    heroStatsPanel_ = FindAs<Aero::FrameworkElement>(rootElement, "HeroStatsPanel");
    heroStrengthText_ = FindAs<Aero::TextBlock>(rootElement, "HeroStrengthText");
    heroAgilityText_ = FindAs<Aero::TextBlock>(rootElement, "HeroAgilityText");
    heroIntelligenceText_ = FindAs<Aero::TextBlock>(rootElement, "HeroIntelligenceText");
    infoStatsPanel_ = FindAs<Aero::FrameworkElement>(rootElement, "InfoStatsPanel");
    const char* attackIconNames[] = {
        "InfoAttackIconDamageUnknown",
        "InfoAttackIconDamageNormal",
        "InfoAttackIconDamageMelee",
        "InfoAttackIconDamagePierce",
        "InfoAttackIconDamageSiege",
        "InfoAttackIconDamageMagic",
        "InfoAttackIconDamageChaos",
        "InfoAttackIconDamageHero",
        "InfoAttackIconDamageUnknownNeutral",
        "InfoAttackIconDamageNormalNeutral",
        "InfoAttackIconDamageMeleeNeutral",
        "InfoAttackIconDamagePierceNeutral",
        "InfoAttackIconDamageSiegeNeutral",
        "InfoAttackIconDamageMagicNeutral",
        "InfoAttackIconDamageChaosNeutral",
        "InfoAttackIconDamageHeroNeutral",
    };
    for (int i = 0; i < (int)infoAttackIcons_.size(); ++i) {
        infoAttackIcons_[i] = FindAs<Aero::Image>(rootElement, attackIconNames[i]);
    }
    const char* armorIconNames[] = {
        "InfoArmorIconArmorUnknown",
        "InfoArmorIconArmorSmall",
        "InfoArmorIconArmorMedium",
        "InfoArmorIconArmorLarge",
        "InfoArmorIconArmorFort",
        "InfoArmorIconArmorNormal",
        "InfoArmorIconArmorHero",
        "InfoArmorIconArmorDivine",
        "InfoArmorIconArmorNone",
        "InfoArmorIconArmorUnknownNeutral",
        "InfoArmorIconArmorSmallNeutral",
        "InfoArmorIconArmorMediumNeutral",
        "InfoArmorIconArmorLargeNeutral",
        "InfoArmorIconArmorFortNeutral",
        "InfoArmorIconArmorNormalNeutral",
        "InfoArmorIconArmorHeroNeutral",
        "InfoArmorIconArmorDivineNeutral",
        "InfoArmorIconArmorNoneNeutral",
    };
    for (int i = 0; i < (int)infoArmorIcons_.size(); ++i) {
        infoArmorIcons_[i] = FindAs<Aero::Image>(rootElement, armorIconNames[i]);
    }
    infoAttackText_ = FindAs<Aero::TextBlock>(rootElement, "InfoAttackText");
    infoArmorText_ = FindAs<Aero::TextBlock>(rootElement, "InfoArmorText");
    infoGoldText_ = FindAs<Aero::TextBlock>(rootElement, "InfoGoldText");
    infoFoodText_ = FindAs<Aero::TextBlock>(rootElement, "InfoFoodText");
    buffStatusBar_ = FindAs<Aero::FrameworkElement>(rootElement, "BuffStatusBar");
    buffStatusBarLabel_ = FindAs<Aero::TextBlock>(rootElement, "BuffStatusBarLabel");
    for (int i = 0; i < (int)buffSlots_.size(); ++i) {
        char name[64];
        std::snprintf(name, sizeof(name), "BuffIcon%d", i);
        buffSlots_[i] = FindAs<Aero::Border>(rootElement, name);
        std::snprintf(name, sizeof(name), "BuffIconImage%d", i);
        buffIcons_[i] = FindAs<Aero::Image>(rootElement, name);
        std::snprintf(name, sizeof(name), "BuffIconText%d", i);
        buffLabels_[i] = FindAs<Aero::TextBlock>(rootElement, name);
        std::snprintf(name, sizeof(name), "BuffDuration%d", i);
        buffDurations_[i] = FindAs<Aero::TextBlock>(rootElement, name);
        std::snprintf(name, sizeof(name), "BuffStackBack%d", i);
        buffStackBacks_[i] = FindAs<Aero::Border>(rootElement, name);
        std::snprintf(name, sizeof(name), "BuffStackText%d", i);
        buffStacks_[i] = FindAs<Aero::TextBlock>(rootElement, name);
    }
    hpFill_ = FindAs<Aero::Border>(rootElement, "HpFill");
    mpFill_ = FindAs<Aero::Border>(rootElement, "MpFill");
    xpFill_ = FindAs<Aero::Border>(rootElement, "XpFill");
    tooltipPanel_ = FindAs<Aero::Border>(rootElement, "TooltipPanel");
    tooltipText_ = FindAs<Aero::TextBlock>(rootElement, "TooltipText");
    tooltipResourceRow_ = FindAs<Aero::FrameworkElement>(rootElement, "TooltipResourceRow");
    tooltipCostGroups_[0] = FindAs<Aero::FrameworkElement>(rootElement, "TooltipGoldCost");
    tooltipCostGroups_[1] = FindAs<Aero::FrameworkElement>(rootElement, "TooltipLumberCost");
    tooltipCostGroups_[2] = FindAs<Aero::FrameworkElement>(rootElement, "TooltipStoneCost");
    tooltipCostGroups_[3] = FindAs<Aero::FrameworkElement>(rootElement, "TooltipManaCost");
    tooltipCostGroups_[4] = FindAs<Aero::FrameworkElement>(rootElement, "TooltipFoodCost");
    tooltipCostTexts_[0] = FindAs<Aero::TextBlock>(rootElement, "TooltipGoldCostText");
    tooltipCostTexts_[1] = FindAs<Aero::TextBlock>(rootElement, "TooltipLumberCostText");
    tooltipCostTexts_[2] = FindAs<Aero::TextBlock>(rootElement, "TooltipStoneCostText");
    tooltipCostTexts_[3] = FindAs<Aero::TextBlock>(rootElement, "TooltipManaCostText");
    tooltipCostTexts_[4] = FindAs<Aero::TextBlock>(rootElement, "TooltipFoodCostText");
    chatPanel_ = FindAs<Aero::Border>(rootElement, "ChatPanel");
    chatLogText_ = FindAs<Aero::TextBlock>(rootElement, "ChatLogText");
    chatInput_ = FindAs<Aero::TextBox>(rootElement, "ChatInputBox");
    minimapViewport_ = FindAs<Aero::FrameworkElement>(rootElement, "MinimapViewport");
    minimapContentViewport_ = FindAs<Aero::FrameworkElement>(rootElement, "MinimapContentViewport");
    minimapButtons_[0] = FindAs<Aero::FrameworkElement>(rootElement, "MiniMapSignalButton");
    minimapButtons_[1] = FindAs<Aero::FrameworkElement>(rootElement, "MiniMapTerrainButton");
    minimapButtons_[2] = FindAs<Aero::FrameworkElement>(rootElement, "MiniMapAllyButton");
    minimapButtons_[3] = FindAs<Aero::FrameworkElement>(rootElement, "MiniMapCreepButton");
    minimapButtons_[4] = FindAs<Aero::FrameworkElement>(rootElement, "MiniMapFormationButton");
    minimapButtonPushed_[0] = FindAs<Aero::Image>(rootElement, "MiniMapSignalButtonPushed");
    minimapButtonPushed_[1] = FindAs<Aero::Image>(rootElement, "MiniMapTerrainButtonPushed");
    minimapButtonPushed_[2] = FindAs<Aero::Image>(rootElement, "MiniMapAllyButtonPushed");
    minimapButtonPushed_[3] = FindAs<Aero::Image>(rootElement, "MiniMapCreepButtonPushed");
    minimapButtonPushed_[4] = FindAs<Aero::Image>(rootElement, "MiniMapFormationButtonPushed");
    minimapButtonDisabled_[0] = FindAs<Aero::Image>(rootElement, "MiniMapSignalButtonDisabled");
    minimapButtonDisabled_[1] = FindAs<Aero::Image>(rootElement, "MiniMapTerrainButtonDisabled");
    minimapButtonDisabled_[2] = FindAs<Aero::Image>(rootElement, "MiniMapAllyButtonDisabled");
    minimapButtonDisabled_[3] = FindAs<Aero::Image>(rootElement, "MiniMapCreepButtonDisabled");
    minimapButtonDisabled_[4] = FindAs<Aero::Image>(rootElement, "MiniMapFormationButtonDisabled");
    minimapButtonInactive_[1] = FindAs<Aero::Image>(rootElement, "MiniMapTerrainButtonInactive");
    minimapButtonInactive_[2] = FindAs<Aero::Image>(rootElement, "MiniMapAllyButtonInactive");
    minimapButtonInactive_[3] = FindAs<Aero::Image>(rootElement, "MiniMapCreepButtonInactive");
    minimapButtonInactivePushed_[1] = FindAs<Aero::Image>(rootElement, "MiniMapTerrainButtonInactivePushed");
    minimapButtonInactivePushed_[2] = FindAs<Aero::Image>(rootElement, "MiniMapAllyButtonInactivePushed");
    minimapButtonInactivePushed_[3] = FindAs<Aero::Image>(rootElement, "MiniMapCreepButtonInactivePushed");
    minimapButtonInactiveDisabled_[1] = FindAs<Aero::Image>(rootElement, "MiniMapTerrainButtonInactiveDisabled");
    minimapButtonInactiveDisabled_[2] = FindAs<Aero::Image>(rootElement, "MiniMapAllyButtonInactiveDisabled");
    minimapButtonInactiveDisabled_[3] = FindAs<Aero::Image>(rootElement, "MiniMapCreepButtonInactiveDisabled");
    minimapButtonOff_[2] = FindAs<Aero::Image>(rootElement, "MiniMapAllyButtonOff");
    minimapButtonOff_[4] = FindAs<Aero::Image>(rootElement, "MiniMapFormationButtonOff");
    minimapButtonOffPushed_[2] = FindAs<Aero::Image>(rootElement, "MiniMapAllyButtonOffPushed");
    minimapButtonOffPushed_[4] = FindAs<Aero::Image>(rootElement, "MiniMapFormationButtonOffPushed");
    minimapButtonOffDisabled_[2] = FindAs<Aero::Image>(rootElement, "MiniMapAllyButtonOffDisabled");
    minimapButtonOffDisabled_[4] = FindAs<Aero::Image>(rootElement, "MiniMapFormationButtonOffDisabled");
    portraitViewport_ = FindAs<Aero::FrameworkElement>(rootElement, "PortraitViewport");
    portraitIcon_ = FindAs<Aero::FrameworkElement>(rootElement, "PortraitIcon");
    portraitImage_ = FindAs<Aero::Image>(rootElement, "PortraitIcon");
    consolePanel_ = FindAs<Aero::FrameworkElement>(rootElement, "ConsolePanel");
    upperButtonBar_ = FindAs<Aero::FrameworkElement>(rootElement, "UpperButtonBarFrame");
    upperButtons_[0] = FindAs<Aero::FrameworkElement>(rootElement, "UpperButtonBarQuestsButton");
    upperButtons_[1] = FindAs<Aero::FrameworkElement>(rootElement, "UpperButtonBarMenuButton");
    upperButtons_[2] = FindAs<Aero::FrameworkElement>(rootElement, "UpperButtonBarAlliesButton");
    upperButtons_[3] = FindAs<Aero::FrameworkElement>(rootElement, "UpperButtonBarChatButton");
    upperButtonPushed_[0] = FindAs<Aero::Image>(rootElement, "UpperButtonBarQuestsPushed");
    upperButtonPushed_[1] = FindAs<Aero::Image>(rootElement, "UpperButtonBarMenuPushed");
    upperButtonPushed_[2] = FindAs<Aero::Image>(rootElement, "UpperButtonBarAlliesPushed");
    upperButtonPushed_[3] = FindAs<Aero::Image>(rootElement, "UpperButtonBarChatPushed");
    upperButtonHighlights_[0] = FindAs<Aero::Image>(rootElement, "UpperButtonBarQuestsHighlight");
    upperButtonHighlights_[1] = FindAs<Aero::Image>(rootElement, "UpperButtonBarMenuHighlight");
    upperButtonHighlights_[2] = FindAs<Aero::Image>(rootElement, "UpperButtonBarAlliesHighlight");
    upperButtonHighlights_[3] = FindAs<Aero::Image>(rootElement, "UpperButtonBarChatHighlight");
    upperButtonLabels_[0] = FindAs<Aero::TextBlock>(rootElement, "UpperButtonBarQuestsText");
    upperButtonLabels_[1] = FindAs<Aero::TextBlock>(rootElement, "UpperButtonBarMenuText");
    upperButtonLabels_[2] = FindAs<Aero::TextBlock>(rootElement, "UpperButtonBarAlliesText");
    upperButtonLabels_[3] = FindAs<Aero::TextBlock>(rootElement, "UpperButtonBarChatText");
    commandColumn_ = FindAs<Aero::FrameworkElement>(rootElement, "CommandColumn");
    inventoryCover_ = FindAs<Aero::FrameworkElement>(rootElement, "InventoryCover");
    inventoryPanel_ = FindAs<Aero::FrameworkElement>(rootElement, "InventoryPanel");
    inventoryGrid_ = FindAs<Aero::FrameworkElement>(rootElement, "InventoryGrid");
    commandCardPanel_ = FindAs<Aero::FrameworkElement>(rootElement, "CommandCardPanel");
    commandCardGrid_ = FindAs<Aero::FrameworkElement>(rootElement, "CommandCardGrid");

    for (int i = 0; i < 12; ++i) {
        char buttonName[32];
        char iconName[32];
        char textName[32];
        char disabledName[40];
        char hoverName[40];
        char pressedName[40];
        char activeName[40];
        char cooldownName[40];
        char cooldownTextName[40];
        char numberBackName[40];
        char numberName[32];
        char autocastName[40];
        std::snprintf(buttonName, sizeof(buttonName), "CommandButton%d", i);
        std::snprintf(iconName, sizeof(iconName), "CommandIcon%d", i);
        std::snprintf(textName, sizeof(textName), "CommandText%d", i);
        std::snprintf(disabledName, sizeof(disabledName), "CommandDisabledBack%d", i);
        std::snprintf(hoverName, sizeof(hoverName), "CommandHoverHighlight%d", i);
        std::snprintf(pressedName, sizeof(pressedName), "CommandPressedBorder%d", i);
        std::snprintf(activeName, sizeof(activeName), "CommandActiveHighlight%d", i);
        std::snprintf(cooldownName, sizeof(cooldownName), "CommandCooldown%d", i);
        std::snprintf(cooldownTextName, sizeof(cooldownTextName), "CommandCooldownText%d", i);
        std::snprintf(numberBackName, sizeof(numberBackName), "CommandNumberBack%d", i);
        std::snprintf(numberName, sizeof(numberName), "CommandNumber%d", i);
        std::snprintf(autocastName, sizeof(autocastName), "CommandAutocast%d", i);
        commandButtons_[i] = FindAs<Aero::FrameworkElement>(rootElement, buttonName);
        commandIcons_[i] = FindAs<Aero::Image>(rootElement, iconName);
        commandLabels_[i] = FindAs<Aero::TextBlock>(rootElement, textName);
        commandDisabledBacks_[i] = FindAs<Aero::Border>(rootElement, disabledName);
        commandHoverHighlights_[i] = FindAs<Aero::Border>(rootElement, hoverName);
        commandPressedBorders_[i] = FindAs<Aero::Border>(rootElement, pressedName);
        commandActiveHighlights_[i] = FindAs<Aero::Border>(rootElement, activeName);
        commandCooldownOverlays_[i] = FindAs<Aero::Border>(rootElement, cooldownName);
        commandCooldownLabels_[i] = FindAs<Aero::TextBlock>(rootElement, cooldownTextName);
        commandNumberBacks_[i] = FindAs<Aero::Border>(rootElement, numberBackName);
        commandNumberLabels_[i] = FindAs<Aero::TextBlock>(rootElement, numberName);
        commandAutocastOverlays_[i] = FindAs<Aero::Border>(rootElement, autocastName);
    }

    for (int i = 0; i < 12; ++i) {
        char slotName[32];
        char colorName[32];
        char focusName[40];
        char heroGlowName[40];
        char hpName[32];
        char mpName[32];
        char levelName[32];
        std::snprintf(slotName, sizeof(slotName), "MultiSelectSlot%d", i);
        std::snprintf(colorName, sizeof(colorName), "MultiSelectColor%d", i);
        std::snprintf(focusName, sizeof(focusName), "MultiSelectFocus%d", i);
        std::snprintf(heroGlowName, sizeof(heroGlowName), "MultiSelectHeroGlow%d", i);
        std::snprintf(hpName, sizeof(hpName), "MultiSelectHp%d", i);
        std::snprintf(mpName, sizeof(mpName), "MultiSelectMp%d", i);
        std::snprintf(levelName, sizeof(levelName), "MultiSelectLevel%d", i);
        multiSelectSlots_[i] = FindAs<Aero::Border>(rootElement, slotName);
        multiSelectColors_[i] = FindAs<Aero::Border>(rootElement, colorName);
        multiSelectFocusHighlights_[i] = FindAs<Aero::Border>(rootElement, focusName);
        multiSelectHeroGlows_[i] = FindAs<Aero::Border>(rootElement, heroGlowName);
        multiSelectHpFills_[i] = FindAs<Aero::Border>(rootElement, hpName);
        multiSelectMpFills_[i] = FindAs<Aero::Border>(rootElement, mpName);
        multiSelectLevelLabels_[i] = FindAs<Aero::TextBlock>(rootElement, levelName);
    }

    for (int i = 0; i < 5; ++i) {
        char slotName[32];
        char iconName[32];
        char textName[32];
        std::snprintf(slotName, sizeof(slotName), "QueueSlot%d", i);
        std::snprintf(iconName, sizeof(iconName), "QueueIcon%d", i);
        std::snprintf(textName, sizeof(textName), "QueueText%d", i);
        queueSlots_[i] = FindAs<Aero::Border>(rootElement, slotName);
        queueIcons_[i] = FindAs<Aero::Image>(rootElement, iconName);
        queueLabels_[i] = FindAs<Aero::TextBlock>(rootElement, textName);
    }
    queueBackdrop_ = FindAs<Aero::Border>(rootElement, "QueueBackdrop");
    queueProgressTrack_ = FindAs<Aero::FrameworkElement>(rootElement, "QueueProgressTrack");
    queueProgressFill_ = FindAs<Aero::Border>(rootElement, "QueueProgressFill");

    for (int i = 0; i < 6; ++i) {
        char slotName[32];
        char iconName[32];
        char textName[32];
        char chargesBackName[40];
        char chargesName[32];
        std::snprintf(slotName, sizeof(slotName), "InventorySlot%d", i);
        std::snprintf(chargesBackName, sizeof(chargesBackName), "InventoryDisabledBack%d", i);
        std::snprintf(iconName, sizeof(iconName), "InventoryIcon%d", i);
        std::snprintf(textName, sizeof(textName), "InventoryText%d", i);
        std::snprintf(chargesName, sizeof(chargesName), "InventoryCharges%d", i);
        inventorySlots_[i] = FindAs<Aero::Border>(rootElement, slotName);
        inventoryDisabledBacks_[i] = FindAs<Aero::Border>(rootElement, chargesBackName);
        inventoryIcons_[i] = FindAs<Aero::Image>(rootElement, iconName);
        inventoryLabels_[i] = FindAs<Aero::TextBlock>(rootElement, textName);
        std::snprintf(chargesBackName, sizeof(chargesBackName), "InventoryChargesBack%d", i);
        inventoryChargeBacks_[i] = FindAs<Aero::Border>(rootElement, chargesBackName);
        inventoryChargeLabels_[i] = FindAs<Aero::TextBlock>(rootElement, chargesName);
    }

    gameMenuRoot_ = FindAs<Aero::FrameworkElement>(rootElement, "SmashEscMenu");
    gameMenuBackdrop_ = FindAs<Aero::FrameworkElement>(rootElement, "EscMenuBackdrop");
    gameMenuMainPanel_ = FindAs<Aero::FrameworkElement>(rootElement, "GameMenuMainPanel");
    gameMenuEndGamePanel_ = FindAs<Aero::FrameworkElement>(rootElement, "GameMenuEndGamePanel");
    gameMenuConfirmQuitPanel_ = FindAs<Aero::FrameworkElement>(rootElement, "GameMenuConfirmQuitPanel");
    gameMenuSaveGamePanel_ = FindAs<Aero::FrameworkElement>(rootElement, "GameMenuSaveGamePanel");
    gameMenuLoadGamePanel_ = FindAs<Aero::FrameworkElement>(rootElement, "GameMenuLoadGamePanel");
    gameMenuOverwriteConfirmPanel_ = FindAs<Aero::FrameworkElement>(rootElement, "GameMenuOverwriteConfirmPanel");
    gameMenuDeleteConfirmPanel_ = FindAs<Aero::FrameworkElement>(rootElement, "GameMenuDeleteConfirmPanel");
    gameMenuHelpPanel_ = FindAs<Aero::FrameworkElement>(rootElement, "GameMenuHelpPanel");
    gameMenuTipsPanel_ = FindAs<Aero::FrameworkElement>(rootElement, "GameMenuTipsPanel");
    gameMenuOptionsPanel_ = FindAs<Aero::FrameworkElement>(rootElement, "GameMenuOptionsPanel");
    gameMenuOptionsListPanel_ = FindAs<Aero::FrameworkElement>(rootElement, "GameMenuOptionsListPanel");
    gameMenuGameplayPanel_ = FindAs<Aero::FrameworkElement>(rootElement, "GameMenuGameplayPanel");
    gameMenuVideoPanel_ = FindAs<Aero::FrameworkElement>(rootElement, "GameMenuVideoPanel");
    gameMenuSoundPanel_ = FindAs<Aero::FrameworkElement>(rootElement, "GameMenuSoundPanel");
    gameMenuNetworkPanel_ = FindAs<Aero::FrameworkElement>(rootElement, "GameMenuNetworkPanel");
    gameMenuOptionsBottomPanel_ = FindAs<Aero::FrameworkElement>(rootElement, "GameMenuOptionsBottomPanel");
    gameMenuPauseText_ = FindAs<Aero::TextBlock>(rootElement, "GameMenuPauseText");

    static_assert(kGameMenuActionElementNameCount == kGameMenuActionCount);
    for (int i = 0; i < kGameMenuActionCount; ++i) {
        const char* elementName = GameMenuActionElementName(i);
        gameMenuButtons_[i] = FindAs<Aero::FrameworkElement>(rootElement, elementName);
        gameMenuButtonHighlights_[i] = nullptr;
        gameMenuButtonPressed_[i] = nullptr;
        gameMenuOptionDisabled_[i] = nullptr;
        gameMenuOptionChecked_[i] = nullptr;
        gameMenuOptionDisabledChecked_[i] = nullptr;
        gameMenuOptionSliderThumbs_[i] = nullptr;
        gameMenuOptionPopupMenus_[i] = nullptr;
        gameMenuOptionValueTexts_[i] = nullptr;

        char name[96];
        if (i <= kGameMenuTipsOK) {
            std::snprintf(name, sizeof(name), "GameMenuButtonHover%s", kGameMenuButtonSuffixes[i]);
            gameMenuButtonHighlights_[i] = FindAs<Aero::FrameworkElement>(rootElement, name);
            std::snprintf(name, sizeof(name), "GameMenuButtonPressed%s", kGameMenuButtonSuffixes[i]);
            gameMenuButtonPressed_[i] = FindAs<Aero::FrameworkElement>(rootElement, name);
        } else if (IsGameMenuOptionCheckboxAction(i) || IsGameMenuOptionRadioAction(i)) {
            std::snprintf(name, sizeof(name), "%sPressed", elementName);
            gameMenuButtonPressed_[i] = FindAs<Aero::FrameworkElement>(rootElement, name);
        }

        if (IsGameMenuOptionCheckboxAction(i)) {
            std::snprintf(name, sizeof(name), "%sDisabled", elementName);
            gameMenuOptionDisabled_[i] = FindAs<Aero::FrameworkElement>(rootElement, name);
            std::snprintf(name, sizeof(name), "%sCheck", elementName);
            gameMenuOptionChecked_[i] = FindAs<Aero::FrameworkElement>(rootElement, name);
            std::snprintf(name, sizeof(name), "%sDisabledCheck", elementName);
            gameMenuOptionDisabledChecked_[i] = FindAs<Aero::FrameworkElement>(rootElement, name);
        } else if (IsGameMenuOptionRadioAction(i)) {
            std::snprintf(name, sizeof(name), "%sDisabled", elementName);
            gameMenuOptionDisabled_[i] = FindAs<Aero::FrameworkElement>(rootElement, name);
            std::snprintf(name, sizeof(name), "%sSelected", elementName);
            gameMenuOptionChecked_[i] = FindAs<Aero::FrameworkElement>(rootElement, name);
            std::snprintf(name, sizeof(name), "%sDisabledSelected", elementName);
            gameMenuOptionDisabledChecked_[i] = FindAs<Aero::FrameworkElement>(rootElement, name);
        } else if (IsGameMenuOptionSliderAction(i)) {
            std::snprintf(name, sizeof(name), "%sDisabledBorder", elementName);
            gameMenuOptionDisabled_[i] = FindAs<Aero::FrameworkElement>(rootElement, name);
            std::snprintf(name, sizeof(name), "%sThumb", elementName);
            gameMenuOptionSliderThumbs_[i] = FindAs<Aero::FrameworkElement>(rootElement, name);
            std::snprintf(name, sizeof(name), "%sDisabledThumb", elementName);
            gameMenuOptionDisabledChecked_[i] = FindAs<Aero::FrameworkElement>(rootElement, name);
        } else if (IsGameMenuOptionPopupAction(i)) {
            std::snprintf(name, sizeof(name), "%sValue", elementName);
            gameMenuOptionValueTexts_[i] = FindAs<Aero::TextBlock>(rootElement, name);
            std::snprintf(name, sizeof(name), "%sMenu", elementName);
            gameMenuOptionPopupMenus_[i] = FindAs<Aero::FrameworkElement>(rootElement, name);
        }
    }

    if (!gameMenuOptionStateInitialized_) {
        gameMenuOptionCheckedState_.fill(false);
        gameMenuOptionCycleIndex_.fill(0);
        gameMenuOptionCheckedState_[kGameMenuOptionEnhancedTooltips] = true;
        gameMenuOptionCheckedState_[kGameMenuOptionSoundVolume] = true;
        gameMenuOptionCheckedState_[kGameMenuOptionMusicVolume] = true;
        gameMenuOptionCheckedState_[kGameMenuOptionLowLatency] = true;
        gameMenuOptionCycleIndex_[kGameMenuOptionParticles] = 2;
        gameMenuOptionCycleIndex_[kGameMenuOptionLights] = 2;
        gameMenuOptionCycleIndex_[kGameMenuOptionShadows] = 1;
        gameMenuOptionCycleIndex_[kGameMenuOptionOcclusion] = 1;
        gameMenuOptionStateInitialized_ = true;
    }

}

void NoesisHudHost::LogMissingElements() const {
    if (glueMenuControl_) {
        glueMenuControl_->LogMissingElements();
        return;
    }

    std::vector<std::string> missing;
    auto check = [&missing](const void* ptr, const char* name) {
        if (!ptr) missing.push_back(name);
    };

    check(goldText_, "ResourceBarGoldText");
    check(lumberText_, "ResourceBarLumberText");
    check(foodText_, "ResourceBarSupplyText");
    check(upkeepText_, "ResourceBarUpkeepText");
    check(timeText_, "TimeText");
    check(timeOfDayIndicator_, "TimeOfDayIndicator");
    check(unitNameText_, "UnitNameText");
    check(unitLevelText_, "UnitLevelText");
    check(hpText_, "HpText");
    check(mpText_, "MpText");
    check(heroStatsText_, "HeroStatsText");
    check(heroStatsPanel_, "HeroStatsPanel");
    check(heroStrengthText_, "HeroStrengthText");
    check(heroAgilityText_, "HeroAgilityText");
    check(heroIntelligenceText_, "HeroIntelligenceText");
    check(infoStatsPanel_, "InfoStatsPanel");
    const char* attackIconNames[] = {
        "InfoAttackIconDamageUnknown",
        "InfoAttackIconDamageNormal",
        "InfoAttackIconDamageMelee",
        "InfoAttackIconDamagePierce",
        "InfoAttackIconDamageSiege",
        "InfoAttackIconDamageMagic",
        "InfoAttackIconDamageChaos",
        "InfoAttackIconDamageHero",
        "InfoAttackIconDamageUnknownNeutral",
        "InfoAttackIconDamageNormalNeutral",
        "InfoAttackIconDamageMeleeNeutral",
        "InfoAttackIconDamagePierceNeutral",
        "InfoAttackIconDamageSiegeNeutral",
        "InfoAttackIconDamageMagicNeutral",
        "InfoAttackIconDamageChaosNeutral",
        "InfoAttackIconDamageHeroNeutral",
    };
    for (int i = 0; i < (int)infoAttackIcons_.size(); ++i) {
        check(infoAttackIcons_[i], attackIconNames[i]);
    }
    const char* armorIconNames[] = {
        "InfoArmorIconArmorUnknown",
        "InfoArmorIconArmorSmall",
        "InfoArmorIconArmorMedium",
        "InfoArmorIconArmorLarge",
        "InfoArmorIconArmorFort",
        "InfoArmorIconArmorNormal",
        "InfoArmorIconArmorHero",
        "InfoArmorIconArmorDivine",
        "InfoArmorIconArmorNone",
        "InfoArmorIconArmorUnknownNeutral",
        "InfoArmorIconArmorSmallNeutral",
        "InfoArmorIconArmorMediumNeutral",
        "InfoArmorIconArmorLargeNeutral",
        "InfoArmorIconArmorFortNeutral",
        "InfoArmorIconArmorNormalNeutral",
        "InfoArmorIconArmorHeroNeutral",
        "InfoArmorIconArmorDivineNeutral",
        "InfoArmorIconArmorNoneNeutral",
    };
    for (int i = 0; i < (int)infoArmorIcons_.size(); ++i) {
        check(infoArmorIcons_[i], armorIconNames[i]);
    }
    check(infoAttackText_, "InfoAttackText");
    check(infoArmorText_, "InfoArmorText");
    check(infoGoldText_, "InfoGoldText");
    check(infoFoodText_, "InfoFoodText");
    check(buffStatusBar_, "BuffStatusBar");
    check(buffStatusBarLabel_, "BuffStatusBarLabel");
    for (int i = 0; i < 8; ++i) {
        char name[64];
        std::snprintf(name, sizeof(name), "BuffIcon%d", i);
        if (!buffSlots_[i]) missing.push_back(name);
        std::snprintf(name, sizeof(name), "BuffIconImage%d", i);
        if (!buffIcons_[i]) missing.push_back(name);
        std::snprintf(name, sizeof(name), "BuffIconText%d", i);
        if (!buffLabels_[i]) missing.push_back(name);
        std::snprintf(name, sizeof(name), "BuffDuration%d", i);
        if (!buffDurations_[i]) missing.push_back(name);
        std::snprintf(name, sizeof(name), "BuffStackBack%d", i);
        if (!buffStackBacks_[i]) missing.push_back(name);
        std::snprintf(name, sizeof(name), "BuffStackText%d", i);
        if (!buffStacks_[i]) missing.push_back(name);
    }
    check(hpFill_, "HpFill");
    check(mpFill_, "MpFill");
    check(xpFill_, "XpFill");
    check(tooltipPanel_, "TooltipPanel");
    check(tooltipText_, "TooltipText");
    check(tooltipResourceRow_, "TooltipResourceRow");
    check(tooltipCostGroups_[0], "TooltipGoldCost");
    check(tooltipCostGroups_[1], "TooltipLumberCost");
    check(tooltipCostGroups_[2], "TooltipStoneCost");
    check(tooltipCostGroups_[3], "TooltipManaCost");
    check(tooltipCostGroups_[4], "TooltipFoodCost");
    check(tooltipCostTexts_[0], "TooltipGoldCostText");
    check(tooltipCostTexts_[1], "TooltipLumberCostText");
    check(tooltipCostTexts_[2], "TooltipStoneCostText");
    check(tooltipCostTexts_[3], "TooltipManaCostText");
    check(tooltipCostTexts_[4], "TooltipFoodCostText");
    check(chatPanel_, "ChatPanel");
    check(chatLogText_, "ChatLogText");
    check(chatInput_, "ChatInputBox");
    check(minimapViewport_, "MinimapViewport");
    check(minimapContentViewport_, "MinimapContentViewport");
    check(minimapButtons_[0], "MiniMapSignalButton");
    check(minimapButtons_[1], "MiniMapTerrainButton");
    check(minimapButtons_[2], "MiniMapAllyButton");
    check(minimapButtons_[3], "MiniMapCreepButton");
    check(minimapButtons_[4], "MiniMapFormationButton");
    check(minimapButtonPushed_[0], "MiniMapSignalButtonPushed");
    check(minimapButtonPushed_[1], "MiniMapTerrainButtonPushed");
    check(minimapButtonPushed_[2], "MiniMapAllyButtonPushed");
    check(minimapButtonPushed_[3], "MiniMapCreepButtonPushed");
    check(minimapButtonPushed_[4], "MiniMapFormationButtonPushed");
    check(minimapButtonDisabled_[0], "MiniMapSignalButtonDisabled");
    check(minimapButtonDisabled_[1], "MiniMapTerrainButtonDisabled");
    check(minimapButtonDisabled_[2], "MiniMapAllyButtonDisabled");
    check(minimapButtonDisabled_[3], "MiniMapCreepButtonDisabled");
    check(minimapButtonDisabled_[4], "MiniMapFormationButtonDisabled");
    check(minimapButtonInactive_[1], "MiniMapTerrainButtonInactive");
    check(minimapButtonInactive_[2], "MiniMapAllyButtonInactive");
    check(minimapButtonInactive_[3], "MiniMapCreepButtonInactive");
    check(minimapButtonInactivePushed_[1], "MiniMapTerrainButtonInactivePushed");
    check(minimapButtonInactivePushed_[2], "MiniMapAllyButtonInactivePushed");
    check(minimapButtonInactivePushed_[3], "MiniMapCreepButtonInactivePushed");
    check(minimapButtonInactiveDisabled_[1], "MiniMapTerrainButtonInactiveDisabled");
    check(minimapButtonInactiveDisabled_[2], "MiniMapAllyButtonInactiveDisabled");
    check(minimapButtonInactiveDisabled_[3], "MiniMapCreepButtonInactiveDisabled");
    check(minimapButtonOff_[2], "MiniMapAllyButtonOff");
    check(minimapButtonOff_[4], "MiniMapFormationButtonOff");
    check(minimapButtonOffPushed_[2], "MiniMapAllyButtonOffPushed");
    check(minimapButtonOffPushed_[4], "MiniMapFormationButtonOffPushed");
    check(minimapButtonOffDisabled_[2], "MiniMapAllyButtonOffDisabled");
    check(minimapButtonOffDisabled_[4], "MiniMapFormationButtonOffDisabled");
    check(portraitViewport_, "PortraitViewport");
    check(portraitImage_, "PortraitIcon");
    check(consolePanel_, "ConsolePanel");
    check(upperButtonBar_, "UpperButtonBarFrame");
    check(upperButtons_[0], "UpperButtonBarQuestsButton");
    check(upperButtons_[1], "UpperButtonBarMenuButton");
    check(upperButtons_[2], "UpperButtonBarAlliesButton");
    check(upperButtons_[3], "UpperButtonBarChatButton");
    check(upperButtonPushed_[0], "UpperButtonBarQuestsPushed");
    check(upperButtonPushed_[1], "UpperButtonBarMenuPushed");
    check(upperButtonPushed_[2], "UpperButtonBarAlliesPushed");
    check(upperButtonPushed_[3], "UpperButtonBarChatPushed");
    check(upperButtonHighlights_[0], "UpperButtonBarQuestsHighlight");
    check(upperButtonHighlights_[1], "UpperButtonBarMenuHighlight");
    check(upperButtonHighlights_[2], "UpperButtonBarAlliesHighlight");
    check(upperButtonHighlights_[3], "UpperButtonBarChatHighlight");
    check(upperButtonLabels_[0], "UpperButtonBarQuestsText");
    check(upperButtonLabels_[1], "UpperButtonBarMenuText");
    check(upperButtonLabels_[2], "UpperButtonBarAlliesText");
    check(upperButtonLabels_[3], "UpperButtonBarChatText");
    check(commandColumn_, "CommandColumn");
    check(inventoryCover_, "InventoryCover");
    check(inventoryPanel_, "InventoryPanel");
    check(inventoryGrid_, "InventoryGrid");
    check(commandCardPanel_, "CommandCardPanel");
    check(commandCardGrid_, "CommandCardGrid");

    for (int i = 0; i < 12; ++i) {
        char name[64];
        std::snprintf(name, sizeof(name), "CommandButton%d", i);
        if (!commandButtons_[i]) missing.push_back(name);
        std::snprintf(name, sizeof(name), "CommandIcon%d", i);
        if (!commandIcons_[i]) missing.push_back(name);
        std::snprintf(name, sizeof(name), "CommandText%d", i);
        if (!commandLabels_[i]) missing.push_back(name);
        std::snprintf(name, sizeof(name), "CommandDisabledBack%d", i);
        if (!commandDisabledBacks_[i]) missing.push_back(name);
        std::snprintf(name, sizeof(name), "CommandHoverHighlight%d", i);
        if (!commandHoverHighlights_[i]) missing.push_back(name);
        std::snprintf(name, sizeof(name), "CommandPressedBorder%d", i);
        if (!commandPressedBorders_[i]) missing.push_back(name);
        std::snprintf(name, sizeof(name), "CommandActiveHighlight%d", i);
        if (!commandActiveHighlights_[i]) missing.push_back(name);
        std::snprintf(name, sizeof(name), "CommandCooldown%d", i);
        if (!commandCooldownOverlays_[i]) missing.push_back(name);
        std::snprintf(name, sizeof(name), "CommandCooldownText%d", i);
        if (!commandCooldownLabels_[i]) missing.push_back(name);
        std::snprintf(name, sizeof(name), "CommandNumber%d", i);
        if (!commandNumberLabels_[i]) missing.push_back(name);
        std::snprintf(name, sizeof(name), "CommandAutocast%d", i);
        if (!commandAutocastOverlays_[i]) missing.push_back(name);
        std::snprintf(name, sizeof(name), "MultiSelectSlot%d", i);
        if (!multiSelectSlots_[i]) missing.push_back(name);
        std::snprintf(name, sizeof(name), "MultiSelectFocus%d", i);
        if (!multiSelectFocusHighlights_[i]) missing.push_back(name);
        std::snprintf(name, sizeof(name), "MultiSelectHeroGlow%d", i);
        if (!multiSelectHeroGlows_[i]) missing.push_back(name);
    }

    for (int i = 0; i < 5; ++i) {
        char name[64];
        std::snprintf(name, sizeof(name), "QueueSlot%d", i);
        if (!queueSlots_[i]) missing.push_back(name);
        std::snprintf(name, sizeof(name), "QueueIcon%d", i);
        if (!queueIcons_[i]) missing.push_back(name);
        std::snprintf(name, sizeof(name), "QueueText%d", i);
        if (!queueLabels_[i]) missing.push_back(name);
    }
    check(queueBackdrop_, "QueueBackdrop");

    for (int i = 0; i < 6; ++i) {
        char name[64];
        std::snprintf(name, sizeof(name), "InventorySlot%d", i);
        if (!inventorySlots_[i]) missing.push_back(name);
        std::snprintf(name, sizeof(name), "InventoryDisabledBack%d", i);
        if (!inventoryDisabledBacks_[i]) missing.push_back(name);
        std::snprintf(name, sizeof(name), "InventoryIcon%d", i);
        if (!inventoryIcons_[i]) missing.push_back(name);
        std::snprintf(name, sizeof(name), "InventoryText%d", i);
        if (!inventoryLabels_[i]) missing.push_back(name);
    }

    check(gameMenuRoot_, "SmashEscMenu");
    check(gameMenuBackdrop_, "EscMenuBackdrop");
    check(gameMenuMainPanel_, "GameMenuMainPanel");
    check(gameMenuEndGamePanel_, "GameMenuEndGamePanel");
    check(gameMenuConfirmQuitPanel_, "GameMenuConfirmQuitPanel");
    check(gameMenuSaveGamePanel_, "GameMenuSaveGamePanel");
    check(gameMenuLoadGamePanel_, "GameMenuLoadGamePanel");
    check(gameMenuOverwriteConfirmPanel_, "GameMenuOverwriteConfirmPanel");
    check(gameMenuDeleteConfirmPanel_, "GameMenuDeleteConfirmPanel");
    check(gameMenuHelpPanel_, "GameMenuHelpPanel");
    check(gameMenuTipsPanel_, "GameMenuTipsPanel");
    check(gameMenuOptionsPanel_, "GameMenuOptionsPanel");
    check(gameMenuOptionsListPanel_, "GameMenuOptionsListPanel");
    check(gameMenuGameplayPanel_, "GameMenuGameplayPanel");
    check(gameMenuVideoPanel_, "GameMenuVideoPanel");
    check(gameMenuSoundPanel_, "GameMenuSoundPanel");
    check(gameMenuNetworkPanel_, "GameMenuNetworkPanel");
    check(gameMenuOptionsBottomPanel_, "GameMenuOptionsBottomPanel");
    check(gameMenuPauseText_, "GameMenuPauseText");
    static_assert(kGameMenuActionElementNameCount == kGameMenuActionCount);
    for (int i = 0; i < kGameMenuActionCount; ++i) {
        if (!gameMenuButtons_[i]) missing.push_back(GameMenuActionElementName(i));
    }

    if (missing.empty()) {
        HudDiagLog("AeroHudHost: all named HUD elements found");
        return;
    }

    std::ostringstream ss;
    ss << "AeroHudHost: missing " << missing.size() << " named elements:";
    const size_t limit = std::min<size_t>(missing.size(), 48);
    for (size_t i = 0; i < limit; ++i) {
        ss << ' ' << missing[i];
    }
    if (missing.size() > limit) {
        ss << " ...";
    }
    HudDiagLog("%s", ss.str().c_str());
}

void NoesisHudHost::CacheRects() {
    minimapRect_ = GetElementRect(minimapContentViewport_ ? minimapContentViewport_ : minimapViewport_);
    if (minimapRect_.w < 8.0f || minimapRect_.h < 8.0f) {
        minimapRect_ = BuildFallbackMinimapRect(width_, height_, dpiScale_);
    }
    portraitRect_ = GetElementRect(portraitViewport_);
    for (int i = 0; i < (int)upperButtonRects_.size(); ++i) {
        upperButtonRects_[i] = GetElementRect(upperButtons_[i]);
    }
    for (int i = 0; i < (int)minimapButtonRects_.size(); ++i) {
        minimapButtonRects_[i] = GetElementRect(minimapButtons_[i]);
    }
    for (int i = 0; i < 12; ++i) {
        commandRects_[i] = GetElementRect(commandButtons_[i]);
        multiSelectRects_[i] = GetElementRect(multiSelectSlots_[i]);
    }
    for (int i = 0; i < 5; ++i) {
        queueRects_[i] = GetElementRect(queueSlots_[i]);
    }
    for (int i = 0; i < 6; ++i) {
        inventoryRects_[i] = GetElementRect(inventorySlots_[i]);
    }
    for (int i = 0; i < kGameMenuActionCount; ++i) {
        gameMenuButtonRects_[i] = GetElementRect(gameMenuButtons_[i]);
    }
    if (glueMenuControl_) {
        glueMenuControl_->CacheRects([this](Aero::FrameworkElement* element) {
            return GetElementRect(element);
        });
    }

    if (!layoutLogged_ && root_) {
        layoutLogged_ = true;
        HudDiagLog(
            "AeroHudHost layout: physical=%dx%d logical=%dx%d scale=%.2f root=%.1fx%.1f minimap=%.1f,%.1f %.1fx%.1f",
            physicalWidth_, physicalHeight_, width_, height_, dpiScale_,
            (float)root_->GetActualWidth(), (float)root_->GetActualHeight(),
            minimapRect_.x, minimapRect_.y, minimapRect_.w, minimapRect_.h);
    }
}

ScreenRect NoesisHudHost::GetElementRect(Aero::FrameworkElement* element) const {
    if (!element) return {};
    Aero::Point p0 = element->PointToScreen(Aero::Point(0.0f, 0.0f));
    Aero::Point p1 = element->PointToScreen(Aero::Point(element->GetActualWidth(), element->GetActualHeight()));
    const float scale = dpiScale_ > 0.0f ? dpiScale_ : 1.0f;
    if (!std::isfinite(p0.x) || !std::isfinite(p0.y) || !std::isfinite(p1.x) || !std::isfinite(p1.y)) {
        return {};
    }
    const float left = std::min(p0.x, p1.x);
    const float top = std::min(p0.y, p1.y);
    const float w = std::abs(p1.x - p0.x);
    const float h = std::abs(p1.y - p0.y);
    return ScreenRect{
        left * scale,
        top * scale,
        w * scale,
        h * scale
    };
}

void NoesisHudHost::ApplyLayoutMetrics() {
    if (width_ <= 0 || height_ <= 0) return;

    const float consoleHeight = JavaFdfToY(kHudLayoutHeight, 0.176f);
    const float inventoryIcon = FdfSquareToPixels(kHudLayoutWidth, kHudLayoutHeight, 0.03125f);
    const float commandIcon = FdfSquareToPixels(kHudLayoutWidth, kHudLayoutHeight, 0.039f);
    const float inventoryWidth = JavaFdfToX(kHudLayoutWidth, 0.079f);
    const float inventoryHeight = JavaFdfToY(kHudLayoutHeight, 0.115f);
    const float inventoryStepX = JavaFdfToX(kHudLayoutWidth, 0.04f);
    const float inventoryStepY = JavaFdfToY(kHudLayoutHeight, 0.03815f);
    const float portraitWidth = JavaFdfToX(kHudLayoutWidth, 0.0835f);
    const float portraitHeight = JavaFdfToY(kHudLayoutHeight, 0.114f);
    const float portraitModelHeight = JavaFdfToY(kHudLayoutHeight, 0.085f);
    const float commandStepX = JavaFdfToX(kHudLayoutWidth, 0.0434f);
    const float commandStepY = JavaFdfToY(kHudLayoutHeight, 0.044f);
    const float commandGridWidth = commandStepX * 4.0f;
    const float commandGridHeight = commandStepY * 3.0f;
    if (consolePanel_) consolePanel_->SetHeight(consoleHeight);
    if (portraitViewport_) {
        portraitViewport_->SetWidth(portraitWidth);
        portraitViewport_->SetHeight(portraitHeight);
    }
    if (portraitIcon_) {
        portraitIcon_->SetWidth(portraitWidth);
        portraitIcon_->SetHeight(portraitModelHeight);
    }
    if (inventoryPanel_) {
        inventoryPanel_->SetWidth(inventoryWidth);
        inventoryPanel_->SetHeight(inventoryHeight);
    }
    if (inventoryGrid_) {
        inventoryGrid_->SetWidth(std::max(1.0f, inventoryStepX * 2.0f));
        inventoryGrid_->SetHeight(std::max(1.0f, inventoryStepY * 3.0f));
    }
    if (commandCardGrid_) {
        commandCardGrid_->SetWidth(commandGridWidth);
        commandCardGrid_->SetHeight(commandGridHeight);
    }
    if (commandCardPanel_) {
        commandCardPanel_->SetWidth(commandGridWidth);
        commandCardPanel_->SetHeight(commandGridHeight);
    }
}

void NoesisHudHost::ApplyState() {
    ApplyLayoutMetrics();

    if (goldText_) goldText_->SetText(std::to_string(gold_).c_str());
    if (lumberText_) lumberText_->SetText(std::to_string(lumber_).c_str());
    if (foodText_) {
        foodText_->SetText((std::to_string(foodUsed_) + " / " + std::to_string(foodCap_)).c_str());
    }
    if (upkeepText_) upkeepText_->SetText(upkeepValue_.c_str());
    if (timeText_) timeText_->SetText(timeValue_.c_str());

    if (unitNameText_) unitNameText_->SetText(unitName_.c_str());
    if (unitLevelText_) {
        unitLevelText_->SetText(unitLevel_ > 0 ? ("Lv. " + std::to_string(unitLevel_)).c_str() : "");
    }

    int hpValue = (int)std::max(0.0f, hp_);
    int hpMaxValue = (int)std::max(0.0f, maxHp_);
    int mpValue = (int)std::max(0.0f, mp_);
    int mpMaxValue = (int)std::max(0.0f, maxMp_);
    float hpRatio = maxHp_ > 0.0f ? std::clamp(hp_ / maxHp_, 0.0f, 1.0f) : 0.0f;
    float mpRatio = maxMp_ > 0.0f ? std::clamp(mp_ / maxMp_, 0.0f, 1.0f) : 0.0f;
    float xpRatio = heroMaxXp_ > 0 ? std::clamp((float)heroXp_ / (float)heroMaxXp_, 0.0f, 1.0f) : 0.0f;

    if (hpText_) hpText_->SetText((std::to_string(hpValue) + " / " + std::to_string(hpMaxValue)).c_str());
    if (mpText_) mpText_->SetText((std::to_string(mpValue) + " / " + std::to_string(mpMaxValue)).c_str());
    if (hpFill_) {
        hpFill_->SetWidth(194.0f * hpRatio);
        hpFill_->SetBackground(MakeBrushFromArgb(HealthBarTintArgb(hpRatio)).GetPtr());
    }
    if (mpFill_) {
        mpFill_->SetWidth(194.0f * mpRatio);
        mpFill_->SetBackground(MakeBrushFromArgb(0xFF0000FF).GetPtr());
    }
    if (xpFill_) xpFill_->SetWidth(194.0f * xpRatio);

    auto statText = [](int base, int bonus) {
        std::string text = std::to_string(base);
        if (bonus > 0) {
            text += "+" + std::to_string(bonus);
        } else if (bonus < 0) {
            text += std::to_string(bonus);
        }
        return text;
    };
    if (heroStatsPanel_) {
        heroStatsPanel_->SetVisibility(heroVisible_ ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
    }
    if (heroStrengthText_) {
        heroStrengthText_->SetText(heroVisible_ ? statText(heroStrBase_, heroStrBonus_).c_str() : "");
    }
    if (heroAgilityText_) {
        heroAgilityText_->SetText(heroVisible_ ? statText(heroAgiBase_, heroAgiBonus_).c_str() : "");
    }
    if (heroIntelligenceText_) {
        heroIntelligenceText_->SetText(heroVisible_ ? statText(heroIntBase_, heroIntBonus_).c_str() : "");
    }
    if (heroStatsText_) {
        heroStatsText_->SetText("");
        heroStatsText_->SetVisibility(Aero::Visibility_Collapsed);
    }
    if (infoStatsPanel_) {
        infoStatsPanel_->SetVisibility(infoStats_.visible ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
    }
    const char* attackIconKeys[] = {
        "DamageUnknown", "DamageNormal", "DamageMelee", "DamagePierce",
        "DamageSiege", "DamageMagic", "DamageChaos", "DamageHero",
        "DamageUnknownNeutral", "DamageNormalNeutral", "DamageMeleeNeutral", "DamagePierceNeutral",
        "DamageSiegeNeutral", "DamageMagicNeutral", "DamageChaosNeutral", "DamageHeroNeutral",
    };
    for (int i = 0; i < (int)infoAttackIcons_.size(); ++i) {
        if (infoAttackIcons_[i]) {
            const bool show = infoStats_.visible && infoStats_.attackIconKey == attackIconKeys[i];
            infoAttackIcons_[i]->SetVisibility(show ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
        }
    }
    const char* armorIconKeys[] = {
        "ArmorUnknown", "ArmorSmall", "ArmorMedium", "ArmorLarge", "ArmorFort",
        "ArmorNormal", "ArmorHero", "ArmorDivine", "ArmorNone",
        "ArmorUnknownNeutral", "ArmorSmallNeutral", "ArmorMediumNeutral", "ArmorLargeNeutral", "ArmorFortNeutral",
        "ArmorNormalNeutral", "ArmorHeroNeutral", "ArmorDivineNeutral", "ArmorNoneNeutral",
    };
    for (int i = 0; i < (int)infoArmorIcons_.size(); ++i) {
        if (infoArmorIcons_[i]) {
            const bool show = infoStats_.visible && infoStats_.armorIconKey == armorIconKeys[i];
            infoArmorIcons_[i]->SetVisibility(show ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
        }
    }
    if (infoAttackText_) {
        infoAttackText_->SetText(infoStats_.visible ? infoStats_.attackText.c_str() : "");
    }
    if (infoArmorText_) {
        infoArmorText_->SetText(infoStats_.visible ? infoStats_.armorText.c_str() : "");
    }
    if (infoGoldText_) {
        infoGoldText_->SetText(infoStats_.visible ? infoStats_.goldText.c_str() : "");
    }
    if (infoFoodText_) {
        infoFoodText_->SetText(infoStats_.visible ? infoStats_.foodText.c_str() : "");
    }
    if (buffStatusBar_) {
        buffStatusBar_->SetVisibility(buffCount_ > 0 ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
    }
    if (buffStatusBarLabel_) {
        buffStatusBarLabel_->SetText(buffCount_ > 0 ? "STATUS" : "");
    }
    for (int i = 0; i < (int)buffSlots_.size(); ++i) {
        const HudBuffItem& buff = buffStates_[i];
        const bool visible = i < buffCount_ && buff.visible;
        if (buffSlots_[i]) {
            buffSlots_[i]->SetVisibility(visible ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
            if (visible) {
                buffSlots_[i]->SetBackground(MakeBrushFromArgb(buff.positive ? 0xCC22372A : 0xCC3B2020).GetPtr());
            }
        }
        if (buffIcons_[i]) {
            const std::string iconUri = visible ? buff.iconUri : std::string();
            if (buffAppliedIconUris_[i] != iconUri) {
                buffAppliedIconUris_[i] = iconUri;
                if (iconUri.empty()) {
                    buffIcons_[i]->SetSource(nullptr);
                } else {
                    buffIcons_[i]->SetSource(Aero::MakePtr<Aero::BitmapImage>(Aero::Uri(iconUri.c_str())));
                }
            }
            buffIcons_[i]->SetVisibility(visible && !buff.iconUri.empty()
                ? Aero::Visibility_Visible
                : Aero::Visibility_Collapsed);
        }
        if (buffLabels_[i]) {
            const bool showLabel = visible && buff.iconUri.empty();
            std::string label = buff.name;
            if (label.size() > 4) label.resize(4);
            buffLabels_[i]->SetText(showLabel ? label.c_str() : "");
            buffLabels_[i]->SetVisibility(showLabel ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
        }
        if (buffDurations_[i]) {
            const bool showDuration = visible && buff.duration > 0.0f && buff.remaining > 0.0f;
            const int seconds = (int)std::ceil(buff.remaining);
            buffDurations_[i]->SetText(showDuration ? std::to_string(seconds).c_str() : "");
        }
        const bool showStack = visible && buff.stackCount > 1;
        if (buffStackBacks_[i]) {
            buffStackBacks_[i]->SetVisibility(showStack ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
        }
        if (buffStacks_[i]) {
            buffStacks_[i]->SetText(showStack ? std::to_string(buff.stackCount).c_str() : "");
        }
    }

    if (portraitImage_) {
        if (portraitAppliedIconUri_ != portraitIconUri_) {
            portraitAppliedIconUri_ = portraitIconUri_;
            if (portraitIconUri_.empty()) {
                portraitImage_->SetSource(nullptr);
            } else {
                portraitImage_->SetSource(Aero::MakePtr<Aero::BitmapImage>(Aero::Uri(portraitIconUri_.c_str())));
            }
        }
        portraitImage_->SetVisibility(portraitIconUri_.empty()
            ? Aero::Visibility_Collapsed
            : Aero::Visibility_Visible);
    }

    if (tooltipText_) tooltipText_->SetText(tooltipValue_.c_str());
    const std::array<int, 5> tooltipCosts = TooltipCostValues(tooltipCosts_);
    bool anyTooltipCost = false;
    for (size_t i = 0; i < tooltipCosts.size(); ++i) {
        const bool showCost = tooltipVisible_ && tooltipCosts[i] > 0;
        anyTooltipCost = anyTooltipCost || showCost;
        if (tooltipCostTexts_[i]) {
            tooltipCostTexts_[i]->SetText(showCost ? std::to_string(tooltipCosts[i]).c_str() : "");
        }
        if (tooltipCostGroups_[i]) {
            tooltipCostGroups_[i]->SetVisibility(showCost ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
        }
    }
    if (tooltipResourceRow_) {
        tooltipResourceRow_->SetVisibility(anyTooltipCost ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
    }
    if (tooltipPanel_) {
        tooltipPanel_->SetVisibility(tooltipVisible_ ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
        const float xScale = width_ > 0 ? (float)width_ / (float)kHudLayoutWidth : 1.0f;
        const float yScale = height_ > 0 ? (float)height_ / (float)kHudLayoutHeight : 1.0f;
        const float vx = xScale > 0.0f ? tooltipX_ / xScale : tooltipX_;
        const float vy = yScale > 0.0f ? tooltipY_ / yScale : tooltipY_;
        float tx = std::clamp(vx + 12.0f, 12.0f, (float)kHudLayoutWidth - 280.0f);
        float ty = std::clamp(vy - 86.0f, 12.0f, (float)kHudLayoutHeight - 120.0f);
        Aero::Canvas::SetLeft(tooltipPanel_, tx);
        Aero::Canvas::SetTop(tooltipPanel_, ty);
    }

    if (chatPanel_) {
        chatPanel_->SetVisibility(chatOpen_ ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
    }
    if (chatLogText_) {
        chatLogText_->SetText(chatJoinedText_.c_str());
    }
    if (chatInput_) {
        if (std::string(chatInput_->GetText()) != chatInputText_) {
            chatInput_->SetText(chatInputText_.c_str());
        }
        if (chatOpen_) chatInput_->Focus();
    }

    if (glueMenuControl_) {
        glueMenuControl_->ApplyState();
    }

    if (gameMenuRoot_) {
        gameMenuRoot_->SetVisibility(gameMenuOpen_ ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
    }
    if (gameMenuBackdrop_) {
        float backdropW = 553.0f;
        float backdropH = 737.0f;
        switch (gameMenuPanel_) {
        case GameMenuPanel::ConfirmQuit:
        case GameMenuPanel::SaveOverwriteConfirm:
        case GameMenuPanel::SaveDeleteConfirm:
            backdropW = 645.0f;
            backdropH = 369.0f;
            break;
        case GameMenuPanel::SaveGame:
        case GameMenuPanel::LoadGame:
            backdropW = 829.0f;
            backdropH = 553.0f;
            break;
        case GameMenuPanel::Help:
            backdropW = 829.0f;
            backdropH = 737.0f;
            break;
        case GameMenuPanel::Tips:
            backdropW = 829.0f;
            backdropH = 545.0f;
            break;
        default:
            break;
        }
        gameMenuBackdrop_->SetWidth(backdropW);
        gameMenuBackdrop_->SetHeight(backdropH);
        gameMenuBackdrop_->SetVisibility(gameMenuOpen_ ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
    }
    if (gameMenuMainPanel_) {
        gameMenuMainPanel_->SetVisibility(gameMenuOpen_ && gameMenuPanel_ == GameMenuPanel::Main
            ? Aero::Visibility_Visible
            : Aero::Visibility_Collapsed);
    }
    if (gameMenuPauseText_) {
        gameMenuPauseText_->SetText(gameMenuPaused_ ? "RESUME GAME" : "PAUSE GAME");
    }
    if (gameMenuEndGamePanel_) {
        gameMenuEndGamePanel_->SetVisibility(gameMenuOpen_ && gameMenuPanel_ == GameMenuPanel::EndGame
            ? Aero::Visibility_Visible
            : Aero::Visibility_Collapsed);
    }
    if (gameMenuConfirmQuitPanel_) {
        gameMenuConfirmQuitPanel_->SetVisibility(gameMenuOpen_ && gameMenuPanel_ == GameMenuPanel::ConfirmQuit
            ? Aero::Visibility_Visible
            : Aero::Visibility_Collapsed);
    }
    if (gameMenuSaveGamePanel_) {
        gameMenuSaveGamePanel_->SetVisibility(gameMenuOpen_ && gameMenuPanel_ == GameMenuPanel::SaveGame
            ? Aero::Visibility_Visible
            : Aero::Visibility_Collapsed);
    }
    if (gameMenuLoadGamePanel_) {
        gameMenuLoadGamePanel_->SetVisibility(gameMenuOpen_ && gameMenuPanel_ == GameMenuPanel::LoadGame
            ? Aero::Visibility_Visible
            : Aero::Visibility_Collapsed);
    }
    if (gameMenuOverwriteConfirmPanel_) {
        gameMenuOverwriteConfirmPanel_->SetVisibility(gameMenuOpen_ && gameMenuPanel_ == GameMenuPanel::SaveOverwriteConfirm
            ? Aero::Visibility_Visible
            : Aero::Visibility_Collapsed);
    }
    if (gameMenuDeleteConfirmPanel_) {
        gameMenuDeleteConfirmPanel_->SetVisibility(gameMenuOpen_ && gameMenuPanel_ == GameMenuPanel::SaveDeleteConfirm
            ? Aero::Visibility_Visible
            : Aero::Visibility_Collapsed);
    }
    if (gameMenuHelpPanel_) {
        gameMenuHelpPanel_->SetVisibility(gameMenuOpen_ && gameMenuPanel_ == GameMenuPanel::Help
            ? Aero::Visibility_Visible
            : Aero::Visibility_Collapsed);
    }
    if (gameMenuTipsPanel_) {
        gameMenuTipsPanel_->SetVisibility(gameMenuOpen_ && gameMenuPanel_ == GameMenuPanel::Tips
            ? Aero::Visibility_Visible
            : Aero::Visibility_Collapsed);
    }
    const bool optionsPanelOpen = gameMenuOpen_ && (
        gameMenuPanel_ == GameMenuPanel::Options ||
        gameMenuPanel_ == GameMenuPanel::OptionsGameplay ||
        gameMenuPanel_ == GameMenuPanel::OptionsVideo ||
        gameMenuPanel_ == GameMenuPanel::OptionsSound ||
        gameMenuPanel_ == GameMenuPanel::OptionsNetwork);
    if (gameMenuOptionsPanel_) {
        gameMenuOptionsPanel_->SetVisibility(optionsPanelOpen
            ? Aero::Visibility_Visible
            : Aero::Visibility_Collapsed);
    }
    if (gameMenuOptionsListPanel_) {
        gameMenuOptionsListPanel_->SetVisibility(gameMenuOpen_ && gameMenuPanel_ == GameMenuPanel::Options
            ? Aero::Visibility_Visible
            : Aero::Visibility_Collapsed);
    }
    if (gameMenuGameplayPanel_) {
        gameMenuGameplayPanel_->SetVisibility(gameMenuOpen_ && gameMenuPanel_ == GameMenuPanel::OptionsGameplay
            ? Aero::Visibility_Visible
            : Aero::Visibility_Collapsed);
    }
    if (gameMenuVideoPanel_) {
        gameMenuVideoPanel_->SetVisibility(gameMenuOpen_ && gameMenuPanel_ == GameMenuPanel::OptionsVideo
            ? Aero::Visibility_Visible
            : Aero::Visibility_Collapsed);
    }
    if (gameMenuSoundPanel_) {
        gameMenuSoundPanel_->SetVisibility(gameMenuOpen_ && gameMenuPanel_ == GameMenuPanel::OptionsSound
            ? Aero::Visibility_Visible
            : Aero::Visibility_Collapsed);
    }
    if (gameMenuNetworkPanel_) {
        gameMenuNetworkPanel_->SetVisibility(gameMenuOpen_ && gameMenuPanel_ == GameMenuPanel::OptionsNetwork
            ? Aero::Visibility_Visible
            : Aero::Visibility_Collapsed);
    }
    if (gameMenuOptionsBottomPanel_) {
        const bool showOptionsBottom = gameMenuOpen_ && (
            gameMenuPanel_ == GameMenuPanel::OptionsGameplay ||
            gameMenuPanel_ == GameMenuPanel::OptionsVideo ||
            gameMenuPanel_ == GameMenuPanel::OptionsSound ||
            gameMenuPanel_ == GameMenuPanel::OptionsNetwork);
        gameMenuOptionsBottomPanel_->SetVisibility(showOptionsBottom
            ? Aero::Visibility_Visible
            : Aero::Visibility_Collapsed);
    }
    for (int i = 0; i < kGameMenuActionCount; ++i) {
        const bool enabled = IsGameMenuActionEnabled(i);
        if (gameMenuButtonHighlights_[i]) {
            gameMenuButtonHighlights_[i]->SetVisibility(
                gameMenuOpen_ && enabled && hoveredGameMenuAction_ == i && pressedGameMenuAction_ != i
                    ? Aero::Visibility_Visible
                    : Aero::Visibility_Collapsed);
        }
        if (gameMenuButtonPressed_[i]) {
            gameMenuButtonPressed_[i]->SetVisibility(
                gameMenuOpen_ && enabled && pressedGameMenuAction_ == i
                    ? Aero::Visibility_Visible
                    : Aero::Visibility_Collapsed);
        }

        if (i <= kGameMenuTipsOK) continue;

        const bool checked = gameMenuOptionCheckedState_[i];
        const bool isSlider = IsGameMenuOptionSliderAction(i);
        const bool showOption = optionsPanelOpen;
        if (gameMenuOptionDisabled_[i]) {
            gameMenuOptionDisabled_[i]->SetVisibility(showOption && !enabled
                ? Aero::Visibility_Visible
                : Aero::Visibility_Collapsed);
        }
        if (gameMenuOptionChecked_[i]) {
            gameMenuOptionChecked_[i]->SetVisibility(showOption && enabled && checked
                ? Aero::Visibility_Visible
                : Aero::Visibility_Collapsed);
        }
        if (gameMenuOptionDisabledChecked_[i]) {
            const bool showDisabledLayer = isSlider ? !enabled : (!enabled && checked);
            gameMenuOptionDisabledChecked_[i]->SetVisibility(showOption && showDisabledLayer
                ? Aero::Visibility_Visible
                : Aero::Visibility_Collapsed);
        }
        if (gameMenuOptionSliderThumbs_[i]) {
            gameMenuOptionSliderThumbs_[i]->SetVisibility(showOption && enabled
                ? Aero::Visibility_Visible
                : Aero::Visibility_Collapsed);
        }
        if (gameMenuOptionPopupMenus_[i]) {
            gameMenuOptionPopupMenus_[i]->SetVisibility(showOption && gameMenuOptionPopupOpenAction_ == i
                ? Aero::Visibility_Visible
                : Aero::Visibility_Collapsed);
        }
        if (gameMenuOptionValueTexts_[i]) {
            gameMenuOptionValueTexts_[i]->SetText(GameMenuOptionCycleText(i, gameMenuOptionCycleIndex_[i]));
        }
    }

    for (int i = 0; i < (int)upperButtons_.size(); ++i) {
        const bool enabled = IsUpperButtonEnabled(i);
        const bool pressed = enabled && pressedUpperButtonIndex_ == i;
        const bool hovered = enabled && hoveredUpperButtonIndex_ == i;
        if (upperButtonPushed_[i]) {
            upperButtonPushed_[i]->SetVisibility(pressed ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
        }
        if (upperButtonHighlights_[i]) {
            upperButtonHighlights_[i]->SetVisibility(hovered && !pressed ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
        }
        if (upperButtonLabels_[i]) {
            upperButtonLabels_[i]->SetOpacity(enabled ? 1.0f : 0.45f);
        }
    }

    for (int i = 0; i < (int)minimapButtons_.size(); ++i) {
        const bool pressed = pressedMinimapButtonIndex_ == i;
        const int mode = minimapButtonModes_[i];
        const bool inactive = mode == 1 && i != 4;
        const bool off = (i == 4 && mode == 1) || mode == 2;
        if (minimapButtonPushed_[i]) {
            minimapButtonPushed_[i]->SetVisibility(pressed && !inactive && !off
                ? Aero::Visibility_Visible
                : Aero::Visibility_Collapsed);
        }
        if (minimapButtonDisabled_[i]) {
            minimapButtonDisabled_[i]->SetVisibility(Aero::Visibility_Collapsed);
        }
        if (minimapButtonInactive_[i]) {
            minimapButtonInactive_[i]->SetVisibility(inactive ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
        }
        if (minimapButtonInactivePushed_[i]) {
            minimapButtonInactivePushed_[i]->SetVisibility(pressed && inactive
                ? Aero::Visibility_Visible
                : Aero::Visibility_Collapsed);
        }
        if (minimapButtonInactiveDisabled_[i]) {
            minimapButtonInactiveDisabled_[i]->SetVisibility(Aero::Visibility_Collapsed);
        }
        if (minimapButtonOff_[i]) {
            minimapButtonOff_[i]->SetVisibility(off ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
        }
        if (minimapButtonOffPushed_[i]) {
            minimapButtonOffPushed_[i]->SetVisibility(pressed && off
                ? Aero::Visibility_Visible
                : Aero::Visibility_Collapsed);
        }
        if (minimapButtonOffDisabled_[i]) {
            minimapButtonOffDisabled_[i]->SetVisibility(Aero::Visibility_Collapsed);
        }
    }

    for (int i = 0; i < 12; ++i) {
        const bool commandVisible = commandStates_[i].visible;
        if (commandLabels_[i]) {
            commandLabels_[i]->SetText(commandStates_[i].label.c_str());
            commandLabels_[i]->SetVisibility(commandVisible && !commandStates_[i].label.empty()
                ? Aero::Visibility_Visible
                : Aero::Visibility_Collapsed);
        }
        if (commandIcons_[i]) {
            const std::string& iconUri = commandStates_[i].iconUri;
            if (commandAppliedIconUris_[i] != iconUri) {
                commandAppliedIconUris_[i] = iconUri;
                if (iconUri.empty()) {
                    commandIcons_[i]->SetSource(nullptr);
                } else {
                    commandIcons_[i]->SetSource(Aero::MakePtr<Aero::BitmapImage>(Aero::Uri(iconUri.c_str())));
                }
            }
            commandIcons_[i]->SetOpacity(commandStates_[i].enabled ? 1.0f : 0.42f);
            commandIcons_[i]->SetVisibility(commandVisible && !iconUri.empty()
                ? Aero::Visibility_Visible
                : Aero::Visibility_Collapsed);
        }
        if (commandButtons_[i]) {
            commandButtons_[i]->SetVisibility(commandVisible
                ? Aero::Visibility_Visible
                : Aero::Visibility_Collapsed);
            commandButtons_[i]->SetOpacity(1.0f);
        }
        if (commandDisabledBacks_[i]) {
            const bool showDisabled = commandVisible && !commandStates_[i].enabled;
            commandDisabledBacks_[i]->SetVisibility(showDisabled ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
        }
        if (commandHoverHighlights_[i]) {
            const bool showHover = commandVisible && commandStates_[i].enabled && hoveredCommandIndex_ == i;
            commandHoverHighlights_[i]->SetVisibility(showHover ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
        }
        if (commandPressedBorders_[i]) {
            const bool showPressed = commandVisible && commandStates_[i].enabled && pressedCommandIndex_ == i;
            commandPressedBorders_[i]->SetVisibility(showPressed ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
        }
        if (commandActiveHighlights_[i]) {
            commandActiveHighlights_[i]->SetVisibility(commandVisible && commandStates_[i].active
                ? Aero::Visibility_Visible
                : Aero::Visibility_Collapsed);
        }
        const bool showCooldown = commandVisible
            && commandStates_[i].cooldownRemaining > 0.0f
            && commandStates_[i].cooldownMax > 0.0f;
        const float cooldownRatio = showCooldown
            ? std::min(1.0f, commandStates_[i].cooldownRemaining / commandStates_[i].cooldownMax)
            : 0.0f;
        if (commandCooldownOverlays_[i]) {
            float buttonHeight = commandButtons_[i] ? commandButtons_[i]->GetActualHeight() : 0.0f;
            if (buttonHeight <= 0.0f) {
                buttonHeight = 50.0f;
            }
            commandCooldownOverlays_[i]->SetHeight(buttonHeight * cooldownRatio);
            commandCooldownOverlays_[i]->SetVisibility(showCooldown ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
        }
        if (commandCooldownLabels_[i]) {
            if (showCooldown) {
                const int cooldownSeconds = std::max(1, (int)std::ceil(commandStates_[i].cooldownRemaining));
                commandCooldownLabels_[i]->SetText(std::to_string(cooldownSeconds).c_str());
            } else {
                commandCooldownLabels_[i]->SetText("");
            }
            commandCooldownLabels_[i]->SetVisibility(showCooldown ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
        }
        const bool showNumber = commandVisible && commandStates_[i].numberOverlay >= 0;
        if (commandNumberLabels_[i]) {
            commandNumberLabels_[i]->SetText(showNumber ? std::to_string(commandStates_[i].numberOverlay).c_str() : "");
        }
        if (commandNumberBacks_[i]) {
            commandNumberBacks_[i]->SetVisibility(showNumber ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
        }
        if (commandAutocastOverlays_[i]) {
            const bool showAutocast = commandVisible && commandStates_[i].autocastOrderId != 0;
            commandAutocastOverlays_[i]->SetVisibility(showAutocast ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
            commandAutocastOverlays_[i]->SetOpacity(commandStates_[i].autocastActive ? 1.0f : 0.55f);
        }
    }

    for (int i = 0; i < 12; ++i) {
        const bool visible = multiSelectVisible_[i];
        const MultiSelectUnitInfo& unit = multiSelectStates_[i];
        const bool focused = visible && unit.handleId == multiSelectPrimaryHandle_;
        if (multiSelectSlots_[i]) {
            multiSelectSlots_[i]->SetVisibility(visible ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
            multiSelectSlots_[i]->SetOpacity(focused ? 1.0f : 0.72f);
        }
        if (multiSelectFocusHighlights_[i]) {
            multiSelectFocusHighlights_[i]->SetVisibility(focused ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
        }
        if (multiSelectHeroGlows_[i]) {
            multiSelectHeroGlows_[i]->SetVisibility(visible && unit.isHero ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
            multiSelectHeroGlows_[i]->SetOpacity(focused ? 1.0f : 0.68f);
        }
        if (!visible) continue;

        if (multiSelectColors_[i]) {
            auto brush = MakeBrushFromArgb(unit.playerColor);
            multiSelectColors_[i]->SetBackground(brush.GetPtr());
        }
        const float hpRatio = std::clamp(unit.hpRatio, 0.0f, 1.0f);
        const float mpRatio = std::clamp(unit.mpRatio, 0.0f, 1.0f);
        if (multiSelectHpFills_[i]) {
            multiSelectHpFills_[i]->SetWidth(30.0f * hpRatio);
            multiSelectHpFills_[i]->SetBackground(MakeBrushFromArgb(HealthBarTintArgb(hpRatio)).GetPtr());
        }
        if (multiSelectMpFills_[i]) {
            multiSelectMpFills_[i]->SetWidth(30.0f * mpRatio);
            multiSelectMpFills_[i]->SetBackground(MakeBrushFromArgb(0xFF0000FF).GetPtr());
        }
        if (multiSelectLevelLabels_[i]) {
            multiSelectLevelLabels_[i]->SetText(unit.isHero ? std::to_string(std::max(1, unit.level)).c_str() : "");
            multiSelectLevelLabels_[i]->SetVisibility(unit.isHero ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
        }
    }

    for (int i = 0; i < 5; ++i) {
        const HudQueueItem& item = queueStates_[i];
        if (queueSlots_[i]) {
            queueSlots_[i]->SetVisibility(item.visible ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
            queueSlots_[i]->SetOpacity(i == 0 ? 1.0f : 0.78f);
        }
        if (queueIcons_[i]) {
            const std::string& iconUri = item.iconUri;
            if (queueAppliedIconUris_[i] != iconUri) {
                queueAppliedIconUris_[i] = iconUri;
                if (iconUri.empty()) {
                    queueIcons_[i]->SetSource(nullptr);
                } else {
                    queueIcons_[i]->SetSource(Aero::MakePtr<Aero::BitmapImage>(Aero::Uri(iconUri.c_str())));
                }
            }
            queueIcons_[i]->SetVisibility(item.visible && !item.iconUri.empty()
                ? Aero::Visibility_Visible
                : Aero::Visibility_Collapsed);
        }
        if (queueLabels_[i]) {
            const bool showLabel = item.visible && item.iconUri.empty();
            queueLabels_[i]->SetText(showLabel ? item.label.c_str() : "");
            queueLabels_[i]->SetVisibility(showLabel ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
        }
    }
    if (queueBackdrop_) {
        bool showQueueBackdrop = false;
        for (const HudQueueItem& item : queueStates_) {
            if (item.visible) {
                showQueueBackdrop = true;
                break;
            }
        }
        queueBackdrop_->SetVisibility(showQueueBackdrop ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
    }
    if (queueProgressFill_) {
        const float trackWidth = queueProgressTrack_ ? (float)queueProgressTrack_->GetActualWidth() : 116.0f;
        queueProgressFill_->SetWidth(std::max(0.0f, trackWidth) * queueProgressRatio_);
    }

    if (inventoryCover_) {
        inventoryCover_->SetVisibility(inventoryAvailable_ ? Aero::Visibility_Collapsed : Aero::Visibility_Visible);
    }
    if (inventoryPanel_) {
        inventoryPanel_->SetVisibility(inventoryAvailable_ ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
    }

    for (int i = 0; i < 6; ++i) {
        const HudInventoryItem& item = inventoryStates_[i];
        const bool showSlot = inventoryAvailable_ && item.visible && (item.occupied || !item.enabled);
        if (inventorySlots_[i]) {
            const double opacity = item.enabled ? 1.0 : 0.58;
            inventorySlots_[i]->SetOpacity(1.0);
            inventorySlots_[i]->SetVisibility(showSlot ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
            if (inventoryDisabledBacks_[i]) {
                inventoryDisabledBacks_[i]->SetOpacity(opacity);
            }
        }
        if (inventoryDisabledBacks_[i]) {
            const bool disabled = showSlot && !item.enabled;
            inventoryDisabledBacks_[i]->SetVisibility(disabled ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
        }
        if (inventoryLabels_[i]) {
            const bool showLabel = showSlot && item.occupied && item.iconUri.empty() && !item.label.empty();
            inventoryLabels_[i]->SetText(showLabel ? item.label.c_str() : "");
            inventoryLabels_[i]->SetVisibility(showLabel ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
        }
        if (inventoryIcons_[i]) {
            const std::string& iconUri = item.iconUri;
            if (inventoryAppliedIconUris_[i] != iconUri) {
                inventoryAppliedIconUris_[i] = iconUri;
                if (iconUri.empty()) {
                    inventoryIcons_[i]->SetSource(nullptr);
                } else {
                    inventoryIcons_[i]->SetSource(Aero::MakePtr<Aero::BitmapImage>(Aero::Uri(iconUri.c_str())));
                }
            }
            inventoryIcons_[i]->SetVisibility(showSlot && item.occupied && !iconUri.empty()
                ? Aero::Visibility_Visible
                : Aero::Visibility_Collapsed);
        }
        const bool showCharges = showSlot && item.occupied && item.charges > 1;
        if (inventoryChargeLabels_[i]) {
            inventoryChargeLabels_[i]->SetText(showCharges ? std::to_string(item.charges).c_str() : "");
        }
        if (inventoryChargeBacks_[i]) {
            inventoryChargeBacks_[i]->SetVisibility(showCharges ? Aero::Visibility_Visible : Aero::Visibility_Collapsed);
        }
    }
}
