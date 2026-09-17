#include "whitehole/smg/hash.hpp"

#include <cstddef>
#include <cstdint>

namespace whitehole::smg {

[[nodiscard]] std::uint32_t jmapHash(std::string_view value) noexcept {
    std::uint32_t hash = 0;
    for (std::size_t i = 0; i < value.size(); ++i) {
        hash = (hash * 31U) + static_cast<std::uint32_t>(static_cast<unsigned char>(value[i]));
    }
    return hash;
}

[[nodiscard]] std::uint32_t superFastHash(std::string_view value, std::uint32_t seed) noexcept {
    auto hash = seed;
    std::size_t pos = 0;
    std::size_t count = value.size() / 4;

    for (std::size_t k = 0; k < count; ++k) {
        const std::uint32_t low =
            static_cast<std::uint32_t>(static_cast<unsigned char>(value[pos]))
            | (static_cast<std::uint32_t>(static_cast<unsigned char>(value[pos + 1])) << 8U);
        const std::uint32_t high =
            static_cast<std::uint32_t>(static_cast<unsigned char>(value[pos + 2]))
            | (static_cast<std::uint32_t>(static_cast<unsigned char>(value[pos + 3])) << 8U);
        pos += 4;

        hash += low;
        hash = hash << 16U ^ ((high << 11U) ^ hash);
        hash += hash >> 11U;
    }

    switch (value.size() & 3U) {
    case 3:
        hash += static_cast<std::uint32_t>(static_cast<unsigned char>(value[pos]))
                | (static_cast<std::uint32_t>(static_cast<unsigned char>(value[pos + 1])) << 8U);
        hash ^= hash << 16U;
        hash ^= static_cast<std::uint32_t>(static_cast<unsigned char>(value[pos + 2])) << 18U;
        hash += hash >> 11U;
        break;
    case 2:
        hash += static_cast<std::uint32_t>(static_cast<unsigned char>(value[pos]))
                | (static_cast<std::uint32_t>(static_cast<unsigned char>(value[pos + 1])) << 8U);
        hash ^= hash << 11U;
        hash += hash >> 17U;
        break;
    case 1:
        hash += static_cast<unsigned char>(value[pos]);
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
