#pragma once

#include "render/math.h"
#include "render/mdx_viewer.h"
#include "model/mdx.h"
#include "resource/mpq_storm.h"

#include <NsCore/Ptr.h>

#include <memory>
#include <string>

namespace Noesis {
class Texture;
}

namespace War3UI {

class MdxUiSceneRenderer {
public:
    MdxUiSceneRenderer();
    ~MdxUiSceneRenderer();

    static void SetSharedContext(MpqManager* mpq, int gameVersion);

    void Configure(const char* sceneKey, const char* modelPath, const char* sequence,
                   bool autoPlay, bool useModelCamera);
    void Resize(int width, int height);
    void Render(float dt);
    Noesis::Texture* GetTexture() const;

private:
    struct Layer {
        std::unique_ptr<MdxModel> model;
        std::unique_ptr<MdxViewer> viewer;
        bool loaded = false;
        bool waitingForStand = false;
        bool billboardPlane = false;
        std::string birthSequence;
        std::string standSequence;
        std::string path;
    };

    bool EnsureLoaded();
    bool EnsureTarget();
    bool LoadModelInto(Layer& layer, const std::string& path, const char* sequence,
                       bool loop, bool setMidFrame);
    bool LoadFirstCandidate(Layer& layer, const std::string* paths, int count,
                            const char* sequence, bool loop, bool setMidFrame);
    bool LoadGlueMainMenu();
    void RenderSingle(float dt);
    void RenderGlueMainMenu(float dt);
    void ReleaseTarget();

    std::string sceneKey_;
    std::string modelPath_;
    std::string sequence_;
    bool autoPlay_ = true;
    bool useModelCamera_ = true;
    bool dirty_ = true;
    bool loadAttempted_ = false;

    int width_ = 0;
    int height_ = 0;
    unsigned int colorTexture_ = 0;
    unsigned int framebuffer_ = 0;
    unsigned int depthBuffer_ = 0;
    sg_image colorImage_ = {0};
    sg_image depthImage_ = {0};
    sg_view colorAttachmentView_ = {0};
    sg_view depthAttachmentView_ = {0};
    Noesis::Ptr<Noesis::Texture> noesisTexture_;

    Layer single_;
    Layer background_;
    Layer topLeft_;
    Layer topRight_;
    Layer logo_;
};

} // namespace War3UI
