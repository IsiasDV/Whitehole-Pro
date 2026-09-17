#pragma once

#include "whitehole/io/binary_file.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
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
    [[nodiscard]] std::vector<BcsvField>& fields() noexcept { return fields_; }
    [[nodiscard]] const std::vector<BcsvRow>& rows() const noexcept { return rows_; }
    [[nodiscard]] std::vector<BcsvRow>& rows() noexcept { return rows_; }

    [[nodiscard]] std::vector<std::uint8_t> serialize() const;

private:
    void parse(const std::vector<std::uint8_t>& data);

    io::Endian endian_{io::Endian::big};
    std::uint32_t entrySize_{0};
    std::vector<BcsvField> fields_;
    std::vector<BcsvRow> rows_;
};

[[nodiscard]] std::string toString(const BcsvValue& value);

} // namespace whitehole::smg
