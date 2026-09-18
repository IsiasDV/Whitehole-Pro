#include "whitehole/db/shortcuts.hpp"

namespace whitehole::db {

Shortcuts::Shortcuts()
    : DataHolderBase("data/shortcuts.json", "/shortcuts.json", true) {}

void Shortcuts::load() {
    loaded_ = false;
    if (!dataPresent()) return;
    data_ = projectOrRoot();
    loaded_ = data_.isObject();
}

std::string Shortcuts::get(std::string_view key) const {
    if (!loaded_ || !data_.isObject()) return "";
    const auto& val = data_.at(std::string(key));
    if (!val.isString()) return "";
    return val.asString();
}

}  // namespace whitehole::db