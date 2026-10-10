#pragma once
#include "UiTypes.h"
#include "UIFrame.h"
#include "SimpleFrame.h"
#include "BackdropFrame.h"
#include "ButtonFrame.h"
#include "TextFrame.h"
#include "TextureFrame.h"
#include "ExtendedFrames.h"
#include "FdfParser.h"
#include "SkinData.h"
#include <unordered_map>
#include <string>
#include <memory>

class MpqManager;
class TextRenderer;
class Texture;

class FrameBuilder {
public:
    FrameBuilder(GameUI& gui, MpqManager* mpq = nullptr, TextRenderer* textRenderer = nullptr);

    void SetTextRenderer(TextRenderer* tr) { textRenderer_ = tr; }
    void SetSkinSection(const std::string& section) { skin_.section = section; }
    void SetSkinGameVersion(int v) { skin_.gameVersion = v; }
    // Full merged template table for cross-template references (SIMPLEBUTTON
    // NormalTexture/PushedTexture/... name texture templates).
    void SetTemplates(const std::map<std::string, FrameDefinition>& t) { templates_ = t; }
    // FDF StringList table (ref FrameTemplateEnvironment.getDecoratedString):
    // Text "COLON_DAMAGE" renders "Damage:", unknown keys stay literal.
    void SetDecoratedStrings(const std::map<std::string, std::string>& table) { decorated_ = table; }
    std::string DecorateText(const std::string& text) const {
        auto it = decorated_.find(text);
        return it != decorated_.end() ? it->second : text;
    }
    // Resolve an FDF File/Texture path through the skin (non-throwing) and
    // load it from the MPQ (ref GameUI.loadTexture: append/force .blp).
    std::shared_ptr<Texture> LoadSkinTexture(const std::string& file);

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
    // Counter for anonymous inner frames (retail ConsoleUI chrome uses
    // nameless Texture blocks; Java inflates them inline).
    int anonCounter_ = 0;
    // Resolved texture cache (ref GameUI.pathToTexture): FDF File keys
    // repeat across console chrome frames.
    std::unordered_map<std::string, std::shared_ptr<Texture>> textureCache_;
    SkinData skin_;
    std::map<std::string, std::string> decorated_;
    std::map<std::string, FrameDefinition> templates_;
    GameUI* gui_;
    MpqManager* mpq_ = nullptr;
    TextRenderer* textRenderer_ = nullptr;
};
