#include "FrameBuilder.h"
#include "../resource/mpq_storm.h"
#include "../asset/Texture.h"
#include <iostream>
#include <algorithm>

FrameBuilder::FrameBuilder(GameUI& gui, MpqManager* mpq, TextRenderer* textRenderer)
    : gui_(&gui), mpq_(mpq), textRenderer_(textRenderer) {}

bool FrameBuilder::BuildFromFdf(const std::string& fdfContent, const std::string& rootName) {
    FdfParser parser(fdfContent);
    parser.Parse();
    const auto& templates = parser.GetTemplates();
    if (templates.empty()) {
        std::cerr << "FrameBuilder: no templates parsed from FDF" << std::endl;
        return false;
    }
    return BuildFromTemplates(templates, rootName);
}

bool FrameBuilder::BuildFromTemplates(const std::map<std::string, FrameDefinition>& templates,
                                       const std::string& rootName) {
    if (templates.empty()) return false;

    // Find root template
    std::string root = rootName;
    if (root.empty()) {
        // Use the first template as root
        root = templates.begin()->first;
    }

    auto rootIt = templates.find(root);
    if (rootIt == templates.end()) {
        std::cerr << "FrameBuilder: root '" << root << "' not found in templates" << std::endl;
        return false;
    }

    UIFrame* rootFrame = ConstructFrame(rootIt->second, gui_->rootFrame);
    if (!rootFrame) {
        std::cerr << "FrameBuilder: failed to construct root frame" << std::endl;
        return false;
    }

    return true;
}

UIFrame* FrameBuilder::ConstructFrame(const FrameDefinition& def, UIFrame* parent) {
    if (def.name.empty()) return nullptr;

    // Check if already constructed
    auto existing = frames_.find(def.name);
    if (existing != frames_.end()) return existing->second;

    UIFrame* frame = CreateFrameByClass(def, parent);
    if (!frame) {
        // Fallback: use SimpleFrame
        frame = new SimpleFrame(def.name, parent);
    }

    frame->SetName(def.name);
    RegisterFrame(def.name, frame);

    // Add to parent if not already parented
    if (parent && frame->GetParent() == nullptr) {
        frame->SetParent(parent);
        auto* parentSimple = dynamic_cast<SimpleFrame*>(parent);
        if (parentSimple) {
            parentSimple->AddChild(frame);
        }
    }

    if (gui_->rootFrame == parent && frame->GetParent() == nullptr) {
        frame->SetParent(gui_->rootFrame);
    }

    ApplyProperties(frame, def);
    ResolveSetPoints(frame, def);
    ResolveAnchors(frame, def);

    // Construct inner frames
    for (const auto& inner : def.innerFrames) {
        UIFrame* child = ConstructFrame(inner, frame);
        if (child) {
            auto* frameSimple = dynamic_cast<SimpleFrame*>(frame);
            if (!frameSimple) {
                auto* container = new SimpleFrame(def.name + "_container", frame);
                container->AddChild(child);
            }
        }
    }

    return frame;
}

UIFrame* FrameBuilder::CreateFrameByClass(const FrameDefinition& def, UIFrame* parent) {
    UIFrame* frame = nullptr;

    // Determine frame type from class and type string
    std::string type = def.frameType;

    // Map WC3 frame types to C++ classes
    if (type == "BACKDROP" || def.frameClass == FrameClass::Texture) {
        frame = new BackdropFrame(def.name, parent);
    }
    else if (type == "BUTTON" || type == "GLUEBUTTON" || type == "TEXTBUTTON") {
        frame = new ButtonFrame(def.name, parent);
        auto* btn = dynamic_cast<ButtonFrame*>(frame);
        if (btn && textRenderer_) btn->SetTextRenderer(textRenderer_);

        // Check for button text
        if (def.HasField("Text")) {
            std::string text = def.GetString("Text", "");
            if (!text.empty()) {
                auto* btnFrame = dynamic_cast<ButtonFrame*>(frame);
                if (btnFrame) btnFrame->SetText(text);
            }
        }

        // Font size
        if (def.HasField("FontJustification") || def.HasField("FontHeight")) {
            float fontSize = def.GetFloat("FontHeight", 14.0f);
            auto* btnFrame = dynamic_cast<ButtonFrame*>(frame);
            if (btnFrame) btnFrame->SetFontSize(fontSize);
        }
    }
    else if (type == "TEXT" || def.frameClass == FrameClass::String) {
        frame = new TextFrame(def.name, parent);
        auto* txtFrame = dynamic_cast<TextFrame*>(frame);
        if (txtFrame && textRenderer_) txtFrame->SetTextRenderer(textRenderer_);

        if (def.HasField("Text")) {
            std::string text = def.GetString("Text", "");
            auto* txtFrame = dynamic_cast<TextFrame*>(frame);
            if (txtFrame) txtFrame->SetText(text);
        }

        if (def.HasField("FontHeight")) {
            float fontSize = def.GetFloat("FontHeight", 14.0f);
            auto* txtFrame = dynamic_cast<TextFrame*>(frame);
            if (txtFrame) txtFrame->SetFontSize(fontSize);
        }
    }
    else if (type == "TIMERBAR" || type == "SIMPLEFRAME" || type == "LISTBOX" ||
             type == "SLIDER" || type == "POPUPMENU" || type == "EDITBOX" ||
             def.frameClass == FrameClass::Frame) {
        frame = new SimpleFrame(def.name, parent);
    }
    else if (type == "GLUETEXTBUTTON" || type == "GLUEBUTTON") {
        frame = new ButtonFrame(def.name, parent);
        auto* btn = dynamic_cast<ButtonFrame*>(frame);
        if (btn && textRenderer_) btn->SetTextRenderer(textRenderer_);
    }
    else {
        frame = new SimpleFrame(def.name, parent);
    }

    return frame;
}

void FrameBuilder::ApplyProperties(UIFrame* frame, const FrameDefinition& def) {
    if (!frame) return;

    // Size
    if (def.HasField("Width")) {
        frame->SetWidth(def.GetFloat("Width", 0));
    }
    if (def.HasField("Height")) {
        frame->SetHeight(def.GetFloat("Height", 0));
    }

    // Visibility
    if (def.HasFlag("Visible")) frame->SetVisible(true);
    if (def.HasFlag("Hidden") || def.HasFlag("Invisible")) frame->SetVisible(false);

    // Backdrop-specific
    auto* backdrop = dynamic_cast<BackdropFrame*>(frame);
    if (backdrop && mpq_) {
        // Background texture
        if (def.HasField("BackdropBackground")) {
            std::string texPath = def.GetString("BackdropBackground", "");
            if (!texPath.empty()) {
                std::vector<uint8_t> buf;
                if (mpq_->ReadFile(texPath, buf)) {
                    auto tex = std::make_shared<Texture>();
                    if (tex->LoadFromMemory(buf.data(), buf.size())) {
                        backdrop->SetBackground(tex);
                    }
                }
            }
        }
        // Edge texture
        if (def.HasField("BackdropEdgeFile")) {
            std::string edgePath = def.GetString("BackdropEdgeFile", "");
            if (!edgePath.empty()) {
                std::vector<uint8_t> buf;
                if (mpq_->ReadFile(edgePath, buf)) {
                    auto tex = std::make_shared<Texture>();
                    if (tex->LoadFromMemory(buf.data(), buf.size())) {
                        backdrop->SetEdgeFile(tex);
                    }
                }
            }
        }
        if (def.HasField("CornerSize")) {
            backdrop->SetCornerSize(def.GetFloat("CornerSize", 8));
        }
        if (def.HasField("BackgroundSize")) {
            backdrop->SetBackgroundSize(def.GetFloat("BackgroundSize", 32));
        }
        if (def.HasFlag("TileBackground")) {
            backdrop->SetTileBackground(true);
        }
    }

    // Button-specific
    auto* button = dynamic_cast<ButtonFrame*>(frame);
    if (button) {
        if (def.HasFlag("Enabled")) button->SetEnabled(true);
        if (def.HasFlag("Disabled")) button->SetEnabled(false);

        if (def.HasField("Tooltip")) {
            button->SetTooltip(def.GetString("Tooltip", ""));
        }
    }

    // Text-specific
    auto* text = dynamic_cast<TextFrame*>(frame);
    if (text) {
        if (def.HasField("Color")) {
            auto col = def.GetVector4("Color");
            uint32_t c = ((uint32_t)(col.w * 255.0f) << 24) |
                         ((uint32_t)(col.x * 255.0f) << 16) |
                         ((uint32_t)(col.y * 255.0f) << 8) |
                         ((uint32_t)(col.z * 255.0f));
            text->SetColor(c);
        }
        if (def.HasField("FontHeight")) {
            text->SetFontSize(def.GetFloat("FontHeight", 14.0f));
        }
        // Font justification flags
        if (def.HasFlag("JUSTIFYLEFT")) text->SetAlign(TextAlign::LEFT);
        if (def.HasFlag("JUSTIFYCENTER")) text->SetAlign(TextAlign::CENTER);
        if (def.HasFlag("JUSTIFYRIGHT")) text->SetAlign(TextAlign::RIGHT);
    }

    // TextureFrame-specific (if we created one via class)
    auto* texFrame = dynamic_cast<TextureFrame*>(frame);
    if (texFrame) {
        if (def.HasField("Color")) {
            auto col = def.GetVector4("Color");
            uint32_t c = ((uint32_t)(col.w * 255.0f) << 24) |
                         ((uint32_t)(col.x * 255.0f) << 16) |
                         ((uint32_t)(col.y * 255.0f) << 8) |
                         ((uint32_t)(col.z * 255.0f));
            texFrame->SetColor(c);
        }
        if (def.HasField("Texture")) {
            std::string texPath = def.GetString("Texture", "");
            if (!texPath.empty() && mpq_) {
                std::vector<uint8_t> buf;
                if (mpq_->ReadFile(texPath, buf)) {
                    auto tex = std::make_shared<Texture>();
                    if (tex->LoadFromMemory(buf.data(), buf.size())) {
                        texFrame->SetTexture(tex);
                    }
                }
            }
        }
    }
}

void FrameBuilder::ResolveSetPoints(UIFrame* frame, const FrameDefinition& def) {
    if (!frame) return;
    for (const auto& sp : def.setPoints) {
        frame->AddSetPoint(sp.myPoint, sp.otherFrame, sp.otherPoint, sp.x, sp.y);
    }
}

void FrameBuilder::ResolveAnchors(UIFrame* frame, const FrameDefinition& def) {
    if (!frame) return;
    for (const auto& a : def.anchors) {
        frame->AddAnchor(a.point, a.x, a.y);
    }
}

void FrameBuilder::RegisterFrame(const std::string& name, UIFrame* frame) {
    if (name.empty() || !frame) return;
    frames_[name] = frame;
    if (gui_) {
        gui_->frameRegistry[name] = frame;
    }
}

UIFrame* FrameBuilder::GetFrame(const std::string& name) const {
    auto it = frames_.find(name);
    return it != frames_.end() ? it->second : nullptr;
}

void FrameBuilder::Clear() {
    frames_.clear();
    ownedFrames_.clear();
}
