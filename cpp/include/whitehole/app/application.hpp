#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace whitehole::app {

[[nodiscard]] std::filesystem::path dataDirectory(const std::filesystem::path& executable);
int runCli(int argc, char** argv);
int runGui(const std::filesystem::path& executable);

} // namespace whitehole::app
