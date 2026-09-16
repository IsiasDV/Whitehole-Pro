#include "whitehole/smg/hash.hpp"

#include <cstddef>

namespace whitehole::smg {

std::uint32_t jmapHash(std::string_view value) noexcept {
    std::uint32_t hash = 0;
    for (const auto character : value) {
        hash = hash * 31U + static_cast<unsigned char>(character);
    }
    return hash;
}

std::uint32_t superFastHash(std::string_view value, std::uint32_t seed) noexcept {
    auto hash = seed;
    std::size_t position = 0;
    auto count = value.size() / 4;
    while (count-- > 0) {
        const auto low = static_cast<std::uint32_t>(static_cast<unsigned char>(value[position]))
            | (static_cast<std::uint32_t>(static_cast<unsigned char>(value[position + 1])) << 8U);
        const auto high = static_cast<std::uint32_t>(static_cast<unsigned char>(value[position + 2]))
            | (static_cast<std::uint32_t>(static_cast<unsigned char>(value[position + 3])) << 8U);
        position += 4;
        hash += low;
        const auto temporary = (high << 11U) ^ hash;
        hash = (hash << 16U) ^ temporary;
        hash += hash >> 11U;
    }

    switch (value.size() & 3U) {
    case 3:
        hash += static_cast<std::uint32_t>(static_cast<unsigned char>(value[position]))
            | (static_cast<std::uint32_t>(static_cast<unsigned char>(value[position + 1])) << 8U);
        hash ^= hash << 16U;
        hash ^= static_cast<std::uint32_t>(static_cast<unsigned char>(value[position + 2])) << 18U;
        hash += hash >> 11U;
        break;
    case 2:
        hash += static_cast<std::uint32_t>(static_cast<unsigned char>(value[position]))
            | (static_cast<std::uint32_t>(static_cast<unsigned char>(value[position + 1])) << 8U);
        hash ^= hash << 11U;
        hash += hash >> 17U;
        break;
    case 1:
        hash += static_cast<unsigned char>(value[position]);
        hash ^= hash << 10U;
        hash += hash >> 1U;
        break;
    default:
        break;
    }

    hash ^= hash << 3U;
    hash += hash >> 5U;
    hash ^= hash << 4U;
    hash += hash >> 17U;
    hash ^= hash << 25U;
    hash += hash >> 6U;
    return hash;
}

} // namespace whitehole::smg
