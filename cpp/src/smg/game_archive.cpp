#include "whitehole/smg/game_archive.hpp"

#include "whitehole/util/text.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace whitehole::smg {
namespace {

// Recognises the "WorldMapNNGalaxy" staging pattern without paying for <regex>.
[[nodiscard]] bool isWorldMapGalaxy(std::string_view name) noexcept {
    constexpr std::string_view prefix = "WorldMap";
    constexpr std::string_view suffix = "Galaxy";
    if (name.size() != prefix.size() + 2 + suffix.size()) {
        return false;
    }
    if (name.substr(0, prefix.size()) != prefix || name.substr(name.size() - suffix.size()) != suffix) {
        return false;
    }
    return std::isdigit(static_cast<unsigned char>(name[prefix.size()])) != 0
        && std::isdigit(static_cast<unsigned char>(name[prefix.size() + 1])) != 0;
}

} // namespace

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
        for (const auto& stage : filesystem_.directories("/StageData")) {
            const auto mapPath = "/StageData/" + stage + "/" + stage + "Map.arc";
            if (!filesystem_.fileExists(mapPath)) {
                continue;
            }
            zones_.push_back(stage);
            if (isWorldMapGalaxy(stage)
                && filesystem_.fileExists("/ObjectData/" + stage.substr(0, stage.size() - 6) + ".arc")) {
                worlds_.push_back(stage);
            }
        }
    }
    std::sort(zones_.begin(), zones_.end());
    std::sort(worlds_.begin(), worlds_.end());
}

bool GameArchive::galaxyExists(std::string_view name) const {
    return std::ranges::find(galaxies_, name) != galaxies_.end();
}

GalaxyArchive GameArchive::openGalaxy(std::string_view name) {
    if (!galaxyExists(name)) {
        throw std::runtime_error("Galaxy does not exist: " + std::string(name));
    }
    return GalaxyArchive(filesystem_, std::string(name), gameType_);
}

} // namespace whitehole::smg
