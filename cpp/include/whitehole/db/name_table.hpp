#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace whitehole::db {

class NameTable {
public:
    void loadJson(const std::filesystem::path& path);
    [[nodiscard]] std::string displayName(std::string_view identifier) const;

private:
    std::unordered_map<std::string, std::string> names_;
};

} // namespace whitehole::db
