#pragma once
// ============================================================================
// Engine Core: Compile-Time Deterministic RTTI-Free StaticTypeId
//
// Computes a deterministic 64-bit FNV-1a hash of the canonical C++ type name
// extracted from `__PRETTY_FUNCTION__` (GCC/Clang) or `__FUNCSIG__` (MSVC) at
// compile time (`constexpr`).
//
// Why NOT static variable addresses (`&Anchor<T>::kAnchor`):
// Across Windows `.dll` or Linux `-fvisibility=hidden` `.so` plugin boundaries,
// each binary image instantiates its own static template data address, causing
// cross-boundary `SubsystemRegistry::TryGet<T>()` / `SimEcsRegistry::Pool<T>()`
// lookups to split or miss. Hashing the extracted canonical type name produces
// the exact same 64-bit integer constant in `.exe`, `.dll`, and `.so` under
// `/GR-` and `-fno-rtti`.
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace engine {

using TypeId = uint64_t;

namespace detail {

constexpr bool IsTypeIdentChar(char c) noexcept {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           c == '_';
}

constexpr uint64_t Fnva64TypeSignature(std::string_view sv) noexcept {
    constexpr std::string_view kClassPrefix = "class ";
    constexpr std::string_view kStructPrefix = "struct ";
    constexpr std::string_view kEnumPrefix = "enum ";
    constexpr std::string_view kUnionPrefix = "union ";

    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < sv.size();) {
        if (i == 0 || !IsTypeIdentChar(sv[i - 1])) {
            if (sv.size() - i >= kClassPrefix.size() &&
                sv.substr(i, kClassPrefix.size()) == kClassPrefix) {
                i += kClassPrefix.size();
                continue;
            }
            if (sv.size() - i >= kStructPrefix.size() &&
                sv.substr(i, kStructPrefix.size()) == kStructPrefix) {
                i += kStructPrefix.size();
                continue;
            }
            if (sv.size() - i >= kEnumPrefix.size() &&
                sv.substr(i, kEnumPrefix.size()) == kEnumPrefix) {
                i += kEnumPrefix.size();
                continue;
            }
            if (sv.size() - i >= kUnionPrefix.size() &&
                sv.substr(i, kUnionPrefix.size()) == kUnionPrefix) {
                i += kUnionPrefix.size();
                continue;
            }
        }
        const char c = sv[i++];
        // Skip whitespace differences between MSVC ("struct Foo") and GCC/Clang ("Foo")
        if (c == ' ' || c == '\t') {
            continue;
        }
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        hash *= 1099511628211ULL;
    }
    return hash == 0 ? 1ULL : hash;
}

constexpr std::string_view StripElaboratedTypePrefix(std::string_view name) noexcept {
    while (!name.empty() && (name.front() == ' ' || name.front() == '\t')) {
        name.remove_prefix(1);
    }
    constexpr std::string_view kClassPrefix = "class ";
    constexpr std::string_view kStructPrefix = "struct ";
    constexpr std::string_view kEnumPrefix = "enum ";
    constexpr std::string_view kUnionPrefix = "union ";
    if (name.size() >= kClassPrefix.size() &&
        name.substr(0, kClassPrefix.size()) == kClassPrefix) {
        name.remove_prefix(kClassPrefix.size());
    } else if (name.size() >= kStructPrefix.size() &&
               name.substr(0, kStructPrefix.size()) == kStructPrefix) {
        name.remove_prefix(kStructPrefix.size());
    } else if (name.size() >= kEnumPrefix.size() &&
               name.substr(0, kEnumPrefix.size()) == kEnumPrefix) {
        name.remove_prefix(kEnumPrefix.size());
    } else if (name.size() >= kUnionPrefix.size() &&
               name.substr(0, kUnionPrefix.size()) == kUnionPrefix) {
        name.remove_prefix(kUnionPrefix.size());
    }
    while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) {
        name.remove_suffix(1);
    }
    return name;
}

template <typename T>
constexpr std::string_view RawCanonicalTypeName() noexcept {
#if defined(__clang__)
    // "std::string_view engine::detail::RawCanonicalTypeName() [T = MyNamespace::MyType]"
    std::string_view sig = __PRETTY_FUNCTION__;
    const size_t start = sig.find("[T = ");
    const size_t end = sig.rfind(']');
    if (start != std::string_view::npos && end != std::string_view::npos && end > start + 5) {
        return StripElaboratedTypePrefix(sig.substr(start + 5, end - (start + 5)));
    }
    return sig;
#elif defined(__GNUC__)
    // "constexpr std::string_view engine::detail::RawCanonicalTypeName() [with T = MyNamespace::MyType; ...]"
    std::string_view sig = __PRETTY_FUNCTION__;
    const size_t start = sig.find("with T = ");
    if (start != std::string_view::npos) {
        const size_t body = start + 9;
        size_t end = sig.find(';', body);
        if (end == std::string_view::npos) {
            end = sig.rfind(']');
        }
        if (end != std::string_view::npos && end > body) {
            return StripElaboratedTypePrefix(sig.substr(body, end - body));
        }
    }
    return sig;
#elif defined(_MSC_VER)
    // "class std::basic_string_view<char,...> __cdecl engine::detail::RawCanonicalTypeName<struct MyNamespace::MyType>(void) noexcept"
    std::string_view sig = __FUNCSIG__;
    const size_t fnPos = sig.find("RawCanonicalTypeName<");
    const size_t end = sig.rfind(">(void)");
    if (fnPos != std::string_view::npos && end != std::string_view::npos && end > fnPos + 21) {
        return StripElaboratedTypePrefix(sig.substr(fnPos + 21, end - (fnPos + 21)));
    }
    return sig;
#else
    #error "Unsupported compiler for RTTI-free StaticTypeId"
#endif
}

} // namespace detail

template <typename T>
constexpr std::string_view StaticTypeName() noexcept {
    using CleanT = std::remove_cv_t<std::remove_reference_t<T>>;
    return detail::RawCanonicalTypeName<CleanT>();
}

template <typename T>
constexpr TypeId StaticTypeId() noexcept {
    using CleanT = std::remove_cv_t<std::remove_reference_t<T>>;
    return detail::Fnva64TypeSignature(detail::RawCanonicalTypeName<CleanT>());
}

} // namespace engine
