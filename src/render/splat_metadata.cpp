#include "splat_metadata.h"
#include "core/path_utils.h"
#include "render/unit_selection_geom.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <unordered_map>

namespace {

bool ReadMpq(MpqManager& mpq, const std::string& path, std::vector<uint8_t>& out) {
    return mpq.ReadFile(path, out);
}

bool ReadMpqAny(MpqManager& mpq, const std::initializer_list<const char*>& paths, std::vector<uint8_t>& out) {
    for (const char* path : paths) {
        if (mpq.ReadFile(path, out)) {
            return true;
        }
    }
    return false;
}

} // namespace

void SplatMetadataStore::Load(MpqManager& mpq) {
    unitRenderSplatMeta.clear();
    selectionCircleVisuals.clear();
    selectionCircleScaleFactor = 1.0f;

    if (mpq.ArchiveCount() <= 0) {
        return;
    }

    SlkTable unitUiSlk;
    SlkTable uberSplatSlk;
    TxtTable miscData;
    std::vector<uint8_t> buf;

    if (ReadMpqAny(mpq, {"Units\\UnitUI.slk", "Units\\unitUI.slk"}, buf)) {
        unitUiSlk.Load(buf.data(), buf.size());
    }
    buf.clear();
    if (ReadMpq(mpq, "Splats\\UberSplatData.slk", buf)) {
        uberSplatSlk.Load(buf.data(), buf.size());
    }

    for (const char* miscPath : {
             "UI\\MiscData.txt",
             "Units\\MiscData.txt",
             "Units\\MiscGame.txt",
             "UI\\MiscUI.txt",
             "war3mapMisc.txt"
         }) {
        buf.clear();
        if (ReadMpq(mpq, miscPath, buf)) {
            miscData.Load(buf.data(), buf.size());
        }
    }

    std::unordered_map<std::string, std::pair<std::string, float>> uberSplatLookup;
    if (uberSplatSlk.Rows() > 0) {
        int idCol = uberSplatSlk.FindColumn("UberSplatID");
        if (idCol < 0) idCol = uberSplatSlk.FindColumn("splatID");
        if (idCol < 0) idCol = 0;
        const int dirCol = uberSplatSlk.FindColumn("Dir");
        const int fileCol = uberSplatSlk.FindColumn("file");
        const int scaleCol = uberSplatSlk.FindColumn("Scale");
        for (int row = 0; row < uberSplatSlk.Rows(); ++row) {
            const std::string id = path_utils::TrimMetadataValue(uberSplatSlk.GetString(row, idCol));
            if (id.empty()) {
                continue;
            }
            std::string path = path_utils::NormalizeMetadataPath(uberSplatSlk.GetString(row, dirCol) + "\\" +
                                                                 uberSplatSlk.GetString(row, fileCol));
            if (dirCol < 0 || fileCol < 0) {
                path.clear();
            }
            const float scale = scaleCol >= 0 ? std::strtof(uberSplatSlk.GetString(row, scaleCol).c_str(), nullptr) : 0.0f;
            uberSplatLookup[id] = {path, scale};
        }
    }

    if (unitUiSlk.Rows() > 0) {
        int idCol = unitUiSlk.FindColumn("unitUIID");
        if (idCol < 0) idCol = unitUiSlk.FindColumn("unitID");
        if (idCol < 0) idCol = 0;
        const int scaleCol = unitUiSlk.FindColumn("scale");
        const int selZCol = unitUiSlk.FindColumn("selZ");
        const int walkSpeedCol = unitUiSlk.FindColumn("walk");
        const int runSpeedCol = unitUiSlk.FindColumn("run");
        const int uberCol = unitUiSlk.FindColumn("uberSplat");
        const int buildingShadowCol = unitUiSlk.FindColumn("buildingShadow");
        for (int row = 0; row < unitUiSlk.Rows(); ++row) {
            const std::string id = path_utils::TrimMetadataValue(unitUiSlk.GetString(row, idCol));
            if (id.size() != 4) {
                continue;
            }

            auto& meta = unitRenderSplatMeta[War3ID::FromString(id).value];
            meta.selectionScale = scaleCol >= 0 ? std::strtof(unitUiSlk.GetString(row, scaleCol).c_str(), nullptr) : 0.0f;
            meta.selectionHeight = selZCol >= 0 ? std::strtof(unitUiSlk.GetString(row, selZCol).c_str(), nullptr) : 0.0f;
            meta.animationWalkSpeed = walkSpeedCol >= 0 ? std::strtof(unitUiSlk.GetString(row, walkSpeedCol).c_str(), nullptr) : 0.0f;
            meta.animationRunSpeed = runSpeedCol >= 0 ? std::strtof(unitUiSlk.GetString(row, runSpeedCol).c_str(), nullptr) : 0.0f;
            meta.buildingShadow = buildingShadowCol >= 0 ? unitUiSlk.GetString(row, buildingShadowCol) : std::string();

            if (id == "hpea" || id == "pea1" || id == "aeph") {
                const int animPropsCol = unitUiSlk.FindColumn("animProps");
                std::cout << "WalkDiag[UnitUI]: id=" << id
                          << " walk=" << meta.animationWalkSpeed
                          << " run=" << meta.animationRunSpeed
                          << " animProps=" << (animPropsCol >= 0 ? unitUiSlk.GetString(row, animPropsCol) : "")
                          << std::endl;
            }

            const std::string uberId = uberCol >= 0 ? path_utils::TrimMetadataValue(unitUiSlk.GetString(row, uberCol)) : std::string();
            if (!uberId.empty()) {
                auto uberIt = uberSplatLookup.find(uberId);
                if (uberIt != uberSplatLookup.end()) {
                    meta.uberSplatPath = uberIt->second.first;
                    meta.uberSplatScale = uberIt->second.second;
                }
            }
        }
    }

    const TxtTable& misc = miscData;
    const int numSizes = misc.GetInt("SelectionCircle", "NumSizes", 0);
    selectionCircleScaleFactor = misc.GetFloat("SelectionCircle", "ScaleFactor", 1.0f);
    for (int i = 0; i < numSizes; ++i) {
        char index[3] = {};
        std::snprintf(index, sizeof(index), "%02d", i);
        SelectionCircleVisual visual;
        visual.size = misc.GetFloat("SelectionCircle", std::string("Size") + index, 0.0f);
        visual.texturePath = path_utils::NormalizeMetadataPath(misc.GetString("SelectionCircle", std::string("Texture") + index));
        visual.dottedTexturePath = path_utils::NormalizeMetadataPath(
            misc.GetString("SelectionCircle", std::string("TextureDotted") + index));
        if (!visual.texturePath.empty()) {
            selectionCircleVisuals.push_back(std::move(visual));
        }
    }
    std::sort(selectionCircleVisuals.begin(), selectionCircleVisuals.end(),
              [](const auto& a, const auto& b) { return a.size < b.size; });

    std::cout << "UnitRenderMetadata: units=" << unitRenderSplatMeta.size()
              << " selectionCircles=" << selectionCircleVisuals.size()
              << " scaleFactor=" << selectionCircleScaleFactor
              << std::endl;
}

const SplatMetadataStore::UnitRenderSplatMeta* SplatMetadataStore::GetUnitRenderSplatMeta(const CUnit& unit) const {
    const auto it = unitRenderSplatMeta.find(unit.GetTypeId().value);
    return (it != unitRenderSplatMeta.end()) ? &it->second : nullptr;
}

float SplatMetadataStore::ComputeSelectionCircleSizeWorld(
    const CUnit& unit,
    const std::function<float(const CUnit&)>& fallbackRadiusWorld) const {
    if (const auto* meta = GetUnitRenderSplatMeta(unit)) {
        if (meta->selectionScale > 0.0f) {
            const float scaleFactor = selectionCircleScaleFactor > 0.0f ? selectionCircleScaleFactor : 1.0f;
            return meta->selectionScale * scaleFactor;
        }
    }
    return fallbackRadiusWorld(unit) * 2.0f;
}

std::string SplatMetadataStore::PickSelectionCircleTexturePath(float selectionSizeWorld) const {
    if (selectionCircleVisuals.empty()) {
        return {};
    }
    for (const auto& visual : selectionCircleVisuals) {
        if (selectionSizeWorld < visual.size) {
            return visual.texturePath;
        }
    }
    return selectionCircleVisuals.back().texturePath;
}

void SplatMetadataStore::RefreshRuntimeSplats(TexturedTerrain& terrain,
                                             bool simulationInitialized,
                                             CSimulation& simulation,
                                             const std::unordered_set<int>& selectedHandles) const {
    if (!terrain.IsValid()) {
        return;
    }

    terrain.ClearRuntimeSplats();
    if (!simulationInitialized) {
        return;
    }

    for (auto* unit : simulation.GetUnits()) {
        if (!unit || unit->IsDead() || unit->IsHidden()) {
            continue;
        }

        const auto* meta = GetUnitRenderSplatMeta(*unit);
        if (meta && unit->IsBuilding() && !meta->uberSplatPath.empty() && meta->uberSplatScale > 0.0f) {
            const float halfSize = meta->uberSplatScale * 0.5f;
            terrain.AddRuntimeSplatRect(
                "building:" + std::to_string(unit->GetHandleId()),
                meta->uberSplatPath,
                unit->GetX() - halfSize,
                unit->GetY() - halfSize,
                unit->GetX() + halfSize,
                unit->GetY() + halfSize,
                2.0f,
                false,
                false);
        }

        if (selectedHandles.find(unit->GetHandleId()) == selectedHandles.end()) {
            continue;
        }

        const float selectionSize = ComputeSelectionCircleSizeWorld(
            *unit, [](const CUnit& u) { return unit_selection_geom::ComputeUnitSelectionWorldRadius(u); });
        const std::string texturePath = PickSelectionCircleTexturePath(selectionSize);
        if (texturePath.empty() || selectionSize <= 0.0f) {
            continue;
        }

        const float halfSize = selectionSize * 0.5f;
        terrain.AddRuntimeSplatRect(
            "selection:" + std::to_string(unit->GetHandleId()),
            texturePath,
            unit->GetX() - halfSize,
            unit->GetY() - halfSize,
            unit->GetX() + halfSize,
            unit->GetY() + halfSize,
            6.0f,
            true,
            false);
    }
}