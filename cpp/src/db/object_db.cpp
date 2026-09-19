#include "whitehole/db/object_db.hpp"

#include "whitehole/io/binary_file.hpp"
#include "whitehole/util/json.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace whitehole::db {
namespace {

std::string pick(const util::JsonValue& o, std::initializer_list<const char*> keys) {
    for (const char* k : keys) {
        std::string v = o.at(k).asString();
        if (!v.empty()) return v;
    }
    return {};
}

int jsonInt(const util::JsonValue& value, int fallback = 0) {
    if (!value.isNumber()) return fallback;
    return static_cast<int>(value.asNumber());
}

std::string toLower(std::string_view text) {
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool containsInsensitive(std::string_view haystack, std::string_view lowerNeedle) {
    if (lowerNeedle.empty()) return true;
    if (haystack.empty()) return false;
    return toLower(haystack).find(lowerNeedle) != std::string::npos;
}

// Formats a JSON scalar the way org.json's optString() does: numbers become
// their plain text form (2 rather than 2.0) so value lists read naturally.
std::string scalarText(const util::JsonValue& value) {
    if (value.isString()) return value.asString();
    if (value.isBool()) return value.asBool() ? "true" : "false";
    if (!value.isNumber()) return {};
    const double number = value.asNumber();
    const auto truncated = static_cast<std::int64_t>(number);
    if (number == static_cast<double>(truncated)) {
        return std::to_string(truncated);
    }
    std::ostringstream stream;
    stream << number;
    return stream.str();
}

std::vector<std::string> stringArray(const util::JsonValue& value) {
    std::vector<std::string> out;
    if (!value.isArray()) return out;
    const auto& items = value.asArray();
    out.reserve(items.size());
    for (const auto& item : items) {
        if (item.isString()) {
            out.push_back(item.asString());
        } else if (item.isNumber()) {
            std::ostringstream stream;
            stream << item.asNumber();
            out.push_back(stream.str());
        }
    }
    return out;
}

// Java formats every Values entry as "Value: Notes".
std::vector<std::string> formattedValues(const util::JsonValue& value) {
    std::vector<std::string> out;
    if (!value.isArray()) return out;
    const auto& items = value.asArray();
    out.reserve(items.size());
    for (const auto& item : items) {
        if (item.isObject()) {
            const std::string raw = scalarText(item.at("Value"));
            const std::string notes = item.at("Notes").asString();
            out.push_back(notes.empty() ? raw : raw + ": " + notes);
        } else if (item.isString()) {
            out.push_back(item.asString());
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Compiled-cache primitives
// ---------------------------------------------------------------------------

constexpr std::uint32_t kCacheMagic = 0x57484F44U; // 'WHOD'
constexpr std::uint16_t kCacheVersion = 1;
// Guards against a corrupt header asking for a gigantic allocation.
constexpr std::uint32_t kCacheSaneLimit = 4'000'000U;

void writeLenString(io::BinaryWriter& writer, std::string_view text) {
    writer.writeString(text, true);
}

std::string readLenString(io::BinaryReader& reader) {
    return reader.readString();
}

void writeStringList(io::BinaryWriter& writer, const std::vector<std::string>& values) {
    writer.writeU32(static_cast<std::uint32_t>(values.size()));
    for (const auto& value : values) {
        writeLenString(writer, value);
    }
}

std::vector<std::string> readStringList(io::BinaryReader& reader) {
    const auto count = reader.readU32();
    if (count > kCacheSaneLimit) {
        throw std::runtime_error("objectdb cache string list is implausible");
    }
    std::vector<std::string> out;
    out.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        out.push_back(readLenString(reader));
    }
    return out;
}

// True when the cache is at least as new as the JSON it was compiled from.
bool cacheIsFresh(const std::filesystem::path& jsonPath, const std::filesystem::path& cachePath) {
    std::error_code error;
    if (!std::filesystem::exists(cachePath, error)) return false;
    if (!std::filesystem::exists(jsonPath, error)) return true; // cache is the only source
    const auto jsonTime = std::filesystem::last_write_time(jsonPath, error);
    if (error) return false;
    const auto cacheTime = std::filesystem::last_write_time(cachePath, error);
    if (error) return false;
    return cacheTime >= jsonTime;
}

} // namespace

std::string_view toString(PropertyKind kind) noexcept {
    switch (kind) {
    case PropertyKind::Integer: return "integer";
    case PropertyKind::Float: return "float";
    case PropertyKind::Boolean: return "boolean";
    case PropertyKind::Text: return "text";
    case PropertyKind::List: return "list";
    case PropertyKind::IntList: return "intlist";
    case PropertyKind::TextList: return "textlist";
    case PropertyKind::SwitchId: return "switchid";
    case PropertyKind::ObjectName: return "objectname";
    case PropertyKind::Bitfield: return "bitfield";
    case PropertyKind::Unknown: break;
    }
    return "unknown";
}

bool PropertyInfo::appliesTo(int gameType, std::string_view objectName) const noexcept {
    // Java ClassInfo.getPropertyInfo(): a games value below 4 is a mask that
    // must include the selected game; 0 or >= 4 means "every game".
    if (games < 4 && (games & gameType) == 0) return false;
    if (exclusives.empty()) return true;
    return std::find(exclusives.begin(), exclusives.end(), objectName) != exclusives.end();
}

PropertyKind ObjectDatabase::kindFromTypeName(std::string_view declaredType) noexcept {
    if (declaredType == "Integer") return PropertyKind::Integer;
    if (declaredType == "Float") return PropertyKind::Float;
    if (declaredType == "Boolean") return PropertyKind::Boolean;
    if (declaredType == "Bitfield") return PropertyKind::Bitfield;
    if (declaredType == "Text") return PropertyKind::Text;
    return PropertyKind::Unknown;
}

std::string_view ObjectDatabase::aliasField(std::string_view field) noexcept {
    // Java ObjectDB.getPropertyInfoForObject() alias table.
    if (field == "CommonPath_ID") return "Rail";
    if (field == "CameraSetId") return "Camera";
    if (field == "GroupId") return "Group";
    if (field == "DemoGroupId") return "DemoCast";
    if (field == "MessageId") return "Message";
    return field;
}

void ObjectDatabase::clear() noexcept {
    objects_.clear();
    classes_.clear();
    categories_.clear();
    timestamp_ = 0;
    cacheLoaded_ = false;
}

void ObjectDatabase::load(const std::filesystem::path& path,
                          const std::filesystem::path& cachePath) {
    clear();
    const bool wantCache = !cachePath.empty();
    if (wantCache && cacheIsFresh(path, cachePath) && tryLoadCache(cachePath)) {
        return;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        // Preserve the original behaviour: a missing database is not fatal.
        return;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();

    try {
        loadFromJson(buffer.str());
    } catch (...) {
        // A malformed database must never take the editor or the CLI down.
        clear();
        return;
    }

    if (wantCache && !classes_.empty()) {
        try {
            (void)writeCache(cachePath);
        } catch (...) {
            // The cache is an optimisation only; failure is not an error.
        }
    }
}

void ObjectDatabase::loadFromJson(std::string_view jsonText) {
    const auto root = util::parseJson(jsonText);

    // Object records, tolerant of every shape that has shipped: the community
    // array form, an object map, and a bare array at the root.
    const auto loadRecords = [this](const util::JsonValue& source) {
        if (source.isArray()) {
            const auto& items = source.asArray();
            objects_.reserve(objects_.size() + items.size());
            for (const auto& item : items) {
                if (!item.isObject()) continue;
                const std::string key = pick(item, {"InternalName", "Name", "name", "ObjectName"});
                if (key.empty()) continue;
                ObjectInfo entry;
                entry.internalName = key;
                entry.name = pick(item, {"Name", "SimpleName", "DisplayName", "name"});
                if (entry.name.empty()) entry.name = key;
                entry.category = pick(item, {"Category", "category"});
                entry.description = pick(item, {"Notes", "Description", "description"});
                entry.classNameSmg1 = pick(item, {"ClassNameSMG1"});
                entry.classNameSmg2 = pick(item, {"ClassNameSMG2"});
                entry.listSmg1 = pick(item, {"ListSMG1"});
                entry.listSmg2 = pick(item, {"ListSMG2"});
                entry.areaShape = pick(item, {"AreaShape"});
                entry.file = pick(item, {"File"});
                entry.games = jsonInt(item.at("Games"));
                entry.progress = jsonInt(item.at("Progress"));
                entry.unused = item.at("IsUnused").asBool(false);
                entry.leftover = item.at("IsLeftover").asBool(false);
                objects_.emplace(key, std::move(entry));
            }
            return;
        }
        if (!source.isObject()) return;
        for (const auto& pair : source.asObject()) {
            const std::string& key = pair.first;
            const auto& item = pair.second;
            ObjectInfo entry;
            entry.internalName = key;
            entry.name = pick(item, {"Name", "SimpleName", "DisplayName", "name"});
            if (entry.name.empty()) entry.name = key;
            entry.category = pick(item, {"Category", "category"});
            entry.description = pick(item, {"Notes", "Description", "description"});
            entry.classNameSmg1 = pick(item, {"ClassNameSMG1"});
            entry.classNameSmg2 = pick(item, {"ClassNameSMG2"});
            entry.games = jsonInt(item.at("Games"));
            entry.unused = item.at("IsUnused").asBool(false);
            entry.leftover = item.at("IsLeftover").asBool(false);
            objects_.emplace(key, std::move(entry));
        }
    };

    if (root.isArray()) {
        loadRecords(root);
        return;
    }
    if (!root.isObject()) return;

    timestamp_ = static_cast<std::uint32_t>(jsonInt(root.at("Timestamp")));

    // Categories: [{"Key","Description"}] — declaration order is kept so the
    // object picker can present them exactly like the Java editor does.
    const auto& rawCategories = root.at("Categories");
    if (rawCategories.isArray()) {
        std::size_t order = 0;
        for (const auto& item : rawCategories.asArray()) {
            CategoryInfo category;
            category.key = item.stringAt("Key");
            if (category.key.empty()) continue;
            category.description = item.stringAt("Description");
            category.order = order++;
            categories_.push_back(std::move(category));
        }
    }

    // Classes carry the per-object parameter metadata used by the property grid.
    const auto& rawClasses = root.at("Classes");
    if (rawClasses.isArray()) {
        const auto& items = rawClasses.asArray();
        classes_.reserve(items.size());
        for (const auto& item : items) {
            ClassInfo info;
            info.internalName = pick(item, {"InternalName", "Name"});
            if (info.internalName.empty()) continue;
            info.name = pick(item, {"Name"});
            info.notes = pick(item, {"Notes", "Description"});
            info.games = jsonInt(item.at("Games"));
            info.progress = jsonInt(item.at("Progress"));

            const auto& rawParameters = item.at("Parameters");
            if (rawParameters.isObject()) {
                const auto& parameters = rawParameters.asObject();
                info.properties.reserve(parameters.size());
                for (const auto& entry : parameters) {
                    const auto& raw = entry.second;
                    PropertyInfo property;
                    property.identifier = entry.first;
                    const std::string label = pick(raw, {"Name"});
                    property.simpleName = label.empty() ? entry.first : label;
                    property.declaredType = pick(raw, {"Type"});
                    property.kind = kindFromTypeName(property.declaredType);
                    property.description = pick(raw, {"Description"});
                    property.games = jsonInt(raw.at("Games"));
                    property.needed = raw.at("Needed").asBool(false);
                    property.values = formattedValues(raw.at("Values"));
                    property.exclusives = stringArray(raw.at("Exclusives"));
                    // Java switches any parameter with a value list to a list cell.
                    if (!property.values.empty()) {
                        property.kind = property.kind == PropertyKind::Float
                                            ? PropertyKind::List
                                            : PropertyKind::IntList;
                    }
                    if (property.kind == PropertyKind::Unknown) {
                        // objectdb.json omits "Type" for switches and group IDs;
                        // they are integer references in the BCSV.
                        property.kind = PropertyKind::Integer;
                    }
                    info.properties.emplace(entry.first, std::move(property));
                }
            }
            classes_.emplace(info.internalName, std::move(info));
        }
    }

    const util::JsonValue* rawObjects = &root.at("Objects");
    if (!rawObjects->isArray() && !rawObjects->isObject()) {
        // Alternate key names seen in hand-edited databases.
        for (const char* key : {"objects", "ObjectInfos", "entries"}) {
            const auto& candidate = root.at(key);
            if (candidate.isArray() || candidate.isObject()) {
                rawObjects = &candidate;
                break;
            }
        }
    }
    loadRecords(*rawObjects);
}

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

const ClassInfo* ObjectDatabase::findClass(std::string_view internalName) const {
    const auto it = classes_.find(std::string(internalName));
    return it == classes_.end() ? nullptr : &it->second;
}

const ClassInfo* ObjectDatabase::classForObject(std::string_view objectName, int gameType) const {
    const ObjectInfo* info = find(objectName);
    if (info == nullptr) return nullptr;
    const std::string_view className = info->className(gameType);
    if (className.empty()) return nullptr;
    return findClass(className);
}

const PropertyInfo* ObjectDatabase::rawProperty(std::string_view objectName, std::string_view field,
                                                int gameType) const {
    const ClassInfo* info = classForObject(objectName, gameType);
    if (info == nullptr) return nullptr;
    const auto it = info->properties.find(std::string(aliasField(field)));
    return it == info->properties.end() ? nullptr : &it->second;
}

const PropertyInfo* ObjectDatabase::propertyForObject(std::string_view objectName,
                                                      std::string_view field, int gameType) const {
    const PropertyInfo* property = rawProperty(objectName, field, gameType);
    if (property == nullptr) return nullptr;
    // Java ClassInfo.getPropertyInfo() filters exclusives by internal name.
    const ObjectInfo* info = find(objectName);
    const std::string_view internal =
        info != nullptr ? std::string_view(info->internalName) : objectName;
    return property->appliesTo(gameType, internal) ? property : nullptr;
}

bool ObjectDatabase::propertyUsed(std::string_view objectName, std::string_view field,
                                  int gameType) const {
    return propertyForObject(objectName, field, gameType) != nullptr;
}

std::string ObjectDatabase::propertyLabel(std::string_view objectName, std::string_view field,
                                          int gameType) const {
    const PropertyInfo* property = propertyForObject(objectName, field, gameType);
    if (property == nullptr) return std::string(field);
    return property->simpleName;
}

std::string ObjectDatabase::propertyDescription(std::string_view objectName, std::string_view field,
                                                int gameType) const {
    const PropertyInfo* property = propertyForObject(objectName, field, gameType);
    return property == nullptr ? std::string() : property->description;
}

std::vector<std::string> ObjectDatabase::propertyValues(std::string_view objectName,
                                                        std::string_view field, int gameType) const {
    const PropertyInfo* property = propertyForObject(objectName, field, gameType);
    if (property == nullptr) return {};
    return property->values;
}

bool ObjectDatabase::objectAvailable(std::string_view objectName, int gameType) const {
    // Java ObjectSelectForm: games below 4 is a mask that must include the game.
    const ObjectInfo* info = find(objectName);
    if (info == nullptr) return false;
    return info->games >= 4 || (info->games & gameType) != 0;
}

std::vector<const ObjectInfo*> ObjectDatabase::search(std::string_view needle, int gameType) const {
    const std::string lowerNeedle = toLower(needle);
    std::vector<const ObjectInfo*> out;
    out.reserve(objects_.size() / 8 + 1);
    for (const auto& [key, info] : objects_) {
        if (!objectAvailable(key, gameType)) continue;
        if (containsInsensitive(info.name, lowerNeedle) ||
            containsInsensitive(info.internalName, lowerNeedle) ||
            containsInsensitive(info.className(gameType), lowerNeedle)) {
            out.push_back(&info);
        }
    }
    std::sort(out.begin(), out.end(), [](const ObjectInfo* left, const ObjectInfo* right) {
        return left->name < right->name;
    });
    return out;
}

bool ObjectDatabase::writeCache(const std::filesystem::path& cachePath) const {
    if (objects_.empty() && classes_.empty()) return false;

    io::BinaryWriter writer(io::Endian::little);
    writer.writeU32(kCacheMagic);
    writer.writeU16(kCacheVersion);
    writer.writeU16(static_cast<std::uint16_t>(0));
    writer.writeU32(timestamp_);

    writer.writeU32(static_cast<std::uint32_t>(objects_.size()));
    for (const auto& [key, info] : objects_) {
        writeLenString(writer, key);
        writeLenString(writer, info.name);
        writeLenString(writer, info.category);
        writeLenString(writer, info.description);
        writeLenString(writer, info.internalName);
        writeLenString(writer, info.classNameSmg1);
        writeLenString(writer, info.classNameSmg2);
        writeLenString(writer, info.listSmg1);
        writeLenString(writer, info.listSmg2);
        writeLenString(writer, info.areaShape);
        writeLenString(writer, info.file);
        writer.writeU32(static_cast<std::uint32_t>(info.games));
        writer.writeU32(static_cast<std::uint32_t>(info.progress));
        writer.writeU8(static_cast<std::uint8_t>(info.unused ? 1 : 0));
        writer.writeU8(static_cast<std::uint8_t>(info.leftover ? 1 : 0));
    }

    writer.writeU32(static_cast<std::uint32_t>(classes_.size()));
    for (const auto& [key, info] : classes_) {
        writeLenString(writer, key);
        writeLenString(writer, info.name);
        writeLenString(writer, info.notes);
        writer.writeU32(static_cast<std::uint32_t>(info.games));
        writer.writeU32(static_cast<std::uint32_t>(info.progress));
        writer.writeU32(static_cast<std::uint32_t>(info.properties.size()));
        for (const auto& [propertyKey, property] : info.properties) {
            writeLenString(writer, propertyKey);
            writeLenString(writer, property.identifier);
            writeLenString(writer, property.simpleName);
            writeLenString(writer, property.declaredType);
            writeLenString(writer, property.description);
            writer.writeU8(static_cast<std::uint8_t>(property.kind));
            writer.writeU8(static_cast<std::uint8_t>(property.needed ? 1 : 0));
            writer.writeU16(static_cast<std::uint16_t>(0));
            writer.writeU32(static_cast<std::uint32_t>(property.games));
            writeStringList(writer, property.values);
            writeStringList(writer, property.exclusives);
        }
    }

    writer.writeU32(static_cast<std::uint32_t>(categories_.size()));
    for (const auto& category : categories_) {
        writeLenString(writer, category.key);
        writeLenString(writer, category.description);
        writer.writeU32(static_cast<std::uint32_t>(category.order));
    }

    io::writeFile(cachePath, writer.data());
    return true;
}

bool ObjectDatabase::tryLoadCache(const std::filesystem::path& cachePath) {
    std::vector<std::uint8_t> bytes;
    try {
        bytes = io::readFile(cachePath);
    } catch (...) {
        return false;
    }
    if (bytes.size() < 16) return false;

    try {
        io::BinaryReader reader(bytes, io::Endian::little);
        if (reader.readU32() != kCacheMagic) return false;
        if (reader.readU16() != kCacheVersion) return false;
        (void)reader.readU16(); // reserved

        clear();
        timestamp_ = reader.readU32();

        const auto objectCount = reader.readU32();
        if (objectCount > kCacheSaneLimit) {
            clear();
            return false;
        }
        objects_.reserve(objectCount);
        for (std::uint32_t index = 0; index < objectCount; ++index) {
            const std::string key = readLenString(reader);
            ObjectInfo info;
            info.name = readLenString(reader);
            info.category = readLenString(reader);
            info.description = readLenString(reader);
            info.internalName = readLenString(reader);
            info.classNameSmg1 = readLenString(reader);
            info.classNameSmg2 = readLenString(reader);
            info.listSmg1 = readLenString(reader);
            info.listSmg2 = readLenString(reader);
            info.areaShape = readLenString(reader);
            info.file = readLenString(reader);
            info.games = static_cast<int>(reader.readU32());
            info.progress = static_cast<int>(reader.readU32());
            info.unused = reader.readU8() != 0;
            info.leftover = reader.readU8() != 0;
            objects_.emplace(key, std::move(info));
        }

        const auto classCount = reader.readU32();
        if (classCount > kCacheSaneLimit) {
            clear();
            return false;
        }
        classes_.reserve(classCount);
        for (std::uint32_t index = 0; index < classCount; ++index) {
            const std::string key = readLenString(reader);
            ClassInfo info;
            info.internalName = key;
            info.name = readLenString(reader);
            info.notes = readLenString(reader);
            info.games = static_cast<int>(reader.readU32());
            info.progress = static_cast<int>(reader.readU32());
            const auto propertyCount = reader.readU32();
            if (propertyCount > kCacheSaneLimit) {
                clear();
                return false;
            }
            info.properties.reserve(propertyCount);
            for (std::uint32_t propertyIndex = 0; propertyIndex < propertyCount; ++propertyIndex) {
                const std::string propertyKey = readLenString(reader);
                PropertyInfo property;
                property.identifier = readLenString(reader);
                property.simpleName = readLenString(reader);
                property.declaredType = readLenString(reader);
                property.description = readLenString(reader);
                property.kind = static_cast<PropertyKind>(reader.readU8());
                property.needed = reader.readU8() != 0;
                (void)reader.readU16(); // reserved
                property.games = static_cast<int>(reader.readU32());
                property.values = readStringList(reader);
                property.exclusives = readStringList(reader);
                info.properties.emplace(propertyKey, std::move(property));
            }
            classes_.emplace(key, std::move(info));
        }

        const auto categoryCount = reader.readU32();
        if (categoryCount > kCacheSaneLimit) {
            clear();
            return false;
        }
        categories_.reserve(categoryCount);
        for (std::uint32_t index = 0; index < categoryCount; ++index) {
            CategoryInfo category;
            category.key = readLenString(reader);
            category.description = readLenString(reader);
            category.order = reader.readU32();
            categories_.push_back(std::move(category));
        }

        // The writer emits nothing past this point, so trailing bytes mean the
        // file did not come from this version.
        if (reader.remaining() != 0) {
            clear();
            return false;
        }

        cacheLoaded_ = true;
        return true;
    } catch (...) {
        clear();
        return false;
    }
}

} // namespace whitehole::db
