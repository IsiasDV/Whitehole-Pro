#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <span>
#include <vector>

namespace whitehole::io {

enum class Endian { little, big };

std::vector<std::uint8_t> readFile(const std::filesystem::path& path);
void writeFile(const std::filesystem::path& path, std::span<const std::uint8_t> data);
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
    [[nodiscard]] std::span<const std::uint8_t> peekBytes(std::size_t count);

    [[nodiscard]] std::string readString(std::size_t maxLength = 0);

    template<std::size_t N>
    [[nodiscard]] std::array<std::uint8_t, N> readLittleBytes() {
        ensure(N);
        std::array<std::uint8_t, N> out{};
        std::memcpy(out.data(), data_.data() + position_, N);
        position_ += N;
        return out;
    }

    [[nodiscard]] bool trySkip(std::size_t amount) noexcept {
        if (amount > remaining()) {
            return false;
        }
        position_ += amount;
        return true;
    }

private:
    void ensure(std::size_t count) const;
    [[nodiscard]] std::uint16_t read16Raw(std::size_t base) const noexcept;
    [[nodiscard]] std::uint32_t read32Raw(std::size_t base) const noexcept;

    const std::vector<std::uint8_t>& data_;
    Endian endian_{Endian::little};
    std::size_t position_{0};
};

class BinaryWriter {
public:
    explicit BinaryWriter(Endian endian = Endian::little) : endian_(endian) {}

    void setEndian(Endian endian) noexcept { endian_ = endian; }
    [[nodiscard]] std::size_t position() const noexcept { return position_; }
    [[nodiscard]] std::size_t capacity() const noexcept { return data_.capacity(); }
    [[nodiscard]] const std::vector<std::uint8_t>& data() const noexcept { return data_; }
    std::vector<std::uint8_t>&& take() && { return std::move(data_); }

    void seek(std::size_t position);
    void writeU8(std::uint8_t value);
    void writeU16(std::uint16_t value);
    void writeU32(std::uint32_t value);
    void writeF32(float value);

    void writeBytes(std::span<const std::uint8_t> value) noexcept;
    void writeBytes(const std::vector<std::uint8_t>& value) noexcept;

    void writeString(std::string_view value, bool nullTerminate = true);

    void writeSpanRepeated(std::size_t count, std::uint8_t value) noexcept;
    void ensure(std::size_t count);
    void patchU16(std::size_t offset, std::uint16_t value) const;
    void patchU32(std::size_t offset, std::uint32_t value) const;
    void align32();

    std::vector<std::uint8_t>&& finalize() && { return std::move(data_); }

private:
    void reserveExact();
    void growTo(std::size_t required);

    std::vector<std::uint8_t> data_;
    Endian endian_{Endian::little};
    std::size_t position_{0};
};

} // namespace whitehole::io
