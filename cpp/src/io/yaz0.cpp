#include "whitehole/io/yaz0.hpp"

#include "whitehole/io/binary_view.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

namespace whitehole::io::yaz0 {

namespace {

constexpr std::size_t chainSize = 65536;
constexpr std::size_t hashMask = chainSize - 1;
constexpr std::size_t minMatch = 3;
constexpr std::size_t maxWindow = 4096;
constexpr std::size_t maxChain = 128;

[[nodiscard]] std::uint32_t threeByteHash(std::uint8_t a, std::uint8_t b, std::uint8_t c) noexcept {
    std::uint32_t h = static_cast<std::uint32_t>(a) * 31u + static_cast<std::uint32_t>(b);
    h = h * 31u + static_cast<std::uint32_t>(c);
    return h & hashMask;
}

struct MatchResult {
    std::size_t position{0};
    std::size_t length{0};
};

[[nodiscard]] MatchResult bestMatchAt(std::span<const std::uint8_t> data, std::size_t position,
                                      const std::vector<std::uint16_t>& head,
                                      const std::vector<std::uint16_t>& prev) noexcept {
    if (position >= data.size()) {
        return {};
    }
    const std::size_t maxLen = std::min<std::size_t>(273, data.size() - position);
    if (maxLen < minMatch || position + 2 >= data.size()) {
        return {};
    }

    const auto h = threeByteHash(data[position], data[position + 1], data[position + 2]);
    std::size_t bestPos = 0;
    std::size_t bestLen = 0;
    std::size_t chain = 0;

    for (std::size_t i = head[h]; i != std::numeric_limits<std::uint16_t>::max(); i = prev[i]) {
        if (i >= position || i + 2 >= data.size()) {
            continue;
        }
        if (data[i] != data[position] || data[i + 1] != data[position + 1]) {
            continue;
        }
        std::size_t len = 2;
        const std::size_t maxCompare = std::min({data.size() - position, data.size() - i, maxLen});
        while (len < maxCompare && data[i + len] == data[position + len]) {
            ++len;
        }
        if (len >= minMatch && len > bestLen) {
            bestPos = i;
            bestLen = len;
            if (len >= maxLen) {
                break;
            }
        }
        if (++chain > maxChain) {
            break;
        }
    }

    return {bestPos, bestLen};
}

void buildHashTables(std::span<const std::uint8_t> data,
                     std::vector<std::uint16_t>& head,
                     std::vector<std::uint16_t>& prev) {
    head.assign(data.size() + 1, std::numeric_limits<std::uint16_t>::max());
    prev.assign(data.size() + 1, std::numeric_limits<std::uint16_t>::max());
    for (std::size_t i = 0; i + 2 < data.size(); ++i) {
        const auto h = threeByteHash(data[i], data[i + 1], data[i + 2]);
        prev[i] = head[h];
        head[h] = static_cast<std::uint16_t>(i);
    }
}

std::vector<std::uint8_t> compressGeneric(std::span<const std::uint8_t> data) {
    std::vector<std::uint8_t> output(16, 0);
    output[0] = 'Y';
    output[1] = 'a';
    output[2] = 'z';
    output[3] = '0';
    const auto size = static_cast<std::uint32_t>(data.size());
    output[4] = static_cast<std::uint8_t>(size >> 24U);
    output[5] = static_cast<std::uint8_t>(size >> 16U);
    output[6] = static_cast<std::uint8_t>(size >> 8U);
    output[7] = static_cast<std::uint8_t>(size);

    std::vector<std::uint16_t> head, prev;
    buildHashTables(data, head, prev);

    std::size_t position = 0;
    while (position < data.size()) {
        const auto flagPosition = output.size();
        output.push_back(0);
        std::uint8_t flags = 0;

        for (unsigned bit = 0; bit < 8 && position < data.size(); ++bit) {
            const auto match = bestMatchAt(data, position, head, prev);
            bool useMatch = false;

            if (match.length >= minMatch && position + 1 < data.size()) {
                const auto next = bestMatchAt(data, position + 1, head, prev);
                if (next.position != 0 && next.length > match.length + 1) {
                    useMatch = false;
                } else {
                    useMatch = true;
                }
            }

            if (!useMatch) {
                flags |= static_cast<std::uint8_t>(0x80U >> bit);
                output.push_back(data[position++]);
                continue;
            }

            const auto distance = position - match.position - 1;
            if (distance > 0xFFFF) {
                useMatch = false;
            }

            if (!useMatch || match.length < minMatch) {
                flags |= static_cast<std::uint8_t>(0x80U >> bit);
                output.push_back(data[position++]);
                continue;
            }

            if (match.length >= 0x12) {
                output.push_back(static_cast<std::uint8_t>(distance >> 8U));
                output.push_back(static_cast<std::uint8_t>(distance));
                output.push_back(static_cast<std::uint8_t>(match.length - 0x12U));
            } else {
                output.push_back(static_cast<std::uint8_t>(((match.length - 2U) << 4U) | (distance >> 8U)));
                output.push_back(static_cast<std::uint8_t>(distance));
            }
            position += match.length;
        }
        output[flagPosition] = flags;
    }
    return output;
}

} // namespace

bool isCompressed(std::span<const std::uint8_t> data) noexcept {
    return data.size() >= 4 && data[0] == 'Y' && data[1] == 'a' && data[2] == 'z' && data[3] == '0';
}

bool isCompressed(const std::vector<std::uint8_t>& data) noexcept {
    return isCompressed(std::span{data});
}

std::vector<std::uint8_t> decompress(const std::vector<std::uint8_t>& data) {
    std::vector<std::uint8_t> output;
    decompressTo(data, output);
    return output;
}

void decompressTo(const std::vector<std::uint8_t>& data, std::vector<std::uint8_t>& output) {
    if (!isCompressed(data)) {
        output = data;
        return;
    }
    if (data.size() < 16) {
        throw binary::Error{"Yaz0 header is truncated", {}, 0};
    }

    const auto outputSize = binary::read32BE(std::span{data}.subspan<4, 4>());
    if (outputSize > (1U << 30U)) {
        throw binary::Error{"Yaz0 output exceeds the 1 GiB safety limit", {}, 0};
    }

    output.assign(outputSize, 0);
    std::size_t outputPosition = 0;
    std::size_t inputPosition = 16;

    while (outputPosition < outputSize) {
        if (inputPosition >= data.size()) {
            throw binary::Error{"Yaz0 stream ended before reaching its declared size", {}, inputPosition};
        }
        const auto flags = data[inputPosition++];
        for (unsigned bit = 0; bit < 8 && outputPosition < outputSize; ++bit) {
            if ((flags & (0x80U >> bit)) != 0) {
                if (inputPosition >= data.size()) {
                    throw binary::Error{"Yaz0 literal is truncated", {}, inputPosition};
                }
                output[outputPosition++] = data[inputPosition++];
                continue;
            }

            if (inputPosition + 1 >= data.size()) {
                throw binary::Error{"Yaz0 back-reference is truncated", {}, inputPosition};
            }
            const auto first = data[inputPosition++];
            const auto second = data[inputPosition++];
            const auto distance = static_cast<std::size_t>(((first & 0x0FU) << 8U) | second) + 1;
            std::size_t length = first >> 4U;
            if (length == 0) {
                if (inputPosition >= data.size()) {
                    throw binary::Error{"Yaz0 long back-reference is truncated", {}, inputPosition};
                }
                length = static_cast<std::size_t>(data[inputPosition++]) + 0x12U;
            } else {
                length += 2;
            }
            if (distance > outputPosition) {
                throw binary::Error{"Yaz0 back-reference points before the output buffer", {}, inputPosition};
            }

            auto src = output.data() + outputPosition - distance;
            std::size_t remaining = outputSize - outputPosition;
            std::size_t copyLen = length;
            if (copyLen > remaining) {
                copyLen = remaining;
            }
            if (copyLen < 16) {
                for (std::size_t i = 0; i < copyLen; ++i) {
                    output[outputPosition + i] = src[i];
                }
            } else {
                std::memcpy(output.data() + outputPosition, src, copyLen);
            }
            outputPosition += copyLen;
        }
    }
}

std::vector<std::uint8_t> compress(const std::vector<std::uint8_t>& data, unsigned level) {
    return compress(std::span{data}, level);
}

std::vector<std::uint8_t> compress(std::span<const std::uint8_t> data, unsigned level) {
    if (isCompressed(data)) {
        return std::vector<std::uint8_t>{data.begin(), data.end()};
    }
    if (data.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw binary::Error{"Yaz0 only supports inputs up to 4 GiB", {}, 0};
    }
    return compressGeneric(data);
}

std::size_t estimateCompressedSize(const std::vector<std::uint8_t>& data, unsigned level) noexcept {
    if (isCompressed(data) || data.size() < 16) {
        return data.size();
    }
    if (level == 0 || data.size() < 128) {
        return data.size() * 11 / 10 + 64;
    }
    return data.size() * 11 / 10 + 64;
}

} // namespace whitehole::io::yaz0
