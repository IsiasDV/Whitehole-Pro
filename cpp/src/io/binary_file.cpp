#include "whitehole/io/binary_file.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace whitehole::io {

std::vector<std::uint8_t> readFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        throw std::runtime_error("Could not open file for reading: " + path.string());
    }

    const auto end = stream.tellg();
    if (end < 0) {
        throw std::runtime_error("Could not determine file size: " + path.string());
    }
    const auto size = static_cast<std::uintmax_t>(end);
    if (size > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("File is too large to load: " + path.string());
    }

    std::vector<std::uint8_t> result(static_cast<std::size_t>(size));
    stream.seekg(0);
    if (!result.empty() && !stream.read(reinterpret_cast<char*>(result.data()), end)) {
        throw std::runtime_error("Could not read file: " + path.string());
    }
    return result;
}

void writeFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& data) {
    if (const auto parent = path.parent_path(); !parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw std::runtime_error("Could not open file for writing: " + path.string());
    }
    if (!data.empty()) {
        stream.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    }
    if (!stream) {
        throw std::runtime_error("Could not write file: " + path.string());
    }
}

BinaryReader::BinaryReader(const std::vector<std::uint8_t>& data, Endian endian)
    : data_(data), endian_(endian) {}

void BinaryReader::require(std::size_t count) const {
    if (count > remaining()) {
        throw std::runtime_error("Unexpected end of binary data at offset " + std::to_string(position_));
    }
}

void BinaryReader::seek(std::size_t position) {
    if (position > data_.size()) {
        throw std::runtime_error("Binary seek is outside the file");
    }
    position_ = position;
}

void BinaryReader::skip(std::ptrdiff_t amount) {
    if (amount < 0) {
        const auto distance = static_cast<std::size_t>(-amount);
        if (distance > position_) {
            throw std::runtime_error("Binary seek is before the start of the file");
        }
        position_ -= distance;
        return;
    }
    const auto distance = static_cast<std::size_t>(amount);
    require(distance);
    position_ += distance;
}

std::uint8_t BinaryReader::readU8() {
    require(1);
    return data_[position_++];
}

std::uint16_t BinaryReader::readU16() {
    require(2);
    const auto a = static_cast<std::uint16_t>(data_[position_]);
    const auto b = static_cast<std::uint16_t>(data_[position_ + 1]);
    position_ += 2;
    return endian_ == Endian::big ? static_cast<std::uint16_t>((a << 8U) | b)
                                  : static_cast<std::uint16_t>(a | (b << 8U));
}

std::uint32_t BinaryReader::readU32() {
    require(4);
    std::uint32_t value = 0;
    if (endian_ == Endian::big) {
        for (int i = 0; i < 4; ++i) {
            value = (value << 8U) | data_[position_++];
        }
    } else {
        for (int i = 0; i < 4; ++i) {
            value |= static_cast<std::uint32_t>(data_[position_++]) << (i * 8U);
        }
    }
    return value;
}

float BinaryReader::readF32() {
    const auto bits = readU32();
    float value = 0.0F;
    static_assert(sizeof(value) == sizeof(bits));
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::vector<std::uint8_t> BinaryReader::readBytes(std::size_t count) {
    require(count);
    const auto begin = data_.begin() + static_cast<std::ptrdiff_t>(position_);
    position_ += count;
    return {begin, begin + static_cast<std::ptrdiff_t>(count)};
}

std::string BinaryReader::readString(std::size_t maxLength) {
    std::string result;
    while (remaining() > 0 && (maxLength == 0 || result.size() < maxLength)) {
        const auto value = readU8();
        if (value == 0) {
            break;
        }
        result.push_back(static_cast<char>(value));
    }
    return result;
}

void BinaryWriter::reserve(std::size_t count) {
    if (count > std::numeric_limits<std::size_t>::max() - position_) {
        throw std::overflow_error("Binary output is too large");
    }
    const auto required = position_ + count;
    if (required > data_.size()) {
        data_.resize(required, 0);
    }
}

void BinaryWriter::seek(std::size_t position) {
    if (position > data_.size()) {
        data_.resize(position, 0);
    }
    position_ = position;
}

void BinaryWriter::writeU8(std::uint8_t value) {
    reserve(1);
    data_[position_++] = value;
}

void BinaryWriter::writeU16(std::uint16_t value) {
    reserve(2);
    if (endian_ == Endian::big) {
        data_[position_++] = static_cast<std::uint8_t>(value >> 8U);
        data_[position_++] = static_cast<std::uint8_t>(value);
    } else {
        data_[position_++] = static_cast<std::uint8_t>(value);
        data_[position_++] = static_cast<std::uint8_t>(value >> 8U);
    }
}

void BinaryWriter::writeU32(std::uint32_t value) {
    reserve(4);
    if (endian_ == Endian::big) {
        for (int i = 3; i >= 0; --i) {
            data_[position_++] = static_cast<std::uint8_t>(value >> (i * 8U));
        }
    } else {
        for (int i = 0; i < 4; ++i) {
            data_[position_++] = static_cast<std::uint8_t>(value >> (i * 8U));
        }
    }
}

void BinaryWriter::writeF32(float value) {
    std::uint32_t bits = 0;
    static_assert(sizeof(value) == sizeof(bits));
    std::memcpy(&bits, &value, sizeof(bits));
    writeU32(bits);
}

void BinaryWriter::writeBytes(const std::vector<std::uint8_t>& value) {
    reserve(value.size());
    std::copy(value.begin(), value.end(), data_.begin() + static_cast<std::ptrdiff_t>(position_));
    position_ += value.size();
}

void BinaryWriter::writeString(std::string_view value, bool nullTerminate) {
    reserve(value.size() + (nullTerminate ? 1U : 0U));
    for (const char character : value) {
        data_[position_++] = static_cast<std::uint8_t>(character);
    }
    if (nullTerminate) {
        data_[position_++] = 0;
    }
}

} // namespace whitehole::io
