#include "whitehole/db/object_db.hpp"
#include "whitehole/util/json.hpp"
#include <fstream>
#include <initializer_list>
#include <sstream>
namespace whitehole::db {
namespace {
std::string pick(const util::JsonValue& o, std::initializer_list<const char*> keys) {
    for (const char* k : keys) {
        std::string v = o.at(k).asString();
        if (!v.empty()) return v;
    }
    return {};
}
} // namespace
void ObjectDatabase::load(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return;
    std::ostringstream buf;
    buf << in.rdbuf();
    util::JsonValue root;
    try { root = util::parseJson(buf.str()); } catch (...) { return; }
    const util::JsonValue* items = &root;
    if (root.isObject()) {
        static const char* kKeys[] = {"Objects", "objects", "ObjectInfos", "entries"};
        for (const char* k : kKeys) {
            const auto& cand = root.at(k);
            if (cand.isArray()) { items = &cand; break; }
            if (cand.isObject()) {
                objects_.reserve(objects_.size() + cand.asObject().size());
                for (const auto& entry_pair : cand.asObject()) {
                    const std::string& name = entry_pair.first;
                    const util::JsonValue& info = entry_pair.second;
                    ObjectInfo entry;
                    entry.name = name;
                    entry.category = info.stringAt("Category");
                    entry.description = pick(info, {"Description", "description", "Notes"});
                    const std::string disp = pick(info, {"SimpleName", "DisplayName", "name"});
                    if (!disp.empty()) entry.name = disp;
                    objects_[name] = std::move(entry);
                }
                return;
            }
        }
    }
    if (!items->isArray()) return;
    for (const auto& item : items->asArray()) {
        std::string key = pick(item, {"InternalName", "Name", "name", "ObjectName"});
        if (key.empty()) continue;
        ObjectInfo entry;
        entry.name = pick(item, {"SimpleName", "DisplayName"});
        if (entry.name.empty()) entry.name = key;
        entry.category = pick(item, {"Category", "category"});
        entry.description = pick(item, {"Description", "description", "Notes"});
        objects_[std::move(key)] = std::move(entry);
    }
}
void ObjectDatabase::clear() noexcept { objects_.clear(); }
bool ObjectDatabase::contains(std::string_view name) const {
    return objects_.find(std::string(name)) != objects_.end();
}
std::string ObjectDatabase::displayName(std::string_view name) const {
    const ObjectInfo* info = find(name);
    if (info == nullptr || info->name.empty()) return std::string("\"") + std::string(name) + "\"";
    return info->name;
}
const ObjectInfo* ObjectDatabase::find(std::string_view name) const {
    const auto it = objects_.find(std::string(name));
    return it == objects_.end() ? nullptr : &it->second;
}
std::vector<std::string> ObjectDatabase::names() const {
    std::vector<std::string> out;
    out.reserve(objects_.size());
    for (const auto& [k, v] : objects_) out.push_back(k);
    return out;
}
} // namespace whitehole::db
