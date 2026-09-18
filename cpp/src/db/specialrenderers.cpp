#include "whitehole/db/specialrenderers.hpp"

namespace whitehole::db {

SpecialRenderers::SpecialRenderers()
    : DataHolderBase("data/specialrenderers.json", "/specialrenderers.json", true) {}

void SpecialRenderers::load() {
    renderers_.clear();
    loaded_ = false;
    if (!dataPresent()) return;

    const auto& root = projectOrRoot();
    const auto arr = root.at("SpecialRenderers");
    if (!arr.isArray()) return;

    for (const auto& e : arr.asArray()) {
        if (!e.isObject()) continue;
        std::string name;
        // Prefer ObjectName, fall back to ClassName (as Java does).
                 const auto objName = e.at("ObjectName");
        if (objName.isString() && !objName.asString().empty()) {
            name = objName.asString();
        } else {
            const auto className = e.at("ClassName");
            if (className.isString() && !className.asString().empty()) {
                name = className.asString();
            }
        }
        const auto rendererType = e.at("RendererType");
        if (!name.empty() && rendererType.isString() && !rendererType.asString().empty()) {
                        renderers_[name] = rendererType.asString();
        }
    }
    loaded_ = true;
}

std::string SpecialRenderers::lookup(std::string_view objectName) const {
    const auto it = renderers_.find(std::string(objectName));
    if (it == renderers_.end()) return "";
    return it->second;
}

}  // namespace whitehole::db