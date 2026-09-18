#pragma once

// Lightweight object-database lookup. Java ObjectDB parses the full
// community objectdb.json (huge, Swing-tree helpers, auto-update). The native
// editor only needs fast, renderer-safe answers: does an object exist, what
// is its friendly name, and does a model file exist — so we parse just the
// fields we use and keep everything in flat hash maps.

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace whitehole::db {

struct ObjectInfo {
    std::string name;
    std::string category;
    std::string description;
};

class ObjectDatabase {
public:
    void load(const std::filesystem::path& path);
    void clear() noexcept;

    [[nodiscard]] bool empty() const noexcept { return objects_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return objects_.size(); }
    [[nodiscard]] bool contains(std::string_view name) const;
    [[nodiscard]] std::string displayName(std::string_view name) const;
    [[nodiscard]] const ObjectInfo* find(std::string_view name) const;
    [[nodiscard]] std::vector<std::string> names() const;

private:
    std::unordered_map<std::string, ObjectInfo> objects_;
};

} // namespace whitehole::db
