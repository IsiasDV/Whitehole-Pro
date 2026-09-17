// Windowed entry point for whitehole-pro.exe (no console window).
//
// The console-subsystem whitehole-neo.exe stays available for scripting and
// tests; this target only exists so double-clicking the app never flashes a
// console, and so "Open with..." can pass a map archive straight to the editor.

#include "whitehole/app/application.hpp"

#include <windows.h>
#include <shellapi.h>

#include <filesystem>
#include <string>

namespace {

[[nodiscard]] std::filesystem::path executablePath() {
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const auto length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return {};
        }
        if (length < buffer.size()) {
            buffer.resize(length);
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    return std::filesystem::path(buffer);
}

[[nodiscard]] std::filesystem::path firstArgument(std::filesystem::path executable) {
    int count = 0;
    auto arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (arguments == nullptr) {
        return {};
    }
    std::filesystem::path result;
    if (count > 1) {
        result = std::filesystem::path(arguments[1]);
    }
    LocalFree(arguments);
    (void)executable;
    return result;
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    const auto executable = executablePath();
    return whitehole::app::runGui(executable, firstArgument(executable));
}
