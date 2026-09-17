#include "object_mods.h"

#ifndef __STORMLIB_NO_STATIC_LINK__
#define __STORMLIB_NO_STATIC_LINK__
#endif
#include <StormLib.h>

#include <cstring>
#include <iostream>
#include <utility>
#include <vector>

namespace {

uint32_t ReadLE32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

float ReadF32(const uint8_t* p) {
    float v = 0.0f;
    std::memcpy(&v, p, sizeof(float));
    return v;
}

War3ID ReadObjectId(const uint8_t* p) {
    return War3ID(ReadLE32(p));
}

bool ReadMapFile(const std::string& mapPath, const char* fileName,
                 std::vector<uint8_t>& out) {
    HANDLE hMpq = nullptr;
    if (!SFileOpenArchive(mapPath.c_str(), 0, MPQ_OPEN_READ_ONLY, &hMpq)) {
        return false;
    }

    HANDLE hFile = nullptr;
    if (!SFileOpenFileEx(hMpq, fileName, SFILE_OPEN_BASE_FILE, &hFile)) {
        SFileCloseArchive(hMpq);
        return false;
    }

    DWORD sz = SFileGetFileSize(hFile, nullptr);
    out.resize(sz);
    DWORD rd = 0;
    bool ok = SFileReadFile(hFile, out.data(), sz, &rd, nullptr) && rd == sz;
    SFileCloseFile(hFile);
    SFileCloseArchive(hMpq);
    if (!ok) out.clear();
    return ok;
}

class Reader {
public:
    Reader(const uint8_t* data, size_t size) : data_(data), size_(size) {}

    bool U32(uint32_t& out) {
        if (off_ + 4 > size_) return false;
        out = ReadLE32(data_ + off_);
        off_ += 4;
        return true;
    }

    bool I32(int& out) {
        uint32_t value = 0;
        if (!U32(value)) return false;
        out = static_cast<int>(value);
        return true;
    }

    bool F32(float& out) {
        if (off_ + 4 > size_) return false;
        out = ReadF32(data_ + off_);
        off_ += 4;
        return true;
    }

    bool Id(War3ID& out) {
        if (off_ + 4 > size_) return false;
        out = ReadObjectId(data_ + off_);
        off_ += 4;
        return true;
    }

    bool String(std::string& out) {
        out.clear();
        while (off_ < size_) {
            char ch = static_cast<char>(data_[off_++]);
            if (ch == '\0') return true;
            out.push_back(ch);
            if (out.size() > 8192) return false;
        }
        return false;
    }

private:
    const uint8_t* data_ = nullptr;
    size_t size_ = 0;
    size_t off_ = 0;

public:
    size_t Offset() const { return off_; }
    size_t Size() const { return size_; }
};

bool ParseModificationTable(Reader& r, int version, bool custom, bool extended,
                            bool readVersionJunk, bool diagnostics,
                            WtsTable* wts,
                            std::vector<ObjectModificationEntry>& out) {
    (void)version;
    int count = 0;
    if (!r.I32(count) || count < 0 || count > 200000) {
        if (diagnostics) {
            std::cerr << "ObjectMods: invalid " << (custom ? "custom" : "original")
                      << " table count at offset=" << r.Offset()
                      << " version=" << version << " count=" << count << std::endl;
        }
        return false;
    }
    out.reserve(out.size() + static_cast<size_t>(count));

    for (int i = 0; i < count; ++i) {
        ObjectModificationEntry entry;
        entry.custom = custom;
        if (!r.Id(entry.oldId)) {
            if (diagnostics) {
                std::cerr << "ObjectMods: missing oldId in " << (custom ? "custom" : "original")
                          << " entry=" << i << " offset=" << r.Offset() << std::endl;
            }
            return false;
        }
        if (!r.Id(entry.newId)) {
            if (diagnostics) {
                std::cerr << "ObjectMods: missing newId in " << (custom ? "custom" : "original")
                          << " entry=" << i << " offset=" << r.Offset() << std::endl;
            }
            return false;
        }

        if (version >= 3 && readVersionJunk) {
            int junkCount = 0;
            if (!r.I32(junkCount) || junkCount < 0 || junkCount > 1024) {
                if (diagnostics) {
                    std::cerr << "ObjectMods: invalid junk count in " << (custom ? "custom" : "original")
                              << " entry=" << i << " offset=" << r.Offset()
                              << " junkCount=" << junkCount << std::endl;
                }
                return false;
            }
            for (int j = 0; j < junkCount; ++j) {
                int ignored = 0;
                if (!r.I32(ignored)) {
                    if (diagnostics) {
                        std::cerr << "ObjectMods: truncated junk data in " << (custom ? "custom" : "original")
                                  << " entry=" << i << " junkIndex=" << j
                                  << " offset=" << r.Offset() << std::endl;
                    }
                    return false;
                }
            }
        }

        int modCount = 0;
        if (!r.I32(modCount) || modCount < 0 || modCount > 200000) {
            if (diagnostics) {
                std::cerr << "ObjectMods: invalid modification count in " << (custom ? "custom" : "original")
                          << " entry=" << i << " offset=" << r.Offset()
                          << " modCount=" << modCount << std::endl;
            }
            return false;
        }
        entry.modifications.reserve(static_cast<size_t>(modCount));
        for (int j = 0; j < modCount; ++j) {
            ObjectModification mod;
            if (!r.Id(mod.id)) {
                if (diagnostics) {
                    std::cerr << "ObjectMods: missing mod id in " << (custom ? "custom" : "original")
                              << " entry=" << i << " mod=" << j
                              << " offset=" << r.Offset() << std::endl;
                }
                return false;
            }
            if (!r.I32(mod.varType)) {
                if (diagnostics) {
                    std::cerr << "ObjectMods: missing var type in " << (custom ? "custom" : "original")
                              << " entry=" << i << " mod=" << j
                              << " offset=" << r.Offset() << std::endl;
                }
                return false;
            }
            if (extended) {
                if (!r.I32(mod.level)) {
                    if (diagnostics) {
                        std::cerr << "ObjectMods: missing level in " << (custom ? "custom" : "original")
                                  << " entry=" << i << " mod=" << j
                                  << " offset=" << r.Offset() << std::endl;
                    }
                    return false;
                }
                if (!r.I32(mod.dataPointer)) {
                    if (diagnostics) {
                        std::cerr << "ObjectMods: missing data pointer in " << (custom ? "custom" : "original")
                                  << " entry=" << i << " mod=" << j
                                  << " offset=" << r.Offset() << std::endl;
                    }
                    return false;
                }
            }

            switch (mod.varType) {
            case ObjectModification::INT:
                if (!r.I32(mod.intValue)) {
                    if (diagnostics) {
                        std::cerr << "ObjectMods: missing int value in " << (custom ? "custom" : "original")
                                  << " entry=" << i << " mod=" << j
                                  << " field=" << mod.id.ToString()
                                  << " offset=" << r.Offset() << std::endl;
                    }
                    return false;
                }
                break;
            case ObjectModification::BOOLEAN:
                if (!r.I32(mod.intValue)) {
                    if (diagnostics) {
                        std::cerr << "ObjectMods: missing bool value in " << (custom ? "custom" : "original")
                                  << " entry=" << i << " mod=" << j
                                  << " field=" << mod.id.ToString()
                                  << " offset=" << r.Offset() << std::endl;
                    }
                    return false;
                }
                mod.intValue = mod.intValue != 0 ? 1 : 0;
                break;
            case ObjectModification::STRING:
                if (!r.String(mod.stringValue)) {
                    if (diagnostics) {
                        std::cerr << "ObjectMods: missing string value in " << (custom ? "custom" : "original")
                                  << " entry=" << i << " mod=" << j
                                  << " field=" << mod.id.ToString()
                                  << " offset=" << r.Offset() << std::endl;
                    }
                    return false;
                }
                if (wts) mod.stringValue = wts->Resolve(mod.stringValue);
                break;
            case ObjectModification::REAL:
            case ObjectModification::UNREAL:
            default:
                if (!r.F32(mod.realValue)) {
                    if (diagnostics) {
                        std::cerr << "ObjectMods: missing real value in " << (custom ? "custom" : "original")
                                  << " entry=" << i << " mod=" << j
                                  << " field=" << mod.id.ToString()
                                  << " varType=" << mod.varType
                                  << " offset=" << r.Offset() << std::endl;
                    }
                    return false;
                }
                break;
            }

            War3ID ignoredJunk;
            if (!r.Id(ignoredJunk)) {
                if (diagnostics) {
                    std::cerr << "ObjectMods: missing trailing junk id in " << (custom ? "custom" : "original")
                              << " entry=" << i << " mod=" << j
                              << " field=" << mod.id.ToString()
                              << " offset=" << r.Offset() << std::endl;
                }
                return false;
            }
            entry.modifications.push_back(std::move(mod));
        }
        out.push_back(std::move(entry));
    }
    return true;
}

bool ParseObjectModsWithOptions(const uint8_t* data, size_t size, bool extended,
                                bool readVersionJunk, bool diagnostics,
                                ObjectModificationSet& out, WtsTable* wts) {
    Reader r(data, size);
    int version = 0;
    if (!r.I32(version)) {
        if (diagnostics) {
            std::cerr << "ObjectMods: missing version size=" << size << std::endl;
        }
        return false;
    }
    if (version < 1 || version > 3) {
        if (diagnostics) {
            std::cerr << "ObjectMods: unsupported version=" << version
                      << " size=" << size << std::endl;
        }
        return false;
    }

    ObjectModificationSet parsed;
    parsed.version = version;
    if (!ParseModificationTable(r, version, false, extended, readVersionJunk,
                                diagnostics, wts, parsed.original)) {
        return false;
    }
    if (!ParseModificationTable(r, version, true, extended, readVersionJunk,
                                diagnostics, wts, parsed.custom)) {
        return false;
    }

    out = std::move(parsed);
    return true;
}

bool ParseObjectMods(const uint8_t* data, size_t size,
                     bool extended, ObjectModificationSet& out, WtsTable* wts) {
    if (ParseObjectModsWithOptions(data, size, extended, true, false, out, wts)) {
        return true;
    }
    if (extended && ParseObjectModsWithOptions(data, size, extended, false, false, out, wts)) {
        return true;
    }
    if (extended && ParseObjectModsWithOptions(data, size, false, true, false, out, wts)) {
        return true;
    }
    if (extended && ParseObjectModsWithOptions(data, size, false, false, false, out, wts)) {
        return true;
    }
    return ParseObjectModsWithOptions(data, size, extended, true, true, out, wts);
}

} // namespace

bool LoadWar3MapObjectMods(const std::string& mapPath,
                           const char* fileName,
                           bool extended,
                           ObjectModificationSet& out,
                           WtsTable* wts) {
    std::vector<uint8_t> wtsData;
    WtsTable localWts;
    if (!wts && ReadMapFile(mapPath, "war3map.wts", wtsData)) {
        localWts.Load(wtsData.data(), wtsData.size());
        wts = &localWts;
    }

    std::vector<uint8_t> data;
    if (!ReadMapFile(mapPath, fileName, data)) {
        return false;
    }

    if (!ParseObjectMods(data.data(), data.size(), extended, out, wts)) {
        std::cerr << "ObjectMods: failed to parse " << fileName << std::endl;
        return false;
    }
    return true;
}

bool LoadWar3MapUnitMods(const std::string& mapPath,
                         ObjectModificationSet& out,
                         WtsTable* wts) {
    return LoadWar3MapObjectMods(mapPath, "war3map.w3u", false, out, wts);
}

bool LoadWar3MapDestructableMods(const std::string& mapPath,
                                  ObjectModificationSet& out,
                                  WtsTable* wts) {
    return LoadWar3MapObjectMods(mapPath, "war3map.w3b", false, out, wts);
}

bool LoadWar3MapItemMods(const std::string& mapPath,
                          ObjectModificationSet& out,
                          WtsTable* wts) {
    return LoadWar3MapObjectMods(mapPath, "war3map.w3t", false, out, wts);
}

bool LoadWar3MapAbilityMods(const std::string& mapPath,
                            ObjectModificationSet& out,
                            WtsTable* wts) {
    return LoadWar3MapObjectMods(mapPath, "war3map.w3a", true, out, wts);
}

bool LoadWar3MapUpgradeMods(const std::string& mapPath,
                            ObjectModificationSet& out,
                            WtsTable* wts) {
    return LoadWar3MapObjectMods(mapPath, "war3map.w3q", true, out, wts);
}

bool LoadWar3MapBuffMods(const std::string& mapPath,
                         ObjectModificationSet& out,
                         WtsTable* wts) {
    return LoadWar3MapObjectMods(mapPath, "war3map.w3h", true, out, wts);
}

bool ParseObjectModsData(const uint8_t* data, size_t size,
                         bool extended, ObjectModificationSet& out,
                         WtsTable* wts) {
    if (!data || size == 0) return false;
    return ParseObjectMods(data, size, extended, out, wts);
}
