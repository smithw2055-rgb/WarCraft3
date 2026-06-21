#pragma once
#include "UiTypes.h"
#include "UIFrame.h"
#include "SimpleFrame.h"
#include "BackdropFrame.h"
#include "ButtonFrame.h"
#include "TextFrame.h"
#include "TextureFrame.h"
#include "FdfParser.h"
#include <unordered_map>
#include <string>
#include <memory>

class MpqManager;
class TextRenderer;

class FrameBuilder {
public:
    FrameBuilder(GameUI& gui, MpqManager* mpq = nullptr, TextRenderer* textRenderer = nullptr);

    void SetTextRenderer(TextRenderer* tr) { textRenderer_ = tr; }

    // Build a full FDF file: parse then construct all frames
    bool BuildFromFdf(const std::string& fdfContent, const std::string& rootName = "");

    // Build frames from already-parsed templates
    bool BuildFromTemplates(const std::map<std::string, FrameDefinition>& templates,
                             const std::string& rootName = "");

    // Build a single FrameDefinition into a UIFrame
    UIFrame* ConstructFrame(const FrameDefinition& def, UIFrame* parent = nullptr);

    // Get constructed frame by name
    UIFrame* GetFrame(const std::string& name) const;
    const std::unordered_map<std::string, UIFrame*>& GetAllFrames() const { return frames_; }

    // Cleanup
    void Clear();

private:
    UIFrame* CreateFrameByClass(const FrameDefinition& def, UIFrame* parent);
    void ApplyProperties(UIFrame* frame, const FrameDefinition& def);
    void ResolveSetPoints(UIFrame* frame, const FrameDefinition& def);
    void ResolveAnchors(UIFrame* frame, const FrameDefinition& def);
    void RegisterFrame(const std::string& name, UIFrame* frame);

    std::unordered_map<std::string, UIFrame*> frames_;
    std::unordered_map<std::string, UIFrame*> ownedFrames_;
    GameUI* gui_;
    MpqManager* mpq_ = nullptr;
    TextRenderer* textRenderer_ = nullptr;
};
