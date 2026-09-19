#include "whitehole/app/object_db_update.hpp"

#include <fstream>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#endif

namespace whitehole::app {
namespace {

#ifdef _WIN32

// Minimal RAII wrapper so every exit path closes its WinHTTP handle.
class InternetHandle {
public:
    InternetHandle() = default;
    ~InternetHandle() { reset(); }
    InternetHandle(const InternetHandle&) = delete;
    InternetHandle& operator=(const InternetHandle&) = delete;
    InternetHandle(InternetHandle&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    InternetHandle& operator=(InternetHandle&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    void reset() noexcept {
        if (handle_ != nullptr) {
            WinHttpCloseHandle(handle_);
            handle_ = nullptr;
        }
    }
    void set(HINTERNET handle) noexcept {
        reset();
        handle_ = handle;
    }
    [[nodiscard]] HINTERNET get() const noexcept { return handle_; }
    [[nodiscard]] bool valid() const noexcept { return handle_ != nullptr; }

private:
    HINTERNET handle_{nullptr};
};

constexpr wchar_t kDatabaseHost[] = L"raw.githubusercontent.com";
constexpr wchar_t kDatabasePath[] = L"/SMGCommunity/galaxydatabase/main/objectdb.json";

bool fetchDatabase(std::vector<char>& payload, std::string& error) {
    InternetHandle session;
    session.set(WinHttpOpen(L"WhiteholePro/0.2", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session.valid()) {
        error = "could not start an HTTP session (WinHttpOpen " +
                std::to_string(GetLastError()) + ")";
        return false;
    }

    InternetHandle connection;
    connection.set(WinHttpConnect(session.get(), kDatabaseHost, INTERNET_DEFAULT_HTTPS_PORT, 0));
    if (!connection.valid()) {
        error = "could not reach the database host (WinHttpConnect " +
                std::to_string(GetLastError()) + ")";
        return false;
    }

    InternetHandle request;
    request.set(WinHttpOpenRequest(connection.get(), L"GET", kDatabasePath, nullptr,
                                   WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                   WINHTTP_FLAG_SECURE));
    if (!request.valid()) {
        error = "could not build the request (WinHttpOpenRequest " +
                std::to_string(GetLastError()) + ")";
        return false;
    }

    if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        error = "could not send the request (WinHttpSendRequest " +
                std::to_string(GetLastError()) + ")";
        return false;
    }
    if (!WinHttpReceiveResponse(request.get(), nullptr)) {
        error = "no response from the database host (WinHttpReceiveResponse " +
                std::to_string(GetLastError()) + ")";
        return false;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    if (!WinHttpQueryHeaders(request.get(),
                             WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize,
                             WINHTTP_NO_HEADER_INDEX)) {
        error = "could not read the HTTP status (WinHttpQueryHeaders " +
                std::to_string(GetLastError()) + ")";
        return false;
    }
    if (status != 200) {
        error = "the database host returned HTTP " + std::to_string(status);
        return false;
    }

    payload.clear();
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.get(), &available)) {
            error = "the download stalled (WinHttpQueryDataAvailable " +
                    std::to_string(GetLastError()) + ")";
            return false;
        }
        if (available == 0) break;

        const std::size_t previous = payload.size();
        payload.resize(previous + static_cast<std::size_t>(available));
        DWORD read = 0;
        if (!WinHttpReadData(request.get(), payload.data() + previous, available, &read)) {
            error = "the download failed (WinHttpReadData " + std::to_string(GetLastError()) + ")";
            return false;
        }
        payload.resize(previous + static_cast<std::size_t>(read));
        if (read == 0) break;
    }
    return !payload.empty();
}

#endif // _WIN32

} // namespace

bool objectDatabaseDownloadAvailable() noexcept {
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

std::string downloadObjectDatabase(const std::filesystem::path& destination) {
#ifndef _WIN32
    (void)destination;
    return std::string("automatic download is only implemented on Windows; fetch ") +
           kObjectDatabaseUrl + " into data/objectdb.json manually";
#else
    std::vector<char> payload;
    std::string error;
    if (!fetchDatabase(payload, error)) {
        return error;
    }
    // Guard against captive portals or error pages being written as a database.
    if (payload.size() < 1024) {
        return "the downloaded database was unexpectedly small";
    }
    if (payload.front() != '{') {
        return "the downloaded data was not the object database";
    }

    std::error_code code;
    if (destination.has_parent_path()) {
        std::filesystem::create_directories(destination.parent_path(), code);
    }
    const std::filesystem::path temporary = destination.string() + ".download";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out) {
            return "could not open " + temporary.string() + " for writing";
        }
        out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        if (!out) {
            return "could not write " + temporary.string();
        }
    }
    std::filesystem::remove(destination, code);
    code.clear();
    std::filesystem::rename(temporary, destination, code);
    if (code) {
        return "could not move the downloaded database into place: " + code.message();
    }
    return {};
#endif
}

} // namespace whitehole::app