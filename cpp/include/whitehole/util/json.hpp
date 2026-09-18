#pragma once

// Minimal dependency-free JSON value: objects, arrays, strings, numbers,
// booleans and null. Just enough for settings files, name tables, hints
// and the object database — no third-party dependency so the single-file
// static executable stays portable.

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace whitehole::util {

struct JsonValue;

using JsonObject = std::map<std::string, JsonValue>;
using JsonArray = std::vector<JsonValue>;

struct JsonValue {
    using Storage = std::variant<std::nullptr_t, bool, double, std::string, JsonArray, JsonObject>;
    Storage data{nullptr};

    JsonValue() = default;
    JsonValue(std::nullptr_t) : data(nullptr) {}
    JsonValue(bool value) : data(value) {}
    JsonValue(double value) : data(value) {}
    JsonValue(int value) : data(static_cast<double>(value)) {}
    JsonValue(std::int64_t value) : data(static_cast<double>(value)) {}
    JsonValue(const char* value) : data(std::string(value)) {}
    JsonValue(std::string_view value) : data(std::string(value)) {}
    JsonValue(std::string value) : data(std::move(value)) {}
    JsonValue(JsonArray value) : data(std::move(value)) {}
    JsonValue(JsonObject value) : data(std::move(value)) {}

    [[nodiscard]] bool isNull() const noexcept { return std::holds_alternative<std::nullptr_t>(data); }
    [[nodiscard]] bool isBool() const noexcept { return std::holds_alternative<bool>(data); }
    [[nodiscard]] bool isNumber() const noexcept { return std::holds_alternative<double>(data); }
    [[nodiscard]] bool isString() const noexcept { return std::holds_alternative<std::string>(data); }
    [[nodiscard]] bool isArray() const noexcept { return std::holds_alternative<JsonArray>(data); }
    [[nodiscard]] bool isObject() const noexcept { return std::holds_alternative<JsonObject>(data); }

    [[nodiscard]] bool asBool(bool fallback = false) const noexcept {
        if (const auto* value = std::get_if<bool>(&data)) return *value;
        return fallback;
    }
    [[nodiscard]] double asNumber(double fallback = 0.0) const noexcept {
        if (const auto* value = std::get_if<double>(&data)) return *value;
        return fallback;
    }
    [[nodiscard]] std::string asString(const std::string& fallback = {}) const {
        if (const auto* value = std::get_if<std::string>(&data)) return *value;
        return fallback;
    }
    [[nodiscard]] const JsonArray& asArray() const {
        static const JsonArray empty;
        if (const auto* value = std::get_if<JsonArray>(&data)) return *value;
        return empty;
    }
    [[nodiscard]] const JsonObject& asObject() const {
        static const JsonObject empty;
        if (const auto* value = std::get_if<JsonObject>(&data)) return *value;
        return empty;
    }

    [[nodiscard]] const JsonValue& at(std::string_view key) const {
        static const JsonValue null;
        if (const auto* object = std::get_if<JsonObject>(&data)) {
            const auto found = object->find(std::string(key));
            if (found != object->end()) return found->second;
        }
        return null;
    }
    [[nodiscard]] std::string stringAt(std::string_view key, const std::string& fallback = {}) const {
        return at(key).asString(fallback);
    }
    [[nodiscard]] std::string strAt(std::string_view key, const std::string& fallback = {}) const {
        return stringAt(key, fallback);
    }
};

// Throws std::runtime_error on malformed input.
[[nodiscard]] JsonValue parseJson(std::string_view text);
[[nodiscard]] std::string serializeJson(const JsonValue& value);

} // namespace whitehole::util
