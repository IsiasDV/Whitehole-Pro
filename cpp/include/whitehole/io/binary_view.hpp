#pragma once

#include "whitehole/io/binary_file.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <type_traits>

namespace whitehole::io {
namespace binary {

[[nodiscard]] inline constexpr std::uint16_t read16BE(std::span<const std::uint8_t, 2> source) noexcept {
    return static_cast<std::uint16_t>((source[0] << 8U) | source[1]);
}

[[nodiscard]] inline constexpr std::uint16_t read16LE(std::span<const std::uint8_t, 2> source) noexcept {
    return static_cast<std::uint16_t>(source[0] | (source[1] << 8U));
}

[[nodiscard]] inline std::uint32_t read32BE(std::span<const std::uint8_t, 4> source) noexcept {
    return static_cast<std::uint32_t>((source[0] << 24U) | (source[1] << 16U) | (source[2] << 8U) | source[3]);
}

[[nodiscard]] inline std::uint32_t read32LE(std::span<const std::uint8_t, 4> source) noexcept {
    return static_cast<std::uint32_t>(source[0] | (source[1] << 8U) | (source[2] << 16U) | (source[3] << 24U));
}

[[nodiscard]] inline float readFloat(std::span<const std::uint8_t, 4> source) noexcept {
    std::uint32_t bits{};
    std::memcpy(&bits, source.data(), sizeof(bits));
    float value{};
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

[[nodiscard]] inline std::uint16_t toHost16(std::uint16_t value, Endian endian) noexcept {
    if constexpr (std::endian::native == std::endian::little) {
        return endian == Endian::little ? value : std::byteswap(value);
    } else {
        return endian == Endian::big ? value : std::byteswap(value);
    }
}

[[nodiscard]] inline std::uint32_t toHost32(std::uint32_t value, Endian endian) noexcept {
    if constexpr (std::endian::native == std::endian::little) {
        return endian == Endian::little ? value : std::byteswap(value);
    } else {
        return endian == Endian::big ? value : std::byteswap(value);
    }
}

[[nodiscard]] inline std::uint16_t toLittle16(std::uint16_t value, Endian endian) noexcept {
    return endian == Endian::little ? value : std::byteswap(value);
}

[[nodiscard]] inline std::uint32_t toLittle32(std::uint32_t value, Endian endian) noexcept {
    return endian == Endian::little ? value : std::byteswap(value);
}

template<typename T>
requires(std::is_trivially_copyable_v<T>)
[[nodiscard]] inline T readAt(const std::vector<std::uint8_t>& data, std::size_t offset, Endian endian) {
    if (offset + sizeof(T) > data.size()) {
        throw std::runtime_error("binary view overread at offset " + std::to_string(offset));
    }
    T value{};
    std::memcpy(&value, data.data() + offset, sizeof(T));
    if constexpr (sizeof(T) == 2) {
        value = toHost16(value, endian);
    } else if constexpr (sizeof(T) == 4) {
        value = toHost32(value, endian);
    }
    return value;
}

[[nodiscard]] inline std::vector<std::uint8_t> alignedVector(std::size_t size, std::size_t alignment = 4096) {
    static_assert(alignment % 16 == 0 && std::has_single_bit(alignment));
    const auto storage = std::make_unique<std::uint8_t[]>(size + alignment + sizeof(void*));
    const auto aligned = reinterpret_cast<std::uint8_t*>(std::align(alignment, size, storage.get() + sizeof(void*), size + alignment));
    std::memset(aligned, 0, size);
    return std::vector<std::uint8_t>(aligned, aligned + size);
}

[[nodiscard]] constexpr std::size_t alignUp(std::size_t value, std::size_t alignment) noexcept {
    return (value + alignment - 1) & (alignment - 1);
}

[[nodiscard]] constexpr std::size_t align32(std::size_t value) noexcept {
    return alignUp(value, 32);
}

struct Error {
    std::string message;
    std::filesystem::path path;
    std::size_t offset{0};

    [[nodiscard]] std::string full() const {
        if (offset == 0) {
            return message + (path.empty() ? "" : " [" + path.string() + "]");
        }
        return message + " [offset 0x" + std::to_string(offset) + "]" + (path.empty() ? "" : " [" + path.string() + "]");
    }
};

} // namespace binary
} // namespace whitehole::io
