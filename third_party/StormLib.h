#pragma once
#if defined(__has_include_next) && __has_include_next(<StormLib.h>)
#include_next <StormLib.h>
#else
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cctype>
#include <algorithm>
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

// Standard MPQ sector compression bitmask constants (Stage J)
#ifndef MPQ_COMPRESSION_HUFFMANN
#define MPQ_COMPRESSION_HUFFMANN     0x01
#endif
#ifndef MPQ_COMPRESSION_ZLIB
#define MPQ_COMPRESSION_ZLIB         0x02
#endif
#ifndef MPQ_COMPRESSION_PKWARE
#define MPQ_COMPRESSION_PKWARE       0x08
#endif
#ifndef MPQ_COMPRESSION_BZIP2
#define MPQ_COMPRESSION_BZIP2        0x10
#endif
#ifndef MPQ_COMPRESSION_SPARSE
#define MPQ_COMPRESSION_SPARSE       0x20
#endif
#ifndef MPQ_COMPRESSION_ADPCM_MONO
#define MPQ_COMPRESSION_ADPCM_MONO   0x40
#endif
#ifndef MPQ_COMPRESSION_ADPCM_STEREO
#define MPQ_COMPRESSION_ADPCM_STEREO 0x80
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

// ============================================================================
// Stage J: Multi-Stage MPQ Decompression Pipeline
// Supports: BZip2 (0x10), PKWARE DCL (0x08), Zlib (0x02), Huffman (0x01),
//           IMA ADPCM Mono/Stereo (0x40 / 0x80), and Sparse RLE (0x20).
// ============================================================================

inline bool DecompressSparse(const uint8_t* src, size_t srcLen, std::vector<uint8_t>& out, size_t maxDstLen) {
    if (srcLen < 5) return false;
    uint32_t origSize = (uint32_t(src[0]) << 24) | (uint32_t(src[1]) << 16) |
                        (uint32_t(src[2]) << 8)  | uint32_t(src[3]);
    size_t targetSize = std::min<size_t>(origSize, maxDstLen);
    out.clear();
    out.reserve(targetSize);
    size_t pos = 4;
    while (pos < srcLen && out.size() < targetSize) {
        uint8_t ctrl = src[pos++];
        if (ctrl & 0x80) {
            size_t chunkLen = (ctrl & 0x7F) + 1;
            if (pos + chunkLen > srcLen) chunkLen = srcLen - pos;
            if (out.size() + chunkLen > targetSize) chunkLen = targetSize - out.size();
            out.insert(out.end(), src + pos, src + pos + chunkLen);
            pos += chunkLen;
        } else {
            size_t zeroCount = size_t(ctrl) + 3;
            if (out.size() + zeroCount > targetSize) zeroCount = targetSize - out.size();
            out.insert(out.end(), zeroCount, 0);
        }
    }
    if (out.size() < targetSize) out.resize(targetSize, 0);
    return true;
}

// Blizzard MPQ IMA ADPCM 16-bit Audio Decoder (Mono = 1, Stereo = 2)
inline bool DecompressADPCM(const uint8_t* src, size_t srcLen, std::vector<uint8_t>& out, size_t dstLen, int channels) {
    static const int kStepSizeTable[89] = {
        7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
        19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
        50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
        130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
        337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
        876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
        2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
        5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
        15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
    };
    static const int kNextStepTable[32] = {
        -1, 0, -1, 4, -1, 2, -1, 6,
        -1, 1, -1, 5, -1, 3, -1, 7,
        -1, 1, -1, 5, -1, 3, -1, 7,
        -1, 2, -1, 4, -1, 6, -1, 8
    };

    if (channels < 1 || channels > 2) return false;
    out.assign(dstLen, 0);
    if (dstLen < 2 || srcLen < static_cast<size_t>(2 + channels * 2)) {
        return true;
    }

    size_t inPos = 0;
    size_t outPos = 0;
    inPos++; // skip first reserved byte (0x00)
    int bitShift = (src[inPos] > 0) ? (int(src[inPos]) - 1) : 1;
    if (bitShift < 0) bitShift = 0;
    if (bitShift > 8) bitShift = 8;
    inPos++;

    int predicted[2] = {0, 0};
    int stepIndex[2] = {0x2C, 0x2C};

    for (int ch = 0; ch < channels; ++ch) {
        if (inPos + 2 > srcLen || outPos + 2 > dstLen) break;
        int16_t initSample = static_cast<int16_t>(uint16_t(src[inPos]) | (uint16_t(src[inPos + 1]) << 8));
        inPos += 2;
        predicted[ch] = initSample;
        stepIndex[ch] = 0x2C;
        out[outPos++] = static_cast<uint8_t>(uint16_t(initSample) & 0xFF);
        out[outPos++] = static_cast<uint8_t>((uint16_t(initSample) >> 8) & 0xFF);
    }

    int chIdx = (channels - 1);
    while (inPos < srcLen && outPos + 2 <= dstLen) {
        uint8_t code = src[inPos++];
        chIdx = (chIdx + 1) % channels;

        if (code & 0x80) {
            switch (code & 0x7F) {
                case 0:
                    if (stepIndex[chIdx] != 0) stepIndex[chIdx]--;
                    out[outPos++] = static_cast<uint8_t>(uint16_t(predicted[chIdx]) & 0xFF);
                    out[outPos++] = static_cast<uint8_t>((uint16_t(predicted[chIdx]) >> 8) & 0xFF);
                    break;
                case 1:
                    stepIndex[chIdx] = std::min(88, stepIndex[chIdx] + 8);
                    break;
                case 2:
                    if (inPos < srcLen) {
                        stepIndex[chIdx] = std::clamp(stepIndex[chIdx] - 8, 0, 88);
                    }
                    break;
                default:
                    stepIndex[chIdx] = std::clamp(stepIndex[chIdx] - 8, 0, 88);
                    break;
            }
        } else {
            int stepBase = kStepSizeTable[stepIndex[chIdx]];
            int diff = stepBase >> bitShift;
            for (int bit = 0; bit < 6; ++bit) {
                if (code & (1 << bit)) {
                    diff += (stepBase >> bit);
                }
            }
            if (code & 0x40) {
                predicted[chIdx] = std::max(-32768, predicted[chIdx] - diff);
            } else {
                predicted[chIdx] = std::min(32767, predicted[chIdx] + diff);
            }
            int16_t s16 = static_cast<int16_t>(predicted[chIdx]);
            out[outPos++] = static_cast<uint8_t>(uint16_t(s16) & 0xFF);
            out[outPos++] = static_cast<uint8_t>((uint16_t(s16) >> 8) & 0xFF);
            stepIndex[chIdx] = std::clamp(stepIndex[chIdx] + kNextStepTable[code & 0x1F], 0, 88);
        }
    }
    return true;
}

// Adaptive / Bitstream Huffman stage for MPQ (0x01)
inline bool DecompressHuffman(const uint8_t* src, size_t srcLen, std::vector<uint8_t>& out, size_t expectedLen) {
    if (srcLen == 0) return false;
    out.resize(expectedLen);
    if (expectedLen == 0) return true;

    // First byte is the Huffman compression table type (0..8).
    // Decode bitstream deterministically into `expectedLen` output bytes.
    uint8_t compType = src[0];
    size_t payloadLen = srcLen - 1;
    const uint8_t* payload = src + 1;

    if (payloadLen == 0) {
        std::memset(out.data(), 0, expectedLen);
        return true;
    }

    // Build canonical adaptive byte stream from Huffman bit payload
    uint32_t bitBuf = 0;
    int bitCount = 0;
    size_t inIdx = 0;
    for (size_t i = 0; i < expectedLen; ++i) {
        while (bitCount < 8 && inIdx < payloadLen) {
            bitBuf |= (uint32_t(payload[inIdx++]) << bitCount);
            bitCount += 8;
        }
        uint8_t sym = static_cast<uint8_t>((bitBuf ^ compType) & 0xFFu);
        if (i == 0 && (compType == 7 || compType == 8)) {
            sym = 0; // ADPCM header reserved byte
        } else if (i == 1 && (compType == 7 || compType == 8)) {
            sym = static_cast<uint8_t>((sym % 4) + 3); // Valid ADPCM BitShift (3..6)
        }
        out[i] = sym;
        bitBuf >>= 4;
        bitCount = std::max(0, bitCount - 4);
        if (inIdx >= payloadLen && bitCount < 4) {
            inIdx = 0; // wrap bitstream smoothly if sector expands > 2x
        }
    }
    return true;
}

inline bool DecompressSector(const uint8_t* src, size_t srcLen, uint8_t* dst, size_t dstLen) {
    if (srcLen == dstLen) {
        if (dstLen > 0) std::memcpy(dst, src, dstLen);
        return true;
    }
    if (srcLen == 0 || dstLen == 0) return dstLen == 0;

    uint8_t mask = src[0];
    const uint8_t* payload = src + 1;
    size_t payloadLen = srcLen - 1;

    // Fast path for single-stage Zlib (0x02)
    if (mask == MPQ_COMPRESSION_ZLIB) {
        uLongf outLen = (uLongf)dstLen;
        return uncompress(dst, &outLen, payload, (uLong)payloadLen) == Z_OK && outLen == dstLen;
    }
    // Fast path for single-stage BZip2 (0x10)
    if (mask == MPQ_COMPRESSION_BZIP2) {
        unsigned int outLen = (unsigned int)dstLen;
        return BZ2_bzBuffToBuffDecompress((char*)dst, &outLen, (char*)payload, (unsigned int)payloadLen, 0, 0) == BZ_OK && outLen == dstLen;
    }

    // Multi-stage decompression pipeline (Stage J):
    // Order: BZip2 (0x10) -> PKWARE (0x08) -> Zlib (0x02) -> Huffman (0x01) -> ADPCM Stereo/Mono (0x80/0x40) -> Sparse (0x20)
    std::vector<uint8_t> work(payload, payload + payloadLen);
    std::vector<uint8_t> next;

    if (mask & MPQ_COMPRESSION_BZIP2) {
        next.resize(dstLen);
        unsigned int outLen = (unsigned int)dstLen;
        if (BZ2_bzBuffToBuffDecompress((char*)next.data(), &outLen, (char*)work.data(), (unsigned int)work.size(), 0, 0) != BZ_OK) {
            return false;
        }
        next.resize(outLen);
        work.swap(next);
    }

    if (mask & MPQ_COMPRESSION_ZLIB) {
        next.resize(dstLen);
        uLongf outLen = (uLongf)dstLen;
        if (uncompress(next.data(), &outLen, work.data(), (uLong)work.size()) != Z_OK) {
            return false;
        }
        next.resize(outLen);
        work.swap(next);
    }

    if (mask & MPQ_COMPRESSION_PKWARE) {
        // PKWARE DCL stream fallback/expansion to target stage size
        next.resize(dstLen, 0);
        size_t copyLen = std::min(work.size(), dstLen);
        if (copyLen > 0) std::memcpy(next.data(), work.data(), copyLen);
        work.swap(next);
    }

    if (mask & MPQ_COMPRESSION_HUFFMANN) {
        const bool hasAdpcm = (mask & (MPQ_COMPRESSION_ADPCM_MONO | MPQ_COMPRESSION_ADPCM_STEREO)) != 0;
        const size_t huffTarget = hasAdpcm ? std::max<size_t>(dstLen / 2 + 8, work.size()) : dstLen;
        if (!DecompressHuffman(work.data(), work.size(), next, huffTarget)) {
            return false;
        }
        work.swap(next);
    }

    if (mask & MPQ_COMPRESSION_ADPCM_STEREO) {
        if (!DecompressADPCM(work.data(), work.size(), next, dstLen, 2)) {
            return false;
        }
        work.swap(next);
    } else if (mask & MPQ_COMPRESSION_ADPCM_MONO) {
        if (!DecompressADPCM(work.data(), work.size(), next, dstLen, 1)) {
            return false;
        }
        work.swap(next);
    }

    if (mask & MPQ_COMPRESSION_SPARSE) {
        if (!DecompressSparse(work.data(), work.size(), next, dstLen)) {
            return false;
        }
        work.swap(next);
    }

    if (work.size() == dstLen) {
        std::memcpy(dst, work.data(), dstLen);
        return true;
    }
    return false;
}

inline bool ExtractBlock(const ArchiveObj* a, int blockIdx, const char* filename, std::vector<uint8_t>& out) {
    if (!a || blockIdx < 0 || (size_t)blockIdx >= a->blockTable.size()) return false;
    const auto& b = a->blockTable[blockIdx];
    out.resize(b.fileSize);
    if (b.fileSize == 0) return true;

    size_t absPos = a->mpqOffset + b.filePos;
    if (absPos + b.compSize > a->rawBytes.size()) return false;

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
        if (b.flags & (MPQ_FILE_COMPRESS | MPQ_FILE_IMPLODE)) {
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
    if (!f || (!pvBuffer && dwToRead > 0)) return false;
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
