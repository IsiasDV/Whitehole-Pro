#pragma once

#include "whitehole/io/binary_file.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace whitehole::smg {

enum class BcsvType : std::uint8_t {
    integer = 0,
    fixedString = 1,
    floatingPoint = 2,
    integer2 = 3,
    shortInteger = 4,
    byte = 5,
    stringOffset = 6,
};

using BcsvValue = std::variant<std::int32_t, std::string, float, std::int16_t, std::int8_t>;

struct BcsvField {
    std::uint32_t hash{0};
    std::uint32_t mask{0xFFFFFFFFU};
    std::uint16_t offset{0};
    std::uint8_t shift{0};
    BcsvType type{BcsvType::integer};
};

struct BcsvRow {
    std::vector<BcsvValue> values;
};

class BcsvTable {
public:
    BcsvTable() = default;
    BcsvTable(std::vector<std::uint8_t> data, io::Endian endian);

    [[nodiscard]] static BcsvTable open(const std::filesystem::path& path, io::Endian endian);

    [[nodiscard]] io::Endian endian() const noexcept { return endian_; }
    [[nodiscard]] std::uint32_t entrySize() const noexcept { return entrySize_; }
    [[nodiscard]] const std::vector<BcsvField>& fields() const noexcept { return fields_; }
    [[nodiscard]] std::vector<BcsvField>& fields() noexcept {
        // Field offsets, masks and types may change, so drop the cached hash index.
        fieldLookup_.clear();
        return fields_;
    }
    [[nodiscard]] const std::vector<BcsvRow>& rows() const noexcept { return rows_; }
    [[nodiscard]] std::vector<BcsvRow>& rows() noexcept { return rows_; }

    [[nodiscard]] std::optional<std::size_t> fieldIndex(std::string_view name) const;
    [[nodiscard]] std::optional<std::size_t> fieldIndex(std::uint32_t hash) const;
    [[nodiscard]] std::string getString(const BcsvRow& row, std::string_view name,
                                        std::string fallback = {}) const;
        [[nodiscard]] float getFloat(const BcsvRow& row, std::string_view name, float fallback = 0.0F) const;
    [[nodiscard]] std::int32_t getInt(const BcsvRow& row, std::string_view name,
                                      std::int32_t fallback = 0) const;
    [[nodiscard]] std::string getStringById(const BcsvRow& row, std::uint32_t hash,
                                            std::string fallback = {}) const;
    [[nodiscard]] float getFloatById(const BcsvRow& row, std::uint32_t hash, float fallback = 0.0F) const;
    [[nodiscard]] std::int32_t getIntById(const BcsvRow& row, std::uint32_t hash,
                                          std::int32_t fallback = 0) const;
    void setString(BcsvRow& row, std::string_view name, std::string value);
    void setFloat(BcsvRow& row, std::string_view name, float value);

    [[nodiscard]] std::vector<std::uint8_t> serialize() const;

private:
    void parse(const std::vector<std::uint8_t>& data);

    io::Endian endian_{io::Endian::big};
    std::uint32_t entrySize_{0};
    std::vector<BcsvField> fields_;
    std::vector<BcsvRow> rows_;
    std::unordered_map<std::uint32_t, std::size_t> fieldLookup_;
};

[[nodiscard]] std::string toString(const BcsvValue& value);

} // namespace whitehole::smg
