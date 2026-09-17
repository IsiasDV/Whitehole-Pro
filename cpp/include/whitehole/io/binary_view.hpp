#pragma once

#include "whitehole/io/binary_file.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace whitehole::io {
namespace binary {

[[nodiscard]] inline constexpr std::uint16_t read16BE(std::span<const std::uint8_t, 2> source) noexcept {
    const std::uint16_t lo = static_cast<std::uint16_t>(source[1]);
    const std::uint16_t hi = static_cast<std::uint16_t>(source[0]);
    return static_cast<std::uint16_t>((hi << 8U) | lo);
}

[[nodiscard]] inline constexpr std::uint16_t read16LE(std::span<const std::uint8_t, 2> source) noexcept {
    const std::uint16_t lo = static_cast<std::uint16_t>(source[0]);
    const std::uint16_t hi = static_cast<std::uint16_t>(source[1]);
    return static_cast<std::uint16_t>(lo | (hi << 8U));
}

[[nodiscard]] inline std::uint32_t read32BE(std::span<const std::uint8_t, 4> source) noexcept {
    const std::uint32_t b0 = static_cast<std::uint32_t>(source[0]);
    const std::uint32_t b1 = static_cast<std::uint32_t>(source[1]);
    const std::uint32_t b2 = static_cast<std::uint32_t>(source[2]);
    const std::uint32_t b3 = static_cast<std::uint32_t>(source[3]);
    return (b0 << 24U) | (b1 << 16U) | (b2 << 8U) | b3;
}

[[nodiscard]] inline std::uint32_t read32LE(std::span<const std::uint8_t, 4> source) noexcept {
    const std::uint32_t b0 = static_cast<std::uint32_t>(source[0]);
    const std::uint32_t b1 = static_cast<std::uint32_t>(source[1]);
    const std::uint32_t b2 = static_cast<std::uint32_t>(source[2]);
    const std::uint32_t b3 = static_cast<std::uint32_t>(source[3]);
    return b0 | (b1 << 8U) | (b2 << 16U) | (b3 << 24U);
}

[[nodiscard]] inline std::uint32_t read32BE(std::uint8_t const* data) noexcept {
    const std::uint32_t b0 = static_cast<std::uint32_t>(data[0]);
    const std::uint32_t b1 = static_cast<std::uint32_t>(data[1]);
    const std::uint32_t b2 = static_cast<std::uint32_t>(data[2]);
    const std::uint32_t b3 = static_cast<std::uint32_t>(data[3]);
    return (b0 << 24U) | (b1 << 16U) | (b2 << 8U) | b3;
}

[[nodiscard]] inline float readFloat(std::uint8_t const* data) noexcept {
    const std::uint32_t b0 = static_cast<std::uint32_t>(data[0]);
    const std::uint32_t b1 = static_cast<std::uint32_t>(data[1]);
    const std::uint32_t b2 = static_cast<std::uint32_t>(data[2]);
    const std::uint32_t b3 = static_cast<std::uint32_t>(data[3]);
    std::uint32_t bits = b0 | (b1 << 8U) | (b2 << 16U) | (b3 << 24U);
    float value{};
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

[[nodiscard]] constexpr std::uint16_t bswap16(std::uint16_t value) noexcept {
    return static_cast<std::uint16_t>((value >> 8U) | (value << 8U));
}

[[nodiscard]] constexpr std::uint32_t bswap32(std::uint32_t value) noexcept {
    return ((value & 0x000000FFU) << 24U) | ((value & 0x0000FF00U) << 8U)
         | ((value & 0x00FF0000U) >> 8U) | ((value & 0xFF000000U) >> 24U);
}

#if defined(_WIN32) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
constexpr bool kHostLittleEndian = true;
#else
constexpr bool kHostLittleEndian = false;
#endif

[[nodiscard]] inline std::uint16_t toHost16(std::uint16_t value, Endian endian) noexcept {
    if constexpr (kHostLittleEndian) {
        return endian == Endian::little ? value : bswap16(value);
    } else {
        return endian == Endian::big ? value : bswap16(value);
    }
}

[[nodiscard]] inline std::uint32_t toHost32(std::uint32_t value, Endian endian) noexcept {
    if constexpr (kHostLittleEndian) {
        return endian == Endian::little ? value : bswap32(value);
    } else {
        return endian == Endian::big ? value : bswap32(value);
    }
}

[[nodiscard]] inline std::uint16_t toLittle16(std::uint16_t value, Endian endian) noexcept {
    return endian == Endian::little ? value : bswap16(value);
}

[[nodiscard]] inline std::uint32_t toLittle32(std::uint32_t value, Endian endian) noexcept {
    return endian == Endian::little ? value : bswap32(value);
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

namespace detail {

[[nodiscard]] inline std::string hexOffset(std::size_t value) {
    std::array<char, 24> buffer{};
    const auto converted = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, 16);
    return "0x" + std::string(buffer.data(), converted.ptr);
}

[[nodiscard]] inline std::string formatError(std::string_view message, const std::filesystem::path& path,
                                             std::size_t offset) {
    std::string detail(message);
    if (offset != 0) {
        detail += " [offset " + hexOffset(offset) + "]";
    }
    if (!path.empty()) {
        detail += " [" + path.string() + "]";
    }
    return detail;
}

} // namespace detail

[[nodiscard]] constexpr std::size_t alignUp(std::size_t value, std::size_t alignment) noexcept {
    return (value + alignment - 1) & (alignment - 1);
}

[[nodiscard]] constexpr std::size_t align32(std::size_t value) noexcept {
    return alignUp(value, 32);
}

struct Error : std::runtime_error {
    std::string message;
    std::filesystem::path path;
    std::size_t offset{0};

    Error(std::string message_, std::filesystem::path path_, std::size_t offset_ = 0)
        : std::runtime_error(detail::formatError(message_, path_, offset_)),
          message(std::move(message_)), path(std::move(path_)), offset(offset_) {}

    [[nodiscard]] std::string full() const { return detail::formatError(message, path, offset); }
};

} // namespace binary
} // namespace whitehole::io
