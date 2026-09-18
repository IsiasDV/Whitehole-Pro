#ifndef WHITEHOLE_DB_HINTS_HPP
#define WHITEHOLE_DB_HINTS_HPP

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "whitehole/db/data_holder.hpp"

namespace whitehole::db {

struct HintEntry {
    std::string name;
    std::string hint;
};

// Native port of whitehole.db.Hints. The JSON file (data/hints.json) has a
// top-level "Hints" array of {Hint, Name?, Game} objects, where Game is 0 for
// all games, 1 for SMG1, 2 for SMG2. One Hints instance holds the applicable
// list for the currently-selected game; load() filters by game type.
class Hints final : public DataHolderBase {
public:
    Hints();

    // Re-parses the (base or project) JSON root, filtering entries whose
    // Game field matches 0 (all), 1 (SMG1), or 2 (SMG2) as appropriate.
    void load(int gameType);

    // Returns true if load() was successfully called.
    [[nodiscard]] bool isLoaded() const noexcept { return loaded_; }

    // Hints applicable to the current game, in file order.
    [[nodiscard]] const std::vector<HintEntry>& hints() const noexcept { return applicableHints_; }

private:
    std::vector<HintEntry> applicableHints_;
    bool loaded_{false};
};

}  // namespace whitehole::db

#endif  // WHITEHOLE_DB_HINTS_HPP