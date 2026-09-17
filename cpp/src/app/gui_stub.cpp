#include "whitehole/app/application.hpp"

#include <iostream>

namespace whitehole::app {

int runGui(const std::filesystem::path&, const std::filesystem::path&) {
    std::cerr << "The Whitehole desktop editor currently builds on Windows. Use the CLI on this platform:\n"
              << "  whitehole-neo map objects <archive.arc>\n"
              << "  whitehole-neo game list <game-directory>\n";
    return 1;
}

} // namespace whitehole::app
