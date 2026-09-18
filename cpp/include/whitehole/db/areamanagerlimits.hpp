#ifndef WHITEHOLE_DB_AREAMANAGERLIMITS_HPP
#define WHITEHOLE_DB_AREAMANAGERLIMITS_HPP

#include <string>
#include <string_view>
#include <unordered_map>

#include "whitehole/db/data_holder.hpp"

namespace whitehole::db {

// Native port of whitehole.db.AreaManagerLimits. JSON file
// (data/areamanagerlimits.json) has:
//   "AreaManagerAliases": { "SMG1": {...}, "SMG2": {...} }  // alias -> canonical
//   "AreaManagers":        { "SMG1": {...}, "SMG2": {...} }  // canonical -> limit
class AreaManagerLimits final : public DataHolderBase {
public:
    AreaManagerLimits();

    // Re-parses base/project JSON, resolving alias + limits maps for the given game.
    void load(int gameType);

    [[nodiscard]] bool isLoaded() const noexcept { return loaded_; }

    // Resolve an alias to its canonical manager name (returns input if not aliased).
    [[nodiscard]] std::string resolveAlias(std::string_view alias) const;

    // Get the limit for a manager name (alias or canonical); empty if not found.
    [[nodiscard]] std::string limitFor(std::string_view managerName) const;

private:
    std::unordered_map<std::string, std::string> aliases_;   // alias -> canonical
    std::unordered_map<std::string, std::string> limits_;    // canonical -> limit
    bool loaded_{false};
};

}  // namespace whitehole::db

#endif  // WHITEHOLE_DB_AREAMANAGERLIMITS_HPP