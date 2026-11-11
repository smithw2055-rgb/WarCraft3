#pragma once
// ============================================================================
// Engine Resource: TableSchemaBinder — Zero-Allocation Declarative SLK/CSV/INI
//                  Schema Reflection & Row Hydration
//
// Improvements over v1:
// 1. `ParseIntCell` / `ParseFloatCell` / `ParseBoolCell` operate directly on
//    `std::string_view` using `<charconv>` / stack buffers (zero per-cell
//    `std::string` heap allocations!).
// 2. `ParseBoolCell` treats ANY non-zero integer ("1", "2", "-1", etc.) as
//    `true`, matching Warcraft III `GetMergedInt(...) != 0` SLK/INI semantics!
// ============================================================================
#include <cctype>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace engine::resource {

namespace detail {

inline std::string_view TrimCellView(std::string_view sv) noexcept {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) {
        sv.remove_suffix(1);
    }
    if (sv.size() >= 2 && sv.front() == '"' && sv.back() == '"') {
        sv.remove_prefix(1);
        sv.remove_suffix(1);
    }
    return sv;
}

inline bool EqualsIgnoreCaseAscii(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

inline bool IsNullSentinelCell(std::string_view sv) noexcept {
    return sv.empty() || sv == "-" || sv == "_";
}

inline int ParseIntCell(std::string_view raw, int defaultVal = 0) noexcept {
    const std::string_view sv = TrimCellView(raw);
    if (IsNullSentinelCell(sv)) return defaultVal;
    int value = defaultVal;
    const auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
    if (ec == std::errc{}) {
        return value;
    }
    return defaultVal;
}

inline float ParseFloatCell(std::string_view raw, float defaultVal = 0.0f) noexcept {
    const std::string_view sv = TrimCellView(raw);
    if (IsNullSentinelCell(sv)) return defaultVal;
    char stackBuf[64];
    const size_t n = (sv.size() < sizeof(stackBuf) - 1) ? sv.size() : (sizeof(stackBuf) - 1);
    std::memcpy(stackBuf, sv.data(), n);
    stackBuf[n] = '\0';
    char* endPtr = nullptr;
    const float v = std::strtof(stackBuf, &endPtr);
    return (endPtr && endPtr != stackBuf) ? v : defaultVal;
}

inline bool ParseBoolCell(std::string_view raw, bool defaultVal = false) noexcept {
    const std::string_view sv = TrimCellView(raw);
    if (IsNullSentinelCell(sv)) return defaultVal;

    // Any valid integer: non-zero -> true, zero -> false (matches GetMergedInt != 0!)
    int intVal = 0;
    const auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), intVal);
    if (ec == std::errc{} && ptr == sv.data() + sv.size()) {
        return intVal != 0;
    }

    if (EqualsIgnoreCaseAscii(sv, "true") ||
        EqualsIgnoreCaseAscii(sv, "yes") ||
        EqualsIgnoreCaseAscii(sv, "on")) {
        return true;
    }
    if (EqualsIgnoreCaseAscii(sv, "false") ||
        EqualsIgnoreCaseAscii(sv, "no") ||
        EqualsIgnoreCaseAscii(sv, "off")) {
        return false;
    }
    return defaultVal;
}

inline void SplitCsvView(std::string_view raw, std::vector<std::string>& outItems) {
    outItems.clear();
    const std::string_view sv = TrimCellView(raw);
    if (IsNullSentinelCell(sv)) return;
    size_t start = 0;
    while (start <= sv.size()) {
        const size_t comma = sv.find(',', start);
        const std::string_view token = (comma == std::string_view::npos)
            ? sv.substr(start)
            : sv.substr(start, comma - start);
        const std::string_view clean = TrimCellView(token);
        if (!IsNullSentinelCell(clean)) {
            outItems.emplace_back(clean);
        }
        if (comma == std::string_view::npos) break;
        start = comma + 1;
    }
}

} // namespace detail

// CellLookupFn takes a column/key name and returns a borrowed string_view (empty if absent)
using CellViewLookupFn = std::function<std::string_view(std::string_view columnName)>;

template <typename RowStruct>
class TableSchemaBinder {
public:
    using FieldHydratorFn = std::function<void(RowStruct& row, const CellViewLookupFn& lookup)>;

    TableSchemaBinder& BindString(std::string column,
                                  std::string RowStruct::* member,
                                  std::string defaultVal = "",
                                  std::string fallbackColumn = "") {
        fields_.push_back([col = std::move(column),
                           fb = std::move(fallbackColumn),
                           member,
                           def = std::move(defaultVal)](RowStruct& r, const CellViewLookupFn& get) {
            std::string_view sv = detail::TrimCellView(get(col));
            if (detail::IsNullSentinelCell(sv) && !fb.empty()) {
                sv = detail::TrimCellView(get(fb));
            }
            r.*member = detail::IsNullSentinelCell(sv) ? def : std::string(sv);
        });
        return *this;
    }

    TableSchemaBinder& BindInt(std::string column,
                               int RowStruct::* member,
                               int defaultVal = 0,
                               std::string fallbackColumn = "") {
        fields_.push_back([col = std::move(column),
                           fb = std::move(fallbackColumn),
                           member,
                           defaultVal](RowStruct& r, const CellViewLookupFn& get) {
            std::string_view sv = detail::TrimCellView(get(col));
            if (detail::IsNullSentinelCell(sv) && !fb.empty()) {
                sv = detail::TrimCellView(get(fb));
            }
            r.*member = detail::ParseIntCell(sv, defaultVal);
        });
        return *this;
    }

    TableSchemaBinder& BindFloat(std::string column,
                                 float RowStruct::* member,
                                 float defaultVal = 0.0f,
                                 std::string fallbackColumn = "") {
        fields_.push_back([col = std::move(column),
                           fb = std::move(fallbackColumn),
                           member,
                           defaultVal](RowStruct& r, const CellViewLookupFn& get) {
            std::string_view sv = detail::TrimCellView(get(col));
            if (detail::IsNullSentinelCell(sv) && !fb.empty()) {
                sv = detail::TrimCellView(get(fb));
            }
            r.*member = detail::ParseFloatCell(sv, defaultVal);
        });
        return *this;
    }

    TableSchemaBinder& BindBool(std::string column,
                                bool RowStruct::* member,
                                bool defaultVal = false,
                                std::string fallbackColumn = "") {
        fields_.push_back([col = std::move(column),
                           fb = std::move(fallbackColumn),
                           member,
                           defaultVal](RowStruct& r, const CellViewLookupFn& get) {
            std::string_view sv = detail::TrimCellView(get(col));
            if (detail::IsNullSentinelCell(sv) && !fb.empty()) {
                sv = detail::TrimCellView(get(fb));
            }
            r.*member = detail::ParseBoolCell(sv, defaultVal);
        });
        return *this;
    }

    TableSchemaBinder& BindStringList(std::string column,
                                      std::vector<std::string> RowStruct::* member,
                                      std::string fallbackColumn = "") {
        fields_.push_back([col = std::move(column),
                           fb = std::move(fallbackColumn),
                           member](RowStruct& r, const CellViewLookupFn& get) {
            std::string_view sv = detail::TrimCellView(get(col));
            if (detail::IsNullSentinelCell(sv) && !fb.empty()) {
                sv = detail::TrimCellView(get(fb));
            }
            detail::SplitCsvView(sv, r.*member);
        });
        return *this;
    }

    void HydrateRow(RowStruct& outRow, const CellViewLookupFn& lookup) const {
        for (const auto& fn : fields_) {
            fn(outRow, lookup);
        }
    }

    size_t BoundFieldCount() const { return fields_.size(); }

private:
    std::vector<FieldHydratorFn> fields_;
};

} // namespace engine::resource
