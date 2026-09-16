#include "whitehole/io/directory_filesystem.hpp"

#include "whitehole/io/binary_file.hpp"

#include <algorithm>
#include <stdexcept>

namespace whitehole::io {
namespace {

bool isWithin(const std::filesystem::path& root, const std::filesystem::path& candidate) {
    auto rootPart = root.begin();
    auto candidatePart = candidate.begin();
    while (rootPart != root.end() && candidatePart != candidate.end() && *rootPart == *candidatePart) {
        ++rootPart;
        ++candidatePart;
    }
    return rootPart == root.end();
}

std::vector<std::string> list(const std::filesystem::path& directory, bool wantDirectories) {
    if (!std::filesystem::is_directory(directory)) {
        throw std::runtime_error("Project path is not a directory: " + directory.string());
    }
    std::vector<std::string> result;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        const auto status = entry.symlink_status();
        if ((wantDirectories && std::filesystem::is_directory(status))
            || (!wantDirectories && std::filesystem::is_regular_file(status))) {
            result.push_back(entry.path().filename().string());
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

} // namespace

DirectoryFilesystem::DirectoryFilesystem(std::filesystem::path root) {
    if (!std::filesystem::is_directory(root)) {
        throw std::runtime_error("Project root is not a directory: " + root.string());
    }
    root_ = std::filesystem::canonical(std::move(root));
}

void DirectoryFilesystem::validateName(std::string_view name) {
    if (name.empty() || name == "." || name == ".."
        || name.find('/') != std::string_view::npos
        || name.find('\\') != std::string_view::npos) {
        throw std::runtime_error("Project path contains an unsafe name");
    }
}

std::filesystem::path DirectoryFilesystem::resolve(std::string_view path) const {
    std::filesystem::path relative;
    for (const auto& component : std::filesystem::path(path)) {
        const auto name = component.string();
        if (name.empty() || name == "/" || name == "\\" || name == ".") {
            continue;
        }
        validateName(name);
        relative /= component;
    }
    const auto candidate = std::filesystem::weakly_canonical(root_ / relative);
    if (!isWithin(root_, candidate)) {
        throw std::runtime_error("Project path escapes the project root");
    }
    return candidate;
}

std::filesystem::path DirectoryFilesystem::renameDestination(std::string_view path, std::string_view newName) const {
    validateName(newName);
    const auto source = resolve(path);
    if (source == root_) {
        throw std::runtime_error("The project root cannot be renamed");
    }
    const auto destination = std::filesystem::weakly_canonical(source.parent_path() / std::filesystem::path(newName));
    if (!isWithin(root_, destination)) {
        throw std::runtime_error("Rename destination escapes the project root");
    }
    if (std::filesystem::exists(destination)) {
        throw std::runtime_error("Rename destination already exists: " + destination.string());
    }
    return destination;
}

std::vector<std::string> DirectoryFilesystem::directories(std::string_view path) const {
    return list(resolve(path), true);
}

std::vector<std::string> DirectoryFilesystem::files(std::string_view path) const {
    return list(resolve(path), false);
}

bool DirectoryFilesystem::directoryExists(std::string_view path) const {
    return std::filesystem::is_directory(resolve(path));
}

bool DirectoryFilesystem::fileExists(std::string_view path) const {
    return std::filesystem::is_regular_file(resolve(path));
}

std::vector<std::uint8_t> DirectoryFilesystem::read(std::string_view path) const {
    const auto file = resolve(path);
    if (!std::filesystem::is_regular_file(file)) {
        throw std::runtime_error("Project file does not exist: " + std::string(path));
    }
    return readFile(file);
}

void DirectoryFilesystem::write(std::string_view path, const std::vector<std::uint8_t>& data) {
    const auto file = resolve(path);
    if (file == root_) {
        throw std::runtime_error("Cannot write to the project root");
    }
    if (!std::filesystem::is_directory(file.parent_path())) {
        throw std::runtime_error("Project file parent directory does not exist");
    }
    writeFile(file, data);
}

void DirectoryFilesystem::createDirectory(std::string_view path) {
    const auto directory = resolve(path);
    if (!std::filesystem::create_directories(directory) && !std::filesystem::is_directory(directory)) {
        throw std::runtime_error("Could not create project directory: " + directory.string());
    }
}

void DirectoryFilesystem::renameFile(std::string_view path, std::string_view newName) {
    const auto source = resolve(path);
    if (!std::filesystem::is_regular_file(source)) {
        throw std::runtime_error("Project file does not exist: " + std::string(path));
    }
    std::filesystem::rename(source, renameDestination(path, newName));
}

void DirectoryFilesystem::renameDirectory(std::string_view path, std::string_view newName) {
    const auto source = resolve(path);
    if (!std::filesystem::is_directory(source) || source == root_) {
        throw std::runtime_error("Project directory does not exist or is the project root");
    }
    std::filesystem::rename(source, renameDestination(path, newName));
}

void DirectoryFilesystem::removeFile(std::string_view path) {
    const auto file = resolve(path);
    if (!std::filesystem::is_regular_file(file) || !std::filesystem::remove(file)) {
        throw std::runtime_error("Could not remove project file: " + std::string(path));
    }
}

void DirectoryFilesystem::removeDirectory(std::string_view path) {
    const auto directory = resolve(path);
    if (directory == root_) {
        throw std::runtime_error("The project root cannot be removed");
    }
    if (!std::filesystem::is_directory(directory) || !std::filesystem::remove(directory)) {
        throw std::runtime_error("Project directory does not exist or is not empty: " + std::string(path));
    }
}

} // namespace whitehole::io
