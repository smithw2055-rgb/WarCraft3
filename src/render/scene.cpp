#include "scene.h"
#include "resource/slk.h"
#include <iostream>
#include <algorithm>
#include <cctype>
#include <unordered_set>

static std::string War3IDToString(uint32_t id) {
    char s[5] = {(char)(id&0xFF), (char)((id>>8)&0xFF),
                 (char)((id>>16)&0xFF), (char)((id>>24)&0xFF), 0};
    return std::string(s);
}

static uint32_t StringToWar3ID(const std::string& id) {
    uint32_t war3ID = 0;
    for (int i = 0; i < 4 && i < (int)id.size(); ++i) {
        war3ID |= ((uint32_t)(unsigned char)id[i]) << ((3 - i) * 8);
    }
    return war3ID;
}

static std::string NormalizePath(const std::string& p) {
    std::string r = p;
    for (auto& c : r) if (c == '/') c = '\\';
    // Remove trailing .mdx if present
    if (r.size() > 4) {
        std::string tail = r.substr(r.size() - 4);
        std::transform(tail.begin(), tail.end(), tail.begin(), [](unsigned char c) {
            return (char)std::tolower(c);
        });
        if (tail == ".mdx") r.resize(r.size() - 4);
    }
    return r;
}

static void PushUniquePath(std::vector<std::string>& paths, const std::string& path) {
    if (path.empty()) return;
    if (std::find(paths.begin(), paths.end(), path) == paths.end()) {
        paths.push_back(path);
    }
}

bool SceneManager::Init(const MpqManager& mpq) {
    mpq_ = const_cast<MpqManager*>(&mpq);
    ResetLoadStats();

    // Load UnitUI.slk → unit War3ID → model path
    {
        std::vector<uint8_t> buf;
        if (mpq.ReadFile("Units\\unitUI.slk", buf)) {
            SlkTable slk;
            if (slk.Load(buf.data(), buf.size())) {
                int colID = -1, colFile = -1;
                for (int i = 0; i < slk.Cols(); i++) {
                    if (slk.colNames_[i] == "unitUIID") colID = i;
                    if (slk.colNames_[i] == "file") colFile = i;
                }
                if (colID >= 0 && colFile >= 0) {
                    for (int r = 0; r < slk.Rows(); r++) {
                        std::string id = slk.GetString(r, colID);
                        std::string file = slk.GetString(r, colFile);
                        if (!id.empty() && !file.empty()) {
                            uint32_t war3ID = StringToWar3ID(id);
                            std::string modelPath = NormalizePath(file) + ".mdx";
                            unitPaths_[war3ID] = modelPath;
                        }
                    }
                    std::cout << "Scene: " << unitPaths_.size() << " unit paths loaded" << std::endl;
                }
            }
        }
    }

    // Load Doodads.slk → doodad War3ID → model path
    {
        std::vector<uint8_t> buf;
        if (mpq.ReadFile("Doodads\\Doodads.slk", buf)) {
            SlkTable slk;
            if (slk.Load(buf.data(), buf.size())) {
                int colID = -1, colDir = -1, colFile = -1;
                for (int i = 0; i < slk.Cols(); i++) {
                    if (slk.colNames_[i] == "doodID") colID = i;
                    if (slk.colNames_[i] == "dir") colDir = i;
                    if (slk.colNames_[i] == "file") colFile = i;
                }
                if (colID >= 0 && colFile >= 0) {
                    for (int r = 0; r < slk.Rows(); r++) {
                        std::string id = slk.GetString(r, colID);
                        std::string dir = slk.GetString(r, colDir);
                        std::string file = slk.GetString(r, colFile);
                        if (!id.empty() && !file.empty()) {
                            uint32_t war3ID = StringToWar3ID(id);
                            std::string basePath = file;
                            if (!dir.empty()) {
                                basePath = dir + "\\" + file;
                            }
                            // Store the base file path; ResolveAndLoad will try variations
                            doodadPaths_[war3ID] = NormalizePath(basePath);
                        }
                    }
                    std::cout << "Scene: " << doodadPaths_.size() << " doodad paths loaded" << std::endl;
                }
                else {
                    std::cout << "Scene: Doodads.slk missing columns"
                              << " doodID=" << colID
                              << " dir=" << colDir
                              << " file=" << colFile << std::endl;
                }
            }
        }
    }

    {
        SlkTable slk;
        std::vector<uint8_t> buf;
        if (mpq.ReadFile("Units\\DestructableData.slk", buf)) {
            if (slk.Load(buf.data(), buf.size())) {
                int colID = -1, colFile = -1;
                for (int i = 0; i < slk.Cols(); i++) {
                    const std::string& name = slk.colNames_[i];
                    if (name == "unitID" || name == "destructableID" || name == "doodID") colID = i;
                    if (name == "file" || name == "dir") {
                        if (name == "file") colFile = i;
                    }
                }

                if (colID < 0 && slk.Cols() > 0) {
                    colID = 0;
                }

                int destructableCount = 0;
                if (colID >= 0 && colFile >= 0) {
                    for (int r = 0; r < slk.Rows(); ++r) {
                        std::string sid = slk.GetString(r, colID);
                        std::string file = slk.GetString(r, colFile);
                        if (sid.size() != 4 || file.empty()) continue;

                        uint32_t war3ID = StringToWar3ID(sid);
                        doodadPaths_[war3ID] = NormalizePath(file);
                        destructableCount++;
                    }
                    std::cout << "Scene: " << destructableCount << " destructable paths loaded" << std::endl;
                }
                else {
                    std::cout << "Scene: DestructableData.slk missing columns"
                              << " id=" << colID
                              << " file=" << colFile << std::endl;
                }
            }
        }
    }

    return true;
}

void SceneManager::Clear() {
    for (auto* inst : instances_) delete inst;
    instances_.clear();
    cache_.clear();
    ResetLoadStats();
}

void SceneManager::ResetLoadStats() {
    loadStats_ = LoadStats{};
}

void SceneManager::PrintLoadStats() const {
    std::cout << "SceneStats: units " << loadStats_.unitSuccess << "/" << loadStats_.unitAttempts
              << ", doodads " << loadStats_.doodadSuccess << "/" << loadStats_.doodadAttempts
              << std::endl;
    if (loadStats_.missReasons.empty()) return;

    std::vector<std::pair<std::string, int>> misses(loadStats_.missReasons.begin(), loadStats_.missReasons.end());
    std::sort(misses.begin(), misses.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });
    for (const auto& miss : misses) {
        std::cout << "  SceneMiss: " << miss.first << " x" << miss.second << std::endl;
    }
}

MdxViewer* SceneManager::GetOrLoadModel(const std::string& path) {
    auto it = cache_.find(path);
    if (it == cache_.end()) {
        auto [insertIt, inserted] = cache_.try_emplace(path);
        insertIt->second.path = path;
        it = insertIt;
    }

    MdxEntry& entry = it->second;
    if (!EnsureModelCached(path, entry)) {
        return nullptr;
    }
    if (entry.sharedViewer) {
        return entry.sharedViewer.get();
    }

    entry.sharedViewer = std::make_unique<MdxViewer>();
    entry.sharedViewer->DebugLogState("scene-shared-after-ctor");
    std::cout << "SceneModelLoad: viewer loading " << path << std::endl;
    if (!entry.sharedViewer->Load(entry.model, mpq_)) {
        std::cout << "SceneModelLoad: viewer load failed for " << path << std::endl;
        entry.sharedViewer.reset();
        return nullptr;
    }

    return entry.sharedViewer.get();
}

bool SceneManager::EnsureModelCached(const std::string& path, MdxEntry& entry) {
    if (entry.modelLoaded) {
        return true;
    }

    std::vector<uint8_t> data;
    if (!mpq_ || !mpq_->ReadFile(path, data)) {
        return false;
    }

    MdxModel model;
    if (!MdxLoad(data.data(), data.size(), model)) {
        return false;
    }
    entry.path = path;
    entry.model = std::move(model);
    entry.modelLoaded = true;
    return true;
}

std::unique_ptr<MdxViewer> SceneManager::CreateUnitViewer(const std::string& path) {
    MdxViewer* sharedViewer = GetOrLoadModel(path);
    if (!sharedViewer) {
        return nullptr;
    }

    auto viewer = std::make_unique<MdxViewer>();
    if (!viewer->ShareModelResourcesFrom(*sharedViewer)) {
        return nullptr;
    }
    return viewer;
}

MdxViewer* SceneManager::ResolveAndLoad(uint32_t war3ID, uint32_t skinId, int variation, bool doodad, const char** missReason) {
    std::vector<uint32_t> ids;
    if (skinId != 0 && skinId != war3ID) ids.push_back(skinId);
    ids.push_back(war3ID);

    if (!doodad) {
        bool sawUnitPath = false;
        for (uint32_t id : ids) {
            auto unitIt = unitPaths_.find(id);
            if (unitIt == unitPaths_.end()) continue;
            sawUnitPath = true;
            MdxViewer* viewer = GetOrLoadModel(unitIt->second);
            if (viewer) return viewer;
        }
        if (missReason) *missReason = sawUnitPath ? "unit-model-miss" : "unit-path-miss";
        return nullptr;
    }

    bool sawDoodadPath = false;
    int safeVariation = std::max(0, variation);
    for (uint32_t id : ids) {
        auto doodIt = doodadPaths_.find(id);
        if (doodIt == doodadPaths_.end()) continue;

        sawDoodadPath = true;
        std::string basePath = NormalizePath(doodIt->second);
        size_t slash = basePath.find_last_of('\\');
        std::string leaf = (slash == std::string::npos) ? basePath : basePath.substr(slash + 1);

        std::vector<std::string> candidates;
        PushUniquePath(candidates, basePath + ".mdx");
        PushUniquePath(candidates, basePath + std::to_string(safeVariation) + ".mdx");
        PushUniquePath(candidates, basePath + "\\" + leaf + std::to_string(safeVariation) + ".mdx");
        PushUniquePath(candidates, basePath + "\\" + leaf + "0.mdx");
        PushUniquePath(candidates, basePath + "_" + std::to_string(safeVariation) + ".mdx");
        PushUniquePath(candidates, basePath + "_v" + std::to_string(safeVariation) + ".mdx");
        PushUniquePath(candidates, basePath + "_V" + std::to_string(safeVariation) + ".mdx");

        for (const std::string& candidate : candidates) {
            MdxViewer* viewer = GetOrLoadModel(candidate);
            if (viewer) return viewer;
        }
    }

    if (missReason) *missReason = sawDoodadPath ? "doodad-model-miss" : "doodad-path-miss";
    return nullptr;
}

bool SceneManager::AddDoodad(uint32_t war3ID, const float pos[3], float angle,
                              const float scale[3], int variation, int flags, int life, uint32_t skinId) {
    (void)flags;
    (void)life;
    loadStats_.doodadAttempts++;

    const char* missReason = nullptr;
    MdxViewer* viewer = ResolveAndLoad(war3ID, skinId, variation, true, &missReason);
    if (!viewer) {
        loadStats_.missReasons[missReason ? missReason : "doodad-unknown-miss"]++;
        return false;
    }

    auto* inst = new PlacedModel();
    inst->pos[0] = pos[0]; inst->pos[1] = pos[2]; inst->pos[2] = pos[1];
    inst->renderPos[0] = inst->pos[0]; inst->renderPos[1] = inst->pos[1]; inst->renderPos[2] = inst->pos[2];
    inst->angle = angle;
    inst->renderAngle = angle;
    inst->scale[0] = scale[0]; inst->scale[1] = scale[2]; inst->scale[2] = scale[1];
    inst->variation = variation;
    inst->visible = true;
    inst->viewer = viewer;
    instances_.push_back(inst);
    static int doodadDiagCount = 0;
    if (doodadDiagCount < 8) {
        char s[5] = {(char)(war3ID & 0xFF), (char)((war3ID >> 8) & 0xFF),
                     (char)((war3ID >> 16) & 0xFF), (char)((war3ID >> 24) & 0xFF), 0};
        std::cout << "DoodadDiag[" << doodadDiagCount << "]: id=" << s
                  << " rawPos=(" << pos[0] << "," << pos[1] << "," << pos[2] << ")"
                  << " mappedPos=(" << inst->pos[0] << "," << inst->pos[1] << "," << inst->pos[2] << ")"
                  << " scale=(" << inst->scale[0] << "," << inst->scale[1] << "," << inst->scale[2] << ")"
                  << " angle=" << angle
                  << " variation=" << variation
                  << std::endl;
        doodadDiagCount++;
    }
    loadStats_.doodadSuccess++;
    return true;
}

bool SceneManager::AddUnit(uint32_t war3ID, const float pos[3], float angle,
                            const float scale[3], int player, int hitpoints, int flags, uint32_t skinId,
                            bool playBirthAnimation) {
    (void)hitpoints;
    (void)flags;
    loadStats_.unitAttempts++;

    const char* missReason = nullptr;
    std::unique_ptr<MdxViewer> uniqueViewer;
    MdxViewer* viewer = nullptr;

    // Units need per-instance animation/geoset state like warsmash_ref. We still
    // share immutable GPU/model resources under the hood to avoid buffer-pool exhaustion.
    std::vector<uint32_t> ids;
    if (skinId != 0 && skinId != war3ID) ids.push_back(skinId);
    ids.push_back(war3ID);

    bool sawUnitPath = false;
    for (uint32_t id : ids) {
        auto unitIt = unitPaths_.find(id);
        if (unitIt == unitPaths_.end()) continue;
        sawUnitPath = true;
        uniqueViewer = CreateUnitViewer(unitIt->second);
        if (uniqueViewer) {
            break;
        }
    }

    viewer = uniqueViewer.get();
    if (!viewer && !sawUnitPath) {
        missReason = "unit-path-miss";
    }

    if (!viewer) {
        viewer = ResolveAndLoad(war3ID, skinId, 0, false, &missReason);
    }
    if (!viewer) {
        loadStats_.missReasons[missReason ? missReason : "unit-unknown-miss"]++;
        return false;
    }

    auto* inst = new PlacedModel();
    inst->pos[0] = pos[0]; inst->pos[1] = pos[2]; inst->pos[2] = pos[1];
    inst->renderPos[0] = inst->pos[0]; inst->renderPos[1] = inst->pos[1]; inst->renderPos[2] = inst->pos[2];
    inst->angle = angle;
    inst->renderAngle = angle;
    inst->scale[0] = scale[0]; inst->scale[1] = scale[2]; inst->scale[2] = scale[1];
    inst->player = player;
    inst->visible = true;
    inst->smoothMotion = true;
    inst->ownedViewer = std::move(uniqueViewer);
    inst->viewer = viewer;
    if (playBirthAnimation) {
        inst->viewer->PlayBirthThenStandSequence();
    }
    instances_.push_back(inst);
    loadStats_.unitSuccess++;
    return true;
}

bool SceneManager::UpdateInstancePosition(int index, const float pos[3], float angle,
                                          float movementSpeed, float turnRate,
                                          bool smoothMotion) {
    if (index < 0 || index >= (int)instances_.size()) return false;
    auto* inst = instances_[index];
    inst->pos[0] = pos[0];
    inst->pos[1] = pos[2]; // Y and Z swapped
    inst->pos[2] = pos[1];
    inst->angle = angle;
    inst->movementSpeed = movementSpeed;
    inst->turnRate = turnRate;
    inst->smoothMotion = smoothMotion;
    return true;
}

bool SceneManager::SetInstanceVisible(int index, bool visible) {
    if (index < 0 || index >= (int)instances_.size()) return false;
    instances_[index]->visible = visible;
    return true;
}

void SceneManager::RenderAll(const Mat4& view, const Mat4& proj, float dt) {
    auto normalizeAngleDegrees = [](float angle) {
        while (angle < 0.0f) angle += 360.0f;
        while (angle >= 360.0f) angle -= 360.0f;
        return angle;
    };
    auto normalizeAngleDelta = [](float delta) {
        while (delta < -180.0f) delta += 360.0f;
        while (delta > 180.0f) delta -= 360.0f;
        return delta;
    };

    std::unordered_set<MdxViewer*> updatedViewers;
    for (auto* inst : instances_) {
        if (inst->smoothMotion) {
            const float dx = inst->pos[0] - inst->renderPos[0];
            const float dz = inst->pos[2] - inst->renderPos[2];
            const float distance = std::sqrt((dx * dx) + (dz * dz));
            const float speedDelta = std::max(0.0f, inst->movementSpeed) * std::max(0.0f, dt);
            if ((distance > speedDelta) && (speedDelta > 0.0f) && (dt < 1.0f)) {
                inst->renderPos[0] += (speedDelta * dx) / distance;
                inst->renderPos[2] += (speedDelta * dz) / distance;
            }
            else {
                inst->renderPos[0] = inst->pos[0];
                inst->renderPos[2] = inst->pos[2];
            }
            inst->renderPos[1] = inst->pos[1];

            const float targetAngle = normalizeAngleDegrees(inst->angle);
            const float currentAngle = normalizeAngleDegrees(inst->renderAngle);
            const float angleDelta = normalizeAngleDelta(targetAngle - currentAngle);
            const float turnDegreesPerTick = std::max(0.1f, inst->turnRate * 180.0f / 3.1415926535f);
            const float turnDegreesPerSecond = turnDegreesPerTick / (1.0f / 20.0f);
            const float maxTurnStep = turnDegreesPerSecond * std::max(0.0f, dt);
            if (std::fabs(angleDelta) <= maxTurnStep) {
                inst->renderAngle = targetAngle;
            }
            else {
                inst->renderAngle = normalizeAngleDegrees(
                    currentAngle + ((angleDelta < 0.0f) ? -maxTurnStep : maxTurnStep));
            }
        }
        else {
            inst->renderPos[0] = inst->pos[0];
            inst->renderPos[1] = inst->pos[1];
            inst->renderPos[2] = inst->pos[2];
            inst->renderAngle = inst->angle;
        }

        if (inst->visible && inst->viewer) {
            if (updatedViewers.insert(inst->viewer).second) {
                inst->viewer->Update(dt, view);
            }
        }
    }

    for (auto* inst : instances_) {
        if (inst->visible && inst->viewer) {
            Vec3 p = {inst->renderPos[0], inst->renderPos[1], inst->renderPos[2]};
            float s = (inst->scale[0] + inst->scale[1] + inst->scale[2]) / 3.0f;
            inst->viewer->RenderAt(p, view, proj, s, inst->player, inst->renderAngle);
        }
    }
}
