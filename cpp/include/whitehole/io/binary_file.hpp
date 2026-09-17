#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace whitehole::io {

enum class Endian { little, big };

std::vector<std::uint8_t> readFile(const std::filesystem::path& path);
void writeFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& data);

class BinaryReader {
public:
    explicit BinaryReader(const std::vector<std::uint8_t>& data, Endian endian = Endian::little);

    void setEndian(Endian endian) noexcept { endian_ = endian; }
    [[nodiscard]] Endian endian() const noexcept { return endian_; }
    [[nodiscard]] std::size_t size() const noexcept { return data_.size(); }
    [[nodiscard]] std::size_t position() const noexcept { return position_; }
    [[nodiscard]] std::size_t remaining() const noexcept { return data_.size() - position_; }

    void seek(std::size_t position);
    void skip(std::ptrdiff_t amount);

    [[nodiscard]] std::uint8_t readU8();
    [[nodiscard]] std::uint16_t readU16();
    [[nodiscard]] std::uint32_t readU32();
    [[nodiscard]] float readF32();
    [[nodiscard]] std::vector<std::uint8_t> readBytes(std::size_t count);
    [[nodiscard]] std::string readString(std::size_t maxLength = 0);

private:
    void require(std::size_t count) const;

    const std::vector<std::uint8_t>& data_;
    Endian endian_;
    std::size_t position_{0};
};

class BinaryWriter {
public:
    explicit BinaryWriter(Endian endian = Endian::little) : endian_(endian) {}

    void setEndian(Endian endian) noexcept { endian_ = endian; }
    [[nodiscard]] std::size_t position() const noexcept { return position_; }
    [[nodiscard]] const std::vector<std::uint8_t>& data() const noexcept { return data_; }
    [[nodiscard]] std::vector<std::uint8_t> take() && { return std::move(data_); }

    void seek(std::size_t position);
    void writeU8(std::uint8_t value);
    void writeU16(std::uint16_t value);
    void writeU32(std::uint32_t value);
    void writeF32(float value);
    void writeBytes(const std::vector<std::uint8_t>& value);
    void writeString(std::string_view value, bool nullTerminate = true);

private:
    void reserve(std::size_t count);

    std::vector<std::uint8_t> data_;
    Endian endian_;
    std::size_t position_{0};
};

} // namespace whitehole::io
