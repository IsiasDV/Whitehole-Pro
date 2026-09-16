#include "whitehole/io/yaz0.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace whitehole::io::yaz0 {
namespace {

struct Match {
    std::size_t position{0};
    std::size_t length{0};
};

Match findMatch(const std::vector<std::uint8_t>& data, std::size_t position) {
    Match best;
    if (position + 2 >= data.size()) {
        return best;
    }

    const auto start = position > 4096 ? position - 4096 : 0;
    for (auto candidate = start; candidate < position; ++candidate) {
        if (data[candidate] != data[position]) {
            continue;
        }
        std::size_t length = 1;
        const auto maximum = std::min<std::size_t>(273, data.size() - position);
        while (length < maximum && data[candidate + length] == data[position + length]) {
            ++length;
        }
        if (length >= 3 && length > best.length) {
            best = {candidate, length};
            if (length == maximum) {
                break;
            }
        }
    }
    return best;
}

} // namespace

bool isCompressed(const std::vector<std::uint8_t>& data) noexcept {
    return data.size() >= 4 && data[0] == 'Y' && data[1] == 'a' && data[2] == 'z' && data[3] == '0';
}

std::vector<std::uint8_t> decompress(const std::vector<std::uint8_t>& data) {
    if (!isCompressed(data)) {
        return data;
    }
    if (data.size() < 16) {
        throw std::runtime_error("Yaz0 header is truncated");
    }

    const auto outputSize = (static_cast<std::uint32_t>(data[4]) << 24U)
        | (static_cast<std::uint32_t>(data[5]) << 16U)
        | (static_cast<std::uint32_t>(data[6]) << 8U)
        | static_cast<std::uint32_t>(data[7]);
    if (outputSize > (1U << 30U)) {
        throw std::runtime_error("Yaz0 output exceeds the 1 GiB safety limit");
    }

    std::vector<std::uint8_t> output;
    output.reserve(outputSize);
    std::size_t inputPosition = 16;

    while (output.size() < outputSize) {
        if (inputPosition >= data.size()) {
            throw std::runtime_error("Yaz0 stream ended before reaching its declared size");
        }
        const auto flags = data[inputPosition++];
        for (unsigned bit = 0; bit < 8 && output.size() < outputSize; ++bit) {
            if ((flags & (0x80U >> bit)) != 0) {
                if (inputPosition >= data.size()) {
                    throw std::runtime_error("Yaz0 literal is truncated");
                }
                output.push_back(data[inputPosition++]);
                continue;
            }

            if (inputPosition + 1 >= data.size()) {
                throw std::runtime_error("Yaz0 back-reference is truncated");
            }
            const auto first = data[inputPosition++];
            const auto second = data[inputPosition++];
            const auto distance = static_cast<std::size_t>(((first & 0x0FU) << 8U) | second) + 1;
            std::size_t length = first >> 4U;
            if (length == 0) {
                if (inputPosition >= data.size()) {
                    throw std::runtime_error("Yaz0 long back-reference is truncated");
                }
                length = static_cast<std::size_t>(data[inputPosition++]) + 0x12U;
            } else {
                length += 2;
            }
            if (distance > output.size()) {
                throw std::runtime_error("Yaz0 back-reference points before the output buffer");
            }

            for (std::size_t index = 0; index < length && output.size() < outputSize; ++index) {
                output.push_back(output[output.size() - distance]);
            }
        }
    }
    return output;
}

std::vector<std::uint8_t> compress(const std::vector<std::uint8_t>& data) {
    if (isCompressed(data)) {
        return data;
    }
    if (data.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("Yaz0 only supports inputs up to 4 GiB");
    }

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

    std::size_t position = 0;
    while (position < data.size()) {
        const auto flagPosition = output.size();
        output.push_back(0);
        std::uint8_t flags = 0;

        for (unsigned bit = 0; bit < 8 && position < data.size(); ++bit) {
            auto match = findMatch(data, position);
            if (match.length >= 3 && position + 1 < data.size()) {
                const auto next = findMatch(data, position + 1);
                if (next.length > match.length + 1) {
                    match = {};
                }
            }

            if (match.length < 3) {
                flags |= static_cast<std::uint8_t>(0x80U >> bit);
                output.push_back(data[position++]);
                continue;
            }

            const auto distance = position - match.position - 1;
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

} // namespace whitehole::io::yaz0
