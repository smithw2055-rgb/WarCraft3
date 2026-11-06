#pragma once
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
#include "third_party/sokol/sokol_app.h"
#include "third_party/sokol/sokol_gfx.h"
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

inline const char* DEFAULT_MAP = "Maps/FrozenThrone/(4)Avalanche.w3x";

inline std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    return value;
}

inline std::string TrimUiOnlyMapString(std::string value) {
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

inline int UiOnlyPlayerSlotsFromFileName(const std::string& fileName) {
    if (fileName.size() <= 3 || fileName[0] != '(') return 0;
    const size_t close = fileName.find(')');
    if (close == std::string::npos || close <= 1) return 0;
    return std::max(0, std::atoi(fileName.substr(1, close - 1).c_str()));
}

inline const char* UiOnlyTilesetName(char tileset) {
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

inline std::string UiOnlyMapSizeLabel(int width, int height) {
    if (width <= 0 || height <= 0) return {};
    return std::to_string(width) + " x " + std::to_string(height);
}

inline std::string UiOnlySuggestedPlayersLabel(const std::string& value) {
    std::string cleaned = TrimUiOnlyMapString(value);
    if (cleaned.empty()) return {};
    const std::string lower = LowerAscii(cleaned);
    if (lower.rfind("suggested", 0) == 0 || lower.rfind("players", 0) == 0) {
        return cleaned;
    }
    return "Suggested Players: " + cleaned;
}

inline bool ReadUiOnlyMapArchiveFile(const fs::path& mapPath, const char* fileName, std::vector<uint8_t>& out) {
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

inline std::string ResolveUiOnlyTrigString(const WtsTable* wts, const std::string& value) {
    if (!wts || value.empty()) return TrimUiOnlyMapString(value);
    return TrimUiOnlyMapString(wts->Resolve(value));
}

inline bool IsUiOnlyTrigStringRef(const std::string& value) {
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

inline std::string CleanUiOnlyResolvedMapString(const std::string& value) {
    std::string cleaned = TrimUiOnlyMapString(value);
    return IsUiOnlyTrigStringRef(cleaned) ? std::string() : cleaned;
}

inline void PopulateUiOnlyGlueMapInfo(const fs::path& mapPath, War3UI::GlueMapListItem& item) {
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

inline std::vector<War3UI::GlueMapListItem> DiscoverUiOnlyGlueMaps(const fs::path& projectRoot) {
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

inline std::string StripUiOnlyCampaignToken(std::string value) {
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

inline std::vector<std::string> SplitUiOnlyCampaignCsv(const std::string& value) {
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

inline std::string StripUiOnlySkinValue(std::string value) {
    const size_t comment = value.find("//");
    if (comment != std::string::npos) {
        value.resize(comment);
    }
    return StripUiOnlyCampaignToken(value);
}

inline std::string ResolveUiOnlySkinField(const MpqManager& mpq,
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

inline void AddUiOnlyCandidatePath(std::vector<std::string>& paths, const std::string& path) {
    if (path.empty()) {
        return;
    }
    if (std::find(paths.begin(), paths.end(), path) == paths.end()) {
        paths.push_back(path);
    }
}

inline std::string UiOnlyMdxPathFromModelField(std::string path) {
    path = StripUiOnlySkinValue(path);
    const std::string lower = LowerAscii(path);
    if (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".mdl") {
        path.resize(path.size() - 4);
        path += ".mdx";
    }
    return path;
}

inline Vec3 UiOnlyMdxPointToYUp(const float value[3]) {
    return Vec3(value[0], value[2], -value[1]);
}

inline int UiOnlySelectStandSequenceIndex(const MdxModel& model) {
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

inline std::vector<War3UI::GlueCampaignListItem> LoadUiOnlyGlueCampaigns(const MpqManager& mpq,
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

inline ScreenRect FallbackMinimapRect(int screenW, int screenH) {
    if (screenW <= 0 || screenH <= 0) return {};
    const float w = std::clamp((float)screenW * 0.105f, 190.0f, 360.0f);
    const float h = std::clamp((float)screenH * 0.38f, 118.0f, 220.0f);
    return ScreenRect{20.0f, (float)screenH - h - 24.0f, w, h};
}

inline Vec4 MdxEventSplatTint(const std::array<std::array<float, 4>, 3>& colors,
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

inline float MdxEventSplatCell(const std::array<std::array<float, 3>, 2>& uvIntervals,
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

inline Vec4 MdxEventSplatUvRect(const std::array<std::array<float, 3>, 2>& uvIntervals,
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

inline SplatModel::BlendMode MdxEventSplatBlendMode(int blendMode) {
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

inline bool CurrentAnimationRequestMatchesViewer(MdxViewer* viewer,
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

inline bool ApplyAnimationRequestToViewer(int handleId,
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

inline void ApplyPendingUnitAnimation(int handleId, MdxViewer* viewer, GameController* g) {
    if (!viewer || !viewer->GetAnimator()) return;
    const auto pendingIt = g->simUnitPendingAnimations.find(handleId);
    if (pendingIt == g->simUnitPendingAnimations.end()) return;
    ApplyAnimationRequestToViewer(handleId, viewer, pendingIt->second, g);
    g->simUnitPendingAnimations.erase(pendingIt);
}

inline void QueueUnitAnimationRequest(int handleId,
                                      const animation_utils::PendingUnitAnimation& request,
                                      GameController* g) {
    if (!g) return;
    g->simUnitQueuedAnimations[handleId].push_back(request);
}

inline bool ApplyNextQueuedUnitAnimation(int handleId, MdxViewer* viewer, GameController* g) {
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



inline void AppendCommaSeparatedArgs(std::vector<std::string>& out, const char* value) {
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

inline bool ParseDebugUiResources(const char* value, int& gold, int& lumber, int& foodUsed, int& foodCap, std::string& timeText) {
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

inline void AddJassErrorStats(const std::vector<std::string>& errors,
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

inline void PrintTopJassUnknowns(const std::unordered_map<std::string, int>& unknownFunctions) {
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

inline void PrintTopJassParserErrors(const std::unordered_map<std::string, int>& parserErrorCounts) {
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

inline std::string JassDiagnosticCategory(const std::string& name) {
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

inline void PrintTopJassStaticEntries(const char* prefix,
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

inline void PrintJassStaticDiagnosticsForVm(const char* phase, const JassVM& vm) {
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

inline void PlayResolvedUnitAnimation(CUnit* unit,
                               MdxViewer* viewer,
                               GameController* g,
                               animation_utils::UnitVisualAnimationMode mode) {
    if (!unit || !viewer) return;

    const int handleId = unit->GetHandleId();
    auto& splatMeta = g->GetSplatMeta();
    const auto* scriptTags = &g->simUnitAnimationPropertyTags;

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

    const CBehaviorCategory behaviorCategory =
        unit_text_utils::GetUnitCurrentBehaviorCategory(unit);
    const CResourceType carriedType =
        unit_text_utils::GetUnitCarriedResourceType(unit);

    switch (mode) {
        case animation_utils::UnitVisualAnimationMode::DEAD:
        case animation_utils::UnitVisualAnimationMode::CORPSE:
        case animation_utils::UnitVisualAnimationMode::BONE_CORPSE:
        case animation_utils::UnitVisualAnimationMode::DISSIPATE:
            animation_utils::PlayDeathVisualForUnit(unit, viewer, mode, g->GetSimulation());
            return;
        case animation_utils::UnitVisualAnimationMode::CONSTRUCT: {
            const auto lastModeIt = g->simUnitVisualAnimationModes.find(handleId);
            if (lastModeIt == g->simUnitVisualAnimationModes.end()
                || lastModeIt->second != animation_utils::UnitVisualAnimationMode::CONSTRUCT) {
                viewer->PlayBirthThenStandSequence();
            }
            float buildRatio = 0.0f;
            if (const auto* unitType = unit->GetUnitType()) {
                const float buildTime = static_cast<float>(unitType->GetBuildTime());
                if (buildTime > 0.0f) {
                    buildRatio = unit->GetConstructionProgress() / buildTime;
                }
            }
            viewer->SetCurrentSequenceFrameByRatio(std::clamp(buildRatio, 0.0f, 1.0f));
            return;
        }
        case animation_utils::UnitVisualAnimationMode::READY:
            animation_utils::PlayReadyVisualForUnit(unit, viewer, behaviorCategory,
                CResourceType::NONE, splatMeta, scriptTags);
            return;
        case animation_utils::UnitVisualAnimationMode::READY_GOLD:
            animation_utils::PlayReadyVisualForUnit(unit, viewer, behaviorCategory,
                CResourceType::GOLD, splatMeta, scriptTags);
            return;
        case animation_utils::UnitVisualAnimationMode::READY_LUMBER:
            animation_utils::PlayReadyVisualForUnit(unit, viewer, behaviorCategory,
                CResourceType::LUMBER, splatMeta, scriptTags);
            return;
        case animation_utils::UnitVisualAnimationMode::WALK:
            animation_utils::PlayWalkVisualForUnit(unit, viewer, behaviorCategory,
                CResourceType::NONE, splatMeta, scriptTags);
            return;
        case animation_utils::UnitVisualAnimationMode::WALK_GOLD:
            animation_utils::PlayWalkVisualForUnit(unit, viewer, behaviorCategory,
                CResourceType::GOLD, splatMeta, scriptTags);
            return;
        case animation_utils::UnitVisualAnimationMode::WALK_LUMBER:
            animation_utils::PlayWalkVisualForUnit(unit, viewer, behaviorCategory,
                CResourceType::LUMBER, splatMeta, scriptTags);
            return;
        case animation_utils::UnitVisualAnimationMode::WALK_WORK:
            animation_utils::PlayWalkWorkVisualForUnit(unit, viewer, behaviorCategory,
                carriedType, splatMeta, scriptTags);
            return;
        case animation_utils::UnitVisualAnimationMode::WORK:
            animation_utils::PlayWorkVisualForUnit(unit, viewer, behaviorCategory,
                carriedType, splatMeta, scriptTags);
            return;
        case animation_utils::UnitVisualAnimationMode::SPELL:
            if (!animation_utils::PlaySpellVisualForUnit(unit, viewer, scriptTags)) {
                animation_utils::PlayReadyVisualForUnit(unit, viewer, behaviorCategory,
                    carriedType, splatMeta, scriptTags);
            }
            return;
        case animation_utils::UnitVisualAnimationMode::ATTACK: {
            const int currentTick = g->GetSimulation().GetGameTurnTick();
            const auto* attackBehavior = animation_utils::GetAttackBehavior(unit);
            const int behaviorAttackStartTick =
                attackBehavior ? attackBehavior->GetAttackAnimationStartTick() : -1;
            const float behaviorAttackDuration =
                attackBehavior ? attackBehavior->GetAttackAnimationDuration() : 0.0f;
            const auto lastAttackIt = g->simUnitVisualLastAttackAnimationTicks.find(handleId);
            const int lastAttackTick = (lastAttackIt != g->simUnitVisualLastAttackAnimationTicks.end())
                ? lastAttackIt->second
                : std::numeric_limits<int>::min();
            const bool shouldStartAttackAnimation = attackBehavior
                && attackBehavior->IsAttackAnimationActive(currentTick)
                && behaviorAttackStartTick >= 0
                && behaviorAttackStartTick != lastAttackTick;
            if (shouldStartAttackAnimation) {
                const auto attackTags = animation_utils::BuildContextualAnimationTags(
                    unit, scriptTags, CResourceType::NONE);
                if (viewer->PlayAttackSequenceWithTagsAndDuration(
                        attackTags, behaviorAttackDuration)) {
                    g->simUnitVisualLastAttackAnimationTicks[handleId] =
                        behaviorAttackStartTick;
                }
                else {
                    animation_utils::PlayReadyVisualForUnit(unit, viewer, behaviorCategory,
                        carriedType, splatMeta, scriptTags);
                }
            }
            else if (!attackBehavior || !attackBehavior->IsAttackAnimationActive(currentTick)) {
                if (!viewer->IsPlayingSequenceWithToken("stand")
                    || viewer->IsCurrentSequenceFinished()) {
                    animation_utils::PlayReadyVisualForUnit(unit, viewer,
                        CBehaviorCategory::ATTACK, carriedType, splatMeta, scriptTags);
                }
            }
            return;
        }
        default:
            return;
    }
}

inline void SyncSimulationUnitAnimation(CUnit* unit, MdxViewer* viewer, GameController* g) {
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

    // Re-evaluate animation from live behavior, work, and carried-resource
    // state rather than only position changes.
    const bool isMoving = animation_utils::ComputeUnitVisualIsMoving(
        unit, g->simUnitVisualLastPositions);
    const bool wasMoving = g->simUnitVisualMovingStates.count(handleId)
        && g->simUnitVisualMovingStates[handleId];
    const CBehaviorCategory behaviorCategory =
        unit_text_utils::GetUnitCurrentBehaviorCategory(unit);
    const CResourceType carriedType =
        unit_text_utils::GetUnitCarriedResourceType(unit);
    const std::vector<std::string> currentPropertyTags =
        animation_utils::CollectUnitAnimationPropertyTags(
            unit, &g->simUnitAnimationPropertyTags);

    const auto lastModeIt = g->simUnitVisualAnimationModes.find(handleId);
    const auto lastMode = (lastModeIt != g->simUnitVisualAnimationModes.end())
        ? lastModeIt->second
        : animation_utils::UnitVisualAnimationMode::NONE;

    const auto lastPropertyTagsIt = g->simUnitVisualPropertyTags.find(handleId);
    const bool propertyTagsChanged = (lastPropertyTagsIt == g->simUnitVisualPropertyTags.end())
        || (lastPropertyTagsIt->second != currentPropertyTags);

    const auto lastResourceIt = g->simUnitVisualResourceTags.find(handleId);
    const bool resourceChanged = (lastResourceIt == g->simUnitVisualResourceTags.end())
        || (lastResourceIt->second != carriedType);

    const auto resolvedMode = animation_utils::ResolveUnitVisualAnimationMode(
        unit, isMoving, behaviorCategory, carriedType,
        g->GetSimulation().GetGameTurnTick());
    const bool shouldRefreshAttack =
        resolvedMode == animation_utils::UnitVisualAnimationMode::ATTACK
        && viewer->IsCurrentSequenceFinished();

    bool needsResolve = (isMoving != wasMoving)
        || (resolvedMode != lastMode)
        || propertyTagsChanged
        || resourceChanged
        || shouldRefreshAttack
        || (g->simUnitPendingAnimations.count(handleId) != 0);

    if (needsResolve) {
        PlayResolvedUnitAnimation(unit, viewer, g, resolvedMode);
    }

    g->simUnitVisualConstructionStates[handleId] = unit->IsConstructing();
    g->simUnitVisualResourceTags[handleId] = carriedType;
    g->simUnitVisualPropertyTags[handleId] = currentPropertyTags;
    g->simUnitVisualAnimationModes[handleId] = resolvedMode;
    g->simUnitVisualMovingStates[handleId] = isMoving;
    if (resolvedMode != animation_utils::UnitVisualAnimationMode::ATTACK) {
        g->simUnitVisualLastAttackAnimationTicks.erase(handleId);
    }

    // Handle queued animations
    const auto queuedIt = g->simUnitQueuedAnimations.find(handleId);
    if (queuedIt != g->simUnitQueuedAnimations.end() && !queuedIt->second.empty()) {
        if (!viewer->IsCurrentSequenceLooping() && viewer->IsCurrentSequenceFinished()) {
            ApplyNextQueuedUnitAnimation(handleId, viewer, g);
        }
    }

    // Sync interpolated position/rotation/scale/tint via SceneManager & RenderInterpolationBuffer
    render::RenderTransformSnapshot interpSnap;
    const bool hasInterp = g->GetPresentation().SampleInterpolatedUnit(handleId, interpSnap);
    const float sampleX = hasInterp ? interpSnap.position.x : unit->GetX();
    const float sampleY = hasInterp ? interpSnap.position.z : unit->GetY();
    const float sampleFacing = hasInterp ? interpSnap.facingDeg : unit->GetFacing();

    float visualHeight = unit_selection_geom::ComputeUnitVisualHeight(
        *unit, g->GetTexturedTerrain().IsValid(),
        [&](float x, float z) { return g->GetTexturedTerrain().GetGroundHeight(x, z); });
    if (hasInterp) {
        const float groundH = g->GetTexturedTerrain().IsValid()
            ? g->GetTexturedTerrain().GetGroundHeight(sampleX, sampleY)
            : 0.0f;
        visualHeight = groundH + interpSnap.flyHeight;
    }
    {
        float pos[3] = {sampleX, visualHeight, sampleY};
        auto vit = g->simUnitToVisualIdx.find(handleId);
        if (vit != g->simUnitToVisualIdx.end()) {
            g->GetScene().UpdateInstancePosition(vit->second, pos, sampleFacing);
            // Script scale (SetUnitScale) multiplies the seed scale; refresh
            // every frame so mid-game changes apply (cheap idempotent write).
            float seedScale[3] = {1.0f, 1.0f, 1.0f};
            auto seedIt = g->simUnitVisualSeeds.find(handleId);
            if (seedIt != g->simUnitVisualSeeds.end()) {
                seedScale[0] = seedIt->second.scale[0];
                seedScale[1] = seedIt->second.scale[1];
                seedScale[2] = seedIt->second.scale[2];
            }
            const float s = hasInterp ? interpSnap.scale.x : unit->GetScale();
            const float scale[3] = {seedScale[0] * s, seedScale[1] * s, seedScale[2] * s};
            g->GetScene().SetInstanceScale(vit->second, scale);

            int vr = 255, vg = 255, vb = 255, va = 255;
            unit->GetVertexColor(vr, vg, vb, va);
            g->GetScene().SetInstanceVertexColor(
                vit->second,
                hasInterp ? interpSnap.vertexColor.x : (vr / 255.0f),
                hasInterp ? interpSnap.vertexColor.y : (vg / 255.0f),
                hasInterp ? interpSnap.vertexColor.z : (vb / 255.0f),
                hasInterp ? interpSnap.vertexColor.w : (va / 255.0f)
            );
        }
    }
}

// ======================================================================
// GameController implementation

// --- Init helpers (melee start, etc.) ---

inline bool IsHumanTownHallType(War3ID typeId) {
    // Any-race town hall tier (ref melee starting town halls per race).
    return typeId == War3ID("htow") || typeId == War3ID("hkee") || typeId == War3ID("hcas")
        || typeId == War3ID("ogre") || typeId == War3ID("ofrt") || typeId == War3ID("ostr")
        || typeId == War3ID("unpl") || typeId == War3ID("unp1") || typeId == War3ID("unp2")
        || typeId == War3ID("etol") || typeId == War3ID("etoa") || typeId == War3ID("etoe");
}

inline int CountPlayerUnitsOfType(CSimulation& sim, int playerIndex, War3ID typeId) {
    int count = 0;
    for (auto* unit : sim.GetUnits())
        if (unit && unit->GetPlayerIndex() == playerIndex && unit->GetTypeId() == typeId) ++count;
    return count;
}

inline int CountPlayerTownHalls(CSimulation& sim, int playerIndex) {
    int count = 0;
    for (auto* unit : sim.GetUnits())
        if (unit && unit->GetPlayerIndex() == playerIndex && IsHumanTownHallType(unit->GetTypeId())) ++count;
    return count;
}

inline bool IsCampaignMapPath(const std::string& mapPath) {
    std::string path = LowerAscii(mapPath);
    std::replace(path.begin(), path.end(), '/', '\\');
    return path.find("\\maps\\campaign\\") != std::string::npos
        || path.find("\\maps\\frozenthrone\\campaign\\") != std::string::npos
        || path.find("warsmash_mpq_map_maps_campaign_") != std::string::npos
        || path.find("warsmash_mpq_map_maps_frozenthrone_campaign_") != std::string::npos
        || path.rfind("maps\\campaign\\", 0) == 0
        || path.rfind("maps\\frozenthrone\\campaign\\", 0) == 0;
}

inline bool FindPlayerTownHallPosition(CSimulation& sim, int playerIndex, float& outX, float& outY) {
    for (auto* unit : sim.GetUnits())
        if (unit && unit->GetPlayerIndex() == playerIndex && IsHumanTownHallType(unit->GetTypeId())) {
            outX = unit->GetX(); outY = unit->GetY(); return true;
        }
    return false;
}

inline CUnit* FindGameplaySmokeWorker(CSimulation& sim, int playerIndex) {
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

inline CUnit* FindNearestGoldMine(CSimulation& sim, const CUnit* worker) {
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

inline CUnit* FindPlayerDropoff(CSimulation& sim, int playerIndex) {
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

inline bool FindGameplaySmokeBuildSite(CSimulation& sim, const CUnit* anchor, War3ID buildingTypeId,
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
        const CUnitType* buildType =
            sim.GetUnitData() ? sim.GetUnitData()->GetUnitType(buildingTypeId) : nullptr;
        CAbilityBuild::RoundBuildPositionForType(buildType, x, y);
        if (!CAbilityBuild::IsBuildLocationObstructed(sim, x, y, anchor->GetPlayerIndex(), buildingTypeId)) {
            outX = x;
            outY = y;
            return true;
        }
    }
    return false;
}

inline bool FindPlayerStartLocationPosition(const JassNativesData* data, int playerIndex, float& outX, float& outY) {
    if (!data) return false;
    if (playerIndex < 0 || playerIndex >= (int)data->playerMapConfig.size()) return false;
    const auto& pc = data->playerMapConfig[playerIndex];
    const int si = pc.startLocationIndex;
    if (si < 0 || si >= (int)data->startLocations.size()) return false;
    const auto& start = data->startLocations[si];
    if (!start.defined) return false;
    outX = start.x; outY = start.y; return true;
}

inline bool FindInitialSpawnFocusPosition(CSimulation& sim, GameController* g, float& outX, float& outY) {
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


static War3ID RaceTownHallId(int race) {
    switch (race) {
        case 2: return War3ID("ogre");
        case 3: return War3ID("unpl");
        case 4: return War3ID("etol");
        default: return War3ID("htow");
    }
}

inline War3ID RaceWorkerId(int race) {
    switch (race) {
        case 2: return War3ID("opeo");
        case 3: return War3ID("uaco");
        case 4: return War3ID("ewsp");
        default: return War3ID("hpea");
    }
}


// --- Selection helpers ---

inline bool RectanglesOverlap(const ScreenRect& a, const ScreenRect& b) {
    if (!a.IsValid() || !b.IsValid()) return false;
    return !(a.x + a.w < b.x || b.x + b.w < a.x || a.y + a.h < b.y || b.y + b.h < a.y);
}

inline bool ComputeUnitSelectionScreenBounds(const CUnit& unit, const Mat4& view, const Mat4& proj,
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

inline std::vector<CUnit*> CollectUnitsInScreenRect(const ScreenRect& rect, const Mat4& view, const Mat4& proj,
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

