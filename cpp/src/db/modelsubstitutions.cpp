#include "whitehole/db/modelsubstitutions.hpp"

#include <algorithm>
#include <cctype>

namespace whitehole::db {

ModelSubstitutions::ModelSubstitutions()
    : DataHolderBase("data/modelsubstitutions.json", "/modelsubstitutions.json", true) {}

std::string ModelSubstitutions::toLower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

void ModelSubstitutions::load() {
    subs_.clear();
    loaded_ = false;
    if (!dataPresent()) return;

    const auto& root = projectOrRoot();
    if (!root.isObject()) return;
    for (const auto& [key, val] : root.asObject()) {
        const std::string from = key;
                const std::string to = val.isString() ? val.asString() : "";
        if (!from.empty() && !to.empty()) {
            subs_[toLower(from)] = to;
        }
    }
    loaded_ = true;
}

std::string ModelSubstitutions::substitute(std::string_view modelName) const {
    const auto key = toLower(modelName);
    const auto it = subs_.find(key);
    if (it == subs_.end()) return "";
    return it->second;
}

}  // namespace whitehole::db