#include "whitehole/db/areamanagerlimits.hpp"

namespace whitehole::db {

AreaManagerLimits::AreaManagerLimits()
    : DataHolderBase("data/areamanagerlimits.json", "/areamanagerlimits.json", true) {}

void AreaManagerLimits::load(int gameType) {
    aliases_.clear();
    limits_.clear();
    loaded_ = false;
    if (!dataPresent()) return;

    const auto& root = projectOrRoot();
    const std::string gameStr = (gameType == 1) ? "SMG1" : "SMG2";

    const auto aliasObj = root.at("AreaManagerAliases");
    if (aliasObj.isObject()) {
        const auto gameAliases = aliasObj.at(gameStr);
        if (gameAliases.isObject()) {
            for (const auto& [key, val] : gameAliases.asObject()) {
                                const std::string value = val.isString() ? val.asString() : "";
                if (!key.empty() && !value.empty()) {
                    aliases_[key] = value;
                }
            }
        }
    }

    const auto managersObj = root.at("AreaManagers");
    if (managersObj.isObject()) {
        const auto gameManagers = managersObj.at(gameStr);
        if (gameManagers.isObject()) {
            for (const auto& [key, val] : gameManagers.asObject()) {
                                    const std::string value = val.isString() ? val.asString() : "";
                    if (!key.empty() && !value.empty()) {
                        limits_[key] = value;
                    }
            }
        }
    }
    loaded_ = true;
}

std::string AreaManagerLimits::resolveAlias(std::string_view alias) const {
    const auto it = aliases_.find(std::string(alias));
    if (it == aliases_.end()) return std::string(alias);
    return it->second;
}

std::string AreaManagerLimits::limitFor(std::string_view managerName) const {
    const std::string canonical = resolveAlias(managerName);
    const auto it = limits_.find(canonical);
    if (it == limits_.end()) return "";
    return it->second;
}

}  // namespace whitehole::db