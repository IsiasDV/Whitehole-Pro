#include "whitehole/smg/game_archive.hpp"

#include "whitehole/util/text.hpp"

#include <algorithm>
#include <regex>
#include <stdexcept>

namespace whitehole::smg {

GameArchive::GameArchive(std::filesystem::path root) : filesystem_(std::move(root)) {
    if (filesystem_.fileExists("/StageData/ObjNameTable.arc")) {
        gameType_ = 1;
    } else if (filesystem_.fileExists("/AudioRes/Info/ActionSound.arc")
               || filesystem_.fileExists("/SystemData/ObjNameTable.arc")) {
        gameType_ = 2;
    } else {
        return;
    }

    for (const auto& stage : filesystem_.directories("/StageData")) {
        const auto scenario = "/StageData/" + stage + "/" + stage + "Scenario.arc";
        if (filesystem_.fileExists(scenario)) {
            galaxies_.push_back(stage);
        }
    }
    std::sort(galaxies_.begin(), galaxies_.end());

    if (gameType_ == 1) {
        for (const auto& file : filesystem_.files("/StageData")) {
            if (file.size() >= 4 && util::equalIgnoreCase(file.substr(file.size() - 4), ".arc")
                && (file.find("Galaxy.arc") != std::string::npos || file.find("Zone.arc") != std::string::npos)) {
                zones_.push_back(file.substr(0, file.size() - 4));
            }
        }
    } else {
        const std::regex worldPattern("^WorldMap\\d{2}Galaxy$");
        for (const auto& stage : filesystem_.directories("/StageData")) {
            const auto mapPath = "/StageData/" + stage + "/" + stage + "Map.arc";
            if (!filesystem_.fileExists(mapPath)) {
                continue;
            }
            zones_.push_back(stage);
            if (std::regex_match(stage, worldPattern)
                && filesystem_.fileExists("/ObjectData/" + stage.substr(0, stage.size() - 6) + ".arc")) {
                worlds_.push_back(stage);
            }
        }
    }
    std::sort(zones_.begin(), zones_.end());
    std::sort(worlds_.begin(), worlds_.end());
}

bool GameArchive::galaxyExists(std::string_view name) const {
    return std::find(galaxies_.begin(), galaxies_.end(), name) != galaxies_.end();
}

GalaxyArchive GameArchive::openGalaxy(std::string_view name) {
    if (!galaxyExists(name)) {
        throw std::runtime_error("Galaxy does not exist: " + std::string(name));
    }
    return GalaxyArchive(filesystem_, std::string(name), gameType_);
}

} // namespace whitehole::smg
