#ifndef WHITEHOLE_DB_SPECIALRENDERERS_HPP
#define WHITEHOLE_DB_SPECIALRENDERERS_HPP

#include <string>
#include <string_view>
#include <unordered_map>

#include "whitehole/db/data_holder.hpp"

namespace whitehole::db {

// Data-only loader for SpecialRenderers.json. Does NOT construct any
// BmdRenderer/renderer objects (renderer code is in a separate lane).
// JSON has a top-level "SpecialRenderers" array; each entry has either
// "ObjectName" or "ClassName" and a "RendererType".
class SpecialRenderers final : public DataHolderBase {
public:
    SpecialRenderers();

    // Re-parses base/project JSON into an in-memory lookup table.
    void load();

    [[nodiscard]] bool isLoaded() const noexcept { return loaded_; }

    // Lookup a special renderer name by object name; empty if none.
    [[nodiscard]] std::string lookup(std::string_view objectName) const;

private:
    std::unordered_map<std::string, std::string> renderers_;
    bool loaded_{false};
};

}  // namespace whitehole::db

#endif  // WHITEHOLE_DB_SPECIALRENDERERS_HPP