#include "whitehole/db/name_table.hpp"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace whitehole::db {
namespace {

void skipSpace(const std::string& text, std::size_t& index) {
    while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index]))) {
        ++index;
    }
}

std::string parseJsonString(const std::string& text, std::size_t& index) {
    if (index >= text.size() || text[index] != '"') {
        throw std::runtime_error("JSON string map expected a quoted string");
    }
    ++index;
    std::string result;
    while (index < text.size()) {
        const auto character = text[index++];
        if (character == '"') {
            return result;
        }
        if (character == '\\' && index < text.size()) {
            const auto escaped = text[index++];
            if (escaped == 'n') {
                result.push_back('\n');
            } else if (escaped == 't') {
                result.push_back('\t');
            } else {
                result.push_back(escaped);
            }
        } else {
            result.push_back(character);
        }
    }
    throw std::runtime_error("JSON string is not terminated");
}

} // namespace

void NameTable::loadJson(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return;
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    const auto text = buffer.str();
    std::size_t index = 0;
    skipSpace(text, index);
    if (index >= text.size() || text[index] != '{') {
        throw std::runtime_error("Galaxy/zone name table is not a JSON object");
    }
    ++index;
    while (index < text.size()) {
        skipSpace(text, index);
        if (index < text.size() && text[index] == '}') {
            return;
        }
        const auto key = parseJsonString(text, index);
        skipSpace(text, index);
        if (index >= text.size() || text[index] != ':') {
            throw std::runtime_error("JSON string map is missing a colon");
        }
        ++index;
        skipSpace(text, index);
        names_[key] = parseJsonString(text, index);
        skipSpace(text, index);
        if (index < text.size() && text[index] == ',') {
            ++index;
        }
    }
}

std::string NameTable::displayName(std::string_view identifier) const {
    const auto found = names_.find(std::string(identifier));
    if (found != names_.end()) {
        return found->second;
    }
    return '"' + std::string(identifier) + '"';
}

} // namespace whitehole::db
