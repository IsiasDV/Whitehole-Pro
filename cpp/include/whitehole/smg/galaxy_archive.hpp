#pragma once

#include "whitehole/io/directory_filesystem.hpp"
#include "whitehole/smg/bcsv.hpp"
#include "whitehole/smg/stage_archive.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace whitehole::smg {

class GalaxyArchive {
public:
    GalaxyArchive(io::DirectoryFilesystem& filesystem, std::string name, int gameType);

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] const std::vector<std::string>& zones() const noexcept { return zones_; }
    [[nodiscard]] const BcsvTable& scenarioData() const noexcept { return scenarioData_; }
    [[nodiscard]] StageArchive openZone(std::string_view zoneName) const;

private:
    io::DirectoryFilesystem* filesystem_;
    std::string name_;
    int gameType_{0};
    std::vector<std::string> zones_;
    BcsvTable scenarioData_;
};

} // namespace whitehole::smg
