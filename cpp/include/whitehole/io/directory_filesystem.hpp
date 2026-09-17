#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace whitehole::io {

class DirectoryFilesystem {
public:
    explicit DirectoryFilesystem(std::filesystem::path root);

    [[nodiscard]] const std::filesystem::path& root() const noexcept { return root_; }
    [[nodiscard]] std::vector<std::string> directories(std::string_view path = "/") const;
    [[nodiscard]] std::vector<std::string> files(std::string_view path = "/") const;
    [[nodiscard]] bool directoryExists(std::string_view path) const;
    [[nodiscard]] bool fileExists(std::string_view path) const;
    [[nodiscard]] std::vector<std::uint8_t> read(std::string_view path) const;

    void write(std::string_view path, const std::vector<std::uint8_t>& data);
    void createDirectory(std::string_view path);
    void renameFile(std::string_view path, std::string_view newName);
    void renameDirectory(std::string_view path, std::string_view newName);
    void removeFile(std::string_view path);
    void removeDirectory(std::string_view path);

private:
    [[nodiscard]] std::filesystem::path resolve(std::string_view path) const;
    [[nodiscard]] std::filesystem::path renameDestination(std::string_view path, std::string_view newName) const;
    static void validateName(std::string_view name);

    std::filesystem::path root_;
};

} // namespace whitehole::io
