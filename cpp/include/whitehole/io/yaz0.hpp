#pragma once

#include <cstdint>
#include <vector>

namespace whitehole::io::yaz0 {

[[nodiscard]] bool isCompressed(const std::vector<std::uint8_t>& data) noexcept;
[[nodiscard]] std::vector<std::uint8_t> decompress(const std::vector<std::uint8_t>& data);
[[nodiscard]] std::vector<std::uint8_t> compress(const std::vector<std::uint8_t>& data);

} // namespace whitehole::io::yaz0
