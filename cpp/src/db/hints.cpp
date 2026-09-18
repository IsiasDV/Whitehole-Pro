#include "whitehole/db/hints.hpp"

namespace whitehole::db {

Hints::Hints() : DataHolderBase("data/hints.json", "/hints.json", true) {}

void Hints::load(int gameType) {
    applicableHints_.clear();
    loaded_ = false;
    if (!dataPresent()) return;

    const auto& root = projectOrRoot();
    const auto hintsArr = root.at("Hints");
    if (!hintsArr.isArray()) return;

    for (const auto& e : hintsArr.asArray()) {
        if (!e.isObject()) continue;
                const int game = static_cast<int>(e.at("Game").asNumber(0.0));
        if (game != 0 && game != gameType) continue;
        const std::string hint = e.stringAt("Hint");
        if (hint.empty()) continue;
        const std::string name = e.stringAt("Name");  // may be absent
        applicableHints_.emplace_back(name, hint);
    }
    loaded_ = true;
}

}  // namespace whitehole::db