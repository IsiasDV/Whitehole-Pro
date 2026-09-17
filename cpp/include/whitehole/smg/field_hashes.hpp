#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace whitehole::smg {

class FieldHashes {
public:
    void loadFile(const std::filesystem::path& path);
    void addName(std::string_view name);
    [[nodiscard]] std::string nameOf(std::uint32_t hash) const;

private:
    std::unordered_map<std::uint32_t, std::string> names_;
};

} // namespace whitehole::smg
