// FDF tests: property-grammar shapes (vectors incl. space-separated
// retail style, fonts, string pairs, justification keywords, menu
// items, bare flags), inheritance of the new members, a builder smoke
// test, and a retail-corpus round trip (MPQ-gated).
#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>

#include "resource/mpq_storm.h"
#include "ui/FdfParser.h"
#include "ui/FrameBuilder.h"
#include "ui/SimpleFrame.h"
#include "ui/UIFrame.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR ""
#endif

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)
#define TEST_NEAR(a, b, eps) do { if (std::fabs((a) - (b)) > (eps)) { std::cerr << "NEAR @" << __LINE__ << ": " #a " vs " #b "\n"; return false; } } while (0)

namespace fs = std::filesystem;

namespace {

std::map<std::string, FrameDefinition> ParseTemplates(const std::string& fdf) {
    FdfParser parser(fdf);
    parser.Parse();
    return parser.GetTemplates();
}

} // namespace

bool TestFdfNumericShapes() {
    auto templates = ParseTemplates(
        "Frame \"FRAME\" \"Root\" {\n"
        "    Width 0.5,\n"
        "    Height 0.25f,\n"
        "    Offset 0.1 0.2,\n"
        "    Mixed 1, 2, 3,\n"
        "    Color 1.0 1.0 1.0 1.0,\n"
        "    Insets 0.01, 0.02, 0.03, 0.04,\n"
        "}\n");
    TEST_ASSERT(templates.count("Root") == 1);
    const FrameDefinition& def = templates.at("Root");
    TEST_NEAR(def.GetFloat("Width", 0), 0.5f, 1e-6f);
    TEST_NEAR(def.GetFloat("Height", 0), 0.25f, 1e-6f);
    TEST_ASSERT(def.vector2Fields.count("Offset") == 1);
    TEST_NEAR(def.vector2Fields.at("Offset").x, 0.1f, 1e-6f);
    TEST_NEAR(def.vector2Fields.at("Offset").y, 0.2f, 1e-6f);
    TEST_ASSERT(def.vector3Fields.count("Mixed") == 1);
    TEST_NEAR(def.vector3Fields.at("Mixed").z, 3.0f, 1e-6f);
    TEST_ASSERT(def.vector4Fields.count("Color") == 1);
    TEST_NEAR(def.vector4Fields.at("Color").w, 1.0f, 1e-6f);
    TEST_NEAR(def.vector4Fields.at("Insets").x, 0.01f, 1e-6f);
    return true;
}

bool TestFdfFontAndPairs() {
    auto templates = ParseTemplates(
        "Frame \"FRAME\" \"Root\" {\n"
        "    Font \"InfoFont\",0.013,\n"
        "    TitleFont \"TitleFont\", 0.02, \"BOLD\",\n"
        "    Text \"Hello\",\n"
        "    AltText \"A\" \"B\",\n"
        "    PairComma \"C\", \"D\",\n"
        "}\n");
    const FrameDefinition& def = templates.at("Root");
    TEST_ASSERT(def.fontFields.count("Font") == 1);
    TEST_ASSERT(def.fontFields.at("Font").name == "InfoFont");
    TEST_NEAR(def.fontFields.at("Font").size, 0.013f, 1e-6f);
    TEST_ASSERT(def.fontFields.count("TitleFont") == 1);
    TEST_ASSERT(def.fontFields.at("TitleFont").flags == "BOLD");
    TEST_ASSERT(def.GetString("Text", "") == "Hello");
    TEST_ASSERT(def.stringPairFields.count("AltText") == 1);
    TEST_ASSERT(def.stringPairFields.at("AltText").second == "B");
    TEST_ASSERT(def.stringPairFields.count("PairComma") == 1);
    TEST_ASSERT(def.stringPairFields.at("PairComma").second == "D");
    // First of the pair stays visible through the string lookup.
    TEST_ASSERT(def.GetString("AltText", "") == "A");
    return true;
}

bool TestFdfJustifyMenuFlags() {
    auto templates = ParseTemplates(
        "Frame \"FRAME\" \"Root\" {\n"
        "    FontJustificationH JUSTIFYLEFT,\n"
        "    FontJustificationV JUSTIFYBOTTOM,\n"
        "    SetAllPoints,\n"
        "    MenuItem \"Okay\", 0,\n"
        "    MenuItem \"Cancel\",1,\n"
        "}\n");
    const FrameDefinition& def = templates.at("Root");
    TEST_ASSERT(def.GetString("FontJustificationH", "") == "JUSTIFYLEFT");
    TEST_ASSERT(def.GetString("FontJustificationV", "") == "JUSTIFYBOTTOM");
    TEST_ASSERT(def.HasFlag("SetAllPoints"));
    TEST_ASSERT(def.menuItems.size() == 2);
    TEST_ASSERT(def.menuItems[0].first == "Okay" && def.menuItems[0].second == 0);
    TEST_ASSERT(def.menuItems[1].first == "Cancel" && def.menuItems[1].second == 1);
    return true;
}

bool TestFdfNestingAndInherit() {
    auto templates = ParseTemplates(
        "Frame \"FRAME\" \"Base\" {\n"
        "    Width 0.5,\n"
        "    Font \"BaseFont\",0.01,\n"
        "    AltText \"A\" \"B\",\n"
        "}\n"
        "Frame \"BUTTON\" \"Child\" INHERITS \"Base\" {\n"
        "    Height 0.25,\n"
        "    Frame \"TEXT\" \"Label\" {\n"
        "        Text \"Hi\",\n"
        "    }\n"
        "}\n");
    TEST_ASSERT(templates.count("Child") == 1);
    const FrameDefinition& child = templates.at("Child");
    // Inherited numerics, fonts and pairs survive.
    TEST_NEAR(child.GetFloat("Width", 0), 0.5f, 1e-6f);
    TEST_ASSERT(child.fontFields.count("Font") == 1);
    TEST_ASSERT(child.stringPairFields.count("AltText") == 1);
    TEST_NEAR(child.GetFloat("Height", 0), 0.25f, 1e-6f);
    TEST_ASSERT(child.innerFrames.size() == 1);
    TEST_ASSERT(child.innerFrames[0].name == "Label");
    TEST_ASSERT(child.innerFrames[0].GetString("Text", "") == "Hi");
    return true;
}

bool TestFdfBuilderSmoke() {
    auto templates = ParseTemplates(
        "Frame \"FRAME\" \"Root\" {\n"
        "    Width 0.4,\n"
        "    Height 0.3,\n"
        "    Frame \"CHECKBOX\" \"Opt\" {\n"
        "        Width 0.1,\n"
        "    }\n"
        "    Frame \"EDITBOX\" \"Name\" {\n"
        "        Font \"F\",0.01,\n"
        "    }\n"
        "    Frame \"LISTBOX\" \"List\" {\n"
        "    }\n"
        "    Frame \"GLUETEXTBUTTON\" \"Ok\" {\n"
        "        ButtonText \"OK\",\n"
        "    }\n"
        "    Frame \"POPUPMENU\" \"Menu\" {\n"
        "        MenuItem \"A\", 0,\n"
        "    }\n"
        "}\n");
    GameUI gui;
    gui.screenW = 1280;
    gui.screenH = 720;
    FrameBuilder builder(gui, nullptr, nullptr);
    TEST_ASSERT(builder.BuildFromTemplates(templates, "Root"));
    // Root + 5 children, all reachable by name.
    TEST_ASSERT(builder.GetFrame("Root") != nullptr);
    TEST_ASSERT(builder.GetFrame("Opt") != nullptr);
    TEST_ASSERT(builder.GetFrame("Name") != nullptr);
    TEST_ASSERT(builder.GetFrame("List") != nullptr);
    TEST_ASSERT(builder.GetFrame("Ok") != nullptr);
    TEST_ASSERT(builder.GetFrame("Menu") != nullptr);
    // Unknown widget types degrade to SimpleFrame instead of failing.
    TEST_ASSERT(engine::ui::DynamicCastFrame<SimpleFrame>(builder.GetFrame("Opt")) != nullptr);
    TEST_ASSERT(engine::ui::DynamicCastFrame<SimpleFrame>(builder.GetFrame("List")) != nullptr);
    TEST_ASSERT(builder.GetFrame("Missing") == nullptr);
    return true;
}

bool TestFdfRetailCorpus() {
    const std::string res = std::string(PROJECT_SOURCE_DIR) + "/resources/";
    if (!fs::exists(fs::path(PROJECT_SOURCE_DIR) / "resources" / "war3x.mpq")) return true;
    MpqManager mpq;
    for (auto a : {"War3Patch.mpq", "war3x.mpq", "war3.mpq"}) mpq.Open(res + a);
    if (mpq.ArchiveCount() == 0) return true;
    std::vector<std::string> files = mpq.ListAllFiles("UI\\FrameDef\\*.fdf");
    if (files.empty()) {
        // Fall back to a full listing (some StormLib builds need it).
        for (const auto& f : mpq.ListFiles("")) {
            if (f.size() > 4 && f.compare(f.size() - 4, 4, ".fdf") == 0 &&
                f.find("FrameDef") != std::string::npos) {
                files.push_back(f);
            }
        }
    }
    TEST_ASSERT(!files.empty());
    int parsed = 0;
    for (const auto& path : files) {
        std::vector<uint8_t> bytes;
        if (!mpq.ReadFile(path, bytes) || bytes.empty()) continue;
        std::string content((const char*)bytes.data(), bytes.size());
        try {
            FdfParser parser(content);
            parser.Parse();
            parsed++;
        } catch (const std::exception& e) {
            std::cerr << "RETAIL FDF FAIL " << path << ": " << e.what() << "\n";
            TEST_ASSERT(false);
        }
    }
    std::cerr << "Retail FDF corpus parsed: " << parsed << "/" << files.size() << "\n";
    TEST_ASSERT(parsed == (int)files.size());
    return true;
}
