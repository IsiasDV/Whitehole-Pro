#include "whitehole/smg/field_hashes.hpp"

#include "whitehole/smg/hash.hpp"

#include <array>
#include <cctype>
#include <charconv>
#include <fstream>
#include <string_view>

namespace whitehole::smg {

void FieldHashes::addName(std::string_view name) {
    if (name.empty()) {
        return;
    }
    names_[jmapHash(name)] = std::string(name);
}

void FieldHashes::loadFile(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream) {
        return;
    }
    std::string line;
    while (std::getline(stream, line)) {
        const auto start = line.find_first_not_of(" \t\r");
        if (start == std::string::npos || line[start] == '#') {
            continue;
        }
        const auto end = line.find_last_not_of(" \t\r");
        addName(line.substr(start, end - start + 1));
    }
}

std::string FieldHashes::nameOf(std::uint32_t hash) const {
    const auto found = names_.find(hash);
    if (found != names_.end()) {
        return found->second;
    }
    std::array<char, 8> digits{};
    const auto converted = std::to_chars(digits.data(), digits.data() + digits.size(), hash, 16);
    const auto length = static_cast<std::size_t>(converted.ptr - digits.data());
    std::string result("[");
    result.append(digits.size() - length, '0');
    for (std::size_t index = 0; index < length; ++index) {
        result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(digits[index]))));
    }
    result.push_back(']');
    return result;
}

} // namespace whitehole::smg
