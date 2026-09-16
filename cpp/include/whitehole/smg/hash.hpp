#pragma once

#include <cstdint>
#include <string_view>

namespace whitehole::smg {

[[nodiscard]] std::uint32_t jmapHash(std::string_view value) noexcept;
[[nodiscard]] std::uint32_t superFastHash(std::string_view value, std::uint32_t seed = 0) noexcept;

} // namespace whitehole::smg
