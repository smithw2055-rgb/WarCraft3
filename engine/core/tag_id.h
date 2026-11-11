#pragma once
// ============================================================================
// Engine Core: TagId — Compile-Time FourCC & FNV-1a 32-bit Identifier
//
// Provides a zero-overhead, game-agnostic 32-bit identifier type:
// - 4-character tags ("hfoo", "Move", "ESAV") pack into big-endian FourCC
//   (100% binary-compatible with legacy WarCraft III rawcodes).
// - Longer symbolic tags ("minimal_rts.harvester") hash deterministically via
//   32-bit FNV-1a at compile time (`constexpr`).
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace engine {

class TagId {
public:
    constexpr TagId() noexcept : value_(0) {}
    constexpr explicit TagId(uint32_t raw) noexcept : value_(raw) {}

    constexpr explicit TagId(std::string_view text) noexcept
        : value_(FromStringView(text)) {}

    constexpr TagId(const char* cstr) noexcept
        : value_(cstr ? FromStringView(std::string_view(cstr)) : 0u) {}

    static constexpr TagId FromFourCC(char a, char b, char c, char d) noexcept {
        return TagId(
            (static_cast<uint32_t>(static_cast<unsigned char>(a)) << 24) |
            (static_cast<uint32_t>(static_cast<unsigned char>(b)) << 16) |
            (static_cast<uint32_t>(static_cast<unsigned char>(c)) << 8) |
            static_cast<uint32_t>(static_cast<unsigned char>(d)));
    }

    static constexpr TagId FromHash(std::string_view sv) noexcept {
        if (sv.empty()) return TagId(0u);
        uint32_t h = 2166136261u;
        for (char c : sv) {
            h ^= static_cast<uint32_t>(static_cast<unsigned char>(c));
            h *= 16777619u;
        }
        return TagId(h == 0u ? 1u : h);
    }

    static constexpr uint32_t FromStringView(std::string_view sv) noexcept {
        if (sv.empty()) return 0u;
        if (sv.size() == 4u) {
            return FromFourCC(sv[0], sv[1], sv[2], sv[3]).value_;
        }
        return FromHash(sv).value_;
    }

    constexpr uint32_t Value() const noexcept { return value_; }
    constexpr uint32_t Raw() const noexcept { return value_; }
    constexpr bool IsValid() const noexcept { return value_ != 0u; }
    constexpr explicit operator bool() const noexcept { return value_ != 0u; }
    constexpr operator uint32_t() const noexcept { return value_; }

    std::string ToString() const {
        if (value_ == 0u) return "";
        char chars[4] = {
            static_cast<char>((value_ >> 24) & 0xFFu),
            static_cast<char>((value_ >> 16) & 0xFFu),
            static_cast<char>((value_ >> 8) & 0xFFu),
            static_cast<char>(value_ & 0xFFu)
        };
        bool allPrintable = true;
        for (char c : chars) {
            if (static_cast<unsigned char>(c) < 32u || static_cast<unsigned char>(c) > 126u) {
                allPrintable = false;
                break;
            }
        }
        if (allPrintable) {
            return std::string(chars, 4);
        }
        char hexBuf[11] = {};
        static constexpr char kHex[] = "0123456789ABCDEF";
        hexBuf[0] = '0';
        hexBuf[1] = 'x';
        for (int i = 0; i < 8; ++i) {
            hexBuf[2 + i] = kHex[(value_ >> ((7 - i) * 4)) & 0xFu];
        }
        return std::string(hexBuf, 10);
    }

    constexpr bool operator==(const TagId& other) const noexcept { return value_ == other.value_; }
    constexpr bool operator!=(const TagId& other) const noexcept { return value_ != other.value_; }
    constexpr bool operator<(const TagId& other) const noexcept { return value_ < other.value_; }

private:
    uint32_t value_ = 0;
};

struct TagIdHasher {
    size_t operator()(const TagId& t) const noexcept {
        return std::hash<uint32_t>{}(t.Value());
    }
};

namespace literals {
constexpr TagId operator""_tag(const char* str, size_t len) noexcept {
    return TagId(std::string_view(str, len));
}
} // namespace literals

} // namespace engine

namespace std {
template <>
struct hash<engine::TagId> {
    size_t operator()(const engine::TagId& t) const noexcept {
        return std::hash<uint32_t>{}(t.Value());
    }
};
} // namespace std
