#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace whitehole::app {

[[nodiscard]] std::filesystem::path dataDirectory(const std::filesystem::path& executable);
int runCli(int argc, char** argv);

// Opens the desktop editor. `initialFile` accepts a map archive (.arc/.szs) or a
// game workspace directory so "Open with..." and drag-and-drop work from Explorer.
int runGui(const std::filesystem::path& executable, const std::filesystem::path& initialFile = {});

} // namespace whitehole::app
