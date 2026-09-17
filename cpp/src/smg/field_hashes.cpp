#include "whitehole/smg/field_hashes.hpp"

#include "whitehole/smg/hash.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>

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
    std::ostringstream stream;
    stream << '[' << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << hash << ']';
    return stream.str();
}

} // namespace whitehole::smg
