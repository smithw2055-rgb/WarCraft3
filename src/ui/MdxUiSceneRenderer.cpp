#include "MdxUiSceneRenderer.h"

#include "third_party/sokol/sokol_gfx.h"
#include "render/mdx_sequence_select.h"

#include <NsRender/GLFactory.h>
#include <NsRender/Texture.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace War3UI {
namespace {

MpqManager* g_mdxUiMpq = nullptr;
int g_mdxUiGameVersion = 0;

std::string Trim(std::string value) {
    auto isSpace = [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](unsigned char c) {
        return !isSpace(c);
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](unsigned char c) {
        return !isSpace(c);
    }).base(), value.end());
    return value;
}

std::string StripToken(std::string value) {
    value = Trim(value);
    if (value.size() >= 2) {
        const char first = value.front();
        const char last = value.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            value = value.substr(1, value.size() - 2);
        }
    }
    return Trim(value);
}

std::string StripSkinValue(std::string value) {
    const size_t comment = value.find("//");
    if (comment != std::string::npos) {
        value.resize(comment);
    }
    return StripToken(value);
}

std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    return value;
}

void AddCandidate(std::vector<std::string>& paths, const std::string& path) {
    if (!path.empty() && std::find(paths.begin(), paths.end(), path) == paths.end()) {
        paths.push_back(path);
    }
}

std::string MdxPathFromModelField(std::string path) {
    path = StripSkinValue(path);
    const std::string lower = LowerAscii(path);
    if (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".mdl") {
        path.resize(path.size() - 4);
        path += ".mdx";
    }
    return path;
}

Vec3 MdxPointToYUp(const float value[3]) {
    return Vec3(value[0], value[2], -value[1]);
}

int FindSequenceIndexByName(const MdxModel& model, const char* sequenceName) {
    if (sequenceName && *sequenceName) {
        const std::string requested = LowerAscii(sequenceName);
        for (int i = 0; i < (int)model.sequences.size(); ++i) {
            if (LowerAscii(model.sequences[(size_t)i].name) == requested) {
                return i;
            }
        }
    }
    for (int i = 0; i < (int)model.sequences.size(); ++i) {
        if (LowerAscii(model.sequences[(size_t)i].name).find("stand") != std::string::npos) {
            return i;
        }
    }
    for (int i = 0; i < (int)model.sequences.size(); ++i) {
        const MdxSequence& sequence = model.sequences[(size_t)i];
        if (sequence.endFrame > sequence.startFrame) {
            return i;
        }
    }
    return -1;
}

std::string ResolveSkinField(const std::string& fieldName) {
    if (!g_mdxUiMpq) {
        return {};
    }

    std::vector<uint8_t> bytes;
    if (!g_mdxUiMpq->ReadFile("UI\\war3skins.txt", bytes)) {
        return {};
    }

    const std::string versionedField = fieldName + "_V" + std::to_string(std::max(0, g_mdxUiGameVersion));
    const std::string betaField = versionedField + "Beta";
    std::string betaValue;
    std::string baseValue;

    std::istringstream stream(std::string(bytes.begin(), bytes.end()));
    std::string line;
    while (std::getline(stream, line)) {
        line = Trim(line);
        if (line.empty() || line.rfind("//", 0) == 0 || line.front() == '[') {
            continue;
        }

        const size_t equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }

        const std::string key = Trim(line.substr(0, equals));
        const std::string value = StripSkinValue(line.substr(equals + 1));
        if (key == versionedField) {
            return value;
        }
        if (key == betaField && betaValue.empty()) {
            betaValue = value;
        } else if (key == fieldName && baseValue.empty()) {
            baseValue = value;
        }
    }

    return !betaValue.empty() ? betaValue : baseValue;
}

struct MenuFogSettings {
    bool enabled = false;
    float color[3] = {0.0f, 0.0f, 0.0f};
    float start = 0.0f;
    float end = 1.0f;
};

std::vector<float> ParseFloatList(std::string value) {
    value = StripSkinValue(value);
    for (char& c : value) {
        if (c == ',' || c == ';') {
            c = ' ';
        }
    }

    std::vector<float> values;
    std::istringstream stream(value);
    float number = 0.0f;
    while (stream >> number) {
        values.push_back(number);
    }
    return values;
}

float GetIndexedValue(const std::vector<float>& values, int index, float fallback) {
    if (values.empty()) {
        return fallback;
    }
    if (index < 0) {
        index = 0;
    }
    if (index >= (int)values.size()) {
        index = (int)values.size() - 1;
    }
    return values[(size_t)index];
}

MenuFogSettings LoadMenuFogSettings() {
    static MpqManager* cachedMpq = nullptr;
    static int cachedGameVersion = -1;
    static bool cached = false;
    static MenuFogSettings cachedFog;
    if (cached && cachedMpq == g_mdxUiMpq && cachedGameVersion == g_mdxUiGameVersion) {
        return cachedFog;
    }

    cachedMpq = g_mdxUiMpq;
    cachedGameVersion = g_mdxUiGameVersion;
    cached = true;
    cachedFog = MenuFogSettings{};

    MenuFogSettings fog;
    if (!g_mdxUiMpq) {
        return cachedFog;
    }

    std::vector<uint8_t> bytes;
    if (!g_mdxUiMpq->ReadFile("UI\\MiscData.txt", bytes)) {
        return cachedFog;
    }

    std::vector<float> styleValues;
    std::vector<float> startValues;
    std::vector<float> endValues;
    std::vector<float> colorValues;
    bool inMenuFog = false;

    std::istringstream stream(std::string(bytes.begin(), bytes.end()));
    std::string line;
    while (std::getline(stream, line)) {
        line = Trim(line);
        if (line.empty() || line.rfind("//", 0) == 0) {
            continue;
        }
        if (line.front() == '[') {
            inMenuFog = line == "[MenuZFog]";
            continue;
        }
        if (!inMenuFog) {
            continue;
        }

        const size_t equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }

        const std::string key = Trim(line.substr(0, equals));
        const std::string value = line.substr(equals + 1);
        if (key == "Style") {
            styleValues = ParseFloatList(value);
        } else if (key == "Start") {
            startValues = ParseFloatList(value);
        } else if (key == "End") {
            endValues = ParseFloatList(value);
        } else if (key == "Color") {
            colorValues = ParseFloatList(value);
        }
    }

    const int versionIndex = std::max(0, g_mdxUiGameVersion);
    const int style = (int)GetIndexedValue(styleValues, versionIndex, -1.0f) + 1;
    fog.enabled = style > 0;
    fog.start = GetIndexedValue(startValues, versionIndex, 0.0f);
    fog.end = std::max(fog.start + 0.001f, GetIndexedValue(endValues, versionIndex, fog.start + 1.0f));

    const int colorBase = versionIndex * 4;
    if ((int)colorValues.size() >= colorBase + 4) {
        fog.color[0] = colorValues[(size_t)colorBase + 1] / 255.0f;
        fog.color[1] = colorValues[(size_t)colorBase + 2] / 255.0f;
        fog.color[2] = colorValues[(size_t)colorBase + 3] / 255.0f;
    } else if (colorValues.size() >= 4) {
        fog.color[0] = colorValues[1] / 255.0f;
        fog.color[1] = colorValues[2] / 255.0f;
        fog.color[2] = colorValues[3] / 255.0f;
    }

    cachedFog = fog;
    return cachedFog;
}

#ifdef _WIN32
using GLGenTexturesProc = void (APIENTRY*)(int n, unsigned int* textures);
using GLDeleteTexturesProc = void (APIENTRY*)(int n, const unsigned int* textures);
using GLBindTextureProc = void (APIENTRY*)(unsigned int target, unsigned int texture);
using GLTexParameteriProc = void (APIENTRY*)(unsigned int target, unsigned int pname, int param);
using GLTexImage2DProc = void (APIENTRY*)(unsigned int target, int level, int internalformat,
                                          int width, int height, int border, unsigned int format,
                                          unsigned int type, const void* pixels);
using GLGetErrorProc = unsigned int (APIENTRY*)();

constexpr unsigned int GL_TEXTURE_2D_WAR3 = 0x0DE1;
constexpr unsigned int GL_RGBA_WAR3 = 0x1908;
constexpr unsigned int GL_UNSIGNED_BYTE_WAR3 = 0x1401;
constexpr unsigned int GL_TEXTURE_MIN_FILTER_WAR3 = 0x2801;
constexpr unsigned int GL_TEXTURE_MAG_FILTER_WAR3 = 0x2800;
constexpr unsigned int GL_TEXTURE_WRAP_S_WAR3 = 0x2802;
constexpr unsigned int GL_TEXTURE_WRAP_T_WAR3 = 0x2803;
constexpr int GL_LINEAR_WAR3 = 0x2601;
constexpr int GL_CLAMP_TO_EDGE_WAR3 = 0x812F;
constexpr unsigned int GL_NO_ERROR_WAR3 = 0;

void* LoadGLProc(const char* name) {
    void* proc = reinterpret_cast<void*>(wglGetProcAddress(name));
    if (proc) return proc;

    HMODULE lib = GetModuleHandleA("opengl32.dll");
    return lib ? reinterpret_cast<void*>(GetProcAddress(lib, name)) : nullptr;
}

bool CreateGLTexture(int width, int height, unsigned int& outTexture) {
    static GLGenTexturesProc genTextures =
        reinterpret_cast<GLGenTexturesProc>(LoadGLProc("glGenTextures"));
    static GLBindTextureProc bindTexture =
        reinterpret_cast<GLBindTextureProc>(LoadGLProc("glBindTexture"));
    static GLTexParameteriProc texParameteri =
        reinterpret_cast<GLTexParameteriProc>(LoadGLProc("glTexParameteri"));
    static GLTexImage2DProc texImage2D =
        reinterpret_cast<GLTexImage2DProc>(LoadGLProc("glTexImage2D"));
    static GLGetErrorProc getError =
        reinterpret_cast<GLGetErrorProc>(LoadGLProc("glGetError"));

    if (!genTextures || !bindTexture || !texParameteri || !texImage2D) {
        return false;
    }

    unsigned int texture = 0;
    genTextures(1, &texture);
    if (!texture) {
        return false;
    }

    bindTexture(GL_TEXTURE_2D_WAR3, texture);
    texParameteri(GL_TEXTURE_2D_WAR3, GL_TEXTURE_MIN_FILTER_WAR3, GL_LINEAR_WAR3);
    texParameteri(GL_TEXTURE_2D_WAR3, GL_TEXTURE_MAG_FILTER_WAR3, GL_LINEAR_WAR3);
    texParameteri(GL_TEXTURE_2D_WAR3, GL_TEXTURE_WRAP_S_WAR3, GL_CLAMP_TO_EDGE_WAR3);
    texParameteri(GL_TEXTURE_2D_WAR3, GL_TEXTURE_WRAP_T_WAR3, GL_CLAMP_TO_EDGE_WAR3);
    texImage2D(GL_TEXTURE_2D_WAR3, 0, (int)GL_RGBA_WAR3, width, height, 0,
               GL_RGBA_WAR3, GL_UNSIGNED_BYTE_WAR3, nullptr);
    bindTexture(GL_TEXTURE_2D_WAR3, 0);

    if (getError) {
        unsigned int err = GL_NO_ERROR_WAR3;
        while ((err = getError()) != GL_NO_ERROR_WAR3) {
            std::fprintf(stderr, "MdxUiSceneRenderer: GL texture creation error 0x%x\n", err);
        }
    }

    outTexture = texture;
    return true;
}

void DeleteGLTexture(unsigned int texture) {
    if (!texture) {
        return;
    }
    static GLDeleteTexturesProc deleteTextures =
        reinterpret_cast<GLDeleteTexturesProc>(LoadGLProc("glDeleteTextures"));
    if (deleteTextures) {
        deleteTextures(1, &texture);
    }
}
#else
constexpr unsigned int GL_TEXTURE_2D_WAR3 = 0x0DE1;
bool CreateGLTexture(int, int, unsigned int&) { return false; }
void DeleteGLTexture(unsigned int) {}
#endif

} // namespace

MdxUiSceneRenderer::MdxUiSceneRenderer() = default;

MdxUiSceneRenderer::~MdxUiSceneRenderer() {
    ReleaseTarget();
}

void MdxUiSceneRenderer::SetSharedContext(MpqManager* mpq, int gameVersion) {
    g_mdxUiMpq = mpq;
    g_mdxUiGameVersion = gameVersion;
}

void MdxUiSceneRenderer::Configure(const char* sceneKey, const char* modelPath,
                                   const char* sequence, bool autoPlay,
                                   bool useModelCamera) {
    const std::string newSceneKey = sceneKey ? sceneKey : "";
    const std::string newModelPath = modelPath ? modelPath : "";
    const std::string newSequence = sequence ? sequence : "";
    if (sceneKey_ == newSceneKey && modelPath_ == newModelPath && sequence_ == newSequence &&
        autoPlay_ == autoPlay && useModelCamera_ == useModelCamera) {
        return;
    }

    sceneKey_ = newSceneKey;
    modelPath_ = newModelPath;
    sequence_ = newSequence;
    autoPlay_ = autoPlay;
    useModelCamera_ = useModelCamera;
    dirty_ = true;
    loadAttempted_ = false;
    single_ = Layer{};
    background_ = Layer{};
    topLeft_ = Layer{};
    topRight_ = Layer{};
    logo_ = Layer{};
}

void MdxUiSceneRenderer::Resize(int width, int height) {
    width = std::max(1, width);
    height = std::max(1, height);
    if (width_ == width && height_ == height) {
        return;
    }

    width_ = width;
    height_ = height;
    ReleaseTarget();
}

Aero::Texture* MdxUiSceneRenderer::GetTexture() const {
    return noesisTexture_.GetPtr();
}

bool MdxUiSceneRenderer::LoadModelInto(Layer& layer, const std::string& path,
                                       const char* sequence, bool loop,
                                       bool setMidFrame) {
    if (!g_mdxUiMpq || path.empty()) {
        return false;
    }

    std::vector<uint8_t> data;
    if (!g_mdxUiMpq->ReadFile(path, data)) {
        return false;
    }

    MdxModel model;
    if (!MdxLoad(data.data(), data.size(), model)) {
        return false;
    }

    Layer nextLayer;
    nextLayer.model = std::make_unique<MdxModel>(std::move(model));
    nextLayer.viewer = std::make_unique<MdxViewer>();
    if (!nextLayer.viewer->Load(*nextLayer.model, g_mdxUiMpq, false, true)) {
        return false;
    }

    bool played = false;
    if (sequence && *sequence) {
        played = nextLayer.viewer->PlaySequenceByName(sequence, loop, 1.0f, false);
    }
    if (!played) {
        nextLayer.viewer->PlayStandSequence();
    }
    if (setMidFrame) {
        nextLayer.viewer->SetCurrentSequenceFrameByRatio(0.5f);
    }
    nextLayer.cameraSequenceIndex = FindSequenceIndexByName(*nextLayer.model, sequence);
    nextLayer.cameraLoop = loop;
    if (setMidFrame && nextLayer.cameraSequenceIndex >= 0) {
        nextLayer.cameraElapsedMs =
            MdxSequenceDurationSeconds(*nextLayer.model, nextLayer.cameraSequenceIndex) * 500.0f;
    }

    nextLayer.loaded = true;
    nextLayer.path = path;
    layer = std::move(nextLayer);
    return true;
}

bool MdxUiSceneRenderer::LoadFirstCandidate(Layer& layer, const std::string* paths, int count,
                                            const char* sequence, bool loop, bool setMidFrame) {
    for (int i = 0; i < count; ++i) {
        if (LoadModelInto(layer, paths[i], sequence, loop, setMidFrame)) {
            std::cout << "MdxUiSceneRenderer: loaded " << paths[i] << std::endl;
            return true;
        }
    }
    return false;
}

bool MdxUiSceneRenderer::LoadGlueMainMenu() {
    std::vector<std::string> backgroundCandidates;
    const std::string skinBackground = ResolveSkinField("GlueSpriteLayerBackground");
    AddCandidate(backgroundCandidates, skinBackground);
    AddCandidate(backgroundCandidates, MdxPathFromModelField(skinBackground));
    AddCandidate(backgroundCandidates, "UI\\Glues\\MainMenu\\MainMenu3D_exp\\MainMenu3D_exp.mdx");
    AddCandidate(backgroundCandidates, "UI\\Glues\\MainMenu\\MainMenu3D\\MainMenu3D.mdx");

    const bool loadedBackground = !backgroundCandidates.empty() &&
        LoadFirstCandidate(background_, backgroundCandidates.data(), (int)backgroundCandidates.size(),
                           "Stand", true, false);

    auto loadSkinLayer = [this](Layer& layer, const char* skinField, const char* fallback,
                                const char* sequence, bool loop, bool midFrame) -> bool {
        std::vector<std::string> candidates;
        const std::string skinModel = ResolveSkinField(skinField);
        AddCandidate(candidates, skinModel);
        AddCandidate(candidates, MdxPathFromModelField(skinModel));
        AddCandidate(candidates, fallback);
        return !candidates.empty() &&
            LoadFirstCandidate(layer, candidates.data(), (int)candidates.size(),
                               sequence, loop, midFrame);
    };

    const bool loadedLeft = loadSkinLayer(topLeft_, "GlueSpriteLayerTopLeft",
        "UI\\Glues\\SpriteLayers\\Expansion\\TopLeftPanel-Expansion.mdx",
        "MainMenu Birth", false, false);
    if (loadedLeft) {
        topLeft_.birthSequence = "MainMenu Birth";
        topLeft_.standSequence = "MainMenu Stand";
        topLeft_.waitingForStand = true;
    }
    const bool loadedRight = loadSkinLayer(topRight_, "GlueSpriteLayerTopRight",
        "UI\\Glues\\SpriteLayers\\Expansion\\TopRightPanel-Expansion.mdx",
        "MainMenu Birth", false, false);
    if (loadedRight) {
        topRight_.birthSequence = "MainMenu Birth";
        topRight_.standSequence = "MainMenu Stand";
        topRight_.waitingForStand = true;
    }
    const bool loadedLogo = loadSkinLayer(logo_, "MainMenuLogo",
        "UI\\Glues\\MainMenu\\WarCraftIIILogo_exp\\WarCraftIIILogo_exp.mdx",
        "Stand", true, false);
    logo_.billboardPlane = true;

    if (!loadedBackground && !loadedLeft && !loadedRight && !loadedLogo) {
        std::cerr << "MdxUiSceneRenderer: failed to load glue main menu MDX scene" << std::endl;
    }
    return loadedBackground || loadedLeft || loadedRight || loadedLogo;
}

bool MdxUiSceneRenderer::EnsureLoaded() {
    if (!dirty_ && loadAttempted_) {
        return single_.loaded || background_.loaded || topLeft_.loaded || topRight_.loaded || logo_.loaded;
    }
    if (loadAttempted_) {
        return false;
    }
    loadAttempted_ = true;
    dirty_ = false;

    if (!g_mdxUiMpq || g_mdxUiMpq->ArchiveCount() == 0) {
        std::cerr << "MdxUiSceneRenderer: MPQ context unavailable for scene '" << sceneKey_ << "'" << std::endl;
        return false;
    }

    if (sceneKey_ == "GlueMainMenuBackground" || sceneKey_ == "GlueMenuMainMenuBackground" ||
        sceneKey_ == "GlueBackground") {
        return LoadGlueMainMenu();
    }

    std::vector<std::string> candidates;
    AddCandidate(candidates, modelPath_);
    AddCandidate(candidates, MdxPathFromModelField(modelPath_));
    if (candidates.empty()) {
        std::cerr << "MdxUiSceneRenderer: no model path for scene '" << sceneKey_ << "'" << std::endl;
        return false;
    }

    return LoadFirstCandidate(single_, candidates.data(), (int)candidates.size(),
                              sequence_.empty() ? "Stand" : sequence_.c_str(), autoPlay_, false);
}

bool MdxUiSceneRenderer::EnsureTarget() {
    if (width_ <= 0 || height_ <= 0) {
        return false;
    }
    if (colorImage_.id && depthImage_.id && colorAttachmentView_.id && depthAttachmentView_.id &&
        noesisTexture_ != nullptr) {
        return true;
    }

    ReleaseTarget();

    if (!CreateGLTexture(width_, height_, colorTexture_)) {
        std::cerr << "MdxUiSceneRenderer: failed to create GL color texture" << std::endl;
        return false;
    }

    sg_image_desc colorDesc = {};
    colorDesc.width = width_;
    colorDesc.height = height_;
    colorDesc.pixel_format = SG_PIXELFORMAT_RGBA8;
    colorDesc.usage.color_attachment = true;
    colorDesc.gl_texture_target = GL_TEXTURE_2D_WAR3;
    for (int i = 0; i < SG_NUM_INFLIGHT_FRAMES; ++i) {
        colorDesc.gl_textures[i] = colorTexture_;
    }
    colorDesc.label = "war3_mdx_ui_color";
    colorImage_ = sg_make_image(&colorDesc);

    sg_image_desc depthDesc = {};
    depthDesc.width = width_;
    depthDesc.height = height_;
    depthDesc.pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL;
    depthDesc.usage.depth_stencil_attachment = true;
    depthDesc.label = "war3_mdx_ui_depth";
    depthImage_ = sg_make_image(&depthDesc);

    sg_view_desc colorViewDesc = {};
    colorViewDesc.color_attachment.image = colorImage_;
    colorViewDesc.label = "war3_mdx_ui_color_attachment";
    colorAttachmentView_ = sg_make_view(&colorViewDesc);

    sg_view_desc depthViewDesc = {};
    depthViewDesc.depth_stencil_attachment.image = depthImage_;
    depthViewDesc.label = "war3_mdx_ui_depth_attachment";
    depthAttachmentView_ = sg_make_view(&depthViewDesc);

    if (!colorImage_.id || !depthImage_.id || !colorAttachmentView_.id || !depthAttachmentView_.id) {
        std::cerr << "MdxUiSceneRenderer: failed to create sokol offscreen attachments" << std::endl;
        ReleaseTarget();
        return false;
    }

    noesisTexture_ = AeroApp::GLFactory::WrapTexture(colorTexture_, (uint32_t)width_, (uint32_t)height_,
        1, true, true);
    return noesisTexture_ != nullptr;
}

void MdxUiSceneRenderer::Render(float dt) {
    if (!EnsureLoaded() || !EnsureTarget()) {
        return;
    }

    sg_reset_state_cache();
    sg_pass_action action = {};
    action.colors[0].load_action = SG_LOADACTION_CLEAR;
    action.colors[0].clear_value = {0.0f, 0.0f, 0.0f, 1.0f};
    action.depth.load_action = SG_LOADACTION_CLEAR;
    action.depth.clear_value = 1.0f;
    action.stencil.load_action = SG_LOADACTION_CLEAR;
    action.stencil.clear_value = 0;

    sg_pass pass = {};
    pass.action = action;
    pass.attachments.colors[0] = colorAttachmentView_;
    pass.attachments.depth_stencil = depthAttachmentView_;
    pass.label = "war3_mdx_ui_pass";
    sg_begin_pass(&pass);
    sg_apply_viewport(0, 0, width_, height_, true);
    sg_apply_scissor_rect(0, 0, width_, height_, true);

    if (background_.loaded || topLeft_.loaded || topRight_.loaded || logo_.loaded) {
        RenderGlueMainMenu(dt);
    } else {
        RenderSingle(dt);
    }

    sg_end_pass();
    sg_reset_state_cache();
}

uint32_t MdxUiSceneRenderer::CameraFrameForLayer(const Layer& layer) const {
    if (!layer.model || layer.cameraSequenceIndex < 0
        || layer.cameraSequenceIndex >= (int)layer.model->sequences.size()) {
        return 0;
    }
    const MdxSequence& sequence = layer.model->sequences[(size_t)layer.cameraSequenceIndex];
    if (sequence.endFrame <= sequence.startFrame) {
        return sequence.startFrame;
    }
    const uint32_t range = sequence.endFrame - sequence.startFrame;
    uint32_t advance = (uint32_t)std::max(0.0f, layer.cameraElapsedMs);
    if (layer.cameraLoop) {
        advance %= range;
    } else if (advance >= range) {
        advance = range - 1;
    }
    return sequence.startFrame + advance;
}

void MdxUiSceneRenderer::AdvanceLayerCamera(Layer& layer, float dt) {
    if (!layer.model) {
        return;
    }
    layer.cameraElapsedMs += std::max(0.0f, dt) * 1000.0f;
    if (!layer.cameraLoop || layer.cameraSequenceIndex < 0
        || layer.cameraSequenceIndex >= (int)layer.model->sequences.size()) {
        return;
    }
    const float durationMs = MdxSequenceDurationSeconds(*layer.model, layer.cameraSequenceIndex) * 1000.0f;
    if (durationMs > 0.0f && layer.cameraElapsedMs >= durationMs) {
        layer.cameraElapsedMs = std::fmod(layer.cameraElapsedMs, durationMs);
    }
}

bool MdxUiSceneRenderer::BuildModelCameraViewProj(Layer& layer, float dt, Mat4& view, Mat4& proj) {
    if (!layer.model || layer.model->cameras.empty() || width_ <= 0 || height_ <= 0) {
        return false;
    }
    AdvanceLayerCamera(layer, dt);
    MdxCameraSample camera;
    MdxSampleCamera(layer.model->cameras[0], layer.model.get(),
                    CameraFrameForLayer(layer), layer.cameraElapsedMs,
                    camera, layer.cameraSequenceIndex);
    view = LookAt(MdxPointToYUp(camera.pos), MdxPointToYUp(camera.target), Vec3(0.0f, 1.0f, 0.0f));
    const float nearClip = std::max(1.0f, camera.nearClip);
    const float farClip = std::max(nearClip + 1.0f, camera.farClip);
    proj = Perspective(camera.fov * 0.6f * 57.2957795f, (float)width_ / (float)height_,
                       nearClip, farClip);
    return true;
}

void MdxUiSceneRenderer::RenderSingle(float dt) {
    if (!single_.loaded || width_ <= 0 || height_ <= 0) {
        return;
    }

    Mat4 view;
    Mat4 proj;
    if (!useModelCamera_ || !BuildModelCameraViewProj(single_, dt, view, proj)) {
        view = LookAt(Vec3(0.0f, 900.0f, 1800.0f), Vec3(0.0f, 400.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
        proj = Perspective(60.0f, (float)width_ / (float)height_, 8.0f, 70000.0f);
    }

    single_.viewer->Update(dt, view);
    single_.viewer->RenderAt(Vec3(0.0f, 0.0f, 0.0f), view, proj, 1.0f, 0, 0.0f, 1.0f, true);
}

void MdxUiSceneRenderer::RenderGlueMainMenu(float dt) {
    if (width_ <= 0 || height_ <= 0) {
        return;
    }

    if (background_.loaded) {
        Mat4 view;
        Mat4 proj;
        if (!useModelCamera_ || !BuildModelCameraViewProj(background_, dt, view, proj)) {
            view = LookAt(Vec3(0.0f, 900.0f, 1800.0f), Vec3(0.0f, 400.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
            proj = Perspective(60.0f, (float)width_ / (float)height_, 8.0f, 70000.0f);
        }
        const MenuFogSettings fog = LoadMenuFogSettings();
        if (fog.enabled) {
            background_.viewer->SetFog(fog.color, fog.start, fog.end);
        } else {
            background_.viewer->DisableFog();
        }
        background_.viewer->Update(dt, view);
        background_.viewer->RenderAt(Vec3(0.0f, 0.0f, 0.0f), view, proj, 1.0f, 0, 0.0f, 1.0f, true);
    }

    Mat4 spriteProj;
    const float aspect = (float)width_ / (float)height_;
    const float uiWidth = aspect > (4.0f / 3.0f) ? 0.8f * aspect / (4.0f / 3.0f) : 0.8f;
    const float uiHeight = aspect < (4.0f / 3.0f) ? 0.6f * (4.0f / 3.0f) / aspect : 0.6f;
    const float uiX = (0.8f - uiWidth) * 0.5f;
    const float uiY = (0.6f - uiHeight) * 0.5f;
    spriteProj.SetOrtho(uiX, uiX + uiWidth, uiY, uiY + uiHeight, -1024.0f, 1024.0f);
    Mat4 spriteView = Mat4::Identity();

    auto renderLayer = [&](Layer& layer) {
        if (!layer.loaded) {
            return;
        }
        layer.viewer->Update(dt, spriteView);
        if (layer.waitingForStand && layer.viewer->IsCurrentSequenceFinished()) {
            layer.waitingForStand = false;
            if (!layer.standSequence.empty() && !layer.viewer->PlaySequenceByName(layer.standSequence.c_str(), true)) {
                layer.viewer->PlayStandSequence();
            }
        }

        Mat4 model = Mat4::Identity();
        if (layer.billboardPlane) {
            const float logoScale = 1.35f;
            model.m[0] = 0.0f;
            model.m[5] = logoScale;
            model.m[8] = -logoScale;
            model.m[12] = 0.585f;
            model.m[13] = 0.47f;
        }
        layer.viewer->RenderWithModelMatrix(model, spriteView, spriteProj, 0, 1.0f, false);
    };

    renderLayer(topLeft_);
    renderLayer(topRight_);
    renderLayer(logo_);
}

void MdxUiSceneRenderer::ReleaseTarget() {
    noesisTexture_.Reset();
    if (colorAttachmentView_.id) {
        sg_destroy_view(colorAttachmentView_);
        colorAttachmentView_ = {0};
    }
    if (depthAttachmentView_.id) {
        sg_destroy_view(depthAttachmentView_);
        depthAttachmentView_ = {0};
    }
    if (colorImage_.id) {
        sg_destroy_image(colorImage_);
        colorImage_ = {0};
    }
    if (depthImage_.id) {
        sg_destroy_image(depthImage_);
        depthImage_ = {0};
    }
    DeleteGLTexture(colorTexture_);
    colorTexture_ = 0;
    framebuffer_ = 0;
    depthBuffer_ = 0;
}

} // namespace War3UI
