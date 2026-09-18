#ifndef WHITEHOLE_DB_SHORTCUTS_HPP
#define WHITEHOLE_DB_SHORTCUTS_HPP

#include <string>
#include <string_view>
#include <unordered_map>

#include "whitehole/db/data_holder.hpp"

namespace whitehole::db {

// Native port of whitehole.db.Shortcuts. In Java this is just a generic
// GameAndProjectDataHolder for data/shortcuts.json (no separate class).
// The C++ wrapper adds a typed lookup for shortcut JSON content.
class Shortcuts final : public DataHolderBase {
public:
    Shortcuts();

    // Re-parses base/project JSON into an in-memory copy of the root.
    void load();

    [[nodiscard]] bool isLoaded() const noexcept { return loaded_; }

    // Returns the value of a top-level key in the JSON root, or empty.
    [[nodiscard]] std::string get(std::string_view key) const;

private:
    whitehole::util::JsonValue data_;
    bool loaded_{false};
};

}  // namespace whitehole::db

#endif  // WHITEHOLE_DB_SHORTCUTS_HPP