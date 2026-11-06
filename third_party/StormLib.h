#pragma once
#if defined(__has_include_next) && __has_include_next(<StormLib.h>)
#include_next <StormLib.h>
#else
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cctype>
#include <vector>
#include <string>
#include <fstream>
#include <unordered_map>
#include <zlib.h>
extern "C" int BZ2_bzBuffToBuffDecompress(char*, unsigned int*, char*, unsigned int, int, int);
#define BZ_OK 0

typedef void* HANDLE;
typedef uint32_t DWORD;

#ifndef MPQ_OPEN_READ_ONLY
#define MPQ_OPEN_READ_ONLY 0x00000100
#endif
#ifndef SFILE_OPEN_FROM_MPQ
#define SFILE_OPEN_FROM_MPQ 0x00000000
#endif
#ifndef SFILE_OPEN_BASE_FILE
#define SFILE_OPEN_BASE_FILE 0x00000000
#endif
#ifndef MPQ_CREATE_ARCHIVE_V1
#define MPQ_CREATE_ARCHIVE_V1 0x00000000
#endif
#ifndef MPQ_FILE_SINGLE_UNIT
#define MPQ_FILE_SINGLE_UNIT 0x01000000
#endif
#ifndef MPQ_FILE_EXISTS
#define MPQ_FILE_EXISTS 0x80000000
#endif
#ifndef MPQ_FILE_COMPRESS
#define MPQ_FILE_COMPRESS 0x00000200
#endif
#ifndef MPQ_FILE_IMPLODE
#define MPQ_FILE_IMPLODE 0x00000100
#endif
#ifndef MPQ_FILE_ENCRYPTED
#define MPQ_FILE_ENCRYPTED 0x00010000
#endif
#ifndef MPQ_FILE_FIX_KEY
#define MPQ_FILE_FIX_KEY 0x00020000
#endif

struct SFILE_FIND_DATA {
    char cFileName[1024];
    char* szPlainName;
    uint32_t dwHashIndex;
    uint32_t dwBlockIndex;
    uint32_t dwFileSize;
    uint32_t dwFileFlags;
    uint32_t dwCompSize;
    uint32_t dwFileTimeLo;
    uint32_t dwFileTimeHi;
    uint32_t lcLocale;
};

namespace storm_lite {

inline const uint32_t* GetCryptTable() {
    static uint32_t table[0x500];
    static bool init = false;
    if (!init) {
        uint32_t seed = 0x00100001;
        for (uint32_t i = 0; i < 0x100; ++i) {
            uint32_t idx = i;
            for (int j = 0; j < 5; ++j, idx += 0x100) {
                seed = (seed * 125 + 3) % 0x2AAAAB;
                uint32_t t1 = (seed & 0xFFFF) << 16;
                seed = (seed * 125 + 3) % 0x2AAAAB;
                uint32_t t2 = (seed & 0xFFFF);
                table[idx] = t1 | t2;
            }
        }
        init = true;
    }
    return table;
}

inline uint32_t HashString(const char* str, uint32_t hashType) {
    const uint32_t* crypt = GetCryptTable();
    uint32_t seed1 = 0x7FED7FED;
    uint32_t seed2 = 0xEEEEEEEE;
    while (*str) {
        unsigned char ch = (unsigned char)*str++;
        if (ch == '/') ch = '\\';
        ch = (unsigned char)std::toupper(ch);
        seed1 = crypt[(hashType << 8) + ch] ^ (seed1 + seed2);
        seed2 = ch + seed1 + seed2 + (seed2 << 5) + 3;
    }
    return seed1;
}

inline void DecryptBlock(void* data, size_t byteLen, uint32_t key) {
    const uint32_t* crypt = GetCryptTable();
    uint32_t seed = 0xEEEEEEEE;
    uint32_t* ptr = static_cast<uint32_t*>(data);
    size_t count = byteLen / 4;
    for (size_t i = 0; i < count; ++i) {
        seed += crypt[0x400 + (key & 0xFF)];
        uint32_t ch = ptr[i] ^ (key + seed);
        key = ((~key << 0x15) + 0x11111111) | (key >> 0x0B);
        seed = ch + seed + (seed << 5) + 3;
        ptr[i] = ch;
    }
}

inline void EncryptBlock(void* data, size_t byteLen, uint32_t key) {
    const uint32_t* crypt = GetCryptTable();
    uint32_t seed = 0xEEEEEEEE;
    uint32_t* ptr = static_cast<uint32_t*>(data);
    size_t count = byteLen / 4;
    for (size_t i = 0; i < count; ++i) {
        seed += crypt[0x400 + (key & 0xFF)];
        uint32_t orig = ptr[i];
        uint32_t enc = orig ^ (key + seed);
        key = ((~key << 0x15) + 0x11111111) | (key >> 0x0B);
        seed = orig + seed + (seed << 5) + 3;
        ptr[i] = enc;
    }
}

#pragma pack(push, 1)
struct MpqHeader {
    uint32_t magic;
    uint32_t headerSize;
    uint32_t archiveSize;
    uint16_t formatVersion;
    uint16_t blockSize;
    uint32_t hashTablePos;
    uint32_t blockTablePos;
    uint32_t hashTableSize;
    uint32_t blockTableSize;
};
struct MpqHashEntry {
    uint32_t name1;
    uint32_t name2;
    uint16_t locale;
    uint16_t platform;
    uint32_t blockIndex;
};
struct MpqBlockEntry {
    uint32_t filePos;
    uint32_t compSize;
    uint32_t fileSize;
    uint32_t flags;
};
#pragma pack(pop)

struct ArchiveObj {
    std::string path;
    bool writable = false;
    size_t mpqOffset = 0;
    MpqHeader hdr{};
    std::vector<uint8_t> rawBytes;
    std::vector<MpqHashEntry> hashTable;
    std::vector<MpqBlockEntry> blockTable;
    std::vector<std::string> listfile;
    std::unordered_map<std::string, std::vector<uint8_t>> createdFiles;
};

struct FileObj {
    ArchiveObj* archive = nullptr;
    std::string name;
    std::vector<uint8_t> data;
    size_t readPos = 0;
    bool writable = false;
};

struct FindObj {
    std::vector<std::string> names;
    size_t index = 0;
};

inline int FindHashBlock(const ArchiveObj* a, const char* name) {
    if (!a || a->hashTable.empty()) return -1;
    uint32_t mask = (uint32_t)a->hashTable.size() - 1;
    uint32_t start = HashString(name, 0) & mask;
    uint32_t n1 = HashString(name, 1);
    uint32_t n2 = HashString(name, 2);
    uint32_t idx = start;
    do {
        const auto& e = a->hashTable[idx];
        if (e.blockIndex == 0xFFFFFFFFu) return -1;
        if (e.blockIndex != 0xFFFFFFFEu && e.name1 == n1 && e.name2 == n2) {
            if (e.blockIndex < a->blockTable.size() &&
                (a->blockTable[e.blockIndex].flags & MPQ_FILE_EXISTS)) {
                return (int)e.blockIndex;
            }
        }
        idx = (idx + 1) & mask;
    } while (idx != start);
    return -1;
}

inline bool DecompressSector(const uint8_t* src, size_t srcLen, uint8_t* dst, size_t dstLen) {
    if (srcLen == dstLen) {
        std::memcpy(dst, src, dstLen);
        return true;
    }
    if (srcLen == 0) return false;
    uint8_t mask = src[0];
    const uint8_t* payload = src + 1;
    size_t payloadLen = srcLen - 1;
    if (mask == 0x02) {
        uLongf outLen = (uLongf)dstLen;
        return uncompress(dst, &outLen, payload, (uLong)payloadLen) == Z_OK && outLen == dstLen;
    }
    if (mask == 0x10) {
        unsigned int outLen = (unsigned int)dstLen;
        return BZ2_bzBuffToBuffDecompress((char*)dst, &outLen, (char*)payload, (unsigned int)payloadLen, 0, 0) == BZ_OK && outLen == dstLen;
    }
    return false;
}

inline bool ExtractBlock(const ArchiveObj* a, int blockIdx, const char* filename, std::vector<uint8_t>& out) {
    if (!a || blockIdx < 0 || (size_t)blockIdx >= a->blockTable.size()) return false;
    const auto& b = a->blockTable[blockIdx];
    size_t absPos = a->mpqOffset + b.filePos;
    if (absPos + b.compSize > a->rawBytes.size()) return false;
    out.resize(b.fileSize);
    if (b.fileSize == 0) return true;

    const char* plain = filename;
    for (const char* p = filename; *p; ++p) {
        if (*p == '\\' || *p == '/') plain = p + 1;
    }
    uint32_t fileKey = 0;
    if (b.flags & MPQ_FILE_ENCRYPTED) {
        fileKey = HashString(plain, 3);
        if (b.flags & MPQ_FILE_FIX_KEY) {
            fileKey = (fileKey + b.filePos) ^ b.fileSize;
        }
    }

    if ((b.flags & MPQ_FILE_SINGLE_UNIT) || !(b.flags & (MPQ_FILE_COMPRESS | MPQ_FILE_IMPLODE))) {
        std::vector<uint8_t> tmp(a->rawBytes.begin() + absPos, a->rawBytes.begin() + absPos + b.compSize);
        if (b.flags & MPQ_FILE_ENCRYPTED) DecryptBlock(tmp.data(), tmp.size(), fileKey);
        if (b.flags & MPQ_FILE_COMPRESS) {
            return DecompressSector(tmp.data(), tmp.size(), out.data(), out.size());
        }
        if (tmp.size() < out.size()) return false;
        std::memcpy(out.data(), tmp.data(), out.size());
        return true;
    }

    size_t sectorSize = 512u << a->hdr.blockSize;
    size_t numSectors = (b.fileSize + sectorSize - 1) / sectorSize;
    size_t tableBytes = (numSectors + 1) * sizeof(uint32_t);
    if (b.compSize < tableBytes) return false;
    std::vector<uint32_t> offsets(numSectors + 1);
    std::memcpy(offsets.data(), a->rawBytes.data() + absPos, tableBytes);
    if (b.flags & MPQ_FILE_ENCRYPTED) {
        DecryptBlock(offsets.data(), tableBytes, fileKey - 1);
    }
    for (size_t s = 0; s < numSectors; ++s) {
        uint32_t sStart = offsets[s];
        uint32_t sEnd = offsets[s + 1];
        if (sEnd < sStart || sEnd > b.compSize) return false;
        size_t sCompLen = sEnd - sStart;
        size_t sUncompLen = std::min(sectorSize, (size_t)b.fileSize - s * sectorSize);
        std::vector<uint8_t> sec(a->rawBytes.begin() + absPos + sStart, a->rawBytes.begin() + absPos + sEnd);
        if (b.flags & MPQ_FILE_ENCRYPTED) {
            DecryptBlock(sec.data(), sec.size(), fileKey + (uint32_t)s);
        }
        if (!DecompressSector(sec.data(), sCompLen, out.data() + s * sectorSize, sUncompLen)) {
            return false;
        }
    }
    return true;
}

} // namespace storm_lite

inline bool SFileOpenArchive(const char* szMpqName, DWORD, DWORD, HANDLE* phMpq) {
    if (!szMpqName || !phMpq) return false;
    *phMpq = nullptr;
    std::ifstream f(szMpqName, std::ios::binary | std::ios::ate);
    if (!f) return false;
    std::streamsize sz = f.tellg();
    if (sz < 32) return false;
    f.seekg(0, std::ios::beg);
    auto* a = new storm_lite::ArchiveObj();
    a->path = szMpqName;
    a->rawBytes.resize((size_t)sz);
    if (!f.read((char*)a->rawBytes.data(), sz)) { delete a; return false; }

    bool found = false;
    for (size_t off = 0; off + sizeof(storm_lite::MpqHeader) <= a->rawBytes.size(); off += 512) {
        if (std::memcmp(a->rawBytes.data() + off, "MPQ\x1A", 4) == 0) {
            a->mpqOffset = off;
            std::memcpy(&a->hdr, a->rawBytes.data() + off, sizeof(storm_lite::MpqHeader));
            found = true;
            break;
        }
    }
    if (!found || a->hdr.hashTableSize == 0 || (a->hdr.hashTableSize & (a->hdr.hashTableSize - 1)) != 0) {
        delete a; return false;
    }
    size_t htPos = a->mpqOffset + a->hdr.hashTablePos;
    size_t htBytes = a->hdr.hashTableSize * sizeof(storm_lite::MpqHashEntry);
    size_t btPos = a->mpqOffset + a->hdr.blockTablePos;
    size_t btBytes = a->hdr.blockTableSize * sizeof(storm_lite::MpqBlockEntry);
    if (htPos + htBytes > a->rawBytes.size() || btPos + btBytes > a->rawBytes.size()) {
        delete a; return false;
    }
    a->hashTable.resize(a->hdr.hashTableSize);
    std::memcpy(a->hashTable.data(), a->rawBytes.data() + htPos, htBytes);
    storm_lite::DecryptBlock(a->hashTable.data(), htBytes, storm_lite::HashString("(hash table)", 3));

    a->blockTable.resize(a->hdr.blockTableSize);
    std::memcpy(a->blockTable.data(), a->rawBytes.data() + btPos, btBytes);
    storm_lite::DecryptBlock(a->blockTable.data(), btBytes, storm_lite::HashString("(block table)", 3));

    int lfIdx = storm_lite::FindHashBlock(a, "(listfile)");
    if (lfIdx >= 0) {
        std::vector<uint8_t> lfBytes;
        if (storm_lite::ExtractBlock(a, lfIdx, "(listfile)", lfBytes)) {
            std::string cur;
            for (uint8_t c : lfBytes) {
                if (c == '\r' || c == '\n') {
                    if (!cur.empty()) { a->listfile.push_back(cur); cur.clear(); }
                } else {
                    cur.push_back((char)c);
                }
            }
            if (!cur.empty()) a->listfile.push_back(cur);
        }
    }
    *phMpq = a;
    return true;
}

inline bool SFileCreateArchive(const char* szMpqName, DWORD, DWORD dwMaxFileCount, HANDLE* phMpq) {
    if (!szMpqName || !phMpq) return false;
    auto* a = new storm_lite::ArchiveObj();
    a->path = szMpqName;
    a->writable = true;
    uint32_t htSize = 16;
    while (htSize < dwMaxFileCount) htSize <<= 1;
    a->hdr.magic = 0x1A51504Du;
    a->hdr.headerSize = 32;
    a->hdr.formatVersion = 0;
    a->hdr.blockSize = 3;
    a->hdr.hashTableSize = htSize;
    *phMpq = a;
    return true;
}

inline bool SFileCreateFile(HANDLE hMpq, const char* szArchivedName, uint64_t, DWORD dwFileSize, DWORD, DWORD, HANDLE* phFile) {
    auto* a = static_cast<storm_lite::ArchiveObj*>(hMpq);
    if (!a || !a->writable || !szArchivedName || !phFile) return false;
    auto* f = new storm_lite::FileObj();
    f->archive = a;
    f->name = szArchivedName;
    f->data.reserve(dwFileSize);
    f->writable = true;
    *phFile = f;
    return true;
}

inline bool SFileWriteFile(HANDLE hFile, const void* pvData, DWORD dwSize, DWORD) {
    auto* f = static_cast<storm_lite::FileObj*>(hFile);
    if (!f || !f->writable) return false;
    const uint8_t* p = static_cast<const uint8_t*>(pvData);
    f->data.insert(f->data.end(), p, p + dwSize);
    return true;
}

inline bool SFileFinishFile(HANDLE hFile) {
    auto* f = static_cast<storm_lite::FileObj*>(hFile);
    if (!f || !f->writable || !f->archive) return false;
    f->archive->createdFiles[f->name] = std::move(f->data);
    delete f;
    return true;
}

inline bool SFileCloseArchive(HANDLE hMpq) {
    auto* a = static_cast<storm_lite::ArchiveObj*>(hMpq);
    if (!a) return false;
    if (a->writable) {
        uint32_t htSize = a->hdr.hashTableSize;
        std::vector<storm_lite::MpqHashEntry> ht(htSize, {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFFFFFu});
        std::vector<storm_lite::MpqBlockEntry> bt;
        std::vector<uint8_t> payload;
        for (const auto& [name, bytes] : a->createdFiles) {
            uint32_t bIdx = (uint32_t)bt.size();
            uint32_t fPos = (uint32_t)(sizeof(storm_lite::MpqHeader) + payload.size());
            payload.insert(payload.end(), bytes.begin(), bytes.end());
            bt.push_back({fPos, (uint32_t)bytes.size(), (uint32_t)bytes.size(), MPQ_FILE_EXISTS | MPQ_FILE_SINGLE_UNIT});
            uint32_t mask = htSize - 1;
            uint32_t idx = storm_lite::HashString(name.c_str(), 0) & mask;
            while (ht[idx].blockIndex != 0xFFFFFFFFu) idx = (idx + 1) & mask;
            ht[idx] = {storm_lite::HashString(name.c_str(), 1), storm_lite::HashString(name.c_str(), 2), 0, 0, bIdx};
        }
        a->hdr.hashTablePos = (uint32_t)(sizeof(storm_lite::MpqHeader) + payload.size());
        a->hdr.blockTablePos = a->hdr.hashTablePos + (uint32_t)(ht.size() * sizeof(storm_lite::MpqHashEntry));
        a->hdr.blockTableSize = (uint32_t)bt.size();
        a->hdr.archiveSize = a->hdr.blockTablePos + (uint32_t)(bt.size() * sizeof(storm_lite::MpqBlockEntry));
        storm_lite::EncryptBlock(ht.data(), ht.size() * sizeof(storm_lite::MpqHashEntry), storm_lite::HashString("(hash table)", 3));
        if (!bt.empty()) {
            storm_lite::EncryptBlock(bt.data(), bt.size() * sizeof(storm_lite::MpqBlockEntry), storm_lite::HashString("(block table)", 3));
        }
        std::ofstream out(a->path, std::ios::binary);
        out.write((const char*)&a->hdr, sizeof(storm_lite::MpqHeader));
        if (!payload.empty()) out.write((const char*)payload.data(), (std::streamsize)payload.size());
        out.write((const char*)ht.data(), (std::streamsize)(ht.size() * sizeof(storm_lite::MpqHashEntry)));
        if (!bt.empty()) out.write((const char*)bt.data(), (std::streamsize)(bt.size() * sizeof(storm_lite::MpqBlockEntry)));
    }
    delete a;
    return true;
}

inline bool SFileHasFile(HANDLE hMpq, const char* szFileName) {
    auto* a = static_cast<storm_lite::ArchiveObj*>(hMpq);
    return storm_lite::FindHashBlock(a, szFileName) >= 0;
}

inline bool SFileOpenFileEx(HANDLE hMpq, const char* szFileName, DWORD, HANDLE* phFile) {
    if (!phFile) return false;
    *phFile = nullptr;
    auto* a = static_cast<storm_lite::ArchiveObj*>(hMpq);
    int bIdx = storm_lite::FindHashBlock(a, szFileName);
    if (bIdx < 0) return false;
    auto* f = new storm_lite::FileObj();
    f->archive = a;
    f->name = szFileName;
    if (!storm_lite::ExtractBlock(a, bIdx, szFileName, f->data)) {
        delete f;
        return false;
    }
    *phFile = f;
    return true;
}

inline DWORD SFileGetFileSize(HANDLE hFile, DWORD* pdwFileSizeHigh) {
    if (pdwFileSizeHigh) *pdwFileSizeHigh = 0;
    auto* f = static_cast<storm_lite::FileObj*>(hFile);
    return f ? (DWORD)f->data.size() : 0;
}

inline bool SFileReadFile(HANDLE hFile, void* pvBuffer, DWORD dwToRead, DWORD* pdwRead, void*) {
    auto* f = static_cast<storm_lite::FileObj*>(hFile);
    if (!f || !pvBuffer) return false;
    size_t avail = f->data.size() - f->readPos;
    size_t n = std::min((size_t)dwToRead, avail);
    if (n > 0) std::memcpy(pvBuffer, f->data.data() + f->readPos, n);
    f->readPos += n;
    if (pdwRead) *pdwRead = (DWORD)n;
    return true;
}

inline bool SFileCloseFile(HANDLE hFile) {
    auto* f = static_cast<storm_lite::FileObj*>(hFile);
    delete f;
    return true;
}

inline HANDLE SFileFindFirstFile(HANDLE hMpq, const char*, SFILE_FIND_DATA* lpFindFileData, const char*) {
    auto* a = static_cast<storm_lite::ArchiveObj*>(hMpq);
    if (!a || a->listfile.empty() || !lpFindFileData) return nullptr;
    auto* fn = new storm_lite::FindObj();
    fn->names = a->listfile;
    fn->index = 1;
    std::memset(lpFindFileData, 0, sizeof(SFILE_FIND_DATA));
    std::strncpy(lpFindFileData->cFileName, fn->names[0].c_str(), sizeof(lpFindFileData->cFileName) - 1);
    return fn;
}

inline bool SFileFindNextFile(HANDLE hFind, SFILE_FIND_DATA* lpFindFileData) {
    auto* fn = static_cast<storm_lite::FindObj*>(hFind);
    if (!fn || fn->index >= fn->names.size() || !lpFindFileData) return false;
    std::memset(lpFindFileData, 0, sizeof(SFILE_FIND_DATA));
    std::strncpy(lpFindFileData->cFileName, fn->names[fn->index++].c_str(), sizeof(lpFindFileData->cFileName) - 1);
    return true;
}

inline bool SFileFindClose(HANDLE hFind) {
    auto* fn = static_cast<storm_lite::FindObj*>(hFind);
    delete fn;
    return true;
}
#endif // __has_include_next(<StormLib.h>)
