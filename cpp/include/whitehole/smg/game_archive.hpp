#pragma once

#include "whitehole/io/directory_filesystem.hpp"
#include "whitehole/smg/galaxy_archive.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace whitehole::smg {

class GameArchive {
public:
    explicit GameArchive(std::filesystem::path root);

    [[nodiscard]] const io::DirectoryFilesystem& filesystem() const noexcept { return filesystem_; }
    [[nodiscard]] io::DirectoryFilesystem& filesystem() noexcept { return filesystem_; }
    [[nodiscard]] int gameType() const noexcept { return gameType_; }
    [[nodiscard]] const std::vector<std::string>& galaxies() const noexcept { return galaxies_; }
    [[nodiscard]] const std::vector<std::string>& zones() const noexcept { return zones_; }
    [[nodiscard]] const std::vector<std::string>& worlds() const noexcept { return worlds_; }
    [[nodiscard]] bool galaxyExists(std::string_view name) const;
    [[nodiscard]] GalaxyArchive openGalaxy(std::string_view name);

private:
    io::DirectoryFilesystem filesystem_;
    int gameType_{0};
    std::vector<std::string> galaxies_;
    std::vector<std::string> zones_;
    std::vector<std::string> worlds_;
};

} // namespace whitehole::smg
