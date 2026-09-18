#pragma once

// Base helper for the "data/<file>.json" + optional project-level override
// JSON holders. This is the native equivalent of GameAndProjectDataHolder:
// base-game JSON from the data directory, project JSON from a stage archive
// (or workspace), project values win when present.

#include "whitehole/io/directory_filesystem.hpp"
#include "whitehole/util/json.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace whitehole::db {

class DataHolderBase {
public:
    explicit DataHolderBase(std::string baseGameRelative,
                            std::string projectRelative,
                            bool required,
                            int gameType = 2);

    void setBaseGameRoot(const std::filesystem::path& root);
    void initBaseGame();
    void initProject(const whitehole::io::DirectoryFilesystem& filesystem,
                     const std::filesystem::path& projectArchivePath = {});
    void clearProject();
    void setGameType(int gameType);

    [[nodiscard]] bool loaded() const noexcept { return loaded_; }
    [[nodiscard]] const whitehole::util::JsonValue& root() const;
    [[nodiscard]] const whitehole::util::JsonValue& projectOrRoot() const;

    [[nodiscard]] bool projectPresent() const;
    [[nodiscard]] bool dataPresent() const;

    static whitehole::util::JsonValue parseJson(const std::vector<std::uint8_t>& data);

protected:
    const std::string baseGamePath_;
    const std::string projectPath_;
    const bool required_;
    std::filesystem::path baseGameRoot_;
    int gameType_{2};

    std::optional<whitehole::util::JsonValue> baseGameData_;
    std::optional<whitehole::util::JsonValue> projectData_;
    bool loaded_{false};

    static std::string gameString(int gameType) {
        if (gameType == 1) return "SMG1";
        return "SMG2";
    }
};

} // namespace whitehole::db
