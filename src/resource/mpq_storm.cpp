#include "mpq_storm.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif
#ifdef HAVE_BZIP2
#include <bzlib.h>
#endif

namespace fs = std::filesystem;

namespace {

// Archives canonically use backslashes; folders use native separators.
std::string ToArchivePath(const std::string& p) {
    std::string out = p;
    for (auto& c : out) {
        if (c == '/') c = '\\';
    }
    return out;
}

std::string ToNativePath(const std::string& p) {
    std::string out = p;
    for (auto& c : out) {
        if (c == '\\' || c == '/') c = fs::path::preferred_separator;
    }
    return out;
}

bool ReadWholeFile(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    std::streamsize size = f.tellg();
    if (size < 0) return false;
    f.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(size));
    return size == 0 || static_cast<bool>(f.read(reinterpret_cast<char*>(out.data()), size));
}

// Case-insensitive glob matcher supporting '*' and '?' with unified slash comparison.
bool MatchMpqWildcard(const char* pat, const char* str) {
    if (!pat || pat[0] == '\0' || (pat[0] == '*' && pat[1] == '\0')) {
        return true;
    }
    const char* starPat = nullptr;
    const char* starStr = nullptr;
    while (*str) {
        char pc = *pat;
        char sc = *str;
        if (pc == '/') pc = '\\';
        if (sc == '/') sc = '\\';
        pc = static_cast<char>(std::tolower(static_cast<unsigned char>(pc)));
        sc = static_cast<char>(std::tolower(static_cast<unsigned char>(sc)));

        if (pc == '?' || pc == sc) {
            ++pat;
            ++str;
        } else if (*pat == '*') {
            starPat = pat++;
            starStr = str;
        } else if (starPat) {
            pat = starPat + 1;
            str = ++starStr;
        } else {
            return false;
        }
    }
    while (*pat == '*') {
        ++pat;
    }
    return *pat == '\0';
}

// ============================================================================
// PKWARE Data Compression Library (DCL) Explode Decoder (MPQ 0x08 & IMPLODE)
// Implements canonical binary & ASCII DCL decompression with strict bounds checks.
// ============================================================================
class PkwareDclDecoder {
public:
    static bool Explode(const uint8_t* src,
                        size_t srcLen,
                        uint8_t* dst,
                        size_t dstLen,
                        size_t* outActualLen = nullptr) {
        if (!src || !dst || srcLen < 2) {
            return false;
        }
        const uint8_t litCoded = src[0];
        const uint8_t dictBits = src[1];
        if (litCoded > 1 || dictBits < 4 || dictBits > 6) {
            return false;
        }

        static const PkwareDclDecoder kTables;
        BitState bs{src + 2, srcLen - 2, 0, 0, 0};

        const size_t windowMask = (size_t(1) << (dictBits + 6)) - 1u; // 1023, 2047, or 4095
        size_t outPos = 0;

        while (outPos < dstLen) {
            int flag = bs.GetBits(1);
            if (flag < 0) {
                if (outActualLen) {
                    *outActualLen = outPos;
                    return outPos > 0;
                }
                return false;
            }

            if (flag == 0) {
                // Literal byte
                int sym = 0;
                if (litCoded == 0) {
                    sym = bs.GetBits(8);
                } else {
                    sym = kTables.litHuff_.Decode(bs);
                }
                if (sym < 0 || sym > 255) return false;
                dst[outPos++] = static_cast<uint8_t>(sym);
            } else {
                // Length + Distance pair
                int lenIdx = kTables.lenHuff_.Decode(bs);
                if (lenIdx < 0 || lenIdx >= 16) return false;

                static constexpr uint8_t kExLenBits[16] = {
                    0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8
                };
                static constexpr uint16_t kLenBase[16] = {
                    0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 14, 22, 38, 70, 134, 262
                };
                int extra = bs.GetBits(kExLenBits[lenIdx]);
                if (extra < 0) return false;
                const size_t copyLen = static_cast<size_t>(kLenBase[lenIdx] + extra) + 2u;
                if (copyLen == 519u) {
                    // End-of-stream marker
                    break;
                }

                int distIdx = kTables.distHuff_.Decode(bs);
                if (distIdx < 0 || distIdx >= 64) return false;

                const int lowBitsCount = (copyLen == 2u) ? 2 : static_cast<int>(dictBits);
                int lowDist = bs.GetBits(lowBitsCount);
                if (lowDist < 0) return false;

                const size_t distance = ((static_cast<size_t>(distIdx) << lowBitsCount) |
                                         static_cast<size_t>(lowDist)) + 1u;
                if (distance > windowMask + 1u || distance > outPos) {
                    return false;
                }
                if (copyLen > dstLen - outPos) {
                    return false;
                }
                for (size_t i = 0; i < copyLen; ++i) {
                    dst[outPos] = dst[outPos - distance];
                    ++outPos;
                }
            }
        }
        if (outActualLen) {
            *outActualLen = outPos;
            return outPos > 0;
        }
        return outPos == dstLen;
    }

private:
    struct BitState {
        const uint8_t* data = nullptr;
        size_t size = 0;
        size_t pos = 0;
        uint32_t bitBuf = 0;
        int bitCount = 0;

        bool EnsureBits(int n) {
            while (bitCount < n) {
                if (pos >= size) return false;
                bitBuf |= static_cast<uint32_t>(data[pos++]) << bitCount;
                bitCount += 8;
            }
            return true;
        }

        int PeekBits(int n) {
            if (!EnsureBits(n)) return -1;
            return static_cast<int>(bitBuf & ((1u << n) - 1u));
        }

        void DropBits(int n) {
            bitBuf >>= n;
            bitCount -= n;
        }

        int GetBits(int n) {
            if (n == 0) return 0;
            int v = PeekBits(n);
            if (v >= 0) DropBits(n);
            return v;
        }
    };

    struct CanonicalHuff {
        static constexpr int kMaxBits = 13;
        std::vector<int16_t> lutSym;
        std::vector<uint8_t> lutLen;
        int maxBits = 0;

        void BuildFromLengths(const uint8_t* bitLengths, size_t symbolCount) {
            maxBits = 0;
            int count[kMaxBits + 1] = {};
            for (size_t i = 0; i < symbolCount; ++i) {
                const int b = bitLengths[i];
                if (b > 0 && b <= kMaxBits) {
                    ++count[b];
                    if (b > maxBits) maxBits = b;
                }
            }
            // PKWARE DCL assigns canonical codes from longest bit-length to shortest
            // (or equivalently inverted bit-reversed canonical codes). Let's build from
            // the exact PKWARE DCL canonical rule: codes start at 0 for longest length?
            // No: in Mark Adler's blast.c, PKWARE DCL assigns canonical codes in normal
            // shortest-to-longest order, then complements (`~code`) and bit-reverses!
            int nextCode[kMaxBits + 1] = {};
            int code = 0;
            for (int bits = 1; bits <= maxBits; ++bits) {
                code = (code + count[bits - 1]) << 1;
                nextCode[bits] = code;
            }

            const size_t tableSize = size_t(1) << maxBits;
            lutSym.assign(tableSize, -1);
            lutLen.assign(tableSize, 0);

            for (size_t sym = 0; sym < symbolCount; ++sym) {
                const int len = bitLengths[sym];
                if (len <= 0 || len > maxBits) continue;
                const uint32_t rawCode = static_cast<uint32_t>(nextCode[len]++);
                // Complement and bit-reverse `len` bits (PKWARE DCL wire format)
                const uint32_t comp = (~rawCode) & ((1u << len) - 1u);
                uint32_t rev = 0;
                for (int b = 0; b < len; ++b) {
                    if (comp & (1u << b)) {
                        rev |= (1u << (len - 1 - b));
                    }
                }
                const size_t step = size_t(1) << len;
                for (size_t idx = rev; idx < tableSize; idx += step) {
                    lutSym[idx] = static_cast<int16_t>(sym);
                    lutLen[idx] = static_cast<uint8_t>(len);
                }
            }
        }

        int Decode(BitState& bs) const {
            while (bs.bitCount < maxBits && bs.pos < bs.size) {
                bs.bitBuf |= static_cast<uint32_t>(bs.data[bs.pos++]) << bs.bitCount;
                bs.bitCount += 8;
            }
            const uint32_t mask = (1u << maxBits) - 1u;
            const size_t idx = static_cast<size_t>(bs.bitBuf & mask);
            const int sym = lutSym[idx];
            const int len = lutLen[idx];
            if (sym < 0 || len == 0 || len > bs.bitCount) {
                return -1;
            }
            bs.DropBits(len);
            return sym;
        }
    };

    PkwareDclDecoder() {
        static constexpr uint8_t kLenBits[16] = {
            3, 2, 3, 3, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 7, 7
        };
        static constexpr uint8_t kDistBits[64] = {
            2, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6,
            6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
            7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
            8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8
        };
        // Standard PKWARE DCL ASCII literal bit-lengths (256 entries, from RFC / blast.c)
        static constexpr uint8_t kLitBits[256] = {
            11,12,12,12,12,12,12,12,12, 8, 7,12,12, 7,12,12,
            12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,
             4,10, 8,12,10,12,10, 8, 7, 7, 8, 9, 7, 6, 7, 8,
             7, 6, 7, 7, 7, 7, 8, 7, 7, 8, 8, 9,11, 8,11,10,
            12, 6, 7, 6, 6, 5, 7, 8, 8, 6,11, 9, 6, 7, 6, 6,
             7,11, 6, 6, 6, 7, 9, 8, 9, 9,11, 8,11, 9,12, 8,
            12, 5, 6, 6, 6, 5, 6, 6, 6, 5,11, 7, 5, 6, 5, 5,
             6,10, 5, 5, 5, 5, 8, 7, 8, 7,10,11,11,11,12,12,
            13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,
            13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,
            13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,
            13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,
            13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,
            13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,
            13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,
            13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13
        };
        lenHuff_.BuildFromLengths(kLenBits, 16);
        distHuff_.BuildFromLengths(kDistBits, 64);
        litHuff_.BuildFromLengths(kLitBits, 256);
    }

    CanonicalHuff lenHuff_;
    CanonicalHuff distHuff_;
    CanonicalHuff litHuff_;
};

// ============================================================================
// Blizzard MPQ IMA ADPCM Audio Decoder (0x40 Mono / 0x80 Stereo)
// Decodes Warcraft III WAV sectors compressed with composite Zlib/PKWARE + ADPCM.
// ============================================================================
class MpqImaAdpcmDecoder {
public:
    static bool Decompress(const uint8_t* src,
                           size_t srcLen,
                           uint8_t* dst,
                           size_t dstLen,
                           int channelCount) {
        if (!src || !dst || (channelCount != 1 && channelCount != 2)) {
            return false;
        }
        const size_t headerBytes = 2u + static_cast<size_t>(channelCount) * 2u;
        if (srcLen < headerBytes || dstLen < static_cast<size_t>(channelCount) * 2u || (dstLen & 1u) != 0u) {
            return false;
        }

        static constexpr int kStepSizeTable[89] = {
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
        static constexpr int kNextStepTable[32] = {
            -1, 0, -1, 4, -1, 2, -1, 6,
            -1, 1, -1, 5, -1, 3, -1, 7,
            -1, 1, -1, 5, -1, 3, -1, 7,
            -1, 2, -1, 4, -1, 6, -1, 8
        };

        const uint8_t bitShift = src[1];
        if (bitShift > 16u) {
            return false;
        }

        int predicted[2] = {0, 0};
        int stepIndex[2] = {0x2C, 0x2C};

        size_t inPos = 2u;
        size_t outPos = 0u;
        for (int ch = 0; ch < channelCount; ++ch) {
            int16_t initSample = 0;
            std::memcpy(&initSample, src + inPos, 2u);
            inPos += 2u;
            predicted[ch] = static_cast<int>(initSample);
            std::memcpy(dst + outPos, &initSample, 2u);
            outPos += 2u;
        }

        int ch = channelCount - 1;
        while (inPos < srcLen && outPos < dstLen) {
            ch = (ch + 1) % channelCount;
            const uint8_t code = src[inPos++];

            if ((code & 0x80u) != 0u) {
                switch (code & 0x7Fu) {
                    case 0x00u: {
                        if (stepIndex[ch] != 0) {
                            --stepIndex[ch];
                        }
                        if (outPos + 2u > dstLen) return false;
                        const int16_t s = static_cast<int16_t>(predicted[ch]);
                        std::memcpy(dst + outPos, &s, 2u);
                        outPos += 2u;
                        break;
                    }
                    case 0x01u: {
                        stepIndex[ch] = std::min(88, stepIndex[ch] + 8);
                        ch = (ch + channelCount - 1) % channelCount;
                        break;
                    }
                    case 0x02u: {
                        return outPos == dstLen;
                    }
                    default: {
                        stepIndex[ch] = std::max(0, stepIndex[ch] - 8);
                        ch = (ch + channelCount - 1) % channelCount;
                        break;
                    }
                }
            } else {
                const int stepSize = kStepSizeTable[stepIndex[ch]];
                int diff = stepSize >> bitShift;
                for (int bit = 0; bit < 6; ++bit) {
                    if ((code & (1u << bit)) != 0u) {
                        diff += (stepSize >> bit);
                    }
                }
                int pred = predicted[ch];
                if ((code & 0x40u) != 0u) {
                    pred -= diff;
                } else {
                    pred += diff;
                }
                pred = std::clamp(pred, -32768, 32767);
                predicted[ch] = pred;

                if (outPos + 2u > dstLen) return false;
                const int16_t s = static_cast<int16_t>(pred);
                std::memcpy(dst + outPos, &s, 2u);
                outPos += 2u;

                stepIndex[ch] = std::clamp(stepIndex[ch] + kNextStepTable[code & 0x1Fu], 0, 88);
            }
        }
        return outPos == dstLen;
    }
};

// ============================================================================
// Streaming, Bounds-Safe Native MPQ v1/v2 Reader (used when WITHOUT_STORMLIB=1)
//
// Reads ONLY the 32-byte MPQ header, hash table, and block table on Open(),
// and streams individual compressed/encrypted sectors on demand via FILE* seek.
// ============================================================================
class NativeMpqArchive {
public:
    struct HashEntry {
        uint32_t hashA = 0;
        uint32_t hashB = 0;
        uint16_t locale = 0;
        uint16_t platform = 0;
        uint32_t blockIndex = 0;
    };

    struct BlockEntry {
        uint32_t filePos = 0;
        uint32_t compressedSize = 0;
        uint32_t fileSize = 0;
        uint32_t flags = 0;
    };

    static constexpr uint32_t kFlagImplode     = 0x00000100u;
    static constexpr uint32_t kFlagCompress    = 0x00000200u;
    static constexpr uint32_t kFlagEncrypted   = 0x00010000u;
    static constexpr uint32_t kFlagFixKey      = 0x00020000u;
    static constexpr uint32_t kFlagSingleUnit  = 0x01000000u;
    static constexpr uint32_t kFlagSectorCrc   = 0x04000000u;
    static constexpr uint32_t kFlagExists      = 0x80000000u;

    NativeMpqArchive() = default;
    ~NativeMpqArchive() { Close(); }

    NativeMpqArchive(const NativeMpqArchive&) = delete;
    NativeMpqArchive& operator=(const NativeMpqArchive&) = delete;

    void Close() {
        std::lock_guard<std::mutex> lock(ioMutex_);
        if (fp_) {
            std::fclose(fp_);
            fp_ = nullptr;
        }
        archiveFileSize_ = 0;
        mpqOffset_ = 0;
        sectorSize_ = 4096;
        hashTable_.clear();
        blockTable_.clear();
        listFiles_.clear();
    }

    static const uint32_t* CryptTable() {
        static uint32_t tbl[0x500];
        static bool inited = false;
        if (!inited) {
            uint32_t seed = 0x00100001u;
            for (uint32_t idx1 = 0; idx1 < 0x100u; ++idx1) {
                uint32_t idx2 = idx1;
                for (int i = 0; i < 5; ++i, idx2 += 0x100u) {
                    seed = (seed * 125u + 3u) % 0x2AAAABu;
                    const uint32_t temp1 = (seed & 0xFFFFu) << 16;
                    seed = (seed * 125u + 3u) % 0x2AAAABu;
                    const uint32_t temp2 = (seed & 0xFFFFu);
                    tbl[idx2] = temp1 | temp2;
                }
            }
            inited = true;
        }
        return tbl;
    }

    static uint32_t HashString(const std::string& str, uint32_t hashType) {
        const uint32_t* tbl = CryptTable();
        uint32_t seed1 = 0x7FED7FEDu;
        uint32_t seed2 = 0xEEEEEEEEu;
        for (unsigned char ch : str) {
            unsigned char c = static_cast<unsigned char>(std::toupper(ch));
            if (c == '/') c = '\\';
            seed1 = tbl[(hashType << 8) + c] ^ (seed1 + seed2);
            seed2 = c + seed1 + seed2 + (seed2 << 5) + 3u;
        }
        return seed1;
    }

    static void DecryptBlock(void* data, size_t byteLen, uint32_t key) {
        const uint32_t* tbl = CryptTable();
        uint32_t seed = 0xEEEEEEEEu;
        auto* dwords = static_cast<uint32_t*>(data);
        const size_t count = byteLen / 4u;
        for (size_t i = 0; i < count; ++i) {
            seed += tbl[0x400u + (key & 0xFFu)];
            uint32_t ch = dwords[i] ^ (key + seed);
            key = ((~key << 0x15) + 0x11111111u) | (key >> 0x0B);
            seed = ch + seed + (seed << 5) + 3u;
            dwords[i] = ch;
        }
    }

    bool Open(const std::string& filePath) {
        Close();

        FILE* fp = std::fopen(filePath.c_str(), "rb");
        if (!fp) return false;

        if (!Seek64(fp, 0, SEEK_END)) {
            std::fclose(fp);
            return false;
        }
        const int64_t rawFileSize = Tell64(fp);
        if (rawFileSize < 32) {
            std::fclose(fp);
            return false;
        }

        fp_ = fp;
        archiveFileSize_ = static_cast<uint64_t>(rawFileSize);

        // Scan 512-byte boundaries for 'MPQ\x1A' (supports pure .mpq at 0 and
        // Warcraft III .w3x/.w3m archives with a 512-byte 'HM3W' outer header).
        static constexpr uint64_t kMaxHeaderSearchOffset = 16ull * 1024ull * 1024ull;
        const uint64_t searchLimit = std::min<uint64_t>(
            archiveFileSize_ - 32ull, kMaxHeaderSearchOffset);

        uint8_t hdr[32] = {};
        bool foundMpqMagic = false;

        for (uint64_t off = 0; off <= searchLimit; off += 512ull) {
            if (!ReadAtUnlocked(off, hdr, 32)) {
                Close();
                return false;
            }
            if (off == 0 && std::memcmp(hdr, "HM3W", 4) == 0) {
                continue;
            }
            if (hdr[0] == 'M' && hdr[1] == 'P' && hdr[2] == 'Q' && hdr[3] == 0x1A) {
                mpqOffset_ = off;
                foundMpqMagic = true;
                break;
            }
        }

        if (!foundMpqMagic) {
            Close();
            return false;
        }

        uint32_t headerSize = 0;
        uint16_t formatVersion = 0;
        uint16_t blockSizeShift = 0;
        uint32_t hashTableOffset = 0;
        uint32_t blockTableOffset = 0;
        uint32_t hashEntries = 0;
        uint32_t blockEntries = 0;

        std::memcpy(&headerSize, hdr + 4, 4);
        std::memcpy(&formatVersion, hdr + 12, 2);
        std::memcpy(&blockSizeShift, hdr + 14, 2);
        std::memcpy(&hashTableOffset, hdr + 16, 4);
        std::memcpy(&blockTableOffset, hdr + 20, 4);
        std::memcpy(&hashEntries, hdr + 24, 4);
        std::memcpy(&blockEntries, hdr + 28, 4);

        // Strict validation of untrusted archive header fields:
        // 1. blockSizeShift <= 15 prevents C++ shift UB and sectorSize_ == 0 divide-by-zero.
        // 2. hashEntries must be non-zero, <= 1M, and an exact power of 2 for mask probing.
        // 3. blockEntries must be <= 1M.
        if (headerSize < 32u ||
            blockSizeShift > 15u ||
            hashEntries == 0u ||
            hashEntries > (1u << 20) ||
            (hashEntries & (hashEntries - 1u)) != 0u ||
            blockEntries > (1u << 20)) {
            Close();
            return false;
        }

        sectorSize_ = 512u << blockSizeShift;

        uint64_t hashOffset64 = mpqOffset_ + static_cast<uint64_t>(hashTableOffset);
        uint64_t blockOffset64 = mpqOffset_ + static_cast<uint64_t>(blockTableOffset);

        // Optional MPQ v2+ extended 16-bit high table offsets
        if (formatVersion >= 1u && headerSize >= 44u && mpqOffset_ + 44ull <= archiveFileSize_) {
            uint8_t extHdr[12] = {};
            if (ReadAtUnlocked(mpqOffset_ + 32ull, extHdr, 12)) {
                uint16_t hashPosHi = 0;
                uint16_t blockPosHi = 0;
                std::memcpy(&hashPosHi, extHdr + 8, 2);
                std::memcpy(&blockPosHi, extHdr + 10, 2);
                hashOffset64 += (static_cast<uint64_t>(hashPosHi) << 32);
                blockOffset64 += (static_cast<uint64_t>(blockPosHi) << 32);
            }
        }

        const uint64_t hashBytes = static_cast<uint64_t>(hashEntries) * sizeof(HashEntry);
        const uint64_t blockBytes = static_cast<uint64_t>(blockEntries) * sizeof(BlockEntry);

        if (hashBytes > archiveFileSize_ || hashOffset64 > archiveFileSize_ - hashBytes ||
            blockBytes > archiveFileSize_ || blockOffset64 > archiveFileSize_ - blockBytes) {
            Close();
            return false;
        }

        hashTable_.resize(hashEntries);
        if (!ReadAtUnlocked(hashOffset64, hashTable_.data(), static_cast<size_t>(hashBytes))) {
            Close();
            return false;
        }
        DecryptBlock(hashTable_.data(), static_cast<size_t>(hashBytes),
                     HashString("(hash table)", 3));

        blockTable_.resize(blockEntries);
        if (blockEntries > 0u) {
            if (!ReadAtUnlocked(blockOffset64, blockTable_.data(), static_cast<size_t>(blockBytes))) {
                Close();
                return false;
            }
            DecryptBlock(blockTable_.data(), static_cast<size_t>(blockBytes),
                         HashString("(block table)", 3));
        }

        // Parse optional (listfile) so ListFiles() / ListAllFiles() work natively
        std::vector<uint8_t> lfBytes;
        if (ReadFile("(listfile)", lfBytes) && !lfBytes.empty()) {
            std::string text(reinterpret_cast<const char*>(lfBytes.data()), lfBytes.size());
            std::istringstream iss(text);
            std::string line;
            while (std::getline(iss, line)) {
                while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
                    line.pop_back();
                }
                if (!line.empty()) {
                    listFiles_.push_back(line);
                }
            }
        }
        return true;
    }

    int FindBlockIndex(const std::string& filename) const {
        if (hashTable_.empty()) return -1;
        const uint32_t mask = static_cast<uint32_t>(hashTable_.size() - 1u);
        const uint32_t start = HashString(filename, 0) & mask;
        const uint32_t nameA = HashString(filename, 1);
        const uint32_t nameB = HashString(filename, 2);

        uint32_t idx = start;
        for (size_t step = 0; step < hashTable_.size(); ++step) {
            const HashEntry& he = hashTable_[idx];
            if (he.blockIndex == 0xFFFFFFFFu) {
                return -1; // Never-occupied slot ends probe chain
            }
            if (he.blockIndex != 0xFFFFFFFEu && he.hashA == nameA && he.hashB == nameB) {
                if (he.blockIndex < blockTable_.size() &&
                    (blockTable_[he.blockIndex].flags & kFlagExists) != 0u) {
                    return static_cast<int>(he.blockIndex);
                }
            }
            idx = (idx + 1u) & mask;
        }
        return -1;
    }

    bool HasFile(const std::string& filename) const {
        return FindBlockIndex(filename) >= 0;
    }

    bool ReadFile(const std::string& filename, std::vector<uint8_t>& out) const {
        out.clear();
        const int bidx = FindBlockIndex(filename);
        if (bidx < 0) return false;

        const BlockEntry& blk = blockTable_[static_cast<size_t>(bidx)];
        if (blk.fileSize == 0u) {
            return true;
        }
        if (sectorSize_ == 0u) {
            return false;
        }

        const uint64_t blockStart = mpqOffset_ + static_cast<uint64_t>(blk.filePos);
        const uint64_t compBytes = static_cast<uint64_t>(blk.compressedSize);
        if (compBytes > archiveFileSize_ || blockStart > archiveFileSize_ - compBytes) {
            return false;
        }

        // Compute file decryption key when MPQ_FILE_ENCRYPTED is set
        uint32_t fileKey = 0;
        if ((blk.flags & kFlagEncrypted) != 0u) {
            std::string base = filename;
            const size_t slash = base.find_last_of("\\/");
            if (slash != std::string::npos) {
                base = base.substr(slash + 1u);
            }
            fileKey = HashString(base, 3);
            if ((blk.flags & kFlagFixKey) != 0u) {
                fileKey = (fileKey + blk.filePos) ^ blk.fileSize;
            }
        }

        const bool hasCompressFlag = (blk.flags & kFlagCompress) != 0u;
        const bool hasImplodeFlag  = (blk.flags & kFlagImplode) != 0u;
        const bool isCompressed    = hasCompressFlag || hasImplodeFlag;
        const bool isSingleUnit    = (blk.flags & kFlagSingleUnit) != 0u;

        out.resize(blk.fileSize);

        auto decompressChunk = [&](const uint8_t* src,
                                   size_t srcLen,
                                   uint8_t* dst,
                                   size_t dstLen,
                                   bool sectorIsCompressed) -> bool {
            if (!sectorIsCompressed) {
                // Strict equality check prevents any heap over-read when block metadata is malformed
                if (srcLen != dstLen) {
                    return false;
                }
                std::memcpy(dst, src, dstLen);
                return true;
            }
            if (srcLen == 0u) {
                return false;
            }

            // Legacy MPQ_FILE_IMPLODE (0x00000100): raw PKWARE DCL stream with no mask prefix byte
            if (hasImplodeFlag && !hasCompressFlag) {
                return PkwareDclDecoder::Explode(src, srcLen, dst, dstLen);
            }

            // MPQ_FILE_COMPRESS (0x00000200): 1-byte compression bitmask prefix
            const uint8_t compMask = src[0];
            const uint8_t* payload = src + 1;
            const size_t payloadLen = srcLen - 1u;

            if (compMask == 0x00u) {
                if (payloadLen != dstLen) return false;
                std::memcpy(dst, payload, dstLen);
                return true;
            }
            static constexpr uint8_t kSupportedMask = 0x10u | 0x08u | 0x02u | 0x80u | 0x40u;
            if ((compMask & ~kSupportedMask) != 0u || (compMask & 0xC0u) == 0xC0u) {
                return false;
            }

            uint8_t remaining = compMask;
            const uint8_t* stageIn = payload;
            size_t stageInLen = payloadLen;
            std::vector<uint8_t> scratchA;
            std::vector<uint8_t> scratchB;
            bool useA = true;

            auto allocStageBuf = [&](size_t cap) -> uint8_t* {
                std::vector<uint8_t>& buf = useA ? scratchA : scratchB;
                useA = !useA;
                buf.resize(cap);
                return buf.data();
            };

            if ((remaining & 0x10u) != 0u) {
                remaining &= static_cast<uint8_t>(~0x10u);
#ifdef HAVE_BZIP2
                const size_t cap = (remaining == 0u) ? dstLen : (dstLen * 2u + 64u);
                uint8_t* stageOut = (remaining == 0u) ? dst : allocStageBuf(cap);
                unsigned int dLen = static_cast<unsigned int>(cap);
                const int rc = BZ2_bzBuffToBuffDecompress(
                    reinterpret_cast<char*>(stageOut),
                    &dLen,
                    const_cast<char*>(reinterpret_cast<const char*>(stageIn)),
                    static_cast<unsigned int>(stageInLen),
                    0,
                    0);
                if (rc != BZ_OK) return false;
                if (remaining == 0u && static_cast<size_t>(dLen) != dstLen) return false;
                stageIn = stageOut;
                stageInLen = static_cast<size_t>(dLen);
#else
                return false;
#endif
            }

            if ((remaining & 0x08u) != 0u) {
                remaining &= static_cast<uint8_t>(~0x08u);
                if (remaining == 0u) {
                    if (!PkwareDclDecoder::Explode(stageIn, stageInLen, dst, dstLen, nullptr)) {
                        return false;
                    }
                    stageIn = dst;
                    stageInLen = dstLen;
                } else {
                    const size_t cap = dstLen * 2u + 64u;
                    uint8_t* stageOut = allocStageBuf(cap);
                    size_t actualLen = 0u;
                    if (!PkwareDclDecoder::Explode(stageIn, stageInLen, stageOut, cap, &actualLen)) {
                        return false;
                    }
                    stageIn = stageOut;
                    stageInLen = actualLen;
                }
            }

            if ((remaining & 0x02u) != 0u) {
                remaining &= static_cast<uint8_t>(~0x02u);
#ifdef HAVE_ZLIB
                const size_t cap = (remaining == 0u) ? dstLen : (dstLen * 2u + 64u);
                uint8_t* stageOut = (remaining == 0u) ? dst : allocStageBuf(cap);
                uLongf dLen = static_cast<uLongf>(cap);
                const int rc = uncompress(stageOut, &dLen, stageIn, static_cast<uLong>(stageInLen));
                if (rc != Z_OK) return false;
                if (remaining == 0u && static_cast<size_t>(dLen) != dstLen) return false;
                stageIn = stageOut;
                stageInLen = static_cast<size_t>(dLen);
#else
                return false;
#endif
            }

            if ((remaining & 0x80u) != 0u) {
                remaining &= static_cast<uint8_t>(~0x80u);
                if (remaining != 0u) return false;
                return MpqImaAdpcmDecoder::Decompress(stageIn, stageInLen, dst, dstLen, 2);
            }

            if ((remaining & 0x40u) != 0u) {
                remaining &= static_cast<uint8_t>(~0x40u);
                if (remaining != 0u) return false;
                return MpqImaAdpcmDecoder::Decompress(stageIn, stageInLen, dst, dstLen, 1);
            }

            return remaining == 0u;
        };

        if (!isCompressed || isSingleUnit) {
            std::vector<uint8_t> temp(blk.compressedSize);
            if (!ReadAtThreadSafe(blockStart, temp.data(), temp.size())) {
                out.clear();
                return false;
            }
            if ((blk.flags & kFlagEncrypted) != 0u) {
                DecryptBlock(temp.data(), temp.size(), fileKey);
            }
            const bool chunkCompressed = isCompressed && (blk.compressedSize < blk.fileSize);
            if (!decompressChunk(temp.data(), temp.size(), out.data(), out.size(), chunkCompressed)) {
                out.clear();
                return false;
            }
            return true;
        }

        // Sector-table multi-sector MPQ stream
        const uint32_t numSectors =
            static_cast<uint32_t>((static_cast<uint64_t>(blk.fileSize) + sectorSize_ - 1ull) / sectorSize_);
        const uint32_t tableEntries = numSectors + (((blk.flags & kFlagSectorCrc) != 0u) ? 2u : 1u);
        const uint64_t tableBytes = static_cast<uint64_t>(tableEntries) * sizeof(uint32_t);
        if (tableBytes > blk.compressedSize) {
            out.clear();
            return false;
        }

        std::vector<uint32_t> sectorOffsets(tableEntries);
        if (!ReadAtThreadSafe(blockStart, sectorOffsets.data(), static_cast<size_t>(tableBytes))) {
            out.clear();
            return false;
        }
        if ((blk.flags & kFlagEncrypted) != 0u) {
            DecryptBlock(sectorOffsets.data(), static_cast<size_t>(tableBytes), fileKey - 1u);
        }

        std::vector<uint8_t> sectorBuf;
        size_t written = 0;
        for (uint32_t s = 0; s < numSectors; ++s) {
            const uint32_t secStart = sectorOffsets[s];
            const uint32_t secEnd = sectorOffsets[s + 1u];
            if (secEnd < secStart || secEnd > blk.compressedSize) {
                out.clear();
                return false;
            }
            const uint32_t secCompLen = secEnd - secStart;
            const size_t rawLen = std::min<size_t>(sectorSize_, blk.fileSize - written);
            if (secCompLen > rawLen) {
                out.clear();
                return false;
            }

            sectorBuf.resize(secCompLen);
            if (secCompLen > 0u &&
                !ReadAtThreadSafe(blockStart + secStart, sectorBuf.data(), secCompLen)) {
                out.clear();
                return false;
            }
            if ((blk.flags & kFlagEncrypted) != 0u) {
                DecryptBlock(sectorBuf.data(), sectorBuf.size(), fileKey + s);
            }
            const bool sectorCompressed = (secCompLen < rawLen);
            if (!decompressChunk(sectorBuf.data(),
                                 sectorBuf.size(),
                                 out.data() + written,
                                 rawLen,
                                 sectorCompressed)) {
                out.clear();
                return false;
            }
            written += rawLen;
        }
        return written == blk.fileSize;
    }

    const std::vector<std::string>& GetListFiles() const { return listFiles_; }

private:
    static bool Seek64(FILE* fp, int64_t offset, int origin) {
#if defined(_WIN32)
        return _fseeki64(fp, offset, origin) == 0;
#else
        return fseeko(fp, static_cast<off_t>(offset), origin) == 0;
#endif
    }

    static int64_t Tell64(FILE* fp) {
#if defined(_WIN32)
        return _ftelli64(fp);
#else
        return static_cast<int64_t>(ftello(fp));
#endif
    }

    bool ReadAtUnlocked(uint64_t offset, void* dst, size_t len) const {
        if (!fp_) return false;
        if (len == 0u) return true;
        if (len > archiveFileSize_ || offset > archiveFileSize_ - len) return false;
        if (!Seek64(fp_, static_cast<int64_t>(offset), SEEK_SET)) return false;
        return std::fread(dst, 1u, len, fp_) == len;
    }

    bool ReadAtThreadSafe(uint64_t offset, void* dst, size_t len) const {
        std::lock_guard<std::mutex> lock(ioMutex_);
        return ReadAtUnlocked(offset, dst, len);
    }

    mutable std::mutex ioMutex_;
    FILE* fp_ = nullptr;
    uint64_t archiveFileSize_ = 0;
    uint64_t mpqOffset_ = 0;
    uint32_t sectorSize_ = 4096;
    std::vector<HashEntry> hashTable_;
    std::vector<BlockEntry> blockTable_;
    std::vector<std::string> listFiles_;
};

// Pre-flight MPQ header validation applied uniformly across both NativeMpqArchive
// and StormLib builds so malformed/fuzzed headers (e.g. non-power-of-two hash table
// size or invalid sector shift) are rejected deterministically.
bool ValidateMpqHeaderInvariants(const std::string& filePath) {
    FILE* fp = std::fopen(filePath.c_str(), "rb");
    if (!fp) return false;
#if defined(_WIN32)
    if (_fseeki64(fp, 0, SEEK_END) != 0) {
        std::fclose(fp);
        return false;
    }
    const int64_t rawSize = _ftelli64(fp);
#else
    if (fseeko(fp, 0, SEEK_END) != 0) {
        std::fclose(fp);
        return false;
    }
    const int64_t rawSize = static_cast<int64_t>(ftello(fp));
#endif
    if (rawSize < 32) {
        std::fclose(fp);
        return false;
    }
    const uint64_t fileSize = static_cast<uint64_t>(rawSize);
    static constexpr uint64_t kMaxSearch = 16ull * 1024ull * 1024ull;
    const uint64_t searchLimit = std::min<uint64_t>(fileSize - 32ull, kMaxSearch);

    uint8_t hdr[32] = {};
    bool found = false;
    for (uint64_t off = 0; off <= searchLimit; off += 512ull) {
#if defined(_WIN32)
        if (_fseeki64(fp, static_cast<int64_t>(off), SEEK_SET) != 0) break;
#else
        if (fseeko(fp, static_cast<off_t>(off), SEEK_SET) != 0) break;
#endif
        if (std::fread(hdr, 1u, 32u, fp) != 32u) break;
        if (off == 0 && std::memcmp(hdr, "HM3W", 4) == 0) continue;
        if (hdr[0] == 'M' && hdr[1] == 'P' && hdr[2] == 'Q' && hdr[3] == 0x1A) {
            found = true;
            break;
        }
    }
    std::fclose(fp);
    if (!found) {
        return false;
    }

    uint32_t headerSize = 0;
    uint16_t blockSizeShift = 0;
    uint32_t hashEntries = 0;
    uint32_t blockEntries = 0;
    std::memcpy(&headerSize, hdr + 4, 4);
    std::memcpy(&blockSizeShift, hdr + 14, 2);
    std::memcpy(&hashEntries, hdr + 24, 4);
    std::memcpy(&blockEntries, hdr + 28, 4);

    if (headerSize < 32u ||
        blockSizeShift > 15u ||
        hashEntries == 0u ||
        hashEntries > (1u << 22) ||
        (hashEntries & (hashEntries - 1u)) != 0u ||
        blockEntries > (1u << 22)) {
        return false;
    }
    return true;
}

bool OpenArchiveImpl(const std::string& path, HANDLE& outHandle) {
#ifdef WITHOUT_STORMLIB
    auto* native = new NativeMpqArchive();
    if (!native->Open(path)) {
        delete native;
        return false;
    }
    outHandle = static_cast<HANDLE>(native);
    return true;
#else
    if (!ValidateMpqHeaderInvariants(path)) {
        return false;
    }
    return SFileOpenArchive(path.c_str(), 0, MPQ_OPEN_READ_ONLY, &outHandle);
#endif
}

void CloseArchiveImpl(HANDLE handle) {
    if (!handle) return;
#ifdef WITHOUT_STORMLIB
    delete static_cast<NativeMpqArchive*>(handle);
#else
    SFileCloseArchive(handle);
#endif
}

bool ArchiveHasFileImpl(HANDLE handle, const std::string& key) {
    if (!handle) return false;
#ifdef WITHOUT_STORMLIB
    return static_cast<const NativeMpqArchive*>(handle)->HasFile(key);
#else
    return SFileHasFile(handle, key.c_str());
#endif
}

bool ReadFromArchive(const MpqArchive* archive,
                     const std::string& filename,
                     std::vector<uint8_t>& out) {
    if (!archive || !archive->handle) return false;
    const std::string key = ToArchivePath(filename);
#ifdef WITHOUT_STORMLIB
    return static_cast<const NativeMpqArchive*>(archive->handle)->ReadFile(key, out);
#else
    HANDLE hFile = nullptr;
    if (!SFileOpenFileEx(archive->handle, key.c_str(), SFILE_OPEN_BASE_FILE, &hFile)) {
        return false;
    }
    DWORD fileSize = SFileGetFileSize(hFile, nullptr);
    if (fileSize == 0xFFFFFFFFu) {
        SFileCloseFile(hFile);
        return false;
    }
    if (fileSize == 0) {
        out.clear();
        SFileCloseFile(hFile);
        return true;
    }
    out.resize(fileSize);
    DWORD bytesRead = 0;
    const bool ok = SFileReadFile(hFile, out.data(), fileSize, &bytesRead, nullptr);
    SFileCloseFile(hFile);
    if (!ok) out.clear();
    return ok;
#endif
}

void ListArchiveWithPattern(HANDLE handle,
                            const std::string& pattern,
                            std::vector<std::string>& outResult) {
    if (!handle) return;
#ifdef WITHOUT_STORMLIB
    const auto* native = static_cast<const NativeMpqArchive*>(handle);
    for (const auto& name : native->GetListFiles()) {
        if (MatchMpqWildcard(pattern.c_str(), name.c_str())) {
            outResult.push_back(name);
        }
    }
#else
    SFILE_FIND_DATA fd;
    const char* pat = pattern.empty() ? "*" : pattern.c_str();
    HANDLE hFind = SFileFindFirstFile(handle, pat, &fd, nullptr);
    if (!hFind) return;
    do {
        outResult.push_back(fd.cFileName);
    } while (SFileFindNextFile(hFind, &fd));
    SFileFindClose(hFind);
#endif
}

} // namespace

MpqManager::~MpqManager() {
    lifetimeToken_.reset();
    CloseAll();
}

bool MpqManager::Open(const std::string& path) {
    for (auto& a : archives_) {
        if (a.path == path) return true;
    }
    HANDLE h = nullptr;
    if (!OpenArchiveImpl(path, h)) {
        std::cerr << "MpqStorm: failed to open " << path << std::endl;
        return false;
    }
    archives_.push_back({path, h});
    std::cout << "MpqStorm: opened " << path << std::endl;
    return true;
}

bool MpqManager::OpenCasc(const std::string& storagePath) {
    for (auto& c : cascStorages_) {
        if (c.Path() == storagePath) return true;
    }
    CascStorage storage;
    if (!storage.Open(storagePath)) {
        std::cerr << "MpqStorm: failed to open CASC storage " << storagePath << std::endl;
        return false;
    }
    cascStorages_.push_back(std::move(storage));
    return true;
}

const CascStorage* MpqManager::FindFileInCasc(const std::string& filename) const {
    const std::string key = ToArchivePath(filename);
    for (const auto& c : cascStorages_) {
        if (c.FileExists(key)) return &c;
    }
    return nullptr;
}

void MpqManager::CloseAll() {
    for (auto& a : archives_) {
        CloseArchiveImpl(a.handle);
    }
    archives_.clear();
    for (auto& c : cascStorages_) {
        c.Close();
    }
    cascStorages_.clear();
    CloseTop();
}

bool MpqManager::OpenOnTop(const std::string& path) {
    for (const auto& a : topArchives_) {
        if (a.path == path) return true;
    }
    HANDLE h = nullptr;
    if (!OpenArchiveImpl(path, h)) {
        std::cerr << "MpqStorm: failed to mount " << path << std::endl;
        return false;
    }
    topArchives_.push_back({path, h});
    std::cout << "MpqStorm: mounted on top " << path << std::endl;
    return true;
}

void MpqManager::CloseTop() {
    for (auto& a : topArchives_) {
        CloseArchiveImpl(a.handle);
    }
    topArchives_.clear();
}

const MpqArchive* MpqManager::FindFileInArchives(const std::string& filename) const {
    if (const MpqArchive* top = FindFileInTopArchives(filename)) return top;
    return FindFileInGameArchives(filename);
}

const MpqArchive* MpqManager::FindFileInTopArchives(const std::string& filename) const {
    const std::string key = ToArchivePath(filename);
    // Map mounts shadow game archives (last mounted wins).
    for (auto it = topArchives_.rbegin(); it != topArchives_.rend(); ++it) {
        if (ArchiveHasFileImpl(it->handle, key)) return &(*it);
    }
    return nullptr;
}

const MpqArchive* MpqManager::FindFileInGameArchives(const std::string& filename) const {
    const std::string key = ToArchivePath(filename);
    // Game archives: first opened wins so patches beat base data.
    for (const auto& a : archives_) {
        if (ArchiveHasFileImpl(a.handle, key)) return &a;
    }
    return nullptr;
}

bool MpqManager::AddFolder(const std::string& path) {
    for (const auto& f : folders_) {
        if (f == path) return true;
    }
    std::error_code ec;
    if (!fs::is_directory(path, ec)) {
        std::cerr << "MpqStorm: folder overlay not a directory: " << path << std::endl;
        return false;
    }
    folders_.push_back(path);
    std::cout << "MpqStorm: folder overlay " << path << std::endl;
    return true;
}

bool MpqManager::FindFileInFolders(const std::string& filename, std::string& outPath) const {
    const std::string rel = ToNativePath(filename);
    for (auto it = folders_.rbegin(); it != folders_.rend(); ++it) {
        fs::path candidate = fs::path(*it) / rel;
        std::error_code ec;
        if (fs::is_regular_file(candidate, ec)) {
            outPath = candidate.string();
            return true;
        }
    }
    return false;
}

bool MpqManager::FileExists(const std::string& filename) const {
    std::string folderPath;
    if (FindFileInFolders(filename, folderPath)) return true;
    if (FindFileInTopArchives(filename) != nullptr) return true;
    if (FindFileInCasc(filename) != nullptr) return true;
    if (FindFileInGameArchives(filename) != nullptr) return true;
    const MpqArchive* impArchive = nullptr;
    return FindMapImport(filename, folderPath, &impArchive);
}

bool MpqManager::ReadFile(const std::string& filename, std::vector<uint8_t>& out) const {
    // Lookup order (matches header + ref CompoundDataSource newest-first):
    // folders -> map mounts -> CASC -> game MPQs -> map-import fallback.
    // CASC shadows MPQ game archives (1.30+ live data beats stale MPQs)
    // but never folders or map mounts.
    std::string folderPath;
    if (FindFileInFolders(filename, folderPath)) {
        if (ReadWholeFile(folderPath, out)) return true;
        out.clear();
    }
    if (ReadFromArchive(FindFileInTopArchives(filename), filename, out)) return true;
    // CASC storages shadow MPQ game archives but never folders or maps.
    const auto* casc = FindFileInCasc(ToArchivePath(filename));
    if (casc) return casc->ReadFile(ToArchivePath(filename), out);
    if (ReadFromArchive(FindFileInGameArchives(filename), filename, out)) return true;
    // Map import fallback (war3mapImported\<path>).
    std::string impFolder;
    const MpqArchive* impArchive = nullptr;
    if (FindMapImport(filename, impFolder, &impArchive)) {
        if (!impFolder.empty() && ReadWholeFile(impFolder, out)) return true;
        if (impArchive && ReadFromArchive(impArchive, std::string("war3mapImported\\") + filename, out)) {
            return true;
        }
    }
    return false;
}

bool MpqManager::FindMapImport(const std::string& filename,
                               std::string& folderOut,
                               const MpqArchive** archiveOut) const {
    const std::string key = ToArchivePath(filename);
    // Already prefixed: direct map-mount hit counts, no recursion.
    if (key.size() >= 17 && (key.compare(0, 17, "war3mapImported\\") == 0 ||
                             key.compare(0, 17, "war3mapImported/") == 0)) {
        if (archiveOut) *archiveOut = nullptr;
        return false;
    }
    const std::string prefixed = std::string("war3mapImported\\") + key;
    const std::string rel = ToNativePath(prefixed);
    for (auto it = folders_.rbegin(); it != folders_.rend(); ++it) {
        fs::path candidate = fs::path(*it) / rel;
        std::error_code ec;
        if (fs::is_regular_file(candidate, ec)) {
            folderOut = candidate.string();
            if (archiveOut) *archiveOut = nullptr;
            return true;
        }
    }
    for (auto it = topArchives_.rbegin(); it != topArchives_.rend(); ++it) {
        if (ArchiveHasFileImpl(it->handle, prefixed)) {
            folderOut.clear();
            if (archiveOut) *archiveOut = &(*it);
            return true;
        }
    }
    return false;
}

std::vector<std::string> MpqManager::ListFiles(const std::string& archivePath) const {
    std::vector<std::string> result;
    // Map mounts first (they shadow game archives), then game archives.
    for (const auto& a : topArchives_) {
        if (!archivePath.empty() && a.path != archivePath) continue;
        ListArchiveWithPattern(a.handle, "*", result);
    }
    for (const auto& a : archives_) {
        if (!archivePath.empty() && a.path != archivePath) continue;
        ListArchiveWithPattern(a.handle, "*", result);
    }
    return result;
}

std::vector<std::string> MpqManager::ListAllFiles(const std::string& pattern) const {
    std::vector<std::string> result;
    const std::string effectivePattern = pattern.empty() ? "*" : pattern;
    for (const auto& a : archives_) {
        ListArchiveWithPattern(a.handle, effectivePattern, result);
    }
    for (const auto& c : cascStorages_) {
        for (auto& name : c.ListFiles(effectivePattern)) {
            result.push_back(std::move(name));
        }
    }
    return result;
}

bool MpqManager::ReadSingleArchiveFile(const std::string& archivePath,
                                       const std::string& filename,
                                       std::vector<uint8_t>& out) {
    out.clear();
    if (archivePath.empty() || filename.empty()) return false;
    HANDLE hMpq = nullptr;
    if (!OpenArchiveImpl(archivePath, hMpq)) {
        return false;
    }
    MpqArchive temp{archivePath, hMpq};
    const bool ok = ReadFromArchive(&temp, filename, out);
    CloseArchiveImpl(hMpq);
    return ok;
}

bool MpqManager::ReadFromGameArchivesOnly(const std::string& filename,
                                          std::vector<uint8_t>& out) const {
    out.clear();
    const std::string key = ToArchivePath(filename);
    for (const auto& a : archives_) {
        if (!a.handle) continue;
#ifdef WITHOUT_STORMLIB
        if (static_cast<const NativeMpqArchive*>(a.handle)->ReadFile(key, out)) {
            return true;
        }
#else
        HANDLE hFile = nullptr;
        if (SFileOpenFileEx(a.handle, key.c_str(), SFILE_OPEN_FROM_MPQ, &hFile)) {
            DWORD sz = SFileGetFileSize(hFile, nullptr);
            if (sz == 0xFFFFFFFFu) {
                SFileCloseFile(hFile);
                return false;
            }
            out.resize(sz);
            DWORD read = 0;
            const bool ok = (sz == 0) ||
                            (SFileReadFile(hFile, out.data(), sz, &read, nullptr) && read == sz);
            SFileCloseFile(hFile);
            if (!ok) out.clear();
            return ok;
        }
#endif
    }
    return false;
}

bool MpqManager::ReadJassScriptFile(const std::string& mapPath,
                                    const std::string& fileName,
                                    std::vector<uint8_t>& out) const {
    out.clear();
    const size_t pos = fileName.find_last_of("\\/");
    const std::string base = (pos == std::string::npos) ? fileName : fileName.substr(pos + 1);
    std::string lowerBase = base;
    for (char& c : lowerBase) {
        if (c >= 65 && c <= 90) c = static_cast<char>(c + 32);
    }
    const bool isMapScript = (lowerBase == "war3map.j");

    if (isMapScript) {
        if (ReadSingleArchiveFile(mapPath, base, out)) return true;
        if (base != fileName && ReadSingleArchiveFile(mapPath, fileName, out)) return true;
    }
    if (ReadFromGameArchivesOnly(fileName, out)) return true;
    if (base != fileName && ReadFromGameArchivesOnly(base, out)) return true;
    if (!isMapScript) {
        if (ReadSingleArchiveFile(mapPath, fileName, out)) return true;
        if (base != fileName && ReadSingleArchiveFile(mapPath, base, out)) return true;
    }
    return false;
}

struct NativeMpqArchiveReader::Impl {
    NativeMpqArchive archive;
};

NativeMpqArchiveReader::NativeMpqArchiveReader()
    : impl_(std::make_unique<Impl>()) {}

NativeMpqArchiveReader::~NativeMpqArchiveReader() = default;

bool NativeMpqArchiveReader::Open(const std::string& filePath) {
    return impl_ && impl_->archive.Open(filePath);
}

void NativeMpqArchiveReader::Close() {
    if (impl_) {
        impl_->archive.Close();
    }
}

bool NativeMpqArchiveReader::HasFile(const std::string& filename) const {
    return impl_ && impl_->archive.HasFile(ToArchivePath(filename));
}

bool NativeMpqArchiveReader::ReadFile(const std::string& filename, std::vector<uint8_t>& out) const {
    if (!impl_) {
        out.clear();
        return false;
    }
    return impl_->archive.ReadFile(ToArchivePath(filename), out);
}

std::vector<std::string> NativeMpqArchiveReader::ListFiles(const std::string& pattern) const {
    std::vector<std::string> out;
    if (!impl_) return out;
    for (const auto& entry : impl_->archive.GetListFiles()) {
        if (MatchMpqWildcard(pattern.c_str(), entry.c_str())) {
            out.push_back(entry);
        }
    }
    return out;
}
