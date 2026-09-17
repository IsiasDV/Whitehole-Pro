#include "whitehole/smg/galaxy_archive.hpp"

#include "whitehole/io/rarc.hpp"

#include <algorithm>
#include <stdexcept>

namespace whitehole::smg {

GalaxyArchive::GalaxyArchive(io::DirectoryFilesystem& filesystem, std::string name, int gameType)
    : filesystem_(&filesystem), name_(std::move(name)), gameType_(gameType) {
    const auto scenarioPath = "/StageData/" + name_ + "/" + name_ + "Scenario.arc";
    if (!filesystem_->fileExists(scenarioPath)) {
        throw std::runtime_error("Galaxy scenario archive is missing: " + scenarioPath);
    }
    const auto archive = io::RarcArchive(filesystem_->read(scenarioPath));
    const auto zoneTable = BcsvTable(archive.read("ZoneList.bcsv"), archive.endian());
    for (const auto& row : zoneTable.rows()) {
        zones_.push_back(zoneTable.getString(row, "ZoneName"));
    }
    if (archive.fileExists("ScenarioData.bcsv")) {
        scenarioData_ = BcsvTable(archive.read("ScenarioData.bcsv"), archive.endian());
    }
}

StageArchive GalaxyArchive::openZone(std::string_view zoneName) const {
    const auto found = std::find(zones_.begin(), zones_.end(), zoneName);
    if (found == zones_.end()) {
        throw std::runtime_error("Zone is not part of this galaxy: " + std::string(zoneName));
    }
    return StageArchive::open(*filesystem_, zoneName, gameType_);
}

} // namespace whitehole::smg
