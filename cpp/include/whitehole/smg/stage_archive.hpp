#pragma once

#include "whitehole/io/directory_filesystem.hpp"
#include "whitehole/io/rarc.hpp"
#include "whitehole/smg/bcsv.hpp"
#include "whitehole/smg/placement.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace whitehole::smg {

struct ObjectTable {
    std::string path;
    std::string kind;
    std::string layer;
    BcsvTable table;
};

class StageArchive {
public:
    [[nodiscard]] static StageArchive openMapFile(const std::filesystem::path& path, int gameType = 2);
    [[nodiscard]] static StageArchive open(io::DirectoryFilesystem& filesystem, std::string_view stageName,
                                           int gameType);

    [[nodiscard]] const std::string& stageName() const noexcept { return stageName_; }
    [[nodiscard]] const std::filesystem::path& sourcePath() const noexcept { return sourcePath_; }
    [[nodiscard]] const std::vector<ObjectTable>& tables() const noexcept { return tables_; }
    [[nodiscard]] const std::vector<PlacementObject>& objects() const noexcept { return objects_; }
    [[nodiscard]] std::vector<PlacementObject>& objects() noexcept { return objects_; }

    void applyEdits();
    void save();
    void saveTo(const std::filesystem::path& path);

private:
    void loadFromArchive();
    void loadTable(std::string_view path, std::string kind, std::string layer);

    io::DirectoryFilesystem* filesystem_{nullptr};
    std::string stageName_;
    std::filesystem::path sourcePath_;
    std::string filesystemPath_;
    int gameType_{2};
    std::optional<io::RarcArchive> archive_;
    std::vector<ObjectTable> tables_;
    std::vector<PlacementObject> objects_;
};

} // namespace whitehole::smg
