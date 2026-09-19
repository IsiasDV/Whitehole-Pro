#pragma once

// Native equivalent of the Java ObjectDB: parses the community
// `objectdb.json` (Objects + Classes + Categories + Timestamp) so the editor
// can describe every object and every one of its BCSV parameters.
//
// Java re-parses the whole multi-megabyte JSON on every launch and then walks
// Swing-aware helper classes. Here we parse it once and persist a compact
// compiled cache next to the executable, keyed on the database `Timestamp`,
// so later launches skip JSON parsing entirely.
//
// The original "lightweight" entry points (contains/displayName/find/names)
// are preserved verbatim so existing call sites and tests keep working.

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace whitehole::db {

// Editor type of a parameter. Mirrors the `Type` values used by objectdb.json
// ("Integer"/"Float"/"Boolean"/"Bitfield") plus the list/switch/picker flavors
// the Java property grid distinguishes for its cells.
enum class PropertyKind : std::uint8_t {
    Integer,
    Float,
    Boolean,
    Text,
    List,       // integer field with a fixed value list
    IntList,    // integer field with a fixed value list (Java "intlist")
    TextList,   // string field with a fixed value list (Java "textlist")
    SwitchId,   // SW_* switch reference, resolved against the current zone/galaxy
    ObjectName, // field that names another object
    Bitfield,
    Unknown,
};

[[nodiscard]] std::string_view toString(PropertyKind kind) noexcept;

struct PropertyInfo {
    std::string identifier;    // BCSV field name, e.g. "Obj_arg0"
    std::string simpleName;    // human label; defaults to identifier
    std::string declaredType;  // raw "Type" from JSON, may be empty
    std::string description;   // tooltip text
    PropertyKind kind{PropertyKind::Integer};
    int games{0};                        // bitmask: 1=SMG1, 2=SMG2, 3=both, 0/>=4 = all
    bool needed{false};                  // Java `Needed`
    std::vector<std::string> values;     // formatted as "Value: Notes"
    std::vector<std::string> exclusives; // object internal names; empty = any object

    // Java ClassInfo.getPropertyInfo(): game mask first, then exclusives.
    [[nodiscard]] bool appliesTo(int gameType, std::string_view objectName) const noexcept;
};

struct ClassInfo {
    std::string internalName;
    std::string name;
    std::string notes;
    int games{0};
    int progress{0};
    std::unordered_map<std::string, PropertyInfo> properties;

    [[nodiscard]] bool valid() const noexcept { return !internalName.empty(); }
};

struct ObjectInfo {
    std::string name;        // friendly display name
    std::string category;    // category key, e.g. "stagepart"
    std::string description; // "Notes" text

    std::string internalName;
    std::string classNameSmg1;
    std::string classNameSmg2;
    std::string listSmg1; // placement list, e.g. "MapPartsInfo"
    std::string listSmg2;
    std::string areaShape;
    std::string file; // "Map" / "Design" / ...
    int games{0};
    int progress{0};
    bool unused{false};   // Java IsUnused
    bool leftover{false}; // Java IsLeftover

    [[nodiscard]] std::string_view className(int gameType) const noexcept {
        return gameType == 1 ? std::string_view(classNameSmg1) : std::string_view(classNameSmg2);
    }
    [[nodiscard]] std::string_view list(int gameType) const noexcept {
        return gameType == 1 ? std::string_view(listSmg1) : std::string_view(listSmg2);
    }
};

struct CategoryInfo {
    std::string key;
    std::string description;
    std::size_t order{0};
};

class ObjectDatabase {
public:
    // `cachePath` enables the compiled-cache fast path. When empty the database
    // is parsed from JSON and nothing is written.
    void load(const std::filesystem::path& path, const std::filesystem::path& cachePath = {});
    // Parses objectdb.json text directly (cache-miss path and tests).
    void loadFromJson(std::string_view jsonText);
    void clear() noexcept;

    // ---- preserved legacy API -------------------------------------------
    [[nodiscard]] bool empty() const noexcept { return objects_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return objects_.size(); }
    [[nodiscard]] bool contains(std::string_view name) const;
    [[nodiscard]] std::string displayName(std::string_view name) const;
    [[nodiscard]] const ObjectInfo* find(std::string_view name) const;
    [[nodiscard]] std::vector<std::string> names() const;

    // ---- database metadata ----------------------------------------------
    [[nodiscard]] std::uint32_t timestamp() const noexcept { return timestamp_; }
    [[nodiscard]] std::size_t classCount() const noexcept { return classes_.size(); }
    [[nodiscard]] std::size_t categoryCount() const noexcept { return categories_.size(); }
    [[nodiscard]] bool cacheLoaded() const noexcept { return cacheLoaded_; }
    [[nodiscard]] const std::vector<CategoryInfo>& categories() const noexcept { return categories_; }

    [[nodiscard]] const ClassInfo* findClass(std::string_view internalName) const;

    // Class describing `objectName` for `gameType`, via Java ObjectInfo.classInfo().
    [[nodiscard]] const ClassInfo* classForObject(std::string_view objectName, int gameType) const;

    // Java getPropertyInfoForObject(): alias the field, resolve the class for
    // `gameType`, then return the raw property without applying the
    // game/exclusives filters.
    [[nodiscard]] const PropertyInfo* rawProperty(std::string_view objectName,
                                                  std::string_view field, int gameType) const;

    // Same lookup with Java ClassInfo.getPropertyInfo() filtering.
    [[nodiscard]] const PropertyInfo* propertyForObject(std::string_view objectName,
                                                       std::string_view field, int gameType) const;

    [[nodiscard]] bool propertyUsed(std::string_view objectName, std::string_view field,
                                    int gameType) const;
    [[nodiscard]] std::string propertyLabel(std::string_view objectName, std::string_view field,
                                            int gameType) const;
    [[nodiscard]] std::string propertyDescription(std::string_view objectName,
                                                  std::string_view field, int gameType) const;
    [[nodiscard]] std::vector<std::string> propertyValues(std::string_view objectName,
                                                          std::string_view field, int gameType) const;

    // Java AbstractObj.loadDBInfo() game filtering.
    [[nodiscard]] bool objectAvailable(std::string_view objectName, int gameType) const;

    // Case-insensitive search over display name, internal name and class name.
    [[nodiscard]] std::vector<const ObjectInfo*> search(std::string_view needle, int gameType) const;

    // Java alias table: CommonPath_ID->Rail, CameraSetId->Camera,
    // GroupId->Group, DemoGroupId->DemoCast, MessageId->Message.
    [[nodiscard]] static std::string_view aliasField(std::string_view field) noexcept;
    [[nodiscard]] static PropertyKind kindFromTypeName(std::string_view declaredType) noexcept;

    // ---- compiled cache ---------------------------------------------------
    // Persists the currently loaded database. False when there is nothing to
    // write or the file could not be created.
    [[nodiscard]] bool writeCache(const std::filesystem::path& cachePath) const;

private:
    [[nodiscard]] bool tryLoadCache(const std::filesystem::path& cachePath);

    std::unordered_map<std::string, ObjectInfo> objects_;
    std::unordered_map<std::string, ClassInfo> classes_;
    std::vector<CategoryInfo> categories_;
    std::uint32_t timestamp_{0};
    bool cacheLoaded_{false};
};

} // namespace whitehole::db
