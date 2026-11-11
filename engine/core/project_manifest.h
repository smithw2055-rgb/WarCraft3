#pragma once
// ============================================================================
// Engine Core: ProjectManifest — Godot `project.godot` & JSON Project Descriptor
//
// Parses & serializes both JSON (`project.json` / `war3.project.json`) and INI
// (`project.godot`-style) project descriptors with symmetric string escaping
// (preserving Windows backslash paths cleanly across ToJson/ParseJson and
// ToIni/ParseIni round-trips).
// ============================================================================
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace engine {

struct VfsMountSpec {
    std::string type = "folder";   // "folder", "mpq", "casc", "zip", "memory"
    std::string path;
    std::string mountPoint = "/";
    int priority = 0;              // Higher priority shadows lower priority
    bool optional = false;
};

struct ProjectManifest {
    std::string projectId;
    std::string title = "Engine Project";
    std::string engineVersion = "0.2.0";
    std::string modulePlugin;      // e.g., "war3_module_plugin" or "minimal_rts_module"
    std::string entryModuleId;     // e.g., "War3GameModule" or "MinimalRtsModule"
    std::string defaultScene;
    uint32_t fixedTickHz = 20;
    int windowWidth = 1280;
    int windowHeight = 720;
    std::vector<VfsMountSpec> vfsMounts;
    std::unordered_map<std::string, std::string> customSettings;

    bool IsValid() const {
        return !projectId.empty() && fixedTickHz > 0;
    }

    static bool ParseJson(std::string_view jsonText,
                          ProjectManifest& outManifest,
                          std::string* outError = nullptr);

    static bool ParseIni(std::string_view iniText,
                         ProjectManifest& outManifest,
                         std::string* outError = nullptr);

    static bool LoadFromFile(const std::string& filePath,
                             ProjectManifest& outManifest,
                             std::string* outError = nullptr);

    std::string ToJson() const;
    std::string ToIni() const;
};

namespace detail {

struct JsonValue {
    enum class Kind : uint8_t {
        Null = 0,
        Bool,
        Number,
        String,
        Array,
        Object
    };

    Kind kind = Kind::Null;
    bool boolVal = false;
    double numVal = 0.0;
    std::string strVal;
    std::vector<JsonValue> arrayItems;
    std::vector<std::pair<std::string, JsonValue>> objectFields;

    const JsonValue* FindField(std::string_view key) const {
        if (kind != Kind::Object) return nullptr;
        for (const auto& kv : objectFields) {
            if (kv.first == key) return &kv.second;
        }
        return nullptr;
    }

    std::string GetString(std::string_view key, const std::string& fallback = "") const {
        const JsonValue* f = FindField(key);
        return (f && f->kind == Kind::String) ? f->strVal : fallback;
    }

    int GetInt(std::string_view key, int fallback = 0) const {
        const JsonValue* f = FindField(key);
        return (f && f->kind == Kind::Number) ? static_cast<int>(f->numVal) : fallback;
    }

    double GetNumber(std::string_view key, double fallback = 0.0) const {
        const JsonValue* f = FindField(key);
        return (f && f->kind == Kind::Number) ? f->numVal : fallback;
    }

    bool GetBool(std::string_view key, bool fallback = false) const {
        const JsonValue* f = FindField(key);
        return (f && f->kind == Kind::Bool) ? f->boolVal : fallback;
    }
};

class MiniJsonParser {
public:
    explicit MiniJsonParser(std::string_view src) : src_(src) {}

    bool ParseRoot(JsonValue& out, std::string* outErr = nullptr) {
        SkipWs();
        if (!ParseValue(out)) {
            if (outErr) *outErr = error_;
            return false;
        }
        SkipWs();
        if (pos_ != src_.size()) {
            if (outErr) *outErr = "Trailing characters after root JSON value";
            return false;
        }
        return true;
    }

private:
    void SkipWs() {
        while (pos_ < src_.size()) {
            const char c = src_[pos_];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                ++pos_;
            } else if (c == '/' && pos_ + 1 < src_.size() && src_[pos_ + 1] == '/') {
                pos_ += 2;
                while (pos_ < src_.size() && src_[pos_] != '\n') ++pos_;
            } else {
                break;
            }
        }
    }

    bool ParseValue(JsonValue& out) {
        SkipWs();
        if (pos_ >= src_.size()) {
            error_ = "Unexpected end of JSON input";
            return false;
        }
        const char c = src_[pos_];
        if (c == '"') {
            out = JsonValue{};
            out.kind = JsonValue::Kind::String;
            return ParseString(out.strVal);
        }
        if (c == '{') return ParseObject(out);
        if (c == '[') return ParseArray(out);
        if (c == 't' && src_.substr(pos_, 4) == "true") {
            pos_ += 4;
            out = JsonValue{};
            out.kind = JsonValue::Kind::Bool;
            out.boolVal = true;
            return true;
        }
        if (c == 'f' && src_.substr(pos_, 5) == "false") {
            pos_ += 5;
            out = JsonValue{};
            out.kind = JsonValue::Kind::Bool;
            out.boolVal = false;
            return true;
        }
        if (c == 'n' && src_.substr(pos_, 4) == "null") {
            pos_ += 4;
            out = JsonValue{};
            out.kind = JsonValue::Kind::Null;
            return true;
        }
        if (c == '-' || (c >= '0' && c <= '9')) {
            return ParseNumber(out);
        }
        error_ = "Invalid JSON token";
        return false;
    }

    bool ParseString(std::string& out) {
        if (pos_ >= src_.size() || src_[pos_] != '"') return false;
        ++pos_;
        out.clear();
        while (pos_ < src_.size()) {
            const char c = src_[pos_++];
            if (c == '"') return true;
            if (c == '\\') {
                if (pos_ >= src_.size()) {
                    error_ = "Unterminated escape in JSON string";
                    return false;
                }
                const char esc = src_[pos_++];
                switch (esc) {
                    case '"':  out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/':  out.push_back('/'); break;
                    case 'b':  out.push_back('\b'); break;
                    case 'f':  out.push_back('\f'); break;
                    case 'n':  out.push_back('\n'); break;
                    case 'r':  out.push_back('\r'); break;
                    case 't':  out.push_back('\t'); break;
                    default:   out.push_back(esc); break;
                }
            } else {
                out.push_back(c);
            }
        }
        error_ = "Unterminated JSON string";
        return false;
    }

    bool ParseNumber(JsonValue& out) {
        const size_t start = pos_;
        if (src_[pos_] == '-') ++pos_;
        while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_]))) ++pos_;
        if (pos_ < src_.size() && src_[pos_] == '.') {
            ++pos_;
            while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_]))) ++pos_;
        }
        if (pos_ < src_.size() && (src_[pos_] == 'e' || src_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < src_.size() && (src_[pos_] == '+' || src_[pos_] == '-')) ++pos_;
            while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_]))) ++pos_;
        }
        const std::string token(src_.substr(start, pos_ - start));
        char* endPtr = nullptr;
        const double v = std::strtod(token.c_str(), &endPtr);
        if (!endPtr || *endPtr != '\0') {
            error_ = "Malformed JSON number";
            return false;
        }
        out = JsonValue{};
        out.kind = JsonValue::Kind::Number;
        out.numVal = v;
        return true;
    }

    bool ParseArray(JsonValue& out) {
        ++pos_; // '['
        out = JsonValue{};
        out.kind = JsonValue::Kind::Array;
        SkipWs();
        if (pos_ < src_.size() && src_[pos_] == ']') {
            ++pos_;
            return true;
        }
        while (pos_ < src_.size()) {
            JsonValue elem;
            if (!ParseValue(elem)) return false;
            out.arrayItems.push_back(std::move(elem));
            SkipWs();
            if (pos_ < src_.size() && src_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (pos_ < src_.size() && src_[pos_] == ']') {
                ++pos_;
                return true;
            }
            error_ = "Expected ',' or ']' in JSON array";
            return false;
        }
        error_ = "Unterminated JSON array";
        return false;
    }

    bool ParseObject(JsonValue& out) {
        ++pos_; // '{'
        out = JsonValue{};
        out.kind = JsonValue::Kind::Object;
        SkipWs();
        if (pos_ < src_.size() && src_[pos_] == '}') {
            ++pos_;
            return true;
        }
        while (pos_ < src_.size()) {
            SkipWs();
            std::string key;
            if (!ParseString(key)) {
                if (error_.empty()) error_ = "Expected string key in JSON object";
                return false;
            }
            SkipWs();
            if (pos_ >= src_.size() || src_[pos_] != ':') {
                error_ = "Expected ':' after JSON object key";
                return false;
            }
            ++pos_;
            JsonValue val;
            if (!ParseValue(val)) return false;
            out.objectFields.emplace_back(std::move(key), std::move(val));
            SkipWs();
            if (pos_ < src_.size() && src_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (pos_ < src_.size() && src_[pos_] == '}') {
                ++pos_;
                return true;
            }
            error_ = "Expected ',' or '}' in JSON object";
            return false;
        }
        error_ = "Unterminated JSON object";
        return false;
    }

    std::string_view src_;
    size_t pos_ = 0;
    std::string error_;
};

inline std::string EscapeQuotedString(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out.push_back(c); break;
        }
    }
    return out;
}

inline std::string UnescapeQuotedString(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (c == '\\' && i + 1 < s.size()) {
            const char esc = s[++i];
            switch (esc) {
                case '"':  out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case 'n':  out.push_back('\n'); break;
                case 'r':  out.push_back('\r'); break;
                case 't':  out.push_back('\t'); break;
                default:   out.push_back(esc); break;
            }
        } else {
            out.push_back(c);
        }
    }
    return out;
}

inline std::string TrimAscii(std::string_view sv) {
    size_t b = 0;
    size_t e = sv.size();
    while (b < e && std::isspace(static_cast<unsigned char>(sv[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(sv[e - 1]))) --e;
    return std::string(sv.substr(b, e - b));
}

} // namespace detail

inline bool ProjectManifest::ParseJson(std::string_view jsonText,
                                       ProjectManifest& outManifest,
                                       std::string* outError) {
    detail::MiniJsonParser parser(jsonText);
    detail::JsonValue root;
    if (!parser.ParseRoot(root, outError)) {
        return false;
    }
    if (root.kind != detail::JsonValue::Kind::Object) {
        if (outError) *outError = "Root JSON manifest must be an object";
        return false;
    }

    ProjectManifest m;
    m.projectId = root.GetString("projectId", root.GetString("project_id", ""));
    m.title = root.GetString("title", "Engine Project");
    m.engineVersion = root.GetString("engineVersion", root.GetString("engine_version", "0.2.0"));
    m.modulePlugin = root.GetString("modulePlugin", root.GetString("module_plugin", ""));
    m.entryModuleId = root.GetString("entryModuleId", root.GetString("entry_module_id", ""));
    m.defaultScene = root.GetString("defaultScene", root.GetString("default_scene", ""));
    m.fixedTickHz = static_cast<uint32_t>(std::max(1, root.GetInt("fixedTickHz", root.GetInt("fixed_tick_hz", 20))));
    m.windowWidth = std::max(64, root.GetInt("windowWidth", root.GetInt("window_width", 1280)));
    m.windowHeight = std::max(64, root.GetInt("windowHeight", root.GetInt("window_height", 720)));

    if (const auto* mounts = root.FindField("vfsMounts")) {
        if (mounts->kind == detail::JsonValue::Kind::Array) {
            for (const auto& item : mounts->arrayItems) {
                if (item.kind != detail::JsonValue::Kind::Object) continue;
                VfsMountSpec spec;
                spec.type = item.GetString("type", "folder");
                spec.path = item.GetString("path", "");
                spec.mountPoint = item.GetString("mountPoint", "/");
                spec.priority = item.GetInt("priority", 0);
                spec.optional = item.GetBool("optional", false);
                if (!spec.path.empty()) {
                    m.vfsMounts.push_back(std::move(spec));
                }
            }
        }
    }

    if (const auto* settings = root.FindField("customSettings")) {
        if (settings->kind == detail::JsonValue::Kind::Object) {
            for (const auto& kv : settings->objectFields) {
                if (kv.second.kind == detail::JsonValue::Kind::String) {
                    m.customSettings[kv.first] = kv.second.strVal;
                } else if (kv.second.kind == detail::JsonValue::Kind::Number) {
                    m.customSettings[kv.first] = std::to_string(static_cast<int64_t>(kv.second.numVal));
                } else if (kv.second.kind == detail::JsonValue::Kind::Bool) {
                    m.customSettings[kv.first] = kv.second.boolVal ? "true" : "false";
                }
            }
        }
    }

    if (!m.IsValid()) {
        if (outError) *outError = "ProjectManifest requires non-empty projectId";
        return false;
    }
    outManifest = std::move(m);
    return true;
}

inline bool ProjectManifest::ParseIni(std::string_view iniText,
                                      ProjectManifest& outManifest,
                                      std::string* outError) {
    ProjectManifest m;
    std::string section;
    std::unordered_map<int, VfsMountSpec> indexedMounts;

    std::istringstream iss{std::string(iniText)};
    std::string rawLine;
    while (std::getline(iss, rawLine)) {
        std::string line = detail::TrimAscii(rawLine);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        if (line.front() == '[' && line.back() == ']') {
            section = detail::TrimAscii(std::string_view(line).substr(1, line.size() - 2));
            continue;
        }

        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = detail::TrimAscii(std::string_view(line).substr(0, eq));
        std::string val = detail::TrimAscii(std::string_view(line).substr(eq + 1));

        // Symmetric unescaping for quoted INI string values (fixes Windows backslash round-trip!)
        if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
            val = detail::UnescapeQuotedString(std::string_view(val).substr(1, val.size() - 2));
        }

        if (section == "application" || section.empty()) {
            if (key == "project_id" || key == "projectId") m.projectId = val;
            else if (key == "config/name" || key == "title") m.title = val;
            else if (key == "config/engine_version" || key == "engineVersion") m.engineVersion = val;
            else if (key == "run/main_scene" || key == "defaultScene") m.defaultScene = val;
            else if (key == "module/plugin" || key == "modulePlugin") m.modulePlugin = val;
            else if (key == "module/entry_id" || key == "entryModuleId") m.entryModuleId = val;
        } else if (section == "physics" || section == "simulation") {
            if (key == "common/physics_ticks_per_second" || key == "fixedTickHz") {
                m.fixedTickHz = static_cast<uint32_t>(std::max(1, std::atoi(val.c_str())));
            }
        } else if (section == "display") {
            if (key == "window/size/viewport_width" || key == "windowWidth") {
                m.windowWidth = std::max(64, std::atoi(val.c_str()));
            } else if (key == "window/size/viewport_height" || key == "windowHeight") {
                m.windowHeight = std::max(64, std::atoi(val.c_str()));
            }
        } else if (section.rfind("vfs_mount.", 0) == 0) {
            const int idx = std::atoi(section.c_str() + 10);
            VfsMountSpec& spec = indexedMounts[idx];
            if (key == "type") spec.type = val;
            else if (key == "path") spec.path = val;
            else if (key == "mountPoint" || key == "mount_point") spec.mountPoint = val;
            else if (key == "priority") spec.priority = std::atoi(val.c_str());
            else if (key == "optional") spec.optional = (val == "true" || val == "1");
        } else if (section == "custom") {
            m.customSettings[key] = val;
        }
    }

    std::vector<int> mountIndices;
    mountIndices.reserve(indexedMounts.size());
    for (const auto& kv : indexedMounts) mountIndices.push_back(kv.first);
    std::sort(mountIndices.begin(), mountIndices.end());
    for (int idx : mountIndices) {
        if (!indexedMounts[idx].path.empty()) {
            m.vfsMounts.push_back(std::move(indexedMounts[idx]));
        }
    }

    if (!m.IsValid()) {
        if (outError) *outError = "INI ProjectManifest missing project_id";
        return false;
    }
    outManifest = std::move(m);
    return true;
}

inline bool ProjectManifest::LoadFromFile(const std::string& filePath,
                                          ProjectManifest& outManifest,
                                          std::string* outError) {
    std::ifstream ifs(filePath, std::ios::binary);
    if (!ifs) {
        if (outError) *outError = "Unable to open manifest file: " + filePath;
        return false;
    }
    std::ostringstream ss;
    ss << ifs.rdbuf();
    const std::string content = ss.str();

    const std::string trimmed = detail::TrimAscii(content);
    if (!trimmed.empty() && trimmed.front() == '{') {
        return ParseJson(content, outManifest, outError);
    }
    return ParseIni(content, outManifest, outError);
}

inline std::string ProjectManifest::ToJson() const {
    std::ostringstream os;
    os << "{\n";
    os << "  \"projectId\": \"" << detail::EscapeQuotedString(projectId) << "\",\n";
    os << "  \"title\": \"" << detail::EscapeQuotedString(title) << "\",\n";
    os << "  \"engineVersion\": \"" << detail::EscapeQuotedString(engineVersion) << "\",\n";
    os << "  \"modulePlugin\": \"" << detail::EscapeQuotedString(modulePlugin) << "\",\n";
    os << "  \"entryModuleId\": \"" << detail::EscapeQuotedString(entryModuleId) << "\",\n";
    os << "  \"defaultScene\": \"" << detail::EscapeQuotedString(defaultScene) << "\",\n";
    os << "  \"fixedTickHz\": " << fixedTickHz << ",\n";
    os << "  \"windowWidth\": " << windowWidth << ",\n";
    os << "  \"windowHeight\": " << windowHeight << ",\n";
    os << "  \"vfsMounts\": [\n";
    for (size_t i = 0; i < vfsMounts.size(); ++i) {
        const auto& m = vfsMounts[i];
        os << "    {\n";
        os << "      \"type\": \"" << detail::EscapeQuotedString(m.type) << "\",\n";
        os << "      \"path\": \"" << detail::EscapeQuotedString(m.path) << "\",\n";
        os << "      \"mountPoint\": \"" << detail::EscapeQuotedString(m.mountPoint) << "\",\n";
        os << "      \"priority\": " << m.priority << ",\n";
        os << "      \"optional\": " << (m.optional ? "true" : "false") << "\n";
        os << "    }" << (i + 1 < vfsMounts.size() ? "," : "") << "\n";
    }
    os << "  ],\n";
    os << "  \"customSettings\": {\n";
    std::vector<std::string> keys;
    keys.reserve(customSettings.size());
    for (const auto& kv : customSettings) keys.push_back(kv.first);
    std::sort(keys.begin(), keys.end());
    for (size_t i = 0; i < keys.size(); ++i) {
        const auto& k = keys[i];
        os << "    \"" << detail::EscapeQuotedString(k) << "\": \""
           << detail::EscapeQuotedString(customSettings.at(k)) << "\""
           << (i + 1 < keys.size() ? "," : "") << "\n";
    }
    os << "  }\n";
    os << "}\n";
    return os.str();
}

inline std::string ProjectManifest::ToIni() const {
    std::ostringstream os;
    os << "; Engine Project Manifest (Godot-style INI)\n";
    os << "[application]\n";
    os << "project_id=\"" << detail::EscapeQuotedString(projectId) << "\"\n";
    os << "config/name=\"" << detail::EscapeQuotedString(title) << "\"\n";
    os << "config/engine_version=\"" << detail::EscapeQuotedString(engineVersion) << "\"\n";
    os << "module/plugin=\"" << detail::EscapeQuotedString(modulePlugin) << "\"\n";
    os << "module/entry_id=\"" << detail::EscapeQuotedString(entryModuleId) << "\"\n";
    os << "run/main_scene=\"" << detail::EscapeQuotedString(defaultScene) << "\"\n\n";

    os << "[simulation]\n";
    os << "common/physics_ticks_per_second=" << fixedTickHz << "\n\n";

    os << "[display]\n";
    os << "window/size/viewport_width=" << windowWidth << "\n";
    os << "window/size/viewport_height=" << windowHeight << "\n\n";

    for (size_t i = 0; i < vfsMounts.size(); ++i) {
        const auto& m = vfsMounts[i];
        os << "[vfs_mount." << i << "]\n";
        os << "type=\"" << detail::EscapeQuotedString(m.type) << "\"\n";
        os << "path=\"" << detail::EscapeQuotedString(m.path) << "\"\n";
        os << "mount_point=\"" << detail::EscapeQuotedString(m.mountPoint) << "\"\n";
        os << "priority=" << m.priority << "\n";
        os << "optional=" << (m.optional ? "true" : "false") << "\n\n";
    }

    if (!customSettings.empty()) {
        os << "[custom]\n";
        std::vector<std::string> keys;
        keys.reserve(customSettings.size());
        for (const auto& kv : customSettings) keys.push_back(kv.first);
        std::sort(keys.begin(), keys.end());
        for (const auto& k : keys) {
            os << k << "=\"" << detail::EscapeQuotedString(customSettings.at(k)) << "\"\n";
        }
    }
    return os.str();
}

} // namespace engine
