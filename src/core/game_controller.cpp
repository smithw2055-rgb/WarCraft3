#include "game_controller.h"
#include "../render/renderer.h"
#include "../render/mdx_event_splat.h"
#include "../render/mdx_sequence_select.h"
#include "../render/picking.h"
#include "../render/unit_selection_geom.h"
#include "../ecs/animation_utils.h"
#include "../ecs/ability_factory.h"
#include "../ecs/unit_text_utils.h"
#include "../ecs/orders/order.h"
#include "../ecs/widget.h"
#include "../ecs/destructable.h"
#include "../ecs/item.h"
#include "../ecs/abilities/ability_attack.h"
#include "../ecs/abilities/ability_build.h"
#include "../ecs/abilities/ability_gold_mine.h"
#include "../ecs/abilities/ability_harvest.h"
#include "../ecs/abilities/ability_hero.h"
#include "../ecs/abilities/ability_inventory.h"
#include "../ecs/abilities/ability_move.h"
#include "../ecs/abilities/ability_queue.h"
#include "../ecs/abilities/ability_repair.h"
#include "../core/map_path_resolver.h"
#include "../core/path_utils.h"
#include "../platform/platform.h"
#include "../debug/move_debug.h"
#include "../asset/stb_image_write.h"
#include "../map/w3i.h"
#include "../map/wts.h"
#include "../resource/slk.h"
#include "../third_party/sokol/sokol_app.h"
#include "../third_party/sokol/sokol_gfx.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifndef WITHOUT_STORMLIB
#include <StormLib.h>
#endif

namespace fs = std::filesystem;

static const char* DEFAULT_MAP = "Maps/FrozenThrone/(4)Avalanche.w3x";

static std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    return value;
}

static std::string TrimUiOnlyMapString(std::string value) {
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

static int UiOnlyPlayerSlotsFromFileName(const std::string& fileName) {
    if (fileName.size() <= 3 || fileName[0] != '(') return 0;
    const size_t close = fileName.find(')');
    if (close == std::string::npos || close <= 1) return 0;
    return std::max(0, std::atoi(fileName.substr(1, close - 1).c_str()));
}

static const char* UiOnlyTilesetName(char tileset) {
    switch (tileset) {
    case 'A': return "Ashenvale";
    case 'B': return "Barrens";
    case 'C': return "Felwood";
    case 'D': return "Dungeon";
    case 'F': return "Lordaeron Fall";
    case 'G': return "Underground";
    case 'I': return "Icecrown";
    case 'J': return "Dalaran Ruins";
    case 'K': return "Black Citadel";
    case 'L': return "Lordaeron Summer";
    case 'N': return "Northrend";
    case 'O': return "Outland";
    case 'Q': return "Village Fall";
    case 'V': return "Village";
    case 'W': return "Lordaeron Winter";
    case 'X': return "Dalaran";
    case 'Y': return "Cityscape";
    case 'Z': return "Sunken Ruins";
    default: return "Unknown";
    }
}

static std::string UiOnlyMapSizeLabel(int width, int height) {
    if (width <= 0 || height <= 0) return {};
    return std::to_string(width) + " x " + std::to_string(height);
}

static std::string UiOnlySuggestedPlayersLabel(const std::string& value) {
    std::string cleaned = TrimUiOnlyMapString(value);
    if (cleaned.empty()) return {};
    const std::string lower = LowerAscii(cleaned);
    if (lower.rfind("suggested", 0) == 0 || lower.rfind("players", 0) == 0) {
        return cleaned;
    }
    return "Suggested Players: " + cleaned;
}

static bool ReadUiOnlyMapArchiveFile(const fs::path& mapPath, const char* fileName, std::vector<uint8_t>& out) {
#ifndef WITHOUT_STORMLIB
    HANDLE hMpq = nullptr;
    if (!SFileOpenArchive(mapPath.string().c_str(), 0, MPQ_OPEN_READ_ONLY, &hMpq)) return false;

    HANDLE hFile = nullptr;
    if (!SFileOpenFileEx(hMpq, fileName, SFILE_OPEN_BASE_FILE, &hFile)) {
        SFileCloseArchive(hMpq);
        return false;
    }

    DWORD sz = SFileGetFileSize(hFile, nullptr);
    out.resize(sz);
    DWORD rd = 0;
    const bool ok = SFileReadFile(hFile, out.data(), sz, &rd, nullptr) && rd == sz;
    SFileCloseFile(hFile);
    SFileCloseArchive(hMpq);
    return ok;
#else
    (void)mapPath;
    (void)fileName;
    (void)out;
    return false;
#endif
}

static std::string ResolveUiOnlyTrigString(const WtsTable* wts, const std::string& value) {
    if (!wts || value.empty()) return TrimUiOnlyMapString(value);
    return TrimUiOnlyMapString(wts->Resolve(value));
}

static bool IsUiOnlyTrigStringRef(const std::string& value) {
    if (value.size() <= 8) return false;
    std::string prefix = value.substr(0, 8);
    std::transform(prefix.begin(), prefix.end(), prefix.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    if (prefix != "trigstr_") return false;
    return std::all_of(value.begin() + 8, value.end(), [](unsigned char c) {
        return std::isdigit(c) != 0;
    });
}

static std::string CleanUiOnlyResolvedMapString(const std::string& value) {
    std::string cleaned = TrimUiOnlyMapString(value);
    return IsUiOnlyTrigStringRef(cleaned) ? std::string() : cleaned;
}

static void PopulateUiOnlyGlueMapInfo(const fs::path& mapPath, War3UI::GlueMapListItem& item) {
    W3iInfo info;
    if (!W3ILoadFromMpq(mapPath.string(), info)) {
        return;
    }

    WtsTable wts;
    WtsTable* wtsPtr = nullptr;
    std::vector<uint8_t> wtsBytes;
    if (ReadUiOnlyMapArchiveFile(mapPath, "war3map.wts", wtsBytes) && wts.Load(wtsBytes.data(), wtsBytes.size())) {
        wtsPtr = &wts;
    }

    item.displayName = CleanUiOnlyResolvedMapString(ResolveUiOnlyTrigString(wtsPtr, info.name));
    item.description = CleanUiOnlyResolvedMapString(ResolveUiOnlyTrigString(wtsPtr, info.description));
    item.author = CleanUiOnlyResolvedMapString(ResolveUiOnlyTrigString(wtsPtr, info.author));
    item.recommendedPlayers = UiOnlySuggestedPlayersLabel(
        CleanUiOnlyResolvedMapString(ResolveUiOnlyTrigString(wtsPtr, info.recommendedPlayers)));
    item.mapSize = UiOnlyMapSizeLabel(info.playableWidth, info.playableHeight);
    item.tileset = UiOnlyTilesetName(info.tileset);
    item.playerSlots = info.playableWidth > 0 ? std::max(item.playerSlots, UiOnlyPlayerSlotsFromFileName(item.fileName))
                                               : UiOnlyPlayerSlotsFromFileName(item.fileName);
}

static std::vector<War3UI::GlueMapListItem> DiscoverUiOnlyGlueMaps(const fs::path& projectRoot) {
    std::vector<War3UI::GlueMapListItem> preferred;
    std::vector<War3UI::GlueMapListItem> fallback;
    const fs::path mapsRoot = projectRoot / "resources" / "Maps";
    if (!fs::exists(mapsRoot)) {
        return {};
    }

    std::error_code ec;
    for (fs::recursive_directory_iterator it(mapsRoot, fs::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        const fs::path path = it->path();
        const std::string ext = LowerAscii(path.extension().string());
        if (ext != ".w3x" && ext != ".w3m") continue;

        fs::path relative = fs::relative(path, mapsRoot, ec);
        if (ec) {
            ec.clear();
            relative = path.filename();
        }
        War3UI::GlueMapListItem item;
        item.path = (fs::path("Maps") / relative).generic_string();
        item.fileName = path.filename().string();
        item.playerSlots = UiOnlyPlayerSlotsFromFileName(item.fileName);
        PopulateUiOnlyGlueMapInfo(path, item);
        fallback.push_back(item);

        const std::string relLower = LowerAscii(relative.generic_string());
        if (relLower.rfind("frozenthrone/", 0) == 0 && relLower.find("/scenario/") == std::string::npos) {
            preferred.push_back(item);
        }
    }

    auto byDisplayName = [](const War3UI::GlueMapListItem& a, const War3UI::GlueMapListItem& b) {
        return LowerAscii(a.fileName) < LowerAscii(b.fileName);
    };
    std::sort(preferred.begin(), preferred.end(), byDisplayName);
    std::sort(fallback.begin(), fallback.end(), byDisplayName);
    return preferred.empty() ? fallback : preferred;
}

static std::string StripUiOnlyCampaignToken(std::string value) {
    value = TrimUiOnlyMapString(value);
    if (value.size() >= 2) {
        const char first = value.front();
        const char last = value.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            value = value.substr(1, value.size() - 2);
        }
    }
    return TrimUiOnlyMapString(value);
}

static std::vector<std::string> SplitUiOnlyCampaignCsv(const std::string& value) {
    std::vector<std::string> fields;
    std::string field;
    bool quoted = false;
    char quoteChar = 0;
    for (char c : value) {
        if ((c == '"' || c == '\'') && (!quoted || c == quoteChar)) {
            quoted = !quoted;
            quoteChar = quoted ? c : 0;
            field.push_back(c);
            continue;
        }
        if (c == ',' && !quoted) {
            fields.push_back(StripUiOnlyCampaignToken(field));
            field.clear();
            continue;
        }
        field.push_back(c);
    }
    fields.push_back(StripUiOnlyCampaignToken(field));
    return fields;
}

static std::string StripUiOnlySkinValue(std::string value) {
    const size_t comment = value.find("//");
    if (comment != std::string::npos) {
        value.resize(comment);
    }
    return StripUiOnlyCampaignToken(value);
}

static std::string ResolveUiOnlySkinField(const MpqManager& mpq,
                                          const std::string& fieldName,
                                          int gameVersion) {
    std::vector<uint8_t> bytes;
    if (!mpq.ReadFile("UI\\war3skins.txt", bytes)) {
        return {};
    }

    const std::string versionedField = fieldName + "_V" + std::to_string(std::max(0, gameVersion));
    const std::string betaField = versionedField + "Beta";
    std::string betaValue;
    std::string baseValue;

    std::istringstream stream(std::string(bytes.begin(), bytes.end()));
    std::string line;
    while (std::getline(stream, line)) {
        line = TrimUiOnlyMapString(line);
        if (line.empty() || line.rfind("//", 0) == 0 || line.front() == '[') {
            continue;
        }

        const size_t equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }

        const std::string key = TrimUiOnlyMapString(line.substr(0, equals));
        const std::string value = StripUiOnlySkinValue(line.substr(equals + 1));
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

static void AddUiOnlyCandidatePath(std::vector<std::string>& paths, const std::string& path) {
    if (path.empty()) {
        return;
    }
    if (std::find(paths.begin(), paths.end(), path) == paths.end()) {
        paths.push_back(path);
    }
}

static std::string UiOnlyMdxPathFromModelField(std::string path) {
    path = StripUiOnlySkinValue(path);
    const std::string lower = LowerAscii(path);
    if (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".mdl") {
        path.resize(path.size() - 4);
        path += ".mdx";
    }
    return path;
}

static Vec3 UiOnlyMdxPointToYUp(const float value[3]) {
    return Vec3(value[0], value[2], -value[1]);
}

static int UiOnlySelectStandSequenceIndex(const MdxModel& model) {
    int sequenceIndex = MdxSelectSequenceIndexByPrimaryAndTags(model, "stand", {});
    if (sequenceIndex >= 0) {
        return sequenceIndex;
    }
    for (int i = 0; i < (int)model.sequences.size(); ++i) {
        const MdxSequence& sequence = model.sequences[(size_t)i];
        if (sequence.endFrame > sequence.startFrame) {
            return i;
        }
    }
    return -1;
}

static std::vector<War3UI::GlueCampaignListItem> LoadUiOnlyGlueCampaigns(const MpqManager& mpq,
                                                                         int gameVersion) {
    std::vector<uint8_t> bytes;
    std::string loadedPath;
    std::vector<std::string> candidatePaths;
    const std::string skinCampaignFile = ResolveUiOnlySkinField(mpq, "CampaignFile", gameVersion);
    AddUiOnlyCandidatePath(candidatePaths, skinCampaignFile);
    if (gameVersion >= 1) {
        AddUiOnlyCandidatePath(candidatePaths, "UI\\CampaignStrings_exp.txt");
    }
    AddUiOnlyCandidatePath(candidatePaths, "UI\\CampaignStrings.txt");
    AddUiOnlyCandidatePath(candidatePaths, "UI\\CampaignInfoClassic.txt");
    AddUiOnlyCandidatePath(candidatePaths, "UI\\CampaignInfo.txt");

    if (!skinCampaignFile.empty()) {
        std::cout << "UI-only mode: skin CampaignFile resolved to " << skinCampaignFile
                  << " for gameVersion=" << gameVersion << std::endl;
    }

    for (const std::string& path : candidatePaths) {
        if (mpq.ReadFile(path, bytes)) {
            loadedPath = path;
            break;
        }
    }
    if (bytes.empty()) {
        return {};
    }

    TxtTable table;
    if (!table.Load(bytes.data(), bytes.size())) {
        return {};
    }

    std::vector<War3UI::GlueCampaignListItem> campaigns;
    const std::vector<std::string> ids = SplitUiOnlyCampaignCsv(table.GetString("Index", "CampaignList"));
    for (const std::string& id : ids) {
        if (id.empty() || !table.HasSection(id)) continue;

        War3UI::GlueCampaignListItem campaign;
        campaign.id = id;
        campaign.header = StripUiOnlyCampaignToken(table.GetString(id, "Header"));
        campaign.name = StripUiOnlyCampaignToken(table.GetString(id, "Name"));

        for (int missionIndex = 0;; ++missionIndex) {
            const std::string raw = table.GetString(id, "Mission" + std::to_string(missionIndex));
            std::vector<std::string> fields = SplitUiOnlyCampaignCsv(raw);
            if (fields.empty() || fields[0].empty()) break;

            War3UI::GlueCampaignMission mission;
            mission.header = fields.size() > 0 ? fields[0] : "";
            mission.name = fields.size() > 1 ? fields[1] : "";
            mission.mapPath = fields.size() > 2 ? fields[2] : "";
            campaign.missions.push_back(mission);
        }

        if (!campaign.header.empty() || !campaign.name.empty() || !campaign.missions.empty()) {
            campaigns.push_back(campaign);
        }
    }

    if (!loadedPath.empty()) {
        std::cout << "UI-only mode: loaded " << campaigns.size()
                  << " campaign entries from " << loadedPath << std::endl;
    }
    return campaigns;
}

#ifndef GL_RGBA
#define GL_RGBA 0x1908
#endif
#ifndef GL_UNSIGNED_BYTE
#define GL_UNSIGNED_BYTE 0x1401
#endif
#ifndef GL_PACK_ALIGNMENT
#define GL_PACK_ALIGNMENT 0x0D05
#endif
#ifndef APIENTRY
#ifdef _WIN32
#define APIENTRY __stdcall
#else
#define APIENTRY
#endif
#endif

#ifdef _WIN32
extern "C" __declspec(dllimport) void APIENTRY glReadPixels(int x, int y, int width, int height, unsigned int format, unsigned int type, void* data);
extern "C" __declspec(dllimport) void APIENTRY glPixelStorei(unsigned int pname, int param);
#else
extern "C" void glReadPixels(int x, int y, int width, int height, unsigned int format, unsigned int type, void* data);
extern "C" void glPixelStorei(unsigned int pname, int param);
#endif

static ScreenRect FallbackMinimapRect(int screenW, int screenH) {
    if (screenW <= 0 || screenH <= 0) return {};
    const float w = std::clamp((float)screenW * 0.105f, 190.0f, 360.0f);
    const float h = std::clamp((float)screenH * 0.38f, 118.0f, 220.0f);
    return ScreenRect{20.0f, (float)screenH - h - 24.0f, w, h};
}

static Vec4 MdxEventSplatTint(const std::array<std::array<float, 4>, 3>& colors,
                              const std::array<float, 3>& intervalTimes,
                              int intervalCount,
                              float lifeSpanValue,
                              float lifeRemaining) {
    auto normalizeColor = [](const std::array<float, 4>& color) {
        auto channel = [](float value) {
            if (!std::isfinite(value)) {
                return 1.0f;
            }
            return std::clamp(value / 255.0f, 0.0f, 1.0f);
        };
        return Vec4(channel(color[0]), channel(color[1]), channel(color[2]), channel(color[3]));
    };
    auto lerp = [](const Vec4& a, const Vec4& b, float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return Vec4(a.x + (b.x - a.x) * t,
                    a.y + (b.y - a.y) * t,
                    a.z + (b.z - a.z) * t,
                    a.w + (b.w - a.w) * t);
    };

    const Vec4 start = normalizeColor(colors[0]);
    const Vec4 middle = normalizeColor(colors[1]);
    const Vec4 end = normalizeColor(colors[2]);
    const float lifeSpan = std::max(lifeSpanValue, 0.0001f);
    const float age = std::clamp(lifeSpan - lifeRemaining, 0.0f, lifeSpan);

    if (intervalCount >= 3) {
        const float birth = std::max(0.0f, intervalTimes[0]);
        const float pause = std::max(0.0f, intervalTimes[1]);
        const float decay = std::max(0.0f, intervalTimes[2]);
        if (birth > 0.0001f && age < birth) {
            return lerp(start, middle, age / birth);
        }
        if (age < birth + pause || decay <= 0.0001f) {
            return middle;
        }
        return lerp(middle, end, (age - birth - pause) / decay);
    }

    if (intervalCount >= 2) {
        const float sustain = std::max(0.0f, intervalTimes[0]);
        const float decay = std::max(0.0f, intervalTimes[1]);
        if (sustain > 0.0001f && age < sustain) {
            return lerp(start, middle, age / sustain);
        }
        if (decay <= 0.0001f) {
            return middle;
        }
        return lerp(middle, end, (age - sustain) / decay);
    }

    return start;
}

static float MdxEventSplatCell(const std::array<std::array<float, 3>, 2>& uvIntervals,
                               const std::array<float, 3>& intervalTimes,
                               int intervalCount,
                               int columns,
                               int rows,
                               float lifeSpanValue,
                               float lifeRemaining) {
    if (columns <= 1 && rows <= 1) {
        return 0.0f;
    }
    const float lifeSpan = std::max(lifeSpanValue, 0.0001f);
    const float age = std::clamp(lifeSpan - lifeRemaining, 0.0f, lifeSpan);
    int intervalIndex = 0;
    float factor = 0.0f;
    if (intervalCount >= 2) {
        const float sustain = std::max(0.0f, intervalTimes[0]);
        const float decay = std::max(0.0f, intervalTimes[1]);
        if (sustain > 0.0001f && age < sustain) {
            factor = age / sustain;
            intervalIndex = 0;
        } else {
            factor = decay > 0.0001f ? (age - sustain) / decay : 1.0f;
            intervalIndex = 1;
        }
    }

    const auto& interval = uvIntervals[intervalIndex];
    const float start = interval[0];
    const float end = interval[1];
    const float repeat = interval[2];
    const float spriteCount = end - start;
    const float maxCell = (float)std::max(0, columns * rows - 1);
    if (spriteCount > 0.0f) {
        const float advanced = std::fmod(std::floor(spriteCount * repeat * std::clamp(factor, 0.0f, 1.0f)),
                                         spriteCount);
        return std::clamp(start + advanced, 0.0f, maxCell);
    }
    return std::clamp(start, 0.0f, maxCell);
}

static Vec4 MdxEventSplatUvRect(const std::array<std::array<float, 3>, 2>& uvIntervals,
                                const std::array<float, 3>& intervalTimes,
                                int intervalCount,
                                int columns,
                                int rows,
                                float lifeSpanValue,
                                float lifeRemaining) {
    columns = std::max(1, columns);
    rows = std::max(1, rows);
    const float cell = MdxEventSplatCell(uvIntervals, intervalTimes, intervalCount,
                                         columns, rows, lifeSpanValue, lifeRemaining);
    const float left = std::floor(std::fmod(cell, (float)columns)) / (float)columns;
    const float top = std::floor(cell / (float)columns) / (float)rows;
    return Vec4(left, top, left + (1.0f / (float)columns), top + (1.0f / (float)rows));
}

static SplatModel::BlendMode MdxEventSplatBlendMode(int blendMode) {
    switch (blendMode) {
    case 1:
        return SplatModel::BlendMode::AdditiveAlpha;
    case 2:
        return SplatModel::BlendMode::Modulate;
    case 3:
        return SplatModel::BlendMode::Modulate2x;
    case 4:
        return SplatModel::BlendMode::Blend;
    case 0:
    default:
        return SplatModel::BlendMode::Blend;
    }
}

// --- Helpers (animation / visual) ---

static bool CurrentAnimationRequestMatchesViewer(MdxViewer* viewer,
                                                 const animation_utils::PendingUnitAnimation& request) {
    if (!viewer || !viewer->GetAnimator()) return false;
    if (request.useTags) {
        return viewer->IsPlayingSequenceByPrimaryAndTags(request.primaryTag.c_str(), request.secondaryTags);
    }
    const int currentSequence = viewer->GetAnimator()->GetCurrentSequence();
    if (request.useSequenceIndex) {
        return currentSequence == request.sequenceIndex;
    }
    if (request.sequenceName.empty()) {
        return false;
    }
    std::string currentName;
    if (!viewer->GetSequenceName(currentSequence, currentName)) {
        return false;
    }
    const std::string requested = LowerAscii(request.sequenceName);
    const std::string current = LowerAscii(currentName);
    return current.find(requested) != std::string::npos;
}

static bool ApplyAnimationRequestToViewer(int handleId,
                                          MdxViewer* viewer,
                                          const animation_utils::PendingUnitAnimation& request,
                                          GameController* g) {
    if (!viewer || !viewer->GetAnimator()) return false;
    if (!request.force && CurrentAnimationRequestMatchesViewer(viewer, request)) return true;

    bool loop = false;
    bool queueStandAfterCurrent = true;
    std::string lowerName;
    if (request.useTags) {
        lowerName = request.primaryTag;
        for (const std::string& tag : request.secondaryTags) {
            lowerName += " ";
            lowerName += tag;
        }
        animation_utils::ClassifyScriptedAnimationName(lowerName, loop, queueStandAfterCurrent);
    } else if (animation_utils::GetRequestedAnimationLowerName(viewer, request, lowerName)) {
        animation_utils::ClassifyScriptedAnimationName(lowerName, loop, queueStandAfterCurrent);
    }

    bool played = false;
    const float speedRatio = std::clamp(request.speedRatio, 0.1f, 4.0f);
    if (request.useTags) {
        if (!request.force && viewer->IsPlayingSequenceByPrimaryAndTags(request.primaryTag.c_str(), request.secondaryTags)) {
            played = true;
        } else if (request.durationSeconds > 0.0f) {
            if (request.primaryTag == "attack") {
                played = viewer->PlayAttackSequenceWithTagsAndDuration(request.secondaryTags, request.durationSeconds);
            } else if (request.primaryTag == "spell" || request.primaryTag.empty()) {
                played = viewer->PlaySpellSequenceWithTags(request.secondaryTags, request.durationSeconds);
            } else if (request.primaryTag == "channel") {
                played = viewer->PlayChannelSequenceWithTags(request.secondaryTags, request.durationSeconds);
            } else if (request.primaryTag == "decay") {
                played = viewer->PlayDecaySequenceWithTags(request.secondaryTags, request.durationSeconds);
            } else if (request.primaryTag == "dissipate") {
                played = viewer->PlayDissipateSequence(request.durationSeconds);
            }
        }
        if (!played) {
            played = viewer->PlaySequenceByPrimaryAndTags(request.primaryTag.c_str(), request.secondaryTags,
                                                         loop, speedRatio, queueStandAfterCurrent, request.force);
        }
    } else if (request.useSequenceIndex) {
        played = viewer->PlaySequenceByIndex(request.sequenceIndex, loop, speedRatio, queueStandAfterCurrent);
    } else if (!request.sequenceName.empty()) {
        played = viewer->PlaySequenceByName(request.sequenceName.c_str(), loop, speedRatio, queueStandAfterCurrent);
    }

    if (played) {
        auto storedRequest = request;
        storedRequest.force = false;
        g->simUnitScriptedAnimations[handleId] = {storedRequest, loop, queueStandAfterCurrent};
    }
    return played;
}

static void ApplyPendingUnitAnimation(int handleId, MdxViewer* viewer, GameController* g) {
    if (!viewer || !viewer->GetAnimator()) return;
    const auto pendingIt = g->simUnitPendingAnimations.find(handleId);
    if (pendingIt == g->simUnitPendingAnimations.end()) return;
    ApplyAnimationRequestToViewer(handleId, viewer, pendingIt->second, g);
    g->simUnitPendingAnimations.erase(pendingIt);
}

static void QueueUnitAnimationRequest(int handleId,
                                      const animation_utils::PendingUnitAnimation& request,
                                      GameController* g) {
    if (!g) return;
    g->simUnitQueuedAnimations[handleId].push_back(request);
}

static bool ApplyNextQueuedUnitAnimation(int handleId, MdxViewer* viewer, GameController* g) {
    if (!viewer || !viewer->GetAnimator() || !g) return false;
    auto queuedIt = g->simUnitQueuedAnimations.find(handleId);
    if (queuedIt == g->simUnitQueuedAnimations.end() || queuedIt->second.empty()) {
        return false;
    }
    auto request = queuedIt->second.front();
    queuedIt->second.pop_front();
    if (queuedIt->second.empty()) {
        g->simUnitQueuedAnimations.erase(queuedIt);
    }
    if (ApplyAnimationRequestToViewer(handleId, viewer, request, g)) {
        g->simUnitPendingAnimations.erase(handleId);
        return true;
    }
    g->simUnitPendingAnimations[handleId] = std::move(request);
    return false;
}

namespace {

void AppendCommaSeparatedArgs(std::vector<std::string>& out, const char* value) {
    if (!value) return;

    std::stringstream ss(value);
    std::string item;
    while (std::getline(ss, item, ',')) {
        item.erase(item.begin(), std::find_if(item.begin(), item.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        item.erase(std::find_if(item.rbegin(), item.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), item.end());
        if (!item.empty()) {
            out.push_back(item);
        }
    }
}

bool ParseDebugUiResources(const char* value, int& gold, int& lumber, int& foodUsed, int& foodCap, std::string& timeText) {
    std::vector<std::string> parts;
    AppendCommaSeparatedArgs(parts, value);
    if (parts.size() < 4) {
        return false;
    }

    gold = std::atoi(parts[0].c_str());
    lumber = std::atoi(parts[1].c_str());
    foodUsed = std::atoi(parts[2].c_str());
    foodCap = std::atoi(parts[3].c_str());
    if (parts.size() >= 5) {
        timeText = parts[4];
    }
    return true;
}

void AddJassErrorStats(const std::vector<std::string>& errors,
                       std::unordered_map<std::string, int>& unknownFunctions,
                       std::unordered_map<std::string, int>& parserErrorCounts,
                       int& parserErrors,
                       int& lexerErrors,
                       int& otherErrors) {
    for (const std::string& error : errors) {
        const std::string unknownPrefix = "Unknown function: ";
        const size_t unknownPos = error.find(unknownPrefix);
        if (unknownPos != std::string::npos) {
            unknownFunctions[error.substr(unknownPos + unknownPrefix.size())]++;
        } else if (error.rfind("Parser:", 0) == 0) {
            parserErrors++;
            parserErrorCounts[error.substr(8)]++;
        } else if (error.rfind("Lexer:", 0) == 0) {
            lexerErrors++;
        } else {
            otherErrors++;
        }
    }
}

void PrintTopJassUnknowns(const std::unordered_map<std::string, int>& unknownFunctions) {
    std::vector<std::pair<std::string, int>> sorted(unknownFunctions.begin(), unknownFunctions.end());
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });
    const size_t limit = std::min<size_t>(sorted.size(), 25);
    for (size_t i = 0; i < limit; ++i) {
        std::cout << "  unknown_native " << sorted[i].first
                  << " count=" << sorted[i].second << std::endl;
    }
    if (sorted.size() > limit) {
        std::cout << "  unknown_native_more=" << (sorted.size() - limit) << std::endl;
    }
}

void PrintTopJassParserErrors(const std::unordered_map<std::string, int>& parserErrorCounts) {
    std::vector<std::pair<std::string, int>> sorted(parserErrorCounts.begin(), parserErrorCounts.end());
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });
    const size_t limit = std::min<size_t>(sorted.size(), 20);
    for (size_t i = 0; i < limit; ++i) {
        std::cout << "  parser_error " << sorted[i].second
                  << "x " << sorted[i].first << std::endl;
    }
    if (sorted.size() > limit) {
        std::cout << "  parser_error_more=" << (sorted.size() - limit) << std::endl;
    }
}

std::string JassDiagnosticCategory(const std::string& name) {
    const std::string lower = LowerAscii(name);
    if (lower.find("ability") != std::string::npos || lower.find("abil") != std::string::npos) {
        return "ability";
    }
    if (lower.find("triggerregister") != std::string::npos || lower.find("event") != std::string::npos) {
        return "trigger_event";
    }
    if (lower.find("camera") != std::string::npos || lower.find("cinematic") != std::string::npos) {
        return "camera_cinematic";
    }
    if (lower.find("timer") != std::string::npos) {
        return "timer";
    }
    if (lower.find("destruct") != std::string::npos) {
        return "destructable";
    }
    if (lower.find("item") != std::string::npos) {
        return "item";
    }
    if (lower.find("unit") != std::string::npos || lower.find("corpse") != std::string::npos) {
        return "unit";
    }
    if (lower.find("sound") != std::string::npos || lower.find("music") != std::string::npos) {
        return "sound";
    }
    if (lower.rfind("create", 0) == 0 || lower.find("create") != std::string::npos) {
        return "create";
    }
    return "other";
}

void PrintTopJassStaticEntries(const char* prefix,
                               const std::unordered_map<std::string, int>& entries,
                               size_t limit) {
    std::vector<std::pair<std::string, int>> sorted(entries.begin(), entries.end());
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });
    const size_t count = std::min(limit, sorted.size());
    for (size_t i = 0; i < count; ++i) {
        std::cout << "  " << prefix << " " << sorted[i].first
                  << " count=" << sorted[i].second
                  << " category=" << JassDiagnosticCategory(sorted[i].first) << std::endl;
    }
    if (sorted.size() > count) {
        std::cout << "  " << prefix << "_more=" << (sorted.size() - count) << std::endl;
    }
}

void PrintJassStaticDiagnosticsForVm(const char* phase, const JassVM& vm) {
    std::unordered_set<std::string> registered;
    registered.reserve(vm.GetRegisteredNatives().size());
    for (const auto& entry : vm.GetRegisteredNatives()) {
        registered.insert(entry.first);
    }
    std::unordered_set<std::string> functions = vm.GetFunctionNames();

    std::unordered_map<std::string, int> unsupportedCalls;
    int totalCalls = 0;
    for (const auto& entry : vm.GetFunctionCallCounts()) {
        totalCalls += entry.second;
        if (registered.count(entry.first) == 0 && functions.count(entry.first) == 0) {
            unsupportedCalls[entry.first] += entry.second;
        }
    }

    std::unordered_map<std::string, int> unsupportedNativeDecls;
    for (const auto& entry : vm.GetNativeDeclarations()) {
        if (registered.count(entry.first) == 0) {
            unsupportedNativeDecls[entry.first] += entry.second;
        }
    }

    std::unordered_map<std::string, int> callCategories;
    std::unordered_map<std::string, int> declCategories;
    int totalUnsupportedCalls = 0;
    int totalUnsupportedDecls = 0;
    for (const auto& entry : unsupportedCalls) {
        callCategories[JassDiagnosticCategory(entry.first)] += entry.second;
        totalUnsupportedCalls += entry.second;
    }
    for (const auto& entry : unsupportedNativeDecls) {
        declCategories[JassDiagnosticCategory(entry.first)] += entry.second;
        totalUnsupportedDecls += entry.second;
    }

    std::cout << "jass_static phase=" << phase
              << " declared_natives=" << vm.GetNativeDeclarations().size()
              << " registered_natives=" << registered.size()
              << " function_defs=" << functions.size()
              << " function_call_names=" << vm.GetFunctionCallCounts().size()
              << " function_calls=" << totalCalls << std::endl;
    std::cout << "jass_static_unsupported phase=" << phase
              << " call_unique=" << unsupportedCalls.size()
              << " call_total=" << totalUnsupportedCalls
              << " native_decl_unique=" << unsupportedNativeDecls.size()
              << " native_decl_total=" << totalUnsupportedDecls << std::endl;

    std::vector<std::string> categories = {
        "ability", "trigger_event", "camera_cinematic", "unit", "item",
        "destructable", "create", "timer", "sound", "other"
    };
    for (const auto& category : categories) {
        const int callCount = callCategories.count(category) ? callCategories[category] : 0;
        const int declCount = declCategories.count(category) ? declCategories[category] : 0;
        if (callCount || declCount) {
            std::cout << "  jass_static_category phase=" << phase
                      << " category=" << category
                      << " unsupported_calls=" << callCount
                      << " unsupported_native_decls=" << declCount << std::endl;
        }
    }

    PrintTopJassStaticEntries("unsupported_static_call", unsupportedCalls, 25);
    PrintTopJassStaticEntries("unsupported_native_decl", unsupportedNativeDecls, 25);
}

void PlayResolvedUnitAnimation(CUnit* unit, MdxViewer* viewer, GameController* g) {
    if (!unit || !viewer) return;

    const int handleId = unit->GetHandleId();
    auto& splatMeta = g->GetSplatMeta();
    const auto* scriptTags = &g->simUnitAnimationPropertyTags;

    auto GetSplatMetaFn = [&](const CUnit& u) -> const SplatMetadataStore::UnitRenderSplatMeta* {
        return splatMeta.GetUnitRenderSplatMeta(u);
    };

    CResourceType carriedType = g->simUnitVisualResourceTags.count(handleId)
        ? g->simUnitVisualResourceTags[handleId] : CResourceType::NONE;
    CBehaviorCategory behaviorCategory = unit_text_utils::GetUnitCurrentBehaviorCategory(unit);

    // If script requested a specific animation, use it
    ApplyPendingUnitAnimation(handleId, viewer, g);

    const auto scriptIt = g->simUnitScriptedAnimations.find(handleId);
    if (scriptIt != g->simUnitScriptedAnimations.end()) {
        const auto& state = scriptIt->second;
        if (CurrentAnimationRequestMatchesViewer(viewer, state.request)) {
            return;
        }
        bool applied = false;
        const float speedRatio = std::clamp(state.request.speedRatio, 0.1f, 4.0f);
        if (state.request.useTags) {
            applied = viewer->PlaySequenceByPrimaryAndTags(
                state.request.primaryTag.c_str(), state.request.secondaryTags, state.loop, speedRatio,
                state.queueStandAfterCurrent, state.request.force);
        } else if (state.request.useSequenceIndex) {
            applied = viewer->PlaySequenceByIndex(state.request.sequenceIndex, state.loop, speedRatio,
                                                  state.queueStandAfterCurrent);
        } else if (!state.request.sequenceName.empty()) {
            applied = viewer->PlaySequenceByName(state.request.sequenceName.c_str(), state.loop, speedRatio,
                                                  state.queueStandAfterCurrent);
        }
        if (applied) {
            return;
        }
        g->simUnitScriptedAnimations.erase(scriptIt);
    }

    // Death / decay
    auto modeIt = g->simUnitVisualAnimationModes.find(handleId);
    if (modeIt != g->simUnitVisualAnimationModes.end()) {
        auto mode = modeIt->second;
        if (mode == animation_utils::UnitVisualAnimationMode::DEAD ||
            mode == animation_utils::UnitVisualAnimationMode::CORPSE ||
            mode == animation_utils::UnitVisualAnimationMode::BONE_CORPSE ||
            mode == animation_utils::UnitVisualAnimationMode::DISSIPATE) {
            animation_utils::PlayDeathVisualForUnit(unit, viewer, mode, g->GetSimulation());
            return;
        }
    }

    // Spell
    if (auto* spell = animation_utils::GetSpellBehavior(unit)) {
        if (spell->IsCasting() || spell->IsBackswing()) {
            animation_utils::PlaySpellVisualForUnit(unit, viewer, scriptTags);
            return;
        }
    }

    // Building construction (no dedicated method; use stand and set construction flag)
    if (g->simUnitVisualConstructionStates.count(handleId)) {
        viewer->PlayStandSequence();
        return;
    }

    // Determine if moving
    bool isMoving = animation_utils::ComputeUnitVisualIsMoving(unit, g->simUnitVisualLastPositions);
    bool wasMoving = g->simUnitVisualMovingStates.count(handleId) && g->simUnitVisualMovingStates[handleId];
    bool justStopped = !isMoving && wasMoving;

    if (isMoving) {
        bool playedWalk = false;
        bool hasWorkTag = false;
        auto propTags = g->simUnitVisualPropertyTags.count(handleId)
            ? g->simUnitVisualPropertyTags[handleId] : std::vector<std::string>{};
        auto scriptTagsVec = g->simUnitAnimationPropertyTags.count(handleId)
            ? g->simUnitAnimationPropertyTags[handleId] : std::vector<std::string>{};
        for (const auto& t : propTags) if (t == "work") hasWorkTag = true;
        for (const auto& t : scriptTagsVec) if (t == "work") hasWorkTag = true;

        if (hasWorkTag) {
            playedWalk = animation_utils::PlayWalkWorkVisualForUnit(unit, viewer, behaviorCategory, carriedType,
                                                                     splatMeta, scriptTags);
        }
        if (!playedWalk) {
            animation_utils::PlayWalkVisualForUnit(unit, viewer, behaviorCategory, carriedType,
                                                    splatMeta, scriptTags);
        }
    } else {
        bool isWorking = false;
        auto propTags = g->simUnitVisualPropertyTags.count(handleId)
            ? g->simUnitVisualPropertyTags[handleId] : std::vector<std::string>{};
        auto scriptTagsVec = g->simUnitAnimationPropertyTags.count(handleId)
            ? g->simUnitAnimationPropertyTags[handleId] : std::vector<std::string>{};
        for (const auto& t : propTags) if (t == "work") isWorking = true;
        for (const auto& t : scriptTagsVec) if (t == "work") isWorking = true;

        if (isWorking) {
            animation_utils::PlayWorkVisualForUnit(unit, viewer, behaviorCategory, carriedType,
                                                    splatMeta, scriptTags);
        } else {
            animation_utils::PlayReadyVisualForUnit(unit, viewer, behaviorCategory, carriedType,
                                                     splatMeta, scriptTags);
        }
    }

    g->simUnitVisualMovingStates[handleId] = isMoving;
}

void SyncSimulationUnitAnimation(CUnit* unit, MdxViewer* viewer, GameController* g) {
    if (!unit || !viewer) return;
    const int handleId = unit->GetHandleId();

    const auto lastPosIt = g->simUnitVisualLastPositions.find(handleId);
    bool posChanged = false;
    if (lastPosIt != g->simUnitVisualLastPositions.end()) {
        float dx = unit->GetX() - lastPosIt->second.first;
        float dy = unit->GetY() - lastPosIt->second.second;
        posChanged = (dx * dx + dy * dy) > 0.01f;
    } else {
        posChanged = true;
    }

    if (posChanged) {
        g->simUnitVisualLastPositions[handleId] = {unit->GetX(), unit->GetY()};
    }

    // Re-evaluate animation if state changed
    bool isMoving = animation_utils::ComputeUnitVisualIsMoving(unit, g->simUnitVisualLastPositions);
    bool wasMoving = g->simUnitVisualMovingStates.count(handleId) && g->simUnitVisualMovingStates[handleId];

    bool needsResolve = (isMoving != wasMoving);
    if (g->simUnitPendingAnimations.count(handleId)) needsResolve = true;

    if (needsResolve) {
        PlayResolvedUnitAnimation(unit, viewer, g);
    }

    // Handle queued animations
    const auto queuedIt = g->simUnitQueuedAnimations.find(handleId);
    if (queuedIt != g->simUnitQueuedAnimations.end() && !queuedIt->second.empty()) {
        if (!viewer->IsCurrentSequenceLooping() && viewer->IsCurrentSequenceFinished()) {
            ApplyNextQueuedUnitAnimation(handleId, viewer, g);
        }
    }

    // Sync position/rotation/scale via SceneManager
    float visualHeight = unit_selection_geom::ComputeUnitVisualHeight(
        *unit, g->GetTexturedTerrain().IsValid(),
        [&](float x, float z) { return g->GetTexturedTerrain().GetGroundHeight(x, z); });
    {
        float pos[3] = {unit->GetX(), visualHeight, unit->GetY()};
        auto vit = g->simUnitToVisualIdx.find(handleId);
        if (vit != g->simUnitToVisualIdx.end()) {
            g->GetScene().UpdateInstancePosition(vit->second, pos, unit->GetFacing());
            // Script scale (SetUnitScale) multiplies the seed scale; refresh
            // every frame so mid-game changes apply (cheap idempotent write).
            float seedScale[3] = {1.0f, 1.0f, 1.0f};
            auto seedIt = g->simUnitVisualSeeds.find(handleId);
            if (seedIt != g->simUnitVisualSeeds.end()) {
                seedScale[0] = seedIt->second.scale[0];
                seedScale[1] = seedIt->second.scale[1];
                seedScale[2] = seedIt->second.scale[2];
            }
            const float s = unit->GetScale();
            const float scale[3] = {seedScale[0] * s, seedScale[1] * s, seedScale[2] * s};
            g->GetScene().SetInstanceScale(vit->second, scale);
        }
    }
}

} // namespace

// ======================================================================
// GameController implementation
// ======================================================================

GameController::GameController() = default;
GameController::~GameController() = default;

void GameController::ParseArgs(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (std::strcmp(arg, "--screenshot") == 0 && i + 1 < argc) {
            screenshotPath = argv[++i];
            screenshotFrame = 1;
        } else if (std::strncmp(arg, "--screenshot=", 13) == 0) {
            screenshotPath = arg + 13;
            screenshotFrame = 1;
        } else if (std::strcmp(arg, "--screenshot-frame") == 0 && i + 1 < argc) {
            screenshotFrame = std::max(0, std::atoi(argv[++i]));
        } else if (std::strncmp(arg, "--screenshot-frame=", 19) == 0) {
            screenshotFrame = std::max(0, std::atoi(arg + 19));
        } else if (std::strcmp(arg, "--exit-after-frame") == 0 && i + 1 < argc) {
            exitAfterFrame_ = std::max(0, std::atoi(argv[++i]));
        } else if (std::strncmp(arg, "--exit-after-frame=", 19) == 0) {
            exitAfterFrame_ = std::max(0, std::atoi(arg + 19));
        } else if (std::strcmp(arg, "--ui-only") == 0) {
            uiOnlyMode_ = true;
        } else if (std::strcmp(arg, "--hide-hud") == 0) {
            hideHud_ = true;
        } else if (std::strcmp(arg, "--terrain-only") == 0) {
            terrainOnly_ = true;
            hideHud_ = true;
        } else if (std::strcmp(arg, "--dump-unsupported") == 0) {
            dumpUnsupported_ = true;
        } else if (std::strcmp(arg, "--diagnose-map") == 0) {
            diagnoseMapOnly_ = true;
            dumpUnsupported_ = true;
            if (exitAfterFrame_ < 0) exitAfterFrame_ = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                mapPath_ = argv[++i];
            }
        } else if (std::strncmp(arg, "--diagnose-map=", 15) == 0) {
            diagnoseMapOnly_ = true;
            dumpUnsupported_ = true;
            if (exitAfterFrame_ < 0) exitAfterFrame_ = 1;
            mapPath_ = arg + 15;
        } else if (std::strcmp(arg, "--debug-gameplay-smoke") == 0) {
            debugGameplaySmoke_ = true;
            dumpUnsupported_ = true;
        } else if (std::strcmp(arg, "--overview-camera") == 0) {
            overviewCamera_ = true;
        } else if (std::strcmp(arg, "--scene-kind") == 0 && i + 1 < argc) {
            debugSceneRenderKind_ = argv[++i];
            hideHud_ = true;
        } else if (std::strncmp(arg, "--scene-kind=", 13) == 0) {
            debugSceneRenderKind_ = arg + 13;
            hideHud_ = true;
        } else if (std::strcmp(arg, "--scene-first") == 0 && i + 1 < argc) {
            debugSceneRenderStart_ = 0;
            debugSceneRenderCount_ = std::max(0, std::atoi(argv[++i]));
            hideHud_ = true;
        } else if (std::strncmp(arg, "--scene-first=", 14) == 0) {
            debugSceneRenderStart_ = 0;
            debugSceneRenderCount_ = std::max(0, std::atoi(arg + 14));
            hideHud_ = true;
        } else if (std::strcmp(arg, "--scene-range") == 0 && i + 1 < argc) {
            int start = 0;
            int count = -1;
            std::sscanf(argv[++i], "%d,%d", &start, &count);
            debugSceneRenderStart_ = std::max(0, start);
            debugSceneRenderCount_ = count;
            hideHud_ = true;
        } else if (std::strncmp(arg, "--scene-range=", 14) == 0) {
            int start = 0;
            int count = -1;
            std::sscanf(arg + 14, "%d,%d", &start, &count);
            debugSceneRenderStart_ = std::max(0, start);
            debugSceneRenderCount_ = count;
            hideHud_ = true;
        } else if (std::strcmp(arg, "--debug-hud-game") == 0) {
            debugHudGameMode_ = true;
            debugSelectFirstUnitForHud_ = true;
        } else if (std::strcmp(arg, "--debug-ui-command-card") == 0) {
            debugUiCommandCard_ = true;
        } else if (std::strcmp(arg, "--debug-ui-game-menu") == 0) {
            debugUiGameMenu_ = true;
        } else if (std::strcmp(arg, "--debug-ui-game-menu-panel") == 0 && i + 1 < argc) {
            debugUiGameMenuPanel_ = argv[++i];
            debugUiGameMenu_ = true;
        } else if (std::strncmp(arg, "--debug-ui-game-menu-panel=", 27) == 0) {
            debugUiGameMenuPanel_ = arg + 27;
            debugUiGameMenu_ = true;
        } else if (std::strcmp(arg, "--debug-ui-game-menu-actions") == 0 && i + 1 < argc) {
            AppendCommaSeparatedArgs(debugUiGameMenuActions_, argv[++i]);
            debugUiGameMenu_ = true;
        } else if (std::strncmp(arg, "--debug-ui-game-menu-actions=", 29) == 0) {
            AppendCommaSeparatedArgs(debugUiGameMenuActions_, arg + 29);
            debugUiGameMenu_ = true;
        } else if (std::strcmp(arg, "--debug-ui-glue-menu") == 0) {
            debugUiGlueMenu_ = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                debugUiGlueMenuPage_ = argv[++i];
            }
            if (hudLayoutPath_ == "console") {
                hudLayoutPath_ = "glue";
            }
        } else if (std::strncmp(arg, "--debug-ui-glue-menu=", 21) == 0) {
            debugUiGlueMenu_ = true;
            debugUiGlueMenuPage_ = arg + 21;
            if (hudLayoutPath_ == "console") {
                hudLayoutPath_ = "glue";
            }
        } else if ((std::strcmp(arg, "--debug-ui-glue-action") == 0 ||
                    std::strcmp(arg, "--debug-ui-glue-actions") == 0) && i + 1 < argc) {
            AppendCommaSeparatedArgs(debugUiGlueMenuActions_, argv[++i]);
            debugUiGlueMenu_ = true;
            if (hudLayoutPath_ == "console") {
                hudLayoutPath_ = "glue";
            }
        } else if (std::strncmp(arg, "--debug-ui-glue-action=", 23) == 0) {
            AppendCommaSeparatedArgs(debugUiGlueMenuActions_, arg + 23);
            debugUiGlueMenu_ = true;
            if (hudLayoutPath_ == "console") {
                hudLayoutPath_ = "glue";
            }
        } else if (std::strncmp(arg, "--debug-ui-glue-actions=", 24) == 0) {
            AppendCommaSeparatedArgs(debugUiGlueMenuActions_, arg + 24);
            debugUiGlueMenu_ = true;
            if (hudLayoutPath_ == "console") {
                hudLayoutPath_ = "glue";
            }
        } else if (std::strcmp(arg, "--debug-ui-upper-button") == 0 && i + 1 < argc) {
            debugUiUpperButton_ = argv[++i];
        } else if (std::strncmp(arg, "--debug-ui-upper-button=", 24) == 0) {
            debugUiUpperButton_ = arg + 24;
        } else if (std::strcmp(arg, "--debug-ui-resources") == 0 && i + 1 < argc) {
            if (ParseDebugUiResources(argv[++i], debugUiGold_, debugUiLumber_, debugUiFoodUsed_, debugUiFoodCap_, debugUiTimeText_)) {
                debugUiResources_ = true;
            } else {
                std::cerr << "Invalid --debug-ui-resources value. Expected gold,lumber,foodUsed,foodCap[,time]" << std::endl;
            }
        } else if (std::strncmp(arg, "--debug-ui-resources=", 21) == 0) {
            if (ParseDebugUiResources(arg + 21, debugUiGold_, debugUiLumber_, debugUiFoodUsed_, debugUiFoodCap_, debugUiTimeText_)) {
                debugUiResources_ = true;
            } else {
                std::cerr << "Invalid --debug-ui-resources value. Expected gold,lumber,foodUsed,foodCap[,time]" << std::endl;
            }
        } else if (std::strcmp(arg, "--hud-layout") == 0 && i + 1 < argc) {
            hudLayoutPath_ = argv[++i];
        } else if (std::strncmp(arg, "--hud-layout=", 11) == 0) {
            hudLayoutPath_ = arg + 11;
        } else if (std::strncmp(arg, "--run=", 6) == 0) {
            mapPath_ = arg + 6;
        } else if (std::strcmp(arg, "--run") == 0 && i + 1 < argc) {
            mapPath_ = argv[++i];
        } else if (std::strncmp(arg, "--map=", 6) == 0) {
            mapPath_ = arg + 6;
        } else if (std::strcmp(arg, "--map") == 0 && i + 1 < argc) {
            mapPath_ = argv[++i];
        } else if (std::strcmp(arg, "--debug-select-first-unit") == 0) {
            debugSelectFirstUnitForHud_ = true;
        } else if (std::strncmp(arg, "--ai=", 5) == 0) {
            // Single-player setup: force slots to COMPUTER AI (ref
            // MeleeStartingAI/PickMeleeAI; e.g. --ai=1,2).
            std::vector<std::string> parts;
            AppendCommaSeparatedArgs(parts, arg + 5);
            for (const auto& p : parts) {
                const int slot = std::atoi(p.c_str());
                if (slot >= 0 && slot < 24) config_.aiPlayerSlots.push_back(slot);
            }
        } else if (std::strncmp(arg, "--closed=", 9) == 0) {
            // Force slots closed (NONE controller + inactive; e.g. --closed=3).
            std::vector<std::string> parts;
            AppendCommaSeparatedArgs(parts, arg + 9);
            for (const auto& p : parts) {
                const int slot = std::atoi(p.c_str());
                if (slot >= 0 && slot < 24) config_.closedPlayerSlots.push_back(slot);
            }
        } else if (std::strncmp(arg, "--local-player=", 15) == 0) {
            config_.localPlayerIndex = std::max(0, std::atoi(arg + 15));
        } else if (arg[0] != '-') {
            mapPath_ = arg;
        }
    }
}

bool GameController::Initialize() {
    std::cout << "Warcraft III C++ Recreation - Initializing..." << std::endl;
    {
        std::ofstream(MoveDebugLogPath(), std::ios::trunc).close();
        MoveDebugLogLine("[MoveDebug] session_begin");
    }

    if (!Renderer::Initialize()) return false;

    fs::path project_root = PROJECT_SOURCE_DIR;
    if (uiOnlyMode_) {
        fs::path iniPath = project_root / "resources" / "warsmash.ini";
        config_.Load(iniPath.string());
        std::cout << "UI-only mode: skipping map loading and scene simulation" << std::endl;
        if (debugUiGlueMenu_) {
            OpenDataSources();
        }
        if (!ui_.Init(debugUiGlueMenu_ ? &mpq_ : nullptr, 1280, 720, hudLayoutPath_, config_.gameVersion)) {
            std::cerr << "UI-only mode: MainUI init failed" << std::endl;
            return false;
        }
        ui_.SetGlueMapLaunchCallback([this](const std::string& path, int localPlayer) {
            HandleGlueMapLaunchRequest(path, localPlayer);
        });
        if (debugUiGlueMenu_) {
            std::vector<War3UI::GlueCampaignListItem> campaigns = LoadUiOnlyGlueCampaigns(mpq_, config_.gameVersion);
            ui_.SetGlueMenuCampaignsForUiOnly(campaigns);
            std::vector<War3UI::GlueMapListItem> maps = DiscoverUiOnlyGlueMaps(project_root);
            ui_.SetGlueMenuMapsForUiOnly(maps);
            std::cout << "UI-only mode: discovered " << maps.size()
                      << " custom game maps for glue menu" << std::endl;
        }
        if (debugUiCommandCard_) {
            ui_.PopulateDebugCommandCardForUiOnly();
        }
        if (debugUiResources_) {
            ui_.SetResourcesForUiOnly(debugUiGold_, debugUiLumber_, debugUiFoodUsed_, debugUiFoodCap_, debugUiTimeText_);
        }
        if (debugUiGlueMenu_) {
            ui_.OpenGlueMenuForUiOnly(debugUiGlueMenuPage_);
            for (const std::string& action : debugUiGlueMenuActions_) {
                if (!ui_.InvokeGlueMenuActionForUiOnly(action)) {
                    std::cerr << "UI-only mode: failed to invoke glue menu action '" << action << "'" << std::endl;
                    return false;
                }
            }
        }
        if (!debugUiUpperButton_.empty()) {
            if (!ui_.InvokeUpperButtonForUiOnly(debugUiUpperButton_)) {
                std::cerr << "UI-only mode: failed to invoke upper button '" << debugUiUpperButton_ << "'" << std::endl;
            }
        }
        if (debugUiGameMenu_) {
            if (!debugUiGameMenuPanel_.empty()) {
                ui_.OpenGameMenuPanelForUiOnly(debugUiGameMenuPanel_);
            } else {
                ui_.OpenGameMenuForUiOnly();
            }
            for (const std::string& action : debugUiGameMenuActions_) {
                if (!ui_.InvokeGameMenuActionForUiOnly(action)) {
                    std::cerr << "UI-only mode: failed to invoke game menu action '" << action << "'" << std::endl;
                    return false;
                }
            }
        }
        if (pendingGlueMapLaunch_) {
            if (!StartPendingGlueMapLaunch(1280, 720)) {
                return false;
            }
            initialized_ = true;
            return true;
        }
        state_ = GameState::IN_GAME;
        pendingState_ = GameState::IN_GAME;
        stateTransition_ = false;
        initialized_ = true;
        return true;
    }

    fs::path iniPath = project_root / "resources" / "warsmash.ini";
    config_.Load(iniPath.string());

    OpenDataSources();

    // Determine map path
    fs::path resourcesDir = project_root / "resources";
    if (mapPath_.empty()) {
        std::vector<fs::path> mapFiles;
        if (fs::exists(resourcesDir)) {
            for (const auto& entry : fs::recursive_directory_iterator(resourcesDir)) {
                std::string ext = entry.path().extension().string();
                if (ext == ".w3x" || ext == ".w3m")
                    mapFiles.push_back(entry.path());
            }
        }
        fs::path defaultPath = resourcesDir / DEFAULT_MAP;
        if (fs::exists(defaultPath))
            mapPath_ = defaultPath.string();
        else if (!mapFiles.empty())
            mapPath_ = mapFiles[0].string();
    } else {
        std::string resolvedMapPath;
        if (ResolveMapPathForLoad(resourcesDir, mpq_, mapPath_, resolvedMapPath)) {
            mapPath_ = resolvedMapPath;
        }
    }

    std::cout << "\nMap: " << fs::path(mapPath_).filename() << std::endl;

    if (!mapPath_.empty() && fs::exists(mapPath_)) {
        mpq_.OpenOnTop(mapPath_);
    }

    if (debugHudGameMode_) {
        std::cout << "HUD game debug: loading game state from default map and rendering HUD only" << std::endl;
    }

    if (!debugHudGameMode_) {
        scene_.Init(mpq_);
        splatMeta_.Load(mpq_);
    }

    if (!debugHudGameMode_) {
        audio_.Init();
    }
    if (!debugHudGameMode_ && audio_.IsValid() && mpq_.ArchiveCount() > 0) {
        audio_.SetMpqManager(&mpq_);
        audio_.LoadSound("ui_click", "UI\\Mouse\\HumanClick1.wav", &mpq_);
        audio_.LoadSound("ui_error", "UI\\Mouse\\HumanClickError.wav", &mpq_);
        uiSounds_.LoadFromMpq(mpq_);
    }

    if (!hideHud_) {
        if (!ui_.Init(&mpq_, 1280, 720, hudLayoutPath_, config_.gameVersion)) {
            std::cerr << "GameController: MainUI init failed" << std::endl;
            return false;
        }
        ui_.SetGlueMapLaunchCallback([this](const std::string& path, int localPlayer) {
            HandleGlueMapLaunchRequest(path, localPlayer);
        });
        // Headless/smoke support: open the game menu like the ui-only debug
        // path does, so textured menu chrome can be verified in full mode.
        if (debugUiGameMenu_) {
            if (!debugUiGameMenuPanel_.empty()) {
                ui_.OpenGameMenuPanelForUiOnly(debugUiGameMenuPanel_);
            } else {
                ui_.OpenGameMenuForUiOnly();
            }
        }
        // Upper-button dialogs (quests/allies/chat) for in-game verification,
        // mirroring the ui-only path (ref --debug-ui-upper-button).
        if (!debugUiUpperButton_.empty()) {
            if (!ui_.InvokeUpperButtonForUiOnly(debugUiUpperButton_)) {
                std::cerr << "GameController: failed to invoke upper button '"
                          << debugUiUpperButton_ << "'" << std::endl;
            }
        }
    } else {
        std::cout << "GameController: HUD init skipped (--hide-hud)" << std::endl;
    }
    initialized_ = true;
    return true;
}

void GameController::OpenDataSources() {
    fs::path projectRoot = PROJECT_SOURCE_DIR;
    fs::path resourcesDir = projectRoot / "resources";

    if (!config_.dataSources.empty()) {
        for (const auto& ds : config_.dataSources) {
            if (ds.first == "MPQ" || ds.first == "mpq") {
                auto p = path_utils::ResolveConfiguredPath(
                    projectRoot, resourcesDir, ds.second);
                if (fs::exists(p)) mpq_.Open(p.string());
                else std::cerr << "MpqStorm: configured MPQ not found " << p.string() << std::endl;
            } else if (ds.first == "CASC" || ds.first == "casc") {
                auto p = path_utils::ResolveConfiguredPath(
                    projectRoot, resourcesDir, ds.second);
                if (fs::exists(p)) mpq_.OpenCasc(p.string());
                else std::cerr << "MpqStorm: configured CASC storage not found " << p.string() << std::endl;
            }
        }
    }

    if (mpq_.ArchiveCount() == 0) {
        for (const auto& name : {"War3Patch.mpq", "War3xLocal.mpq", "war3x.mpq", "war3.mpq"}) {
            fs::path p = resourcesDir / name;
            if (fs::exists(p)) mpq_.Open(p.string());
        }
    }
}

void GameController::HandleGlueMapLaunchRequest(const std::string& mapPath, int localPlayerIndex) {
    fs::path projectRoot = PROJECT_SOURCE_DIR;
    fs::path resourcesDir = projectRoot / "resources";

    std::string resolvedPath;
    const bool resolved = ResolveMapPathForLoad(resourcesDir, mpq_, mapPath, resolvedPath);
    std::ostringstream launchLog;
    launchLog << "GlueMapLaunch: requested path='" << mapPath
              << "' localPlayer=" << localPlayerIndex
              << " resolved=" << (resolved ? "yes" : "no");
    if (resolved) {
        launchLog << " resolvedPath='" << resolvedPath << "'";
    }
    MoveDebugLogLine(launchLog.str());

    if (!resolved) {
        return;
    }

    config_.localPlayerIndex = localPlayerIndex;
    // Fresh menu launch: never inherit a stale ChangeLevel snapshot (e.g.
    // the user aborted to the menu mid-campaign).
    carriedHeroes_.clear();
    arrivedViaChangeLevel_ = false;
    if (uiOnlyMode_) {
        pendingGlueMapLaunch_ = true;
        pendingGlueMapLaunchRequestedPath_ = mapPath;
        pendingGlueMapLaunchResolvedPath_ = resolvedPath;
        pendingGlueMapLaunchLocalPlayer_ = localPlayerIndex;
        MoveDebugLogLine("GlueMapLaunch: queued pending UI-only launch");
        return;
    }

    if (mapConfigInitialized_ || loadingInitialized_ || simulationInitialized_) {
        MoveDebugLogLine("GlueMapLaunch: runtime map switch requested after initialization; deferred hot-switch support pending");
        return;
    }

    mapPath_ = resolvedPath;
    if (fs::exists(mapPath_)) {
        mpq_.OpenOnTop(mapPath_);
    }
    TransitionTo(GameState::MAP_CONFIG);
    MoveDebugLogLine("GlueMapLaunch: queued map load path='" + mapPath_ + "'");
}

bool GameController::StartPendingGlueMapLaunch(int screenW, int screenH) {
    if (!pendingGlueMapLaunch_) {
        return false;
    }

    const std::string requestedPath = pendingGlueMapLaunchRequestedPath_;
    const std::string resolvedPath = pendingGlueMapLaunchResolvedPath_;
    const int localPlayerIndex = pendingGlueMapLaunchLocalPlayer_;
    pendingGlueMapLaunch_ = false;
    pendingGlueMapLaunchRequestedPath_.clear();
    pendingGlueMapLaunchResolvedPath_.clear();

    if (resolvedPath.empty() || !fs::exists(resolvedPath)) {
        MoveDebugLogLine("GlueMapLaunch: pending launch aborted; resolved map missing path='" + resolvedPath + "'");
        return false;
    }

    MoveDebugLogLine("GlueMapLaunch: starting runtime map launch requested='" + requestedPath
                     + "' resolved='" + resolvedPath + "'");

    uiOnlyMode_ = false;
    debugUiGlueMenu_ = false;
    mapPath_ = resolvedPath;
    config_.localPlayerIndex = localPlayerIndex;

    if (fs::exists(mapPath_)) {
        mpq_.OpenOnTop(mapPath_);
    }

    if (!debugHudGameMode_) {
        scene_.Init(mpq_);
        splatMeta_.Load(mpq_);
    }

    if (!debugHudGameMode_) {
        audio_.Init();
    }
    if (!debugHudGameMode_ && audio_.IsValid() && mpq_.ArchiveCount() > 0) {
        audio_.SetMpqManager(&mpq_);
        audio_.LoadSound("ui_click", "UI\\Mouse\\HumanClick1.wav", &mpq_);
        audio_.LoadSound("ui_error", "UI\\Mouse\\HumanClickError.wav", &mpq_);
        uiSounds_.LoadFromMpq(mpq_);
    }

    hideHud_ = false;
    if (!ui_.SwitchHudLayout(&mpq_, screenW, screenH, "console", config_.gameVersion)) {
        hideHud_ = true;
        MoveDebugLogLine("GlueMapLaunch: HUD reload failed; continuing map launch with HUD hidden");
    } else {
        MoveDebugLogLine("GlueMapLaunch: HUD reloaded for gameplay");
        ui_.SetGlueMapLaunchCallback([this](const std::string& path, int localPlayer) {
            HandleGlueMapLaunchRequest(path, localPlayer);
        });
    }

    state_ = GameState::BOOTSTRAP;
    pendingState_ = GameState::BOOTSTRAP;
    stateTransition_ = false;
    mapConfigInitialized_ = false;
    loadingInitialized_ = false;
    loadingFailed_ = false;
    loadingFailureReason_.clear();
    simulationInitialized_ = false;
    terrainLoaded_ = false;
    glueMapLaunchInProgress_ = true;
    TransitionTo(GameState::MAP_CONFIG);
    MoveDebugLogLine("GlueMapLaunch: runtime map launch queued path='" + mapPath_ + "'");
    return true;
}

void GameController::ReadMapFile(const std::string& mapPath, const std::string& fileName,
                                  std::vector<uint8_t>& out) {
#ifndef WITHOUT_STORMLIB
    HANDLE hMpq = nullptr;
    if (!SFileOpenArchive(mapPath.c_str(), 0, MPQ_OPEN_READ_ONLY, &hMpq)) return;
    HANDLE hFile = nullptr;
    if (!SFileOpenFileEx(hMpq, fileName.c_str(), SFILE_OPEN_BASE_FILE, &hFile)) {
        SFileCloseArchive(hMpq); return;
    }
    DWORD sz = SFileGetFileSize(hFile, nullptr);
    out.resize(sz);
    DWORD read = 0;
    SFileReadFile(hFile, out.data(), sz, &read, nullptr);
    SFileCloseFile(hFile);
    SFileCloseArchive(hMpq);
#else
    (void)mapPath; (void)fileName; (void)out;
#endif
}

// --- State machine ---

void GameController::TransitionTo(GameState newState) {
    pendingState_ = newState;
    stateTransition_ = true;
}

void GameController::OnEnterState(GameState state) {
    switch (state) {
    case GameState::MAP_CONFIG: InitMapConfig(); break;
    case GameState::LOADING:    InitLoading();   break;
    case GameState::IN_GAME:    InitInGame();    break;
    default: break;
    }
}

void GameController::OnLeaveState(GameState state) { (void)state; }

// --- Init phases ---

void GameController::InitMapConfig() {
    if (mapConfigInitialized_) {
        std::cout << "InitMapConfig: already initialized; skipping duplicate config phase" << std::endl;
        if (!loadingFailed_ && !loadingInitialized_ && !simulationInitialized_) {
            TransitionTo(GameState::LOADING);
        }
        return;
    }

    if (mapPath_.empty() || !fs::exists(mapPath_)) {
        loadingFailed_ = true;
        loadingFailureReason_ = "Map file not found: " + mapPath_;
        std::cerr << loadingFailureReason_ << std::endl;
        return;
    }

    if (!debugHudGameMode_) {
        scene_.ResetLoadStats();
    }

    W3ILoadFromMpq(mapPath_, w3i_);
    W3IPrint(w3i_);

    // Map terrain fog (ref useTerrainFog): w3i colors are 0-255 bytes.
    if (w3i_.useTerrainFog) {
        dayNight_.SetMapFog(true,
            w3i_.fogColor[0] / 255.0f, w3i_.fogColor[1] / 255.0f,
            w3i_.fogColor[2] / 255.0f, w3i_.fogDensity);
        dayNight_.SetMapFogRange(w3i_.fogHeight[0], w3i_.fogHeight[1]);
    } else {
        dayNight_.SetMapFog(false, 0.5f, 0.5f, 0.5f, 0.0f);
    }

    configEnv_ = std::make_unique<JassConfigEnv>();
    if (configEnv_->Init(mpq_, mapPath_, config_.jassFileList)) {
        configEnv_->Config();
        int configuredCount = configEnv_->GetConfiguredPlayerCount();
        if (configuredCount > 0) {
            std::cout << "JASS Config: " << configuredCount << " players configured" << std::endl;
        }
        // Campaign maps put the human on a non-zero slot (Human01: Player 1 =
        // MAP_CONTROL_USER). With no explicit --map local player, follow the
        // first USER slot instead of blindly using slot 0 (ref SetPlayerController).
        if (config_.localPlayerIndex < 0) {
            if (auto cfgData = configEnv_->GetData()) {
                for (int i = 0; i < (int)cfgData->playerMapConfig.size(); ++i) {
                    if (cfgData->playerMapConfig[i].controller == 0 /* MAP_CONTROL_USER */) {
                        config_.localPlayerIndex = i;
                        std::cout << "LocalPlayer: auto-selected USER slot " << i << " from config" << std::endl;
                        break;
                    }
                }
            }
        }
        // Single-player slot overrides (CLI --ai/--closed): applied after
        // config() so map-defined starts/teams survive; only the controller
        // and slot-active flag are rewritten (ref SetPlayerController).
        if (auto cfgData = configEnv_->GetData()) {
            for (int slot : config_.aiPlayerSlots) {
                if (slot >= 0 && slot < (int)cfgData->playerMapConfig.size()) {
                    cfgData->playerMapConfig[(size_t)slot].controller = 1; // MAP_CONTROL_COMPUTER
                    cfgData->playerMapConfig[(size_t)slot].slotActive = true;
                    std::cout << "SlotOverride: player " << slot << " -> COMPUTER AI" << std::endl;
                }
            }
            for (int slot : config_.closedPlayerSlots) {
                if (slot >= 0 && slot < (int)cfgData->playerMapConfig.size()) {
                    cfgData->playerMapConfig[(size_t)slot].controller = 5; // MAP_CONTROL_NONE
                    cfgData->playerMapConfig[(size_t)slot].slotActive = false;
                    std::cout << "SlotOverride: player " << slot << " -> CLOSED" << std::endl;
                }
            }
        }
    }

    mapConfigInitialized_ = true;
    TransitionTo(GameState::LOADING);
}

void GameController::InitLoading() {
    if (simulationInitialized_) {
        std::cout << "InitLoading: simulation already initialized; skipping duplicate loader creation" << std::endl;
        return;
    }
    if (loadingFailed_) {
        std::cerr << "InitLoading: skipped because loading already failed: "
                  << loadingFailureReason_ << std::endl;
        return;
    }
    if (loadingInitialized_) {
        std::cout << "InitLoading: already initialized; skipping duplicate loader creation" << std::endl;
        return;
    }

    mapLoader_ = std::make_unique<MapLoader>(
        mpq_, simulation_, unitData_, destructableData_, doodadData_,
        itemData_, upgradeData_, abilityData_, scene_, texturedTerrain_, tileset_, map_, w3i_, wpm_);
    mapLoader_->SetConfigData(configEnv_ ? configEnv_->GetData() : nullptr);
    mapLoader_->SetLoadRenderAssets(!debugHudGameMode_ && !diagnoseMapOnly_ && !debugGameplaySmoke_);
    mapLoader_->CreateTasks(mapPath_, 0);
    if (!hideHud_) {
        ui_.ApplyMapAbilityMods(mapPath_);
    }
    loadingProgress_ = 0.0f;
    loadingInitialized_ = true;
    std::cout << "Loading started..." << std::endl;
}

void GameController::InitInGame() {
    if (simulationInitialized_) {
        std::cout << "InitInGame: already initialized; skipping duplicate entry" << std::endl;
        return;
    }

    if (loadingFailed_) {
        std::cerr << "InitInGame: skipped because loading failed: "
                  << loadingFailureReason_ << std::endl;
        return;
    }
    if (!mapLoader_) {
        loadingFailed_ = true;
        loadingFailureReason_ = "MapLoader was not initialized";
        std::cerr << "InitInGame: " << loadingFailureReason_ << std::endl;
        return;
    }
    if (mapLoader_->HasFatalError()) {
        loadingFailed_ = true;
        loadingFailureReason_ = mapLoader_->GetFatalError();
        std::cerr << "InitInGame: map loading failed: "
                  << loadingFailureReason_ << std::endl;
        return;
    }
    if (!debugHudGameMode_ && !diagnoseMapOnly_ && !debugGameplaySmoke_ && !texturedTerrain_.IsValid()) {
        loadingFailed_ = true;
        loadingFailureReason_ = "Terrain was not built";
        std::cerr << "InitInGame: " << loadingFailureReason_ << std::endl;
        return;
    }

    if (mapLoader_) {
        for (const auto& seed : mapLoader_->GetUnitVisualSeeds()) {
            simUnitVisualSeeds[seed.handleId] = {
                {seed.scale[0], seed.scale[1], seed.scale[2]},
                seed.skinId, seed.hitpoints, seed.flags
            };
        }
        for (const auto& seed : mapLoader_->GetItemVisualSeeds()) {
            if (seed.handleId >= 0 && seed.visualIndex >= 0) {
                simItemToVisualIdx[seed.handleId] = seed.visualIndex;
            }
        }
        const auto& unitLoadResult = mapLoader_->GetUnitLoadResult();
        int visualUnitCreated = unitLoadResult.visualCreated;
        int visualMissForSimUnits = unitLoadResult.visualMissForSimUnits;
        simulation_.Update();
        simulationInitialized_ = true;
        ui_.SetSimulation(&simulation_);
        if (!debugHudGameMode_ && !diagnoseMapOnly_ && !debugGameplaySmoke_) {
            for (auto* unit : simulation_.GetUnits()) {
                if (EnsureSimulationUnitVisual(unit))
                    visualUnitCreated++;
                else
                    visualMissForSimUnits++;
            }
        }
        std::cout << "Simulation: " << simulation_.GetUnits().size() << " units" << std::endl;
        std::cout << "SceneUnitSync: simCreated=" << unitLoadResult.simCreated
                  << " visualCreated=" << visualUnitCreated
                  << " visualMissForSimUnits=" << visualMissForSimUnits << std::endl;

        ui_.SetUnitData(&unitData_);
        if (!simulation_.GetUnits().empty()) {
            War3ID firstType = simulation_.GetUnits()[0]->GetTypeId();
            ui_.PopulateCommandCardForUnitType(firstType);
        }
    }

    terrainLoaded_ = texturedTerrain_.IsValid();

    if (!debugHudGameMode_ && !diagnoseMapOnly_ && !debugGameplaySmoke_) {
        minimap_.Generate(texturedTerrain_, tileset_, map_, w3i_, &mpq_);

        Vec3 center = texturedTerrain_.GetCenter();
        camera_.SetTarget(center.x, center.y, center.z);
        camera_.SetDistance(2500.0f);
        camera_.SetMaxDistance(5000.0f);
        camera_.SaveHomeState();
        if (overviewCamera_) {
            camera_.SetTarget(center.x, center.y, center.z);
            camera_.SetMaxDistance(20000.0f);
            camera_.SetDistance(11000.0f);
            camera_.SetAngles(0.8f, 1.35f);
            camera_.SaveHomeState();
            std::cout << "OverviewCamera: map center (" << center.x << ", " << center.y << ", " << center.z << ")" << std::endl;
        }
        // Spawn focus runs after JASS main + melee fallback below (town
        // halls/workers must exist for FindInitialSpawnFocusPosition).
        // Camera stays map-centered until then.
    }

    if (debugHudGameMode_) {
        std::cout << "JASS Common: skipped for HUD game debug" << std::endl;
    } else if (mpq_.ArchiveCount() > 0) {
        commonEnv_ = std::make_unique<JassCommonEnv>();
        if (commonEnv_->Init(mpq_, mapPath_, &simulation_, config_.jassFileList,
                             configEnv_ ? configEnv_->GetData() : nullptr)) {
            // Map-init dedupe window: units.doo placement and the map
            // script's CreateNeutral*/CreatePlayer* overlap; fold exact
            // duplicates instead of doubling creeps/mines (ref
            // CSimulation::SetMapInitPhase).
            simulation_.SetMapInitPhase(true);
            commonEnv_->Main();
            commonEnv_->PumpQueuedThreads(4096);
            jassNatives_ = commonEnv_->ReleaseNatives();
            WireJassCallbacks();
            if (jassNatives_) jassNatives_->PumpQueuedThreads(4096);
            simulation_.Update();
            if (jassNatives_) {
                jassNatives_->UpdateTimers(CSimulation::SIM_STEP_TIME);
                jassNatives_->UpdateRegions();
                jassNatives_->UpdateRescue();
                jassNatives_->UpdateTextTags(CSimulation::SIM_STEP_TIME);
                jassNatives_->PumpQueuedThreads(1024);
            }
            EnsureHumanMeleeStartsFromMapConfig();
            SpawnCarriedHeroes();
            FlushStartupSimulationState();
            EnsureComputerAIStarted();
            simulation_.SetMapInitPhase(false);
            // Real-game opening: focus the local start (town hall) now that
            // JASS main + melee fallback have created the starting units.
            // Previously this ran before spawns existed, silently failed, and
            // left the camera over unexplored map center (all-black fog).
            if (!overviewCamera_ && !debugHudGameMode_ && !diagnoseMapOnly_ && !debugGameplaySmoke_) {
                FocusCameraAndMinimapOnInitialSpawn();
            }
        } else {
            std::cerr << "JASS Common: unavailable; terrain/static scene remains ready" << std::endl;
        }
    } else {
        std::cout << "JASS Common: skipped because no resource MPQs are open" << std::endl;
    }

    if (debugSelectFirstUnitForHud_) {
        DebugSelectFirstUnitForHud();
    }

    if (dumpUnsupported_) {
        DumpUnsupportedDiagnostics();
    }

    if (debugGameplaySmoke_) {
        RunGameplaySmoke();
        std::cout << "GameplaySmoke: complete; requesting quit." << std::endl;
        sapp_request_quit();
    }

    if (diagnoseMapOnly_) {
        std::cout << "Diagnose-map complete; requesting quit." << std::endl;
        sapp_request_quit();
    }

    if (glueMapLaunchInProgress_) {
        std::ostringstream done;
        done << "GlueMapLaunch: entered map path='" << mapPath_
             << "' terrain=" << (texturedTerrain_.IsValid() ? "yes" : "no")
             << " units=" << simulation_.GetUnits().size()
             << " players=" << simulation_.GetPlayerCount();
        MoveDebugLogLine(done.str());
        glueMapLaunchInProgress_ = false;
    }

    std::cout << "Initialization complete. Starting render loop..." << std::endl;
}

void GameController::DumpUnsupportedDiagnostics() {
    std::cout << "=== Map Diagnostics ===" << std::endl;
    std::cout << "map=" << mapPath_ << std::endl;
    std::cout << "archives=" << mpq_.ArchiveCount() << std::endl;
    std::cout << "terrain valid=" << (texturedTerrain_.IsValid() ? "yes" : "no")
              << " size=" << map_.tileWidth << "x" << map_.tileHeight
              << " tileset=" << map_.tileset
              << " corners=" << map_.tiles.size() << std::endl;

    if (mapLoader_) {
        const auto& unitLoad = mapLoader_->GetUnitLoadResult();
        std::cout << "map_units total=" << unitLoad.mapUnitCount
                  << " sim_created=" << unitLoad.simCreated
                  << " visual_created=" << unitLoad.visualCreated
                  << " visual_miss_for_sim_units=" << unitLoad.visualMissForSimUnits
                  << " items_registered=" << unitLoad.itemRegistered << std::endl;
    }

    std::cout << "simulation units=" << simulation_.GetUnits().size()
              << " destructables=" << simulation_.GetDestructables().size()
              << " items=" << simulation_.GetItems().size()
              << " players=" << simulation_.GetPlayerCount() << std::endl;

    std::unordered_map<std::string, int> unknownFunctions;
    std::unordered_map<std::string, int> parserErrorCounts;
    int parserErrors = 0;
    int lexerErrors = 0;
    int otherErrors = 0;
    if (configEnv_) {
        AddJassErrorStats(configEnv_->GetVM().GetErrors(), unknownFunctions, parserErrorCounts,
                          parserErrors, lexerErrors, otherErrors);
    }
    if (commonEnv_) {
        AddJassErrorStats(commonEnv_->GetVM().GetErrors(), unknownFunctions, parserErrorCounts,
                          parserErrors, lexerErrors, otherErrors);
    }

    int totalUnknownCalls = 0;
    for (const auto& entry : unknownFunctions) {
        totalUnknownCalls += entry.second;
    }
    std::cout << "jass_unknown_unique=" << unknownFunctions.size()
              << " jass_unknown_calls=" << totalUnknownCalls
              << " parser_errors=" << parserErrors
              << " lexer_errors=" << lexerErrors
              << " other_errors=" << otherErrors << std::endl;
    PrintTopJassUnknowns(unknownFunctions);
    PrintTopJassParserErrors(parserErrorCounts);
    if (configEnv_) {
        PrintJassStaticDiagnosticsForVm("config", configEnv_->GetVM());
    }
    if (commonEnv_) {
        PrintJassStaticDiagnosticsForVm("common", commonEnv_->GetVM());
    }
    std::cout << "=== End Map Diagnostics ===" << std::endl;
}

void GameController::RunSyncInit() {
    if (uiOnlyMode_) {
        return;
    }

    InitMapConfig();
    if (loadingFailed_) {
        pendingState_ = state_;
        stateTransition_ = false;
        return;
    }
    InitLoading();
    if (loadingFailed_) {
        pendingState_ = state_;
        stateTransition_ = false;
        return;
    }
    int stage = 0;
    while (mapLoader_ && !mapLoader_->Process()) {
        stage++;
        std::cout << "  Loading stage " << stage << " ("
                  << (int)(mapLoader_->GetCompletionRatio() * 100.0f) << "%)" << std::endl;
    }
    if (!mapLoader_) {
        loadingFailed_ = true;
        loadingFailureReason_ = "MapLoader was not initialized";
        std::cerr << "RunSyncInit: " << loadingFailureReason_ << std::endl;
        pendingState_ = state_;
        stateTransition_ = false;
        return;
    }
    if (mapLoader_->HasFatalError()) {
        loadingFailed_ = true;
        loadingFailureReason_ = mapLoader_->GetFatalError();
        std::cerr << "RunSyncInit: map loading failed: "
                  << loadingFailureReason_ << std::endl;
        pendingState_ = state_;
        stateTransition_ = false;
        return;
    }
    terrainLoaded_ = texturedTerrain_.IsValid();
    if (!debugHudGameMode_ && !diagnoseMapOnly_ && !debugGameplaySmoke_ && !terrainLoaded_) {
        loadingFailed_ = true;
        loadingFailureReason_ = "Terrain was not built";
        std::cerr << "RunSyncInit: " << loadingFailureReason_ << std::endl;
        pendingState_ = state_;
        stateTransition_ = false;
        return;
    }
    if (!debugHudGameMode_ && !diagnoseMapOnly_ && !debugGameplaySmoke_) {
        scene_.PrintLoadStats();
    }
    InitInGame();
    if (!simulationInitialized_) {
        pendingState_ = state_;
        stateTransition_ = false;
        return;
    }

    state_ = GameState::IN_GAME;
    pendingState_ = GameState::IN_GAME;
    stateTransition_ = false;
}

// --- Wire JASS callbacks ---

void GameController::WireJassCallbacks() {
    if (!jassNatives_) return;

    auto* jassData = jassNatives_->GetData().get();
    if (!jassData) return;

    // Ref GetLocationZ reads the map terrain height; route JASS queries to
    // the live textured terrain (0 when no map is loaded).
    jassData->SetTerrainHeightCallback([this](float x, float y) -> float {
        float height = texturedTerrain_.IsValid() ? texturedTerrain_.GetGroundHeight(x, y) : 0.0f;
        if (jassNatives_ && jassNatives_->GetData()) {
            height += jassNatives_->GetData()->TerrainDeformOffset(x, y);
        }
        return height;
    });
    jassData->SetLocalPlayerIndex(config_.localPlayerIndex >= 0 ? config_.localPlayerIndex : 0);
    // Ref ForFiles enumerates the datasource listfile; route JASS queries
    // to the live archive set (empty when headless / unmounted).
    jassData->SetFileLister([this](const std::string& pattern) -> std::vector<std::string> {
        if (mpq_.ArchiveCount() == 0) return {};
        return mpq_.ListAllFiles(pattern);
    });
    // Ref doPreloadScript reads file bytes from the datasource.
    jassData->SetScriptReader([this](const std::string& path) -> std::vector<uint8_t> {
        std::vector<uint8_t> out;
        mpq_.ReadFile(path, out);
        return out;
    });

    jassData->camSetPosition = [this](float x, float y, float z) {
        camera_.SetTarget(x, y, z); camera_.ResetToHome();
    };
    jassData->camSetTarget = [this](float x, float y, float z) {
        camera_.SetTarget(x, y, z); camera_.ResetToHome();
    };
    jassData->camReset = [this]() { camera_.ResetToHome(); };
    jassData->camGetX = [this]() -> float { return camera_.GetTarget().x; };
    jassData->camGetY = [this]() -> float { return camera_.GetTarget().y; };
    jassData->camGetZ = [this]() -> float { return camera_.GetTarget().z; };
    jassData->camGetEyeX = [this]() -> float { return camera_.GetPosition().x; };
    jassData->camGetEyeY = [this]() -> float { return camera_.GetPosition().y; };
    jassData->camGetEyeZ = [this]() -> float { return camera_.GetPosition().z; };

    jassData->unitSetAnimation = [this](int simHandle, const std::string& animName) {
        animation_utils::PendingUnitAnimation request;
        request.sequenceName = animName;
        request.speedRatio = 1.0f;
        simUnitQueuedAnimations.erase(simHandle);
        auto it = simUnitToVisualIdx.find(simHandle);
        if (it != simUnitToVisualIdx.end()) {
            auto* viewer = scene_.GetViewer(it->second);
            if (viewer && ApplyAnimationRequestToViewer(simHandle, viewer, request, this)) {
                simUnitPendingAnimations.erase(simHandle);
                return;
            }
        }
        simUnitPendingAnimations[simHandle] = request;
    };
    jassData->unitQueueAnimation = [this](int simHandle, const std::string& animName) {
        animation_utils::PendingUnitAnimation request;
        request.sequenceName = animName;
        request.speedRatio = 1.0f;
        auto it = simUnitToVisualIdx.find(simHandle);
        if (it != simUnitToVisualIdx.end()) {
            auto* viewer = scene_.GetViewer(it->second);
            if (viewer && viewer->GetAnimator()) {
                bool shouldQueue = !viewer->IsCurrentSequenceFinished();
                if (shouldQueue) {
                    QueueUnitAnimationRequest(simHandle, request, this);
                    return;
                } else if (ApplyAnimationRequestToViewer(simHandle, viewer, request, this)) {
                    simUnitPendingAnimations.erase(simHandle);
                    return;
                }
            }
        } else {
            simUnitPendingAnimations[simHandle] = request;
        }
    };
    jassData->unitSetAnimationByIndex = [this](int simHandle, int index) {
        animation_utils::PendingUnitAnimation request;
        request.sequenceIndex = index;
        request.useSequenceIndex = true;
        request.speedRatio = 1.0f;
        simUnitQueuedAnimations.erase(simHandle);
        auto it = simUnitToVisualIdx.find(simHandle);
        if (it != simUnitToVisualIdx.end()) {
            auto* viewer = scene_.GetViewer(it->second);
            if (viewer && ApplyAnimationRequestToViewer(simHandle, viewer, request, this)) {
                simUnitPendingAnimations.erase(simHandle);
                return;
            }
        }
        simUnitPendingAnimations[simHandle] = request;
    };
    jassData->unitSetAnimationByIndexEx = [this](int simHandle, int index, float speedRatio) {
        animation_utils::PendingUnitAnimation request;
        request.sequenceIndex = index;
        request.useSequenceIndex = true;
        request.speedRatio = speedRatio;
        simUnitQueuedAnimations.erase(simHandle);
        auto it = simUnitToVisualIdx.find(simHandle);
        if (it != simUnitToVisualIdx.end()) {
            auto* viewer = scene_.GetViewer(it->second);
            if (viewer && ApplyAnimationRequestToViewer(simHandle, viewer, request, this)) {
                simUnitPendingAnimations.erase(simHandle);
                return;
            }
        }
        simUnitPendingAnimations[simHandle] = request;
    };
    jassData->unitSetAnimationByTags = [this](int simHandle,
                                              const std::string& primaryTag,
                                              const std::vector<std::string>& secondaryTags,
                                              float speedRatio,
                                              float durationSeconds,
                                              bool force) {
        animation_utils::PendingUnitAnimation request;
        request.primaryTag = primaryTag;
        request.secondaryTags = secondaryTags;
        request.useTags = true;
        request.force = force;
        request.speedRatio = speedRatio;
        request.durationSeconds = durationSeconds;
        if (force) {
            simUnitQueuedAnimations.erase(simHandle);
            simUnitScriptedAnimations.erase(simHandle);
        }
        auto it = simUnitToVisualIdx.find(simHandle);
        if (it != simUnitToVisualIdx.end()) {
            auto* viewer = scene_.GetViewer(it->second);
            if (viewer && ApplyAnimationRequestToViewer(simHandle, viewer, request, this)) {
                simUnitPendingAnimations.erase(simHandle);
                return;
            }
        }
        simUnitPendingAnimations[simHandle] = request;
    };
    jassData->unitQueueAnimationByTags = [this](int simHandle,
                                                const std::string& primaryTag,
                                                const std::vector<std::string>& secondaryTags) {
        animation_utils::PendingUnitAnimation request;
        request.primaryTag = primaryTag;
        request.secondaryTags = secondaryTags;
        request.useTags = true;
        request.speedRatio = 1.0f;
        auto it = simUnitToVisualIdx.find(simHandle);
        if (it != simUnitToVisualIdx.end()) {
            auto* viewer = scene_.GetViewer(it->second);
            if (viewer && viewer->GetAnimator()) {
                const bool shouldQueue = !viewer->IsCurrentSequenceFinished();
                if (shouldQueue) {
                    QueueUnitAnimationRequest(simHandle, request, this);
                    return;
                }
                if (ApplyAnimationRequestToViewer(simHandle, viewer, request, this)) {
                    simUnitPendingAnimations.erase(simHandle);
                    return;
                }
            }
        }
        simUnitPendingAnimations[simHandle] = request;
    };
    jassData->unitAddAnimationProperties = [this](int simHandle, const std::string& properties, bool add) {
        auto tags = animation_utils::ExtractAnimationSecondaryTags(properties);
        if (tags.empty()) return;
        auto& activeTags = simUnitAnimationPropertyTags[simHandle];
        if (add) {
            for (const auto& tag : tags)
                if (std::find(activeTags.begin(), activeTags.end(), tag) == activeTags.end())
                    activeTags.push_back(tag);
        } else {
            activeTags.erase(
                std::remove_if(activeTags.begin(), activeTags.end(),
                    [&](const std::string& t) { return std::find(tags.begin(), tags.end(), t) != tags.end(); }),
                activeTags.end());
            if (activeTags.empty()) simUnitAnimationPropertyTags.erase(simHandle);
        }
        simUnitVisualAnimationModes.erase(simHandle);
    };
    jassData->unitSetAnimationSecondaryTag = [this](int simHandle, const std::string& tag, bool add) -> bool {
        std::vector<std::string> tags{tag};
        auto& activeTags = simUnitAnimationPropertyTags[simHandle];
        bool changed = false;
        if (add) {
            if (std::find(activeTags.begin(), activeTags.end(), tag) == activeTags.end()) {
                activeTags.push_back(tag);
                changed = true;
            }
        } else {
            const auto oldSize = activeTags.size();
            activeTags.erase(std::remove(activeTags.begin(), activeTags.end(), tag), activeTags.end());
            changed = activeTags.size() != oldSize;
            if (activeTags.empty()) simUnitAnimationPropertyTags.erase(simHandle);
        }
        if (changed) simUnitVisualAnimationModes.erase(simHandle);
        return changed;
    };
    jassData->unitForceResetAnimation = [this](int simHandle) {
        simUnitPendingAnimations.erase(simHandle);
        auto it = simUnitToVisualIdx.find(simHandle);
        if (it != simUnitToVisualIdx.end()) {
            auto* viewer = scene_.GetViewer(it->second);
            if (viewer && ApplyNextQueuedUnitAnimation(simHandle, viewer, this)) {
                simUnitVisualAnimationModes.erase(simHandle);
                return;
            }
            const auto scriptIt = simUnitScriptedAnimations.find(simHandle);
            if (viewer && scriptIt != simUnitScriptedAnimations.end()) {
                auto request = scriptIt->second.request;
                request.force = true;
                simUnitScriptedAnimations.erase(scriptIt);
                if (ApplyAnimationRequestToViewer(simHandle, viewer, request, this)) {
                    simUnitVisualAnimationModes.erase(simHandle);
                    return;
                }
            }
        } else {
            auto queuedIt = simUnitQueuedAnimations.find(simHandle);
            if (queuedIt != simUnitQueuedAnimations.end() && !queuedIt->second.empty()) {
                simUnitPendingAnimations[simHandle] = queuedIt->second.front();
                queuedIt->second.pop_front();
                if (queuedIt->second.empty()) {
                    simUnitQueuedAnimations.erase(queuedIt);
                }
                return;
            }
        }
        simUnitScriptedAnimations.erase(simHandle);
        simUnitVisualAnimationModes.erase(simHandle);
    };
    jassData->unitClearAnimations = [this](int simHandle) {
        simUnitPendingAnimations.erase(simHandle);
        simUnitQueuedAnimations.erase(simHandle);
        simUnitScriptedAnimations.erase(simHandle);
        simUnitVisualAnimationModes.erase(simHandle);
    };

    simulation_.SetUnitDeathCallback(
        [this](int unitHandle, int killerHandle, int) {
            if (jassNatives_) {
                jassNatives_->FireDeathEvent(unitHandle, killerHandle, JASS_EVENT_UNIT_DEATH);
                if (CUnit* dead = simulation_.GetUnit(unitHandle)) {
                    if (dead->IsHero()) jassNatives_->FireRevivableEvent(unitHandle);
                }
            }
        }
    );

    simulation_.SetUnitDamageCallback(
        [this](int unitHandle, int sourceHandle, float damage) {
            if (jassNatives_) {
                jassNatives_->FireDamageEvent(unitHandle, sourceHandle, damage, JASS_EVENT_UNIT_DAMAGED);
            }
        }
    );

    simulation_.SetUnitOrderCallback(
        [this](int unitHandle, int orderId, int targetHandle, float x, float y) {
            if (jassNatives_) {
                jassNatives_->FireOrderEvent(unitHandle, orderId, targetHandle, x, y);
            }
        }
    );

    simulation_.SetItemEventCallback(
        [this](int unitHandle, int itemHandle, int eventType) {
            if (jassNatives_) {
                jassNatives_->FireItemEvent(unitHandle, itemHandle, eventType);
            }
        }
    );

    simulation_.SetUnitLoadedCallback(
        [this](int transportHandle, int passengerHandle) {
            if (jassNatives_) {
                jassNatives_->FireLoadedEvent(transportHandle, passengerHandle);
            }
        }
    );

    simulation_.SetUnitDecayCallback(
        [this](int unitHandle) {
            if (jassNatives_) {
                jassNatives_->FireDecayEvent(unitHandle);
            }
        }
    );

    simulation_.SetShopEventCallback(
        [this](int buyerHandle, int sellerHandle, int itemHandle) {
            if (jassNatives_) {
                jassNatives_->FireSellItemEvent(buyerHandle, sellerHandle, itemHandle);
            }
        }
    );

    simulation_.SetShopUnitEventCallback(
        [this](int buyerHandle, int sellerHandle, int hiredHandle) {
            if (jassNatives_) {
                jassNatives_->FireSellUnitEvent(buyerHandle, sellerHandle, hiredHandle);
            }
        }
    );

    simulation_.SetPawnEventCallback(
        [this](int buyerHandle, int sellerHandle, int itemHandle) {
            if (jassNatives_) {
                jassNatives_->FirePawnItemEvent(sellerHandle, buyerHandle, itemHandle);
            }
        }
    );

    simulation_.SetHeroSkillCallback(
        [this](int unitHandle, int skillId, int newLevel) {
            if (jassNatives_) {
                jassNatives_->FireHeroSkillEvent(unitHandle, skillId, newLevel);
            }
        }
    );

    // Queue lifecycle (kind 0 = train, 1 = research, 2 = upgrade morph,
    // 3 = hero revive; upgrades have no cancel path: started=false means
    // finished).
    simulation_.SetQueueEventCallback(
        [this](int buildingHandle, int typeId, int kind, bool started) {
            if (!jassNatives_) return;
            if (kind == 0) {
                if (started) {
                    jassNatives_->FireTrainStartEvent(buildingHandle, typeId);
                } else {
                    jassNatives_->FireTrainCancelEvent(buildingHandle, typeId);
                }
            } else if (kind == 1) {
                if (started) {
                    jassNatives_->FireResearchStartEvent(buildingHandle, typeId);
                } else {
                    jassNatives_->FireResearchCancelEvent(buildingHandle, typeId);
                }
            } else if (kind == 3) {
                if (started) {
                    jassNatives_->FireReviveStartEvent(buildingHandle, typeId);
                } else {
                    jassNatives_->FireReviveCancelEvent(buildingHandle, typeId);
                }
            } else {
                if (started) {
                    jassNatives_->FireUpgradeStartEvent(buildingHandle, typeId);
                } else {
                    jassNatives_->FireUpgradeFinishEvent(buildingHandle, typeId);
                }
            }
        }
    );

    simulation_.SetConstructEventCallback(
        [this](int buildingHandle, bool finished) {
            if (!jassNatives_) return;
            if (finished) {
                jassNatives_->FireConstructFinishEvent(buildingHandle);
            } else {
                jassNatives_->FireConstructCancelEvent(buildingHandle);
            }
        }
    );

    simulation_.SetOwnerChangedCallback(
        [this](int unitHandle, int prevOwnerIndex) {
            if (jassNatives_) {
                jassNatives_->FireChangeOwnerEvent(unitHandle, prevOwnerIndex);
            }
        }
    );

    simulation_.SetItemRemovedCallback(
        [this](int itemHandle) {
            if (jassNatives_) {
                jassNatives_->ForgetItem(itemHandle);
            }
        }
    );

    simulation_.SetUnitTrainedCallback(
        [this](int buildingHandle, int trainedHandle) {
            if (jassNatives_) {
                jassNatives_->FireTrainedEvent(buildingHandle, trainedHandle);
            }
        }
    );

    simulation_.SetUnitSummonedCallback(
        [this](int summonerHandle, int summonedHandle) {
            if (jassNatives_) {
                jassNatives_->FireSummonEvent(summonerHandle, summonedHandle);
            }
        }
    );

    simulation_.SetResearchFinishedCallback(
        [this](int unitHandle, int upgradeId) {
            if (jassNatives_) {
                jassNatives_->FireResearchEvent(unitHandle, upgradeId);
            }
        }
    );

    simulation_.SetHeroRevivedCallback(
        [this](int reviverHandle, int revivedHandle) {
            if (jassNatives_) {
                jassNatives_->FireReviveEvent(reviverHandle, revivedHandle);
            }
        }
    );

    simulation_.SetSpellEventCallback(
        [this](int casterHandle, int stage) {
            if (jassNatives_) {
                jassNatives_->FireSpellEvent(casterHandle, stage);
            }
        }
    );
}

// --- Update ---

void GameController::Update(int screenW, int screenH, float dt) {
    if (uiOnlyMode_) {
        if (pendingGlueMapLaunch_ && StartPendingGlueMapLaunch(screenW, screenH)) {
            // Continue into the normal state machine in this frame.
        } else {
            frameCount_++;
            return;
        }
    }

    if (uiOnlyMode_) {
        frameCount_++;
        return;
    }

    // Campaign ChangeLevel: JASS queued a map switch (ref ChangeLevel).
    // Resolve like a glue launch and reuse the runtime reload path so
    // campaign missions actually chain instead of stalling on victory.
    if (jassNatives_ && simulationInitialized_) {
        auto jd = jassNatives_->GetData();
        if (jd && !jd->pendingChangeLevel.empty()) {
            const std::string next = jd->pendingChangeLevel;
            jd->pendingChangeLevel.clear();
            fs::path projectRoot = PROJECT_SOURCE_DIR;
            fs::path resourcesDir = projectRoot / "resources";
            std::string resolved;
            if (ResolveMapPathForLoad(resourcesDir, mpq_, next, resolved) && !resolved.empty()) {
                pendingGlueMapLaunch_ = true;
                pendingGlueMapLaunchRequestedPath_ = next;
                pendingGlueMapLaunchResolvedPath_ = resolved;
                pendingGlueMapLaunchLocalPlayer_ = config_.localPlayerIndex;
                MoveDebugLogLine("ChangeLevel: queued campaign map switch to '" + resolved + "'");
                // Snapshot human heroes BEFORE the reload tears the sim down;
                // SpawnCarriedHeroes consumes them once the next map is up.
                CollectCarriedHeroes();
                StartPendingGlueMapLaunch(screenW, screenH);
                frameCount_++;
                return;
            } else {
                MoveDebugLogLine("ChangeLevel: could not resolve next map '" + next + "'");
            }
        }
    }

    if (stateTransition_) {
        OnLeaveState(state_);
        state_ = pendingState_;
        OnEnterState(state_);
        stateTransition_ = false;
    }

    switch (state_) {
    case GameState::LOADING: UpdateLoading(dt); break;
    case GameState::IN_GAME: UpdateInGame(screenW, screenH, dt); break;
    default: break;
    }

    frameCount_++;
}

void GameController::UpdateLoading(float dt) {
    (void)dt;
    if (loadingFailed_) return;
    if (mapLoader_ && mapLoader_->Process()) {
        if (mapLoader_->HasFatalError()) {
            loadingFailed_ = true;
            loadingFailureReason_ = mapLoader_->GetFatalError();
            std::cerr << "UpdateLoading: map loading failed: "
                      << loadingFailureReason_ << std::endl;
            return;
        }
        terrainLoaded_ = texturedTerrain_.IsValid();
        if (!debugHudGameMode_ && !diagnoseMapOnly_ && !debugGameplaySmoke_ && !terrainLoaded_) {
            loadingFailed_ = true;
            loadingFailureReason_ = "Terrain was not built";
            std::cerr << "UpdateLoading: " << loadingFailureReason_ << std::endl;
            return;
        }
        if (!debugHudGameMode_ && !diagnoseMapOnly_ && !debugGameplaySmoke_) {
            scene_.PrintLoadStats();
        }
        TransitionTo(GameState::IN_GAME);
    }
}

void GameController::UpdateInGame(int screenW, int screenH, float dt) {
    // Camera from mouse state
    if (mouseDown[0] || mouseDown[2]) {
        camera_.OnMouseMove(mouseDX, mouseDY, mouseDown[0], mouseDown[2]);
    }
    if (mouseScroll != 0.0f) {
        camera_.OnMouseWheel(mouseScroll);
    }
    mouseDX = 0; mouseDY = 0; mouseScroll = 0;

    // Keyboard WASD panning
    float panSpeed = std::max(100.0f, camera_.GetPosition().y * 0.5f) * dt;
    float panX = 0, panZ = 0;
    if (Platform::IsKeyDown(SAPP_KEYCODE_W) || Platform::IsKeyDown(SAPP_KEYCODE_UP))    panZ += panSpeed;
    if (Platform::IsKeyDown(SAPP_KEYCODE_S) || Platform::IsKeyDown(SAPP_KEYCODE_DOWN))  panZ -= panSpeed;
    if (Platform::IsKeyDown(SAPP_KEYCODE_A) || Platform::IsKeyDown(SAPP_KEYCODE_LEFT))  panX -= panSpeed;
    if (Platform::IsKeyDown(SAPP_KEYCODE_D) || Platform::IsKeyDown(SAPP_KEYCODE_RIGHT)) panX += panSpeed;
    if (panX != 0 || panZ != 0) camera_.Pan(panX, panZ);

    // Player start position jump (keys 1-4)
    for (int k = SAPP_KEYCODE_1; k <= SAPP_KEYCODE_4; k++) {
        if (Platform::WasKeyPressed(k)) {
            int pi = k - SAPP_KEYCODE_1;
            float fx = 0.25f + (pi % 2) * 0.5f;
            float fy = 0.25f + (pi / 2) * 0.5f;
            float wx = w3i_.cameraBounds[0] + (w3i_.cameraBounds[1] - w3i_.cameraBounds[0]) * fx;
            float wy = w3i_.cameraBounds[2] + (w3i_.cameraBounds[3] - w3i_.cameraBounds[2]) * fy;
            wx *= 128.0f; wy *= 128.0f;
            float gh = texturedTerrain_.IsValid() ? texturedTerrain_.GetGroundHeight(wx, wy) : 0.0f;
            camera_.SetTarget(wx, gh + 100.0f, wy);
            camera_.ResetToHome();
        }
    }

    if (jassNatives_ && jassNatives_->GetData()) {
        const auto& noise = jassNatives_->GetData()->sourceNoise;
        camera_.SetNoise(noise.magnitude, noise.velocity, noise.vertOnly, noise.horizOnly);
        auto& rot = jassNatives_->GetData()->rotateMode;
        if (rot.active) {
            if (!camera_.StepRotateAround(rot.x, rot.y, rot.radians, rot.duration, dt)) {
                rot.active = false;
            }
        }
    }
    camera_.Update(dt);

    // Audio listener
    if (audio_.IsValid()) {
        Vec3 cp = camera_.GetPosition();
        audio_.SetListenerPosition(cp.x, cp.y, cp.z);
        float yaw = camera_.GetYaw(), pitch = camera_.GetPitch();
        float fx = std::sin(yaw) * std::cos(pitch);
        float fy = std::sin(pitch);
        float fz = std::cos(yaw) * std::cos(pitch);
        audio_.SetListenerOrientation(fx, fy, fz, 0.0f, 1.0f, 0.0f);
        audio_.Update();
    }

    // Day/night
    dayNight_.Update(dt);
    if (texturedTerrain_.IsValid()) dayNight_.Apply(texturedTerrain_);
    UpdateMidiAmbient(dt);
    SyncMusic();
    if (jassNatives_ && jassNatives_->GetData() && texturedTerrain_.IsValid()) {
        const auto& fog = jassNatives_->GetData()->globalUnitFog;
        if (fog.e > fog.d && fog.d >= 0.0f) {
            float color[3] = {fog.a, fog.b, fog.c};
            if (color[0] > 1.0f || color[1] > 1.0f || color[2] > 1.0f) {
                color[0] /= 255.0f; color[1] /= 255.0f; color[2] /= 255.0f;
            }
            texturedTerrain_.SetFog(color, fog.d, fog.e);
        }
    }
    if (ui_.GetResourceBar()) ui_.GetResourceBar()->SetTimeOfDay(dayNight_.GetTimeOfDay());

    for (auto it = activeEventSplats_.begin(); it != activeEventSplats_.end(); ) {
        it->lifeRemaining -= dt;
        if (it->lifeRemaining <= 0.0f) {
            it = activeEventSplats_.erase(it);
        } else {
            ++it;
        }
    }

    // Simulation step
    StepSimulation(dt);

    // Hovered unit raycast
    if (simulationInitialized_ && !sapp_mouse_locked() && !selectionDragVisible_) {
        const float bottomConsoleY = (float)screenH * 0.73f;
        if (mousePosY_ > 40.0f && mousePosY_ < bottomConsoleY && mousePosX_ >= 0.0f && mousePosX_ <= (float)screenW) {
            float aspect = (screenH > 0) ? (float)screenW / screenH : 1.0f;
            Mat4 view = camera_.GetViewMatrix();
            Mat4 proj = camera_.GetProjectionMatrix(aspect);
            CUnit* hovered = PickSimulationUnitAtScreenPoint(mousePosX_, mousePosY_, view, proj, screenW, screenH, -1, false, false);
            if (hovered && !hovered->IsDead() && !hovered->IsHidden()) {
                hoveredUnitHandle_ = hovered->GetHandleId();
                std::string hoverText;
                if (hovered->GetUnitType() && (hovered->GetUnitType()->GetTypeId() == War3ID("ngol") || hovered->GetAbility(War3ID("Agld")))) {
                    int gold = 13500;
                    if (auto* gm = dynamic_cast<CAbilityGoldMine*>(hovered->GetAbility(War3ID("Agld")))) {
                        gold = gm->GetGold();
                    }
                    hoverText = "Gold Mine\nGold: " + std::to_string(gold);
                } else if (hovered->GetUnitType()) {
                    hoverText = hovered->GetUnitType()->GetName();
                }
                if (!hoverText.empty()) {
                    ui_.ShowTooltip(hoverText, mousePosX_, mousePosY_, HudResourceCosts{});
                }
            } else {
                if (hoveredUnitHandle_ >= 0) ui_.HideTooltip();
                hoveredUnitHandle_ = -1;
            }
        } else {
            if (hoveredUnitHandle_ >= 0) ui_.HideTooltip();
            hoveredUnitHandle_ = -1;
        }
    } else {
        if (hoveredUnitHandle_ >= 0) ui_.HideTooltip();
        hoveredUnitHandle_ = -1;
    }

    // After sim step, sync visuals
    if (!debugHudGameMode_ && !diagnoseMapOnly_ && !debugGameplaySmoke_) {
        SyncAndReconcileVisuals();
        RefreshRuntimeUnitSplats();
    }
    RefreshSelectionUi(false);
}

void GameController::StepSimulation(float dt) {
    if (!simulationInitialized_) return;

    const float simStep = CSimulation::SIM_STEP_TIME;
    simulationAccumulator_ = std::min(simulationAccumulator_ + dt, simStep * 3.0f);
    bool ranSimulationStep = false;
    while (simulationAccumulator_ >= simStep) {
        simulation_.Update();
        if (jassNatives_) {
            jassNatives_->UpdateTimers(simStep);
            jassNatives_->UpdateRegions();
            jassNatives_->UpdateRescue();
            jassNatives_->UpdateAI();
            jassNatives_->PumpQueuedThreads(1024);
        }
        simulationAccumulator_ -= simStep;
        ranSimulationStep = true;
    }

    if (ranSimulationStep) {
        if (!debugHudGameMode_ && !diagnoseMapOnly_ && !debugGameplaySmoke_) {
            SyncAndReconcileVisuals();
        }
        SyncFogOfWar();
        SyncLightnings();
        SyncScriptDecor();
        RefreshSelectionUi(false);
        MaybeLogAIStatus();
    }
}

void GameController::SyncFogOfWar() {
    if (!simulationInitialized_ || !terrainLoaded_) return;
    if (std::getenv("WARSMASH_NO_FOG")) return; // diagnostic: leave fog texture cleared (all visible)
    const uint64_t version = simulation_.GetFogSystem()->Version();
    if (version == fogVersionSynced_) return;
    fogVersionSynced_ = version;
    const int localPlayer = config_.localPlayerIndex >= 0 ? config_.localPlayerIndex : 0;
    // Fog must rasterize over the instance-grid extents (ref ground shader
    // v_suv), not the camera bounds: otherwise visible blobs shift away from
    // their units and spawn stays black.
    float bounds[4] = {-4096, -4096, 4096, 4096};
    texturedTerrain_.GetFogMapBounds(bounds);
    constexpr int kFogGrid = 128;
    fogGrid_.assign(kFogGrid * kFogGrid, 0);
    simulation_.GetFogSystem()->Rasterize(localPlayer, kFogGrid, kFogGrid,
                                          bounds[0], bounds[1], bounds[2], bounds[3],
                                          fogGrid_.data());
    texturedTerrain_.SetFogOfWarData(fogGrid_.data(), kFogGrid, kFogGrid);
}

void GameController::SyncMinimapLayer() {
    if (!simulationInitialized_ || !minimap_.IsValid()) return;
    const int local = config_.localPlayerIndex >= 0 ? config_.localPlayerIndex : 0;
    const CFogSystem* fog = simulation_.GetFogSystem();

    // --- Fog overlay (ref MeleeUIMinimap fog pass) ---
    // Rasterize over the minimap world bounds at modest resolution; row 0 =
    // worldMinY matches Minimap::Generate() pixel order so the overlay aligns
    // with the base image and WorldToScreen dots.
    {
        const uint64_t version = fog ? fog->Version() : 0;
        float minX = 0, minY = 0, maxX = 1, maxY = 1;
        minimap_.GetWorldBounds(minX, minY, maxX, maxY);
        constexpr int kFogW = 96;
        constexpr int kFogH = 96;
        if (version != minimapFogVersionSynced_ || minimapFogRgba_.empty()) {
            minimapFogVersionSynced_ = version;
            std::vector<unsigned char> grid((size_t)kFogW * kFogH, 0);
            if (fog) {
                fog->Rasterize(local, kFogW, kFogH, minX, minY, maxX, maxY, grid.data());
            }
            minimapFogRgba_.assign((size_t)kFogW * kFogH * 4, 0);
            for (int i = 0; i < kFogW * kFogH; ++i) {
                const unsigned char v = grid[(size_t)i];
                uint8_t alpha = 0;
                if (v >= 200) alpha = 255;       // MASKED: opaque black
                else if (v >= 64) alpha = 128;    // FOGGED: 50% black
                minimapFogRgba_[(size_t)i * 4 + 0] = 0;
                minimapFogRgba_[(size_t)i * 4 + 1] = 0;
                minimapFogRgba_[(size_t)i * 4 + 2] = 0;
                minimapFogRgba_[(size_t)i * 4 + 3] = alpha;
            }
            minimap_.SetFogOverlay(kFogW, kFogH, minimapFogRgba_);
        }
    }

    // --- Unit dots (ref MeleeUIMinimap unit pass) ---
    static const uint32_t kMinimapPlayerColors[12] = {
        0xFFFF0000u, 0xFF0042FFu, 0xFF1CE6B6u, 0xFF540081u,
        0xFFFFFC00u, 0xFFFE8A0Eu, 0xFF20C000u, 0xFFE55BB0u,
        0xFF959697u, 0xFF7EBFF1u, 0xFF106246u, 0xFF4F2B05u,
    };
    auto argbToRgba = [](uint32_t argb) -> uint32_t {
        return ((argb & 0x00FFFFFFu) << 8) | ((argb >> 24) & 0xFFu);
    };
    // Hero pulse 0.50..0.95 triangle wave over ~80 frames (ref heroAlpha).
    const int kPulsePeriod = 80;
    const int pulseT = frameCount_ % (2 * kPulsePeriod);
    const float pulseTri = pulseT < kPulsePeriod
        ? (float)pulseT / (float)kPulsePeriod
        : (float)(2 * kPulsePeriod - pulseT) / (float)kPulsePeriod;
    const float heroAlpha = 0.50f + 0.45f * pulseTri;
    const uint8_t heroAlphaByte = (uint8_t)std::clamp((int)(heroAlpha * 255.0f), 0, 255);

    const CPlayer* localPlayer = simulation_.GetPlayer(local);
    minimapDotsScratch_.clear();
    minimapDotsScratch_.reserve(simulation_.GetUnits().size());
    for (const CUnit* unit : simulation_.GetUnits()) {
        if (!unit || unit->IsDead() || unit->IsHidden() || unit->IsLoadedAboard()) continue;
        if (!IsUnitVisibleInFog(unit)) continue;
        const float ux = unit->GetX();
        const float uy = unit->GetY();
        const int owner = unit->GetPlayerIndex();
        const CUnitType* type = unit->GetUnitType();

        const bool hasOverlayMine = unit->GetAbility(War3ID("Agl2")) != nullptr
            || unit->GetAbility(War3ID("Aegm")) != nullptr
            || unit->GetAbility(War3ID("Abgm")) != nullptr;
        const bool hasBaseMine = !hasOverlayMine
            && unit->GetAbility(War3ID("Agld")) != nullptr;
        const bool isNeutralIcon = type && type->IsNeutralBuildingShowMinimapIcon();
        Minimap::UnitDot dot;
        dot.worldX = ux;
        dot.worldY = uy;
        if (hasBaseMine) {
            dot.rgba = 0xFFCC00FFu;      // gold mine (yellow, opaque)
            dot.sizePixels = 11.0f;
        } else if (hasOverlayMine) {
            const bool entangled = unit->GetAbility(War3ID("Aegm")) != nullptr
                || unit->GetAbility(War3ID("Aenc")) != nullptr;
            const bool haunted = unit->GetAbility(War3ID("Abgm")) != nullptr;
            if (entangled) dot.rgba = 0x00E676FFu;       // entangled (green)
            else if (haunted) dot.rgba = 0xE040FBFFu;    // haunted (purple)
            else dot.rgba = 0xFFCC00FFu;
            dot.sizePixels = 11.0f;
        } else if (isNeutralIcon) {
            dot.rgba = 0xFFF59DFFu;      // neutral special (pale yellow)
            dot.sizePixels = 11.0f;
        } else if (unit->IsHero()) {
            bool allied = false;
            if (localPlayer && owner >= 0) {
                if (const CPlayer* other = simulation_.GetPlayer(owner)) {
                    allied = localPlayer->HasAlliance(*other, CAllianceType::PASSIVE);
                } else {
                    allied = (owner == local);
                }
            } else {
                allied = (owner == local);
            }
            if (allied) {
                dot.rgba = (0xFFFFFF00u | heroAlphaByte);  // white pulse
            } else {
                dot.rgba = (0xFF333300u | heroAlphaByte);  // red pulse
            }
            dot.sizePixels = 14.0f;
        } else {
            uint32_t argb = 0xFF888888u;
            if (owner >= 0 && owner < 12) argb = kMinimapPlayerColors[owner];
            dot.rgba = argbToRgba(argb);
            dot.sizePixels = (type && type->IsBuilding()) ? 7.0f : 4.0f;
        }
        minimapDotsScratch_.push_back(dot);
    }
    minimap_.SetUnitDots(minimapDotsScratch_);

    // Script ping markers (ref MeleeUI minimap pings): live JASS pings draw
    // on top; aging/expiry happens in JassNatives::UpdateTimers.
    minimap_.ClearScriptMarkers();
    if (jassNatives_ && jassNatives_->GetData()) {
        for (const auto& ping : jassNatives_->GetData()->minimapPings) {
            const auto clampByte = [](int v) -> uint32_t {
                return (uint32_t)std::clamp(v, 0, 255);
            };
            const uint32_t rgba = (clampByte(ping.red) << 24) | (clampByte(ping.green) << 16) |
                                  (clampByte(ping.blue) << 8) | 0xFFu;
            minimap_.AddScriptMarker(ping.x, ping.y, rgba, ping.extra ? 14.0f : 10.0f);
        }
    }
}

void GameController::SyncHudLiveData() {
    if (!simulationInitialized_) return;
    const int local = config_.localPlayerIndex >= 0 ? config_.localPlayerIndex : 0;
    // Resource bar + upkeep + clock from the local player (ref melee UI).
    if (const CPlayer* player = simulation_.GetPlayer(local)) {
        const float hod = simulation_.GetGameTimeOfDay();
        const int totalMin = (int)(hod * 60.0f);
        char clock[16] = {};
        std::snprintf(clock, sizeof(clock), "%02d:%02d", (totalMin / 60) % 24, totalMin % 60);
        ui_.SetLiveResources(player->GetGold(), player->GetLumber(), player->GetFoodUsed(),
                             player->GetFoodCap(), clock, player->GetUpkeepLevel());
    }
    // Timer dialog: first displayed JASS dialog wins (ref CTimerDialog).
    std::string title, value;
    bool show = false;
    if (jassNatives_ && jassNatives_->GetData()) {
        auto data = jassNatives_->GetData();
        for (const auto& [id, dialog] : data->timerDialogs) {
            (void)id;
            if (!dialog || !dialog->displayed) continue;
            float remain = 0.0f;
            auto tit = data->timers.find(dialog->timerSimId);
            if (tit != data->timers.end() && tit->second) remain = std::max(0.0f, tit->second->remaining);
            const int totalSec = (int)remain;
            char buf[16] = {};
            std::snprintf(buf, sizeof(buf), "%d:%02d", totalSec / 60, totalSec % 60);
            title = dialog->title;
            value = buf;
            show = true;
            break;
        }
    }
    ui_.SetTimerDialog(title, value, show);
    // Cinematic presentation: letterbox follows the mode flag; the subtitle
    // panel follows the live scene while it carries speaker text or dialogue.
    bool cineMode = false;
    std::string speaker, dialogue;
    bool cineVisible = false;
    if (jassNatives_ && jassNatives_->GetData()) {
        auto data = jassNatives_->GetData();
        cineMode = data->cinematicMode;
        if (data->cinematic.playing &&
            (!data->cinematic.speakerTitle.empty() || !data->cinematic.text.empty())) {
            speaker = data->cinematic.speakerTitle;
            dialogue = data->cinematic.text;
            cineVisible = true;
        }
    }
    ui_.SetCinematicMode(cineMode);
    ui_.SetCinematicScene(speaker, dialogue, cineVisible);
    // Cinematic filter: start->end color transition over duration (ref
    // CCineFilter); texture/UV/blend variants fall back to the color tint.
    {
        uint32_t rgba = 0;
        bool showFilter = false;
        if (jassNatives_ && jassNatives_->GetData()) {
            const auto& filter = jassNatives_->GetData()->cineFilter;
            if (filter.displayed) {
                const float t = filter.duration > 0.0f
                    ? std::clamp(filter.age / filter.duration, 0.0f, 1.0f)
                    : 1.0f;
                auto lerp = [t](int a, int b) {
                    return (uint32_t)std::clamp((int)(a + (b - a) * t), 0, 255);
                };
                rgba = (lerp(filter.startColor[3], filter.endColor[3]) << 24) |
                       (lerp(filter.startColor[0], filter.endColor[0]) << 16) |
                       (lerp(filter.startColor[1], filter.endColor[1]) << 8) |
                       lerp(filter.startColor[2], filter.endColor[2]);
                showFilter = (rgba >> 24) != 0;
            }
        }
        ui_.SetCineFilter(rgba, showFilter);
    }
    // Player text messages: forward local-player messages to HUD toasts;
    // sticky messages live in the UI until cleared. A ClearTextMessages
    // since the last push also clears already-shown toasts. Remote-player
    // messages are retained for future multiplayer routing.
    // Day/night cycle follows the JASS switch (ref EnableDawnDusk).
    if (jassNatives_ && jassNatives_->GetData()) {
        auto data = jassNatives_->GetData();
        if (data->textClearSeq != lastTextClearSeq_) {
            lastTextClearSeq_ = data->textClearSeq;
            ui_.ClearGameMessages();
        }
        auto& queue = data->textMessages;
        for (const auto& msg : queue) {
            if (msg.player == local || msg.player < 0) {
                ui_.ShowGameMessage(msg.text, msg.duration > 0.0f ? msg.duration : 5.0f);
            }
        }
        queue.erase(std::remove_if(queue.begin(), queue.end(),
                                   [local](const JassNativesData::JassTextMessage& msg) {
                                       return msg.player == local || msg.player < 0;
                                   }),
                    queue.end());
        dayNight_.SetEnabled(data->dawnDuskEnabled);
    }
}

void GameController::SyncLightnings() {
    if (!simulationInitialized_ || !jassNatives_) return;
    auto* lightning = scene_.GetLightning();
    if (!lightning) return;
    // One-shot retail-table override (custom maps may ship their own
    // Splats\LightningData.slk; the map MPQ mounts on top).
    if (!lightningParamsLoaded_ && mpq_.ArchiveCount() > 0) {
        lightningParamsLoaded_ = true;
        std::vector<uint8_t> bytes;
        if (mpq_.ReadFile("Splats\\LightningData.slk", bytes) && !bytes.empty()) {
            lightning->LoadParamsFromSlk(bytes.data(), bytes.size());
        }
    }
    const auto& bolts = jassNatives_->GetData()->lightnings;
    const int localPlayer = config_.localPlayerIndex >= 0 ? config_.localPlayerIndex : 0;
    std::unordered_set<int> live;
    for (const auto& [id, bolt] : bolts) {
        if (!bolt) continue;
        live.insert(id);
        // JASS (x, y, z-height) -> scene Y-up (x, z, y).
        lightning->SetBolt(id, bolt->x1, bolt->z1, bolt->y1,
                           bolt->x2, bolt->z2, bolt->y2,
                           bolt->r, bolt->g, bolt->b, bolt->a, bolt->code);
        bool show = true;
        if (bolt->checkVisibility) {
            show = simulation_.GetFogSystem()->IsVisibleTo(
                localPlayer, (bolt->x1 + bolt->x2) * 0.5f, (bolt->y1 + bolt->y2) * 0.5f);
        }
        lightning->SetBoltVisible(id, show);
    }
    for (auto it = lightningSynced_.begin(); it != lightningSynced_.end();) {
        if (!live.count(*it)) {
            lightning->RemoveBolt(*it);
            it = lightningSynced_.erase(it);
        } else {
            ++it;
        }
    }
    lightningSynced_ = std::move(live);
}

void GameController::SyncScriptDecor() {
    if (!jassNatives_ || !jassNatives_->GetData()) return;
    auto data = jassNatives_->GetData();

    static std::unordered_set<int> splatIds;
    static std::unordered_set<int> trackableIds;
    static size_t doodadCursor = 0;

    if (texturedTerrain_.IsValid()) {
        for (const auto& [id, splat] : data->ubersplats) {
            if (!splat || splatIds.count(id)) continue;
            if (!splat->render || splat->forcePaused) continue;
            texturedTerrain_.AddUberSplat(splat->name, splat->x, splat->y, 1.0f, false);
            splatIds.insert(id);
        }
    }

    if (auto* lightning = scene_.GetLightning()) {
        static std::vector<int> billboardIds;
        for (int id : billboardIds) lightning->RemoveBillboard(id);
        billboardIds.clear();
        for (const auto& [id, image] : data->images) {
            if (!image) continue;
            float ground = 0.0f;
            float water = 0.0f;
            if (texturedTerrain_.IsValid()) {
                ground = texturedTerrain_.GetGroundHeight(image->posX, image->posY);
                water = texturedTerrain_.GetWaterHeight(image->posX, image->posY);
            }
            const ImageBillboardPose pose = MakeImageBillboard(
                image->posX, image->posY, image->posZ,
                image->sizeX, image->sizeY,
                image->visible, image->render,
                image->constantHeight, image->aboveWater,
                ground, water,
                image->red, image->green, image->blue, image->alpha);
            if (!pose.visible) continue;
            const int bid = 0x72000000 + id;
            lightning->SetBillboard(bid, pose.x, pose.y, pose.z,
                                    pose.halfW, pose.halfH,
                                    pose.r, pose.g, pose.b, pose.a);
            billboardIds.push_back(bid);
        }
    }

    while (doodadCursor < data->doodadAnims.size()) {
        const auto& req = data->doodadAnims[doodadCursor++];
        scene_.PlayDoodadAnimation(req.x, req.y, req.radius, req.doodadId,
                                   req.animName, req.nearestOnly);
    }

    for (const auto& [id, track] : data->trackables) {
        if (!track || trackableIds.count(id) || track->modelPath.empty()) continue;
        float height = texturedTerrain_.IsValid()
            ? texturedTerrain_.GetGroundHeight(track->x, track->y) : 0.0f;
        float pos[3] = {track->x, height, track->y};
        if (scene_.AddTransientModelAtScenePosition(track->modelPath, pos, track->facing, 1.0f) >= 0) {
            trackableIds.insert(id);
        }
    }

    minimap_.ClearScriptMarkers();
    for (const auto& [simId, indicator] : data->unitIndicators) {
        CUnit* unit = simulation_.GetUnit(simId);
        if (!unit || unit->IsDead()) continue;
        const uint32_t rgba = (uint32_t(indicator.alpha) << 24)
            | (uint32_t(indicator.red) << 16)
            | (uint32_t(indicator.green) << 8)
            | uint32_t(indicator.blue);
        minimap_.AddScriptMarker(unit->GetX(), unit->GetY(), rgba, 8.0f);
    }

    struct PlayedDestructableAnim {
        int generation = -1;
        std::string name;
    };
    static std::unordered_map<int, PlayedDestructableAnim> playedAnims;
    for (auto* destructable : simulation_.GetDestructables()) {
        if (!destructable || destructable->IsHidden()) continue;
        const float x = destructable->GetX();
        const float y = destructable->GetY();
        scene_.SetPlaybackSpeedNear(x, y, 96.0f, destructable->GetAnimationSpeed());
        PlayedDestructableAnim& slot = playedAnims[destructable->GetHandleId()];
        const std::string& anim = destructable->GetAnimation();
        if (slot.generation != destructable->GetAnimGeneration() || slot.name != anim) {
            const bool loop = destructable->GetQueuedAnimation().empty();
            if (!anim.empty() &&
                scene_.PlayAnimationNear(x, y, 96.0f, anim, loop, false) > 0) {
                slot.generation = destructable->GetAnimGeneration();
                slot.name = anim;
            }
        } else if (!destructable->GetQueuedAnimation().empty()) {
            const std::string next = destructable->GetQueuedAnimation();
            if (scene_.PlayAnimationNear(x, y, 96.0f, next, true, true) > 0) {
                destructable->PromoteQueuedAnimation();
                slot.name = destructable->GetAnimation();
            }
        }
    }
}

void GameController::SyncWeather(float dt) {
    auto* lightning = scene_.GetLightning();
    if (!lightning) return;
    static std::vector<int> boltIds;
    for (int id : boltIds) lightning->RemoveBolt(id);
    boltIds.clear();
    if (!jassNatives_ || !jassNatives_->GetData()) return;
    struct Drop { float x = 0, y = 0, z = 0; bool live = false; };
    static std::vector<Drop> drops;
    int slot = 0;
    constexpr int kBase = 0x71000000;
    for (const auto& entry : jassNatives_->GetData()->weathers) {
        const auto& weather = entry.second;
        if (!weather || !weather->enabled) continue;
        const float width = std::max(32.0f, weather->maxX - weather->minX);
        const float height = std::max(32.0f, weather->maxY - weather->minY);
        const int count = std::min(40, std::max(8, (int)((width * height) / 50000.0f)));
        if ((int)drops.size() < slot + count) drops.resize(slot + count);
        const std::string code = weather->code.ToString();
        const bool snow = code.find('S') != std::string::npos || code.find('s') != std::string::npos;
        const float speed = snow ? 70.0f : 420.0f;
        const float length = snow ? 6.0f : 36.0f;
        for (int i = 0; i < count; i++) {
            Drop& drop = drops[slot + i];
            if (!drop.live) {
                const float u = std::fmod(i * 0.6180339f, 1.0f);
                drop.x = weather->minX + u * width;
                drop.y = weather->minY + std::fmod(u * 1.7f, 1.0f) * height;
                drop.z = 40.0f + std::fmod(u * 240.0f, 240.0f);
                drop.live = true;
            }
            drop.z -= speed * dt;
            if (drop.z < 0.0f) drop.z = 260.0f;
            if (drop.x < weather->minX || drop.x > weather->maxX ||
                drop.y < weather->minY || drop.y > weather->maxY) {
                drop.live = false;
                continue;
            }
            const int id = kBase + slot + i;
            lightning->SetBolt(id, drop.x, drop.z, drop.y,
                               drop.x, std::max(0.0f, drop.z - length), drop.y,
                               snow ? 0.9f : 0.65f, snow ? 0.9f : 0.75f, 0.95f, 0.45f, "CLPB");
            boltIds.push_back(id);
        }
        slot += count;
    }
}

void GameController::SyncJassEffectVisuals() {
    if (!jassNatives_ || !jassNatives_->GetData()) return;
    auto data = jassNatives_->GetData();

    auto groundHeight = [&](float x, float y) {
        return texturedTerrain_.IsValid() ? texturedTerrain_.GetGroundHeight(x, y) : 0.0f;
    };

    for (auto& [effectId, e] : data->effects) {
        if (!e || e->modelPath.empty()) continue;
        auto known = effectVisuals_.find(effectId);

        if (e->targetSimHandle < 0) {
            // Point effect: one-shot transient on first sight.
            if (known == effectVisuals_.end()) {
                float pos[3] = {e->x, groundHeight(e->x, e->y), e->y};
                scene_.AddTransientModelAtScenePosition(e->modelPath, pos, 0.0f, 1.0f);
                effectVisuals_[effectId] = -1;
            }
            continue;
        }

        // Attached effect: persistent instance, repositioned every frame.
        int idx = (known == effectVisuals_.end()) ? -1 : known->second;
        if (idx < 0) {
            int player = 0;
            if (CWidget* w = simulation_.GetWidget(e->targetSimHandle)) {
                if (auto* u = dynamic_cast<CUnit*>(w)) player = u->GetPlayerIndex();
            }
            idx = scene_.AddAttachedModel(e->modelPath, player);
            if (idx < 0) continue; // model missing; retry next frame
            effectVisuals_[effectId] = idx;
        }

        // Compose unit-instance transform * attach-node matrix (mirrors the
        // SceneManager instance convention: Y-rotation + translation).
        Mat4 finalMat = Mat4::Identity();
        bool placed = false;
        if (CWidget* w = simulation_.GetWidget(e->targetSimHandle)) {
            float wx = w->GetX(), wy = w->GetY();
            float h = groundHeight(wx, wy);
            float angle = 0.0f;
            MdxViewer* viewer = nullptr;
            int viewerIdx = -1;
            if (auto* u = dynamic_cast<CUnit*>(w)) {
                angle = u->GetFacing();
                h = unit_selection_geom::ComputeUnitVisualHeight(
                    *u, texturedTerrain_.IsValid(),
                    [&](float x, float z) { return texturedTerrain_.GetGroundHeight(x, z); });
                auto vit = simUnitToVisualIdx.find(u->GetHandleId());
                if (vit != simUnitToVisualIdx.end()) {
                    viewerIdx = vit->second;
                    viewer = scene_.GetViewer(viewerIdx);
                }
            } else {
                auto vit = simItemToVisualIdx.find(w->GetHandleId());
                if (vit != simItemToVisualIdx.end()) {
                    viewerIdx = vit->second;
                    viewer = scene_.GetViewer(viewerIdx);
                }
            }
            // Instance transform shares the scene convention, including
            // the instance scale (seed x script), so attached models grow
            // with scaled units.
            float iscale[3] = {1.0f, 1.0f, 1.0f};
            if (viewerIdx >= 0) scene_.GetInstanceScale(viewerIdx, iscale);
            Mat4 inst = MakeSceneInstanceMatrix(wx, h, wy, angle,
                                                iscale[0], iscale[1], iscale[2]);
            finalMat = inst;
            if (viewer && viewer->GetAnimator() && !e->attachPoint.empty()) {
                // Pivot-inclusive attach location in model space plus the
                // node orientation (translation stripped, so the unit
                // instance transform is applied exactly once).
                Vec3 loc{0, 0, 0};
                Mat4 node = Mat4::Identity();
                if (viewer->GetAnimator()->GetAttachmentWorldMatrixYUp(e->attachPoint, node, loc)) {
                    node.m[12] = 0.0f;
                    node.m[13] = 0.0f;
                    node.m[14] = 0.0f;
                    finalMat = inst * Mat4::Translate(loc.x, loc.y, loc.z) * node;
                }
            }
            placed = true;
        }
        if (placed) {
            if (!scene_.UpdateAttachedModel(idx, finalMat)) {
                effectVisuals_.erase(effectId);
            }
        }
    }

    // Drop visuals whose effects were destroyed (or never resolved).
    std::vector<int> gone;
    for (auto& [effectId, idx] : effectVisuals_) {
        if (data->effects.find(effectId) == data->effects.end()) {
            if (idx >= 0) scene_.RemoveAttachedModel(idx);
            gone.push_back(effectId);
        }
    }
    for (int id : gone) effectVisuals_.erase(id);
}

static bool IsAllyForFog(const CSimulation& sim, int local, int owner) {
    if (owner == local) return true;
    if (local < 0 || owner < 0) return false;
    const CPlayer* self = sim.GetPlayer(local);
    const CPlayer* other = sim.GetPlayer(owner);
    return self && other && self->HasAlliance(*other, CAllianceType::ALLIED);
}

bool GameController::IsUnitVisibleInFog(const CUnit* unit) const {
    if (!unit) return false;
    // Buildings/structures persist on the map once scouted (grey fog shows
    // bases); only the black mask hides them.
    const int local = config_.localPlayerIndex >= 0 ? config_.localPlayerIndex : 0;
    const CFogSystem* fog = simulation_.GetFogSystem();
    if (unit->GetUnitType() && unit->GetUnitType()->IsBuilding()) {
        return !fog->IsMaskedTo(local, unit->GetX(), unit->GetY());
    }
    // Own and allied forces are always shown (ref alliance vision).
    if (IsAllyForFog(simulation_, local, unit->GetPlayerIndex())) return true;
    // Enemies (and neutrals/creeps) need live sight ...
    if (!fog->IsVisibleTo(local, unit->GetX(), unit->GetY())) return false;
    // ... and stealth needs a local detector (or Faerie Fire) on top.
    if (!unit->IsStealthed()) return true;
    for (auto* viewer : simulation_.GetUnits()) {
        if (!viewer || viewer->IsDead()) continue;
        const int vp = viewer->GetPlayerIndex();
        if (vp != local && !IsAllyForFog(simulation_, local, vp)) continue;
        if (!unit->IsInvisibleTo(viewer)) return true;
    }
    return false;
}

void GameController::SyncFogStaticsVisibility() {
    if (!simulationInitialized_) return;
    const uint64_t version = simulation_.GetFogSystem()->Version();
    if (version == fogStaticsVersionSynced_) return;
    fogStaticsVersionSynced_ = version;
    const int local = config_.localPlayerIndex >= 0 ? config_.localPlayerIndex : 0;
    const CFogSystem* fog = simulation_.GetFogSystem();
    const int count = scene_.GetInstanceCount();
    for (int i = 0; i < count; ++i) {
        const PlacedModel* inst = scene_.GetInstance(i);
        if (!inst || inst->transient) continue;
        // Units and ground items are driven by sim state, not by position.
        if (inst->debugKind == "unit" || inst->debugKind == "item") continue;
        // Spell visuals punch through fog like in retail.
        if (inst->debugKind == "event-spawn" || inst->debugKind == "attached-effect") continue;
        const bool explored = !fog->IsMaskedTo(local, inst->pos[0], inst->pos[2]);
        scene_.SetInstanceVisible(i, explored);
    }
}

void GameController::SyncAndReconcileVisuals() {
    // Sync all sim units to visuals
    for (auto* unit : simulation_.GetUnits()) {
        if (!unit || unit->IsDead()) continue;
        int handleId = unit->GetHandleId();

        auto visualIt = simUnitToVisualIdx.find(handleId);
        if (visualIt == simUnitToVisualIdx.end()) {
            EnsureSimulationUnitVisual(unit);
            visualIt = simUnitToVisualIdx.find(handleId);
        }
        if (visualIt != simUnitToVisualIdx.end()) {
            if (auto* viewer = scene_.GetViewer(visualIt->second)) {
                if (jassNatives_ && jassNatives_->GetData()) {
                    auto blend = jassNatives_->GetData()->unitBlendTimes.find(handleId);
                    if (blend != jassNatives_->GetData()->unitBlendTimes.end()) {
                        viewer->SetBlendTime(blend->second);
                    }
                }
                SyncSimulationUnitAnimation(unit, viewer, this);
            }
            scene_.SetInstanceVisible(visualIt->second,
                !unit->IsHidden() && !unit->IsLoadedAboard() && IsUnitVisibleInFog(unit));
        }
    }

    for (auto* item : simulation_.GetItems()) {
        if (!item || item->IsDead()) continue;
        const int handleId = item->GetHandleId();
        auto visualIt = simItemToVisualIdx.find(handleId);
        if (visualIt == simItemToVisualIdx.end()) {
            EnsureSimulationItemVisual(item);
            visualIt = simItemToVisualIdx.find(handleId);
        }
        if (visualIt != simItemToVisualIdx.end()) {
            float height = texturedTerrain_.IsValid()
                ? texturedTerrain_.GetGroundHeight(item->GetX(), item->GetY())
                : 0.0f;
            float pos[3] = {item->GetX(), height, item->GetY()};
            scene_.UpdateInstancePosition(visualIt->second, pos, 0.0f);
            bool explored = true;
            if (simulationInitialized_) {
                const int local = config_.localPlayerIndex >= 0 ? config_.localPlayerIndex : 0;
                explored = !simulation_.GetFogSystem()->IsMaskedTo(local, item->GetX(), item->GetY());
            }
            scene_.SetInstanceVisible(visualIt->second, !item->IsHidden() && explored);
        }
    }

    // Reconcile (remove dead, add new)
    std::vector<int> removedHandles;
    for (auto& [handleId, visualIdx] : simUnitToVisualIdx) {
        CUnit* unit = simulation_.GetUnit(handleId);
        if (!unit || unit->IsDead()) {
            scene_.SetInstanceVisible(visualIdx, false);
            removedHandles.push_back(handleId);
        }
    }
    for (int handleId : removedHandles) {
        simUnitToVisualIdx.erase(handleId);
        simUnitVisualRegistrationFailed.erase(handleId);
        simUnitVisualMovingStates.erase(handleId);
        simUnitVisualLastPositions.erase(handleId);
        simUnitPendingAnimations.erase(handleId);
        simUnitQueuedAnimations.erase(handleId);
        simUnitVisualPropertyTags.erase(handleId);
        simUnitAnimationPropertyTags.erase(handleId);
        selectedUnitHandlesSet_.erase(handleId);
        selectedUnitHandles_.erase(
            std::remove(selectedUnitHandles_.begin(), selectedUnitHandles_.end(), handleId),
            selectedUnitHandles_.end());
        if (selectedUnitHandle_ == handleId) selectedUnitHandle_ = -1;
    }

    std::vector<int> removedItemHandles;
    for (auto& [handleId, visualIdx] : simItemToVisualIdx) {
        CItem* item = simulation_.GetItem(handleId);
        if (!item || item->IsDead()) {
            scene_.SetInstanceVisible(visualIdx, false);
            removedItemHandles.push_back(handleId);
        }
    }
    for (int handleId : removedItemHandles) {
        simItemToVisualIdx.erase(handleId);
        simItemVisualRegistrationFailed.erase(handleId);
    }

    SyncFogStaticsVisibility();
    SyncJassEffectVisuals();
}

void GameController::CollectUnitOverlays(int screenW, int screenH) {
    static const uint32_t kPlayerColors[12] = {
        0xFFFF0000, 0xFF0042FF, 0xFF1CE6B6, 0xFF540081,
        0xFFFFFC00, 0xFFFE8A0E, 0xFF20C000, 0xFFE55BB0,
        0xFF959697, 0xFF7EBFF1, 0xFF106246, 0xFF4F2B05
    };

    float aspect = (screenH > 0) ? (float)screenW / screenH : 1.0f;
    Mat4 view = camera_.GetViewMatrix();
    Mat4 proj = camera_.GetProjectionMatrix(aspect);

    std::vector<UnitOverlayInfo>& overlays = overlayCache_;
    overlays.clear();
    if (simulationInitialized_) {
        for (auto* unit : simulation_.GetUnits()) {
            if (unit->IsDead()) continue;
            // Bars/rings follow the same fog rule as the model itself.
            if (unit->IsHidden() || unit->IsLoadedAboard() || !IsUnitVisibleInFog(unit)) continue;

            float gndH = texturedTerrain_.IsValid() ? texturedTerrain_.GetGroundHeight(unit->GetX(), unit->GetY()) : 0.0f;

            UnitOverlayInfo info;
            info.handleId = unit->GetHandleId();
            info.isSelected = IsUnitSelected(unit->GetHandleId());
            info.isAlive = !unit->IsDead();
            info.hpRatio = unit->GetMaxLife() > 0 ? unit->GetLife() / unit->GetMaxLife() : 1.0f;
            info.mpRatio = unit->GetMaxMana() > 0 ? unit->GetMana() / (float)unit->GetMaxMana() : 0.0f;

            Vec3 headPos(unit->GetX(), gndH + 140.0f, unit->GetY());
            picking::WorldToScreen(headPos, view, proj, screenW, screenH, info.screenX, info.screenY);

            Vec3 footPos(unit->GetX(), gndH + 10.0f, unit->GetY());
            picking::WorldToScreen(footPos, view, proj, screenW, screenH, info.footX, info.footY);

            auto* unitType = unitData_.GetUnitType(unit->GetTypeId());
            info.name = unitType ? unitType->GetName() : "?";

            int pi = unit->GetPlayerIndex();
            if (pi >= 0 && pi < 12) info.playerColor = kPlayerColors[pi];
            else info.playerColor = 0xFF888888;

            overlays.push_back(info);
        }
    }
    ui_.SetOverlayData(overlays);
}

void GameController::CollectTextTagOverlays(int screenW, int screenH) {
    std::vector<TextTagOverlayInfo>& tags = textTagCache_;
    tags.clear();
    if (!jassNatives_ || !jassNatives_->GetData()) {
        ui_.SetTextTagData(tags);
        return;
    }
    float aspect = (screenH > 0) ? (float)screenW / screenH : 1.0f;
    Mat4 view = camera_.GetViewMatrix();
    Mat4 proj = camera_.GetProjectionMatrix(aspect);
    for (const auto& [id, tag] : jassNatives_->GetData()->textTags) {
        (void)id;
        if (!tag || !tag->visible || tag->text.empty()) continue;
        const float gndH = texturedTerrain_.IsValid()
            ? texturedTerrain_.GetGroundHeight(tag->x, tag->y) : 0.0f;
        Vec3 worldPos(tag->x, gndH + tag->z + 10.0f, tag->y);
        float sx = 0, sy = 0;
        if (!picking::WorldToScreen(worldPos, view, proj, screenW, screenH, sx, sy)) continue;
        // Fade out past the fadepoint (ref TextTag.update alpha ramp).
        uint32_t color = (uint32_t)tag->color;
        if (!tag->permanent && tag->lifespan > 0 && tag->fadepoint >= 0) {
            const float remaining = tag->lifespan - tag->age;
            const float fadeSpan = tag->lifespan - tag->fadepoint;
            if (fadeSpan > 0 && remaining < fadeSpan) {
                const float t = std::clamp(remaining / fadeSpan, 0.0f, 1.0f);
                const uint32_t a = (uint32_t)(((color >> 24) & 0xFF) * t);
                color = (color & 0x00FFFFFF) | (a << 24);
            }
        }
        TextTagOverlayInfo info;
        info.screenX = sx;
        info.screenY = sy;
        info.text = tag->text;
        info.color = color;
        info.fontSize = tag->fontSize > 0 ? tag->fontSize : 12.0f;
        info.centered = tag->centered;
        tags.push_back(std::move(info));
    }
    ui_.SetTextTagData(tags);
}

// --- Render ---

void GameController::Render(int screenW, int screenH) {
    if (!initialized_) return;
    if (uiOnlyMode_) {
        RenderUiOnly(screenW, screenH);
        return;
    }

    if (debugHudGameMode_) {
        ui_.Update(screenW, screenH);

        const float hudDebugClearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        Renderer::SetClearColor(hudDebugClearColor);
        Renderer::BeginFrame();
        sg_reset_state_cache();
        sg_apply_viewport(0, 0, screenW, screenH, true);
        sg_apply_scissor_rect(0, 0, screenW, screenH, true);
        ui_.RenderHudOnly();
        if (screenshotFrame >= 0) {
            screenshotFrame = std::min(screenshotFrame, frameCount_);
            CaptureScreenshotIfNeeded(screenW, screenH);
        }
        Renderer::EndFrame();
        Platform::ProcessInput();
        if (screenshotFrame < 0 && exitAfterFrame_ >= 0) {
            sapp_request_quit();
            exitAfterFrame_ = -1;
            return;
        }
        MaybeExitAfterFrame();
        return;
    }

    if (state_ != GameState::IN_GAME) {
        if (screenshotFrame >= 0 || exitAfterFrame_ >= 0) {
            ui_.Update(screenW, screenH);

            Renderer::BeginFrame();
            sg_reset_state_cache();
            sg_apply_viewport(0, 0, screenW, screenH, true);
            sg_apply_scissor_rect(0, 0, screenW, screenH, true);
            ui_.RenderHudOnly();
            CaptureScreenshotIfNeeded(screenW, screenH);
            Renderer::EndFrame();
            MaybeExitAfterFrame();
        }
        return;
    }

    float dt = 0.016f;
    float aspect = (screenH > 0) ? (float)screenW / screenH : 1.0f;
    Mat4 view = camera_.GetViewMatrix();
    Mat4 proj = camera_.GetProjectionMatrix(aspect);

    if (!hideHud_) {
        ui_.Update(screenW, screenH);
    }

    Renderer::BeginFrame();
    Renderer::SetWireframe(wireframe);

    if (texturedTerrain_.IsValid()) {
        texturedTerrain_.SetWaterDeforms(simulationInitialized_ && simulation_.GetWaterDeforms());
        texturedTerrain_.Render(view, proj, frameCount_ * 0.016f);
    } else if (simpleTerrain_.IsValid())
        simpleTerrain_.Render(view, proj);

    sg_reset_state_cache();
    sg_apply_viewport(0, 0, screenW, screenH, true);
    sg_apply_scissor_rect(0, 0, screenW, screenH, true);
    if (!terrainOnly_) {
            scene_.SetDebugRenderRange(debugSceneRenderStart_, debugSceneRenderCount_);
            scene_.SetDebugRenderKind(debugSceneRenderKind_);
            SyncWeather(dt);
            scene_.RenderAll(view, proj, dt);
            for (const SceneEventEffect& eventEffect : scene_.GetFrameEventEffects()) {
                if (audio_.IsValid() && !eventEffect.effect.soundPaths.empty()) {
                    audio_.Play3DPath(eventEffect.effect.soundPaths.front(),
                                      eventEffect.x, eventEffect.y, eventEffect.z,
                                      eventEffect.effect.volume);
                }
                if (!eventEffect.effect.texturePath.empty()) {
                    ActiveEventSplat splat;
                    splat.key = "mdx_event_" + std::to_string(nextEventSplatId_++);
                    splat.texturePath = eventEffect.effect.texturePath;
                    splat.x = eventEffect.x;
                    splat.y = eventEffect.z;
                    const float scale = eventEffect.effect.scale > 0.0f ? eventEffect.effect.scale : 1.0f;
                    splat.halfSize = std::max(16.0f, scale * 64.0f);
                    if (eventEffect.hasWorldTransform) {
                        const Vec3 splatScaleCompensation = MdxEventSplatScaleCompensationForType(
                            eventEffect.effect.type, eventEffect.effect.localScale);
                        MdxEventSplatMesh mesh = BuildMdxEventSplatMeshFromTransform(
                            eventEffect.worldTransform,
                            splat.halfSize,
                            8.0f,
                            splatScaleCompensation);
                        splat.meshVertices = std::move(mesh.vertices);
                        splat.meshIndices = std::move(mesh.indices);
                    }
                    splat.lifeRemaining = eventEffect.effect.lifeSpan > 0.0f
                        ? eventEffect.effect.lifeSpan
                        : 0.5f;
                    splat.lifeSpan = splat.lifeRemaining;
                    splat.colors = eventEffect.effect.colors;
                    splat.intervalTimes = eventEffect.effect.intervalTimes;
                    splat.uvIntervals = eventEffect.effect.uvIntervals;
                    splat.intervalCount = eventEffect.effect.intervalCount;
                    splat.columns = eventEffect.effect.columns;
                    splat.rows = eventEffect.effect.rows;
                    splat.blendMode = eventEffect.effect.blendMode;
                    splat.unshaded = eventEffect.effect.unshaded;
                    splat.highPriority = true;
                    activeEventSplats_.push_back(std::move(splat));
                }
                if (!eventEffect.effect.modelPath.empty()) {
                    const float spawnPos[3] = {eventEffect.x, eventEffect.y, eventEffect.z};
                    float spawnScale = eventEffect.effect.scale > 0.0f
                        ? eventEffect.effect.scale
                        : 1.0f;
                    if (!eventEffect.hasWorldTransform && eventEffect.effect.hasLocalTransform) {
                        const Vec3& localScale = eventEffect.effect.localScale;
                        const float nodeScale = (std::fabs(localScale.x)
                            + std::fabs(localScale.y)
                            + std::fabs(localScale.z)) / 3.0f;
                        if (std::isfinite(nodeScale) && nodeScale > 0.0f) {
                            spawnScale *= nodeScale;
                        }
                    }
                    const float fallbackLife = eventEffect.effect.lifeSpan > 0.0f
                        ? eventEffect.effect.lifeSpan
                        : 1.0f;
                    if (eventEffect.hasWorldTransform) {
                        Mat4 spawnTransform = eventEffect.worldTransform;
                        if (spawnScale > 0.0f && std::isfinite(spawnScale)
                            && std::fabs(spawnScale - 1.0f) > 0.0001f) {
                            spawnTransform = spawnTransform
                                * Mat4::Scale(spawnScale, spawnScale, spawnScale);
                        }
                        scene_.AddTransientModelWithSceneMatrix(
                            eventEffect.effect.modelPath, spawnTransform, 0, fallbackLife);
                    } else {
                        scene_.AddTransientModelAtScenePosition(
                            eventEffect.effect.modelPath, spawnPos, 0.0f, spawnScale, 0, fallbackLife);
                    }
                }
            }
    }

    ScreenRect minimapRect = hideHud_ ? ScreenRect{} : ui_.GetMinimapRect();
    if (!hideHud_ && !minimapRect.IsValid()) {
        minimapRect = FallbackMinimapRect(screenW, screenH);
    }
    if (!hideHud_ && minimapRect.IsValid()) {
        minimap_.SetScreenRect(minimapRect.x, minimapRect.y, minimapRect.w, minimapRect.h);
    }

    if (!hideHud_) SyncHudLiveData();

    if (!hideHud_ && showMinimap && minimap_.IsValid()) {
        SyncMinimapLayer();
        sg_reset_state_cache();
        sg_apply_viewport(0, 0, screenW, screenH, true);
        sg_apply_scissor_rect(0, 0, screenW, screenH, true);
        Vec3 cp = camera_.GetPosition();
        minimap_.Render(screenW, screenH, cp.x, cp.z, camera_.GetYaw(), 0, 0, 45.0f, aspect);
        sg_reset_state_cache();
    }

    if (!hideHud_) {
        CollectUnitOverlays(screenW, screenH);
        CollectTextTagOverlays(screenW, screenH);
        sg_reset_state_cache();
        sg_apply_viewport(0, 0, screenW, screenH, true);
        sg_apply_scissor_rect(0, 0, screenW, screenH, true);
        ui_.Render();

        ui_.RenderHudOnly();
    }
    CaptureScreenshotIfNeeded(screenW, screenH);
    Renderer::EndFrame();
    Platform::ProcessInput();
    MaybeExitAfterFrame();
}

void GameController::RenderUiOnly(int screenW, int screenH) {
    if (state_ != GameState::IN_GAME) return;

    ui_.Update(screenW, screenH);

    const float glueClearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    Renderer::SetClearColor(glueClearColor);
    Renderer::BeginFrame();
    sg_reset_state_cache();
    sg_apply_viewport(0, 0, screenW, screenH, true);
    sg_apply_scissor_rect(0, 0, screenW, screenH, true);

    sg_reset_state_cache();
    sg_apply_viewport(0, 0, screenW, screenH, true);
    sg_apply_scissor_rect(0, 0, screenW, screenH, true);

    ui_.RenderHudOnly();

    CaptureScreenshotIfNeeded(screenW, screenH);
    Renderer::EndFrame();
    Platform::ProcessInput();
    MaybeExitAfterFrame();
}

bool GameController::LoadUiOnlyGlueBackgroundModel() {
    if (glueBackgroundLoaded_ || mpq_.ArchiveCount() == 0) {
        return glueBackgroundLoaded_;
    }

    std::vector<std::string> candidates;
    const std::string skinModel = ResolveUiOnlySkinField(mpq_, "GlueSpriteLayerBackground", config_.gameVersion);
    AddUiOnlyCandidatePath(candidates, skinModel);
    AddUiOnlyCandidatePath(candidates, UiOnlyMdxPathFromModelField(skinModel));
    AddUiOnlyCandidatePath(candidates, "UI\\Glues\\MainMenu\\MainMenu3D_exp\\MainMenu3D_exp.mdx");
    AddUiOnlyCandidatePath(candidates, "UI\\Glues\\MainMenu\\MainMenu3D\\MainMenu3D.mdx");

    for (const std::string& path : candidates) {
        if (path.empty()) {
            continue;
        }

        std::vector<uint8_t> data;
        if (!mpq_.ReadFile(path, data)) {
            continue;
        }

        MdxModel model;
        if (!MdxLoad(data.data(), data.size(), model)) {
            continue;
        }

        if (!glueBackgroundViewer_.Load(model, &mpq_)) {
            continue;
        }

        glueBackgroundModel_ = std::move(model);
        glueBackgroundLoaded_ = true;
        glueBackgroundPath_ = path;
        glueBackgroundViewer_.PlayStandSequence();
        glueBackgroundCameraSequenceIndex_ = UiOnlySelectStandSequenceIndex(glueBackgroundModel_);
        glueBackgroundCameraElapsedMs_ = 0.0f;
        std::cout << "UI-only mode: loaded glue background model " << glueBackgroundPath_
                  << " cameras=" << glueBackgroundModel_.cameras.size()
                  << " sequences=" << glueBackgroundModel_.sequences.size() << std::endl;
        return true;
    }

    std::cerr << "UI-only mode: failed to load glue background model" << std::endl;
    return false;
}

bool GameController::LoadUiOnlyGlueSpriteLayers() {
    if (glueSpriteLayersLoaded_ || mpq_.ArchiveCount() == 0) {
        return glueSpriteLayersLoaded_;
    }

    auto loadLayer = [&](GlueSpriteLayer& layer,
                         const char* skinField,
                         const char* fallbackMdxPath,
                         const Vec3& position,
                         const Vec3& scale,
                         bool billboardPlane,
                         const char* birthSequence,
                         const char* standSequence) -> bool {
        std::vector<std::string> candidates;
        const std::string skinModel = ResolveUiOnlySkinField(mpq_, skinField, config_.gameVersion);
        AddUiOnlyCandidatePath(candidates, skinModel);
        AddUiOnlyCandidatePath(candidates, UiOnlyMdxPathFromModelField(skinModel));
        AddUiOnlyCandidatePath(candidates, fallbackMdxPath);

        for (const std::string& path : candidates) {
            if (path.empty()) {
                continue;
            }

            std::vector<uint8_t> data;
            if (!mpq_.ReadFile(path, data)) {
                continue;
            }

            MdxModel model;
            if (!MdxLoad(data.data(), data.size(), model)) {
                continue;
            }

            if (!layer.viewer.Load(model, &mpq_)) {
                continue;
            }

            layer.model = std::move(model);
            layer.loaded = true;
            layer.position = position;
            layer.scale = scale;
            layer.billboardPlane = billboardPlane;
            layer.birthSequence = birthSequence ? birthSequence : "";
            layer.standSequence = standSequence ? standSequence : "";
            layer.waitingForStand = !layer.birthSequence.empty()
                && layer.viewer.PlaySequenceByName(layer.birthSequence.c_str(), false, 1.0f, false);
            if (!layer.waitingForStand) {
                if (!layer.standSequence.empty()) {
                    const bool playedStand = layer.viewer.PlaySequenceByName(layer.standSequence.c_str(), true);
                    if (playedStand) {
                        layer.viewer.SetCurrentSequenceFrameByRatio(0.5f);
                    }
                } else {
                    layer.viewer.PlayStandSequence();
                }
            }
            layer.path = path;
            std::cout << "UI-only mode: loaded glue sprite layer " << skinField
                      << " path=" << layer.path
                      << " sequences=" << layer.model.sequences.size()
                      << " geosets=" << layer.model.geosets.size()
                      << " pos=(" << layer.position.x << "," << layer.position.y << "," << layer.position.z << ")"
                      << " scale=(" << layer.scale.x << "," << layer.scale.y << "," << layer.scale.z << ")" << std::endl;
            return true;
        }

        std::cerr << "UI-only mode: failed to load glue sprite layer " << skinField << std::endl;
        return false;
    };

    const bool loadedLeft = loadLayer(glueSpriteTopLeft_,
        "GlueSpriteLayerTopLeft",
        "UI\\Glues\\SpriteLayers\\Expansion\\TopLeftPanel-Expansion.mdx",
        Vec3(0.0f, 0.0f, 0.0f),
        Vec3(1.0f, 1.0f, 1.0f),
        false,
        "",
        "MainMenu Stand");
    const bool loadedRight = loadLayer(glueSpriteTopRight_,
        "GlueSpriteLayerTopRight",
        "UI\\Glues\\SpriteLayers\\Expansion\\TopRightPanel-Expansion.mdx",
        Vec3(0.0f, 0.0f, 0.0f),
        Vec3(1.0f, 1.0f, 1.0f),
        false,
        "",
        "MainMenu Stand");
    const bool loadedLogo = loadLayer(glueMainMenuLogo_,
        "MainMenuLogo",
        "UI\\Glues\\MainMenu\\WarCraftIIILogo_exp\\WarCraftIIILogo_exp.mdx",
        Vec3(0.0f, 0.49f, -0.586f),
        Vec3(1.0f, 1.0f, 1.0f),
        true,
        "",
        "Stand");
    glueSpriteLayersLoaded_ = loadedLeft || loadedRight || loadedLogo;
    return glueSpriteLayersLoaded_;
}

void GameController::RenderUiOnlyGlueBackground(int screenW, int screenH) {
    if (!glueBackgroundLoaded_ || screenW <= 0 || screenH <= 0) {
        return;
    }

    Mat4 view;
    Mat4 proj;
    if (!glueBackgroundModel_.cameras.empty()) {
        const MdxCamera& camera = glueBackgroundModel_.cameras[0];
        glueBackgroundCameraElapsedMs_ += 1000.0f / 60.0f;
        uint32_t cameraFrame = 0;
        if (glueBackgroundCameraSequenceIndex_ >= 0
            && glueBackgroundCameraSequenceIndex_ < (int)glueBackgroundModel_.sequences.size()) {
            const MdxSequence& sequence = glueBackgroundModel_.sequences[(size_t)glueBackgroundCameraSequenceIndex_];
            const float durationMs = MdxSequenceDurationSeconds(glueBackgroundModel_, glueBackgroundCameraSequenceIndex_) * 1000.0f;
            if (durationMs > 0.0f) {
                glueBackgroundCameraElapsedMs_ = std::fmod(glueBackgroundCameraElapsedMs_, durationMs);
            }
            const uint32_t range = sequence.endFrame > sequence.startFrame
                ? sequence.endFrame - sequence.startFrame
                : 0;
            const uint32_t offset = range > 0
                ? std::min(range, (uint32_t)glueBackgroundCameraElapsedMs_)
                : 0;
            cameraFrame = sequence.startFrame + offset;
        }

        MdxCameraSample sampledCamera;
        MdxSampleCamera(camera, &glueBackgroundModel_, cameraFrame,
                        glueBackgroundCameraElapsedMs_, sampledCamera,
                        glueBackgroundCameraSequenceIndex_);
        view = LookAt(UiOnlyMdxPointToYUp(sampledCamera.pos),
                      UiOnlyMdxPointToYUp(sampledCamera.target),
                      Vec3(0.0f, 1.0f, 0.0f));
        const float fovDeg = sampledCamera.fov * 0.6f * 57.2957795f;
        const float nearClip = std::max(1.0f, sampledCamera.nearClip);
        const float farClip = std::max(nearClip + 1.0f, sampledCamera.farClip);
        proj = Perspective(fovDeg, (float)screenW / (float)screenH, nearClip, farClip);
    } else {
        view = LookAt(Vec3(0.0f, 900.0f, 1800.0f), Vec3(0.0f, 400.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
        proj = Perspective(60.0f, (float)screenW / (float)screenH, 8.0f, 70000.0f);
    }

    glueBackgroundViewer_.Update(1.0f / 60.0f, view);
    glueBackgroundViewer_.RenderAt(Vec3(0.0f, 0.0f, 0.0f), view, proj, 1.0f, 0, 0.0f, 1.0f, true);
}

void GameController::RenderUiOnlyGlueSpriteLayers(int screenW, int screenH) {
    if (!glueSpriteLayersLoaded_ || screenW <= 0 || screenH <= 0) {
        return;
    }

    Mat4 proj;
    const float aspect = (float)screenW / (float)screenH;
    const float uiWidth = aspect > (4.0f / 3.0f) ? 0.8f * aspect / (4.0f / 3.0f) : 0.8f;
    const float uiHeight = aspect < (4.0f / 3.0f) ? 0.6f * (4.0f / 3.0f) / aspect : 0.6f;
    const float uiX = (0.8f - uiWidth) * 0.5f;
    const float uiY = (0.6f - uiHeight) * 0.5f;
    proj.SetOrtho(uiX, uiX + uiWidth, uiY, uiY + uiHeight, -1024.0f, 1024.0f);
    Mat4 view = Mat4::Identity();

    auto renderLayer = [&](GlueSpriteLayer& layer) {
        if (!layer.loaded) {
            return;
        }
        layer.viewer.Update(1.0f / 60.0f, view);
        if (layer.waitingForStand && layer.viewer.IsCurrentSequenceFinished()) {
            layer.waitingForStand = false;
            if (!layer.standSequence.empty() && !layer.viewer.PlaySequenceByName(layer.standSequence.c_str(), true)) {
                layer.viewer.PlayStandSequence();
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
        } else {
            model.m[0] = 0.8f / 497.896f;
            model.m[5] = 0.0f;
            model.m[6] = 1.0f;
            model.m[9] = -1.0f;
        }
        layer.viewer.RenderWithModelMatrix(model, view, proj, 0, 1.0f, false);
    };

    renderLayer(glueSpriteTopLeft_);
    renderLayer(glueSpriteTopRight_);
    renderLayer(glueMainMenuLogo_);
}

void GameController::CaptureScreenshotIfNeeded(int screenW, int screenH) {
    if (screenshotFrame < 0 || frameCount_ < screenshotFrame) return;

    if (screenshotPath.empty()) {
        std::cerr << "Screenshot requested without a path" << std::endl;
    } else if (WriteFramebufferPng(screenshotPath, screenW, screenH)) {
        std::cout << "Screenshot saved: " << screenshotPath << std::endl;
    } else {
        std::cerr << "Screenshot failed: " << screenshotPath << std::endl;
    }
    screenshotFrame = -1;
}

bool GameController::WriteFramebufferPng(const std::string& path, int screenW, int screenH) {
    if (path.empty() || screenW <= 0 || screenH <= 0) return false;

    fs::path screenshotFile(path);
    if (screenshotFile.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(screenshotFile.parent_path(), ec);
        if (ec) {
            std::cerr << "Screenshot: failed to create directory '"
                      << screenshotFile.parent_path().string() << "': " << ec.message() << std::endl;
            return false;
        }
    }

    std::vector<unsigned char> pixels((size_t)screenW * (size_t)screenH * 4u);
    std::vector<unsigned char> flipped(pixels.size());
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, screenW, screenH, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    const size_t stride = (size_t)screenW * 4u;
    for (int y = 0; y < screenH; ++y) {
        const size_t src = (size_t)(screenH - 1 - y) * stride;
        const size_t dst = (size_t)y * stride;
        std::memcpy(flipped.data() + dst, pixels.data() + src, stride);
    }

    return stbi_write_png(screenshotFile.string().c_str(), screenW, screenH, 4, flipped.data(), (int)stride) != 0;
}

void GameController::MaybeExitAfterFrame() {
    if (exitAfterFrame_ >= 0 && frameCount_ >= exitAfterFrame_) {
        if (debugHudGameMode_ && state_ == GameState::IN_GAME && screenshotFrame >= 0) {
            return;
        }
        std::cout << "Exit-after-frame reached: " << frameCount_ << std::endl;
        sapp_request_quit();
        exitAfterFrame_ = -1;
    }
}

// --- OnEvent ---

void GameController::OnEvent(const sapp_event* ev) {
    Platform::HandleEvent(ev);

    switch (ev->type) {
    case SAPP_EVENTTYPE_MOUSE_DOWN: OnMouseDown(ev); break;
    case SAPP_EVENTTYPE_MOUSE_UP:   OnMouseUp(ev);   break;
    case SAPP_EVENTTYPE_MOUSE_MOVE: OnMouseMove(ev); break;
    case SAPP_EVENTTYPE_MOUSE_SCROLL: OnMouseScroll(ev); break;
    case SAPP_EVENTTYPE_CHAR:
    case SAPP_EVENTTYPE_RESIZED:    ui_.HandleEvent(ev); break;
    case SAPP_EVENTTYPE_KEY_DOWN:   OnKeyDown(ev); break;
    default: break;
    }
}

bool GameController::MinimapWorldFromScreen(float screenX, float screenY,
                                             float* worldX, float* worldY) const {
    if (!worldX || !worldY) return false;
    if (hideHud_ || !minimap_.IsValid()) return false;
    if (!minimap_.HitTest(screenX, screenY, sapp_width(), sapp_height())) return false;
    minimap_.ScreenToWorld(screenX, screenY, sapp_width(), sapp_height(), worldX, worldY);
    return true;
}

CUnit* GameController::PickUnitNearWorldPoint(float worldX, float worldY, float radius) const {
    CUnit* best = nullptr;
    float bestDistSq = radius * radius;
    for (auto* unit : simulation_.GetUnits()) {
        if (!unit || unit->IsDead() || unit->IsHidden()) continue;
        const float dx = unit->GetX() - worldX;
        const float dy = unit->GetY() - worldY;
        const float d2 = dx * dx + dy * dy;
        if (d2 < bestDistSq) {
            bestDistSq = d2;
            best = unit;
        }
    }
    return best;
}

void GameController::OnMouseDown(const sapp_event* ev) {    if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
        const int multiSelectHandle = ui_.HitMultiSelection(ev->mouse_x, ev->mouse_y);
        if (multiSelectHandle >= 0 && selectedUnitHandlesSet_.find(multiSelectHandle) != selectedUnitHandlesSet_.end()) {
            PlayUiSound("Click", 0.5f);
            SetSelectedUnits(GetSelectedSimulationUnits(), multiSelectHandle, true);
            std::cout << "MultiSelectIcon: primary handle=" << multiSelectHandle
                      << " count=" << selectedUnitHandles_.size() << std::endl;
            return;
        }
    }

    if (ui_.HandleEvent(ev)) return;

    int btn = (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) ? 0 :
              (ev->mouse_button == SAPP_MOUSEBUTTON_RIGHT) ? 1 : 2;
    if (ui_.GetGameUI().rootFrame && ui_.OnMouseDown(ev->mouse_x, ev->mouse_y, btn))
        return;

    if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
        mouseDown[0] = true;
        selectionPressActive_ = false;
        selectionDragVisible_ = false;
        selectionStartX_ = ev->mouse_x;
        selectionStartY_ = ev->mouse_y;
        selectionCurrentX_ = ev->mouse_x;
        selectionCurrentY_ = ev->mouse_y;
        ui_.ClearSelectionBox();
        if (minimap_.IsValid() && minimap_.HitTest(ev->mouse_x, ev->mouse_y, sapp_width(), sapp_height())) {
            float wx, wy;
            minimap_.ScreenToWorld(ev->mouse_x, ev->mouse_y, sapp_width(), sapp_height(), &wx, &wy);
            float gh = texturedTerrain_.IsValid() ? texturedTerrain_.GetGroundHeight(wx, wy) : 0.0f;
            camera_.SetTarget(wx, gh + 100.0f, wy);
        } else if (ui_.GetPendingCommand() >= 0 && IssuePendingCommandAtScreenPoint(ev->mouse_x, ev->mouse_y)) {
            return;
        } else {
            if (jassNatives_ && simulationInitialized_) {
                int winW = sapp_width(), winH = sapp_height();
                float aspect = (winH > 0) ? (float)winW / winH : 1.0f;
                Mat4 view = camera_.GetViewMatrix();
                Mat4 proj = camera_.GetProjectionMatrix(aspect);
                auto terrainH = [&](float x, float z) {
                    return texturedTerrain_.IsValid() ? texturedTerrain_.GetGroundHeight(x, z) : 0.0f;
                };
                Vec3 wPos = picking::ScreenToWorld(ev->mouse_x, ev->mouse_y, view, proj, winW, winH,
                                                   texturedTerrain_.IsValid(), texturedTerrain_.GetRadius(), terrainH);
                jassNatives_->NotifyTrackableClick(wPos.x, wPos.z);
            }
            if (simulationInitialized_ && !simulation_.GetUnits().empty()) {
                selectionPressActive_ = true;
            }
        }
    } else if (ev->mouse_button == SAPP_MOUSEBUTTON_RIGHT) {
        bool issuedOrder = false;
        if (simulationInitialized_ && !selectedUnitHandles_.empty()) {
            std::vector<CUnit*> selUnits = GetSelectedSimulationUnits();
            if (!selUnits.empty()) {
                // Minimap first: right-click issues SMART to the world point
                // (retail). 3D raycasts from minimap pixels are meaningless,
                // so widget targeting resolves by proximity instead.
                float mmX = 0.0f, mmY = 0.0f;
                if (MinimapWorldFromScreen(ev->mouse_x, ev->mouse_y, &mmX, &mmY)) {
                    CUnit* nearUnit = PickUnitNearWorldPoint(mmX, mmY, 128.0f);
                    for (auto* su : selUnits) {
                        if (!su) continue;
                        if (nearUnit && nearUnit != su) {
                            bool targeted = false;
                            if (auto* attackAbility = dynamic_cast<CAbilityAttack*>(
                                    su->GetAbility(War3ID("Aatk")))) {
                                if (attackAbility->CheckCanTarget(simulation_, su, OrderIds::SMART, nearUnit)) {
                                    su->IssueOrder(std::make_unique<COrderTargetWidget>(OrderIds::SMART, nearUnit));
                                    targeted = true;
                                }
                            }
                            if (!targeted) {
                                su->IssueOrder(std::make_unique<COrderTargetPoint>(OrderIds::SMART, mmX, mmY));
                            }
                        } else {
                            su->IssueOrder(std::make_unique<COrderTargetPoint>(OrderIds::SMART, mmX, mmY));
                        }
                    }
                    issuedOrder = true;
                }
            }
            if (!issuedOrder && !selUnits.empty()) {
                int winW = sapp_width(), winH = sapp_height();
                float aspect = (winH > 0) ? (float)winW / winH : 1.0f;
                Mat4 view = camera_.GetViewMatrix();
                Mat4 proj = camera_.GetProjectionMatrix(aspect);
                CUnit* targetUnit = PickSimulationUnitAtScreenPoint(ev->mouse_x, ev->mouse_y,
                    view, proj, winW, winH, selectedUnitHandle_, false, true);
                CDestructable* targetDest = targetUnit ? nullptr
                    : PickSimulationDestructableAtScreenPoint(ev->mouse_x, ev->mouse_y, view, proj, winW, winH);
                CWidget* targetWidget = targetUnit ? static_cast<CWidget*>(targetUnit)
                    : static_cast<CWidget*>(targetDest);

                int pendingCmd = ui_.GetPendingCommand();
                if (pendingCmd >= 0) {
                    issuedOrder = IssuePendingCommandAtScreenPoint(ev->mouse_x, ev->mouse_y);
                } else {
                    auto terrainH = [&](float x, float z) {
                        return texturedTerrain_.IsValid() ? texturedTerrain_.GetGroundHeight(x, z) : 0.0f;
                    };
                    Vec3 wPos = picking::ScreenToWorld(ev->mouse_x, ev->mouse_y, view, proj, winW, winH,
                                                       texturedTerrain_.IsValid(), texturedTerrain_.GetRadius(), terrainH);

                    for (auto* su : selUnits) {
                        if (!su) continue;
                        int orderId = OrderIds::SMART;
                        bool canTargetWidget = false;

                        if (targetWidget) {
                            if (auto* harvestAbility = dynamic_cast<CAbilityHarvest*>(su->GetAbility(War3ID("Ahar")))) {
                                if (harvestAbility->CheckCanTarget(simulation_, su, OrderIds::HARVEST, targetWidget)) {
                                    orderId = OrderIds::HARVEST;
                                    canTargetWidget = true;
                                } else if (targetUnit && harvestAbility->CheckCanTarget(simulation_, su, OrderIds::RETURN_RESOURCES, targetWidget)) {
                                    orderId = OrderIds::RETURN_RESOURCES;
                                    canTargetWidget = true;
                                }
                            }
                            if (!canTargetWidget) {
                                if (auto* repairAbility = dynamic_cast<CAbilityRepair*>(su->GetAbility(War3ID("Arep")))) {
                                    if (repairAbility->CheckCanTarget(simulation_, su, OrderIds::REPAIR, targetWidget)) {
                                        orderId = OrderIds::REPAIR;
                                        canTargetWidget = true;
                                    }
                                }
                            }
                            if (!canTargetWidget) {
                                if (auto* attackAbility = dynamic_cast<CAbilityAttack*>(su->GetAbility(War3ID("Aatk")))) {
                                    if (attackAbility->CheckCanTarget(simulation_, su, OrderIds::SMART, targetWidget)) {
                                        orderId = OrderIds::SMART;
                                        canTargetWidget = true;
                                    }
                                }
                            }
                            if (!canTargetWidget && targetUnit) {
                                if (auto* moveAbility = dynamic_cast<CAbilityMove*>(su->GetAbility(War3ID("Amov")))) {
                                    if (moveAbility->CheckCanTarget(simulation_, su, OrderIds::SMART, targetWidget)) {
                                        orderId = OrderIds::SMART;
                                        canTargetWidget = true;
                                    }
                                }
                            }
                        }

                        if (canTargetWidget) {
                            su->IssueOrder(std::make_unique<COrderTargetWidget>(orderId, targetWidget));
                        } else {
                            su->IssueOrder(std::make_unique<COrderTargetPoint>(OrderIds::SMART, wPos.x, wPos.z));
                        }
                    }
                    issuedOrder = true;
                }
            }
        }
        if (!issuedOrder) {
            mouseDown[2] = true;
            sapp_lock_mouse(true);
        }
    } else {
        sapp_lock_mouse(true);
    }
}

bool GameController::IssuePendingCommandAtScreenPoint(float screenX, float screenY) {
    if (!simulationInitialized_ || selectedUnitHandles_.empty()) {
        return false;
    }

    const int pendingCmd = ui_.GetPendingCommand();
    if (pendingCmd < 0) {
        return false;
    }

    std::vector<CUnit*> selUnits = GetSelectedSimulationUnits();
    if (selUnits.empty()) {
        ui_.ClearPendingCommand();
        return false;
    }

    // Minimap routing first: 3D raycasts from minimap pixels are
    // meaningless. Point orders land on the world point; widget-mode orders
    // resolve the nearest live unit (retail minimap targeting).
    float mmX = 0.0f, mmY = 0.0f;
    const bool viaMinimap = MinimapWorldFromScreen(screenX, screenY, &mmX, &mmY);
    CUnit* mmUnit = viaMinimap ? PickUnitNearWorldPoint(mmX, mmY, 128.0f) : nullptr;

    int winW = sapp_width(), winH = sapp_height();
    float aspect = (winH > 0) ? (float)winW / winH : 1.0f;
    Mat4 view = camera_.GetViewMatrix();
    Mat4 proj = camera_.GetProjectionMatrix(aspect);
    CUnit* targetUnit = viaMinimap ? mmUnit
        : PickSimulationUnitAtScreenPoint(screenX, screenY,
            view, proj, winW, winH, selectedUnitHandle_, false, true);
    CDestructable* targetDest = (targetUnit || viaMinimap) ? nullptr
        : PickSimulationDestructableAtScreenPoint(screenX, screenY, view, proj, winW, winH);
    CWidget* targetWidget = targetUnit ? static_cast<CWidget*>(targetUnit)
        : static_cast<CWidget*>(targetDest);

    auto terrainH = [&](float x, float z) {
        return texturedTerrain_.IsValid() ? texturedTerrain_.GetGroundHeight(x, z) : 0.0f;
    };
    Vec3 wPos = viaMinimap ? Vec3(mmX, 0.0f, mmY)
        : picking::ScreenToWorld(screenX, screenY, view, proj, winW, winH,
                                 texturedTerrain_.IsValid(), texturedTerrain_.GetRadius(), terrainH);

    const auto pendingMode = ui_.GetPendingCommandMode();
    ui_.CompletePendingCommand();
    for (auto* su : selUnits) {
        if (!su) continue;
        if (pendingMode == MainUI::PendingCommandMode::POINT_ONLY) {
            su->IssueOrder(std::make_unique<COrderTargetPoint>(pendingCmd, wPos.x, wPos.z));
        } else if (targetWidget) {
            bool canTarget = false;
            for (auto* ab : su->GetAbilities()) {
                if (ab && !ab->IsDisabled() && ab->CheckCanTarget(simulation_, su, pendingCmd, targetWidget)) {
                    canTarget = true;
                    break;
                }
            }
            if (canTarget) {
                su->IssueOrder(std::make_unique<COrderTargetWidget>(pendingCmd, targetWidget));
            } else {
                su->IssueOrder(std::make_unique<COrderTargetPoint>(pendingCmd, wPos.x, wPos.z));
            }
        } else {
            su->IssueOrder(std::make_unique<COrderTargetPoint>(pendingCmd, wPos.x, wPos.z));
        }
    }

    RefreshSelectionUi(true);
    std::cout << "PendingCommand: issued order=" << pendingCmd
              << " mode=" << static_cast<int>(pendingMode)
              << " units=" << selUnits.size()
              << " target=" << (targetWidget ? "widget" : "point")
              << std::endl;
    return true;
}

void GameController::OnMouseUp(const sapp_event* ev) {
    if (!selectionPressActive_ && ui_.HandleEvent(ev)) return;

    int btn = (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) ? 0 :
              (ev->mouse_button == SAPP_MOUSEBUTTON_RIGHT) ? 1 : 2;
    if (!selectionPressActive_ && ui_.OnMouseUp(ev->mouse_x, ev->mouse_y, btn)) return;

    if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
        mouseDown[0] = false;
        if (selectionPressActive_) {
            bool additive = Platform::IsKeyDown(SAPP_KEYCODE_LEFT_SHIFT) || Platform::IsKeyDown(SAPP_KEYCODE_RIGHT_SHIFT);
            ScreenRect rect;
            rect.x = std::min(selectionStartX_, selectionCurrentX_);
            rect.y = std::min(selectionStartY_, selectionCurrentY_);
            rect.w = std::fabs(selectionCurrentX_ - selectionStartX_);
            rect.h = std::fabs(selectionCurrentY_ - selectionStartY_);
            if (selectionDragVisible_ && rect.w >= 4.0f && rect.h >= 4.0f) {
                SelectUnitsInRect(rect, additive);
            } else if (simulationInitialized_ && !simulation_.GetUnits().empty()) {
                int winW = sapp_width(), winH = sapp_height();
                float aspect = (winH > 0) ? (float)winW / winH : 1.0f;
                Mat4 view = camera_.GetViewMatrix();
                Mat4 proj = camera_.GetProjectionMatrix(aspect);
                CUnit* bestUnit = PickSimulationUnitAtScreenPoint(ev->mouse_x, ev->mouse_y,
                    view, proj, winW, winH, -1, false, true);
                if (bestUnit) {
                    PlayUiSound("Click", 0.5f);
                    SelectSingleUnit(bestUnit, additive);
                    std::cout << "SelectedUnit: "
                              << unit_text_utils::DescribeUnitType(bestUnit->GetTypeId(), unitData_)
                              << " handle=" << bestUnit->GetHandleId()
                              << " count=" << selectedUnitHandles_.size() << std::endl;
                } else if (!additive) {
                    SetSelectedUnits({}, -1);
                }
            }
        }
        selectionPressActive_ = false;
        selectionDragVisible_ = false;
        ui_.ClearSelectionBox();
    } else if (ev->mouse_button == SAPP_MOUSEBUTTON_RIGHT) {
        mouseDown[2] = false;
    }
    sapp_lock_mouse(false);
}

void GameController::OnMouseMove(const sapp_event* ev) {
    mousePosX_ = ev->mouse_x;
    mousePosY_ = ev->mouse_y;
    if (!sapp_mouse_locked() && ui_.HandleEvent(ev)) {
        hoveredUnitHandle_ = -1;
        return;
    }
    if (mouseDown[0] && selectionPressActive_ && !sapp_mouse_locked()) {
        selectionCurrentX_ = ev->mouse_x;
        selectionCurrentY_ = ev->mouse_y;
        ScreenRect rect;
        rect.x = std::min(selectionStartX_, selectionCurrentX_);
        rect.y = std::min(selectionStartY_, selectionCurrentY_);
        rect.w = std::fabs(selectionCurrentX_ - selectionStartX_);
        rect.h = std::fabs(selectionCurrentY_ - selectionStartY_);
        if (rect.w >= 4.0f || rect.h >= 4.0f) {
            selectionDragVisible_ = true;
            ui_.SetSelectionBox(rect);
        }
    }
    if (sapp_mouse_locked()) {
        mouseDX += ev->mouse_dx;
        mouseDY += ev->mouse_dy;
    }
}

void GameController::OnMouseScroll(const sapp_event* ev) {
    if (ui_.HandleEvent(ev)) return;
    mouseScroll += ev->scroll_y;
}

void GameController::OnKeyDown(const sapp_event* ev) {
    if (ui_.HandleEvent(ev)) return;
    switch (ev->key_code) {
    case SAPP_KEYCODE_ESCAPE:
        // Cinematic skip (ref ESC -> EVENT_PLAYER_END_CINEMATIC): only the
        // local player's skip triggers fire, and only in cinematic mode.
        if (jassNatives_ && jassNatives_->GetData() && jassNatives_->GetData()->cinematicMode) {
            const int local = config_.localPlayerIndex >= 0 ? config_.localPlayerIndex : 0;
            jassNatives_->FireCinematicSkipEvent(local);
        }
        break;    case SAPP_KEYCODE_P:
        showPathing = !showPathing;
        texturedTerrain_.SetShowPathing(showPathing);
        break;
    case SAPP_KEYCODE_R:
        showRegions = !showRegions;
        texturedTerrain_.SetShowRegions(showRegions);
        break;
    case SAPP_KEYCODE_F1:
        wireframe = !wireframe;
        break;
    case SAPP_KEYCODE_M:
        showMinimap = !showMinimap;
        break;
    case SAPP_KEYCODE_N: {
        static bool musicOn = false;
        if (!musicOn) {
            bool ok = audio_.PlayMusic("Sound\\Music\\mp3\\MainMenu.mp3", &mpq_, 0.5f, true);
            if (!ok) audio_.PlayMusic("Sound\\Music\\mp3\\Human1.mp3", &mpq_, 0.5f, true);
            if (ok || audio_.IsMusicPlaying()) { std::cout << "Music: on" << std::endl; musicOn = true; }
        } else {
            audio_.StopMusic(); std::cout << "Music: off" << std::endl; musicOn = false;
        }
        break;
    }
    case SAPP_KEYCODE_F2: {
        const bool enabled = !dayNight_.IsEnabled();
        dayNight_.SetEnabled(enabled);
        // Keep the JASS switch in sync so the per-frame HUD sync (ref
        // EnableDawnDusk owner) does not immediately revert the toggle.
        if (jassNatives_ && jassNatives_->GetData()) {
            jassNatives_->GetData()->dawnDuskEnabled = enabled;
        }
        std::cout << "Day/night: " << (enabled ? "on" : "off") << std::endl;
        break;
    }
    case SAPP_KEYCODE_F3: {
        static float speed = 1.0f;
        speed = (speed >= 8.0f) ? 1.0f : speed * 2.0f;
        dayNight_.SetDayLength(480.0f / speed);
        std::cout << "Day/night speed: " << speed << "x" << std::endl;
        break;
    }
    case SAPP_KEYCODE_D: {
        int mode = (texturedTerrain_.DebugMode() + 1) % 5;
        texturedTerrain_.SetDebugMode(mode);
        const char* labels[] = {"normal", "hasTex(R/G)", "UV", "rawTex", "base-texture-only"};
        std::cout << "Debug mode: " << labels[mode] << std::endl;
        break;
    }
    default: break;
    }
}

// --- Selection / picking ---

bool GameController::IsUnitSelected(int handleId) const {
    return selectedUnitHandlesSet_.find(handleId) != selectedUnitHandlesSet_.end();
}

std::vector<CUnit*> GameController::GetSelectedSimulationUnits() const {
    std::vector<CUnit*> units;
    units.reserve(selectedUnitHandles_.size());
    for (int handleId : selectedUnitHandles_) {
        CUnit* unit = simulation_.GetUnit(handleId);
        if (!unit || unit->IsDead() || unit->IsHidden()) continue;
        units.push_back(unit);
    }
    return units;
}

// --- Init helpers (melee start, etc.) ---

static bool IsHumanTownHallType(War3ID typeId) {
    // Any-race town hall tier (ref melee starting town halls per race).
    return typeId == War3ID("htow") || typeId == War3ID("hkee") || typeId == War3ID("hcas")
        || typeId == War3ID("ogre") || typeId == War3ID("ofrt") || typeId == War3ID("ostr")
        || typeId == War3ID("unpl") || typeId == War3ID("unp1") || typeId == War3ID("unp2")
        || typeId == War3ID("etol") || typeId == War3ID("etoa") || typeId == War3ID("etoe");
}

static int CountPlayerUnitsOfType(CSimulation& sim, int playerIndex, War3ID typeId) {
    int count = 0;
    for (auto* unit : sim.GetUnits())
        if (unit && unit->GetPlayerIndex() == playerIndex && unit->GetTypeId() == typeId) ++count;
    return count;
}

static int CountPlayerTownHalls(CSimulation& sim, int playerIndex) {
    int count = 0;
    for (auto* unit : sim.GetUnits())
        if (unit && unit->GetPlayerIndex() == playerIndex && IsHumanTownHallType(unit->GetTypeId())) ++count;
    return count;
}

static bool IsCampaignMapPath(const std::string& mapPath) {
    std::string path = LowerAscii(mapPath);
    std::replace(path.begin(), path.end(), '/', '\\');
    return path.find("\\maps\\campaign\\") != std::string::npos
        || path.find("\\maps\\frozenthrone\\campaign\\") != std::string::npos
        || path.find("warsmash_mpq_map_maps_campaign_") != std::string::npos
        || path.find("warsmash_mpq_map_maps_frozenthrone_campaign_") != std::string::npos
        || path.rfind("maps\\campaign\\", 0) == 0
        || path.rfind("maps\\frozenthrone\\campaign\\", 0) == 0;
}

static bool FindPlayerTownHallPosition(CSimulation& sim, int playerIndex, float& outX, float& outY) {
    for (auto* unit : sim.GetUnits())
        if (unit && unit->GetPlayerIndex() == playerIndex && IsHumanTownHallType(unit->GetTypeId())) {
            outX = unit->GetX(); outY = unit->GetY(); return true;
        }
    return false;
}

static CUnit* FindGameplaySmokeWorker(CSimulation& sim, int playerIndex) {
    CUnit* fallback = nullptr;
    for (auto* unit : sim.GetUnits()) {
        if (!unit || unit->IsDead() || unit->IsHidden() || unit->GetPlayerIndex() != playerIndex) continue;
        if (unit->GetAbility(War3ID("Ahar"))) {
            if (unit->GetTypeId() == War3ID("hpea")) return unit;
            if (!fallback) fallback = unit;
        }
    }
    return fallback;
}

static CUnit* FindNearestGoldMine(CSimulation& sim, const CUnit* worker) {
    if (!worker) return nullptr;
    CUnit* best = nullptr;
    double bestDistSq = std::numeric_limits<double>::max();
    for (auto* unit : sim.GetUnits()) {
        if (!unit || unit->IsDead()) continue;
        if (!unit->GetAbility(War3ID("Agld"))) continue;
        const double distSq = unit->DistanceSquared(worker->GetX(), worker->GetY());
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            best = unit;
        }
    }
    return best;
}

static CUnit* FindPlayerDropoff(CSimulation& sim, int playerIndex) {
    CUnit* fallback = nullptr;
    for (auto* unit : sim.GetUnits()) {
        if (!unit || unit->IsDead() || unit->GetPlayerIndex() != playerIndex) continue;
        if (unit->GetAbility(War3ID("Aret"))) {
            if (IsHumanTownHallType(unit->GetTypeId())) return unit;
            if (!fallback) fallback = unit;
        }
    }
    return fallback;
}

static bool FindGameplaySmokeBuildSite(CSimulation& sim, const CUnit* anchor, War3ID buildingTypeId,
                                       float& outX, float& outY) {
    if (!anchor) return false;

    const float offsets[][2] = {
        {384.0f, 0.0f}, {-384.0f, 0.0f}, {0.0f, 384.0f}, {0.0f, -384.0f},
        {384.0f, 384.0f}, {-384.0f, 384.0f}, {384.0f, -384.0f}, {-384.0f, -384.0f},
        {512.0f, 0.0f}, {0.0f, 512.0f}, {-512.0f, 0.0f}, {0.0f, -512.0f}
    };

    for (const auto& offset : offsets) {
        float x = anchor->GetX() + offset[0];
        float y = anchor->GetY() + offset[1];
        CAbilityBuild::RoundBuildPosition(x, y);
        if (!CAbilityBuild::IsBuildLocationObstructed(sim, x, y, anchor->GetPlayerIndex(), buildingTypeId)) {
            outX = x;
            outY = y;
            return true;
        }
    }
    return false;
}

static bool FindPlayerStartLocationPosition(const JassNativesData* data, int playerIndex, float& outX, float& outY) {
    if (!data) return false;
    if (playerIndex < 0 || playerIndex >= (int)data->playerMapConfig.size()) return false;
    const auto& pc = data->playerMapConfig[playerIndex];
    const int si = pc.startLocationIndex;
    if (si < 0 || si >= (int)data->startLocations.size()) return false;
    const auto& start = data->startLocations[si];
    if (!start.defined) return false;
    outX = start.x; outY = start.y; return true;
}

static bool FindInitialSpawnFocusPosition(CSimulation& sim, GameController* g, float& outX, float& outY) {
    const int local = g ? g->GetConfig().localPlayerIndex : -1;
    // Prefer the local (human) slot: campaign maps put it off slot 0
    // (Human01: Player 1). Fall back to the old slot-0-first order.
    if (local >= 0) {
        if (FindPlayerTownHallPosition(sim, local, outX, outY)) return true;
    }
    auto* data = g->GetJassNatives() ? g->GetJassNatives()->GetData().get() : nullptr;
    if (local >= 0 && FindPlayerStartLocationPosition(data, local, outX, outY)) return true;
    if (FindPlayerTownHallPosition(sim, 0, outX, outY)) return true;
    if (FindPlayerStartLocationPosition(data, 0, outX, outY)) return true;
    if (!data) return false;
    int playerLimit = data->configuredPlayerCount > 0
        ? std::min(data->configuredPlayerCount, (int)data->playerMapConfig.size())
        : (int)data->playerMapConfig.size();
    for (int pi = 0; pi < playerLimit; ++pi) {
        if (FindPlayerTownHallPosition(sim, pi, outX, outY)) return true;
        if (FindPlayerStartLocationPosition(data, pi, outX, outY)) return true;
    }
    return false;
}

void GameController::FocusCameraAndMinimapOnInitialSpawn() {
    float focusX = 0, focusY = 0;
    if (!FindInitialSpawnFocusPosition(simulation_, this, focusX, focusY)) return;
    float gh = texturedTerrain_.IsValid() ? texturedTerrain_.GetGroundHeight(focusX, focusY) : 0.0f;
    camera_.SetTarget(focusX, gh + 100.0f, focusY);
    camera_.SetDistance(2200.0f);
    camera_.SaveHomeState();
    minimap_.ClearMarkers();
    minimap_.AddMarker(focusX, focusY, 0xFFD54AFFu, 10.0f);
    std::cout << "InitialFocus: spawn at (" << focusX << ", " << focusY << ")" << std::endl;
}

static War3ID RaceTownHallId(int race) {
    switch (race) {
        case 2: return War3ID("ogre");
        case 3: return War3ID("unpl");
        case 4: return War3ID("etol");
        default: return War3ID("htow");
    }
}

static War3ID RaceWorkerId(int race) {
    switch (race) {
        case 2: return War3ID("opeo");
        case 3: return War3ID("uaco");
        case 4: return War3ID("ewsp");
        default: return War3ID("hpea");
    }
}

void GameController::EnsureHumanMeleeStartForPlayer(int playerIndex, float startX, float startY) {
    // Race-correct fallback when the map script did not create starting
    // units (ref Blizzard.j MeleeStartingUnitsHuman/Orc/Undead/NightElf).
    const CPlayer* player = simulation_.GetPlayer(playerIndex);
    const int race = player ? player->GetRace() : 1;
    const War3ID townHallId = RaceTownHallId(race);
    const War3ID workerId = RaceWorkerId(race);
    if (!unitData_.HasUnitType(townHallId) || !unitData_.HasUnitType(workerId)) return;

    constexpr float townHallFacing = 270.0f;
    constexpr float peasantFacing = 90.0f;
    static const float peasantOffsets[][2] = {
        {-220.0f,  120.0f}, {-220.0f,  -20.0f}, {-220.0f, -160.0f},
        {-80.0f,   180.0f}, {-80.0f,  -220.0f},
    };

    if (CountPlayerTownHalls(simulation_, playerIndex) == 0) {
        simulation_.CreateUnit(townHallId, playerIndex, startX, startY, townHallFacing);
        std::cout << "MeleeFallback: created Town Hall for player " << playerIndex << std::endl;
    }
    float thX = startX, thY = startY;
    FindPlayerTownHallPosition(simulation_, playerIndex, thX, thY);
    int existingPeasants = CountPlayerUnitsOfType(simulation_, playerIndex, workerId);
    for (int i = existingPeasants; i < 5; ++i) {
        simulation_.CreateUnit(workerId, playerIndex, thX + peasantOffsets[i][0], thY + peasantOffsets[i][1], peasantFacing);
    }
    if (existingPeasants < 5)
        std::cout << "MeleeFallback: created " << (5 - existingPeasants) << " Peasants for player " << playerIndex << std::endl;
}

void GameController::EnsureHumanMeleeStartsFromMapConfig() {
    if (!jassNatives_ || !simulationInitialized_) return;
    if (IsCampaignMapPath(mapPath_)) {
        std::cout << "MeleeFallback: skipped for campaign map" << std::endl;
        return;
    }
    auto data = jassNatives_->GetData();
    if (!data) return;

    bool createdAny = false;
    int playerLimit = data->configuredPlayerCount > 0
        ? std::min(data->configuredPlayerCount, (int)data->playerMapConfig.size())
        : (int)data->playerMapConfig.size();

    for (int pi = 0; pi < playerLimit; ++pi) {
        const auto& pc = data->playerMapConfig[pi];
        if (pc.startLocationIndex < 0) continue;
        // Inactive/closed slots never get starting units (ref slotActive).
        if (!pc.slotActive) continue;
        const int si = pc.startLocationIndex;
        if (si < 0 || si >= (int)data->startLocations.size()) continue;
        const auto& start = data->startLocations[si];
        if (!start.defined) continue;
        if (auto* player = simulation_.GetPlayer(pi)) player->SetStartLocation(start.x, start.y);

        const int raceForCount = simulation_.GetPlayer(pi) ? simulation_.GetPlayer(pi)->GetRace() : 1;
        const War3ID workerForCount = RaceWorkerId(raceForCount);
        int beforeTH = CountPlayerTownHalls(simulation_, pi);
        int beforePeasants = CountPlayerUnitsOfType(simulation_, pi, workerForCount);
        EnsureHumanMeleeStartForPlayer(pi, start.x, start.y);
        if (CountPlayerTownHalls(simulation_, pi) > beforeTH ||
            CountPlayerUnitsOfType(simulation_, pi, workerForCount) > beforePeasants)
            createdAny = true;
    }
    if (createdAny) simulation_.Update();
}

void GameController::CollectCarriedHeroes() {
    carriedHeroes_.clear();
    arrivedViaChangeLevel_ = false;
    if (!simulationInitialized_) return;
    for (CUnit* unit : simulation_.GetUnits()) {
        CarriedHero carried = SnapshotHero(unit, simulation_);
        if (carried.typeId.value != 0) carriedHeroes_.push_back(std::move(carried));
    }
    arrivedViaChangeLevel_ = true;
    std::cout << "ChangeLevel: carrying " << carriedHeroes_.size() << " heroes" << std::endl;
}

void GameController::SpawnCarriedHeroes() {
    if (!arrivedViaChangeLevel_) return;
    arrivedViaChangeLevel_ = false;
    std::vector<CarriedHero> carried = std::move(carriedHeroes_);
    carriedHeroes_.clear();
    if (carried.empty() || !simulationInitialized_) return;
    auto* data = jassNatives_ ? jassNatives_->GetData().get() : nullptr;
    Vec3 center = texturedTerrain_.IsValid() ? texturedTerrain_.GetCenter() : Vec3(0, 0, 0);
    int spawned = 0;
    for (size_t hi = 0; hi < carried.size(); ++hi) {
        const CarriedHero& ch = carried[hi];
        CPlayer* owner = simulation_.GetPlayer(ch.ownerPlayer);
        if (!owner) continue;
        if (!simulation_.GetUnitData() ||
            !simulation_.GetUnitData()->GetUnitType(ch.typeId)) {
            continue; // hero type absent from the next map's data
        }
        // Start location, else an owned building, else map center, each
        // fanned out so stacked heroes do not overlap.
        float sx = center.x, sy = center.z;
        bool placed = data && FindPlayerStartLocationPosition(data, ch.ownerPlayer, sx, sy);
        if (!placed) {
            for (CUnit* unit : simulation_.GetUnits()) {
                if (!unit || unit->IsDead() || unit->GetPlayerIndex() != ch.ownerPlayer) continue;
                if (unit->GetUnitType() && unit->GetUnitType()->IsBuilding()) {
                    sx = unit->GetX();
                    sy = unit->GetY();
                    placed = true;
                    break;
                }
            }
        }
        const float angle = (float)hi * 2.39996f; // golden angle fan-out
        const float radius = 128.0f + 64.0f * (float)hi;
        sx += std::cos(angle) * radius;
        sy += std::sin(angle) * radius;
        CUnit* hero = RespawnCarriedHero(simulation_, ch, sx, sy);
        if (hero) spawned++;
    }
    if (spawned > 0) simulation_.Update();
    std::cout << "ChangeLevel: respawned " << spawned << "/" << carried.size()
              << " carried heroes" << std::endl;
}

void GameController::UpdateMidiAmbient(float dt) {
    if (!audio_.IsValid() || !simulationInitialized_) return;
    ambientCheckAcc_ += dt;
    if (ambientCheckAcc_ < 3.0f) return; // re-evaluate a few times a second
    ambientCheckAcc_ = 0.0f;

    // JASS SetSoundEnvironment wins; otherwise the map's w3i ambience.
    std::string env;
    if (jassNatives_ && jassNatives_->GetData() &&
        !jassNatives_->GetData()->music.soundEnvironment.empty()) {
        env = jassNatives_->GetData()->music.soundEnvironment;
    } else if (!w3i_.soundEnvironment.empty()) {
        env = w3i_.soundEnvironment;
    }
    if (env.empty()) {
        audio_.StopMidiAmbient();
        return;
    }
    // Retail layout Sound\Ambient\<env>\<file>.mid; file casing varies
    // (Wetlandsday.mid vs WetlandsNight.mid), so match case-insensitively.
    // ListAllFiles returns full archive paths for the pattern.
    const bool day = simulation_.IsDaytime();
    std::string dir = "Sound\\Ambient\\" + env + "\\";
    std::string want = day ? "day" : "night";
    std::string chosen;
    for (const std::string& name : mpq_.ListAllFiles(dir + "*.mid")) {
        std::string lower = name;
        for (char& c : lower) c = (char)std::tolower((unsigned char)c);
        if (lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".mid") == 0 &&
            lower.find(want) != std::string::npos) {
            chosen = name;
            break;
        }
    }
    if (chosen.empty()) {
        audio_.StopMidiAmbient();
        return;
    }
    if (!audio_.IsMidiAmbientPlaying(chosen)) {
        audio_.PlayMidiAmbient(chosen, &mpq_, 0.45f);
    }
}

void GameController::SyncMusic() {
    // In-game map music only; the menu jingle is managed directly and has
    // no sim or JASS state.
    if (!audio_.IsValid() || !simulationInitialized_ || !jassNatives_) return;
    auto data = jassNatives_->GetData();
    if (!data) return;
    const bool wantPlaying = data->music.playing && !data->music.current.empty();
    const float wantVolume =
        std::max(0.0f, std::min(1.0f, (float)data->music.volume / 127.0f));
    if (wantPlaying &&
        (!musicSyncedPlaying_ || musicSyncedTrack_ != data->music.current)) {
        if (audio_.PlayMusic(data->music.current, &mpq_, wantVolume)) {
            musicSyncedTrack_ = data->music.current;
            musicSyncedPlaying_ = true;
            musicSyncedVolume_ = wantVolume;
        } else {
            // Missing file: latch as synced-stopped so a broken map does
            // not respam every frame; any state change retries.
            musicSyncedTrack_ = data->music.current;
            musicSyncedPlaying_ = false;
        }
        return;
    }
    if (!wantPlaying && musicSyncedPlaying_) {
        audio_.StopMusic();
        musicSyncedPlaying_ = false;
        return;
    }
    if (wantPlaying && musicSyncedPlaying_ && wantVolume != musicSyncedVolume_) {
        audio_.SetMusicVolume(wantVolume);
        musicSyncedVolume_ = wantVolume;
    }
}

int GameController::PlayUiSound(const std::string& key, float volume) {
    if (!audio_.IsValid() || frameCount_ <= 10) return -1;
    const int id = uiSounds_.Play(&audio_, &mpq_, key, volume);
    if (id >= 0) return id;
    // Fallback to the preloaded generic click/error samples.
    if (key == "Error") return audio_.Play2D("ui_error", volume);
    return audio_.Play2D("ui_click", volume);
}

void GameController::MaybeLogAIStatus() {
    if (!std::getenv("WARSMASH_AI_STATUS")) return;
    if (!simulationInitialized_ || !jassNatives_) return;
    const int tick = simulation_.GetGameTurnTick();
    static int lastLoggedTick = -100000;
    if (tick - lastLoggedTick < 200) return; // ~10s sim time
    lastLoggedTick = tick;
    auto data = jassNatives_->GetData();
    for (int pi = 0; pi < simulation_.GetPlayerCount(); ++pi) {
        const CPlayer* player = simulation_.GetPlayer(pi);
        if (!player || player->GetController() != CMapControl::COMPUTER) continue;
        int units = 0, buildings = 0, workers = 0;
        for (const CUnit* u : simulation_.GetUnits()) {
            if (!u || u->IsDead() || u->GetPlayerIndex() != pi) continue;
            ++units;
            const CUnitType* t = u->GetUnitType();
            if (t && t->IsBuilding()) { ++buildings; continue; }
            const War3ID id = u->GetTypeId();
            if (id == War3ID("hpea") || id == War3ID("opeo") ||
                id == War3ID("uaco") || id == War3ID("ewsp") ||
                u->GetAbility(War3ID("Ahar")) != nullptr) {
                ++workers;
            }
        }
        int thinks = -1, waves = -1;
        if (data) {
            auto it = data->aiControllers.find(pi);
            if (it != data->aiControllers.end() && it->second) {
                thinks = it->second->GetThinkCount();
                waves = it->second->GetWaveNumber();
            }
        }
        std::cout << "AIStatus: t=" << tick << " player=" << pi
                  << " race=" << player->GetRace()
                  << " units=" << units << " buildings=" << buildings
                  << " workers=" << workers << " gold=" << player->GetGold()
                  << " lumber=" << player->GetLumber()
                  << " thinks=" << thinks << " waves=" << waves;
        // Probe first worker: ability + behavior, to diagnose harvest stalls.
        for (const CUnit* u : simulation_.GetUnits()) {
            if (!u || u->IsDead() || u->GetPlayerIndex() != pi) continue;
            const War3ID id = u->GetTypeId();
            if (id == War3ID("hpea") || id == War3ID("opeo") ||
                id == War3ID("uaco") || id == War3ID("ewsp")) {
                const bool hasHar = u->GetAbility(War3ID("Ahar")) != nullptr;
                const CBehavior* b = u->GetCurrentBehavior();
                int carried = 0;
                if (const auto* har = dynamic_cast<const CAbilityHarvest*>(
                        u->GetAbility(War3ID("Ahar")))) {
                    carried = har->GetCarriedAmount();
                }
                float hallDist = -1.0f;
                for (const CUnit* h : simulation_.GetUnits()) {
                    if (!h || h->IsDead() || h->GetPlayerIndex() != pi) continue;
                    if (h->GetUnitType() && h->GetUnitType()->IsBuilding()) {
                        const float dx = h->GetX() - u->GetX();
                        const float dy = h->GetY() - u->GetY();
                        const float d = std::sqrt(dx * dx + dy * dy);
                        if (hallDist < 0.0f || d < hallDist) hallDist = d;
                    }
                }
                std::cout << " probeWorker=" << id.ToString()
                          << " hasAhar=" << (hasHar ? 1 : 0)
                          << " queued=" << (u->HasQueuedOrders() ? 1 : 0)
                          << " behav=" << (b ? (int)b->GetCategory() : -1)
                          << " carried=" << carried
                          << " pos=(" << (int)u->GetX() << "," << (int)u->GetY() << ")"
                          << " hallDist=" << (int)hallDist
                          << " speed=" << u->GetSpeed()
                          << " turnRate=" << u->GetTurnRate()
                          << " moveType=" << (u->GetUnitType() ? (int)u->GetMovementType() : -1)
                          << " facing=" << (int)u->GetFacing()
                          << " coll=" << u->GetCollisionSize();
                if (auto* pf = simulation_.GetPathfinding()) {
                    std::cout << " steps=";
                    for (int dir = 0; dir < 8; ++dir) {
                        const float ang = dir * 3.14159265f / 4.0f;
                        const float nx = u->GetX() + std::cos(ang) * 24.0f;
                        const float ny = u->GetY() + std::sin(ang) * 24.0f;
                        const bool ok = pf->IsWorldPointPathableForUnit(
                            nx, ny, std::max(0.0f, u->GetCollisionSize()),
                            u->GetMovementType(), const_cast<CUnit*>(u));
                        std::cout << (ok ? '1' : '0');
                    }
                    std::cout << " near=[";
                    int shown = 0;
                    for (const CUnit* o : simulation_.GetUnits()) {
                        if (!o || o == u || o->IsDead()) continue;
                        const float dx = o->GetX() - u->GetX();
                        const float dy = o->GetY() - u->GetY();
                        if (dx * dx + dy * dy > 200.0f * 200.0f) continue;
                        if (shown++ > 0) std::cout << ",";
                        std::cout << o->GetTypeId().ToString() << "p" << o->GetPlayerIndex()
                                  << "@" << (int)std::sqrt(dx * dx + dy * dy)
                                  << "c" << (int)o->GetCollisionSize()
                                  << (o->IsHidden() ? "H" : "");
                    }
                    std::cout << "]";
                }
                break;
            }
        }
        std::cout << std::endl;
    }
}

void GameController::EnsureComputerAIStarted() {
    if (!jassNatives_ || !simulationInitialized_) return;
    auto data = jassNatives_->GetData();
    if (!data) return;
    for (int pi = 0; pi < simulation_.GetPlayerCount(); ++pi) {
        const CPlayer* player = simulation_.GetPlayer(pi);
        if (!player || player->GetController() != CMapControl::COMPUTER) continue;
        if (pi < (int)data->playerMapConfig.size() && !data->playerMapConfig[(size_t)pi].slotActive) continue;
        auto it = data->aiControllers.find(pi);
        if (it != data->aiControllers.end() && it->second) continue;
        auto controller = std::make_shared<MeleeAIController>(&simulation_, pi);
        controller->SetDifficulty(data->playerAI[pi].difficulty);
        auto paused = data->compAIpaused.find(pi);
        if (paused != data->compAIpaused.end() && paused->second) controller->SetEnabled(false);
        data->aiControllers[pi] = std::move(controller);
        std::cout << "ComputerAI: started MeleeAIController for player " << pi << std::endl;
    }
}

void GameController::FlushStartupSimulationState() {
    if (!simulationInitialized_) return;
    if (!diagnoseMapOnly_ && !debugGameplaySmoke_) {
        SyncAndReconcileVisuals();
    }
    if (!jassNatives_) return;
    constexpr int maxWarmup = 4;
    for (int i = 0; i < maxWarmup; ++i) {
        size_t before = simulation_.GetUnits().size();
        simulation_.Update();
        int timers = jassNatives_->UpdateTimers(CSimulation::SIM_STEP_TIME);
        jassNatives_->UpdateRegions();
        jassNatives_->UpdateRescue();
        int pumped = jassNatives_->PumpQueuedThreads(1024);
        if (!diagnoseMapOnly_ && !debugGameplaySmoke_) {
            SyncAndReconcileVisuals();
        }
        if (timers == 0 && pumped == 0 && simulation_.GetUnits().size() == before) break;
    }
}

void GameController::RefreshRuntimeUnitSplats() {
    splatMeta_.RefreshRuntimeSplats(texturedTerrain_, simulationInitialized_, simulation_, selectedUnitHandlesSet_, hoveredUnitHandle_);
    AddActiveEventSplatsToTerrain();
}

void GameController::AddActiveEventSplatsToTerrain() {
    if (!texturedTerrain_.IsValid()) {
        return;
    }
    for (const ActiveEventSplat& splat : activeEventSplats_) {
        const Vec4 tint = MdxEventSplatTint(splat.colors, splat.intervalTimes, splat.intervalCount,
                                            splat.lifeSpan, splat.lifeRemaining);
        const Vec4 uvRect = MdxEventSplatUvRect(splat.uvIntervals, splat.intervalTimes, splat.intervalCount,
                                                splat.columns, splat.rows, splat.lifeSpan, splat.lifeRemaining);
        const SplatModel::BlendMode blendMode = MdxEventSplatBlendMode(splat.blendMode);
        if (!splat.meshVertices.empty() && !splat.meshIndices.empty()) {
            std::vector<SplatModel::MeshVertex> vertices = splat.meshVertices;
            for (auto& vertex : vertices) {
                vertex.u = uvRect.x + vertex.u * (uvRect.z - uvRect.x);
                vertex.v = uvRect.y + vertex.v * (uvRect.w - uvRect.y);
                vertex.tint = tint;
            }
            texturedTerrain_.AddRuntimeSplatMesh(
                splat.key,
                splat.texturePath,
                vertices,
                splat.meshIndices,
                splat.highPriority,
                false,
                blendMode,
                splat.unshaded);
        } else {
            texturedTerrain_.AddRuntimeSplatRect(
                splat.key,
                splat.texturePath,
                splat.x - splat.halfSize,
                splat.y - splat.halfSize,
                splat.x + splat.halfSize,
                splat.y + splat.halfSize,
                8.0f,
                splat.highPriority,
                false,
                false,
                0.0f,
                tint,
                uvRect,
                blendMode,
                splat.unshaded);
        }
    }
}

// --- Unit visual ---

bool GameController::EnsureSimulationUnitVisual(CUnit* unit) {
    if (!unit) return false;
    int handleId = unit->GetHandleId();
    if (simUnitToVisualIdx.find(handleId) != simUnitToVisualIdx.end()) return true;
    if (simUnitVisualRegistrationFailed.find(handleId) != simUnitVisualRegistrationFailed.end()) return false;

    auto seedIt = simUnitVisualSeeds.find(handleId);
    float scale[3] = {1, 1, 1};
    uint32_t skinId = 0;
    int hp = 0, flags = 0;
    if (seedIt != simUnitVisualSeeds.end()) {
        scale[0] = seedIt->second.scale[0]; scale[1] = seedIt->second.scale[1]; scale[2] = seedIt->second.scale[2];
        skinId = seedIt->second.skinId; hp = seedIt->second.hitpoints; flags = seedIt->second.flags;
    }
    // Script unit scale multiplies the seed (per-frame sync keeps it live).
    const float us = unit->GetScale();
    scale[0] *= us;
    scale[1] *= us;
    scale[2] *= us;

    float height = texturedTerrain_.IsValid()
        ? texturedTerrain_.GetGroundHeight(unit->GetX(), unit->GetY())
        : 0.0f;
    float pos[3] = {unit->GetX(), unit->GetY(), height};
    int visualIdx = scene_.AddUnit(unit->GetTypeId().value, pos, unit->GetFacing(),
                                   scale, unit->GetPlayerIndex(), hp, flags, skinId);
    if (visualIdx < 0) {
        simUnitVisualRegistrationFailed.insert(handleId);
        return false;
    }
    simUnitToVisualIdx[handleId] = visualIdx;

    // Apply initial animation
    if (auto* viewer = scene_.GetViewer(visualIdx)) {
        SyncSimulationUnitAnimation(unit, viewer, this);
    }
    scene_.SetInstanceVisible(visualIdx,
        !unit->IsHidden() && !unit->IsLoadedAboard() && IsUnitVisibleInFog(unit));
    return true;
}

bool GameController::EnsureSimulationItemVisual(CItem* item) {
    if (!item) return false;
    const int handleId = item->GetHandleId();
    if (simItemToVisualIdx.find(handleId) != simItemToVisualIdx.end()) return true;
    if (simItemVisualRegistrationFailed.find(handleId) != simItemVisualRegistrationFailed.end()) return false;
    if (item->IsDead()) return false;

    float height = texturedTerrain_.IsValid()
        ? texturedTerrain_.GetGroundHeight(item->GetX(), item->GetY())
        : 0.0f;
    float pos[3] = {item->GetX(), item->GetY(), height};
    float scale[3] = {1.0f, 1.0f, 1.0f};
    const int visualIdx = scene_.AddItem(item->GetTypeId().value, pos, 0.0f, scale, 0, 0);
    if (visualIdx < 0) {
        simItemVisualRegistrationFailed.insert(handleId);
        return false;
    }

    simItemToVisualIdx[handleId] = visualIdx;
    bool explored = true;
    if (simulationInitialized_) {
        const int local = config_.localPlayerIndex >= 0 ? config_.localPlayerIndex : 0;
        explored = !simulation_.GetFogSystem()->IsMaskedTo(local, item->GetX(), item->GetY());
    }
    scene_.SetInstanceVisible(visualIdx, !item->IsHidden() && explored);
    return true;
}

// --- Selection helpers ---

static bool RectanglesOverlap(const ScreenRect& a, const ScreenRect& b) {
    if (!a.IsValid() || !b.IsValid()) return false;
    return !(a.x + a.w < b.x || b.x + b.w < a.x || a.y + a.h < b.y || b.y + b.h < a.y);
}

static bool ComputeUnitSelectionScreenBounds(const CUnit& unit, const Mat4& view, const Mat4& proj,
                                                int screenW, int screenH, ScreenRect& outRect,
                                                GameController* g) {
    float footX, footY, headX, headY, radiusPixels;
    auto terrainH = [&](float x, float z) {
        return g->GetTexturedTerrain().IsValid() ? g->GetTexturedTerrain().GetGroundHeight(x, z) : 0.0f;
    };
    if (!unit_selection_geom::ComputeUnitSelectionScreenShape(unit, view, proj, screenW, screenH,
                                                               g->GetTexturedTerrain().IsValid(), terrainH,
                                                               footX, footY, headX, headY, radiusPixels))
        return false;
    float minX = std::min(footX, headX) - radiusPixels;
    float maxX = std::max(footX, headX) + radiusPixels;
    float minY = std::min(footY, headY) - radiusPixels;
    float maxY = std::max(footY, headY) + radiusPixels;
    outRect = {minX, minY, maxX - minX, maxY - minY};
    return outRect.IsValid();
}

static std::vector<CUnit*> CollectUnitsInScreenRect(const ScreenRect& rect, const Mat4& view, const Mat4& proj,
                                                    int screenW, int screenH, CSimulation& sim, GameController* g) {
    std::vector<CUnit*> result, buildingFallback;
    if (!rect.IsValid()) return result;
    for (auto* unit : sim.GetUnits()) {
        if (!unit || unit->IsDead() || unit->IsHidden()) continue;
        ScreenRect unitRect;
        if (!ComputeUnitSelectionScreenBounds(*unit, view, proj, screenW, screenH, unitRect, g)) continue;
        if (RectanglesOverlap(rect, unitRect)) {
            if (unit->IsBuilding()) buildingFallback.push_back(unit);
            else result.push_back(unit);
        }
    }
    return result.empty() ? buildingFallback : result;
}

void GameController::SetSelectedUnits(const std::vector<CUnit*>& units, int primaryHandle, bool rebuildCommandCard) {
    selectedUnitHandles_.clear();
    selectedUnitHandlesSet_.clear();
    for (auto* unit : units) {
        if (!unit || unit->IsDead() || unit->IsHidden()) continue;
        int hid = unit->GetHandleId();
        if (selectedUnitHandlesSet_.insert(hid).second) selectedUnitHandles_.push_back(hid);
    }
    if (selectedUnitHandles_.empty()) selectedUnitHandle_ = -1;
    else if (primaryHandle >= 0 && selectedUnitHandlesSet_.find(primaryHandle) != selectedUnitHandlesSet_.end())
        selectedUnitHandle_ = primaryHandle;
    else selectedUnitHandle_ = selectedUnitHandles_.front();

    RefreshSelectionUi(rebuildCommandCard);
}

void GameController::SelectSingleUnit(CUnit* unit, bool additive) {
    if (!unit || unit->IsDead() || unit->IsHidden()) {
        if (!additive) SetSelectedUnits({}, -1);
        return;
    }
    if (!additive) { SetSelectedUnits({unit}, unit->GetHandleId()); return; }
    std::vector<CUnit*> units = GetSelectedSimulationUnits();
    bool already = false;
    for (auto* u : units) if (u && u->GetHandleId() == unit->GetHandleId()) { already = true; break; }
    if (!already) units.push_back(unit);
    SetSelectedUnits(units, unit->GetHandleId());
}

void GameController::SelectUnitsInRect(const ScreenRect& rect, bool additive) {
    int ww = sapp_width(), wh = sapp_height();
    float aspect = wh > 0 ? (float)ww / wh : 1.0f;
    Mat4 view = camera_.GetViewMatrix();
    Mat4 proj = camera_.GetProjectionMatrix(aspect);
    std::vector<CUnit*> units = CollectUnitsInScreenRect(rect, view, proj, ww, wh, simulation_, this);
    if (additive) {
        auto existing = GetSelectedSimulationUnits();
        existing.insert(existing.end(), units.begin(), units.end());
        SetSelectedUnits(existing);
    } else {
        SetSelectedUnits(units);
    }
}

void GameController::DebugSelectFirstUnitForHud() {
    if (!simulationInitialized_) return;

    CUnit* fallback = nullptr;
    CUnit* fallbackNonGoldMine = nullptr;
    CUnit* preferredWorker = nullptr;
    CUnit* localPlayerBuilding = nullptr;
    CUnit* localPlayerUnit = nullptr;
    const int localPlayer = config_.localPlayerIndex >= 0 ? config_.localPlayerIndex : 0;

    auto isGoldMine = [](CUnit* unit) {
        return unit && unit->GetAbility(War3ID("Agld")) != nullptr;
    };

    for (auto* unit : simulation_.GetUnits()) {
        if (!unit || unit->IsDead() || unit->IsHidden()) continue;
        if (!fallback) fallback = unit;
        if (!fallbackNonGoldMine && !isGoldMine(unit)) {
            fallbackNonGoldMine = unit;
        }
        if (unit->GetPlayerIndex() == localPlayer) {
            if (!isGoldMine(unit)) {
                if (!preferredWorker && unit->GetTypeId() == War3ID("hpea")) {
                    preferredWorker = unit;
                }
                if (!unit->IsBuilding()) {
                    localPlayerUnit = unit;
                }
                if (!localPlayerBuilding) {
                    localPlayerBuilding = unit;
                }
            }
        }
    }

    CUnit* selected = preferredWorker ? preferredWorker
        : (localPlayerUnit ? localPlayerUnit
        : (localPlayerBuilding ? localPlayerBuilding
        : (fallbackNonGoldMine ? fallbackNonGoldMine : fallback)));
    if (!selected) {
        std::cout << "DebugSelectFirstUnitForHud: no selectable unit found" << std::endl;
        return;
    }

    SetSelectedUnits({selected}, selected->GetHandleId(), true);
    std::cout << "DebugSelectFirstUnitForHud: selected "
              << unit_text_utils::DescribeUnitType(selected->GetTypeId(), unitData_)
              << " handle=" << selected->GetHandleId()
              << " player=" << selected->GetPlayerIndex() << std::endl;
}

void GameController::RunGameplaySmoke() {
    if (!simulationInitialized_) {
        std::cout << "GameplaySmoke: result=fail reason=simulation_not_initialized" << std::endl;
        return;
    }

    const int playerIndex = config_.localPlayerIndex >= 0 ? config_.localPlayerIndex : 0;
    CUnit* worker = FindGameplaySmokeWorker(simulation_, playerIndex);
    CUnit* dropoff = FindPlayerDropoff(simulation_, playerIndex);
    CUnit* mine = FindNearestGoldMine(simulation_, worker);
    CPlayer* player = simulation_.GetPlayer(playerIndex);
    auto* harvest = worker ? dynamic_cast<CAbilityHarvest*>(worker->GetAbility(War3ID("Ahar"))) : nullptr;

    if (!worker || !dropoff || !mine || !player || !harvest) {
        std::cout << "GameplaySmoke: result=fail"
                  << " worker=" << (worker ? "yes" : "no")
                  << " dropoff=" << (dropoff ? "yes" : "no")
                  << " mine=" << (mine ? "yes" : "no")
                  << " player=" << (player ? "yes" : "no")
                  << " harvestAbility=" << (harvest ? "yes" : "no")
                  << std::endl;
        return;
    }

    auto pumpTicks = [&](int maxTicks, const std::function<bool()>& done) {
        for (int i = 0; i < maxTicks; ++i) {
            simulation_.Update();
            if (jassNatives_) {
                jassNatives_->UpdateTimers(CSimulation::SIM_STEP_TIME);
                jassNatives_->UpdateRegions();
                jassNatives_->UpdateRescue();
                jassNatives_->PumpQueuedThreads(128);
            }
            if (done && done()) return i + 1;
        }
        return maxTicks;
    };

    auto issuePendingFromUi = [&](CWidget* targetWidget, float x, float y) {
        const int pendingCmd = ui_.GetPendingCommand();
        const auto pendingMode = ui_.GetPendingCommandMode();
        if (pendingCmd < 0) {
            return false;
        }
        ui_.CompletePendingCommand();
        bool issued = false;
        for (auto* unit : GetSelectedSimulationUnits()) {
            if (!unit) continue;
            if (pendingMode != MainUI::PendingCommandMode::POINT_ONLY && targetWidget) {
                unit->IssueOrder(std::make_unique<COrderTargetWidget>(pendingCmd, targetWidget));
            } else {
                unit->IssueOrder(std::make_unique<COrderTargetPoint>(pendingCmd, x, y));
            }
            issued = true;
        }
        return issued;
    };

    SetSelectedUnits({worker}, worker->GetHandleId(), true);
    const bool uiHarvestInitial = ui_.IsCommandOrderEnabled(OrderIds::HARVEST);

    const float moveStartX = worker->GetX();
    const float moveStartY = worker->GetY();
    worker->ClearOrders();
    const bool uiMoveClicked = ui_.InvokeCommandOrderForDiagnostics(OrderIds::MOVE);
    const bool uiMoveIssued = issuePendingFromUi(nullptr, moveStartX + 256.0f, moveStartY);
    const int moveTicks = pumpTicks(120, [&]() {
        const float dx = worker->GetX() - moveStartX;
        const float dy = worker->GetY() - moveStartY;
        return std::sqrt(dx * dx + dy * dy) > 32.0f;
    });
    const float moveDx = worker->GetX() - moveStartX;
    const float moveDy = worker->GetY() - moveStartY;
    const bool movePass = uiMoveClicked && uiMoveIssued && std::sqrt(moveDx * moveDx + moveDy * moveDy) > 32.0f;
    std::cout << "GameplaySmoke: move=" << (movePass ? "pass" : "fail")
              << " ticks=" << moveTicks
              << " uiClicked=" << (uiMoveClicked ? "yes" : "no")
              << " uiIssued=" << (uiMoveIssued ? "yes" : "no")
              << " start=(" << moveStartX << "," << moveStartY << ")"
              << " end=(" << worker->GetX() << "," << worker->GetY() << ")"
              << std::endl;

    worker->ClearOrders();
    worker->SetHidden(false);
    worker->SetInvulnerable(false);
    harvest->ClearCarried();
    worker->SetPosition(mine->GetX() + 96.0f, mine->GetY());
    const int mineGoldBefore = dynamic_cast<CAbilityGoldMine*>(mine->GetAbility(War3ID("Agld")))
        ? dynamic_cast<CAbilityGoldMine*>(mine->GetAbility(War3ID("Agld")))->GetGold()
        : 0;
    const bool uiHarvestClicked = ui_.InvokeCommandOrderForDiagnostics(OrderIds::HARVEST);
    const bool uiHarvestIssued = issuePendingFromUi(mine, mine->GetX(), mine->GetY());
    const int harvestTicks = pumpTicks(420, [&]() {
        return harvest->GetCarriedAmount() > 0;
    });
    const bool harvestPass = uiHarvestClicked && uiHarvestIssued && harvest->GetCarriedAmount() > 0;
    RefreshSelectionUi(true);
    const bool uiReturnAfterHarvest = ui_.IsCommandOrderEnabled(OrderIds::RETURN_RESOURCES);
    std::cout << "GameplaySmoke: harvest=" << (harvestPass ? "pass" : "fail")
              << " ticks=" << harvestTicks
              << " uiClicked=" << (uiHarvestClicked ? "yes" : "no")
              << " uiIssued=" << (uiHarvestIssued ? "yes" : "no")
              << " carried=" << harvest->GetCarriedAmount()
              << " type=" << static_cast<int>(harvest->GetCarriedResourceType())
              << " mineGoldBefore=" << mineGoldBefore
              << std::endl;

    const int goldBeforeReturn = player->GetGold();
    worker->ClearOrders();
    worker->SetHidden(false);
    worker->SetInvulnerable(false);
    worker->SetPosition(dropoff->GetX() + 96.0f, dropoff->GetY());
    const bool uiReturnClicked = ui_.InvokeCommandOrderForDiagnostics(OrderIds::RETURN_RESOURCES);
    const bool uiReturnIssued = issuePendingFromUi(dropoff, dropoff->GetX(), dropoff->GetY());
    const int returnTicks = pumpTicks(180, [&]() {
        return player->GetGold() > goldBeforeReturn && harvest->GetCarriedAmount() == 0;
    });
    const bool returnPass = uiReturnClicked && uiReturnIssued
        && player->GetGold() > goldBeforeReturn && harvest->GetCarriedAmount() == 0;
    std::cout << "GameplaySmoke: return=" << (returnPass ? "pass" : "fail")
              << " ticks=" << returnTicks
              << " uiClicked=" << (uiReturnClicked ? "yes" : "no")
              << " uiIssued=" << (uiReturnIssued ? "yes" : "no")
              << " goldBefore=" << goldBeforeReturn
              << " goldAfter=" << player->GetGold()
              << " carried=" << harvest->GetCarriedAmount()
              << std::endl;

    auto* queueAbility = dynamic_cast<CAbilityQueue*>(dropoff->GetAbility(War3ID("Aque")));
    SetSelectedUnits({dropoff}, dropoff->GetHandleId(), true);
    const bool uiTrainBeforeQueue = ui_.IsCommandOrderEnabled((int)War3ID("hpea").value);
    const int peasantsBeforeTrain = CountPlayerUnitsOfType(simulation_, playerIndex, War3ID("hpea"));
    const int goldBeforeTrain = player->GetGold();
    const int lumberBeforeTrain = player->GetLumber();
    const int foodBeforeTrain = player->GetFoodUsed();
    bool trainQueued = false;
    bool trainCompleted = false;
    int trainTicks = 0;
    bool uiTrainClicked = false;
    if (queueAbility && queueAbility->GetTrainedUnits().count(War3ID("hpea")) > 0) {
        uiTrainClicked = ui_.InvokeCommandOrderForDiagnostics((int)War3ID("hpea").value);
        trainTicks = pumpTicks(900, [&]() {
            trainQueued = trainQueued || queueAbility->GetQueueCount() > 0;
            trainCompleted = CountPlayerUnitsOfType(simulation_, playerIndex, War3ID("hpea")) > peasantsBeforeTrain;
            return trainCompleted;
        });
    }
    const int peasantsAfterTrain = CountPlayerUnitsOfType(simulation_, playerIndex, War3ID("hpea"));
    const bool trainPass = queueAbility
        && uiTrainClicked
        && trainQueued
        && trainCompleted
        && peasantsAfterTrain > peasantsBeforeTrain
        && player->GetGold() < goldBeforeTrain
        && player->GetFoodUsed() > foodBeforeTrain;
    std::cout << "GameplaySmoke: train=" << (trainPass ? "pass" : "fail")
              << " ticks=" << trainTicks
              << " queueAbility=" << (queueAbility ? "yes" : "no")
              << " uiClicked=" << (uiTrainClicked ? "yes" : "no")
              << " queued=" << (trainQueued ? "yes" : "no")
              << " peasantsBefore=" << peasantsBeforeTrain
              << " peasantsAfter=" << peasantsAfterTrain
              << " goldBefore=" << goldBeforeTrain
              << " goldAfter=" << player->GetGold()
              << " lumberBefore=" << lumberBeforeTrain
              << " lumberAfter=" << player->GetLumber()
              << " foodBefore=" << foodBeforeTrain
              << " foodAfter=" << player->GetFoodUsed()
              << std::endl;

    const War3ID farmId("hhou");
    float buildX = 0.0f;
    float buildY = 0.0f;
    const bool buildSiteFound = FindGameplaySmokeBuildSite(simulation_, dropoff, farmId, buildX, buildY);
    const auto* farmType = unitData_.GetUnitType(farmId);
    const float farmCollision = farmType ? std::max(0.0f, farmType->GetCollisionSize()) : 64.0f;
    const float workerCollision = std::max(0.0f, worker->GetCollisionSize());
    const float builderOffset = std::min(124.0f, std::max(96.0f, farmCollision + workerCollision + 16.0f));
    const int farmsBeforeBuild = CountPlayerUnitsOfType(simulation_, playerIndex, farmId);
    const int goldBeforeBuild = player->GetGold();
    const int lumberBeforeBuild = player->GetLumber();
    const int foodCapBeforeBuild = player->GetFoodCap();
    bool uiBuildMenuClicked = false;
    bool uiFarmEnabled = false;
    bool uiFarmClicked = false;
    bool uiFarmIssued = false;
    bool buildStarted = false;
    bool buildCompleted = false;
    int buildTicks = 0;
    int builtFarmHandle = -1;

    if (buildSiteFound) {
        worker->ClearOrders();
        worker->SetHidden(false);
        worker->SetInvulnerable(false);
        worker->SetPosition(buildX - builderOffset, buildY);
        SetSelectedUnits({worker}, worker->GetHandleId(), true);
        uiBuildMenuClicked = ui_.InvokeCommandOrderForDiagnostics(OrderIds::HUMAN_BUILD);
        uiFarmEnabled = ui_.IsCommandOrderEnabled((int)farmId.value);
        uiFarmClicked = ui_.InvokeCommandOrderForDiagnostics((int)farmId.value);
        uiFarmIssued = issuePendingFromUi(nullptr, buildX, buildY);
        buildTicks = pumpTicks(1400, [&]() {
            for (auto* unit : simulation_.GetUnits()) {
                if (!unit || unit->GetPlayerIndex() != playerIndex || unit->GetTypeId() != farmId) continue;
                if (unit->GetHandleId() == worker->GetHandleId()) continue;
                if (unit->GetHandleId() != builtFarmHandle && unit->GetHandleId() >= 0) {
                    builtFarmHandle = unit->GetHandleId();
                }
                if (unit->IsConstructing()) {
                    buildStarted = true;
                }
                if (!unit->IsConstructing() && unit->GetHandleId() == builtFarmHandle) {
                    buildCompleted = true;
                    return true;
                }
            }
            return false;
        });
    }

    const int farmsAfterBuild = CountPlayerUnitsOfType(simulation_, playerIndex, farmId);
    const bool buildPass = buildSiteFound
        && uiBuildMenuClicked
        && uiFarmEnabled
        && uiFarmClicked
        && uiFarmIssued
        && buildStarted
        && buildCompleted
        && farmsAfterBuild > farmsBeforeBuild
        && player->GetGold() < goldBeforeBuild
        && player->GetLumber() <= lumberBeforeBuild
        && player->GetFoodCap() > foodCapBeforeBuild;
    std::cout << "GameplaySmoke: build=" << (buildPass ? "pass" : "fail")
              << " ticks=" << buildTicks
              << " siteFound=" << (buildSiteFound ? "yes" : "no")
              << " uiBuildMenuClicked=" << (uiBuildMenuClicked ? "yes" : "no")
              << " farmButton=" << (uiFarmEnabled ? "enabled" : "disabled")
              << " uiFarmClicked=" << (uiFarmClicked ? "yes" : "no")
              << " uiIssued=" << (uiFarmIssued ? "yes" : "no")
              << " started=" << (buildStarted ? "yes" : "no")
              << " completed=" << (buildCompleted ? "yes" : "no")
              << " farmsBefore=" << farmsBeforeBuild
              << " farmsAfter=" << farmsAfterBuild
              << " goldBefore=" << goldBeforeBuild
              << " goldAfter=" << player->GetGold()
              << " lumberBefore=" << lumberBeforeBuild
              << " lumberAfter=" << player->GetLumber()
              << " foodCapBefore=" << foodCapBeforeBuild
              << " foodCapAfter=" << player->GetFoodCap()
              << " site=(" << buildX << "," << buildY << ")"
              << std::endl;

    const int enemyPlayerIndex = (playerIndex == 0) ? 1 : 0;
    CUnit* attackTarget = simulation_.CreateUnit(War3ID("hpea"), enemyPlayerIndex,
                                                 worker->GetX() + 96.0f, worker->GetY(), 180.0f);
    if (attackTarget) {
        simulation_.Update();
        attackTarget->SetLife(simulation_, 6.0f);
    }

    bool uiAttackEnabled = false;
    bool uiAttackClicked = false;
    bool uiAttackIssued = false;
    bool attackDamaged = false;
    bool attackKilled = false;
    bool deadSelectionCleared = false;
    int attackTicks = 0;
    const int attackTargetHandle = attackTarget ? attackTarget->GetHandleId() : -1;
    const float attackTargetLifeBefore = attackTarget ? attackTarget->GetLife() : 0.0f;
    if (attackTarget) {
        worker->ClearOrders();
        worker->SetHidden(false);
        worker->SetInvulnerable(false);
        worker->SetPosition(attackTarget->GetX() - 96.0f, attackTarget->GetY());
        SetSelectedUnits({worker}, worker->GetHandleId(), true);
        uiAttackEnabled = ui_.IsCommandOrderEnabled(OrderIds::ATTACK);
        uiAttackClicked = ui_.InvokeCommandOrderForDiagnostics(OrderIds::ATTACK);
        uiAttackIssued = issuePendingFromUi(attackTarget, attackTarget->GetX(), attackTarget->GetY());
        attackTicks = pumpTicks(240, [&]() {
            CUnit* target = simulation_.GetUnit(attackTargetHandle);
            if (!target) return false;
            attackDamaged = attackDamaged || target->GetLife() < attackTargetLifeBefore;
            attackKilled = target->IsDead();
            return attackKilled;
        });
        CUnit* target = simulation_.GetUnit(attackTargetHandle);
        if (target && target->IsDead()) {
            SetSelectedUnits({target}, target->GetHandleId(), true);
            deadSelectionCleared = selectedUnitHandles_.empty() && selectedUnitHandle_ < 0;
        }
    }

    const bool attackPass = attackTarget
        && uiAttackEnabled
        && uiAttackClicked
        && uiAttackIssued
        && attackDamaged
        && attackKilled
        && deadSelectionCleared;
    std::cout << "GameplaySmoke: attack=" << (attackPass ? "pass" : "fail")
              << " ticks=" << attackTicks
              << " target=" << (attackTarget ? "yes" : "no")
              << " uiAttackButton=" << (uiAttackEnabled ? "enabled" : "disabled")
              << " uiClicked=" << (uiAttackClicked ? "yes" : "no")
              << " uiIssued=" << (uiAttackIssued ? "yes" : "no")
              << " damaged=" << (attackDamaged ? "yes" : "no")
              << " killed=" << (attackKilled ? "yes" : "no")
              << " deadSelectionCleared=" << (deadSelectionCleared ? "yes" : "no")
              << " targetHandle=" << attackTargetHandle
              << " lifeBefore=" << attackTargetLifeBefore
              << " lifeAfter=" << (attackTarget ? attackTarget->GetLife() : 0.0f)
              << std::endl;

    const War3ID dynamicAbilityId("AHds");
    const bool dynamicAbsentBefore = worker->GetAbility(dynamicAbilityId) == nullptr;
    bool dynamicAdded = false;
    bool dynamicLevelSet = false;
    bool dynamicRemoved = false;
    if (dynamicAbsentBefore) {
        auto ability = AbilityFactory::CreateAbility(simulation_.AllocateHandleId(), dynamicAbilityId);
        if (ability) {
            worker->AddAbility(simulation_, std::move(ability));
            if (auto* added = worker->GetAbility(dynamicAbilityId)) {
                dynamicAdded = true;
                added->SetLevel(2);
                dynamicLevelSet = added->GetLevel() == 2;
            }
            dynamicRemoved = worker->RemoveAbility(simulation_, dynamicAbilityId);
        }
    }
    const bool dynamicAbilityPass = dynamicAbsentBefore && dynamicAdded && dynamicLevelSet && dynamicRemoved
        && worker->GetAbility(dynamicAbilityId) == nullptr;
    std::cout << "GameplaySmoke: abilityDynamic=" << (dynamicAbilityPass ? "pass" : "fail")
              << " ability=" << dynamicAbilityId.ToString()
              << " absentBefore=" << (dynamicAbsentBefore ? "yes" : "no")
              << " added=" << (dynamicAdded ? "yes" : "no")
              << " levelSet=" << (dynamicLevelSet ? "yes" : "no")
              << " removed=" << (dynamicRemoved ? "yes" : "no")
              << std::endl;

    std::cout << "GameplaySmoke: ui="
              << ((uiHarvestInitial && uiReturnAfterHarvest && uiTrainBeforeQueue && uiAttackEnabled) ? "pass" : "fail")
              << " harvestButton=" << (uiHarvestInitial ? "enabled" : "disabled")
              << " returnButton=" << (uiReturnAfterHarvest ? "enabled" : "disabled")
              << " trainPeasantButton=" << (uiTrainBeforeQueue ? "enabled" : "disabled")
              << " attackButton=" << (uiAttackEnabled ? "enabled" : "disabled")
              << std::endl;

    std::cout << "GameplaySmoke: result="
              << ((movePass && harvestPass && returnPass && trainPass
                   && buildPass
                   && attackPass
                   && dynamicAbilityPass
                   && uiHarvestInitial && uiReturnAfterHarvest && uiTrainBeforeQueue) ? "pass" : "fail")
              << " worker=" << unit_text_utils::DescribeUnitType(worker->GetTypeId(), unitData_)
              << " workerHandle=" << worker->GetHandleId()
              << " mineHandle=" << mine->GetHandleId()
              << " dropoffHandle=" << dropoff->GetHandleId()
              << std::endl;
}

static std::vector<MultiSelectUnitInfo> BuildMultiSelectionInfo(const std::vector<CUnit*>& units) {
    static const uint32_t pc[12] = {0xFFFF0000,0xFF0042FF,0xFF1CE6B6,0xFF540081,0xFFFFFC00,0xFFFE8A0E,
                                    0xFF20C000,0xFFE55BB0,0xFF959697,0xFF7EBFF1,0xFF106246,0xFF4F2B05};
    std::vector<MultiSelectUnitInfo> info; info.reserve(units.size());
    for (auto* u : units) {
        if (!u) continue;
        MultiSelectUnitInfo item; item.handleId = u->GetHandleId();
        if (auto* ut = u->GetUnitType()) { item.name = ut->GetName(); item.level = ut->GetLevel(); item.isHero = ut->IsHero(); }
        item.hpRatio = u->GetMaxLife() > 0 ? u->GetLife() / u->GetMaxLife() : 1.0f;
        item.mpRatio = u->GetMaxMana() > 0 ? u->GetMana() / (float)u->GetMaxMana() : 0.0f;
        int pi = u->GetPlayerIndex();
        if (pi >= 0 && pi < 12) item.playerColor = pc[pi];
        info.push_back(std::move(item));
    }
    return info;
}

void GameController::RefreshSelectionUi(bool rebuildCommandCard) {
    auto sel = GetSelectedSimulationUnits();
    selectedUnitHandles_.clear(); selectedUnitHandlesSet_.clear();
    for (auto* u : sel) { selectedUnitHandles_.push_back(u->GetHandleId()); selectedUnitHandlesSet_.insert(u->GetHandleId()); }

    if (sel.empty()) {
        selectedUnitHandle_ = -1; selectedUnitTypeId_ = War3ID();
        ui_.SetSelectedUnit(-1); ui_.SetMultiSelection({}); ui_.ClearInfoPanel();
        ui_.HideHeroPanel(); ui_.ClearPendingCommand(); return;
    }

    CUnit* primary = nullptr;
    for (auto* u : sel) if (u->GetHandleId() == selectedUnitHandle_) { primary = u; break; }
    if (!primary) primary = sel.front();
    selectedUnitHandle_ = primary->GetHandleId();
    selectedUnitTypeId_ = primary->GetTypeId();
    ui_.SetSelectedUnit(selectedUnitHandle_);
    (void)rebuildCommandCard;
    ui_.SetUnitData(&unitData_);
    ui_.PopulateCommandCardForUnit(primary);
    ui_.SetMultiSelection(sel.size() > 1 ? BuildMultiSelectionInfo(sel) : std::vector<MultiSelectUnitInfo>{});

    auto* ut = primary->GetUnitType();
    std::string uname = ut ? ut->GetName() : "Unknown";
    int ulvl = ut ? ut->GetLevel() : 1;
    bool isHero = ut && ut->IsHero();
    float hp = primary->GetLife(), maxHp = primary->GetMaxLife();
    float mp = primary->GetMana(), maxMp = (float)primary->GetMaxMana();
    if (isHero) {
        ui_.ShowHeroPanel(uname, std::max(ulvl, 1), 0, 100,
                          ut->GetStartingStrength(), 0, ut->GetStartingAgility(), 0, ut->GetStartingIntelligence(), 0);
    } else {
        ui_.HideHeroPanel();
        ui_.UpdateInfoPanel(selectedUnitTypeId_, uname, ulvl, hp, maxHp, mp, maxMp);
    }
}

// --- Picking ---

CUnit* GameController::PickSimulationUnitAtScreenPoint(float sx, float sy, const Mat4& view, const Mat4& proj,
                                                        int screenW, int screenH, int ignoreHandle,
                                                        bool preferBuildings, bool strictScreenTargeting) {
    picking::PickRay ray = picking::BuildScreenPickRay(sx, sy, view, proj, screenW, screenH);

    CUnit* bestScreen = nullptr, *bestScreenNonBuilding = nullptr, *bestRay = nullptr, *bestRayNonBuilding = nullptr;
    float bestScreenScore = std::numeric_limits<float>::infinity();
    float bestScreenNonBuildingScore = std::numeric_limits<float>::infinity();
    float bestScreenDistSq = std::numeric_limits<float>::infinity();
    float bestScreenNonBuildingDistSq = std::numeric_limits<float>::infinity();
    float bestRayT = std::numeric_limits<float>::infinity();
    float bestRayNonBuildingT = std::numeric_limits<float>::infinity();

    auto terrainH = [&](float x, float z) {
        return texturedTerrain_.IsValid() ? texturedTerrain_.GetGroundHeight(x, z) : 0.0f;
    };

    for (auto* unit : simulation_.GetUnits()) {
        if (!unit || unit->IsDead() || unit->IsHidden()) continue;
        if (ignoreHandle >= 0 && unit->GetHandleId() == ignoreHandle) continue;

        float sphereRadius = unit_selection_geom::ComputeUnitSelectionWorldRadius(*unit);
        Vec3 sphereCenter(unit->GetX(),
                          unit_selection_geom::ComputeUnitVisualHeight(*unit, texturedTerrain_.IsValid(), terrainH)
                              + unit_selection_geom::ComputeUnitSelectionVisualHeight(*unit) * 0.5f,
                          unit->GetY());

        bool visualHasCollisionShapes = false;
        bool visualCollisionHit = false;
        float visualCollisionT = 0.0f;
        if (auto vit = simUnitToVisualIdx.find(unit->GetHandleId()); vit != simUnitToVisualIdx.end()) {
            if (MdxViewer* v = scene_.GetViewer(vit->second)) {
                visualHasCollisionShapes = v->HasCollisionShapes();
                visualCollisionHit = scene_.IntersectInstanceRayWithCollision(vit->second, ray, visualCollisionT);
                sphereCenter = Vec3(unit->GetX() + v->GetBoundsCenter().x,
                                    unit_selection_geom::ComputeUnitVisualHeight(*unit, texturedTerrain_.IsValid(), terrainH) + v->GetBoundsCenter().y,
                                    unit->GetY() + v->GetBoundsCenter().z);
                float vr = v->GetBoundsRadius();
                if (vr > 1.0f) sphereRadius = std::max(sphereRadius, vr);
            }
        }
        sphereRadius = std::max(sphereRadius, unit->IsBuilding() ? 72.0f : 28.0f);

        float footX, footY, headX, headY, rpx;
        bool hasScreenShape = unit_selection_geom::ComputeUnitSelectionScreenShape(
            *unit, view, proj, screenW, screenH, texturedTerrain_.IsValid(), terrainH,
            footX, footY, headX, headY, rpx);

        float hitT;
        if (visualCollisionHit) {
            if (visualCollisionT < bestRayT) { bestRayT = visualCollisionT; bestRay = unit; }
            if (!unit->IsBuilding() && visualCollisionT < bestRayNonBuildingT) { bestRayNonBuildingT = visualCollisionT; bestRayNonBuilding = unit; }
        }
        else if (!visualHasCollisionShapes && picking::IntersectSphere(ray, sphereCenter, sphereRadius, hitT)) {
            if (hitT < bestRayT) { bestRayT = hitT; bestRay = unit; }
            if (!unit->IsBuilding() && hitT < bestRayNonBuildingT) { bestRayNonBuildingT = hitT; bestRayNonBuilding = unit; }
        }

        if (!hasScreenShape) continue;

        float distSq = picking::DistanceToSegmentSquared(sx, sy, footX, footY, headX, headY);
        float hitRPx = strictScreenTargeting ? rpx * (unit->IsBuilding() ? 0.6f : 0.75f) : rpx;
        if (distSq > hitRPx * hitRPx) continue;

        float score = distSq / std::max(1.0f, hitRPx * hitRPx);
        if (preferBuildings && unit->IsBuilding()) score = std::max(0.0f, score - 0.05f);

        if (score < bestScreenScore || (std::abs(score - bestScreenScore) <= 0.03f
            && (preferBuildings && unit->IsBuilding() && bestScreen && !bestScreen->IsBuilding()))) {
            bestScreenScore = score; bestScreenDistSq = distSq; bestScreen = unit;
        }
        if (!unit->IsBuilding() && score < bestScreenNonBuildingScore) {
            bestScreenNonBuildingScore = score; bestScreenNonBuildingDistSq = distSq; bestScreenNonBuilding = unit;
        }
    }

    if (!preferBuildings && bestScreenNonBuilding) return bestScreenNonBuilding;
    if (bestScreen) return bestScreen;
    if (strictScreenTargeting) return nullptr;
    if (!preferBuildings && bestRayNonBuilding) return bestRayNonBuilding;
    return bestRay;
}

CDestructable* GameController::PickSimulationDestructableAtScreenPoint(float sx, float sy, const Mat4& view,
                                                                        const Mat4& proj, int screenW, int screenH) {
    if (!simulationInitialized_) return nullptr;
    CDestructable *bestScreen = nullptr;
    float bestScreenScore = std::numeric_limits<float>::infinity();

    for (auto* d : simulation_.GetDestructables()) {
        if (!d || d->IsDead()) continue;
        float worldRadius = 56.0f, worldHeight = 140.0f;
        if (auto* dt = destructableData_.GetDestructable(d->GetTypeId())) {
            if (dt->isTree) { worldRadius = 72.0f; worldHeight = 180.0f; }
            else if (dt->isWall) { worldRadius = 64.0f; worldHeight = 96.0f; }
        }
        float gh = texturedTerrain_.IsValid() ? texturedTerrain_.GetGroundHeight(d->GetX(), d->GetY()) : 0.0f;
        Vec3 sc(d->GetX(), gh + worldHeight * 0.5f, d->GetY());
        float cx, cy;
        if (!picking::WorldToScreen(sc, view, proj, screenW, screenH, cx, cy)) continue;
        float rpx = 0;
        auto upR = [&](const Vec3& p) { float sx, sy; if (picking::WorldToScreen(p, view, proj, screenW, screenH, sx, sy)) { float dx=sx-cx, dy=sy-cy; rpx=std::max(rpx,std::sqrt(dx*dx+dy*dy)); } };
        upR(Vec3(d->GetX()+worldRadius, gh + worldHeight*0.5f, d->GetY()));
        upR(Vec3(d->GetX(), gh + worldHeight*0.5f, d->GetY()+worldRadius));
        rpx = std::max(rpx, 20.0f);
        float dx = sx - cx, dy = sy - cy, distSq = dx*dx+dy*dy;
        if (distSq > rpx*rpx) continue;
        float score = distSq / std::max(1.0f, rpx*rpx);
        if (score < bestScreenScore) { bestScreenScore = score; bestScreen = d; }
    }
    return bestScreen;
}

// --- Shutdown ---

void GameController::Shutdown() {
    ui_.Shutdown();
    Renderer::Shutdown();
    audio_.Shutdown();
    std::cout << "GameController shut down after " << frameCount_ << " frames." << std::endl;
}
