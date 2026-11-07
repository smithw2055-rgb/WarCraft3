#include "scripting/jass_natives_internal.h"

void JassNatives::RegisterLocationNatives() {
    vm_.RegisterNative("Location", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        float x = args.size() > 0 ? args[0].realVal : 0;
        float y = args.size() > 1 ? args[1].realVal : 0;
        auto loc = std::make_shared<JassLocation>();
        loc->x = x; loc->y = y;
        int id = data->nextLocId++;
        data->locations[id] = loc;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::LOCATION, id));
    });
    vm_.RegisterNative("RemoveLocation", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::LOCATION);
        if (simId >= 0) { data->locations.erase(simId); data->FreeHandle(args[0].intVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("MoveLocation", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::LOCATION);
        if (simId >= 0) {
            auto it = data->locations.find(simId);
            if (it != data->locations.end()) { it->second->x = args[1].realVal; it->second->y = args[2].realVal; }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetLocationX", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::LOCATION);
        if (simId >= 0) { auto it = data->locations.find(simId); if (it != data->locations.end()) return JassValue::Real(it->second->x); }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("GetLocationY", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::LOCATION);
        if (simId >= 0) { auto it = data->locations.find(simId); if (it != data->locations.end()) return JassValue::Real(it->second->y); }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("GetLocationZ", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::LOCATION);
        if (simId < 0) return JassValue::Real(0);
        auto it = data->locations.find(simId);
        if (it == data->locations.end() || !it->second) return JassValue::Real(0);
        // Ref Jass2 GetLocationZ: war3MapViewer.terrain.getGroundHeight(x, y).
        float height = 0.0f;
        if (data->terrainGroundHeight) {
            height = data->terrainGroundHeight(it->second->x, it->second->y);
        } else {
            height = data->TerrainDeformOffset(it->second->x, it->second->y);
        }
        return JassValue::Real(height);
    });
}

// ============================================================
// 9. Rect natives
// ============================================================
void JassNatives::RegisterRectNatives() {
    vm_.RegisterNative("Rect", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Int(0);
        auto rect = std::make_shared<JassRect>();
        rect->minX = args[0].realVal; rect->minY = args[1].realVal;
        rect->maxX = args[2].realVal; rect->maxY = args[3].realVal;
        int id = data->nextRectId++;
        data->rects[id] = rect;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::RECT, id));
    });
    vm_.RegisterNative("RectFromLoc", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Int(0);
        int minSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::LOCATION);
        int maxSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        if (minSim < 0 || maxSim < 0) return JassValue::Int(0);
        auto minIt = data->locations.find(minSim), maxIt = data->locations.find(maxSim);
        if (minIt == data->locations.end() || maxIt == data->locations.end()) return JassValue::Int(0);
        auto rect = std::make_shared<JassRect>();
        rect->minX = std::min(minIt->second->x, maxIt->second->x);
        rect->minY = std::min(minIt->second->y, maxIt->second->y);
        rect->maxX = std::max(minIt->second->x, maxIt->second->x);
        rect->maxY = std::max(minIt->second->y, maxIt->second->y);
        int id = data->nextRectId++;
        data->rects[id] = rect;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::RECT, id));
    });
    vm_.RegisterNative("RemoveRect", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::RECT);
        if (simId >= 0) { data->rects.erase(simId); data->FreeHandle(args[0].intVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("MoveRectToLoc", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        const int rectId = data->GetSimHandle(args[0].intVal, JassHandleInfo::RECT);
        const int locId = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        if (rectId < 0 || locId < 0) return JassValue::Null();
        auto rectIt = data->rects.find(rectId);
        auto locIt = data->locations.find(locId);
        if (rectIt == data->rects.end() || locIt == data->locations.end()) return JassValue::Null();
        JassRect& rect = *rectIt->second;
        const float halfWidth = (rect.maxX - rect.minX) * 0.5f;
        const float halfHeight = (rect.maxY - rect.minY) * 0.5f;
        rect.minX = locIt->second->x - halfWidth;
        rect.maxX = locIt->second->x + halfWidth;
        rect.minY = locIt->second->y - halfHeight;
        rect.maxY = locIt->second->y + halfHeight;
        return JassValue::Null();
    });
    vm_.RegisterNative("GetRectCenterX", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::RECT);
        if (simId >= 0) { auto it = data->rects.find(simId); if (it != data->rects.end()) return JassValue::Real((it->second->minX + it->second->maxX) * 0.5f); }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("GetRectCenterY", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::RECT);
        if (simId >= 0) { auto it = data->rects.find(simId); if (it != data->rects.end()) return JassValue::Real((it->second->minY + it->second->maxY) * 0.5f); }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("GetRectMinX", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::RECT);
        if (simId >= 0) { auto it = data->rects.find(simId); if (it != data->rects.end()) return JassValue::Real(it->second->minX); }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("GetRectMinY", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::RECT);
        if (simId >= 0) { auto it = data->rects.find(simId); if (it != data->rects.end()) return JassValue::Real(it->second->minY); }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("GetRectMaxX", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::RECT);
        if (simId >= 0) { auto it = data->rects.find(simId); if (it != data->rects.end()) return JassValue::Real(it->second->maxX); }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("GetRectMaxY", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::RECT);
        if (simId >= 0) { auto it = data->rects.find(simId); if (it != data->rects.end()) return JassValue::Real(it->second->maxY); }
        return JassValue::Real(0);
    });
}

// ============================================================
// 10. Region natives
// ============================================================
void JassNatives::RegisterRegionNatives() {
    vm_.RegisterNative("CreateRegion", [data = data_](const std::vector<JassValue>&) -> JassValue {
        auto region = std::make_shared<JassRegion>();
        int id = data->nextRegionId++;
        data->regions[id] = region;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::REGION, id));
    });
    vm_.RegisterNative("RemoveRegion", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::REGION);
        if (simId >= 0) { data->regions.erase(simId); data->FreeHandle(args[0].intVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("RegionAddRect", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int rSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::REGION);
        int rectSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::RECT);
        if (rSim < 0 || rectSim < 0) return JassValue::Null();
        auto rit = data->regions.find(rSim); auto rectIt = data->rects.find(rectSim);
        if (rit == data->regions.end() || rectIt == data->rects.end()) return JassValue::Null();
        rit->second->rectIds.push_back(rectSim);
        rit->second->rects.push_back(*rectIt->second);
        return JassValue::Null();
    });
    vm_.RegisterNative("RegionClearRect", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int rSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::REGION);
        int rectSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::RECT);
        if (rSim < 0) return JassValue::Null();
        auto rit = data->regions.find(rSim);
        if (rit == data->regions.end()) return JassValue::Null();
        auto& ids = rit->second->rectIds;
        ids.erase(std::remove(ids.begin(), ids.end(), rectSim), ids.end());
        rit->second->rects.clear();
        for (int rid : rit->second->rectIds) {
            auto rectIt = data->rects.find(rid);
            if (rectIt != data->rects.end()) rit->second->rects.push_back(*rectIt->second);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("IsUnitInRegion", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Bool(false);
        int rSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::REGION);
        if (rSim < 0) return JassValue::Bool(false);
        auto rit = data->regions.find(rSim);
        if (rit == data->regions.end()) return JassValue::Bool(false);
        int uSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::UNIT);
        if (uSim < 0) return JassValue::Bool(false);
        CUnit* unit = sim->GetUnit(uSim);
        if (!unit) return JassValue::Bool(false);
        return JassValue::Bool(IsPointInJassRegion(*rit->second, unit->GetX(), unit->GetY()));
    });
}

// ============================================================
// 11. Unit natives (expanded)
// ============================================================

void JassNatives::RegisterSoundNatives() {
    vm_.RegisterNative("CreateSound", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 6) return JassValue::Int(0);
        auto s = std::make_shared<JassSound>();
        s->label = args[0].strVal;
        s->looping = args[1].boolVal;
        int id = data->nextSoundId++;
        data->sounds[id] = s;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::SOUND, id));
    });
    vm_.RegisterNative("CreateSoundFromLabel", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Int(0);
        auto s = std::make_shared<JassSound>();
        s->label = args[0].strVal;
        s->looping = args[1].boolVal;
        int id = data->nextSoundId++;
        data->sounds[id] = s;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::SOUND, id));
    });
    vm_.RegisterNative("CreateMIDISound", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        auto s = std::make_shared<JassSound>();
        if (!args.empty()) {
            s->label = args[0].strVal;
        }
        int id = data->nextSoundId++;
        data->sounds[id] = s;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::SOUND, id));
    });
    vm_.RegisterNative("KillSoundWhenDone", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (id >= 0) {
            auto it = data->sounds.find(id);
            if (it != data->sounds.end()) it->second->killWhenDone = true;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetSoundVolume", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (id >= 0) { auto it = data->sounds.find(id); if (it != data->sounds.end()) it->second->volume = args[1].realVal / 127.0f; }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetSoundPitch", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (id >= 0) { auto it = data->sounds.find(id); if (it != data->sounds.end()) it->second->pitch = args[1].realVal; }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetSoundDuration", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        const int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (id >= 0) {
            auto it = data->sounds.find(id);
            if (it != data->sounds.end()) it->second->durationMs = args[1].intVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetSoundParamsFromLabel", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        const int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (id >= 0) {
            auto it = data->sounds.find(id);
            if (it != data->sounds.end() && !args[1].strVal.empty()) it->second->label = args[1].strVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetSoundPosition", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        const int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (id >= 0) {
            auto it = data->sounds.find(id);
            if (it != data->sounds.end()) {
                it->second->x = args[1].realVal;
                it->second->y = args[2].realVal;
                it->second->z = args[3].realVal;
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetSoundChannel", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (id >= 0) {
            auto it = data->sounds.find(id);
            if (it != data->sounds.end()) it->second->channel = args[1].intVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetSoundDistances", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !data) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (id >= 0) {
            auto it = data->sounds.find(id);
            if (it != data->sounds.end()) {
                it->second->minDistance = args[1].realVal;
                it->second->maxDistance = args[2].realVal;
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetSoundDistanceCutoff", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (id >= 0) {
            auto it = data->sounds.find(id);
            if (it != data->sounds.end()) it->second->distanceCutoff = args[1].realVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("RegisterStackedSound", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !data) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (id >= 0) {
            auto it = data->sounds.find(id);
            if (it != data->sounds.end()) {
                it->second->stackedByPosition = args[1].boolVal;
                it->second->stackedWidth = args[2].realVal;
                it->second->stackedHeight = args[3].realVal;
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("AttachSoundToUnit", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        int unitId = data->GetSimHandle(args[1].intVal, JassHandleInfo::UNIT);
        if (id >= 0) {
            auto it = data->sounds.find(id);
            if (it != data->sounds.end()) it->second->attachedUnitSimId = unitId;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("StartSound", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int sndSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (sndSim < 0) return JassValue::Null();
        auto sit = data->sounds.find(sndSim);
        if (sit == data->sounds.end()) return JassValue::Null();
        sit->second->playing = true;
        auto* audio = sim ? sim->GetAudioSystem() : nullptr;
        if (audio && !sit->second->label.empty()) {
            audio->Play2D(sit->second->label, sit->second->volume);
        }
        return JassValue::Null();
    });
    // Ability-bound spell sounds (ref unitSoundEffectEvent family): the
    // effect file comes from the ability data (Effectsound); playback is
    // positional at the unit. Looping has no positional-loop API, so the
    // loop variant plays once and tracks the id for Stop (documented).
    auto spellSoundPath = [sim = sim_](CUnit* unit, uint32_t alias) -> std::string {
        if (!sim || !unit || !sim->GetAbilityData()) return "";
        const CAbilityDefFull* def = sim->GetAbilityData()->GetAbility(War3ID(alias));
        return def ? def->effectSound : "";
    };
    auto playSpellSound = [data = data_, sim = sim_, spellSoundPath](const std::vector<JassValue>& args) -> int {
        if (args.size() < 2 || !sim) return -1;
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CUnit* unit = uSim >= 0 ? sim->GetUnit(uSim) : nullptr;
        if (!unit) return -1;
        const std::string path = spellSoundPath(unit, (uint32_t)args[1].intVal);
        if (path.empty()) return -1;
        auto* audio = sim->GetAudioSystem();
        if (!audio) return -1;
        return audio->Play3DPath(path, unit->GetX(), unit->GetY(), 0.0f, 1.0f);
    };
    vm_.RegisterNative("UnitSpellSoundEffect", [playSpellSound](const std::vector<JassValue>& args) -> JassValue {
        playSpellSound(args);
        return JassValue::Null();
    });
    vm_.RegisterNative("UnitLoopSpellSoundEffect", [data = data_, playSpellSound](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        const int id = playSpellSound(args);
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (id >= 0 && uSim >= 0) {
            const int64_t key = (static_cast<int64_t>(uSim) << 32) | (uint32_t)args[1].intVal;
            data->loopedSpellSounds[key] = id;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("UnitStopSpellSoundEffect", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (uSim < 0) return JassValue::Null();
        const int64_t key = (static_cast<int64_t>(uSim) << 32) | (uint32_t)args[1].intVal;
        auto it = data->loopedSpellSounds.find(key);
        if (it != data->loopedSpellSounds.end()) {
            if (sim) {
                if (auto* audio = sim->GetAudioSystem()) audio->Stop(it->second);
            }
            data->loopedSpellSounds.erase(it);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("StopSound", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int sndSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (sndSim >= 0) {
            auto sit = data->sounds.find(sndSim);
            if (sit != data->sounds.end()) {
                sit->second->playing = false;
                if (args[1].boolVal) sit->second->killWhenDone = true;
            }
        }
        (void)sim;
        return JassValue::Null();
    });
    vm_.RegisterNative("DestroySound", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (id >= 0) { data->sounds.erase(id); data->FreeHandle(args[0].intVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetSoundDuration", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Int(0);
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (id >= 0) {
            auto it = data->sounds.find(id);
            if (it != data->sounds.end()) return JassValue::Int(it->second->durationMs);
        }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("PlaySound", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int sndSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (sndSim < 0) return JassValue::Null();
        auto sit = data->sounds.find(sndSim);
        if (sit == data->sounds.end() || !sim) return JassValue::Null();
        sit->second->playing = true;
        auto* audio = sim->GetAudioSystem();
        if (audio && !sit->second->label.empty())
            audio->Play2D(sit->second->label, sit->second->volume);
        return JassValue::Null();
    });
    vm_.RegisterNative("PlaySoundAtPoint", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int sndSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::SOUND);
        if (sndSim < 0) return JassValue::Null();
        auto sit = data->sounds.find(sndSim);
        if (sit == data->sounds.end() || !sim) return JassValue::Null();
        auto* audio = sim->GetAudioSystem();
        if (audio && !sit->second->label.empty())
            audio->Play3D(sit->second->label, args[1].realVal, args[2].realVal, 0, sit->second->volume);
        return JassValue::Null();
    });
    vm_.RegisterNative("VolumeGroupReset", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!data) return JassValue::Null();
        (void)args;
        data->music.volume = 127;
        return JassValue::Null();
    });
}

// ============================================================
// 20. TextTag natives
// ============================================================
void JassNatives::RegisterTextTagNatives() {
    vm_.RegisterNative("CreateTextTag", [data = data_](const std::vector<JassValue>&) -> JassValue {
        auto tag = std::make_shared<JassTextTag>();
        int id = data->nextTextTagId++;
        data->textTags[id] = tag;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::TEXT_TAG, id));
    });
    vm_.RegisterNative("DestroyTextTag", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::TEXT_TAG);
        if (id >= 0) { data->textTags.erase(id); data->FreeHandle(args[0].intVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetTextTagText", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::TEXT_TAG);
        if (id >= 0) { auto it = data->textTags.find(id); if (it != data->textTags.end()) { it->second->text = args[1].strVal; it->second->fontSize = args[2].realVal > 0 ? args[2].realVal : 12.0f; it->second->height = it->second->fontSize; } }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetTextTagPos", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::TEXT_TAG);
        if (id >= 0) { auto it = data->textTags.find(id); if (it != data->textTags.end()) { it->second->x = args[1].realVal; it->second->y = args[2].realVal; it->second->z = args[3].realVal; } }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetTextTagPosUnit", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::TEXT_TAG);
        if (id < 0 || !sim) return JassValue::Null();
        auto it = data->textTags.find(id);
        if (it == data->textTags.end() || !it->second) return JassValue::Null();
        const int unitId = data->GetSimHandle(args[1].intVal, JassHandleInfo::UNIT);
        CUnit* unit = sim->GetUnit(unitId);
        if (!unit) return JassValue::Null();
        // Snap to the unit's current position (continuous follow rides
        // with the renderer's world-anchored text pass).
        it->second->x = unit->GetX();
        it->second->y = unit->GetY();
        it->second->z = args[2].realVal;
        it->second->height = args[2].realVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetTextTagColor", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::TEXT_TAG);
        if (id >= 0) {
            auto it = data->textTags.find(id);
            if (it != data->textTags.end()) {
                int a = args[1].intVal, r = args[2].intVal, g = args[3].intVal, b = args[4].intVal;
                it->second->color = (a << 24) | (r << 16) | (g << 8) | b;
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetTextTagVisibility", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::TEXT_TAG);
        if (id >= 0) { auto it = data->textTags.find(id); if (it != data->textTags.end()) it->second->visible = args[1].boolVal; }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetTextTagSuspended", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::TEXT_TAG);
        if (id >= 0) { auto it = data->textTags.find(id); if (it != data->textTags.end()) it->second->suspended = args[1].boolVal; }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetTextTagPermanent", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::TEXT_TAG);
        if (id >= 0) { auto it = data->textTags.find(id); if (it != data->textTags.end()) it->second->permanent = args[1].boolVal; }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetTextTagCentered", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::TEXT_TAG);
        if (id >= 0) { auto it = data->textTags.find(id); if (it != data->textTags.end()) it->second->centered = args[1].boolVal; }
        return JassValue::Null();
    });
    // Config-spawned floating text (ref spawnTextTag): per-config colors /
    // motion live in the render config table; the sim anchors the tag above
    // the source unit with a readable default (2s life, upward drift).
    auto spawnConfigTag = [data = data_, sim = sim_](const std::vector<JassValue>& args, bool intValue) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Int(0);
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CUnit* unit = uSim >= 0 ? sim->GetUnit(uSim) : nullptr;
        if (!unit) return JassValue::Int(0);
        auto tag = std::make_shared<JassTextTag>();
        tag->text = intValue ? std::to_string(args[2].intVal) : args[2].strVal;
        tag->x = unit->GetX();
        tag->y = unit->GetY();
        tag->z = 60.0f;
        tag->height = 60.0f;
        tag->fontSize = 12.0f;
        tag->vy = 40.0f;
        tag->lifespan = 2.0f;
        tag->fadepoint = 1.5f;
        (void)args[1].intVal; // config ordinal (TextTagConfigType); presets pending
        int id = data->nextTextTagId++;
        data->textTags[id] = tag;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::TEXT_TAG, id));
    };
    vm_.RegisterNative("CreateTextTagFromConfig", [spawnConfigTag](const std::vector<JassValue>& args) -> JassValue {
        return spawnConfigTag(args, false);
    });
    vm_.RegisterNative("CreateIntTextTagFromConfig", [spawnConfigTag](const std::vector<JassValue>& args) -> JassValue {
        return spawnConfigTag(args, true);
    });
    vm_.RegisterNative("SetTextTagAge", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::TEXT_TAG);
        if (id < 0) return JassValue::Null();
        auto it = data->textTags.find(id);
        if (it != data->textTags.end() && it->second) it->second->age = args[1].realVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetTextTagLifespan", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::TEXT_TAG);
        if (id >= 0) { auto it = data->textTags.find(id); if (it != data->textTags.end()) it->second->lifespan = args[1].realVal; }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetTextTagFadepoint", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::TEXT_TAG);
        if (id >= 0) { auto it = data->textTags.find(id); if (it != data->textTags.end()) it->second->fadepoint = args[1].realVal; }
        return JassValue::Null();
    });
    vm_.RegisterNative("CreateFloatingTextFromString", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(0);
    });
}

// ============================================================
// 21. Special Effect natives
// ============================================================
void JassNatives::RegisterEffectNatives() {
    vm_.RegisterNative("AddSpecialEffect", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Int(0);
        auto e = std::make_shared<JassEffect>();
        e->modelPath = args[0].strVal;
        e->x = args[1].realVal;
        e->y = args[2].realVal;
        int id = data->nextEffectId++;
        data->effects[id] = e;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::EFFECT, id));
    });
    vm_.RegisterNative("AddSpecialEffectLoc", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Int(0);
        auto e = std::make_shared<JassEffect>();
        e->modelPath = args[0].strVal;
        int locSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        if (locSim >= 0) {
            auto it = data->locations.find(locSim);
            if (it != data->locations.end()) {
                e->x = it->second->x;
                e->y = it->second->y;
            }
        }
        int id = data->nextEffectId++;
        data->effects[id] = e;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::EFFECT, id));
    });
    vm_.RegisterNative("AddSpecialEffectTarget", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Int(0);
        auto e = std::make_shared<JassEffect>();
        e->modelPath = args[0].strVal;
        // Targets may be units, items or destructables; first hit wins
        // (same rule as the AddSpellEffectTarget path).
        static const JassHandleInfo::Type kinds[] = {
            JassHandleInfo::UNIT, JassHandleInfo::ITEM, JassHandleInfo::DESTRUCTABLE,
        };
        for (auto kind : kinds) {
            const int simId = data->GetSimHandle(args[1].intVal, kind);
            if (simId >= 0) {
                e->targetSimHandle = simId;
                break;
            }
        }
        e->attachPoint = args[2].strVal;
        int id = data->nextEffectId++;
        data->effects[id] = e;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::EFFECT, id));
    });
    vm_.RegisterNative("DestroyEffect", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::EFFECT);
        if (id >= 0) { data->effects.erase(id); data->FreeHandle(args[0].intVal); }
        return JassValue::Null();
    });
    // Attached effects follow their target (retail BlzGetSpecialEffectX/Y):
    // point effects report stored coords, target effects the widget's.
    auto effectPos = [data = data_, sim = sim_](int effectHandle, int coord) -> float {
        const int id = data->GetSimHandle(effectHandle, JassHandleInfo::EFFECT);
        if (id < 0) return 0.0f;
        auto it = data->effects.find(id);
        if (it == data->effects.end()) return 0.0f;
        const auto& e = it->second;
        if (e->targetSimHandle >= 0 && sim) {
            if (CWidget* w = sim->GetWidget(e->targetSimHandle)) {
                return coord == 0 ? w->GetX() : w->GetY();
            }
        }
        return coord == 0 ? e->x : e->y;
    };
    vm_.RegisterNative("BlzGetSpecialEffectX", [effectPos](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        return JassValue::Real(effectPos(args[0].intVal, 0));
    });
    vm_.RegisterNative("BlzGetSpecialEffectY", [effectPos](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        return JassValue::Real(effectPos(args[0].intVal, 1));
    });
    vm_.RegisterNative("BlzGetSpecialEffectZ", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        const int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::EFFECT);
        if (id < 0) return JassValue::Real(0);
        auto it = data->effects.find(id);
        if (it == data->effects.end()) return JassValue::Real(0);
        const auto& e = it->second;
        // Point effects report the stored height; attached effects ride the
        // target's fly height plus the stored offset (same follow rule as
        // X/Y, which track the widget position).
        if (e->targetSimHandle >= 0 && sim) {
            if (CWidget* w = sim->GetWidget(e->targetSimHandle)) {
                return JassValue::Real(e->z + w->GetFlyHeight());
            }
        }
        return JassValue::Real(e->z);
    });
    // NOTE: Add/Remove/EnableWeatherEffect live in RegisterWeatherNatives
    // (registered later, so they own these names); no stubs here.
    vm_.RegisterNative("TerrainDeformCrater", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 6) return JassValue::Int(0);
        auto deform = std::make_shared<JassNativesData::JassTerrainDeform>();
        deform->kind = 0;
        deform->x = args[0].realVal;
        deform->y = args[1].realVal;
        deform->radius = args[2].realVal;
        deform->depth = args[3].realVal;
        deform->duration = args[4].intVal;
        deform->permanent = args[5].boolVal;
        int id = data->nextTerrainDeformId++;
        data->terrainDeforms[id] = deform;
        return JassValue::Int(id);
    });
    vm_.RegisterNative("TerrainDeformRegistration", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        // Registration-only deform (no shape of its own); tracked so Stop
        // calls resolve like any other deform.
        (void)args;
        auto deform = std::make_shared<JassNativesData::JassTerrainDeform>();
        deform->kind = 4;
        int id = data->nextTerrainDeformId++;
        data->terrainDeforms[id] = deform;
        return JassValue::Int(id);
    });
}

// ============================================================
// 22. Fog natives
// ============================================================
void JassNatives::RegisterFogNatives() {
    auto fogStateOf = [](int v) {
        return static_cast<CFogState>(std::clamp(v, 0, 2));
    };
    // sharedVision fans the entry out to SHARED_VISION allies (ref allies).
    auto fogPlayers = [data = data_, sim = sim_](int player, bool shared) {
        std::vector<int> players;
        if (player >= 0) players.push_back(player);
        if (shared && sim) {
            const CPlayer* self = sim->GetPlayer(player);
            for (int i = 0; i < sim->GetPlayerCount(); i++) {
                if (i == player) continue;
                const CPlayer* other = sim->GetPlayer(i);
                if (self && other && self->HasAlliance(*other, CAllianceType::SHARED_VISION)) {
                    players.push_back(i);
                }
            }
        }
        return players;
    };
    vm_.RegisterNative("CreateFogModifierRect", [data = data_, sim = sim_, fogStateOf](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 6 || !sim) return JassValue::Int(0);
        const int player = ResolvePlayerId(data, args[0].intVal);
        if (player < 0) return JassValue::Int(0);
        const int fogId = sim->GetFogSystem()->AddModifier(
            player,
            CFogArea::Rect(args[2].realVal, args[3].realVal, args[4].realVal, args[5].realVal),
            fogStateOf(args[1].intVal));
        return JassValue::Int(data->AllocHandle(JassHandleInfo::FOG_MODIFIER, fogId));
    });
    vm_.RegisterNative("CreateFogModifierRadius", [data = data_, sim = sim_, fogStateOf](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5 || !sim) return JassValue::Int(0);
        const int player = ResolvePlayerId(data, args[0].intVal);
        if (player < 0) return JassValue::Int(0);
        const int fogId = sim->GetFogSystem()->AddModifier(
            player,
            CFogArea::Circle(args[2].realVal, args[3].realVal, args[4].realVal),
            fogStateOf(args[1].intVal));
        return JassValue::Int(data->AllocHandle(JassHandleInfo::FOG_MODIFIER, fogId));
    });
    vm_.RegisterNative("CreateFogModifierRadiusLoc", [data = data_, sim = sim_, fogStateOf](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !sim) return JassValue::Int(0);
        const int player = ResolvePlayerId(data, args[0].intVal);
        if (player < 0) return JassValue::Int(0);
        const int locSim = data->GetSimHandle(args[2].intVal, JassHandleInfo::LOCATION);
        auto it = data->locations.find(locSim);
        if (it == data->locations.end()) return JassValue::Int(0);
        const int fogId = sim->GetFogSystem()->AddModifier(
            player,
            CFogArea::Circle(it->second->x, it->second->y, args[3].realVal),
            fogStateOf(args[1].intVal));
        return JassValue::Int(data->AllocHandle(JassHandleInfo::FOG_MODIFIER, fogId));
    });
    vm_.RegisterNative("DestroyFogModifier", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        const int fogId = data->GetSimHandle(args[0].intVal, JassHandleInfo::FOG_MODIFIER);
        if (fogId >= 0 && sim) sim->GetFogSystem()->RemoveModifier(fogId);
        if (fogId >= 0) data->FreeHandle(args[0].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("FogModifierStart", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Null();
        const int fogId = data->GetSimHandle(args[0].intVal, JassHandleInfo::FOG_MODIFIER);
        if (fogId >= 0) sim->GetFogSystem()->SetModifierEnabled(fogId, true);
        return JassValue::Null();
    });
    vm_.RegisterNative("FogModifierStop", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Null();
        const int fogId = data->GetSimHandle(args[0].intVal, JassHandleInfo::FOG_MODIFIER);
        if (fogId >= 0) sim->GetFogSystem()->SetModifierEnabled(fogId, false);
        return JassValue::Null();
    });
    // Script terrain fog (ref SetTerrainFogEx/ResetTerrainFog): state lives
    // in data->terrainFog and is pushed to the renderer through
    // terrainFogHook (game controller -> DayNightCycle; null when headless).
    auto notifyTerrainFog = [data = data_]() {
        if (data && data->terrainFogHook) data->terrainFogHook();
    };
    auto numVal = [](const JassValue& v) -> float {
        return v.type == JassValue::REAL ? v.realVal : (float)v.intVal;
    };
    vm_.RegisterNative("SetTerrainFog", [data = data_, notifyTerrainFog, numVal](const std::vector<JassValue>& args) -> JassValue {
        if (!data || args.size() < 5) return JassValue::Null();
        // Legacy form is undocumented and ignored by retail (broken since
        // ~2011 per community native lists). Best effort: positions match
        // Ex's zstart/zend slots, so drive linear fog from a/b with c as
        // density and keep the current color.
        auto& fog = data->terrainFog;
        fog.overridden = true;
        fog.style = 0;
        fog.start = numVal(args[0]);
        fog.end = numVal(args[1]);
        fog.density = std::max(0.0f, numVal(args[2]));
        notifyTerrainFog();
        return JassValue::Null();
    });
    vm_.RegisterNative("ResetTerrainFog", [data = data_, notifyTerrainFog](const std::vector<JassValue>& args) -> JassValue {
        (void)args;
        if (!data) return JassValue::Null();
        // Ref resetTerrainFog: drop the override; the hook restores the w3i
        // baseline (defaultFogSettings equivalent).
        data->terrainFog.overridden = false;
        notifyTerrainFog();
        return JassValue::Null();
    });
    vm_.RegisterNative("SetFogStateRect", [data = data_, sim = sim_, fogPlayers, fogStateOf](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 7 || !sim) return JassValue::Null();
        const int player = ResolvePlayerId(data, args[0].intVal);
        if (player < 0) return JassValue::Null();
        const CFogArea area = CFogArea::Rect(args[2].realVal, args[3].realVal,
                                             args[4].realVal, args[5].realVal);
        for (int p : fogPlayers(player, args[6].boolVal)) {
            sim->GetFogSystem()->AddOverride(p, area, fogStateOf(args[1].intVal));
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetFogStateRadius", [data = data_, sim = sim_, fogPlayers, fogStateOf](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 6 || !sim) return JassValue::Null();
        const int player = ResolvePlayerId(data, args[0].intVal);
        if (player < 0) return JassValue::Null();
        const CFogArea area = CFogArea::Circle(args[2].realVal, args[3].realVal, args[4].realVal);
        for (int p : fogPlayers(player, args[5].boolVal)) {
            sim->GetFogSystem()->AddOverride(p, area, fogStateOf(args[1].intVal));
        }
        return JassValue::Null();
    });
    auto fogVisibleTo = [data = data_, sim = sim_](int player, float x, float y) {
        if (!sim || player < 0) return true;
        if (!data->fog.fogEnabled) return true;
        return sim->GetFogSystem()->IsVisibleTo(player, x, y);
    };
    auto fogMaskedTo = [data = data_, sim = sim_](int player, float x, float y) {
        if (!sim || player < 0) return false;
        if (!data->fog.fogEnabled || !data->fog.maskEnabled) return false;
        return sim->GetFogSystem()->IsMaskedTo(player, x, y);
    };
    vm_.RegisterNative("IsLocationVisibleToPlayer", [data = data_, fogVisibleTo](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(true);
        const int player = ResolvePlayerId(data, args[0].intVal);
        const int locSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        auto it = data->locations.find(locSim);
        if (it == data->locations.end()) return JassValue::Bool(true);
        return JassValue::Bool(fogVisibleTo(player, it->second->x, it->second->y));
    });
    vm_.RegisterNative("IsLocationMaskedToPlayer", [data = data_, fogMaskedTo](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        const int player = ResolvePlayerId(data, args[0].intVal);
        const int locSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        auto it = data->locations.find(locSim);
        if (it == data->locations.end()) return JassValue::Bool(false);
        return JassValue::Bool(fogMaskedTo(player, it->second->x, it->second->y));
    });
    vm_.RegisterNative("FogMaskEnable", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->fog.maskEnabled = args[0].boolVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("FogEnable", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->fog.fogEnabled = args[0].boolVal;
        return JassValue::Null();
    });
}

// ============================================================
// 23. Weather & Lightning natives
// ============================================================
void JassNatives::RegisterWeatherNatives() {
    vm_.RegisterNative("AddWeatherEffect", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        auto fx = std::make_shared<JassNativesData::JassWeather>();
        if (args.size() >= 2) {
            const int rid = data->GetSimHandle(args[0].intVal, JassHandleInfo::RECT);
            const auto rit = data->rects.find(rid);
            if (rit != data->rects.end() && rit->second) {
                fx->minX = rit->second->minX;
                fx->minY = rit->second->minY;
                fx->maxX = rit->second->maxX;
                fx->maxY = rit->second->maxY;
            }
            fx->code = War3ID((uint32_t)args[1].intVal);
        }
        const int id = data->nextHandleId++;
        data->weathers[id] = fx;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::WEATHER, id));
    });
    vm_.RegisterNative("RemoveWeatherEffect", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::WEATHER);
        if (id >= 0) {
            data->weathers.erase(id);
            data->FreeHandle(args[0].intVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("EnableWeatherEffect", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::WEATHER);
        const auto it = data->weathers.find(id);
        if (it != data->weathers.end() && it->second) {
            it->second->enabled = args[1].boolVal;
        }
        return JassValue::Null();
    });
}

void JassNatives::RegisterLightningNatives() {
    vm_.RegisterNative("AddLightning", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        auto bolt = std::make_shared<JassNativesData::JassLightning>();
        if (args.size() >= 6) {
            bolt->code = args[0].strVal;
            bolt->checkVisibility = args[1].boolVal;
            bolt->x1 = args[2].realVal; bolt->y1 = args[3].realVal;
            bolt->x2 = args[4].realVal; bolt->y2 = args[5].realVal;
        }
        const int id = data->nextHandleId++;
        data->lightnings[id] = bolt;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::LIGHTNING, id));
    });
    vm_.RegisterNative("AddLightningEx", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        auto bolt = std::make_shared<JassNativesData::JassLightning>();
        if (args.size() >= 8) {
            bolt->code = args[0].strVal;
            bolt->checkVisibility = args[1].boolVal;
            bolt->x1 = args[2].realVal; bolt->y1 = args[3].realVal; bolt->z1 = args[4].realVal;
            bolt->x2 = args[5].realVal; bolt->y2 = args[6].realVal; bolt->z2 = args[7].realVal;
        }
        const int id = data->nextHandleId++;
        data->lightnings[id] = bolt;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::LIGHTNING, id));
    });
    vm_.RegisterNative("DestroyLightning", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::LIGHTNING);
        if (id >= 0) {
            data->lightnings.erase(id);
            data->FreeHandle(args[0].intVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("MoveLightning", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 6) return JassValue::Bool(false);
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::LIGHTNING);
        if (id < 0) return JassValue::Bool(false);
        auto it = data->lightnings.find(id);
        if (it == data->lightnings.end()) return JassValue::Bool(false);
        it->second->checkVisibility = args[1].boolVal;
        it->second->x1 = args[2].realVal; it->second->y1 = args[3].realVal;
        it->second->x2 = args[4].realVal; it->second->y2 = args[5].realVal;
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("MoveLightningEx", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 8) return JassValue::Bool(false);
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::LIGHTNING);
        if (id < 0) return JassValue::Bool(false);
        auto it = data->lightnings.find(id);
        if (it == data->lightnings.end()) return JassValue::Bool(false);
        it->second->checkVisibility = args[1].boolVal;
        it->second->x1 = args[2].realVal; it->second->y1 = args[3].realVal; it->second->z1 = args[4].realVal;
        it->second->x2 = args[5].realVal; it->second->y2 = args[6].realVal; it->second->z2 = args[7].realVal;
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("GetLightningColor", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Real(1.0f); });
    auto lightningColor = [data = data_](const std::vector<JassValue>& args, float JassNativesData::JassLightning::*field) -> JassValue {
        if (args.empty()) return JassValue::Real(1.0f);
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::LIGHTNING);
        if (id < 0) return JassValue::Real(1.0f);
        auto it = data->lightnings.find(id);
        if (it == data->lightnings.end()) return JassValue::Real(1.0f);
        return JassValue::Real(it->second.get()->*field);
    };
    vm_.RegisterNative("GetLightningColorA", [lightningColor](const std::vector<JassValue>& args) -> JassValue {
        return lightningColor(args, &JassNativesData::JassLightning::a);
    });
    vm_.RegisterNative("GetLightningColorR", [lightningColor](const std::vector<JassValue>& args) -> JassValue {
        return lightningColor(args, &JassNativesData::JassLightning::r);
    });
    vm_.RegisterNative("GetLightningColorG", [lightningColor](const std::vector<JassValue>& args) -> JassValue {
        return lightningColor(args, &JassNativesData::JassLightning::g);
    });
    vm_.RegisterNative("GetLightningColorB", [lightningColor](const std::vector<JassValue>& args) -> JassValue {
        return lightningColor(args, &JassNativesData::JassLightning::b);
    });
    vm_.RegisterNative("SetLightningColor", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Bool(false);
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::LIGHTNING);
        if (id < 0) return JassValue::Bool(false);
        auto it = data->lightnings.find(id);
        if (it == data->lightnings.end()) return JassValue::Bool(false);
        it->second->r = args[1].realVal;
        it->second->g = args[2].realVal;
        it->second->b = args[3].realVal;
        it->second->a = args[4].realVal;
        return JassValue::Bool(true);
    });
}

// ============================================================
// 24. Camera natives
// ============================================================
void JassNatives::RegisterCameraNatives() {
    vm_.RegisterNative("CreateCameraSetup", [data = data_](const std::vector<JassValue>&) -> JassValue {
        if (!data) return JassValue::Int(0);
        int id = data->nextCameraSetupId++;
        data->cameraSetups[id] = std::make_shared<JassNativesData::JassCameraSetup>();
        return JassValue::Int(data->AllocHandle(JassHandleInfo::CAMERA_SETUP, id));
    });
    vm_.RegisterNative("CameraSetupSetField", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::CAMERA_SETUP);
        if (simId >= 0) {
            auto it = data->cameraSetups.find(simId);
            if (it != data->cameraSetups.end()) it->second->fields[args[1].intVal] = args[2].realVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("CameraSetupGetField", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::CAMERA_SETUP);
        if (simId >= 0) {
            auto it = data->cameraSetups.find(simId);
            if (it != data->cameraSetups.end()) {
                auto fit = it->second->fields.find(args[1].intVal);
                if (fit != it->second->fields.end()) return JassValue::Real(fit->second);
            }
        }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("CameraSetupSetDestPosition", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::CAMERA_SETUP);
        if (simId >= 0) {
            auto it = data->cameraSetups.find(simId);
            if (it != data->cameraSetups.end()) {
                it->second->destX = args[1].realVal;
                it->second->destY = args[2].realVal;
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("CameraSetupGetDestPositionLoc", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::CAMERA_SETUP);
        if (simId < 0) return JassValue::Int(0);
        auto it = data->cameraSetups.find(simId);
        if (it == data->cameraSetups.end()) return JassValue::Int(0);
        auto loc = std::make_shared<JassLocation>();
        loc->x = it->second->destX;
        loc->y = it->second->destY;
        int id = data->nextLocId++;
        data->locations[id] = loc;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::LOCATION, id));
    });
    vm_.RegisterNative("CameraSetupGetDestPositionX", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::CAMERA_SETUP);
        if (simId >= 0) {
            auto it = data->cameraSetups.find(simId);
            if (it != data->cameraSetups.end()) return JassValue::Real(it->second->destX);
        }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("CameraSetupGetDestPositionY", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::CAMERA_SETUP);
        if (simId >= 0) {
            auto it = data->cameraSetups.find(simId);
            if (it != data->cameraSetups.end()) return JassValue::Real(it->second->destY);
        }
        return JassValue::Real(0);
    });
    // Apply routes the stored destination through the engine camera
    // callbacks (ref CameraManager.applyCameraSetup). ForPlayer variants
    // only apply for the local player, matching the reference.
    auto applyCameraSetup = [data = data_](int setupHandle, float zOffset) {
        if (!data) return;
        int simId = data->GetSimHandle(setupHandle, JassHandleInfo::CAMERA_SETUP);
        if (simId < 0) return;
        auto it = data->cameraSetups.find(simId);
        if (it == data->cameraSetups.end()) return;
        if (data->camSetTarget) data->camSetTarget(it->second->destX, zOffset, it->second->destY);
    };
    vm_.RegisterNative("CameraSetupApply", [data = data_, applyCameraSetup](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 1) return JassValue::Null();
        applyCameraSetup(args[0].intVal, 0.0f);
        return JassValue::Null();
    });
    vm_.RegisterNative("CameraSetupApplyWithZ", [data = data_, applyCameraSetup](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        applyCameraSetup(args[0].intVal, args[1].realVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("CameraSetupApplyForceDuration", [data = data_, applyCameraSetup](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        applyCameraSetup(args[0].intVal, 0.0f);
        return JassValue::Null();
    });
    vm_.RegisterNative("CameraSetupApplyForceDurationWithZ", [data = data_, applyCameraSetup](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        const float zOffset = args.size() >= 4 ? args[3].realVal : 0.0f;
        applyCameraSetup(args[0].intVal, zOffset);
        return JassValue::Null();
    });
    vm_.RegisterNative("CameraSetupApplyForPlayer", [data = data_, applyCameraSetup](const std::vector<JassValue>& args) -> JassValue {
        // (doPan, setup, player, duration) per common.j.
        if (args.size() < 3 || !data) return JassValue::Null();
        if (ResolvePlayerId(data, args[2].intVal) == data->localPlayerIndex) {
            applyCameraSetup(args[1].intVal, 0.0f);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("CameraSetupApplyForPlayerWithZ", [data = data_, applyCameraSetup](const std::vector<JassValue>& args) -> JassValue {
        // (doPan, setup, player, duration, zOffset) per common.j.
        if (args.size() < 4 || !data) return JassValue::Null();
        if (ResolvePlayerId(data, args[2].intVal) == data->localPlayerIndex) {
            applyCameraSetup(args[1].intVal, args.size() >= 5 ? args[4].realVal : 0.0f);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetCameraPosition", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() >= 3 && data->camSetPosition)
            data->camSetPosition(args[0].realVal, args[1].realVal, args[2].realVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("SetCameraLookAt", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() >= 3 && data->camSetTarget)
            data->camSetTarget(args[0].realVal, args[1].realVal, args[2].realVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("SetCameraBounds", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        // Ref: 8 corner coords collapse to a min/max rect.
        if (args.size() < 8 || !data) return JassValue::Null();
        float minX = args[0].realVal, maxX = args[0].realVal;
        float minY = args[1].realVal, maxY = args[1].realVal;
        for (size_t i = 1; i < 4; ++i) {
            const float x = args[i * 2].realVal;
            const float y = args[i * 2 + 1].realVal;
            minX = std::min(minX, x); maxX = std::max(maxX, x);
            minY = std::min(minY, y); maxY = std::max(maxY, y);
        }
        data->cameraBounds.minX = minX;
        data->cameraBounds.minY = minY;
        data->cameraBounds.maxX = maxX;
        data->cameraBounds.maxY = maxY;
        return JassValue::Null();
    });
    vm_.RegisterNative("PanCameraTo", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() >= 2 && data->camSetTarget)
            data->camSetTarget(args[0].realVal, 0, args[1].realVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("PanCameraToTimed", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() >= 3 && data->camSetTarget)
            data->camSetTarget(args[0].realVal, 0, args[1].realVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("GetCameraTargetPositionX", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data->camGetX ? data->camGetX() : 0);
    });
    vm_.RegisterNative("GetCameraTargetPositionY", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data->camGetY ? data->camGetY() : 0);
    });
    vm_.RegisterNative("GetCameraTargetPositionZ", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data->camGetZ ? data->camGetZ() : 0);
    });
    vm_.RegisterNative("GetCameraTargetPositionLoc", [data = data_](const std::vector<JassValue>&) -> JassValue {
        auto loc = std::make_shared<JassLocation>();
        loc->x = data->camGetX ? data->camGetX() : 0.0f;
        loc->y = data->camGetY ? data->camGetY() : 0.0f;
        const int id = data->nextLocId++;
        data->locations[id] = loc;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::LOCATION, id));
    });
    vm_.RegisterNative("GetCameraEyePositionX", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data->camGetEyeX ? data->camGetEyeX() : 0);
    });
    vm_.RegisterNative("GetCameraEyePositionY", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data->camGetEyeY ? data->camGetEyeY() : 0);
    });
    vm_.RegisterNative("GetCameraEyePositionZ", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data->camGetEyeZ ? data->camGetEyeZ() : 0);
    });
    vm_.RegisterNative("GetCameraEyePositionLoc", [data = data_](const std::vector<JassValue>&) -> JassValue {
        auto loc = std::make_shared<JassLocation>();
        loc->x = data->camGetEyeX ? data->camGetEyeX() : 0.0f;
        loc->y = data->camGetEyeY ? data->camGetEyeY() : 0.0f;
        const int id = data->nextLocId++;
        data->locations[id] = loc;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::LOCATION, id));
    });
    vm_.RegisterNative("GetCameraBoundMinX", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data ? data->cameraBounds.minX : 0);
    });
    vm_.RegisterNative("GetCameraBoundMinY", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data ? data->cameraBounds.minY : 0);
    });
    vm_.RegisterNative("GetCameraBoundMaxX", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data ? data->cameraBounds.maxX : 0);
    });
    vm_.RegisterNative("GetCameraBoundMaxY", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data ? data->cameraBounds.maxY : 0);
    });
    vm_.RegisterNative("GetCameraMargin", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data ? data->cameraBounds.margin : 0);
    });
    vm_.RegisterNative("GetCameraField", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Real(data->camGetField ? data->camGetField() : 0);
    });
    vm_.RegisterNative("SetCameraField", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() >= 2 && data->camSetField)
            data->camSetField(args[0].intVal, args[1].realVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("AdjustCameraField", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data || !data->camSetField) return JassValue::Null();
        const float current = data->camGetField ? data->camGetField() : 0.0f;
        data->camSetField(args[0].intVal, current + args[1].realVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("ResetToGameCamera", [data = data_](const std::vector<JassValue>&) -> JassValue {
        if (data->camReset) data->camReset();
        return JassValue::Null();
    });
    vm_.RegisterNative("StopCamera", [data = data_](const std::vector<JassValue>&) -> JassValue {
        if (data) data->cameraStopped = true;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetCinematicCamera", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->cinematicCameraModel = args[0].strVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetSkyModel", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->sky.model = args[0].strVal;
        return JassValue::Null();
    });
}

// ============================================================
// 25. Game natives
// ============================================================

void JassNatives::RegisterTerrainQueryNatives() {
    // Retail pathingtype ordinals (common.j ConvertPathingType 0..7).
    static constexpr int kPathAny = 0, kPathWalk = 1, kPathFly = 2, kPathBuild = 3;
    static constexpr int kPathPeon = 4, kPathBlight = 5, kPathFloat = 6, kPathAmphib = 7;

    vm_.RegisterNative("GetTerrainType", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (args.size() < 2 || !sim) return JassValue::Int(0);
        const W3EMap* map = sim->GetTerrainMap();
        if (!map) return JassValue::Int(0);
        const W3ETileCorner* corner = W3ECornerAt(*map, args[0].realVal, args[1].realVal);
        if (!corner) return JassValue::Int(0);
        const int paletteIndex = corner->GroundTexture();
        if (paletteIndex < 0 || paletteIndex >= (int)map->groundTilePalette.size()) return JassValue::Int(0);
        return JassValue::Int((int)map->groundTilePalette[(size_t)paletteIndex]);
    });
    vm_.RegisterNative("GetTerrainVariance", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (args.size() < 2 || !sim) return JassValue::Int(0);
        const W3EMap* map = sim->GetTerrainMap();
        if (!map) return JassValue::Int(0);
        const W3ETileCorner* corner = W3ECornerAt(*map, args[0].realVal, args[1].realVal);
        return JassValue::Int(corner ? corner->GroundVariation() : 0);
    });
    vm_.RegisterNative("GetTerrainCliffLevel", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (args.size() < 2 || !sim) return JassValue::Int(0);
        const W3EMap* map = sim->GetTerrainMap();
        if (!map) return JassValue::Int(0);
        const W3ETileCorner* corner = W3ECornerAt(*map, args[0].realVal, args[1].realVal);
        return JassValue::Int(corner ? corner->LayerHeight() : 0);
    });
    vm_.RegisterNative("IsTerrainPathable", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (args.size() < 3 || !sim) return JassValue::Bool(false);
        const float x = args[0].realVal, y = args[1].realVal;
        const int type = args[2].intVal;
        const W3EMap* map = sim->GetTerrainMap();
        const W3ETileCorner* corner = map ? W3ECornerAt(*map, x, y) : nullptr;
        const bool inMap = corner != nullptr;
        const bool water = inMap && corner->IsWater();
        const bool boundary = inMap && corner->IsBoundary();
        auto walkable = [&]() -> bool {
            if (!inMap) return false;
            if (sim->GetPathfinding() && sim->GetPathfinding()->HasPathingMap()) {
                // WPM decides: shallow/shore water sets the W3E render-water
                // bit while staying FOOT-pathable, so the tile flag must not
                // gate here (it stays the mapless fallback below). The WPM
                // boundary ring already fails inside IsCellPathableFor.
                return sim->GetPathfinding()->IsWorldPointPathableForUnit(x, y, 0.0f, PathingGridMovementType::FOOT, nullptr);
            }
            if (water || boundary) return false;
            return true;
        };
        // WPM-backed float/fly checks (deep-water UNSWIMABLE / UNFLYABLE
        // bits, not the W3E render-water bit); mapless falls back to tile
        // flags like before.
        const bool hasPath = sim->GetPathfinding() && sim->GetPathfinding()->HasPathingMap();
        auto wpmPathable = [&](PathingGridMovementType movement) -> bool {
            return hasPath && sim->GetPathfinding()->IsWorldPointPathableForUnit(
                       x, y, 0.0f, movement, nullptr);
        };
        switch (type) {
            case kPathAny: return JassValue::Bool(walkable() || (inMap && !boundary));
            case kPathWalk:
            case kPathPeon: return JassValue::Bool(walkable());
            case kPathFly: return JassValue::Bool(hasPath ? (wpmPathable(PathingGridMovementType::FLY) && !boundary) : (inMap && !boundary));
            case kPathBuild: return JassValue::Bool(walkable() && !sim->IsPointBlighted(x, y));
            case kPathBlight: return JassValue::Bool(sim->IsPointBlighted(x, y));
            case kPathFloat: return JassValue::Bool(hasPath ? (wpmPathable(PathingGridMovementType::FLOAT) && !boundary) : (inMap && water && !boundary));
            case kPathAmphib: return JassValue::Bool(hasPath ? (wpmPathable(PathingGridMovementType::AMPHIBIOUS) && !boundary) : (walkable() || (inMap && water && !boundary)));
            default: return JassValue::Bool(false);
        }
    });
    vm_.RegisterNative("IsPointBlighted", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (args.size() < 2 || !sim) return JassValue::Bool(false);
        return JassValue::Bool(sim->IsPointBlighted(args[0].realVal, args[1].realVal));
    });
    // Retail takes the owning player first; blight is global in the sim.
    vm_.RegisterNative("SetBlight", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (args.size() < 5 || !sim) return JassValue::Null();
        sim->SetBlightCircle(args[1].realVal, args[2].realVal, args[3].realVal, args[4].boolVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("SetBlightRect", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Null();
        int rectSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::RECT);
        if (rectSim < 0) return JassValue::Null();
        auto it = data->rects.find(rectSim);
        if (it == data->rects.end()) return JassValue::Null();
        const JassRect& r = *it->second;
        sim->SetBlightRect(r.minX, r.minY, r.maxX, r.maxY, args[2].boolVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("SetBlightPoint", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (args.size() < 5 || !sim) return JassValue::Null();
        // Ref setBlight(x, y, radius, blighted): circular paint.
        sim->SetBlightCircle(args[1].realVal, args[2].realVal, args[3].realVal, args[4].boolVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("SetBlightLoc", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !sim) return JassValue::Null();
        int locSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        if (locSim < 0) return JassValue::Null();
        auto it = data->locations.find(locSim);
        if (it == data->locations.end()) return JassValue::Null();
        sim->SetBlightCircle(it->second->x, it->second->y, args[2].realVal, args[3].boolVal);
        return JassValue::Null();
    });

    vm_.RegisterNative("SetRect", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Null();
        int rectSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::RECT);
        if (rectSim < 0) return JassValue::Null();
        auto it = data->rects.find(rectSim);
        if (it == data->rects.end()) return JassValue::Null();
        JassRect& r = *it->second;
        r.minX = args[1].realVal; r.minY = args[2].realVal;
        r.maxX = args[3].realVal; r.maxY = args[4].realVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetRectFromLoc", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int rectSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::RECT);
        int minSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        int maxSim = data->GetSimHandle(args[2].intVal, JassHandleInfo::LOCATION);
        if (rectSim < 0 || minSim < 0 || maxSim < 0) return JassValue::Null();
        auto rit = data->rects.find(rectSim);
        auto minIt = data->locations.find(minSim), maxIt = data->locations.find(maxSim);
        if (rit == data->rects.end() || minIt == data->locations.end() || maxIt == data->locations.end()) {
            return JassValue::Null();
        }
        JassRect& r = *rit->second;
        r.minX = std::min(minIt->second->x, maxIt->second->x);
        r.minY = std::min(minIt->second->y, maxIt->second->y);
        r.maxX = std::max(minIt->second->x, maxIt->second->x);
        r.maxY = std::max(minIt->second->y, maxIt->second->y);
        return JassValue::Null();
    });
    vm_.RegisterNative("MoveRectTo", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int rectSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::RECT);
        if (rectSim < 0) return JassValue::Null();
        auto it = data->rects.find(rectSim);
        if (it == data->rects.end()) return JassValue::Null();
        JassRect& r = *it->second;
        const float halfW = (r.maxX - r.minX) * 0.5f;
        const float halfH = (r.maxY - r.minY) * 0.5f;
        r.minX = args[1].realVal - halfW; r.maxX = args[1].realVal + halfW;
        r.minY = args[2].realVal - halfH; r.maxY = args[2].realVal + halfH;
        return JassValue::Null();
    });

    static constexpr float kRegionCellSize = 32.0f;
    vm_.RegisterNative("RegionAddCell", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int rSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::REGION);
        if (rSim < 0) return JassValue::Null();
        auto it = data->regions.find(rSim);
        if (it == data->regions.end()) return JassValue::Null();
        JassRect cell;
        cell.minX = args[1].realVal * kRegionCellSize;
        cell.minY = args[2].realVal * kRegionCellSize;
        cell.maxX = cell.minX + kRegionCellSize;
        cell.maxY = cell.minY + kRegionCellSize;
        it->second->cells.push_back(cell);
        return JassValue::Null();
    });
    vm_.RegisterNative("RegionAddCellAtLoc", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int rSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::REGION);
        int locSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        if (rSim < 0 || locSim < 0) return JassValue::Null();
        auto rit = data->regions.find(rSim);
        auto lit = data->locations.find(locSim);
        if (rit == data->regions.end() || lit == data->locations.end()) return JassValue::Null();
        JassRect cell;
        cell.minX = std::floor(lit->second->x / kRegionCellSize) * kRegionCellSize;
        cell.minY = std::floor(lit->second->y / kRegionCellSize) * kRegionCellSize;
        cell.maxX = cell.minX + kRegionCellSize;
        cell.maxY = cell.minY + kRegionCellSize;
        rit->second->cells.push_back(cell);
        return JassValue::Null();
    });
    vm_.RegisterNative("RegionClearCell", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int rSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::REGION);
        if (rSim < 0) return JassValue::Null();
        auto it = data->regions.find(rSim);
        if (it == data->regions.end()) return JassValue::Null();
        const float x0 = args[1].realVal * kRegionCellSize;
        const float y0 = args[2].realVal * kRegionCellSize;
        auto& cells = it->second->cells;
        cells.erase(std::remove_if(cells.begin(), cells.end(), [&](const JassRect& c) {
            return c.minX == x0 && c.minY == y0;
        }), cells.end());
        return JassValue::Null();
    });
    vm_.RegisterNative("RegionClearCellAtLoc", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int rSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::REGION);
        int locSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        if (rSim < 0 || locSim < 0) return JassValue::Null();
        auto rit = data->regions.find(rSim);
        auto lit = data->locations.find(locSim);
        if (rit == data->regions.end() || lit == data->locations.end()) return JassValue::Null();
        const float x0 = std::floor(lit->second->x / kRegionCellSize) * kRegionCellSize;
        const float y0 = std::floor(lit->second->y / kRegionCellSize) * kRegionCellSize;
        auto& cells = rit->second->cells;
        cells.erase(std::remove_if(cells.begin(), cells.end(), [&](const JassRect& c) {
            return c.minX == x0 && c.minY == y0;
        }), cells.end());
        return JassValue::Null();
    });
    vm_.RegisterNative("IsPointInRegion", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Bool(false);
        int rSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::REGION);
        if (rSim < 0) return JassValue::Bool(false);
        auto it = data->regions.find(rSim);
        if (it == data->regions.end()) return JassValue::Bool(false);
        return JassValue::Bool(IsPointInJassRegion(*it->second, args[1].realVal, args[2].realVal));
    });
    vm_.RegisterNative("IsLocationInRegion", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        int rSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::REGION);
        int locSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        if (rSim < 0 || locSim < 0) return JassValue::Bool(false);
        auto rit = data->regions.find(rSim);
        auto lit = data->locations.find(locSim);
        if (rit == data->regions.end() || lit == data->locations.end()) return JassValue::Bool(false);
        return JassValue::Bool(IsPointInJassRegion(*rit->second, lit->second->x, lit->second->y));
    });
    vm_.RegisterNative("GetWorldBounds", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)args;
        auto rect = std::make_shared<JassRect>();
        if (sim) {
            if (const W3EMap* map = sim->GetTerrainMap()) {
                rect->minX = map->offsetX;
                rect->minY = map->offsetY;
                rect->maxX = map->offsetX + (float)map->tileWidth * 128.0f;
                rect->maxY = map->offsetY + (float)map->tileHeight * 128.0f;
            }
        }
        int id = data->nextRectId++;
        data->rects[id] = rect;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::RECT, id));
    });
}

// ============================================================
// 26. Player / team / start-location natives
// ============================================================
void JassNatives::RegisterPlayerTeamNatives() {
    auto allPlayersForce = [data = data_, sim = sim_]() -> int {
        auto force = std::make_shared<JassForce>();
        if (sim) {
            for (int i = 0; i < sim->GetPlayerCount(); i++) {
                force->playerIds.push_back(i);
            }
        }
        int id = data->nextForceId++;
        data->forces[id] = force;
        return data->AllocHandle(JassHandleInfo::FORCE, id);
    };
    vm_.RegisterNative("GetPlayers", [allPlayersForce](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(allPlayersForce());
    });
    vm_.RegisterNative("GetTeams", [allPlayersForce](const std::vector<JassValue>&) -> JassValue {
        // Retail returns all slots as well; team subdivision is per-player.
        return JassValue::Int(allPlayersForce());
    });
    vm_.RegisterNative("GetPlayerTeam", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Int(0);
        if (CPlayer* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal))) {
            return JassValue::Int(p->GetTeam());
        }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("SetPlayerTaxRate", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        const int p1 = ResolvePlayerId(data, args[0].intVal);
        const int p2 = ResolvePlayerId(data, args[1].intVal);
        const int64_t key = ((int64_t)(p1 + 64) * 128 + (p2 + 64)) * 16 + (args[2].intVal & 15);
        data->playerTaxRates[key] = args[3].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("GetPlayerTaxRate", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Int(0);
        const int p1 = ResolvePlayerId(data, args[0].intVal);
        const int p2 = ResolvePlayerId(data, args[1].intVal);
        const int64_t key = ((int64_t)(p1 + 64) * 128 + (p2 + 64)) * 16 + (args[2].intVal & 15);
        auto it = data->playerTaxRates.find(key);
        return JassValue::Int(it != data->playerTaxRates.end() ? it->second : 0);
    });
    vm_.RegisterNative("GetPlayerSelectable", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Bool(true);
        const int p = ResolvePlayerId(data, args[0].intVal);
        auto it = data->playerSelectable.find(p);
        // No retail setter exists; slots are selectable unless recorded off.
        return JassValue::Bool(it != data->playerSelectable.end() ? it->second : true);
    });
    vm_.RegisterNative("GetStartLocPrio", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        (void)data; (void)args;
        // Start-location priority tables are not part of w3i; melee start
        // positions come from the config env. Default: no priority.
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetStartLocPrioSlot", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        (void)data; (void)args;
        return JassValue::Int(-1);
    });
    vm_.RegisterNative("GetUnitCurrentOrder", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Int(0);
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (uSim < 0) return JassValue::Int(0);
        CUnit* u = sim->GetUnit(uSim);
        return JassValue::Int(u ? u->GetLastOrderId() : 0);
    });
    vm_.RegisterNative("SetMapFlag", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->gameConfig.mapFlags |= (uint32_t)args[0].intVal;
        return JassValue::Null();
    });
}

// ============================================================
// 27. Visual natives: spell effects, images, trackables, ubersplats
// ============================================================
void JassNatives::RegisterVisualNatives() {
    auto makeEffect = [data = data_]() -> std::pair<int, std::shared_ptr<JassEffect>> {
        auto e = std::make_shared<JassEffect>();
        int id = data->nextEffectId++;
        data->effects[id] = e;
        return {data->AllocHandle(JassHandleInfo::EFFECT, id), e};
    };
    // Ability art lookup: ById natives resolve the model path from the
    // ability profile (ref ability art); string spell natives pass it.
    auto abilityArt = [data = data_, sim = sim_](int abilityId, int type) -> std::string {
        (void)data;
        if (!sim || type < 0 || type > 6) return "";
        const CAbilityData* abilityData = sim->GetAbilityData();
        if (!abilityData) return "";
        const CAbilityDefFull* def = abilityData->GetAbility(War3ID((uint32_t)abilityId));
        if (!def || type >= (int)def->effectArtByType.size()) return "";
        return def->effectArtByType[(size_t)type];
    };
    auto resolveWidget = [data = data_](int widgetHandle) -> int {
        // Spell targets may be units, items or destructables; first hit wins.
        // Stored for renderer consumption alongside the attach point.
        static const JassHandleInfo::Type kinds[] = {
            JassHandleInfo::UNIT, JassHandleInfo::ITEM, JassHandleInfo::DESTRUCTABLE,
        };
        for (auto kind : kinds) {
            int simId = data->GetSimHandle(widgetHandle, kind);
            if (simId >= 0) return simId;
        }
        return -1;
    };

    vm_.RegisterNative("AddSpellEffect", [makeEffect, abilityArt](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Int(0);
        auto [handle, e] = makeEffect();
        // Retail shows the ability's own art of type t (abilityString is a
        // 4-char code); a longer string is kept verbatim defensively.
        e->abilityId = RawcodeFromString(args[0].strVal);
        e->effectType = args[1].intVal;
        e->modelPath = args[0].strVal.size() == 4
            ? abilityArt(e->abilityId, e->effectType)
            : args[0].strVal;
        e->x = args[2].realVal;
        e->y = args[3].realVal;
        return JassValue::Int(handle);
    });
    vm_.RegisterNative("AddSpellEffectLoc", [data = data_, makeEffect, abilityArt](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Int(0);
        auto [handle, e] = makeEffect();
        e->abilityId = RawcodeFromString(args[0].strVal);
        e->effectType = args[1].intVal;
        e->modelPath = args[0].strVal.size() == 4
            ? abilityArt(e->abilityId, e->effectType)
            : args[0].strVal;
        int locSim = data->GetSimHandle(args[2].intVal, JassHandleInfo::LOCATION);
        if (locSim >= 0) {
            auto it = data->locations.find(locSim);
            if (it != data->locations.end()) {
                e->x = it->second->x;
                e->y = it->second->y;
            }
        }
        return JassValue::Int(handle);
    });
    vm_.RegisterNative("AddSpellEffectById", [makeEffect, abilityArt](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Int(0);
        auto [handle, e] = makeEffect();
        e->abilityId = args[0].intVal;
        e->effectType = args[1].intVal;
        e->modelPath = abilityArt(args[0].intVal, args[1].intVal);
        e->x = args[2].realVal;
        e->y = args[3].realVal;
        return JassValue::Int(handle);
    });
    vm_.RegisterNative("AddSpellEffectByIdLoc", [data = data_, makeEffect, abilityArt](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Int(0);
        auto [handle, e] = makeEffect();
        e->abilityId = args[0].intVal;
        e->effectType = args[1].intVal;
        e->modelPath = abilityArt(args[0].intVal, args[1].intVal);
        int locSim = data->GetSimHandle(args[2].intVal, JassHandleInfo::LOCATION);
        if (locSim >= 0) {
            auto it = data->locations.find(locSim);
            if (it != data->locations.end()) {
                e->x = it->second->x;
                e->y = it->second->y;
            }
        }
        return JassValue::Int(handle);
    });
    vm_.RegisterNative("AddSpellEffectTarget", [makeEffect, resolveWidget](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Int(0);
        auto [handle, e] = makeEffect();
        e->modelPath = args[0].strVal;
        e->effectType = args[1].intVal;
        e->targetSimHandle = resolveWidget(args[2].intVal);
        e->attachPoint = args[3].strVal;
        return JassValue::Int(handle);
    });
    vm_.RegisterNative("AddSpellEffectTargetById", [makeEffect, abilityArt, resolveWidget](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Int(0);
        auto [handle, e] = makeEffect();
        e->abilityId = args[0].intVal;
        e->effectType = args[1].intVal;
        e->modelPath = abilityArt(args[0].intVal, args[1].intVal);
        e->targetSimHandle = resolveWidget(args[2].intVal);
        e->attachPoint = args[3].strVal;
        return JassValue::Int(handle);
    });
    vm_.RegisterNative("GetAbilityEffect", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (args.size() < 3 || !sim) return JassValue::Str("");
        const CAbilityData* abilityData = sim->GetAbilityData();
        if (!abilityData) return JassValue::Str("");
        const CAbilityDefFull* def = abilityData->GetAbility(War3ID::FromString(args[0].strVal));
        if (!def) return JassValue::Str("");
        const int type = args[1].intVal;
        // Single art value per type in the Func profiles (no index lists).
        if (type < 0 || type >= (int)def->effectArtByType.size()) return JassValue::Str("");
        return JassValue::Str(def->effectArtByType[(size_t)type]);
    });
    vm_.RegisterNative("GetAbilityEffectById", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (args.size() < 3 || !sim) return JassValue::Str("");
        const CAbilityData* abilityData = sim->GetAbilityData();
        if (!abilityData) return JassValue::Str("");
        const CAbilityDefFull* def = abilityData->GetAbility(War3ID((uint32_t)args[0].intVal));
        if (!def) return JassValue::Str("");
        const int type = args[1].intVal;
        if (type < 0 || type >= (int)def->effectArtByType.size()) return JassValue::Str("");
        return JassValue::Str(def->effectArtByType[(size_t)type]);
    });
    vm_.RegisterNative("GetAbilitySound", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (args.size() < 3 || !sim) return JassValue::Str("");
        const CAbilityData* abilityData = sim->GetAbilityData();
        if (!abilityData) return JassValue::Str("");
        const CAbilityDefFull* def = abilityData->GetAbility(War3ID::FromString(args[0].strVal));
        // Func profiles carry a single Effectsound per ability (no per-type).
        return JassValue::Str(def ? def->effectSound : "");
    });
    vm_.RegisterNative("GetAbilitySoundById", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (args.size() < 3 || !sim) return JassValue::Str("");
        const CAbilityData* abilityData = sim->GetAbilityData();
        if (!abilityData) return JassValue::Str("");
        const CAbilityDefFull* def = abilityData->GetAbility(War3ID((uint32_t)args[0].intVal));
        return JassValue::Str(def ? def->effectSound : "");
    });

    vm_.RegisterNative("CreateImage", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 11) return JassValue::Int(0);
        auto img = std::make_shared<JassImage>();
        img->file = args[0].strVal;
        img->sizeX = args[1].realVal; img->sizeY = args[2].realVal; img->sizeZ = args[3].realVal;
        img->posX = args[4].realVal; img->posY = args[5].realVal; img->posZ = args[6].realVal;
        img->originX = args[7].realVal; img->originY = args[8].realVal; img->originZ = args[9].realVal;
        img->imageType = args[10].intVal;
        int id = data->nextImageId++;
        data->images[id] = img;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::IMAGE, id));
    });
    vm_.RegisterNative("DestroyImage", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::IMAGE);
        if (id >= 0) { data->images.erase(id); data->FreeHandle(args[0].intVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("ShowImage", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::IMAGE);
        if (id >= 0) {
            auto it = data->images.find(id);
            if (it != data->images.end()) it->second->visible = args[1].boolVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetImageRender", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::IMAGE);
        if (id >= 0) {
            auto it = data->images.find(id);
            if (it != data->images.end()) it->second->render = args[1].boolVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetImageRenderAlways", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::IMAGE);
        if (id >= 0) {
            auto it = data->images.find(id);
            if (it != data->images.end()) it->second->renderAlways = args[1].boolVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetImageAboveWater", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::IMAGE);
        if (id >= 0) {
            auto it = data->images.find(id);
            if (it != data->images.end()) it->second->aboveWater = args[1].boolVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetImageColor", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::IMAGE);
        if (id >= 0) {
            auto it = data->images.find(id);
            if (it != data->images.end()) {
                it->second->red = args[1].intVal;
                it->second->green = args[2].intVal;
                it->second->blue = args[3].intVal;
                it->second->alpha = args[4].intVal;
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetImageConstantHeight", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::IMAGE);
        if (id >= 0) {
            auto it = data->images.find(id);
            if (it != data->images.end()) it->second->constantHeight = args[1].boolVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetImagePosition", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::IMAGE);
        if (id >= 0) {
            auto it = data->images.find(id);
            if (it != data->images.end()) {
                it->second->posX = args[1].realVal;
                it->second->posY = args[2].realVal;
                it->second->posZ = args[3].realVal;
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetImageType", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::IMAGE);
        if (id >= 0) {
            auto it = data->images.find(id);
            if (it != data->images.end()) it->second->imageType = args[1].intVal;
        }
        return JassValue::Null();
    });

    vm_.RegisterNative("CreateTrackable", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Int(0);
        auto t = std::make_shared<JassTrackable>();
        t->modelPath = args[0].strVal;
        t->x = args[1].realVal;
        t->y = args[2].realVal;
        t->facing = args[3].realVal;
        int id = data->nextTrackableId++;
        const int handle = data->AllocHandle(JassHandleInfo::TRACKABLE, id);
        t->jassHandle = handle;
        data->trackables[id] = t;
        return JassValue::Int(handle);
    });
    // Retail has no DestroyTrackable; hit/track event interest is stored on
    // triggers below and fired once the render picking path exists.
    vm_.RegisterNative("TriggerRegisterTrackableHitEvent", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        // Triggers are keyed by raw id (ref CreateTrigger convention).
        if (data->GetSimHandle(args[1].intVal, JassHandleInfo::TRACKABLE) < 0) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it != data->triggers.end()) it->second->registeredEvents.emplace_back(args[1].intVal, 4101);
        return JassValue::Null();
    });
    vm_.RegisterNative("TriggerRegisterTrackableTrackEvent", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (data->GetSimHandle(args[1].intVal, JassHandleInfo::TRACKABLE) < 0) return JassValue::Null();
        auto it = data->triggers.find(args[0].intVal);
        if (it != data->triggers.end()) it->second->registeredEvents.emplace_back(args[1].intVal, 4102);
        return JassValue::Null();
    });
    vm_.RegisterNative("GetTriggeringTrackable", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Int(data->eventTrackableHandle > 0 ? data->eventTrackableHandle : 0);
    });

    vm_.RegisterNative("CreateUbersplat", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 9) return JassValue::Int(0);
        auto s = std::make_shared<JassUbersplat>();
        s->x = args[0].realVal;
        s->y = args[1].realVal;
        s->name = args[2].strVal;
        s->red = args[3].intVal;
        s->green = args[4].intVal;
        s->blue = args[5].intVal;
        s->alpha = args[6].intVal;
        s->forcePaused = args[7].boolVal;
        s->noBirthTime = args[8].boolVal;
        int id = data->nextUbersplatId++;
        data->ubersplats[id] = s;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::UBERSPLAT, id));
    });
    vm_.RegisterNative("DestroyUbersplat", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::UBERSPLAT);
        if (id >= 0) { data->ubersplats.erase(id); data->FreeHandle(args[0].intVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("FinishUbersplat", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::UBERSPLAT);
        if (id >= 0) {
            auto it = data->ubersplats.find(id);
            if (it != data->ubersplats.end()) it->second->finished = true;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("ResetUbersplat", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::UBERSPLAT);
        if (id >= 0) {
            auto it = data->ubersplats.find(id);
            if (it != data->ubersplats.end()) it->second->finished = false;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUbersplatRender", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::UBERSPLAT);
        if (id >= 0) {
            auto it = data->ubersplats.find(id);
            if (it != data->ubersplats.end()) it->second->render = args[1].boolVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUbersplatRenderAlways", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::UBERSPLAT);
        if (id >= 0) {
            auto it = data->ubersplats.find(id);
            if (it != data->ubersplats.end()) it->second->renderAlways = args[1].boolVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("ShowUbersplat", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::UBERSPLAT);
        if (id >= 0) {
            auto it = data->ubersplats.find(id);
            if (it != data->ubersplats.end()) it->second->render = args[1].boolVal;
        }
        return JassValue::Null();
    });
}

// ============================================================
// 28. Pool + shop-stock natives
// ============================================================
// Weighted roll shared by pools (ref pool placement). roll01 comes from

void JassNatives::RegisterCameraFlowNatives() {
    vm_.RegisterNative("CameraSetSourceNoise", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        // Retained for the renderer shake pass; no consumer yet.
        data->sourceNoise.magnitude = args[0].realVal;
        data->sourceNoise.velocity = args[1].realVal;
        data->sourceNoise.vertOnly = false;
        return JassValue::Null();
    });
    vm_.RegisterNative("CameraSetSourceNoiseEx", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        data->sourceNoise.magnitude = args[0].realVal;
        data->sourceNoise.velocity = args[1].realVal;
        data->sourceNoise.vertOnly = args[2].boolVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("CameraSetTargetNoise", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        data->targetNoise.magnitude = args[0].realVal;
        data->targetNoise.velocity = args[1].realVal;
        data->targetNoise.vertOnly = false;
        return JassValue::Null();
    });
    vm_.RegisterNative("CameraSetTargetNoiseEx", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        data->targetNoise.magnitude = args[0].realVal;
        data->targetNoise.velocity = args[1].realVal;
        data->targetNoise.vertOnly = args[2].boolVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("PanCameraToTimedWithZ", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        // Duration is renderer-side; the target callback takes the final pose.
        if (args.size() >= 3 && data->camSetTarget)
            data->camSetTarget(args[0].realVal, args[2].realVal, args[1].realVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("PanCameraToWithZ", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() >= 3 && data->camSetTarget)
            data->camSetTarget(args[0].realVal, args[2].realVal, args[1].realVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("SetCameraQuickPosition", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() >= 2 && data->camSetPosition)
            data->camSetPosition(args[0].realVal, 0, args[1].realVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("SetCameraRotateMode", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        // GameController consumes this via OrbitCamera::StepRotateAround.
        data->rotateMode.active = true;
        data->rotateMode.x = args[0].realVal;
        data->rotateMode.y = args[1].realVal;
        data->rotateMode.radians = args[2].realVal;
        data->rotateMode.duration = args[3].realVal;
        return JassValue::Null();
    });
    auto setController = [data = data_, sim = sim_](bool orient, const std::vector<JassValue>& args) -> JassValue {
        const size_t need = orient ? 3 : 4;
        if (args.size() < need || !sim) return JassValue::Null();
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        JassNativesData::JassCameraController ctl;
        if (uSim >= 0 && sim->GetUnit(uSim)) {
            ctl.unitSimId = uSim;
            ctl.xOffset = args[1].realVal;
            ctl.yOffset = args[2].realVal;
            ctl.inheritOrientation = !orient && args[3].boolVal;
            ctl.active = true;
        }
        if (orient) {
            data->orientController = ctl;
        } else {
            data->targetController = ctl;
        }
        return JassValue::Null();
    };
    vm_.RegisterNative("SetCameraTargetController", [setController](const std::vector<JassValue>& args) -> JassValue {
        return setController(false, args);
    });
    vm_.RegisterNative("SetCameraOrientController", [setController](const std::vector<JassValue>& args) -> JassValue {
        return setController(true, args);
    });

    vm_.RegisterNative("PauseGame", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (!args.empty() && sim) sim->SetPaused(args[0].boolVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("PauseCompAI", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        const int playerId = ResolvePlayerId(data, args[0].intVal);
        data->compAIpaused[playerId] = args[1].boolVal;
        auto it = data->aiControllers.find(playerId);
        if (it != data->aiControllers.end() && it->second) {
            it->second->SetEnabled(!args[1].boolVal);
        }
        if (sim && sim->GetSimulationModule()) {
            sim->GetSimulationModule()->SetPlayerAIEnabled(playerId, !args[1].boolVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetAIDifficulty", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Int(1);
        const int playerId = ResolvePlayerId(data, args[0].intVal);
        if (playerId >= 0 && playerId < 24) return JassValue::Int(data->playerAI[playerId].difficulty);
        return JassValue::Int(data->gameConfig.defaultDifficulty);
    });
    vm_.RegisterNative("EndGame", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (data) {
            data->gameEnded = true;
            if (!args.empty()) data->gameVictory = args[0].boolVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("CripplePlayer", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        const int player = ResolvePlayerId(data, args[0].intVal);
        if (args[2].boolVal) {
            data->crippledPlayers.insert(player);
        } else {
            data->crippledPlayers.erase(player);
        }
        if (sim) {
            if (CPlayer* p = sim->GetPlayer(player)) {
                p->SetCrippled(args[2].boolVal);
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("IsNoVictoryCheat", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Bool(data && data->noVictoryCheat);
    });
    vm_.RegisterNative("IsNoDefeatCheat", [data = data_](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Bool(data && data->noDefeatCheat);
    });
    vm_.RegisterNative("Cheat", [](const std::vector<JassValue>&) -> JassValue {
        // Single-player cheat codes are not implemented.
        return JassValue::Null();
    });
    vm_.RegisterNative("DisableRestartMission", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->restartDisabled = args[0].boolVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("RemovePlayer", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        const int player = ResolvePlayerId(data, args[0].intVal);
        // Sim player slots are index-stable, so removal only closes the
        // lobby slot; the sim unit stays addressable (documented).
        if (player >= 0 && player < (int)data->playerMapConfig.size()) {
            data->playerMapConfig[(size_t)player].slotActive = false;
        }
        data->removedPlayerResults[player] = args[1].intVal;
        if (sim) {
            if (CPlayer* p = sim->GetPlayer(player)) {
                p->SetSlotActive(false);
                p->SetGameResultCode(args[1].intVal);
            }
            if (auto* mod = sim->GetSimulationModule()) {
                mod->OnPlayerRemoved(*sim, player, args[1].intVal);
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetPlayerUnitsOwner", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        const int oldOwner = ResolvePlayerId(data, args[0].intVal);
        const int newOwner = args[1].intVal;
        CPlayer* from = sim->GetPlayer(oldOwner);
        CPlayer* to = sim->GetPlayer(newOwner);
        if (!from) return JassValue::Null();
        std::vector<CUnit*> moving;
        for (CUnit* u : sim->GetUnits()) {
            if (u && u->GetPlayerIndex() == oldOwner) moving.push_back(u);
        }
        for (CUnit* u : moving) {
            from->RemoveUnit(u);
            u->SetPlayerIndex(newOwner);
            if (to) to->AddUnit(u);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetGameTypeSupported", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (args[1].boolVal) {
            data->supportedGameTypes.insert(args[0].intVal);
        } else {
            data->supportedGameTypes.erase(args[0].intVal);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("IsGameTypeSupported", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Bool(false);
        return JassValue::Bool(data && data->supportedGameTypes.count(args[0].intVal) > 0);
    });
    vm_.RegisterNative("SetResourceDensity", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->resourceDensity = args[0].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetCreatureDensity", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->creatureDensity = args[0].intVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("VersionCompatible", [](const std::vector<JassValue>&) -> JassValue {
        // No version-gated behavior exists; all script versions run.
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("VersionSupported", [](const std::vector<JassValue>&) -> JassValue {
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("PlayCinematic", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (data) {
            data->cinematic.playing = true;
            if (!args.empty()) data->cinematic.movie = args[0].strVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetCinematicScene", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 6) return JassValue::Null();
        if (data) {
            data->cinematic.playing = true;
            data->cinematic.portraitUnitId = args[0].intVal;
            data->cinematic.portraitColor = args[1].intVal;
            data->cinematic.speakerTitle = args[2].strVal;
            data->cinematic.text = args[3].strVal;
            data->cinematic.sceneDuration = args[4].realVal;
            data->cinematic.voiceDuration = args[5].realVal;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("EndCinematicScene", [data = data_](const std::vector<JassValue>&) -> JassValue {
        if (data) data->cinematic.playing = false;
        return JassValue::Null();
    });
    vm_.RegisterNative("ForceCinematicSubtitles", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->cinematic.subtitles = args[0].boolVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetIntroShotModel", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->cinematic.introModel = args[0].strVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetIntroShotText", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!args.empty() && data) data->cinematic.introText = args[0].strVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("DisplayLoadDialog", [data = data_](const std::vector<JassValue>&) -> JassValue {
        if (data) data->cinematic.loadDialog = true;
        return JassValue::Null();
    });
    // Preload calls are asset-cache hints; loading here is synchronous so
    // they complete immediately with nothing to do.
    vm_.RegisterNative("Preload", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("PreloadEnd", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("PreloadEndEx", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("PreloadGenClear", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("PreloadGenStart", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("PreloadGenEnd", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("PreloadRefresh", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
    vm_.RegisterNative("PreloadStart", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); });
}

// ============================================================
// 31. Trigger extras + sound state + counted group enums
// ============================================================

void JassNatives::RegisterTerrainEditNatives() {
    vm_.RegisterNative("SetTerrainType", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (args.size() < 6 || !sim) return JassValue::Null();
        W3EMap* map = sim->GetTerrainMap();
        if (!map) return JassValue::Null();
        // Retail area is a tile radius, shape 0 = circle, 1 = square.
        const float radiusTiles = args[4].realVal;
        const bool square = args[5].intVal != 0;
        const uint32_t wantType = (uint32_t)args[2].intVal;
        int paletteIndex = -1;
        for (size_t i = 0; i < map->groundTilePalette.size(); i++) {
            if (map->groundTilePalette[i] == wantType) {
                paletteIndex = (int)i;
                break;
            }
        }
        if (paletteIndex < 0 || paletteIndex > 15) return JassValue::Null();
        const int variation = args[3].intVal & 31;
        const float radiusWorld = radiusTiles * 128.0f;
        bool touched = false;
        for (int cy = 0; cy < map->tileHeight; cy++) {
            for (int cx = 0; cx < map->tileWidth; cx++) {
                const float tx = map->offsetX + (cx + 0.5f) * 128.0f;
                const float ty = map->offsetY + (cy + 0.5f) * 128.0f;
                const float dx = tx - args[0].realVal, dy = ty - args[1].realVal;
                if (square) {
                    if (std::fabs(dx) > radiusWorld || std::fabs(dy) > radiusWorld) continue;
                } else if (dx * dx + dy * dy > radiusWorld * radiusWorld) {
                    continue;
                }
                W3ETileCorner& corner = map->tiles[(size_t)cy * (size_t)map->tileWidth + (size_t)cx];
                corner.textureBits = (uint8_t)((corner.textureBits & 0xF0) | (paletteIndex & 0x0F));
                corner.variationBits = (uint8_t)((corner.variationBits & 0xE0) | variation);
                touched = true;
            }
        }
        // The terrain render pass rebuilds when it observes this counter.
        if (touched) map->editVersion++;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetTerrainPathable", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (args.size() < 4 || !sim) return JassValue::Null();
        sim->SetTerrainPathableCell(args[0].realVal, args[1].realVal, args[2].intVal, args[3].boolVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("SetWaterDeforms", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (!args.empty() && sim) sim->SetWaterDeforms(args[0].boolVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("SetDoodadAnimation", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 7) return JassValue::Null();
        // Consumed by GameController -> SceneManager::PlayDoodadAnimation.
        JassNativesData::JassDoodadAnim req;
        req.x = args[0].realVal;
        req.y = args[1].realVal;
        req.radius = args[2].realVal;
        req.doodadId = (uint32_t)args[3].intVal;
        req.nearestOnly = args[4].boolVal;
        req.animName = args[5].strVal;
        req.animRandom = args[6].boolVal;
        data->doodadAnims.push_back(std::move(req));
        return JassValue::Null();
    });
    vm_.RegisterNative("TerrainDeformRipple", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 10) return JassValue::Int(0);
        // common.j: TerrainDeformRipple(x, y, radius, depth, duration, count,
        //   spaceWaves, timeWaves, radiusStart, limitNeg). Retain shape so the
        //   terrain pass (TerrainDeformOffset) actually lifts ground like crater.
        auto deform = std::make_shared<JassNativesData::JassTerrainDeform>();
        deform->kind = 1;
        deform->x = args[0].realVal;
        deform->y = args[1].realVal;
        deform->radius = args[2].realVal;
        deform->depth = args[3].realVal;
        deform->duration = args[4].intVal;
        deform->permanent = false;
        const int id = data->nextTerrainDeformId++;
        data->terrainDeforms[id] = deform;
        return JassValue::Int(id);
    });
    vm_.RegisterNative("TerrainDeformWave", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 10) return JassValue::Int(0);
        // common.j: TerrainDeformWave(x, y, dirX, dirY, distance, speed, duration,
        //   count, spaceWaves, timeWaves, radiusStart, limitNeg). Use distance as
        //   the affected radius and speed-scaled depth so offset is visible.
        auto deform = std::make_shared<JassNativesData::JassTerrainDeform>();
        deform->kind = 2;
        deform->x = args[0].realVal;
        deform->y = args[1].realVal;
        deform->radius = args[4].realVal > 0.0f ? args[4].realVal : 256.0f;
        deform->depth = args[5].realVal != 0.0f ? std::abs(args[5].realVal) * 0.5f : 64.0f;
        deform->duration = args.size() > 6 ? args[6].intVal : 0;
        deform->permanent = false;
        const int id = data->nextTerrainDeformId++;
        data->terrainDeforms[id] = deform;
        return JassValue::Int(id);
    });
    vm_.RegisterNative("TerrainDeformRandom", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 7) return JassValue::Int(0);
        // common.j: TerrainDeformRandom(x, y, radius, minDamage, maxDamage,
        //   updateInterval, ...). Damage range doubles as depth variation.
        auto deform = std::make_shared<JassNativesData::JassTerrainDeform>();
        deform->kind = 3;
        deform->x = args[0].realVal;
        deform->y = args[1].realVal;
        deform->radius = args[2].realVal;
        const float lo = args.size() > 3 ? (float)args[3].intVal : 0.0f;
        const float hi = args.size() > 4 ? (float)args[4].intVal : 0.0f;
        deform->depth = std::abs(hi - lo) > 0.0f ? std::abs(hi - lo) * 0.5f : 32.0f;
        deform->duration = 0;
        deform->permanent = false;
        const int id = data->nextTerrainDeformId++;
        data->terrainDeforms[id] = deform;
        return JassValue::Int(id);
    });
    vm_.RegisterNative("TerrainDeformStop", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        auto it = data->terrainDeforms.find(args[0].intVal);
        // Duration counts down in the terrain pass; marked stopped here.
        if (it != data->terrainDeforms.end()) it->second->stopped = true;
        return JassValue::Null();
    });
    vm_.RegisterNative("TerrainDeformStopAll", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        (void)args;
        for (auto& [id, deform] : data->terrainDeforms) {
            if (deform) deform->stopped = true;
        }
        return JassValue::Null();
    });
}


// ============================================================
// 35. Hashtable natives (ref hashtable API)
// ============================================================

void JassNatives::RegisterProjectileNatives() {
    auto getProj = [data = data_](int projHandle) -> std::shared_ptr<JassNativesData::JassProjectile> {
        if (!data) return nullptr;
        int simId = data->GetSimHandle(projHandle, JassHandleInfo::PROJECTILE);
        if (simId < 0) return nullptr;
        auto it = data->projectiles.find(simId);
        return it != data->projectiles.end() ? it->second : nullptr;
    };
    // Bind a target triple (unit/item/destructable/location/point-or-null)
    // onto a record. Units stay live-tracked; other widgets resolve to
    // their position once (non-homing), mirroring the effect-target rule.
    auto bindTarget = [data = data_, sim = sim_](const std::shared_ptr<JassNativesData::JassProjectile>& proj,
                                                 const JassValue& target) {
        if (!proj) return;
        const int uSim = data->GetSimHandle(target.intVal, JassHandleInfo::UNIT);
        if (uSim >= 0) {
            proj->hasUnitTarget = true;
            proj->targetSimId = uSim;
            return;
        }
        if (sim) {
            const int iSim = data->GetSimHandle(target.intVal, JassHandleInfo::ITEM);
            if (iSim >= 0 && sim->GetItem(iSim)) {
                proj->hasPointTarget = true;
                proj->tx = sim->GetItem(iSim)->GetX();
                proj->ty = sim->GetItem(iSim)->GetY();
                return;
            }
            const int dSim = data->GetSimHandle(target.intVal, JassHandleInfo::DESTRUCTABLE);
            if (dSim >= 0 && sim->GetDestructable(dSim)) {
                proj->hasPointTarget = true;
                proj->tx = sim->GetDestructable(dSim)->GetX();
                proj->ty = sim->GetDestructable(dSim)->GetY();
                return;
            }
        }
        const int locSim = data->GetSimHandle(target.intVal, JassHandleInfo::LOCATION);
        if (locSim >= 0) {
            auto it = data->locations.find(locSim);
            if (it != data->locations.end()) {
                proj->hasPointTarget = true;
                proj->tx = it->second->x;
                proj->ty = it->second->y;
            }
        }
    };
    auto allocProj = [data = data_](std::shared_ptr<JassNativesData::JassProjectile> proj) -> JassValue {
        int id = data->nextProjectileId++;
        data->projectiles[id] = std::move(proj);
        return JassValue::Int(data->AllocHandle(JassHandleInfo::PROJECTILE, id));
    };
    auto asReal = [](const JassValue& v) -> float {
        return v.type == JassValue::REAL ? v.realVal : (float)v.intVal;
    };
    // CreateJassProjectile(caster, rawcode, x, y, facing, speed, homing, target).
    vm_.RegisterNative("CreateJassProjectile", [data = data_, bindTarget, allocProj, asReal](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 8 || !data) return JassValue::Int(0);
        auto proj = std::make_shared<JassNativesData::JassProjectile>();
        proj->casterSimId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        proj->rawcode = (uint32_t)args[1].intVal;
        proj->x = asReal(args[2]);
        proj->y = asReal(args[3]);
        proj->facing = asReal(args[4]);
        proj->speed = std::max(1.0f, asReal(args[5]));
        proj->homing = args[6].boolVal;
        bindTarget(proj, args[7]);
        if (!proj->hasUnitTarget && !proj->hasPointTarget) {
            // Straight-line fallback along the launch facing.
            proj->hasPointTarget = true;
            proj->tx = proj->x + std::cos(proj->facing) * 2048.0f;
            proj->ty = proj->y + std::sin(proj->facing) * 2048.0f;
        }
        return allocProj(std::move(proj));
    });
    // Full ability-builder form (caster, localStore, castId, sourceUnit,
    // sourceLoc, target, rawcode, speed, homing, callbacks...): script
    // callbacks and cast stores need the ability-struct framework, so only
    // the motion/target/weapon fields are honored here.
    auto createTargeted = [data = data_, bindTarget, allocProj, asReal](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 9 || !data) return JassValue::Int(0);
        auto proj = std::make_shared<JassNativesData::JassProjectile>();
        proj->casterSimId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        const int srcUnit = data->GetSimHandle(args[3].intVal, JassHandleInfo::UNIT);
        const int srcLoc = data->GetSimHandle(args[4].intVal, JassHandleInfo::LOCATION);
        if (srcLoc >= 0) {
            auto it = data->locations.find(srcLoc);
            if (it != data->locations.end()) {
                proj->x = it->second->x;
                proj->y = it->second->y;
            }
        }
        bindTarget(proj, args[5]);
        proj->rawcode = (uint32_t)args[6].intVal;
        proj->speed = std::max(1.0f, asReal(args[7]));
        proj->homing = args[8].boolVal;
        if (srcUnit >= 0) proj->casterSimId = srcUnit;
        if (!proj->hasUnitTarget && !proj->hasPointTarget) {
            proj->hasPointTarget = true;
            proj->tx = proj->x + 2048.0f;
            proj->ty = proj->y;
        }
        return allocProj(std::move(proj));
    };
    vm_.RegisterNative("CreateUnitTargetedProjectile", createTargeted);
    vm_.RegisterNative("CreateLocationTargetedProjectile", createTargeted);
    vm_.RegisterNative("CreateUnitTargetedCollisionProjectile", createTargeted);
    vm_.RegisterNative("CreateLocationTargetedCollisionProjectile", createTargeted);
    vm_.RegisterNative("CreateUnitTargetedPseudoProjectile", createTargeted);
    vm_.RegisterNative("CreateLocationTargetedPseudoProjectile", createTargeted);
    vm_.RegisterNative("GetProjectileX", [getProj](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        auto proj = getProj(args[0].intVal);
        return JassValue::Real(proj ? proj->x : 0.0f);
    });
    vm_.RegisterNative("GetProjectileY", [getProj](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        auto proj = getProj(args[0].intVal);
        return JassValue::Real(proj ? proj->y : 0.0f);
    });
    vm_.RegisterNative("GetProjectileSource", [data = data_, sim = sim_, getProj](const std::vector<JassValue>& args) -> JassValue {
        (void)sim;
        if (args.empty() || !data) return JassValue::Int(0);
        auto proj = getProj(args[0].intVal);
        if (!proj || proj->casterSimId < 0) return JassValue::Int(0);
        return JassValue::Int(data->AllocHandle(JassHandleInfo::UNIT, proj->casterSimId));
    });
    vm_.RegisterNative("IsProjectileReflected", [getProj](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Bool(false);
        auto proj = getProj(args[0].intVal);
        return JassValue::Bool(proj && proj->reflected);
    });
    vm_.RegisterNative("SetProjectileReflected", [getProj](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (auto proj = getProj(args[0].intVal)) proj->reflected = args[1].boolVal;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetProjectileDone", [data = data_, getProj](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (args[1].boolVal) {
            if (auto proj = getProj(args[0].intVal)) proj->done = true;
            (void)data;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetProjectileDamage", [getProj, asReal](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (auto proj = getProj(args[0].intVal)) proj->damage = asReal(args[1]);
        return JassValue::Null();
    });
    vm_.RegisterNative("SetAttackProjectileDamage", [getProj, asReal](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (auto proj = getProj(args[0].intVal)) proj->damage = asReal(args[1]);
        return JassValue::Null();
    });
    vm_.RegisterNative("SetProjectileTargetUnit", [data = data_, getProj](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        auto proj = getProj(args[0].intVal);
        if (!proj) return JassValue::Null();
        const int tSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::UNIT);
        if (tSim >= 0) {
            proj->hasUnitTarget = true;
            proj->hasPointTarget = false;
            proj->targetSimId = tSim;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetProjectileTargetLoc", [data = data_, getProj](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        auto proj = getProj(args[0].intVal);
        if (!proj) return JassValue::Null();
        const int locSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        if (locSim >= 0) {
            auto it = data->locations.find(locSim);
            if (it != data->locations.end()) {
                proj->hasUnitTarget = false;
                proj->hasPointTarget = true;
                proj->tx = it->second->x;
                proj->ty = it->second->y;
            }
        }
        return JassValue::Null();
    });
}

// ============================================================
// 37b. Targeting natives (ref targeting handle API)
// ============================================================
// Ref CTargetType declaration ordinals (see ParseTargetType): the sets
// below store these so ConvertTargetType(i) round-trips.
