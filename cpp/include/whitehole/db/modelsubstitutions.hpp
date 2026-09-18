#ifndef WHITEHOLE_DB_MODELSUBSTITUTIONS_HPP
#define WHITEHOLE_DB_MODELSUBSTITUTIONS_HPP

#include <string>
#include <string_view>
#include <unordered_map>

#include "whitehole/db/data_holder.hpp"

namespace whitehole::db {

// Native port of whitehole.db.ModelSubstitutions. The JSON file
// (data/modelsubstitutions.json) is a flat object: lowercase_key -> substituted_name.
// The caller decides whether to append "Low"/"Middle" suffix based on archive
// existence (kept out of this class to stay data-only).
class ModelSubstitutions final : public DataHolderBase {
public:
    ModelSubstitutions();

    // Re-parses base/project JSON into an in-memory substitution table.
    void load();

    [[nodiscard]] bool isLoaded() const noexcept { return loaded_; }

    // Case-insensitive lookup; returns substituted name or empty string.
    [[nodiscard]] std::string substitute(std::string_view modelName) const;

private:
    std::unordered_map<std::string /*lower name*/, std::string> subs_;
    bool loaded_{false};

    static std::string toLower(std::string_view s);
};

}  // namespace whitehole::db

#endif  // WHITEHOLE_DB_MODELSUBSTITUTIONS_HPP