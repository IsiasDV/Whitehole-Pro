#pragma once

#include "whitehole/io/binary_file.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace whitehole::io {

struct RarcEntry {
    std::string path;
    bool directory{false};
    std::size_t size{0};
    std::size_t dataOffset{0};
};

class RarcArchive {
public:
    explicit RarcArchive(std::vector<std::uint8_t> bytes);

    [[nodiscard]] static RarcArchive open(const std::filesystem::path& path);
    [[nodiscard]] Endian endian() const noexcept { return endian_; }
    [[nodiscard]] bool wasCompressed() const noexcept { return wasCompressed_; }
    [[nodiscard]] std::string_view rootName() const noexcept { return rootName_; }
    [[nodiscard]] const std::vector<RarcEntry>& entries() const noexcept { return entries_; }
    [[nodiscard]] std::vector<std::uint8_t> read(const RarcEntry& entry) const;
    void replace(std::string_view path, std::vector<std::uint8_t> data);
    [[nodiscard]] std::vector<std::uint8_t> serialize(bool compress) const;

    void extractAll(const std::filesystem::path& destination) const;

private:
    void parse();
    void parseNode(std::uint32_t index, const std::string& path, std::vector<bool>& visited,
                   std::size_t nodeOffset, std::size_t entryOffset, std::size_t stringOffset,
                   std::size_t dataOffset, std::uint32_t nodeCount, std::uint32_t entryCount,
                   std::size_t depth = 0);
    [[nodiscard]] std::string readName(std::size_t stringOffset, std::uint32_t relativeOffset) const;

    std::vector<std::uint8_t> bytes_;
    std::vector<RarcEntry> entries_;
    Endian endian_{Endian::big};
    bool wasCompressed_{false};
    std::string rootName_;
    std::size_t stringTableEnd_{0};
    std::uint32_t metadata_{0};
    std::vector<std::optional<std::vector<std::uint8_t>>> replacements_;
};

} // namespace whitehole::io
