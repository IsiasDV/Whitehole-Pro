#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace whitehole::io::yaz0 {

[[nodiscard]] bool isCompressed(std::span<const std::uint8_t> data) noexcept;
[[nodiscard]] bool isCompressed(const std::vector<std::uint8_t>& data) noexcept;

[[nodiscard]] std::vector<std::uint8_t> decompress(const std::vector<std::uint8_t>& data);

void decompressTo(const std::vector<std::uint8_t>& data, std::vector<std::uint8_t>& output);

[[nodiscard]] std::vector<std::uint8_t> compress(const std::vector<std::uint8_t>& data, unsigned level = 3);
[[nodiscard]] std::vector<std::uint8_t> compress(std::span<const std::uint8_t> data, unsigned level = 3);

[[nodiscard]] std::size_t estimateCompressedSize(const std::vector<std::uint8_t>& data, unsigned level = 3) noexcept;

} // namespace whitehole::io::yaz0
