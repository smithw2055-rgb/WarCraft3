#include "FrameBuilder.h"
#include "../resource/mpq_storm.h"
#include "../asset/Texture.h"
#include <iostream>
#include <algorithm>

FrameBuilder::FrameBuilder(GameUI& gui, MpqManager* mpq, TextRenderer* textRenderer)
    : gui_(&gui), mpq_(mpq), textRenderer_(textRenderer) {
    if (mpq_) {
        skin_ = SkinData::Load(mpq_);
    }
}

std::shared_ptr<Texture> FrameBuilder::LoadSkinTexture(const std::string& file) {
    if (file.empty() || !mpq_) return nullptr;
    // Non-throwing skin resolution (ref GameUI.trySkinField): decorated keys
    // resolve, plain paths pass through unchanged.
    const std::string resolved = skin_.TryField(file);
    auto cached = textureCache_.find(resolved);
    if (cached != textureCache_.end()) return cached->second;
    // Ref GameUI.loadTexture: extensionless paths gain .blp, other
    // extensions are forced to .blp.
    std::string withExt = resolved;
    const size_t dot = withExt.find_last_of('.');
    const size_t sep = withExt.find_last_of("/\\");
    if (dot == std::string::npos || (sep != std::string::npos && dot < sep)) {
        withExt += ".blp";
    } else {
        withExt = withExt.substr(0, dot) + ".blp";
    }
    const char* candidates[2] = { resolved.c_str(), withExt.c_str() };
    for (const char* path : candidates) {
        std::vector<uint8_t> buf;
        if (!mpq_->ReadFile(path, buf) || buf.empty()) continue;
        auto tex = std::make_shared<Texture>();
        if (!tex->LoadFromMemory(buf.data(), buf.size())) continue;
        textureCache_[resolved] = tex;
        return tex;
    }
    return nullptr;
}

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
    // Retail FDF uses anonymous inner blocks (e.g. ConsoleUI chrome
    // Texture{File..} entries): inflate them inline under a generated name
    // (ref Java, which parents nameless frames directly).
    std::string name = def.name;
    if (name.empty()) {
        name = (parent ? parent->GetName() : std::string("Root")) + "/@anon" +
               std::to_string(anonCounter_++);
    }

    // Check if already constructed
    auto existing = frames_.find(name);
    if (existing != frames_.end()) {
        if (parent == nullptr || existing->second->GetParent() == parent) {
            return existing->second;
        }
        // Template-instantiated inner child for a different parent: disambiguate name
        name = parent->GetName() + "/" + name;
    }

    FrameDefinition namedDef = def;
    namedDef.name = name;
    UIFrame* frame = CreateFrameByClass(namedDef, parent);
    if (!frame) {
        // Fallback: use SimpleFrame
        frame = new SimpleFrame(name, parent);
    }

    frame->SetName(name);
    RegisterFrame(name, frame);

    // Add to parent if not already parented
    if (parent) {
        frame->SetParent(parent);
        if (auto* parentSimple = dynamic_cast<SimpleFrame*>(parent)) {
            parentSimple->AddChild(frame);
        }
    }

    if (gui_->rootFrame == parent && frame->GetParent() == nullptr) {
        frame->SetParent(gui_->rootFrame);
    }

    ApplyProperties(frame, def);
    ResolveSetPoints(frame, def);
    ResolveAnchors(frame, def);
    if (def.setPoints.empty() && def.anchors.empty() && !def.HasFlag("SetAllPoints")) {
        frame->SetDefaultToParent(true);
    }

    // Construct inner frames
    auto* btn = dynamic_cast<ButtonFrame*>(frame);
    auto* check = dynamic_cast<CheckBoxFrame*>(frame);
    for (const auto& inner : def.innerFrames) {
        UIFrame* child = ConstructFrame(inner, frame);
        if (child) {
            if (btn) {
                if (def.HasField("ControlBackdrop") && def.GetString("ControlBackdrop") == inner.name) {
                    child->SetAllPoints();
                    btn->SetControlBackdrop(child);
                } else if (def.HasField("ControlPushedBackdrop") && def.GetString("ControlPushedBackdrop") == inner.name) {
                    child->SetAllPoints();
                    btn->SetControlPushedBackdrop(child);
                } else if (def.HasField("ControlDisabledBackdrop") && def.GetString("ControlDisabledBackdrop") == inner.name) {
                    child->SetAllPoints();
                    btn->SetControlDisabledBackdrop(child);
                } else if (def.HasField("ControlMouseOverHighlight") && def.GetString("ControlMouseOverHighlight") == inner.name) {
                    child->SetAllPoints();
                    btn->SetControlHighlight(child);
                } else if (!check && def.HasField("ButtonText") && def.GetString("ButtonText") == inner.name) {
                    child->SetAllPoints();
                    child->SetWidth(0);
                    child->SetHeight(0);
                    if (auto* tf = dynamic_cast<TextFrame*>(child)) {
                        tf->SetAlign(TextAlign::CENTER);
                    }
                }
            }
            if (check) {
                if (def.HasField("CheckBoxCheckHighlight") && def.GetString("CheckBoxCheckHighlight") == inner.name) {
                    child->SetAllPoints();
                    check->SetCheckBoxCheckHighlight(child);
                } else if (def.HasField("CheckBoxDisabledCheckHighlight") && def.GetString("CheckBoxDisabledCheckHighlight") == inner.name) {
                    child->SetAllPoints();
                    check->SetCheckBoxDisabledCheckHighlight(child);
                }
            }
            auto* frameSimple = dynamic_cast<SimpleFrame*>(frame);
            if (!frameSimple) {
                auto* container = new SimpleFrame(def.name + "_container", frame);
                container->AddChild(child);
            }
        }
    }

    if (btn) {
        auto wireBackdrop = [&](const char* field, void (ButtonFrame::*setter)(UIFrame*)) {
            if (def.HasField(field)) {
                const std::string bdName = def.GetString(field);
                if (!bdName.empty()) {
                    auto it = frames_.find(name + "/" + bdName);
                    if (it == frames_.end()) it = frames_.find(bdName);
                    if (it != frames_.end() && it->second) {
                        it->second->SetAllPoints();
                        (btn->*setter)(it->second);
                    }
                }
            }
        };
        wireBackdrop("ControlBackdrop", &ButtonFrame::SetControlBackdrop);
        wireBackdrop("ControlPushedBackdrop", &ButtonFrame::SetControlPushedBackdrop);
        wireBackdrop("ControlDisabledBackdrop", &ButtonFrame::SetControlDisabledBackdrop);
        wireBackdrop("ControlMouseOverHighlight", &ButtonFrame::SetControlHighlight);
        if (!check && def.HasField("ButtonText")) {
            const std::string textName = def.GetString("ButtonText");
            if (!textName.empty()) {
                auto it = frames_.find(name + "/" + textName);
                if (it == frames_.end()) it = frames_.find(textName);
                if (it != frames_.end() && it->second) {
                    it->second->SetAllPoints();
                    it->second->SetWidth(0);
                    it->second->SetHeight(0);
                    if (auto* tf = dynamic_cast<TextFrame*>(it->second)) {
                        tf->SetAlign(TextAlign::CENTER);
                    }
                }
            }
        }
    }

    if (check) {
        auto wireCheckHighlight = [&](const char* field, void (CheckBoxFrame::*setter)(UIFrame*)) {
            if (def.HasField(field)) {
                const std::string hlName = def.GetString(field);
                if (!hlName.empty()) {
                    auto it = frames_.find(name + "/" + hlName);
                    if (it == frames_.end()) it = frames_.find(hlName);
                    if (it != frames_.end() && it->second) {
                        it->second->SetAllPoints();
                        (check->*setter)(it->second);
                    }
                }
            }
        };
        wireCheckHighlight("CheckBoxCheckHighlight", &CheckBoxFrame::SetCheckBoxCheckHighlight);
        wireCheckHighlight("CheckBoxDisabledCheckHighlight", &CheckBoxFrame::SetCheckBoxDisabledCheckHighlight);
    }

    if (auto* popup = dynamic_cast<PopupMenuFrame*>(frame)) {
        float inset = def.GetFloat("PopupButtonInset", 0.01f);
        if (def.HasField("PopupArrowFrame")) {
            std::string arrowName = def.GetString("PopupArrowFrame");
            auto it = frames_.find(name + "/" + arrowName);
            if (it == frames_.end()) it = frames_.find(arrowName);
            if (it != frames_.end() && it->second) {
                it->second->AddSetPoint(FramePoint::RIGHT, name, FramePoint::RIGHT, -gui_->ConvertX(inset), 0);
            }
        }
        if (def.HasField("PopupTitleFrame")) {
            std::string titleName = def.GetString("PopupTitleFrame");
            auto it = frames_.find(name + "/" + titleName);
            if (it == frames_.end()) it = frames_.find(titleName);
            if (it != frames_.end() && it->second) {
                it->second->AddSetPoint(FramePoint::LEFT, name, FramePoint::LEFT, gui_->ConvertX(inset), 0);
            }
        }
        if (def.HasField("PopupMenuFrame")) {
            std::string menuName = def.GetString("PopupMenuFrame");
            auto it = frames_.find(name + "/" + menuName);
            if (it == frames_.end()) it = frames_.find(menuName);
            if (it != frames_.end() && it->second) {
                it->second->AddSetPoint(FramePoint::TOPLEFT, name, FramePoint::BOTTOMLEFT, 0, 0);
                it->second->AddSetPoint(FramePoint::TOPRIGHT, name, FramePoint::BOTTOMRIGHT, 0, 0);
                it->second->SetVisible(false);
                popup->SetPopupMenu(it->second);
            }
        }
    }

    // DIALOG DialogBackdrop (ref AllianceDialog/ScriptDialog
    // DialogBackdrop "X"): retail stretches the named backdrop child to fill
    // the dialog; without this the backdrop keeps its 0x0 default and the
    // modal renders text over the live scene.
    if (def.HasField("DialogBackdrop")) {
        const std::string backdropName = def.GetString("DialogBackdrop", "");
        if (!backdropName.empty()) {
            auto it = frames_.find(backdropName);
            if (it != frames_.end() && it->second) {
                it->second->SetAllPoints();
            }
        }
    }

    return frame;
}

UIFrame* FrameBuilder::CreateFrameByClass(const FrameDefinition& def, UIFrame* parent) {
    UIFrame* frame = nullptr;

    // FDF font heights are FDF units (ref GameUI.generateFont).
    const float cvtH = gui_ ? (float)gui_->screenH / 0.6f : 1.0f;

    // Determine frame type from class and type string
    std::string type = def.frameType;

    // Map WC3 frame types to C++ classes
    if (def.frameClass == FrameClass::Texture) {
        // FDF Texture blocks (e.g. console chrome File "ConsoleTexture01"):
        // a real textured quad, NOT a backdrop (ref GameUI Texture case).
        frame = new TextureFrame(def.name, parent);
    }
    else if (type == "BACKDROP" || type == "SMARTBACKDROP" || type == "SMART_BACKDROP") {
        frame = new BackdropFrame(def.name, parent);
        // SmartBackdrop (ref SmartBackdropFrame 9-slice): tile the background
        // so stretched dialog borders keep corner proportions.
        if (type != "BACKDROP") static_cast<BackdropFrame*>(frame)->SetTileBackground(true);
    }
    else if (type == "CONTROL") {
        // Generic container (ref ControlFrame): plain frame grouping children.
        frame = new SimpleFrame(def.name, parent);
    }
    else if (type == "SINGLESTRING" || type == "SINGLE_STRING") {
        frame = new TextFrame(def.name, parent);
        auto* txtFrame = dynamic_cast<TextFrame*>(frame);
        if (txtFrame && textRenderer_) txtFrame->SetTextRenderer(textRenderer_);
        if (def.HasField("Text")) {
            std::string text = DecorateText(def.GetString("Text", ""));
            if (txtFrame) txtFrame->SetText(text);
        }
    }
    else if (type == "SPRITE2") {
        frame = new SpriteFrame(def.name, parent);
    }
    else if (type == "CLICKCONSUMINGTEXTURE" || type == "CLICK_CONSUMING_TEXTURE" ||
             type == "FILTERMODETEXTURE" || type == "FILTER_MODE_TEXTURE") {
        // Click-swallow / blend-mode quads (ref ClickConsumingTextureFrame,
        // FilterModeTextureFrame): same textured quad, no highlight art.
        frame = new TextureFrame(def.name, parent);
    }
    else if (type == "BUTTON" || type == "GLUEBUTTON" || type == "GLUETEXTBUTTON" ||
             type == "TEXTBUTTON" || type == "SIMPLEBUTTON") {
        frame = new ButtonFrame(def.name, parent);
        auto* btn = dynamic_cast<ButtonFrame*>(frame);
        if (btn && textRenderer_) btn->SetTextRenderer(textRenderer_);

        // Button state art (ref SIMPLEBUTTON NormalTexture/PushedTexture/
        // DisabledTexture + UseHighlight, each naming a Texture template).
        if (btn && mpq_) {
            auto resolveState = [&](const std::string& field, ButtonFrame::StateArt& art) {
                const std::string tplName = def.GetString(field, "");
                if (tplName.empty()) return;
                auto tit = templates_.find(tplName);
                if (tit == templates_.end()) return;
                const FrameDefinition& tdef = tit->second;
                const std::string file = tdef.GetString("File", tdef.GetString("Texture", ""));
                if (file.empty()) return;
                auto tex = LoadSkinTexture(file);
                if (!tex) return;
                art.tex = tex;
                if (tdef.vector4Fields.count("TexCoord")) {
                    const auto& tc = tdef.vector4Fields.at("TexCoord");
                    art.u0 = tc.x;
                    art.v0 = tc.z;
                    art.u1 = tc.y;
                    art.v1 = tc.w;
                }
                art.valid = true;
            };
            ButtonFrame::StateArt normal, pushed, disabled, highlight;
            resolveState("NormalTexture", normal);
            resolveState("PushedTexture", pushed);
            resolveState("DisabledTexture", disabled);
            resolveState("UseHighlight", highlight);
            if (normal.valid) btn->SetNormalTexture(normal);
            if (pushed.valid) btn->SetPushedTexture(pushed);
            if (disabled.valid) btn->SetDisabledTexture(disabled);
            if (highlight.valid) btn->SetHighlightTexture(highlight);
        }

        // Check for button text
        if (def.HasField("Text")) {
            std::string text = DecorateText(def.GetString("Text", ""));
            if (!text.empty()) {
                auto* btnFrame = dynamic_cast<ButtonFrame*>(frame);
                if (btnFrame) btnFrame->SetText(text);
            }
        }

        // Font size (FDF units -> pixels, ref GameUI.generateFont).
        if (def.HasField("FontHeight")) {
            float fontSize = def.GetFloat("FontHeight", 14.0f);
            auto* btnFrame = dynamic_cast<ButtonFrame*>(frame);
            if (btnFrame) btnFrame->SetFontSize(fontSize * cvtH);
        }
    }
    else if (type == "TEXT" || def.frameClass == FrameClass::String) {
        frame = new TextFrame(def.name, parent);
        auto* txtFrame = dynamic_cast<TextFrame*>(frame);
        if (txtFrame && textRenderer_) txtFrame->SetTextRenderer(textRenderer_);

        if (def.HasField("Text")) {
            std::string text = DecorateText(def.GetString("Text", ""));
            auto* txtFrame = dynamic_cast<TextFrame*>(frame);
            if (txtFrame) txtFrame->SetText(text);
        }

        if (def.HasField("FontHeight")) {
            float fontSize = def.GetFloat("FontHeight", 14.0f);
            auto* txtFrame = dynamic_cast<TextFrame*>(frame);
            if (txtFrame) txtFrame->SetFontSize(fontSize);
        }
    }
    else if (type == "CHECKBOX" || type == "GLUECHECKBOX") {
        frame = new CheckBoxFrame(def.name, parent);
    }
    else if (type == "HIGHLIGHT") {
        frame = new TextureFrame(def.name, parent);
        auto* tf = dynamic_cast<TextureFrame*>(frame);
        if (tf && mpq_) {
            std::string file = def.GetString("HighlightAlphaFile", def.GetString("File", def.GetString("Texture", "")));
            if (!file.empty()) {
                auto tex = LoadSkinTexture(file);
                if (tex) tf->SetTexture(tex);
            }
        }
    }
    else if (type == "EDITBOX") {
        frame = new EditBoxFrame(def.name, parent);
        if (def.HasField("Text")) {
            static_cast<EditBoxFrame*>(frame)->SetText(def.GetString("Text", ""));
        }
    }
    else if (type == "TEXTAREA") {
        frame = new TextAreaFrame(def.name, parent);
        if (def.HasField("Text")) {
            static_cast<TextAreaFrame*>(frame)->SetText(def.GetString("Text", ""));
        }
    }
    else if (type == "LISTBOX") {
        frame = new ListBoxFrame(def.name, parent);
    }
    else if (type == "POPUPMENU") {
        frame = new PopupMenuFrame(def.name, parent);
    }
    else if (type == "MENU") {
        frame = new MenuFrame(def.name, parent);
    }
    else if (type == "SCROLLBAR" || type == "SLIDER") {
        frame = new ScrollBarFrame(def.name, parent);
    }
    else if (type == "SIMPLESTATUSBAR" || type == "STATUSBAR") {
        frame = new StatusBarFrame(def.name, parent);
    }
    else if (type == "SPRITE") {
        frame = new SpriteFrame(def.name, parent);
    }
    else if (type == "TIMERBAR" || type == "SIMPLEFRAME" ||
             def.frameClass == FrameClass::Frame) {
        frame = new SimpleFrame(def.name, parent);
    }
    else {
        frame = new SimpleFrame(def.name, parent);
    }

    // FDF MenuItem entries feed list-like frames (ref MenuItemFrameDefinitionField).
    if (auto* list = dynamic_cast<ListBoxFrame*>(frame)) {
        for (const auto& item : def.menuItems) list->AddItem(item.first);
    }
    if (auto* popup = dynamic_cast<PopupMenuFrame*>(frame)) {
        for (const auto& item : def.menuItems) popup->AddItem(item.first);
    }

    return frame;
}

void FrameBuilder::ApplyProperties(UIFrame* frame, const FrameDefinition& def) {
    if (!frame) return;

    // FDF geometry is in 0..0.8 x 0..0.6 units (ref GameUI.convertX/convertY);
    // the runtime works in pixels.
    const float cvtW = gui_ ? (float)gui_->screenW / 0.8f : 1.0f;
    const float cvtH = gui_ ? (float)gui_->screenH / 0.6f : 1.0f;

    // Size
    if (def.HasField("Width")) {
        frame->SetWidth(def.GetFloat("Width", 0) * cvtW);
    }
    if (def.HasField("Height")) {
        frame->SetHeight(def.GetFloat("Height", 0) * cvtH);
    }

    if (def.HasFlag("SetAllPoints")) frame->SetAllPoints();

    // Visibility
    if (def.HasFlag("Visible")) frame->SetVisible(true);
    if (def.HasFlag("Hidden") || def.HasFlag("Invisible")) frame->SetVisible(false);

    // Backdrop-specific (skin-resolved, ref GameUI BACKDROP case with
    // DecorateFileNames -> trySkinField for both background and edge).
    auto* backdrop = dynamic_cast<BackdropFrame*>(frame);
    if (backdrop && mpq_) {
        // Background texture
        if (def.HasField("BackdropBackground")) {
            if (auto tex = LoadSkinTexture(def.GetString("BackdropBackground", ""))) {
                backdrop->SetBackground(tex);
            }
        }
        // Edge texture
        if (def.HasField("BackdropEdgeFile")) {
            if (auto tex = LoadSkinTexture(def.GetString("BackdropEdgeFile", ""))) {
                backdrop->SetEdgeFile(tex);
            }
        }
        if (def.HasField("BackdropCornerFlags")) {
            backdrop->SetCornerFlags(
                BackdropFrame::ParseCornerFlags(def.GetString("BackdropCornerFlags", "")));
        }
        if (def.vector4Fields.count("BackdropBackgroundInsets")) {
            // FDF order is L,T,R,B (ref the Java Vector4Definition shuffle).
            const auto& in = def.vector4Fields.at("BackdropBackgroundInsets");
            backdrop->SetBackgroundInsets(in.x * cvtW, in.w * cvtH, in.z * cvtW, in.y * cvtH);
        }
        if (def.HasField("BackdropCornerSize")) {
            backdrop->SetCornerSize(def.GetFloat("BackdropCornerSize", 0.01f) * cvtW);
        } else if (def.HasField("CornerSize")) {
            backdrop->SetCornerSize(def.GetFloat("CornerSize", 8) * cvtW);
        }
        if (def.HasField("BackdropBackgroundSize")) {
            backdrop->SetBackgroundSize(def.GetFloat("BackdropBackgroundSize", 0.05f) * cvtW);
        } else if (def.HasField("BackgroundSize")) {
            backdrop->SetBackgroundSize(def.GetFloat("BackgroundSize", 32) * cvtW);
        }
        if (def.HasFlag("BackdropTileBackground") || def.HasFlag("TileBackground")) {
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
        if (def.HasField("Color") || def.HasField("FontColor")) {
            auto col = def.HasField("Color") ? def.GetVector4("Color") : def.GetVector4("FontColor");
            uint32_t c = ((uint32_t)(col.w * 255.0f) << 24) |
                         ((uint32_t)(col.x * 255.0f) << 16) |
                         ((uint32_t)(col.y * 255.0f) << 8) |
                         ((uint32_t)(col.z * 255.0f));
            text->SetColor(c);
        }
        float fontPx = 14.0f;
        if (def.HasField("FontHeight")) {
            fontPx = def.GetFloat("FontHeight", 14.0f) * cvtH;
            text->SetFontSize(fontPx);
        }
        if (!def.HasField("Height")) {
            // Dependents anchor to this frame's edges (e.g. value below its
            // label), so a heightless text frame gets one line of height.
            text->SetHeight(fontPx);
        }
        // Font justification flags / fields
        std::string justH = def.GetString("FontJustificationH", "");
        if (justH.empty()) {
            if (def.HasFlag("JUSTIFYLEFT")) justH = "JUSTIFYLEFT";
            else if (def.HasFlag("JUSTIFYCENTER")) justH = "JUSTIFYCENTER";
            else if (def.HasFlag("JUSTIFYRIGHT")) justH = "JUSTIFYRIGHT";
        }
        if (justH == "JUSTIFYCENTER") text->SetAlign(TextAlign::CENTER);
        else if (justH == "JUSTIFYRIGHT") text->SetAlign(TextAlign::RIGHT);
        else if (justH == "JUSTIFYLEFT") text->SetAlign(TextAlign::LEFT);
    }

    // TextureFrame-specific (ref GameUI Texture case: File + TexCoord,
    // resolved through the skin when decorated).
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
        if (def.vector4Fields.count("TexCoord")) {
            // FDF order is U0,U1,V0,V1 (ref TextureFrame.setTexCoord usage).
            const auto& tc = def.vector4Fields.at("TexCoord");
            texFrame->SetTexCoords(tc.x, tc.z, tc.y, tc.w);
        }
        std::string file = def.GetString("File", def.GetString("Texture", ""));
        if (!file.empty() && mpq_) {
            if (auto tex = LoadSkinTexture(file)) {
                texFrame->SetTexture(tex);
            }
        }
    }

    // Extended-frame state (ref parsers/fdf/frames/*). Field keys are the
    // FDF attribute names; numeric defaults preserve previous behavior.
    if (auto* check = dynamic_cast<CheckBoxFrame*>(frame)) {
        if (def.HasFlag("Checked")) check->SetChecked(true);
    }
    if (auto* edit = dynamic_cast<EditBoxFrame*>(frame)) {
        if (def.HasField("MaxLength")) edit->SetMaxLength((int)def.GetFloat("MaxLength", 256));
    }
    if (auto* scroll = dynamic_cast<ScrollBarFrame*>(frame)) {
        if (def.HasField("MinValue") || def.HasField("MaxValue"))
            scroll->SetRange(def.GetFloat("MinValue", 0), def.GetFloat("MaxValue", 1));
        if (def.HasField("Value")) scroll->SetValue(def.GetFloat("Value", 0));
    }
    if (auto* status = dynamic_cast<StatusBarFrame*>(frame)) {
        if (def.HasField("MaxValue")) status->SetMax(def.GetFloat("MaxValue", 1));
        if (def.HasField("Value")) status->SetValue(def.GetFloat("Value", 1));
    }
    if (auto* sprite = dynamic_cast<SpriteFrame*>(frame)) {
        if (def.HasField("Model")) sprite->SetModel(def.GetString("Model", ""));
    }
}

void FrameBuilder::ResolveSetPoints(UIFrame* frame, const FrameDefinition& def) {
    if (!frame || !gui_) return;
    for (const auto& sp : def.setPoints) {
        // FDF offsets are in FDF units with an up-positive Y (ref
        // GameUI.inflate convertX/convertY); the renderer is y-down, so the
        // Y offset is negated while X carries over unchanged.
        frame->AddSetPoint(sp.myPoint, sp.otherFrame, sp.otherPoint,
                           gui_->ConvertX(sp.x), -gui_->ConvertY(sp.y));
    }
}

void FrameBuilder::ResolveAnchors(UIFrame* frame, const FrameDefinition& def) {
    if (!frame || !gui_) return;
    for (const auto& a : def.anchors) {
        frame->AddAnchor(a.point, gui_->ConvertX(a.x), -gui_->ConvertY(a.y));
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
