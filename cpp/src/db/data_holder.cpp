#include <cstdio>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include "whitehole/db/data_holder.hpp"
#include "whitehole/util/json.hpp"
namespace whitehole::db {
namespace {
void printWarning(const char* context, std::string_view path, const std::string& detail) {
    ::std::fprintf(stderr, "[whitehole: %s] %s %s\n", context, path.data(), detail.c_str());
}
static whitehole::util::JsonValue loadJson(std::istream& stream) {
    std::ostringstream buf;
    buf << stream.rdbuf();
    const std::string text = buf.str();
    return whitehole::db::DataHolderBase::parseJson({text.begin(), text.end()});
}
} // namespace
DataHolderBase::DataHolderBase(const std::string baseGameRelative, const std::string projectRelative,
                               const bool required, const int gameType)
    : baseGamePath_(baseGameRelative), projectPath_(projectRelative), required_(required), gameType_(gameType) {}

void DataHolderBase::setBaseGameRoot(const std::filesystem::path& root) {
    baseGameRoot_ = root;
}

void DataHolderBase::initBaseGame() {
    baseGameData_.reset();
    if (baseGameRoot_.empty()) {
        printWarning("fatal", baseGamePath_, "base game root not set");
        if (required_) [[unlikely]] {
            throw std::runtime_error("Base game data directory was not set");
        }
        return;
    }
    const std::filesystem::path path = baseGameRoot_ / baseGamePath_;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        printWarning("fatal", path.string(), "could not open file");
        if (required_) [[unlikely]] {
            throw std::runtime_error("Required base game data file missing: " + path.string());
        }
        return;
    }
    try {
        baseGameData_ = loadJson(in);
    } catch (const std::exception& ex) {
        printWarning("warning", path.string(), ex.what());
        if (required_) [[unlikely]] {
            throw;
        }
    }
    if (!baseGameData_ || !baseGameData_->isObject()) {
        if (required_) [[unlikely]] {
            throw std::runtime_error("Base game data was not a JSON object: " + path.string());
        }
    }
}

void DataHolderBase::initProject(const whitehole::io::DirectoryFilesystem& filesystem,
                                 const std::filesystem::path& projectArchivePath) {
    projectData_.reset();
    if (projectArchivePath.empty()) {
        if (!filesystem.fileExists(projectPath_)) {
            return;
        }
        if (!filesystem.directoryExists(projectPath_)) {
            const auto bytes = filesystem.read(projectPath_);
            projectData_ = DataHolderBase::parseJson(bytes);
            return;
        }
        // Traverse the archive path parts
        std::string accumulated;
        std::size_t prev = 0;
        std::size_t pos = 0;
        while (true) {
            pos = projectPath_.find('/', pos);
            std::string part;
            if (pos == std::string::npos) {
                part = projectPath_.substr(prev);
                pos = projectPath_.size();
            } else {
                part = projectPath_.substr(prev, pos - prev);
                pos++;
            }
            if (part.empty()) continue;
            if (!accumulated.empty()) accumulated += '/';
            accumulated += part;
            if (pos == projectPath_.size()) {
                const auto bytes = filesystem.read(accumulated);
                projectData_ = DataHolderBase::parseJson(bytes);
            } else {
                if (!filesystem.directoryExists(accumulated)) {
                    return;
                }
            }
        }
        return;
    }
    // Project archive path was supplied: expect within an existing RARC.
    if (!filesystem.fileExists(projectPath_)) {
        if (projectArchivePath.has_parent_path()) {
            const std::filesystem::path parent = projectArchivePath.parent_path();
            const std::filesystem::path joined = parent / projectPath_;
            const auto joinedPath = joined.string();
            if (filesystem.fileExists(joinedPath)) {
                const auto bytes = filesystem.read(joinedPath);
                projectData_ = DataHolderBase::parseJson(bytes);
                return;
            }
        }
        return;
    }
    const auto bytes = filesystem.read(projectPath_);
    projectData_ = DataHolderBase::parseJson(bytes);
}

void DataHolderBase::clearProject() {
    projectData_.reset();
}

void DataHolderBase::setGameType(int gameType) {
    gameType_ = gameType;
}

const whitehole::util::JsonValue& DataHolderBase::root() const {
    if (projectData_ && projectData_->isObject()) {
        return *projectData_;
    }
    return *baseGameData_;
}

const whitehole::util::JsonValue& DataHolderBase::projectOrRoot() const {
    if (projectData_ && projectData_->isObject()) {
        return *projectData_;
    }
    return *baseGameData_;
}

bool DataHolderBase::projectPresent() const {
    return projectData_.has_value() && projectData_->isObject();
}

bool DataHolderBase::dataPresent() const {
    return baseGameData_.has_value() && baseGameData_->isObject();
}

whitehole::util::JsonValue DataHolderBase::parseJson(const std::vector<std::uint8_t>& data) {
    return whitehole::util::parseJson({reinterpret_cast<const char*>(data.data()), data.size()});
}

} // namespace whitehole::db
